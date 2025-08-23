/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "SummonInfo.h"

#include "CharmInfo.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "DBCEnums.h"
#include "DBCStores.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SummonInfoArgs.h"
#include "TotemPackets.h"

SummonInfo::SummonInfo(Creature* summonedCreature, SummonInfoArgs const& args) :
    _summonedCreature(ASSERT_NOTNULL(summonedCreature)), _summonerGUID(args.Summoner ? args.Summoner->GetGUID() : ObjectGuid::Empty),
    _remainingDuration(args.Duration), _maxHealth(args.MaxHealth), _creatureLevel(args.CreatureLevel),
    _flags(SummonPropertiesFlags::None), _control(SummonPropertiesControl::None), _summonSlot(SummonPropertiesSlot::None)
{
    if (args.SummonPropertiesId.has_value())
        InitializeSummonProperties(*args.SummonPropertiesId, Object::ToUnit(args.Summoner));
}

void SummonInfo::InitializeSummonProperties(uint32 summonPropertiesId, Unit const* summoner)
{
    SummonPropertiesEntry const* summonProperties = sSummonPropertiesStore.LookupEntry(summonPropertiesId);
    if (!summonProperties)
    {
        TC_LOG_ERROR("entities.unit", "Creature %s has been summoned with a non-existing SummonProperties.dbc entry (RecId: %u).", _summonedCreature->GetGUID().ToString().c_str(), summonPropertiesId);
        return;
    }

    if (summonProperties->Faction)
        _factionId = summonProperties->Faction;

    _flags = summonProperties->GetFlags();
    _summonSlot = static_cast<SummonPropertiesSlot>(summonProperties->Slot);
    _control = static_cast<SummonPropertiesControl>(summonProperties->Control);

    if (summoner)
    {
        if (_flags.HasFlag(SummonPropertiesFlags::UseSummonerFaction))
            _factionId = summoner->GetFaction();

        if (_control != SummonPropertiesControl::None)
        {
            // Controlled summons inherit the level of their summoner unless explicitly stated otherwise.
            // Level can be overridden either by SummonPropertiesFlags::UseCreatureLevel or by a spell effect value
            if (!_flags.HasFlag(SummonPropertiesFlags::UseCreatureLevel) && !_creatureLevel.has_value())
                _creatureLevel = summoner->getLevel();

            // Controlled summons inherit their summoner's faction if not overridden by DBC data
            if (!_factionId.has_value())
                _factionId = summoner->GetFaction();
        }
    }
}

Creature* SummonInfo::GetSummonedCreature() const
{
    return _summonedCreature;
}

Unit* SummonInfo::GetUnitSummoner() const
{
    return ObjectAccessor::GetUnit(*_summonedCreature, _summonerGUID);
}

GameObject* SummonInfo::GetGameObjectSummoner() const
{
    return ObjectAccessor::GetGameObject(*_summonedCreature, _summonerGUID);
}

Optional<Milliseconds> SummonInfo::GetRemainingDuration() const
{
    return _remainingDuration;
}

Optional<uint64> SummonInfo::GetMaxHealth() const
{
    return _maxHealth;
}

Optional<uint32> SummonInfo::GetFactionId() const
{
    return _factionId;
}

Optional<uint8> SummonInfo::GetCreatureLevel() const
{
    return _creatureLevel;
}

void SummonInfo::HandlePreSummonActions()
{
    // The summon is going to be treated as a pet. Prepare spell list.
    if (_control == SummonPropertiesControl::Pet)
    {
        if (CharmInfo* charmInfo = _summonedCreature->InitCharmInfo())
            charmInfo->InitCharmCreateSpells();
    }

    if (Unit* summoner = GetUnitSummoner())
    {
        SummonPropertiesSlot slot = GetSummonSlot();

        // Controlled summons always set their creator guid, which is being used to display summoner names in their title
        if (IsControlledBySummoner())
            _summonedCreature->SetCreatorGUID(summoner->GetGUID());

        // Pets are set to Assist by default (this does not apply for class pets which save their states)
        if (_control == SummonPropertiesControl::Pet)
            _summonedCreature->SetReactState(REACT_ASSIST);

        // Totem slot summons always send the TotemCreated packet. Some non-Shaman classes use this
        // to display summon icons that can be canceled (Consecration, DK ghouls, Wild Mushrooms)
        // This packet must be sent before the creature is being added to the world so that the client
        // does send correct GUIDs in CMSG_TOTEM_DESTROYED
        switch (slot)
        {
            case SummonPropertiesSlot::Totem1:
            case SummonPropertiesSlot::Totem2:
            case SummonPropertiesSlot::Totem3:
            case SummonPropertiesSlot::Totem4:
            {
                if (Player* playerSummoner = Object::ToPlayer(summoner))
                {
                    WorldPackets::Totem::TotemCreated totemCreated;
                    totemCreated.Totem = _summonedCreature->GetGUID();
                    totemCreated.SpellID = _summonedCreature->GetUInt32Value(UNIT_CREATED_BY_SPELL);
                    totemCreated.Duration = _remainingDuration.value_or(0ms).count();
                    totemCreated.Slot = AsUnderlyingType(_summonSlot) - 1;

                    playerSummoner->SendDirectMessage(totemCreated.Write());
                }
                break;
            }
            default:
                break;
        }
    }
}

void SummonInfo::HandlePostSummonActions()
{

    // If it's a summon with an expiration timer, mark it as active so its time won't stop ticking if no player is nearby
    if (_remainingDuration.has_value())
        _summonedCreature->setActive(true);

    if (Unit* summoner = GetUnitSummoner())
    {
        // Register Pet and enable its control
        if (_control == SummonPropertiesControl::Pet)
        {
            _summonedCreature->SetOwnerGUID(summoner->GetGUID());

            // Only set the Summon GUID when it's empty (prevent multiple applications on multi-summons like Feral Spirit)
            if (summoner->GetMinionGUID() != _summonedCreature->GetGUID())
            {
                summoner->SetMinionGUID(_summonedCreature->GetGUID());

                // Enable player control over the summon
                if (Player* playerSummoner = summoner->ToPlayer())
                {
                    _summonedCreature->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PLAYER_CONTROLLED);
                    playerSummoner->SendPetSpells(this);
                }
            }
        }

        // Infoke JustSummoned and IsSummonedBy AI hooks
        if (summoner->IsCreature() && summoner->IsAIEnabled())
            summoner->ToCreature()->AI()->JustSummoned(_summonedCreature);

        if (_summonedCreature->IsAIEnabled())
            _summonedCreature->AI()->IsSummonedBy(summoner);
    }

    // Casting passive spells
    if (IsControlledBySummoner())
        castPassiveSpells();
}

void SummonInfo::HandlePreUnsummonActions()
{
    Unit* summoner = GetUnitSummoner();
    if (!summoner)
        return;

    // Clear the current pet action bar and reset the Summon GUID
    if (_control == SummonPropertiesControl::Pet && summoner->GetMinionGUID() == _summonedCreature->GetGUID())
    {
        summoner->SetMinionGUID(ObjectGuid::Empty);
        if (Player* player = summoner->ToPlayer())
            player->SendRemoveControlBar();
    }

    if (summoner->IsCreature() && summoner->IsAIEnabled())
        summoner->ToCreature()->AI()->SummonedCreatureDespawn(_summonedCreature);
}

void SummonInfo::UpdateRemainingDuration(Milliseconds deltaTime)
{
    if (!_remainingDuration.has_value() || _remainingDuration <= 0ms)
        return;

    *_remainingDuration -= deltaTime;
    if (_remainingDuration <= 0ms)
    {
        if (DespawnsWhenExpired())
            _summonedCreature->DespawnOrUnsummon();
        else
            _summonedCreature->KillSelf();
    }
}

bool SummonInfo::DespawnsOnSummonerLogout() const
{
    return _flags.HasFlag(SummonPropertiesFlags::DespawnOnSummonerLogout);
}

void SummonInfo::SetDespawnOnSummonerLogout(bool set)
{
    if (set)
        _flags |= SummonPropertiesFlags::DespawnOnSummonerLogout;
    else
        _flags.RemoveFlag(SummonPropertiesFlags::DespawnOnSummonerLogout);
}

bool SummonInfo::DespawnsOnSummonerDeath() const
{
    return _flags.HasFlag(SummonPropertiesFlags::DespawnOnSummonerDeath);
}

void SummonInfo::SetDespawnOnSummonerDeath(bool set)
{
    if (set)
        _flags |= SummonPropertiesFlags::DespawnOnSummonerDeath;
    else
        _flags.RemoveFlag(SummonPropertiesFlags::DespawnOnSummonerDeath);
}

bool SummonInfo::DespawnsWhenExpired() const
{
    return _flags.HasFlag(SummonPropertiesFlags::DespawnWhenExpired);
}

void SummonInfo::SetDespawnWhenExpired(bool set)
{
    if (set)
        _flags |= SummonPropertiesFlags::DespawnWhenExpired;
    else
        _flags.RemoveFlag(SummonPropertiesFlags::DespawnWhenExpired);
}

bool SummonInfo::UsesSummonerFaction() const
{
    return _flags.HasFlag(SummonPropertiesFlags::UseSummonerFaction);
}

void SummonInfo::SetUseSummonerFaction(bool set)
{
    if (set)
        _flags |= SummonPropertiesFlags::UseSummonerFaction;
    else
        _flags.RemoveFlag(SummonPropertiesFlags::UseSummonerFaction);
}

bool SummonInfo::IsControlledBySummoner() const
{
    return _control > SummonPropertiesControl::None && _control < SummonPropertiesControl::Vehicle;
}

SummonPropertiesSlot SummonInfo::GetSummonSlot() const
{
    return _summonSlot;
}

SummonPropertiesControl SummonInfo::GetControl() const
{
    return _control;
}

void SummonInfo::castPassiveSpells()
{
    CreatureTemplate const* creatureInfo = _summonedCreature->GetCreatureTemplate();
    if (!creatureInfo || !creatureInfo->family)
        return;

    CreatureFamilyEntry const* creatureFamilyEntry = sCreatureFamilyStore.LookupEntry(creatureInfo->family);
    if (!creatureFamilyEntry)
        return;

    PetFamilySpellsStore::const_iterator itr = sPetFamilySpellsStore.find(creatureFamilyEntry->ID);
    if (itr == sPetFamilySpellsStore.end())
        return;

    for (uint32 spellId : itr->second)
    {
        if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId))
        {
            if (spellInfo->IsPassive())
            {
                _summonedCreature->CastSpell(nullptr, spellId);
                //if (spellInfo->HasAttribute(SPELL_ATTR4_OWNER_POWER_SCALING))
                //    _scalingAuras.push_back(spellId);
            }
        }
    }
}
