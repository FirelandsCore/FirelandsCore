/*
 * Copyright (C) 2010 - 2024 Eluna Lua Engine <https://elunaluaengine.github.io/>
 * This program is free software licensed under GPL version 3
 * Please see the included DOCS/LICENSE.md for more information
 */

#include "Hooks.h"
#include "HookHelpers.h"
#include "LuaEngine.h"
#include "BindingMap.h"
#include "ElunaIncludes.h"
#include "ElunaTemplate.h"
#include "ElunaLoader.h"
#include <algorithm> // std::transform
#include <cstdlib> // strtol

using namespace Hooks;

#define START_HOOK(EVENT) \
    auto binding = GetBinding<EventKey<UnitEvents>>(REGTYPE_UNIT);\
    auto key = EventKey<UnitEvents>(EVENT);\
    if (!binding->HasBindingsFor(key))\
        return;

#define START_HOOK_WITH_RETVAL(EVENT, RETVAL) \
    auto binding = GetBinding<EventKey<UnitEvents>>(REGTYPE_UNIT);\
    auto key = EventKey<UnitEvents>(EVENT);\
    if (!binding->HasBindingsFor(key))\
        return RETVAL;

void Eluna::OnPeriodicDamageAurasTick(Unit* target, Unit* attacker, uint32& damage)
{
    START_HOOK(UNIT_EVENT_ON_PERIODIC_DAMAGE_AURAS_TICK);
    HookPush(target);
    HookPush(attacker);
    HookPush(damage);


    int damageIndex = lua_gettop(L) - 1;
    int n = SetupStack(binding, key, 3);

    while (n > 0)
    {
        int r = CallOneFunction(n--, 3, 1);

        if (lua_isnumber(L, r))
        {
            damage = CHECKVAL<uint32>(r);
            // Update the stack for subsequent calls.
            ReplaceArgument(damage, damageIndex);
        }

        lua_pop(L, 1);
    }

    CleanUpStack(3);
}

void Eluna::OnSpellDamageTaken(Unit* target, Unit* attacker, int32& damage)
{
    START_HOOK(UNIT_EVENT_ON_SPELL_DAMAGE_TAKEN);
    HookPush(target);
    HookPush(attacker);
    HookPush(damage);


    int damageIndex = lua_gettop(L) - 1;
    int n = SetupStack(binding, key, 3);

    while (n > 0)
    {
        int r = CallOneFunction(n--, 3, 1);

        if (lua_isnumber(L, r))
        {
            damage = CHECKVAL<int32>(r);
            // Update the stack for subsequent calls.
            ReplaceArgument(damage, damageIndex);
        }

        lua_pop(L, 1);
    }

    CleanUpStack(3);
}

void Eluna::OnMeleeDamageTaken(Unit* target, Unit* attacker, uint32& damage)
{
    START_HOOK(UNIT_EVENT_ON_MELEE_DAMAGE_TAKEN);
    HookPush(target);
    HookPush(attacker);
    HookPush(damage);


    int damageIndex = lua_gettop(L) - 1;
    int n = SetupStack(binding, key, 3);

    while (n > 0)
    {
        int r = CallOneFunction(n--, 3, 1);

        if (lua_isnumber(L, r))
        {
            damage = CHECKVAL<uint32>(r);
            // Update the stack for subsequent calls.
            ReplaceArgument(damage, damageIndex);
        }

        lua_pop(L, 1);
    }

    CleanUpStack(3);
}

void Eluna::OnHealReceived(Unit* target, Unit* attacker, uint32& heal)
{
    START_HOOK(UNIT_EVENT_ON_HEAL_RECEIVED);
    HookPush(target);
    HookPush(attacker);
    HookPush(heal);


    int healIndex = lua_gettop(L) - 1;
    int n = SetupStack(binding, key, 3);

    while (n > 0)
    {
        int r = CallOneFunction(n--, 3, 1);

        if (lua_isnumber(L, r))
        {
            heal = CHECKVAL<uint32>(r);
            // Update the stack for subsequent calls.
            ReplaceArgument(heal, healIndex);
        }

        lua_pop(L, 1);
    }

    CleanUpStack(3);
}
