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

#include "Creature.h"
#include "DBCStores.h"
#include "DBCEnums.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "SummonInfoArgs.h"

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
    return _control > SummonPropertiesControl::None;
}

SummonPropertiesSlot SummonInfo::GetSummonSlot() const
{
    return _summonSlot;
}
