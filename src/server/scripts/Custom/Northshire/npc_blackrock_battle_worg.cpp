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

/*
 * Northshire Valley -- Stormwind Infantry (49869) vs Blackrock Battle Worg (49871),
 * with Brother Paxton (951) patching the infantry up between blows.
 *
 * Originally lived in EasternKingdoms/zone_elwynn_forest.cpp; moved here on
 * request. Behaviour is unchanged, only the location is.
 *
 * ---------------------------------------------------------------------------
 * HOW THIS DIRECTORY GETS BUILT (read before adding another file)
 *
 * 1. cmake/macros/AutoCollect.cmake CollectAndAddSourceFiles() recurses into
 *    subdirectories and file(GLOB)s *.cpp, so Custom/Northshire/*.cpp is picked
 *    up without a CMakeLists.txt of its own.
 *
 * 2. That file(GLOB) has NO CONFIGURE_DEPENDS. A brand new .cpp file therefore
 *    enters the build only after a fresh configure:
 *        cmake -S . -B Build
 *    An incremental "cmake --build" alone will silently ignore it.
 *
 * 3. Dropping the file in is still not enough. custom_script_loader.cpp has to
 *    call AddSC_northshire_brawl() from AddCustomScripts(), otherwise the
 *    RegisterCreatureAI() calls at the bottom of this file never run and the
 *    NPCs stay completely inert (no error, no log line -- just nothing).
 * ---------------------------------------------------------------------------
 *
 * WHY THE BROTHER PAXTON BUILD BELOW LOOKS THE WAY IT DOES
 *
 * Ported from the 7.3.5 reference (LegionCore-Reforged,
 * scripts/Custom/Northshire/npc_brother_paxton.cpp). Every difference from that
 * file is a SILENT runtime trap rather than a compile error, so they are listed:
 *
 *   class npc_brother_paxton : public CreatureScript   -> plain struct + RegisterCreatureAI
 *   paxtonAI->Talk(id, me->GetGUID())                  -> Talk(id, me)      (WorldObject const*)
 *   me->GetCreatureListInGrid(list, range)             -> AnyFriendlyUnitInObjectRangeCheck
 *                                                         + UnitListSearcher + Cell::VisitAllObjects
 *   SPELL_HEAL 11642 / SPELL_PRAYER_OF_HEALING 93091   -> 93091 is a MoP id that no
 *                                                         longer resolves on 12.1.5, so only
 *                                                         11642 is kept (13 creature
 *                                                         templates in this world db ship it)
 *   me->CastSpell(target, id, false)                   -> triggered cast: Paxton walks a
 *                                                         waypoint patrol, and a normal cast
 *                                                         would fight with the movement generator
 *
 * WHY THE INFANTRY / WORG PAIR NEEDS A SCRIPT AT ALL
 *
 * TrinityCore 12.x only allows NPC-vs-NPC combat when at least one side is
 * HOSTILE. WorldObject::IsValidAttackTarget() takes the "CvC" branch
 *
 *     return IsHostileTo(unitTarget) || unitTarget->IsHostileTo(this);
 *
 * and ThreatReference::ShouldBeOffline() feeds that result into
 * CanCreatureAttack(). For the neutral official pair (infantry 2321, worg 32)
 * the threat reference is therefore marked offline, Unit::Attack() leaves the
 * creature without a victim and DoMeleeAttackIfReady() never swings - which is
 * why the Cata / LegionCore script-only implementations cannot be reused
 * verbatim here.
 *
 * The official database ships faction 7 for the worg, which is NOT hostile to
 * the infantry's 2321 either - so upstream these two NPCs cannot fight at all,
 * and that is why TrinityCore issue #13258 never got a script fix.
 *
 * creature_template.faction for 49871 is therefore set to 14 (Blackrock Clan,
 * the faction every other Blackrock NPC uses). That is mutually hostile with
 * 2321, so the threat reference stays online and real combat happens.
 * MoveInLineOfSight() ignores players, so the worg never starts a fight on its
 * own even though faction 14 is hostile to them.
 *
 * REQUIRED DATABASE STATE (not enforced by this file):
 *   49869 / 49871 / 951 : creature_template.ScriptName = '' , AIName = '' (or SmartAI off)
 *   49871               : creature_template.faction = 14
 *   951                 : creature.MovementType = 2 + creature_addon.PathId = 951001
 *   creature_template_sparring rows for (49869,85) and (49871,85) stop the fight at 85%
 */

#include "CellImpl.h"
#include "Containers.h"
#include "CreatureAIImpl.h"
#include "EventMap.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "ScriptedCreature.h"
#include "ScriptMgr.h"
#include "Util.h"

namespace
{
    // -----------------------------------------------------------------------
    // Brother Paxton (951) -- the Northshire healer
    // -----------------------------------------------------------------------

    enum BrotherPaxton
    {
        NPC_BROTHER_PAXTON_ENTRY = 951,

        // Heal, rank 1. 13 NPC templates (Dun Garok Priest, Wrathtail Priestess, ...)
        // already cast it in this database, so the id is guaranteed to resolve.
        SPELL_HEAL_11642         = 11642,

        // creature_text for 951 only has group 0, and that group holds all five
        // lines (four "be healed" variants plus "FIGHT ON"); the core picks one at
        // random per call, so a single group id is the whole rotation.
        SAY_PAXTON_HEAL          = 0,

        EVENT_CHECK_FOR_WOUNDED  = 1,

        HEAL_CHECK_MIN_MS        = 15000,
        HEAL_CHECK_MAX_MS        = 21000
    };

    // Kept outside the enum on purpose: enums only take integral constant
    // expressions, a float literal there is MSVC error C2058.
    constexpr float HEAL_MISSING_HEALTH_PCT = 70.0f;
    constexpr float HEAL_SEARCH_RANGE       = 60.0f;

    // -----------------------------------------------------------------------
    // Stormwind Infantry (49869) / Blackrock Battle Worg (49871)
    // -----------------------------------------------------------------------

    enum NorthshireBrawl
    {
        NPC_STORMWIND_INFANTRY_ENTRY = 49869,

        EVENT_ASK_FOR_HELP           = 1,
        EVENT_SELECT_INFANTRY        = 2,
        EVENT_CALL_PAXTON            = 3,

        // Talk() takes a creature_text *GroupID*, not a line id. 49869 only has
        // GroupID 0, which itself holds the four lines 0-3; the core picks one of
        // them at random and remembers what was already said, so a single group
        // already gives the full rotation.
        SAY_ASK_FOR_HELP             = 0
    };

    // Infantry spawn points are 1.0 - 3.7 yards away from the closest worg spawn.
    constexpr float BRAWL_START_RANGE = 10.0f;
}

// ---------------------------------------------------------------------------
// Brother Paxton
//
// The heal loop is 15-21 s, and one cast is not always enough to top somebody
// up, which is what turns it into a repeating patch-up rather than a one-shot.
// ---------------------------------------------------------------------------
struct npc_brother_paxton : public ScriptedAI
{
    explicit npc_brother_paxton(Creature* creature) : ScriptedAI(creature)
    {
        // Paxton keeps patching the infantry up mid-brawl, which drags him into
        // combat -- and Player::GetQuestDialogStatus() returns QuestGiverStatus::None
        // for any quest giver that is in combat unless
        // UNIT_FLAG3_ALLOW_INTERACTION_WHILE_IN_COMBAT is set. Without that flag the
        // turn-in icon vanishes every time he starts healing.
        //
        // The authoritative source of this flag is the DATABASE, not code:
        // creature_template_difficulty.StaticFlags3 bit 0x00040000
        // (CREATURE_STATIC_FLAG_3_ALLOW_INTERACTION_WHILE_IN_COMBAT). It is mapped onto
        // unit_flags3 by ObjectMgr::ChooseCreatureFlags, so it is re-applied both when the
        // creature is created and on every respawn (Creature::setDeathState).
        //
        // Do NOT try creature_template.unit_flags3: that bit is listed in
        // UNIT_FLAG3_DISALLOWED (UnitDefines.h) and ObjectMgr::CheckCreatureTemplate
        // strips it on load with a "removing incorrect flag" error.
        //
        // The line below is only a safety net for the very first spawn; it survives in
        // memory but is wiped by the ReplaceAllUnitFlags3() done on respawn, which is
        // exactly why the DB flag above is the one that matters.
        me->SetUnitFlag3(UNIT_FLAG3_ALLOW_INTERACTION_WHILE_IN_COMBAT);
    }

    void Reset() override
    {
        _events.Reset();
        _events.ScheduleEvent(EVENT_CHECK_FOR_WOUNDED, Milliseconds(urand(HEAL_CHECK_MIN_MS, HEAL_CHECK_MAX_MS)));
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_CHECK_FOR_WOUNDED:
                    HealWoundedFriendly();
                    _events.ScheduleEvent(EVENT_CHECK_FOR_WOUNDED, Milliseconds(urand(HEAL_CHECK_MIN_MS, HEAL_CHECK_MAX_MS)));
                    break;
                default:
                    break;
            }
        }
    }

private:
    // Units in combat are deliberately NOT skipped: the whole point is the
    // infantry being chewed on by the worgs mid-fight.
    void HealWoundedFriendly()
    {
        if (!me->IsAlive())
            return;

        std::list<Unit*> friendlies;
        Trinity::AnyFriendlyUnitInObjectRangeCheck checker(me, me, HEAL_SEARCH_RANGE);
        Trinity::UnitListSearcher<Trinity::AnyFriendlyUnitInObjectRangeCheck> searcher(me, friendlies, checker);
        Cell::VisitAllObjects(me, searcher, HEAL_SEARCH_RANGE);

        Unit* patient = nullptr;
        float lowest = HEAL_MISSING_HEALTH_PCT;

        for (Unit* unit : friendlies)
        {
            if (unit == me || !unit->IsAlive())
                continue;

            if (unit->GetHealthPct() >= lowest)
                continue;

            lowest = unit->GetHealthPct();
            patient = unit;
        }

        if (!patient)
            return;

        // Face the patient before healing; a priest healing over his shoulder
        // looks broken even when the numbers work.
        me->SetFacingToObject(patient);
        me->CastSpell(patient, SPELL_HEAL_11642, true);
    }

    EventMap _events;
};

// ---------------------------------------------------------------------------
// Stormwind Infantry: shouts for help, then hands off to Paxton.
// ---------------------------------------------------------------------------
class npc_stormwind_infantry : public ScriptedAI
{
public:
    explicit npc_stormwind_infantry(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _events.Reset();
        _events.ScheduleEvent(EVENT_ASK_FOR_HELP, Seconds(urand(10, 100)));
    }

    void UpdateAI(uint32 diff) override
    {
        // No UpdateVictim() here on purpose - the brawl is driven from the worg
        // side, and UpdateVictim() would tear the combat down on the next tick.
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_ASK_FOR_HELP:
                    Talk(SAY_ASK_FOR_HELP);
                    _events.ScheduleEvent(EVENT_ASK_FOR_HELP, Seconds(urand(30, 100)));
                    // The shout is answered a beat later, not in the same frame.
                    _events.ScheduleEvent(EVENT_CALL_PAXTON, Seconds(2));
                    break;
                case EVENT_CALL_PAXTON:
                    CallPaxton();
                    break;
                default:
                    break;
            }
        }

        me->DoMeleeAttackIfReady();
    }

private:
    // The infantry is the one who asks, Paxton is the one who answers: he says
    // one of his own lines (creature_text 951 group 0) and tops the infantry up.
    // Same hand-off as the 7.3.5 reference - SmartAI cannot do it, because
    // SMART_EVENT_TEXT_OVER only fires for the script that issued the TALK.
    void CallPaxton()
    {
        Creature* paxton = me->FindNearestCreature(NPC_BROTHER_PAXTON_ENTRY, 30.0f, true);
        if (!paxton || paxton->HasUnitState(UNIT_STATE_CASTING))
            return;

        if (CreatureAI* paxtonAI = paxton->AI())
            paxtonAI->Talk(SAY_PAXTON_HEAL, me);

        if (me->GetHealthPct() <= 90.0f)
            paxton->CastSpell(me, SPELL_HEAL_11642, true);
    }

    EventMap _events;
};

// ---------------------------------------------------------------------------
// Blackrock Battle Worg: picks the nearest infantry and starts a two-way fight.
// ---------------------------------------------------------------------------
class npc_blackrock_battle_worg : public ScriptedAI
{
public:
    explicit npc_blackrock_battle_worg(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _events.Reset();
        _infantryGuid = ObjectGuid::Empty;
        _loggedFailure = false;
        _events.ScheduleEvent(EVENT_SELECT_INFANTRY, Seconds(1));
    }

    void JustDied(Unit* /*killer*/) override
    {
        if (Creature* infantry = ObjectAccessor::GetCreature(*me, _infantryGuid))
            if (CreatureAI* infantryAI = infantry->AI())
                infantryAI->EnterEvadeMode(EvadeReason::Other);
    }

    // The brawl is script driven: never aggro anything on sight, least of all a
    // player walking through Northshire.
    void MoveInLineOfSight(Unit* who) override
    {
        if (who->GetTypeId() == TYPEID_PLAYER)
            return;

        CreatureAI::MoveInLineOfSight(who);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_SELECT_INFANTRY:
                    TryEngageInfantry();
                    break;
                default:
                    break;
            }
        }

        // No UpdateVictim() while the script is driving the fight, otherwise the
        // core would tear the combat down on the next tick.
        if (!me->GetVictim())
            return;

        me->DoMeleeAttackIfReady();
    }

private:
    void TryEngageInfantry()
    {
        Creature* infantry = me->FindNearestCreature(NPC_STORMWIND_INFANTRY_ENTRY, BRAWL_START_RANGE, true);
        if (!infantry)
        {
            if (!_loggedFailure)
            {
                _loggedFailure = true;
                TC_LOG_INFO("scripts", "Northshire brawl: worg {} sees no infantry within {:.0f} yd (map {}, zone {}, area {})",
                    me->GetGUID().ToString(), BRAWL_START_RANGE, me->GetMapId(), me->GetZoneId(), me->GetAreaId());
            }

            _events.ScheduleEvent(EVENT_SELECT_INFANTRY, Seconds(3));
            return;
        }

        _infantryGuid = infantry->GetGUID();

        bool started = me->Attack(infantry, true);

        // Fallback for neutral factions: the core refuses Unit::Attack when neither side is
        // hostile, but an explicit combat state plus a threat reference on both sides sticks.
        if (!started)
        {
            me->SetInCombatWith(infantry);
            infantry->SetInCombatWith(me);
            me->GetThreatManager().AddThreat(infantry, 1000.0f);
            infantry->GetThreatManager().AddThreat(me, 1000.0f);
            started = me->GetVictim() != nullptr;
        }

        if (started)
        {
            _loggedFailure = false;

            if (CreatureAI* infantryAI = infantry->AI())
                infantryAI->AttackStart(me);
        }
        else if (!_loggedFailure)
        {
            _loggedFailure = true;
            TC_LOG_INFO("scripts", "Northshire brawl: worg {} cannot engage infantry {} (dist {:.2f}, map {}/zone {}/area {}, hostile {}/{})",
                me->GetGUID().ToString(), infantry->GetGUID().ToString(), me->GetDistance(infantry),
                me->GetMapId(), me->GetZoneId(), me->GetAreaId(),
                me->IsHostileTo(infantry), infantry->IsHostileTo(me));
        }

        _events.ScheduleEvent(EVENT_SELECT_INFANTRY, Seconds(started ? 10 : 2));
    }

    EventMap _events;
    ObjectGuid _infantryGuid;
    bool _loggedFailure = false;
};

void AddSC_northshire_brawl()
{
    RegisterCreatureAI(npc_brother_paxton);
    RegisterCreatureAI(npc_stormwind_infantry);
    RegisterCreatureAI(npc_blackrock_battle_worg);
}
