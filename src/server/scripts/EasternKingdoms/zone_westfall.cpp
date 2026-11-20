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

#include "CombatAI.h"
#include "Containers.h"
#include "Log.h"
#include "CreatureAIImpl.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "ScriptMgr.h"
#include "SpellAuraEffects.h"
#include "SpellScript.h"
#include "SharedDefines.h"
#include "TemporarySummon.h"

namespace Scripts::EasternKingdoms::Westfall
{
namespace Creatures
{
    static constexpr uint32 EnergizedHarvestReaper = 42342;
    static constexpr uint32 OverloadedHarvestGolem = 42601;
    static constexpr uint32 JangolodeMineGlubtok = 42492;
    static constexpr uint32 JangolodeMineFigure = 42515;
    static constexpr uint32 HomelessStormwindCitizen1 = 42386;
    static constexpr uint32 HomelessStormwindCitizen2 = 42384;
    static constexpr uint32 WestPlainDrifter = 42391;
    static constexpr uint32 Transient = 42383;
    static constexpr uint32 WestPlainsDrifterFollower = 42400;
    static constexpr uint32 WestfallStew = 42617;
    static constexpr uint32 LousPartingThoughtsTrigger = 42562;
    static constexpr uint32 Thug = 42387;
    static constexpr uint32 SmallTimeHustler = 42390;
    static constexpr uint32 SentinelHillGuard = 42407;
}

namespace Quests
{
    static constexpr uint32 MurderWasTheCaseThatTheyGaveMe = 26209;
    static constexpr uint32 LousPartingThoughts = 26232;
}

namespace Spells
{
    static constexpr uint32 JangolodeMineSummonFigure = 79265;
    static constexpr uint32 JangolodeMineSummonGlubtok = 79263;
    static constexpr uint32 HoboInformation1 = 79181;
    static constexpr uint32 HoboInformation2 = 79182;
    static constexpr uint32 HoboInformation3 = 79183;
    static constexpr uint32 HoboInformation4 = 79184;
    static constexpr uint32 SummonRagamuffinLooter = 79169;
    static constexpr uint32 SummonRagamuffinLooter1 = 79170;
    static constexpr uint32 SummonRagamuffinLooter2 = 79171;
    static constexpr uint32 SummonRagamuffinLooter3 = 79172;
    static constexpr uint32 SummonRagamuffinLooter4 = 79173;
    static constexpr uint32 HoboAggro = 79168;
    static constexpr uint32 HoboInformation = 79184;
    static constexpr uint32 FullBelly = 79451;
    static constexpr uint32 CosmeticSleep = 78677;
}

namespace Gossip
{
    namespace GossipOption
    {
        static constexpr uint32 GossipOption0 = 0;
        static constexpr uint32 GossipOption1 = 1;
    }
}

namespace Events
{
    namespace ItsAlive
    {
        static constexpr uint32 CheckArea = 1;
        static constexpr uint32 DespawnHarvester = 2;
    }

    namespace MurderWasTheCaseThatTheyGaveMe
    {
        static constexpr uint32 HoboTalk = 1;
        static constexpr uint32 JackpotIntro = 2;
        static constexpr uint32 HoboCry = 3;
        static constexpr uint32 PropertyRage = 4;
        static constexpr uint32 JackpotMiddle = 5;
        static constexpr uint32 JackpotEnd = 6;
        static constexpr uint32 ResumeMove = 7;
        static constexpr uint32 GroupOOC = 1;
    }

    namespace LousPartingThoughts
    {
        static constexpr uint32 SummonThugs = 1;
        static constexpr uint32 ThugSay0 = 2;
        static constexpr uint32 ThugSay1 = 3;
        static constexpr uint32 ThugSay2 = 4;
        static constexpr uint32 ThugSay3 = 5;
        static constexpr uint32 ThugSay4 = 6;
        static constexpr uint32 ThugSay5 = 7;
        static constexpr uint32 ThugSay6 = 8;
        static constexpr uint32 ThugCredit = 9;
        static constexpr uint32 ThugShoot1 = 10;
        static constexpr uint32 ThugShoot2 = 11;
        static constexpr uint32 ThugScream = 12;
        static constexpr uint32 ThugWarning = 13;
        static constexpr uint32 HustlerSay0 = 14;
        static constexpr uint32 HustlerSay1 = 15;
        static constexpr uint32 HustlerEventReset = 16;
    }

    namespace WestfallStew
    {
        // 11+ to stay clear of the MurderWasTheCaseThatTheyGaveMe event ids in the same switch
        static constexpr uint32 Start = 11;
        static constexpr uint32 MoveToStew = 12;
        static constexpr uint32 ReturnHome = 13;
        static constexpr uint32 Abort = 14;    // safety net: unlocks a citizen that never reached the stew
        static constexpr uint32 GroupStew = 2; // must not collide with MurderWasTheCaseThatTheyGaveMe::GroupOOC
    }

    // idle chit-chat for the homeless standing under the Sentinel Hill guards
    namespace CitizenIdle
    {
        static constexpr uint32 Chatter      = 15;   // WestfallStew already uses 11-14 in the same switch
        static constexpr uint32 GroupIdle    = 3;    // own group, so cancelling the stew events leaves it alone
        static constexpr Milliseconds MinDelay = 45s;
        static constexpr Milliseconds MaxDelay = 90s;
        static constexpr uint32 Chance       = 30;
        static constexpr float  GuardRange   = 30.0f;
    }

    // Sentinel Hill checkpoint: the refugee pair walks in, the guards send it away
    namespace RefugeeWalk
    {
        static constexpr uint32 StartMove             = 1;
        static constexpr uint32 CrimeSceneDrifterTalk  = 2;
        static constexpr uint32 CrimeSceneTalk12       = 3;
        static constexpr uint32 CrimeSceneTalk13       = 4;
        static constexpr uint32 GuardWhereGoing        = 5;
        static constexpr uint32 GateMeal               = 6;
        static constexpr uint32 GuardCantRead          = 7;
        static constexpr uint32 GuardNoBums            = 8;
        static constexpr uint32 GateCurse              = 9;
        static constexpr uint32 GateLeave              = 10;
        static constexpr uint32 Cancel                 = 11;
    }

    // ... and the drifter walking behind it
    namespace DrifterWalk
    {
        static constexpr uint32 CrimeSceneTalk1 = 1;
        static constexpr uint32 CrimeSceneTalk2 = 2;
    }
}

namespace Actions
{
    namespace MurderWasTheCaseThatTheyGaveMe
    {
        static constexpr uint32 HoboAggroAction = 1;
        static constexpr uint32 HoboAggroActionDone = 2;
    }
    namespace LousPartingThoughts
    {
        static constexpr uint32 ThugReset = 1;
    }
    // refugee pair walking from the Furlbrow farm to the Sentinel Hill checkpoint
    namespace RefugeeWalk
    {
        static constexpr uint32 CrimeScene = 1;
        static constexpr uint32 GateMeal   = 2;
        static constexpr uint32 GateCurse  = 3;
        static constexpr uint32 GateLeave  = 4;
    }
}

// the refugee walkthrough owns the checkpoint: while it runs the local homeless keep quiet,
// otherwise a random "I'm starving!" would cut straight into the guard exchange
bool RefugeeSceneRunning = false;

namespace Data
{
    static constexpr uint32 DataThugDeath = 1;
    static constexpr uint32 DataWestfallStew = 1;
}

namespace Text
{
    namespace HarvestGolem
    {
        static constexpr uint32 AnnounceOutOfArea = 0;
    }
    namespace HoboText
    {
        static constexpr uint32 HoboSayClue1 = 0;
        static constexpr uint32 HoboSayClue2 = 1;
        static constexpr uint32 HoboSayClue3 = 2;
        static constexpr uint32 HoboSayClue4 = 3;
        static constexpr uint32 HoboAggroBribe = 4;
        static constexpr uint32 HoboAggroConvince = 5;
        static constexpr uint32 HoboEvent = 6;
        static constexpr uint32 HoboJackpotIntro = 7;
        static constexpr uint32 HoboJackpotEnd = 8;
        static constexpr uint32 HoboPropertyRage = 9;
        static constexpr uint32 HoboFlee = 10;
    }
    namespace ThugText
    {
        static constexpr uint32 ThugSay0 = 0;
        static constexpr uint32 ThugSay1 = 1;
        static constexpr uint32 ThugSay2 = 2;
        static constexpr uint32 ThugSay3 = 3;
        static constexpr uint32 ThugSay4 = 4;
        static constexpr uint32 ThugSay5 = 5;
        static constexpr uint32 ThugSay6 = 6;
        static constexpr uint32 SayWarning = 0;
    }
    namespace SmallTimeHustlerText
    {
        static constexpr uint32 HustlerSay0 = 0;
        static constexpr uint32 HustlerSay1 = 1;
    }
    // fed-by-stew lines, group ids differ per creature in creature_text
    namespace StewText
    {
        static constexpr uint32 Citizen1 = 11;          // 42386
        static constexpr uint32 Citizen2 = 12;          // 42384
        static constexpr uint32 Transient = 17;         // 42383
        static constexpr uint32 WestPlainsDrifter = 11; // 42391
        static constexpr uint32 DrifterFollower = 6;    // 42400
    }
    // the eight "I'm starving!" lines live in group 6 on 42383 / 42384 / 42386 / 42391 only;
    // 42400 reuses group 6 for its fed-by-stew line, so it is filtered out in HasIdleChatter()
    namespace CitizenText
    {
        static constexpr uint32 IdleChatter = 6;
    }
    // 42383 - the refugee pair's leader, official group ids
    namespace RefugeeText
    {
        static constexpr uint32 CrimeScene0 = 11;  // 42318
        static constexpr uint32 CrimeScene1 = 12;  // 42321
        static constexpr uint32 CrimeScene2 = 13;  // 42323
        static constexpr uint32 GateMeal    = 14;  // 42342
        static constexpr uint32 GateCurse   = 15;  // 42347
        static constexpr uint32 GateLeave   = 16;  // 42348
    }
    // 42400 - the drifter walking behind it, official group ids
    namespace DrifterText
    {
        static constexpr uint32 CrimeScene0 = 0;   // 42320
        static constexpr uint32 CrimeScene1 = 1;   // 42322
        static constexpr uint32 CrimeScene2 = 2;   // 42324
        static constexpr uint32 GateMeal    = 3;   // 42342
        static constexpr uint32 GateCurse   = 4;   // 42347
        static constexpr uint32 GateLeave   = 5;   // 42348
    }
    // 42407 guards run SmartAI, we only push their creature_text groups
    namespace SentinelHillGateText
    {
        static constexpr uint32 GuardWhoa       = 0;   // 42340
        static constexpr uint32 GuardWhereGoing = 1;   // 42341
        static constexpr uint32 GuardCantRead   = 2;   // 42344
        static constexpr uint32 GuardNoBums     = 3;   // 42345
    }
}

namespace Sounds
{
    static constexpr uint32 Shooting = 15071;
    static constexpr uint32 Scream = 17852;
}

namespace Area
{
    static constexpr uint32 TheMoestFarm = 918;
}

// 79084 - Unbound Energy
class spell_westfall_unbound_energy : public SpellScript
{
    void FilterTargets(std::list<WorldObject*>& targets)
    {
        if (targets.empty())
            return;

        Unit* caster = GetCaster();
        targets.remove_if([caster](WorldObject const* target) -> bool
        {
            return caster->ToWorldObject() == target;
        });

        if (targets.size() > 1)
            Trinity::Containers::RandomResize(targets, 1);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_westfall_unbound_energy::FilterTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENTRY);
    }
};

// 42601 - Overloaded Harvest Golem
struct npc_westfall_overloaded_harvest_golem : public ScriptedAI
{
    npc_westfall_overloaded_harvest_golem(Creature* creature) : ScriptedAI(creature) {}

    void JustAppeared() override
    {
        _events.ScheduleEvent(Events::ItsAlive::CheckArea, 1s);
    }

    void PassengerBoarded(Unit* /*passenger*/, int8 /*seatId*/, bool apply) override
    {
        if (!apply)
            me->DespawnOrUnsummon();
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);
        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case Events::ItsAlive::CheckArea:
                    if (me->GetAreaId() != Area::TheMoestFarm)
                    {
                        if (Unit* owner = me->GetCharmerOrOwner())
                            Talk(Text::HarvestGolem::AnnounceOutOfArea, owner);
                        _events.ScheduleEvent(Events::ItsAlive::DespawnHarvester, 8s);
                    }
                    else
                        _events.Repeat(1s);
                    break;
                case Events::ItsAlive::DespawnHarvester:
                    if (me->GetAreaId() != Area::TheMoestFarm)
                        me->DespawnOrUnsummon();
                    else
                        _events.ScheduleEvent(Events::ItsAlive::CheckArea, 1s);
                    break;
                default:
                    break;
            }
        }
    }

private:
    EventMap _events;
};

// 79425 - Reaping Blows
class spell_westfall_reaping_blows : public AuraScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellEffect({ { spellInfo->Id, EFFECT_1 } }) && ValidateSpellInfo({ uint32(spellInfo->GetEffect(EFFECT_1).TriggerSpell) });
    }

    void HandlePeriodic(AuraEffect const* /*aurEff*/)
    {
        // HACK
        // periodic ticks are forcing to cast the spell onto himself instead of target
        // ref AuraEffect::HandlePeriodicTriggerSpellAuraTick
        PreventDefaultAction();
        if (Creature* reaper = GetTarget()->FindNearestCreature(Creatures::EnergizedHarvestReaper, 5.f, true))
            GetTarget()->CastSpell(reaper, GetSpellInfo()->GetEffect(EFFECT_1).TriggerSpell, true);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_westfall_reaping_blows::HandlePeriodic, EFFECT_1, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
    }
};

// 79436 - Wake Harvest Golem
class spell_westfall_wake_harvest_golem : public SpellScript
{
    SpellCastResult CheckTarget()
    {
        Unit* target = GetExplTargetUnit();
        if (!target || !target->IsCreature())
            return SPELL_FAILED_BAD_TARGETS;

        return SPELL_CAST_OK;
    }

    void HandleScriptEffect(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        if (!caster || !caster->IsPlayer())
            return;

        if (Creature* target = GetHitCreature())
        {
            caster->ToPlayer()->KilledMonsterCredit(Creatures::OverloadedHarvestGolem);
            target->DespawnOrUnsummon(100ms);
        }
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_westfall_wake_harvest_golem::CheckTarget);
        OnEffectHitTarget += SpellEffectFn(spell_westfall_wake_harvest_golem::HandleScriptEffect, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

// 79262 - Summon Lou's House
class spell_westfall_summon_lous_house : public SpellScript
{
    void HandleScriptEffect(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        caster->CastSpell(nullptr, Spells::JangolodeMineSummonFigure, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
        caster->CastSpell(nullptr, Spells::JangolodeMineSummonGlubtok, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_westfall_summon_lous_house::HandleScriptEffect, EFFECT_1, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

// 79275 - Quest Credit: Jangolde Event
class spell_westfall_quest_credit_jangolode_event : public SpellScript
{
    void HandleScriptEffect(SpellEffIndex /*effIndex*/)
    {
        GetHitUnit()->ExitVehicle();
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_westfall_quest_credit_jangolode_event::HandleScriptEffect, EFFECT_1, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

// 79290 - General Trigger 1: Glubtok
// 79292 - General Trigger 2: Glubtok
// 79294 - General Trigger 3: Glubtok
// 79297 - General Trigger 4: Glubtok
class spell_westfall_livin_the_life_ping_glubtok : public SpellScript
{
    void FilterTargets(std::list<WorldObject*>& targets)
    {
        Player* player = Object::ToPlayer(GetCaster());
        if (!player)
            return;

        targets.clear();
        if (Creature* creature = player->FindNearestCreatureWithOptions(50.0f, { .CreatureId = Creatures::JangolodeMineGlubtok, .OwnerGuid = player->GetGUID() }))
            targets.push_back(creature);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_westfall_livin_the_life_ping_glubtok::FilterTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENTRY);
    }
};

// 79279 - General Trigger 1: Figure
// 79283 - General Trigger 2: Figure
// 79284 - General Trigger 3: Figure
// 79287 - General Trigger 4: Figure
class spell_westfall_livin_the_life_ping_figure : public SpellScript
{
    void FilterTargets(std::list<WorldObject*>& targets)
    {
        Player* player = Object::ToPlayer(GetCaster());
        if (!player)
            return;

        targets.clear();
        if (Creature* creature = player->FindNearestCreatureWithOptions(50.0f, { .CreatureId = Creatures::JangolodeMineFigure, .OwnerGuid = player->GetGUID() }))
            targets.push_back(creature);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_westfall_livin_the_life_ping_figure::FilterTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENTRY);
    }
};

// 79273 - Despawn Jangolode Actor
class spell_westfall_despawn_jangolode_actor : public SpellScript
{
    void FilterTargets(std::list<WorldObject*>& targets)
    {
        Player* player = Object::ToPlayer(GetCaster());
        if (!player)
            return;

        targets.clear();
        if (Creature* creature = player->FindNearestCreatureWithOptions(50.0f, { .CreatureId = Creatures::JangolodeMineGlubtok, .OwnerGuid = player->GetGUID() }))
            targets.push_back(creature);

        if (Creature* creature = player->FindNearestCreatureWithOptions(50.0f, { .CreatureId = Creatures::JangolodeMineFigure, .OwnerGuid = player->GetGUID() }))
            targets.push_back(creature);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_westfall_despawn_jangolode_actor::FilterTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENTRY);
    }
};

// 42386 - Homeless Stormwind Citizen
// 42384 - Homeless Stormwind Citizen
// 42384 - West Plain Drifter
// 42383 - Transient
struct npc_westfall_hobo_witness : public ScriptedAI
{
    npc_westfall_hobo_witness(Creature* creature) : ScriptedAI(creature), _bribeFailed(false), _hoboRage(false), _flee(false) {}

    struct HoboClueData
    {
        uint32 SpellID;
        uint32 TextID;
    };

    static constexpr std::array<HoboClueData, 4> HoboClues =
    { {
        { Spells::HoboInformation1, Text::HoboText::HoboSayClue1 },
        { Spells::HoboInformation2, Text::HoboText::HoboSayClue2 },
        { Spells::HoboInformation3, Text::HoboText::HoboSayClue3 },
        { Spells::HoboInformation4, Text::HoboText::HoboSayClue4 },
    } };

    bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
    {
        uint8 clueGainChance = 0;
        switch (gossipListId)
        {
            case Gossip::GossipOption::GossipOption0:
            {
                clueGainChance = 25;
                break;
            }
            case Gossip::GossipOption::GossipOption1:
            {
                clueGainChance = 75;
                _bribeFailed = true;
                break;
            }
            default:
                return true;
        }

        me->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
        CloseGossipMenuFor(player);
        if (roll_chance(clueGainChance))
            GiveClue(player);
        else
        {
            me->SetImmuneToPC(false);
            AttackStart(player);
        }

        return false;
    }

    void GiveClue(Player* player)
    {
        _events.CancelEventGroup(Events::MurderWasTheCaseThatTheyGaveMe::GroupOOC);
        me->SetFacingToObject(player);

        uint16 slot = player->FindQuestSlot(Quests::MurderWasTheCaseThatTheyGaveMe);

        for (uint8 i = 0; i < HoboClues.size(); i++)
        {
            if (player->GetQuestSlotCounter(slot, i) == 0)
            {
                HoboClueData const& data = HoboClues[i];

                player->CastSpell(player, data.SpellID);
                Talk(data.TextID, player);

                me->DespawnOrUnsummon(12s);
                return;
            }
        }
    }

    void JustEngagedWith(Unit* who) override
    {
        me->SetAIAnimKitId(0);
        _events.CancelEventGroup(Events::MurderWasTheCaseThatTheyGaveMe::GroupOOC);

        if (!who->IsPlayer())
            return;

        Talk(_bribeFailed ? Text::HoboText::HoboAggroBribe : Text::HoboText::HoboAggroConvince, who);
    }

    void DamageTaken(Unit* /*killer*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo = nullptr*/) override
    {
        if (!_flee && me->HealthBelowPctDamaged(20, damage))
        {
            _flee = true;
            me->DoFleeToGetAssistance();
            Talk(Text::HoboText::HoboFlee);
        }
    }

    void JustDied(Unit* who) override
    {
        if (Creature* creature = who->ToCreature())
            creature->AI()->DoAction(Actions::MurderWasTheCaseThatTheyGaveMe::HoboAggroActionDone);
        DoCastSelf(Spells::SummonRagamuffinLooter);
    }

    void Reset() override
    {
        ScriptedAI::Reset();
        _events.Reset();
        _bribeFailed = false;
        _hoboRage = false;
        _flee = false;
        _stewGUID.Clear();
        _stewOldStandState = -1;
        me->SetNpcFlag(UNIT_NPC_FLAG_GOSSIP);
        me->SetImmuneToPC(true);
        _events.ScheduleEvent(Events::MurderWasTheCaseThatTheyGaveMe::ResumeMove, 100ms, Events::MurderWasTheCaseThatTheyGaveMe::GroupOOC);
    }

    void DoAction(int32 action) override
    {
        switch (action)
        {
            case Actions::MurderWasTheCaseThatTheyGaveMe::HoboAggroAction:
                if (Unit* target = ObjectAccessor::GetUnit(*me, _targetGUID))
                {
                    _hoboRage = true;
                    me->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
                    me->PauseMovement();
                    me->SetFacingToObject(target);
                    AttackStart(target);
                }
                break;
            case Actions::MurderWasTheCaseThatTheyGaveMe::HoboAggroActionDone:
                _hoboRage = false;
                break;
            default:
                return;
        }
    }

    void SetGUID(ObjectGuid const& guid, int32 id) override
    {
        // 26271 - Feeding the Hungry and the Hopeless: the stew hands us its own guid
        if (id == int32(Data::DataWestfallStew))
        {
            if (!_stewGUID.IsEmpty())
                return;

            _stewGUID = guid;
            _events.ScheduleEvent(Events::WestfallStew::Start, 0ms, Events::WestfallStew::GroupStew);
            // safety net: combat, a stuck path or a despawned stew would otherwise keep the citizen locked forever
            _events.ScheduleEvent(Events::WestfallStew::Abort, 20s, Events::WestfallStew::GroupStew);
            return;
        }

        _targetGUID = guid;
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (type != POINT_MOTION_TYPE || id != 1)
            return;

        me->CastSpell(me, Spells::FullBelly, true);

        if (Creature* stew = ObjectAccessor::GetCreature(*me, _stewGUID))
        {
            if (stew->IsSummon())
                if (WorldObject* summoner = stew->ToTempSummon()->GetSummoner())
                    if (Player* player = summoner->ToPlayer())
                        player->KilledMonsterCredit(Creatures::WestfallStew);

            if (roll_chance(50))
            {
                switch (me->GetEntry())
                {
                    case Creatures::HomelessStormwindCitizen1:
                        Talk(Text::StewText::Citizen1, stew);
                        break;
                    case Creatures::HomelessStormwindCitizen2:
                        Talk(Text::StewText::Citizen2, stew);
                        break;
                    case Creatures::Transient:
                        Talk(Text::StewText::Transient, stew);
                        break;
                    case Creatures::WestPlainDrifter:
                        Talk(Text::StewText::WestPlainsDrifter, stew);
                        break;
                    case Creatures::WestPlainsDrifterFollower:
                        Talk(Text::StewText::DrifterFollower, stew);
                        break;
                    default:
                        break;
                }
            }
        }

        // Release the lock as soon as the meal is over, otherwise the citizen stays bound to a stew that
        // no longer exists and ignores every following stew (Full Belly fades, but it never eats again).
        _stewGUID.Clear();
        _events.CancelEvent(Events::WestfallStew::Abort);
        _events.ScheduleEvent(Events::WestfallStew::ReturnHome, 5s + 500ms, Events::WestfallStew::GroupStew);
    }

    void JustReachedHome() override
    {
        // back at the spawn point: restore the pose the citizen had before the stew showed up
        if (_stewOldStandState < 0)
            return;

        if (_stewOldStandState == int32(UNIT_STAND_STATE_SLEEP) && !me->HasAura(Spells::CosmeticSleep))
            me->CastSpell(me, Spells::CosmeticSleep, true);

        me->SetStandState(UnitStandStateType(_stewOldStandState));
        _stewOldStandState = -1;
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);
        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case Events::MurderWasTheCaseThatTheyGaveMe::HoboTalk:
                    me->PauseMovement();
                    Talk(Text::HoboText::HoboEvent);
                    _events.ScheduleEvent(Events::MurderWasTheCaseThatTheyGaveMe::ResumeMove, 6s, Events::MurderWasTheCaseThatTheyGaveMe::GroupOOC);
                    break;
                case Events::MurderWasTheCaseThatTheyGaveMe::JackpotIntro:
                    me->PauseMovement();
                    Talk(Text::HoboText::HoboJackpotIntro);
                    _events.ScheduleEvent(Events::MurderWasTheCaseThatTheyGaveMe::JackpotMiddle, 2s + 500ms, Events::MurderWasTheCaseThatTheyGaveMe::GroupOOC);
                    break;
                case Events::MurderWasTheCaseThatTheyGaveMe::JackpotMiddle:
                    me->SetAIAnimKitId(648);
                    Talk(Text::HoboText::HoboJackpotEnd);
                    _events.ScheduleEvent(Events::MurderWasTheCaseThatTheyGaveMe::JackpotEnd, 6s, Events::MurderWasTheCaseThatTheyGaveMe::GroupOOC);
                    break;
                case Events::MurderWasTheCaseThatTheyGaveMe::JackpotEnd:
                    me->SetAIAnimKitId(0);
                    _events.ScheduleEvent(Events::MurderWasTheCaseThatTheyGaveMe::ResumeMove, 4s, Events::MurderWasTheCaseThatTheyGaveMe::GroupOOC);
                    break;
                case Events::MurderWasTheCaseThatTheyGaveMe::HoboCry:
                    me->HandleEmoteCommand(EMOTE_ONESHOT_CRY);
                    me->PauseMovement();
                    _events.ScheduleEvent(Events::MurderWasTheCaseThatTheyGaveMe::ResumeMove, 2s, Events::MurderWasTheCaseThatTheyGaveMe::GroupOOC);
                    break;
                case Events::MurderWasTheCaseThatTheyGaveMe::PropertyRage:
                {
                    uint32 creatureId = RAND(Creatures::HomelessStormwindCitizen1, Creatures::HomelessStormwindCitizen2, Creatures::Transient, Creatures::WestPlainDrifter);
                    if (Creature* creature = GetClosestCreatureWithEntry(me, creatureId, 25.0f))
                    {
                        if (!creature->IsAlive() || creature->IsInCombat())
                        {
                            _events.ScheduleEvent(Events::MurderWasTheCaseThatTheyGaveMe::ResumeMove, 100ms, Events::MurderWasTheCaseThatTheyGaveMe::GroupOOC);
                            return;
                        }
                        _hoboRage = true;
                        Talk(Text::HoboText::HoboPropertyRage);
                        me->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
                        me->PauseMovement();
                        me->CastSpell(creature, Spells::HoboAggro, true);
                        me->SetFacingToObject(creature);
                    }
                    else
                        _events.ScheduleEvent(Events::MurderWasTheCaseThatTheyGaveMe::ResumeMove, 100ms, Events::MurderWasTheCaseThatTheyGaveMe::GroupOOC);
                    break;
                }
                case Events::MurderWasTheCaseThatTheyGaveMe::ResumeMove:
                    me->ResumeMovement();
                    if (roll_chance(50)) // prevent spam
                    {
                        if (roll_chance(70)) // chance to trigger simple talk event
                            _events.ScheduleEvent(Events::MurderWasTheCaseThatTheyGaveMe::HoboTalk, 30s, 80s, Events::MurderWasTheCaseThatTheyGaveMe::GroupOOC);
                        else
                            _events.ScheduleEvent(urand(Events::MurderWasTheCaseThatTheyGaveMe::JackpotIntro, Events::MurderWasTheCaseThatTheyGaveMe::PropertyRage), 30s, 80s, Events::MurderWasTheCaseThatTheyGaveMe::GroupOOC);
                    }
                    else
                        _events.Repeat(30s, 80s);
                    break;
                case Events::WestfallStew::Start:
                    // remember the original pose: the Sentinel Hill hobos sleep on the ground and have to go
                    // back to it once the meal is over, otherwise they stand up for the rest of their life
                    if (_stewOldStandState < 0)
                        _stewOldStandState = int32(me->GetStandState());

                    me->SetStandState(UNIT_STAND_STATE_STAND);

                    if (me->HasAura(Spells::CosmeticSleep))
                        me->RemoveAurasDueToSpell(Spells::CosmeticSleep);

                    _events.ScheduleEvent(Events::WestfallStew::MoveToStew, 2s, Events::WestfallStew::GroupStew);
                    break;
                case Events::WestfallStew::MoveToStew:
                {
                    if (Creature* stew = ObjectAccessor::GetCreature(*me, _stewGUID))
                    {
                        float x, y, z;
                        stew->GetContactPoint(me, x, y, z, 0.2f);
                        me->GetMotionMaster()->MovePoint(1, x, y, z);
                    }
                    else
                    {
                        // the stew despawned on the way: unlock so the next one can feed us
                        _stewGUID.Clear();
                        _events.CancelEvent(Events::WestfallStew::Abort);
                        me->GetMotionMaster()->MoveTargetedHome();
                    }
                    break;
                }
                case Events::WestfallStew::ReturnHome:
                    _stewGUID.Clear();
                    me->GetMotionMaster()->MoveTargetedHome();
                    break;
                case Events::WestfallStew::Abort:
                    // never made it to the stew (combat, stuck path, despawn): unlock and head back
                    _stewGUID.Clear();
                    if (!me->IsInCombat())
                        me->GetMotionMaster()->MoveTargetedHome();
                    break;
                default:
                    break;
            }
        }

        if (!_hoboRage)
            if (!UpdateVictim())
                return;
    }

private:
    EventMap _events;
    ObjectGuid _targetGUID;
    ObjectGuid _stewGUID;
    int32 _stewOldStandState = -1;
    bool _bribeFailed;
    bool _hoboRage;
    bool _flee;
};

// 42617 - Westfall Stew
// 26271 - Feeding the Hungry and the Hopeless
struct npc_westfall_westfall_stew : public ScriptedAI
{
    npc_westfall_westfall_stew(Creature* creature) : ScriptedAI(creature) {}

    void IsSummonedBy(WorldObject* summoner) override
    {
        if (!summoner)
            return;

        std::vector<Creature*> homeless;
        me->GetCreatureListWithEntryInGrid(homeless, Creatures::HomelessStormwindCitizen1, 8.0f);
        me->GetCreatureListWithEntryInGrid(homeless, Creatures::HomelessStormwindCitizen2, 8.0f);
        me->GetCreatureListWithEntryInGrid(homeless, Creatures::Transient, 8.0f);
        me->GetCreatureListWithEntryInGrid(homeless, Creatures::WestPlainDrifter, 8.0f);
        me->GetCreatureListWithEntryInGrid(homeless, Creatures::WestPlainsDrifterFollower, 8.0f);

        for (Creature* citizen : homeless)
        {
            if (citizen->HasAura(Spells::FullBelly))
                continue;

            // a citizen fighting for its life cannot walk to the pot; skip it instead of locking it for 20s
            if (citizen->IsInCombat())
                continue;

            citizen->AI()->SetGUID(me->GetGUID(), int32(Data::DataWestfallStew));
        }
    }
};

// 42383 / 42384 / 42386 / 42391 / 42400 - plain citizens fed by Westfall Stew
// (the sentinel hill spawns instead run npc_westfall_hobo_witness, which feeds as well)
struct npc_westfall_westfall_citizen : public ScriptedAI
{
    npc_westfall_westfall_citizen(Creature* creature) : ScriptedAI(creature) {}

    void JustAppeared() override
    {
        _events.ScheduleEvent(Events::CitizenIdle::Chatter,
            randtime(Events::CitizenIdle::MinDelay, Events::CitizenIdle::MaxDelay), Events::CitizenIdle::GroupIdle);
    }

    void SetGUID(ObjectGuid const& guid, int32 id) override
    {
        if (id != int32(Data::DataWestfallStew))
            return;

        if (!_stewGUID.IsEmpty())
            return;

        _stewGUID = guid;
        _events.ScheduleEvent(Events::WestfallStew::Start, 0ms, Events::WestfallStew::GroupStew);
        // safety net: combat, a stuck path or a despawned stew would otherwise keep the citizen locked forever
        _events.ScheduleEvent(Events::WestfallStew::Abort, 20s, Events::WestfallStew::GroupStew);
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (type != POINT_MOTION_TYPE || id != 1)
            return;

        me->CastSpell(me, Spells::FullBelly, true);

        if (Creature* stew = ObjectAccessor::GetCreature(*me, _stewGUID))
        {
            if (stew->IsSummon())
                if (WorldObject* summoner = stew->ToTempSummon()->GetSummoner())
                    if (Player* player = summoner->ToPlayer())
                        player->KilledMonsterCredit(Creatures::WestfallStew);

            if (roll_chance(50))
            {
                switch (me->GetEntry())
                {
                    case Creatures::HomelessStormwindCitizen1:
                        Talk(Text::StewText::Citizen1, stew);
                        break;
                    case Creatures::HomelessStormwindCitizen2:
                        Talk(Text::StewText::Citizen2, stew);
                        break;
                    case Creatures::Transient:
                        Talk(Text::StewText::Transient, stew);
                        break;
                    case Creatures::WestPlainDrifter:
                        Talk(Text::StewText::WestPlainsDrifter, stew);
                        break;
                    case Creatures::WestPlainsDrifterFollower:
                        Talk(Text::StewText::DrifterFollower, stew);
                        break;
                    default:
                        break;
                }
            }
        }

        // Release the lock as soon as the meal is over, otherwise the citizen stays bound to a stew that
        // no longer exists and ignores every following stew (Full Belly fades, but it never eats again).
        _stewGUID.Clear();
        _events.CancelEvent(Events::WestfallStew::Abort);
        _events.ScheduleEvent(Events::WestfallStew::ReturnHome, 5s + 500ms, Events::WestfallStew::GroupStew);
    }

    void JustReachedHome() override
    {
        // back at the spawn point: restore the pose the citizen had before the stew showed up
        if (_stewOldStandState < 0)
            return;

        if (_stewOldStandState == int32(UNIT_STAND_STATE_SLEEP) && !me->HasAura(Spells::CosmeticSleep))
            me->CastSpell(me, Spells::CosmeticSleep, true);

        me->SetStandState(UnitStandStateType(_stewOldStandState));
        _stewOldStandState = -1;
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);
        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case Events::WestfallStew::Start:
                    // remember the original pose: the Sentinel Hill hobos sleep on the ground and have to go
                    // back to it once the meal is over, otherwise they stand up for the rest of their life
                    if (_stewOldStandState < 0)
                        _stewOldStandState = int32(me->GetStandState());

                    me->SetStandState(UNIT_STAND_STATE_STAND);

                    if (me->HasAura(Spells::CosmeticSleep))
                        me->RemoveAurasDueToSpell(Spells::CosmeticSleep);

                    _events.ScheduleEvent(Events::WestfallStew::MoveToStew, 2s, Events::WestfallStew::GroupStew);
                    break;
                case Events::WestfallStew::MoveToStew:
                {
                    if (Creature* stew = ObjectAccessor::GetCreature(*me, _stewGUID))
                    {
                        float x, y, z;
                        stew->GetContactPoint(me, x, y, z, 0.2f);
                        me->GetMotionMaster()->MovePoint(1, x, y, z);
                    }
                    else
                    {
                        // the stew despawned on the way: unlock so the next one can feed us
                        _stewGUID.Clear();
                        _events.CancelEvent(Events::WestfallStew::Abort);
                        me->GetMotionMaster()->MoveTargetedHome();
                    }
                    break;
                }
                case Events::WestfallStew::ReturnHome:
                    _stewGUID.Clear();
                    me->GetMotionMaster()->MoveTargetedHome();
                    break;
                case Events::WestfallStew::Abort:
                    // never made it to the stew (combat, stuck path, despawn): unlock and head back
                    _stewGUID.Clear();
                    if (!me->IsInCombat())
                        me->GetMotionMaster()->MoveTargetedHome();
                    break;
                case Events::CitizenIdle::Chatter:
                    IdleChatter();
                    break;
                default:
                    break;
            }
        }
    }

protected:
    // 45-90 s tick, 30 % roll: the citizen mutters one of the eight "I'm starving!" lines.
    // Only spawns that actually have a Sentinel Hill guard at hand join in, so the camps far
    // away from both checkpoints keep their old, silent behaviour.
    void IdleChatter()
    {
        _events.ScheduleEvent(Events::CitizenIdle::Chatter,
            randtime(Events::CitizenIdle::MinDelay, Events::CitizenIdle::MaxDelay), Events::CitizenIdle::GroupIdle);

        if (!_idleChatterKnown)
        {
            _idleChatter = HasIdleChatter();
            _idleChatterKnown = true;
        }

        if (!_idleChatter || !_stewGUID.IsEmpty() || me->IsInCombat() || !me->IsAlive())
            return;

        // asleep, or the checkpoint scene is playing: no small talk
        if (me->GetStandState() == uint8(UNIT_STAND_STATE_SLEEP) || RefugeeSceneRunning)
            return;

        if (!roll_chance(Events::CitizenIdle::Chance))
            return;

        Talk(uint8(Text::CitizenText::IdleChatter));
    }

    virtual bool HasIdleChatter() const
    {
        // group 6 carries the chit-chat lines on these four entries only; on 42400 the same
        // group id is the fed-by-stew line, so the drifter follower is left out on purpose
        switch (me->GetEntry())
        {
            case Creatures::Transient:
            case Creatures::HomelessStormwindCitizen1:
            case Creatures::HomelessStormwindCitizen2:
            case Creatures::WestPlainDrifter:
                break;
            default:
                return false;
        }

        std::vector<Creature*> guards;
        me->GetCreatureListWithEntryInGrid(guards, Creatures::SentinelHillGuard, Events::CitizenIdle::GuardRange);
        return !guards.empty();
    }

    EventMap _events;
    ObjectGuid _stewGUID;
    int32 _stewOldStandState = -1;
    bool _idleChatter = false;
    bool _idleChatterKnown = false;
};

// 274913 - the citizen sitting on the checkpoint bench. It is fed by the Westfall Stew like
// every other one (the stew looks the citizens up by entry, not by script), but it never
// joins the chit-chat. Only this single spawn gets the override, through creature.ScriptName.
struct npc_westfall_westfall_citizen_quiet : public npc_westfall_westfall_citizen
{
    npc_westfall_westfall_citizen_quiet(Creature* creature) : npc_westfall_westfall_citizen(creature) {}

protected:
    bool HasIdleChatter() const override { return false; }
};

// ---------------------------------------------------------------------------
// 42383 (guid 280420) - leader of the refugee pair that walks from the Furlbrow
// pumpkin farm to the Sentinel Hill checkpoint. Path 4238300 pauses at node 1
// (crime scene, 33 s) and node 39 (checkpoint, 42 s); node 43 ends the run.
//
// The two checkpoint guards (42407) keep their plain SmartAI: this script talks
// through them with AI()->Talk(), so the whole exchange runs on one clock.
// ---------------------------------------------------------------------------
struct npc_westfall_refugee_bridge_to_sentinelhill : public ScriptedAI
{
    npc_westfall_refugee_bridge_to_sentinelhill(Creature* creature) : ScriptedAI(creature) {}

    static constexpr uint32 PathId                = Creatures::Transient * 100;  // 4238300
    static constexpr uint32 PointCrimeScene       = 1;
    static constexpr uint32 PointSentinelHillGate = 39;
    static constexpr uint32 PointDespawn          = 43;
    static constexpr float  DrifterSearchRange    = 8.0f;
    static constexpr float  GuardSearchRange      = 12.0f;

    void JustAppeared() override
    {
        RefugeeSceneRunning = true;
        _events.ScheduleEvent(Events::RefugeeWalk::StartMove, 4s);
    }

    void MovementInform(uint32 type, uint32 pointId) override
    {
        if (type != WAYPOINT_MOTION_TYPE)
            return;

        switch (pointId)
        {
            case PointCrimeScene:
                Talk(uint8(Text::RefugeeText::CrimeScene0));
                _events.ScheduleEvent(Events::RefugeeWalk::CrimeSceneDrifterTalk, 6s);
                break;
            case PointSentinelHillGate:
                StartGateScene();
                break;
            case PointDespawn:
                _events.ScheduleEvent(Events::RefugeeWalk::Cancel, 0s);
                break;
            default:
                break;
        }
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case Events::RefugeeWalk::StartMove:
                    me->GetMotionMaster()->MovePath(PathId, false);
                    break;
                case Events::RefugeeWalk::CrimeSceneDrifterTalk:
                    if (Creature* drifter = me->FindNearestCreature(Creatures::WestPlainsDrifterFollower, DrifterSearchRange))
                    {
                        _drifterGUID = drifter->GetGUID();
                        if (drifter->IsAIEnabled())
                            drifter->AI()->DoAction(Actions::RefugeeWalk::CrimeScene);
                    }
                    _events.ScheduleEvent(Events::RefugeeWalk::CrimeSceneTalk12, 6s);
                    break;
                case Events::RefugeeWalk::CrimeSceneTalk12:
                    Talk(uint8(Text::RefugeeText::CrimeScene1));
                    _events.ScheduleEvent(Events::RefugeeWalk::CrimeSceneTalk13, 12s);
                    break;
                case Events::RefugeeWalk::CrimeSceneTalk13:
                    Talk(uint8(Text::RefugeeText::CrimeScene2));
                    break;
                case Events::RefugeeWalk::GuardWhereGoing:
                    GuardTalk(_guard1GUID, Text::SentinelHillGateText::GuardWhereGoing);
                    break;
                case Events::RefugeeWalk::GateMeal:
                    SayOrDelegate(Text::RefugeeText::GateMeal, Actions::RefugeeWalk::GateMeal);
                    break;
                case Events::RefugeeWalk::GuardCantRead:
                    GuardTalk(_guard2GUID, Text::SentinelHillGateText::GuardCantRead);
                    break;
                case Events::RefugeeWalk::GuardNoBums:
                    GuardTalk(_guard2GUID, Text::SentinelHillGateText::GuardNoBums);
                    break;
                case Events::RefugeeWalk::GateCurse:
                    SayOrDelegate(Text::RefugeeText::GateCurse, Actions::RefugeeWalk::GateCurse);
                    break;
                case Events::RefugeeWalk::GateLeave:
                    SayOrDelegate(Text::RefugeeText::GateLeave, Actions::RefugeeWalk::GateLeave);
                    break;
                case Events::RefugeeWalk::Cancel:
                    DespawnPair();
                    break;
                default:
                    break;
            }
        }
    }

private:
    // the two checkpoint guards stand at fixed posts, identified by home position
    void StartGateScene()
    {
        Creature* guard1 = nullptr;
        Creature* guard2 = nullptr;

        std::vector<Creature*> guards;
        me->GetCreatureListWithEntryInGrid(guards, Creatures::SentinelHillGuard, GuardSearchRange);

        for (Creature* guard : guards)
        {
            if (guard->GetHomePosition().GetExactDist2d(-10341.4f, 978.075f) < 0.1f)
                guard1 = guard;
            else if (guard->GetHomePosition().GetExactDist2d(-10341.3f, 981.691f) < 0.1f)
                guard2 = guard;

            if (guard1 && guard2)
                break;
        }

        if (!guard1 || !guard2 || guard1->IsInCombat() || guard2->IsInCombat())
        {
            _events.ScheduleEvent(Events::RefugeeWalk::Cancel, 5min);
            return;
        }

        _guard1GUID = guard1->GetGUID();
        _guard2GUID = guard2->GetGUID();

        // official beat: guard A opens, guard B cuts in at 17 s, the pair is sent
        // off at 32.5 / 38.5 s - the path then carries them away
        GuardTalk(_guard1GUID, Text::SentinelHillGateText::GuardWhoa);
        _events.ScheduleEvent(Events::RefugeeWalk::GuardWhereGoing, 4s);
        _events.ScheduleEvent(Events::RefugeeWalk::GateMeal, 10s);
        _events.ScheduleEvent(Events::RefugeeWalk::GuardCantRead, 17s);
        _events.ScheduleEvent(Events::RefugeeWalk::GuardNoBums, 23s);
        _events.ScheduleEvent(Events::RefugeeWalk::GateCurse, 32500ms);
        _events.ScheduleEvent(Events::RefugeeWalk::GateLeave, 38500ms);
        _events.ScheduleEvent(Events::RefugeeWalk::Cancel, 150s);   // backstop
    }

    Creature* GetDrifter()
    {
        if (Creature* drifter = ObjectAccessor::GetCreature(*me, _drifterGUID))
            return drifter;

        return me->FindNearestCreature(Creatures::WestPlainsDrifterFollower, 30.0f);
    }

    void GuardTalk(ObjectGuid const& guid, uint32 textId)
    {
        if (Creature* guard = ObjectAccessor::GetCreature(*me, guid))
            if (guard->IsAIEnabled())
                guard->AI()->Talk(uint8(textId));
    }

    // official rolls 50/50 between the leader and the drifter for these lines
    void SayOrDelegate(uint32 leaderText, uint32 drifterAction)
    {
        if (urand(1, 100) <= 50)
        {
            Talk(uint8(leaderText));
            return;
        }

        if (Creature* drifter = GetDrifter())
        {
            if (drifter->IsAIEnabled())
            {
                drifter->AI()->DoAction(drifterAction);
                return;
            }
        }

        Talk(uint8(leaderText));
    }

    void DespawnPair()
    {
        RefugeeSceneRunning = false;

        if (Creature* drifter = GetDrifter())
            drifter->DespawnOrUnsummon();

        me->DespawnOrUnsummon();
    }

    EventMap _events;
    ObjectGuid _drifterGUID;
    ObjectGuid _guard1GUID;
    ObjectGuid _guard2GUID;
};

// ---------------------------------------------------------------------------
// 42400 (guid 280419) - the drifter walking behind the leader. Says the crime
// scene half, and takes over some of the gate lines on the leader's 50/50 roll.
// ---------------------------------------------------------------------------
struct npc_westfall_west_plains_drifter_walk_to_sentinel : public ScriptedAI
{
    npc_westfall_west_plains_drifter_walk_to_sentinel(Creature* creature) : ScriptedAI(creature) {}

    void DoAction(int32 action) override
    {
        switch (action)
        {
            case Actions::RefugeeWalk::CrimeScene:
                Talk(uint8(Text::DrifterText::CrimeScene0));
                _events.ScheduleEvent(Events::DrifterWalk::CrimeSceneTalk1, 12s);
                break;
            case Actions::RefugeeWalk::GateMeal:
                Talk(uint8(Text::DrifterText::GateMeal));
                break;
            case Actions::RefugeeWalk::GateCurse:
                Talk(uint8(Text::DrifterText::GateCurse));
                break;
            case Actions::RefugeeWalk::GateLeave:
                Talk(uint8(Text::DrifterText::GateLeave));
                break;
            default:
                break;
        }
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case Events::DrifterWalk::CrimeSceneTalk1:
                    Talk(uint8(Text::DrifterText::CrimeScene1));
                    _events.ScheduleEvent(Events::DrifterWalk::CrimeSceneTalk2, 11s);
                    break;
                case Events::DrifterWalk::CrimeSceneTalk2:
                    Talk(uint8(Text::DrifterText::CrimeScene2));
                    break;
                default:
                    break;
            }
        }
    }

private:
    EventMap _events;
};

// 79169 - Summon Ragamuffin Looter
class spell_westfall_summon_ragamuffin_looter : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo(
            {
                Spells::SummonRagamuffinLooter1,
                Spells::SummonRagamuffinLooter2,
                Spells::SummonRagamuffinLooter3,
                Spells::SummonRagamuffinLooter4
            });
    }

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        caster->CastSpell(caster, Spells::SummonRagamuffinLooter1, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
        caster->CastSpell(caster, Spells::SummonRagamuffinLooter2, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
        caster->CastSpell(caster, Spells::SummonRagamuffinLooter3, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
        caster->CastSpell(caster, Spells::SummonRagamuffinLooter4, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
    }

    void Register() override
    {
        OnEffectLaunch += SpellEffectFn(spell_westfall_summon_ragamuffin_looter::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

// 79168 - Hobo Aggro
class spell_westfall_aggro_hobo : public SpellScript
{
    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        if (Creature* creature = GetHitCreature())
        {
            creature->AI()->SetGUID(GetCaster()->GetGUID(), 0);
            creature->AI()->DoAction(Actions::MurderWasTheCaseThatTheyGaveMe::HoboAggroAction);
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_westfall_aggro_hobo::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 5988 - Areatrigger
class at_westfall_two_shoed_lou_thugs : public AreaTriggerScript
{
public:
    at_westfall_two_shoed_lou_thugs() : AreaTriggerScript("at_westfall_two_shoed_lou_thugs") {}

    bool OnTrigger(Player* player, AreaTriggerEntry const* /*at*/) override
    {
        if (player->IsAlive())
            if (player->GetQuestStatus(Quests::LousPartingThoughts) == QUEST_STATUS_INCOMPLETE)
                if (Creature* lousPartingThoughtsTrigger = player->FindNearestCreature(Creatures::LousPartingThoughtsTrigger, 50.0f, true))
                    lousPartingThoughtsTrigger->AI()->SetGUID(player->GetGUID(), 0);

        return true;
    }
};

// 42387 - Westfall Thug
struct npc_westfall_thug : public ScriptedAI
{
    npc_westfall_thug(Creature* creature) : ScriptedAI(creature) {}

    void EnterEvadeMode(EvadeReason /*why*/) override
    {
        if (TempSummon* summon = me->ToTempSummon())
            if (Creature* summoner = summon->GetSummonerCreatureBase())
                summoner->AI()->DoAction(Actions::LousPartingThoughts::ThugReset);

        me->DespawnOrUnsummon();
    }

    void JustDied(Unit* /*who*/) override
    {
        if (TempSummon* summon = me->ToTempSummon())
            if (Creature* summoner = summon->GetSummonerCreatureBase())
                summoner->AI()->SetData(0, Data::DataThugDeath);
    }
};

Position constexpr ThugPos[4] =
{
    { -9859.36f, 1332.42f, 41.9859f, 2.49582f  },
    { -9862.52f, 1332.08f, 41.9859f, 0.855211f },
    { -9863.49f, 1335.49f, 41.9859f, 5.63741f  },
    { -9860.43f, 1335.46f, 41.9859f, 4.11898f  },
};

// 42562 - Lous Parting Thoughts Trigger
struct npc_westfall_lous_parting_thoughts_trigger : public ScriptedAI
{
    npc_westfall_lous_parting_thoughts_trigger(Creature* creature) : ScriptedAI(creature), _summonGUIDs(), _thugDeathCount(0) {}

    void SetGUID(ObjectGuid const& guid, int32 /*id*/) override
    {
        if (!_eventInvokerGUID.IsEmpty())
            return;

        _eventInvokerGUID = guid;

        _events.ScheduleEvent(Events::LousPartingThoughts::ThugSay0, 0s);
    }

    void SetData(uint32 /*type*/, uint32 data) override
    {
        if (data == Data::DataThugDeath)
        {
            _thugDeathCount++;

            if (_thugDeathCount >= 4)
            {
                _events.ScheduleEvent(Events::LousPartingThoughts::ThugCredit, 0s);
                DoAction(Actions::LousPartingThoughts::ThugReset);
            }
        }
    }

    void DoAction(int32 action) override
    {
        switch (action)
        {
            case Actions::LousPartingThoughts::ThugReset:
                _events.ScheduleEvent(Events::LousPartingThoughts::SummonThugs, 60s);
                break;
        }
    }

    void JustAppeared() override
    {
        _events.ScheduleEvent(Events::LousPartingThoughts::SummonThugs, 0s);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);
        while (uint32 eventId = _events.ExecuteEvent())
        {
            Player* invoker = ObjectAccessor::GetPlayer(*me, _eventInvokerGUID);

            switch (eventId)
            {
                case Events::LousPartingThoughts::SummonThugs:
                    for (uint8 i = 0; i < 4; i++)
                    {
                        if (Creature* thug = me->SummonCreature(Creatures::Thug, ThugPos[i], TEMPSUMMON_CORPSE_TIMED_DESPAWN, 60s))
                            _summonGUIDs[i] = thug->GetGUID();
                    }
                    _eventInvokerGUID = ObjectGuid::Empty;
                    _thugDeathCount = 0;
                    break;
                case Events::LousPartingThoughts::ThugSay0:
                {
                    Creature* thug1 = ObjectAccessor::GetCreature(*me, _summonGUIDs[0]);
                    if (invoker && thug1)
                        thug1->AI()->Talk(Text::ThugText::ThugSay0, invoker);
                    _events.ScheduleEvent(Events::LousPartingThoughts::ThugSay1, 5s);
                    break;
                }
                case Events::LousPartingThoughts::ThugSay1:
                {
                    Creature* thug2 = ObjectAccessor::GetCreature(*me, _summonGUIDs[1]);
                    if (invoker && thug2)
                        thug2->AI()->Talk(Text::ThugText::ThugSay1, invoker);
                    _events.ScheduleEvent(Events::LousPartingThoughts::ThugSay2, 5s);
                    break;
                }
                case Events::LousPartingThoughts::ThugSay2:
                {
                    Creature* thug2 = ObjectAccessor::GetCreature(*me, _summonGUIDs[1]);
                    if (invoker && thug2)
                        thug2->AI()->Talk(Text::ThugText::ThugSay2, invoker);
                    _events.ScheduleEvent(Events::LousPartingThoughts::ThugSay3, 8s + 500ms);
                    break;
                }
                case Events::LousPartingThoughts::ThugSay3:
                {
                    Creature* thug3 = ObjectAccessor::GetCreature(*me, _summonGUIDs[2]);
                    if (invoker && thug3)
                        thug3->AI()->Talk(Text::ThugText::ThugSay3, invoker);
                    _events.ScheduleEvent(Events::LousPartingThoughts::ThugSay4, 5s);
                    break;
                }
                case Events::LousPartingThoughts::ThugSay4:
                {
                    Creature* thug1 = ObjectAccessor::GetCreature(*me, _summonGUIDs[0]);
                    if (invoker && thug1)
                    {
                        thug1->AI()->Talk(Text::ThugText::ThugSay4, invoker);

                        for (ObjectGuid const& guid : _summonGUIDs)
                            if (Creature* thug = ObjectAccessor::GetCreature(*me, guid))
                                thug->SetFacingToObject(invoker);
                    }

                    _events.ScheduleEvent(Events::LousPartingThoughts::ThugSay5, 8s + 500ms);
                    break;
                }
                case Events::LousPartingThoughts::ThugSay5:
                {
                    Creature* thug1 = ObjectAccessor::GetCreature(*me, _summonGUIDs[0]);
                    if (invoker && thug1)
                        thug1->AI()->Talk(Text::ThugText::ThugSay5, invoker);

                    _events.ScheduleEvent(Events::LousPartingThoughts::ThugSay6, 5s);
                    break;
                }
                case Events::LousPartingThoughts::ThugSay6:
                {
                    Creature* thug1 = ObjectAccessor::GetCreature(*me, _summonGUIDs[0]);
                    if (invoker && thug1)
                    {
                        thug1->AI()->Talk(Text::ThugText::ThugSay6, invoker);

                        for (ObjectGuid const& guid : _summonGUIDs)
                            if (Creature* thug = ObjectAccessor::GetCreature(*me, guid))
                            {
                                thug->SetImmuneToAll(false);
                                thug->AI()->AttackStart(invoker);
                            }
                    }
                    break;
                }
                case Events::LousPartingThoughts::ThugCredit:
                {
                    if (invoker)
                    {
                        invoker->CastSpell(invoker, Spells::HoboInformation, TriggerCastFlags(TRIGGERED_FULL_MASK));
                        for (ObjectGuid tapperGUID : me->GetTapList())
                        {
                            Player* tapper = ObjectAccessor::GetPlayer(*me, tapperGUID);
                            if (!tapper || tapper->GetQuestStatus(Quests::LousPartingThoughts) != QUEST_STATUS_INCOMPLETE || tapper->GetDistance(me) > 75.f)
                                continue;

                            tapper->CastSpell(tapper, Spells::HoboInformation, false);
                        }
                    }
                    _events.ScheduleEvent(Events::LousPartingThoughts::ThugShoot1, 1s + 500ms);
                    break;
                }
                case Events::LousPartingThoughts::ThugShoot1:
                {
                    if (invoker)
                        me->PlayDistanceSound(Sounds::Shooting, invoker);

                    _events.ScheduleEvent(Events::LousPartingThoughts::ThugShoot2, 1s + 200ms);
                    break;
                }
                case Events::LousPartingThoughts::ThugShoot2:
                {
                    if (invoker)
                        me->PlayDistanceSound(Sounds::Shooting, invoker);

                    _events.ScheduleEvent(Events::LousPartingThoughts::ThugScream, 1s + 200ms);
                    break;
                }
                case Events::LousPartingThoughts::ThugScream:
                {
                    if (invoker)
                        me->PlayDistanceSound(Sounds::Scream, invoker);

                    _events.ScheduleEvent(Events::LousPartingThoughts::ThugWarning, 2s + 500ms);
                    break;
                }
                case Events::LousPartingThoughts::ThugWarning:
                {
                    if (invoker)
                        Talk(Text::ThugText::SayWarning, invoker);
                    break;
                }
                default:
                    break;
            }
        }
    }

private:
    EventMap _events;
    ObjectGuid _eventInvokerGUID;
    std::array<ObjectGuid, 4> _summonGUIDs;
    uint32 _thugDeathCount;
};

// 42390 - Areatrigger
class at_westfall_small_time_hustler : public AreaTriggerScript
{
public:
    at_westfall_small_time_hustler() : AreaTriggerScript("at_westfall_small_time_hustler") {}

    bool OnTrigger(Player* player, AreaTriggerEntry const* /*at*/) override
    {
        if (player->IsAlive())
            if (Creature* lousPartingThoughtsTrigger = player->FindNearestCreature(Creatures::SmallTimeHustler, 10.0f, true))
                lousPartingThoughtsTrigger->AI()->SetGUID(player->GetGUID(), 0);

        return true;
    }
};

// 42390 - Small Time Hustler
struct npc_westfall_small_time_hustler : public ScriptedAI
{
    npc_westfall_small_time_hustler(Creature* creature) : ScriptedAI(creature) {}

    void SetGUID(ObjectGuid const& guid, int32 /*id*/) override
    {
        if (!_eventInvokerGUID.IsEmpty())
            return;

        _eventInvokerGUID = guid;

        _events.ScheduleEvent(Events::LousPartingThoughts::HustlerSay0, 0s);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);
        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case Events::LousPartingThoughts::HustlerSay0:
                {
                    Player* invoker = ObjectAccessor::GetPlayer(*me, _eventInvokerGUID);
                    if (invoker)
                    {
                        me->SetFacingToObject(invoker);
                        Talk(Text::SmallTimeHustlerText::HustlerSay0, invoker);
                        _events.ScheduleEvent(Events::LousPartingThoughts::HustlerSay1, 4s);
                    }
                    break;
                }
                case Events::LousPartingThoughts::HustlerSay1:
                {
                    Player* invoker = ObjectAccessor::GetPlayer(*me, _eventInvokerGUID);
                    if (invoker)
                    {
                        Talk(Text::SmallTimeHustlerText::HustlerSay1, invoker);
                        _events.ScheduleEvent(Events::LousPartingThoughts::HustlerEventReset, 60s);
                    }
                    break;
                }
                case Events::LousPartingThoughts::HustlerEventReset:
                {
                    _eventInvokerGUID = ObjectGuid::Empty;
                    break;
                }
                default:
                    break;
            }
        }
    }

private:
    EventMap _events;
    ObjectGuid _eventInvokerGUID;
};
}

void AddSC_westfall()
{
    using namespace Scripts::EasternKingdoms::Westfall;

    // Creature
    RegisterCreatureAI(npc_westfall_overloaded_harvest_golem);
    RegisterCreatureAI(npc_westfall_hobo_witness);
    RegisterCreatureAI(npc_westfall_thug);
    RegisterCreatureAI(npc_westfall_lous_parting_thoughts_trigger);
    RegisterCreatureAI(npc_westfall_small_time_hustler);
    RegisterCreatureAI(npc_westfall_westfall_stew);
    RegisterCreatureAI(npc_westfall_westfall_citizen);
    RegisterCreatureAI(npc_westfall_westfall_citizen_quiet);
    RegisterCreatureAI(npc_westfall_refugee_bridge_to_sentinelhill);
    RegisterCreatureAI(npc_westfall_west_plains_drifter_walk_to_sentinel);

    // Areatrigger
    new at_westfall_two_shoed_lou_thugs();
    new at_westfall_small_time_hustler();

    // Spells
    RegisterSpellScript(spell_westfall_unbound_energy);
    RegisterSpellScript(spell_westfall_reaping_blows);
    RegisterSpellScript(spell_westfall_wake_harvest_golem);
    RegisterSpellScript(spell_westfall_summon_lous_house);
    RegisterSpellScript(spell_westfall_quest_credit_jangolode_event);
    RegisterSpellScript(spell_westfall_despawn_jangolode_actor);
    RegisterSpellScript(spell_westfall_livin_the_life_ping_glubtok);
    RegisterSpellScript(spell_westfall_livin_the_life_ping_figure);
    RegisterSpellScript(spell_westfall_summon_ragamuffin_looter);
    RegisterSpellScript(spell_westfall_aggro_hobo);
}
