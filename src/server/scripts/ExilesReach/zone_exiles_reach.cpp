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

#include "ScriptMgr.h"
#include "AreaTrigger.h"
#include "AreaTriggerAI.h"
#include "CellImpl.h"
#include "CombatAI.h"
#include "Containers.h"
#include "ConditionMgr.h"
#include "Conversation.h"
#include "CreatureTextMgr.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "GameObject.h"
#include "MapManager.h"
#include "MapUtils.h"
#include "MotionMaster.h"
#include "MoveSpline.h"
#include "ObjectAccessor.h"
#include "PassiveAI.h"
#include "PathGenerator.h"
#include "PhasingHandler.h"
#include "Player.h"
#include "QuaternionData.h"
#include "Random.h"
#include "ScriptedCreature.h"
#include "SpellAuras.h"
#include "SpellHistory.h"
#include "SpellInfo.h"
#include "SpellScript.h"
#include "TemporarySummon.h"
#include "Vehicle.h"
#include "VehicleDefines.h"
#include "WaypointManager.h"

#include <cmath>
#include "WorldStateMgr.h"

template<class privateAI, class publicAI>
CreatureAI* GetPrivatePublicPairAISelector(Creature* creature)
{
    if (creature->IsPrivateObject())
        return new privateAI(creature);
    return new publicAI(creature);
}

#define RegisterPrivatePublicCreatureAIPair(scriptName, privateAI, publicAI) new FactoryCreatureScript<CreatureAI, &GetPrivatePublicPairAISelector<privateAI, publicAI>>(scriptName);

static Creature* FindCreatureIgnorePhase(WorldObject const* obj, std::string_view stringId, float range = 100.0f)
{
    return obj->FindNearestCreatureWithOptions(range, { .StringId = stringId, .IgnorePhases = true });
}

 // ********************************************
 // * Scripting in this section occurs on ship *
 // ********************************************

enum AttentionExilesReachData
{
    SPELL_DEBUG_LOOK_RIGHT = 290903
};

// 290901 - Attention!
class spell_attention_exiles_reach_tutorial : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DEBUG_LOOK_RIGHT });
    }

    void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetTarget()->CastSpell(GetTarget(), SPELL_DEBUG_LOOK_RIGHT, true);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_attention_exiles_reach_tutorial::OnRemove, EFFECT_0, SPELL_AURA_MOD_ROOT, AURA_EFFECT_HANDLE_REAL);
    }
};

enum WarmingUpData
{
    CONVERSATION_WARMING_UP_ACCEPT          = 12818,
    CONVERSATION_WARMING_UP_COMPLETE        = 12798,
};

class BaseQuestWarmingUp : public QuestScript
{
public:
    BaseQuestWarmingUp(const char* name) : QuestScript(name) { }

    void OnQuestStatusChange(Player* player, Quest const* /*quest*/, QuestStatus /*oldStatus*/, QuestStatus newStatus)
    {
        if (newStatus == QUEST_STATUS_INCOMPLETE)
            Conversation::CreateConversation(CONVERSATION_WARMING_UP_ACCEPT, player, *player, player->GetGUID(), nullptr);
        else if (newStatus == QUEST_STATUS_COMPLETE)
            Conversation::CreateConversation(CONVERSATION_WARMING_UP_COMPLETE, player, *player, player->GetGUID(), nullptr);
    }
};

class q56775_warming_up : public BaseQuestWarmingUp
{
public:
    q56775_warming_up() : BaseQuestWarmingUp("q56775_warming_up") { }

    static constexpr float CLONE_ORIENTATION = 5.124503135681152343f;
    static constexpr float CLONE_Z_OFFSET = 0.308f;

    void OnQuestStatusChange(Player* player, Quest const* quest, QuestStatus oldStatus, QuestStatus newStatus) override
    {
        BaseQuestWarmingUp::OnQuestStatusChange(player, quest, oldStatus, newStatus);

        if (newStatus == QUEST_STATUS_REWARDED)
        {
            Creature* garrickLowerDeck = FindCreatureIgnorePhase(player, "q56775_garrick_lower_deck", 5.0f);
            Creature* garrickUpperDeck = FindCreatureIgnorePhase(player, "q56775_garrick_upper_deck", 75.0f);
            if (!garrickLowerDeck || !garrickUpperDeck)
                return;

            Position pos(garrickLowerDeck->GetPositionX(), garrickLowerDeck->GetPositionY(), garrickLowerDeck->GetPositionZ() - CLONE_Z_OFFSET, CLONE_ORIENTATION);
            garrickUpperDeck->SummonPersonalClone(pos, TEMPSUMMON_MANUAL_DESPAWN, 0s, 0, 0, player);
        }
    }
};

class q59926_warming_up : public BaseQuestWarmingUp
{
public:
    q59926_warming_up() : BaseQuestWarmingUp("q59926_warming_up") { }

    void OnQuestStatusChange(Player* player, Quest const* quest, QuestStatus oldStatus, QuestStatus newStatus) override
    {
        BaseQuestWarmingUp::OnQuestStatusChange(player, quest, oldStatus, newStatus);

        if (newStatus == QUEST_STATUS_REWARDED)
        {
            Creature* grimaxeLowerDeck = FindCreatureIgnorePhase(player, "q59926_grimaxe_lower_deck", 5.0f);
            Creature* grimaxeUpperDeck = FindCreatureIgnorePhase(player, "q59926_grimaxe_upper_deck", 75.0f);
            if (!grimaxeLowerDeck || !grimaxeUpperDeck)
                return;

            grimaxeUpperDeck->SummonPersonalClone(*grimaxeLowerDeck, TEMPSUMMON_MANUAL_DESPAWN, 0s, 0, 0, player);
        }
    }
};

enum WarmingUpCaptainData
{
    NPC_WARLORD_BREKA_GRIMAXE2      = 166824,
    NPC_WARLORD_BREKA_GRIMAXE3      = 166827,
    NPC_CAPTAIN_GARRICK             = 156280,

    PATH_GARRICK_TO_COLE            = 10501450,
    PATH_GARRICK_TO_UPPER_DECK      = 10501451,
    PATH_GRIMAXE_TO_THROG           = 10501900,
    PATH_GRIMAXE_TO_UPPER_DECK      = 10501901,

    EVENT_SHIP_CAPTAIN1_SCRIPT1     = 1,
    EVENT_SHIP_CAPTAIN1_SCRIPT2,
    EVENT_SHIP_CAPTAIN1_SCRIPT3,

    SAY_SPAR                        = 0,
};

// 156280 - Captain Garrick
// 166824 - Warlord Breka Grimaxe
struct npc_ship_captain_warming_up_private : public ScriptedAI
{
    npc_ship_captain_warming_up_private(Creature* creature) : ScriptedAI(creature), _pathToSparringPartner(0), _pathToUpperDeck(0) { }

    void InitializeAI() override
    {
        me->RemoveNpcFlag(UNIT_NPC_FLAG_QUESTGIVER);
    }

    void JustAppeared() override
    {
        if (me->GetEntry() == NPC_CAPTAIN_GARRICK)
        {
            _pathToSparringPartner = PATH_GARRICK_TO_COLE;
            _pathToUpperDeck = PATH_GARRICK_TO_UPPER_DECK;
        }
        else if (me->GetEntry() == NPC_WARLORD_BREKA_GRIMAXE2)
        {
            _pathToSparringPartner = PATH_GRIMAXE_TO_THROG;
            _pathToUpperDeck = PATH_GRIMAXE_TO_UPPER_DECK;
        }

        _events.ScheduleEvent(EVENT_SHIP_CAPTAIN1_SCRIPT1, 1s);
    }

    void WaypointPathEnded(uint32 /*nodeId*/, uint32 pathId) override
    {
        if (pathId == _pathToSparringPartner)
            _events.ScheduleEvent(EVENT_SHIP_CAPTAIN1_SCRIPT2, 0s);
        else if (pathId == _pathToUpperDeck)
            me->DespawnOrUnsummon();
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_SHIP_CAPTAIN1_SCRIPT1:
                    Talk(SAY_SPAR);
                    me->GetMotionMaster()->MovePath(_pathToSparringPartner, false);
                    break;
                case EVENT_SHIP_CAPTAIN1_SCRIPT2:
                    me->HandleEmoteCommand(EMOTE_ONESHOT_SALUTE);
                    _events.ScheduleEvent(EVENT_SHIP_CAPTAIN1_SCRIPT3, 3s);
                    break;
                case EVENT_SHIP_CAPTAIN1_SCRIPT3:
                    me->GetMotionMaster()->MovePath(_pathToUpperDeck, false);
                    break;
                default:
                    break;
            }
        }
    }
private:
    EventMap _events;
    uint32 _pathToSparringPartner;
    uint32 _pathToUpperDeck;
};

enum StandYourGroundData
{
    ACTOR_ID_ALLIANCE                   = 68598,
    ACTOR_ID_HORDE                      = 75920,

    CONVERSATION_PREFIGHT               = 14422,
    CONVERSATION_AGGRO                  = 14423,
    CONVERSATION_JUMP                   = 14424,

    EQUIPMENT_SWORD                     = 108493,
    EQUIPMENT_AXE                       = 175161,

    EVENT_MOVE_TO_A_POSITION            = 1,
    EVENT_PREFIGHT_CONVERSATION,
    EVENT_JUMP_BEHIND,
    EVENT_WALK_BACK,

    PATH_ALLIANCE_SPARING_PARTNER       = 10501460,
    PATH_HORDE_SPARING_PARTNER          = 10501870,

    POSITION_SPARPOINT_ADVERTISMENT     = 1,
    POSITION_SPARPOINT_READY            = 2,

    TALK_SPARING_COMPLETE               = 0,

    NPC_ALLIANCE_SPARING_PARTNER        = 157051,
    NPC_HORDE_SPARING_PARTNER           = 166814,
    NPC_SPAR_POINT_ADVERTISMENT         = 174971,
    NPC_KILL_CREDIT                     = 155607,

    SPELL_COMBAT_TRAINING_COMPLETE      = 303120,
    SPELL_JUMP_LEFT                     = 312757,
    SPELL_JUMP_BEHIND                   = 312755,
    SPELL_COMBAT_TRAINING               = 323071,
    SPELL_UPDATE_PHASE_SHIFT            = 82238,
};

// 58209 - Stand Your Ground
// 59927 - Stand Your Ground
class quest_stand_your_ground : public QuestScript
{
public:
    quest_stand_your_ground() : QuestScript("quest_stand_your_ground") { }

    void OnQuestStatusChange(Player* player, Quest const* /*quest*/, QuestStatus /*oldStatus*/, QuestStatus newStatus) override
    {
        // Remove aura if player drops quest
        if (newStatus == QUEST_STATUS_NONE)
        {
            player->CastSpell(player, SPELL_UPDATE_PHASE_SHIFT);
            player->CastSpell(player, SPELL_COMBAT_TRAINING_COMPLETE);
        }
    }
};

// 303065 - Summon Cole - Combat Training (DNT)
// 325108 - Summon Throg - Combat Training (DNT)
class spell_summon_sparring_partner : public SpellScript
{
    // @TODO: drop after TARGET_UNK_142 impl

    void SelectTarget(WorldObject*& target)
    {
        Player* caster = GetCaster()->ToPlayer();
        if (!caster)
            return;

        Creature* partner = FindCreatureIgnorePhase(caster, caster->GetTeam() == ALLIANCE ? "q58209_cole" : "q59927_throg", 10.0f);
        if (!partner)
            return;

        target = partner;
    }

    void Register() override
    {
        OnObjectTargetSelect += SpellObjectTargetSelectFn(spell_summon_sparring_partner::SelectTarget, EFFECT_0, TARGET_DEST_NEARBY_ENTRY_OR_DB);
    }
};

// 157051 - Alliance Sparring Partner
// 166814 - Horde Sparring Partner
struct npc_sparring_partner_exiles_reach : public ScriptedAI
{
    npc_sparring_partner_exiles_reach(Creature* creature) : ScriptedAI(creature), _jumped(false), _actorIndex(0), _actorId(0), _path(0) { }

    void JustAppeared() override
    {
        if (me->GetEntry() == NPC_ALLIANCE_SPARING_PARTNER)
        {
            SetEquipmentSlots(false, EQUIPMENT_SWORD, EQUIP_NO_CHANGE, EQUIP_NO_CHANGE);
            _path = PATH_ALLIANCE_SPARING_PARTNER;
            _actorId = ACTOR_ID_ALLIANCE;
            _actorIndex = 0;
        }
        else if (me->GetEntry() == NPC_HORDE_SPARING_PARTNER)
        {
            SetEquipmentSlots(false, EQUIPMENT_AXE, EQUIP_NO_CHANGE, EQUIP_NO_CHANGE);
            _path = PATH_HORDE_SPARING_PARTNER;
            _actorId = ACTOR_ID_HORDE;
            _actorIndex = 1;
        }
        me->SetImmuneToPC(true);
        _events.ScheduleEvent(EVENT_MOVE_TO_A_POSITION, 1s);
    }

    void EnterEvadeMode(EvadeReason /*why*/) override
    {
        if (!me->IsAlive())
            return;

        me->CombatStop(true);
        EngagementOver();
        me->ResetPlayerDamageReq();
        _events.ScheduleEvent(EVENT_WALK_BACK, 1s);
    }

    void MovementInform(uint32 uiType, uint32 uiId) override
    {
        if (uiType != POINT_MOTION_TYPE)
            return;

        switch (uiId)
        {
            case POSITION_SPARPOINT_ADVERTISMENT:
                me->SetWalk(true);
                me->GetMotionMaster()->MovePoint(POSITION_SPARPOINT_READY, me->GetFirstCollisionPosition(2.0f, rand_norm() * static_cast<float>(2 * M_PI)));
                break;
            case POSITION_SPARPOINT_READY:
                if (Unit* owner = me->GetDemonCreator())
                    me->SetFacingToObject(owner);
                me->SetImmuneToPC(false);
                me->SetUninteractible(false);
                break;
            default:
                break;
        }
    }

    void WaypointPathEnded(uint32 /*nodeId*/, uint32 pathId) override
    {
        if (pathId != _path)
            return;

        if (Unit* owner = me->GetDemonCreator())
        {
            me->DespawnOrUnsummon();
            owner->CastSpell(owner, SPELL_UPDATE_PHASE_SHIFT);
            owner->CastSpell(owner, SPELL_COMBAT_TRAINING_COMPLETE);
        }
    }

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo = nullptr*/) override
    {
        if (me->GetHealth() <= damage)
        {
            damage = 0;
            me->SetHealth(1);
            DoStopAttack();
            me->SetImmuneToPC(true);
            me->SetUninteractible(true);
            _events.CancelEvent(EVENT_JUMP_BEHIND);

            if (Player* player = me->GetDemonCreatorPlayer())
            {
                me->SetFacingToObject(player);
                Talk(TALK_SPARING_COMPLETE, player);
                player->CastSpell(player, SPELL_COMBAT_TRAINING);
                player->KilledMonsterCredit(NPC_KILL_CREDIT);
            }
        }

        if (me->HealthBelowPctDamaged(65, damage) && !_jumped)
        {
            _jumped = true;
            DoCastVictim(SPELL_JUMP_LEFT, true);
            StartPrivateConversation(CONVERSATION_JUMP);
            _events.ScheduleEvent(EVENT_JUMP_BEHIND, 2s);
        }
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        StartPrivateConversation(CONVERSATION_AGGRO);
    }

    void DamageDealt(Unit* target, uint32& damage, DamageEffectType /*damageType*/) override
    {
        if (target->GetHealthPct() < 95)
            damage = 0;
    }

    void StartPrivateConversation(uint32 conversationId)
    {
        if (Unit* owner = me->GetDemonCreator())
        {
            Conversation* conversation = Conversation::CreateConversation(conversationId, owner, *owner, owner->GetGUID(), nullptr, false);
            conversation->AddActor(_actorId, _actorIndex, me->GetGUID());
            conversation->Start();
        }
    }

    Creature* GetRandomSparPoint()
    {
        std::list<Creature*> sparPoints;
        GetCreatureListWithEntryInGrid(sparPoints, me, NPC_SPAR_POINT_ADVERTISMENT, 25.0f);
        Trinity::Containers::RandomResize(sparPoints, 1);

        if (sparPoints.empty()) // should never happen
            return nullptr;

        return sparPoints.front();
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_MOVE_TO_A_POSITION:
                {
                    if (Creature* sparPoint = GetRandomSparPoint())
                        me->GetMotionMaster()->MovePoint(POSITION_SPARPOINT_ADVERTISMENT, sparPoint->GetPosition());

                    _events.ScheduleEvent(EVENT_PREFIGHT_CONVERSATION, 1s);
                    break;
                }
                case EVENT_PREFIGHT_CONVERSATION:
                    StartPrivateConversation(CONVERSATION_PREFIGHT);
                    break;
                case EVENT_JUMP_BEHIND:
                    DoCastVictim(SPELL_JUMP_BEHIND, true);
                    break;
                case EVENT_WALK_BACK:
                    me->GetMotionMaster()->Clear();
                    me->GetMotionMaster()->MovePath(_path, false);
                    break;
                default:
                    break;
            }
        }

        if (!UpdateVictim())
            return;
    }
private:
    EventMap _events;
    bool _jumped;
    uint8 _actorIndex;
    uint32 _actorId;
    uint32 _path;
};

enum FirstMateStandYourGroundData
{
    QUEST_STAND_YOUR_GROUND_ALLIANCE    = 58209,
    QUEST_STAND_YOUR_GROUND_HORDE       = 59927,

    SPELL_SUMMON_COLE                   = 303064,
    SPELL_SUMMON_THROG                  = 325107,
};

// 160664 - Private Cole
// 166583 - Grunt Throg
struct npc_first_mate_stand_your_ground : public ScriptedAI
{
    npc_first_mate_stand_your_ground(Creature* creature) : ScriptedAI(creature) { }

    void OnQuestAccept(Player* player, Quest const* quest) override
    {
        if (quest->GetQuestId() == QUEST_STAND_YOUR_GROUND_ALLIANCE)
        {
            player->CastSpell(player, SPELL_UPDATE_PHASE_SHIFT);
            player->CastSpell(player, SPELL_SUMMON_COLE);
        }
        else if (quest->GetQuestId() == QUEST_STAND_YOUR_GROUND_HORDE)
        {
            player->CastSpell(player, SPELL_UPDATE_PHASE_SHIFT);
            player->CastSpell(player, SPELL_SUMMON_THROG);
        }
    }
};

struct ActorData
{
    std::string_view StringId;
    Position ActorPosition;
};

static std::vector<ActorData> const ActorDataMap[2] =
{
    // TEAM_ALLIANCE
    {
        { "q58208_garrick",  { 35.5643f, -1.19837f, 12.1479f, 3.3272014f }    },
        { "q58208_richter",  { -1.84858f, -8.38776f, 5.10018f, 1.5184366f }   },
        { "q58208_keela",    { -15.3642f, 6.5793f, 5.5026f, 3.1415925f }      },
        { "q58208_bjorn",    { 12.8406f, -8.49553f, 4.98031f, 4.8520155f }    },
        { "q58208_austin",   { -4.48607f, 9.89729f, 5.07851f, 1.5184366f }    },
        { "q58208_cole",     { -13.3396f, 0.702157f, 5.57996f, 0.087266445f } },
    },
    // TEAM_HORDE
    {
        { "q59928_grimaxe",  { 25.5237f, 0.283005f, 26.5455f, 3.3526998f }   },
        { "q59928_throg",    { -10.8399f, 11.9039f, 8.88028f, 6.2308254f }   },
        { "q59928_mithdran", { -24.4763f, -4.48273f, 9.13471f, 0.62831855f } },
        { "q59928_lana",     { -5.1971f, -15.0268f, 8.992f, 4.712389f }      },
        { "q59928_bo",       { -22.1559f, 5.58041f, 9.09176f, 6.143559f }    },
        { "q59928_jinhake",  { -31.9464f, 7.5772f, 10.6408f, 6.0737457f }    },
    }
};

static std::unordered_map<Races, std::string_view> const ActorPetData =
{
    { RACE_HUMAN,             "q58208_wolf"         },
    { RACE_DWARF,             "q58208_bear"         },
    { RACE_NIGHTELF,          "q58208_tiger"        },
    { RACE_GNOME,             "q58208_bunny"        },
    { RACE_DRAENEI,           "q58208_moth"         },
    { RACE_WORGEN,            "q58208_dog"          },
    { RACE_PANDAREN_ALLIANCE, "q58208_turtle"       },
    { RACE_ORC,               "q59928_wolf"         },
    { RACE_UNDEAD_PLAYER,     "q59928_bat"          },
    { RACE_TAUREN,            "q59928_plainstrider" },
    { RACE_TROLL,             "q59928_raptor"       },
    { RACE_GOBLIN,            "q59928_scorpion"     },
    { RACE_BLOODELF,          "q59928_dragonhawk"   },
    { RACE_PANDAREN_HORDE,    "q59928_turtle"       }
};

enum BraceForImpactData
{
    QUEST_BRACE_FOR_IMPACT_ALLIANCE = 58208,
    QUEST_BRACE_FOR_IMPACT_HORDE    = 59928,
};

// 58208 - Brace For Impact
// 59928 - Brace For Impact
class quest_brace_for_impact : public QuestScript
{
public:
    quest_brace_for_impact() : QuestScript("quest_brace_for_impact") { }

    void OnQuestStatusChange(Player* player, Quest const* quest, QuestStatus /*oldStatus*/, QuestStatus newStatus) override
    {
        if (newStatus != QUEST_STATUS_COMPLETE)
            return;

        TeamId team = TEAM_NEUTRAL;
        Position petSpawnPos;

        if (quest->GetQuestId() == QUEST_BRACE_FOR_IMPACT_ALLIANCE)
        {
            team = TEAM_ALLIANCE;
            petSpawnPos = { -1.4492f, 8.06887f,  5.10348f, 2.6005409f };
        }
        else if (quest->GetQuestId() == QUEST_BRACE_FOR_IMPACT_HORDE)
        {
            team = TEAM_HORDE;
            petSpawnPos = { -22.8374f, -3.08287f, 9.12613f, 3.857178f };
        }

        if (team == TEAM_NEUTRAL)
            return;

        SpawnActors(player, team, petSpawnPos);
        player->CastSpell(player, SPELL_UPDATE_PHASE_SHIFT);
    }

    void SpawnActors(Player* player, TeamId team, Position petSpawnPos)
    {
        for (ActorData const& actor : ActorDataMap[team])
            SpawnActor(player, FindCreatureIgnorePhase(player, actor.StringId, 50.0f), actor.ActorPosition);

        SpawnPet(player, petSpawnPos);
    }

    void SpawnPet(Player* player, Position const& position)
    {
        if (player->GetClass() != CLASS_HUNTER)
            return;

        if (std::string_view const* stringId = Trinity::Containers::MapGetValuePtr(ActorPetData, Races(player->GetRace())))
        {
            Creature* pet = FindCreatureIgnorePhase(player, *stringId, 25.0f);
            if (!pet)
                return;

            SpawnActor(player, pet, position);
        }
    }

    void SpawnActor(Player* player, Creature* creature, Position const& position)
    {
        TransportBase const* transport = player->GetDirectTransport();

        if (!transport || !creature)
            return;

        creature->SummonPersonalClone(transport->GetPositionWithOffset(position), TEMPSUMMON_MANUAL_DESPAWN, 0s, 0, 0, player);
    }
};

enum BraceForImpactCaptainData
{
    PATH_GARRICK_FROM_UPPER_DECK    = 10505890,
    PATH_GARRICK_TO_LOWER_DECK      = 10505891,
    PATH_GRIMAXE_FROM_UPPER_DECK    = 10501910,
    PATH_GRIMAXE_TO_LOWER_DECK      = 10501911,

    EVENT_SHIP_CAPTAIN2_SCRIPT1     = 1,
    EVENT_SHIP_CAPTAIN2_SCRIPT2,

    SAY_GET_TO_POSITIONS            = 1,
};

// 156280 - Captain Garrick
// 166827 - Warlord Breka Grimaxe
struct npc_ship_captain_brace_for_impact_private : public ScriptedAI
{
    npc_ship_captain_brace_for_impact_private(Creature* creature) : ScriptedAI(creature), _pathPreTalk(0), _pathPostTalk(0), _waitTime(0s){ }

    void JustAppeared() override
    {
        if (me->GetEntry() == NPC_CAPTAIN_GARRICK)
        {
            _pathPreTalk = PATH_GARRICK_FROM_UPPER_DECK;
            _pathPostTalk = PATH_GARRICK_TO_LOWER_DECK;
            _waitTime = 0s;
        }
        else if (me->GetEntry() == NPC_WARLORD_BREKA_GRIMAXE3)
        {
            _pathPreTalk = PATH_GRIMAXE_FROM_UPPER_DECK;
            _pathPostTalk = PATH_GRIMAXE_TO_LOWER_DECK;
            _waitTime = 1s;
        }

        me->RemoveNpcFlag(UNIT_NPC_FLAG_QUESTGIVER);
        me->GetMotionMaster()->MovePath(_pathPreTalk, false);
    }

    void WaypointPathEnded(uint32 /*nodeId*/, uint32 pathId) override
    {
        if (pathId == _pathPreTalk)
            _events.ScheduleEvent(EVENT_SHIP_CAPTAIN2_SCRIPT1, _waitTime);
        else if (pathId == _pathPostTalk)
            me->DespawnOrUnsummon();
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_SHIP_CAPTAIN2_SCRIPT1:
                    Talk(SAY_GET_TO_POSITIONS);
                    _events.ScheduleEvent(EVENT_SHIP_CAPTAIN2_SCRIPT2, 3s);
                    break;
                case EVENT_SHIP_CAPTAIN2_SCRIPT2:
                    me->GetMotionMaster()->MovePath(_pathPostTalk, false);
                    break;
                default:
                    break;
            }
        }
    }
private:
    EventMap _events;
    uint32 _pathPreTalk;
    uint32 _pathPostTalk;
    Seconds _waitTime;
};

enum BraceForImpactFirstMateData
{
    NPC_PRIVATE_COLE                = 160664,
    NPC_GRUNT_THROG                 = 166583,

    PATH_COLE_BRACE_FOR_IMPACT      = 10501461,
    PATH_THROG_BRACE_FOR_IMPACT     = 10501871,

    EVENT_FIRST_MATE_1              = 1,
    EVENT_FIRST_MATE_2,

    SAY_STORM = 0,
};

// 160664 - Private Cole
// 166583 - Grunt Throg
struct npc_first_mate_brace_for_impact_private : public ScriptedAI
{
    npc_first_mate_brace_for_impact_private(Creature* creature) : ScriptedAI(creature), _path(0) { }

    void JustAppeared() override
    {
        if (me->GetEntry() == NPC_PRIVATE_COLE)
            _path = PATH_COLE_BRACE_FOR_IMPACT;
        else if (me->GetEntry() == NPC_GRUNT_THROG)
            _path = PATH_THROG_BRACE_FOR_IMPACT;

        _events.ScheduleEvent(EVENT_FIRST_MATE_1, 3s);
    }

    void WaypointPathEnded(uint32 /*nodeId*/, uint32 pathId) override
    {
        if (_path && pathId == _path)
            me->DespawnOrUnsummon();
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_FIRST_MATE_1:
                    Talk(SAY_STORM);
                    _events.ScheduleEvent(EVENT_FIRST_MATE_2, 4s);
                    break;
                case EVENT_FIRST_MATE_2:
                    me->GetMotionMaster()->MovePath(_path, false);
                    break;
                default:
                    break;
            }
        }
    }
private:
    EventMap _events;
    uint32 _path;
};

enum BraceForImpactCrewData
{
    NPC_QUARTERMASTER_RICHTER       = 157042,
    NPC_KEE_LA                      = 157043,
    NPC_BJORN_STOUTHANDS            = 157044,
    NPC_AUSTIN_HUXWORTH             = 157046,

    NPC_BO                          = 166585,
    NPC_MITHDRAN_DAWNTRACKER        = 166590,
    NPC_LANA_JORDAN                 = 166794,
    NPC_PROVISONER_JIN_HAKE         = 166799,

    PATH_RICHTER_BRACE_FOR_IMPACT   = 10501770,
    PATH_KEE_LA_BRACE_FOR_IMPACT    = 10501800,
    PATH_BJORN_BRACE_FOR_IMPACT     = 10501790,
    PATH_AUSTIN_BRACE_FOR_IMPACT    = 10501780,

    PATH_BO_BRACE_FOR_IMPACT        = 10502010,
    PATH_MITHDRAN_BRACE_FOR_IMPACT  = 10501990,
    PATH_LANA_BRACE_FOR_IMPACT      = 10501980,
    PATH_JIN_HAKE_BRACE_FOR_IMPACT  = 10502000,
};

// 157042 - Quartermaster Richter
// 157043 - Kee-La
// 157044 - Bjorn Stouthands
// 157046 - Austin Huxworth
// 166585 - Bo
// 166590 - Mithdran Dawntracker
// 166794 - Lana Jordan
// 166799 - Provisoner Jin'hake
struct npc_crew_ship_private : public ScriptedAI
{
    npc_crew_ship_private(Creature* creature) : ScriptedAI(creature), _path(0) { }

    uint32 GetPathID()
    {
        switch (me->GetEntry())
        {
            case NPC_QUARTERMASTER_RICHTER: return PATH_RICHTER_BRACE_FOR_IMPACT;
            case NPC_KEE_LA:                return PATH_KEE_LA_BRACE_FOR_IMPACT;
            case NPC_BJORN_STOUTHANDS:      return PATH_BJORN_BRACE_FOR_IMPACT;
            case NPC_AUSTIN_HUXWORTH:       return PATH_AUSTIN_BRACE_FOR_IMPACT;
            case NPC_BO:                    return PATH_BO_BRACE_FOR_IMPACT;
            case NPC_MITHDRAN_DAWNTRACKER:  return PATH_MITHDRAN_BRACE_FOR_IMPACT;
            case NPC_LANA_JORDAN:           return PATH_LANA_BRACE_FOR_IMPACT;
            case NPC_PROVISONER_JIN_HAKE:   return PATH_JIN_HAKE_BRACE_FOR_IMPACT;
            default:                        return 0;
        }
    }

    void JustAppeared() override
    {
        _path = GetPathID();
        _scheduler.Schedule(Seconds(7), [this](TaskContext const&)
        {
            me->GetMotionMaster()->MovePath(_path, false);
        });
    }

    void WaypointPathEnded(uint32 /*nodeId*/, uint32 pathId) override
    {
        if (_path && pathId == _path)
            me->DespawnOrUnsummon();
    }

    void UpdateAI(uint32 diff) override
    {
        _scheduler.Update(diff);
    }

private:
    TaskScheduler _scheduler;
    uint32 _path;
};

enum BraceForImpactPetData
{
    EVENT_PET_SHIP_RUN_TO_POSITION = 1,

    MAP_ALLIANCE_SHIP              = 2261,
    MAP_HORDE_SHIP                 = 2369,

    PATH_PET_ALLIANCE_SHIP         = 10501510,
    PATH_PET_HORDE_SHIP            = 10502020
};

// 167337 - Mechanical Bunny
// 167342 - Moth
// 167343 - Dragonhawk
// 167344 - Scorpion
// 167345 - Wolf
// 167346 - Wolf
// 167347 - Tiger
// 167348 - Turtle
// 167349 - Plainstrider
// 167350 - Raptor
// 167351 - Bat
// 167352 - Dog
// 167375 - Bear
struct npc_pet_ship_private : public ScriptedAI
{
    npc_pet_ship_private(Creature* creature) : ScriptedAI(creature), _path(0) { }

    void JustAppeared() override
    {
        if (!me->GetTransport())
            return;

        int32 transportMap = me->GetTransport()->GetMapIdForSpawning();
        if (transportMap == MAP_ALLIANCE_SHIP)
            _path = PATH_PET_ALLIANCE_SHIP;
        else if (transportMap == MAP_HORDE_SHIP)
            _path = PATH_PET_HORDE_SHIP;

        if (_path)
            _events.ScheduleEvent(EVENT_PET_SHIP_RUN_TO_POSITION, 8s);
    }

    void WaypointPathEnded(uint32 /*nodeId*/, uint32 /*pathId*/) override
    {
        me->DespawnOrUnsummon();
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            if (eventId == EVENT_PET_SHIP_RUN_TO_POSITION)
                me->GetMotionMaster()->MovePath(_path, false);
        }
    }

private:
    EventMap _events;
    uint32 _path;
};

enum ExilesReachShipCrashData
{
    MOVIE_ALLIANCE_SHIP_CRASH       = 895,
    MOVIE_HORDE_SHIP_CRASH          = 931,

    SPELL_ALLIANCE_SHIP_CRASH       = 305446,
    SPELL_HORDE_SHIP_CRASH          = 325133,
    SPELL_BEGIN_TUTORIAL            = 295600,
};

class player_exiles_reach_ship_crash : public PlayerScript
{
public:
    player_exiles_reach_ship_crash() : PlayerScript("player_exiles_reach_ship_crash") { }

    void OnMovieComplete(Player* player, uint32 movieId) override
    {
        switch (movieId)
        {
            case MOVIE_ALLIANCE_SHIP_CRASH:
                player->CastSpell(player, SPELL_ALLIANCE_SHIP_CRASH, true);
                break;
            case MOVIE_HORDE_SHIP_CRASH:
                player->CastSpell(player, SPELL_HORDE_SHIP_CRASH, true);
                break;
            default:
                break;
        }
    }
};

class scene_alliance_and_horde_ship : public SceneScript
{
public:
    scene_alliance_and_horde_ship() : SceneScript("scene_alliance_and_horde_ship") { }

    void StartConvo(Player* player)
    {
        // This script is used to send conversation to Captian Garrick and Warlord Grimaxe on movement after entering ship
        player->CastSpell(player, SPELL_BEGIN_TUTORIAL, true);
    }

    void OnSceneComplete(Player* player, uint32 /*sceneInstanceID*/, SceneTemplate const* /*sceneTemplate*/) override
    {
        StartConvo(player);
    }

    void OnSceneCancel(Player* player, uint32 /*sceneInstanceID*/, SceneTemplate const* /*sceneTemplate*/) override
    {
        StartConvo(player);
    }
};

// ***************************************************************
// * Scripting in this section occurs after teleporting to beach *
// ***************************************************************

enum KnockedDownExilesReachData
{
    SPELL_KNOCKED_DOWN_STUN2 = 344889
};

// 305445 - Knocked Down!
class spell_knocked_down_exiles_reach_beach : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_KNOCKED_DOWN_STUN2 });
    }

    void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetTarget()->CastSpell(nullptr, SPELL_KNOCKED_DOWN_STUN2, true);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_knocked_down_exiles_reach_beach::OnRemove, EFFECT_0, SPELL_AURA_MOD_STUN, AURA_EFFECT_HANDLE_REAL);
    }
};

enum ExilesReachShipCrashBeachData
{
    SPELL_KNOCKED_DOWN              = 305445,
    SPELL_CRASHED_LANDED_ALLIANCE   = 305464,
    SPELL_CRASHED_LANDED_HORDE      = 325136
};

// Script scene for washed up on beach to cast spells Alliance and Horde
class scene_alliance_and_horde_crash : public SceneScript
{
public:
    scene_alliance_and_horde_crash() : SceneScript("scene_alliance_and_horde_crash") { }

    void OnSceneTriggerEvent(Player* player, uint32 /*sceneInstanceID*/, SceneTemplate const* /*sceneTemplate*/, std::string const& triggerName) override
    {
        if (triggerName == "Begin Knockdown Aura")
            player->CastSpell(player, SPELL_KNOCKED_DOWN, true);
    }

    void OnSceneComplete(Player* player, uint32 /*sceneInstanceID*/, SceneTemplate const* /*sceneTemplate*/) override
    {
        player->CastSpell(player, player->GetTeam() == ALLIANCE ? SPELL_CRASHED_LANDED_ALLIANCE : SPELL_CRASHED_LANDED_HORDE, true);
    }
};

CreatureAI* CaptainGarrickAISelector(Creature* creature)
{
    if (creature->IsPrivateObject())
    {
        if (Player* privateObjectOwner = ObjectAccessor::GetPlayer(*creature, creature->GetPrivateObjectOwner()))
        {
            if ((privateObjectOwner->GetTeam() == ALLIANCE && privateObjectOwner->GetQuestStatus(QUEST_BRACE_FOR_IMPACT_ALLIANCE) == QUEST_STATUS_NONE))
                return new npc_ship_captain_warming_up_private(creature);
            else
                return new npc_ship_captain_brace_for_impact_private(creature);
        }
    }

    return new NullCreatureAI(creature);
}

enum SpellCrashLandedData
{
    NPC_CAPTAIN_GARRICK_BEACH       = 156626,
    NPC_WARLORD_BREKA_GRIMAXE_BEACH = 166782
};

// 305464 - Crash Landed
// 325136 - Crash Landed
template<uint32 StaticCaptainNPCId>
class spell_crash_landed_generic : public SpellScript
{
    void HandleEffect(SpellEffIndex /*effIndex*/)
    {
        Player* player = GetCaster()->ToPlayer();
        if (!player)
            return;

        if (Creature* creature = player->FindNearestCreature(StaticCaptainNPCId, 50.0f))
            creature->SummonPersonalClone(creature->GetPosition(), TempSummonType(TEMPSUMMON_MANUAL_DESPAWN), 0s, 0, 0, player);
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_crash_landed_generic::HandleEffect, EFFECT_0, SPELL_EFFECT_SEND_EVENT);
    }
};

enum ExilesReachCaptainsBeachData
{
    CONVERSATION_QUEST_MURLOC_MANIA_ALLIANCE               = 12043,
    CONVERSATION_QUEST_MURLOC_MANIA_HORDE                  = 14432,

    EVENT_EMERGENCY_FIRST_AID_SCRIPT_SHEATH                = 1,
    EVENT_EMERGENCY_FIRST_AID_SCRIPT_BANDAGE,
    EVENT_EMERGENCY_FIRST_AID_SCRIPT_MOVE_SECOND_SURVIVOR,
    EVENT_EMERGENCY_FIRST_AID_SCRIPT_MOVE_HOME_BEACH,

    POINT_SECOND_SURVIVOR                                  = 0,
    POINT_BEACH_HOME                                       = 1,

    QUEST_MURLOC_MANIA_ALLIANCE                            = 55122,
    QUEST_EMERGENCY_FIRST_AID_ALLIANCE                     = 54951,
    QUEST_MURLOC_MANIA_HORDE                               = 59929,
    QUEST_EMERGENCY_FIRST_AID_HORDE                        = 59930,

    SPELL_BANDAGING                                        = 305584,

    TALK_ARRIVED_AT_BEACH                                  = 0
};

// 156626 - Captain Garrick
// 166782 - Warlord Breka Grimaxe
struct npc_captain_warlord_beach_arrive_private : public ScriptedAI
{
    npc_captain_warlord_beach_arrive_private(Creature* creature) : ScriptedAI(creature) { }

    void IsSummonedBy(WorldObject* summonerWO) override
    {
        Player* summoner = summonerWO->ToPlayer();
        if (!summoner)
            return;

        me->SetFacingToObject(summoner);
        Talk(TALK_ARRIVED_AT_BEACH, summoner);
        me->DespawnOrUnsummon(5s);
    }
};

// 156626 - Captain Garrick
// 166782 - Warlord Breka Grimaxe
// for Emergency first aid quest 54951
struct npc_captain_warlord_first_aid_private : public ScriptedAI
{
    npc_captain_warlord_first_aid_private(Creature* creature) : ScriptedAI(creature) { }

    void InitializeAI() override
    {
        me->RemoveNpcFlag(UNIT_NPC_FLAG_QUESTGIVER);
    }

    void IsSummonedBy(WorldObject* summonerWO) override
    {
        Player* summoner = summonerWO->ToPlayer();
        if (!summoner)
            return;

        _events.ScheduleEvent(EVENT_EMERGENCY_FIRST_AID_SCRIPT_SHEATH, 2s);
    }

    void MovementInform(uint32 uiType, uint32 uiId) override
    {
        if (uiType != POINT_MOTION_TYPE)
            return;

        switch (uiId)
        {
            case POINT_SECOND_SURVIVOR:
                if (Creature* mate = ObjectAccessor::GetCreature(*me, _quartermasterGUID))
                    me->CastSpell(mate, SPELL_BANDAGING);
                _events.ScheduleEvent(EVENT_EMERGENCY_FIRST_AID_SCRIPT_MOVE_HOME_BEACH, 6s);
                break;
            case POINT_BEACH_HOME:
                me->DespawnOrUnsummon();
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
                case EVENT_EMERGENCY_FIRST_AID_SCRIPT_SHEATH:
                    me->SetSheath(SHEATH_STATE_UNARMED);
                    _events.ScheduleEvent(EVENT_EMERGENCY_FIRST_AID_SCRIPT_BANDAGE, 1s);
                    break;
                case EVENT_EMERGENCY_FIRST_AID_SCRIPT_BANDAGE:
                    if (Creature* mate = ObjectAccessor::GetCreature(*me, _firstMateGUID))
                        me->CastSpell(mate, SPELL_BANDAGING);
                    _events.ScheduleEvent(EVENT_EMERGENCY_FIRST_AID_SCRIPT_MOVE_SECOND_SURVIVOR, 5s);
                    break;
                case EVENT_EMERGENCY_FIRST_AID_SCRIPT_MOVE_SECOND_SURVIVOR:
                    me->GetMotionMaster()->MovePoint(POINT_SECOND_SURVIVOR, -414.15277f, -2605.2014f, 0.91079247f, false);
                    break;
                case EVENT_EMERGENCY_FIRST_AID_SCRIPT_MOVE_HOME_BEACH:
                    me->GetMotionMaster()->MovePoint(POINT_BEACH_HOME, -435.15277f, -2610.9915f, 0.649292f, false);
                    break;
                default:
                    break;
            }
        }
    }

public:
    void SetFirstMateGUID(ObjectGuid coleGUID)
    {
        _firstMateGUID = coleGUID;
    }

    void SetQuartermasterGUID(ObjectGuid richterGUID)
    {
        _quartermasterGUID = richterGUID;
    }

private:
    EventMap _events;
    ObjectGuid _firstMateGUID;
    ObjectGuid _quartermasterGUID;
};

// 156626 - Captain Garrick
struct npc_captain_garrick_beach : public ScriptedAI
{
    npc_captain_garrick_beach(Creature* creature) : ScriptedAI(creature) { }

    void OnQuestAccept(Player* player, Quest const* quest) override
    {
        switch (quest->GetQuestId())
        {
            case QUEST_MURLOC_MANIA_ALLIANCE:
                Conversation::CreateConversation(CONVERSATION_QUEST_MURLOC_MANIA_ALLIANCE, player, *player, player->GetGUID(), nullptr);
                break;
            case QUEST_EMERGENCY_FIRST_AID_ALLIANCE:
            {
                Creature* cole = FindCreatureIgnorePhase(player, "private_cole_beach", 50.0f);
                Creature* richter = FindCreatureIgnorePhase(player, "quartermaster_richter_beach", 50.0f);
                if (!cole || !richter)
                    return;

                Creature* colePersonal = cole->SummonPersonalClone(cole->GetPosition(), TEMPSUMMON_MANUAL_DESPAWN, 0s, 0, 0, player);
                Creature* richterPersonal = richter->SummonPersonalClone(richter->GetPosition(), TEMPSUMMON_MANUAL_DESPAWN, 0s, 0, 0, player);
                Creature* mePersonal = me->SummonPersonalClone(me->GetPosition(), TEMPSUMMON_MANUAL_DESPAWN, 0s, 0, 0, player);
                if (!colePersonal || !richterPersonal || !mePersonal)
                    return;

                if (npc_captain_warlord_first_aid_private* personalAI = CAST_AI(npc_captain_warlord_first_aid_private, mePersonal->GetAI()))
                {
                    personalAI->SetFirstMateGUID(colePersonal->GetGUID());
                    personalAI->SetQuartermasterGUID(richterPersonal->GetGUID());
                }
                break;
            }
            default:
                break;
        }
    }
};

// 166782 - Warlord Breka Grimaxe
struct npc_warlord_grimaxe_beach : public ScriptedAI
{
    npc_warlord_grimaxe_beach(Creature* creature) : ScriptedAI(creature) { }

    void OnQuestAccept(Player* player, Quest const* quest) override
    {
        switch (quest->GetQuestId())
        {
            case QUEST_MURLOC_MANIA_HORDE:
                Conversation::CreateConversation(CONVERSATION_QUEST_MURLOC_MANIA_HORDE, player, *player, player->GetGUID(), nullptr);
                break;
            case QUEST_EMERGENCY_FIRST_AID_HORDE:
            {
                Creature* throg = FindCreatureIgnorePhase(player, "grunt_throg_beach", 50.0f);
                Creature* jinhake = FindCreatureIgnorePhase(player, "jin_hake_beach", 50.0f);
                if (!throg || !jinhake)
                    return;

                Creature* throgPersonal = throg->SummonPersonalClone(throg->GetPosition(), TEMPSUMMON_MANUAL_DESPAWN, 0s, 0, 0, player);
                Creature* jinhakePersonal = jinhake->SummonPersonalClone(jinhake->GetPosition(), TEMPSUMMON_MANUAL_DESPAWN, 0s, 0, 0, player);
                Creature* mePersonal = me->SummonPersonalClone(me->GetPosition(), TEMPSUMMON_MANUAL_DESPAWN, 0s, 0, 0, player);
                if (!throgPersonal || !jinhakePersonal || !mePersonal)
                    return;

                if (npc_captain_warlord_first_aid_private* personalAI = CAST_AI(npc_captain_warlord_first_aid_private, mePersonal->AI()))
                {
                    personalAI->SetFirstMateGUID(throgPersonal->GetGUID());
                    personalAI->SetQuartermasterGUID(jinhakePersonal->GetGUID());
                }
                break;
            }
            default:
                break;
        }
    }
};

CreatureAI* CaptainGarrickBeachAISelector(Creature* creature)
{
    if (creature->IsPrivateObject())
    {
        if (Player* privateObjectOwner = ObjectAccessor::GetPlayer(*creature, creature->GetPrivateObjectOwner()))
        {
            if (privateObjectOwner->GetQuestStatus(QUEST_MURLOC_MANIA_ALLIANCE) == QUEST_STATUS_NONE)
                return new npc_captain_warlord_beach_arrive_private(creature);
            else
                return new npc_captain_warlord_first_aid_private(creature);
        }
    }
    return new npc_captain_garrick_beach(creature);
}

CreatureAI* WarlordGrimaxeBeachAISelector(Creature* creature)
{
    if (creature->IsPrivateObject())
    {
        if (Player* privateObjectOwner = ObjectAccessor::GetPlayer(*creature, creature->GetPrivateObjectOwner()))
        {
            if (privateObjectOwner->GetQuestStatus(QUEST_MURLOC_MANIA_HORDE) == QUEST_STATUS_NONE)
                return new npc_captain_warlord_beach_arrive_private(creature);
            else
                return new npc_captain_warlord_first_aid_private(creature);
        }
    }
    return new npc_warlord_grimaxe_beach(creature);
}

enum HealedByLeaderBeachData
{
    EVENT_SALUTE       = 1,
    EVENT_LEAVE_BEACH,

    NPC_COLE_BEACH     = 149917,
    NPC_RICHTER_BEACH  = 156622,
    NPC_THROG_BEACH    = 166784,
    NPC_JINHAKE_BEACH  = 166800,

    PATH_LONG_BEACH    = 10520070,
    PATH_SHORT_BEACH   = 10520080
};

// 149917 - Private Cole
// 156622 - Quartermaster Richter
// 166784 - Grunt Throg
// 166800 - Provisioner Jin'hake
// for Emergency first aid quest
template<uint32 PathId, uint32 WaitTime>
struct npc_survivors_healed_by_leader_beach_private : public ScriptedAI
{
    npc_survivors_healed_by_leader_beach_private(Creature* creature) : ScriptedAI(creature) { }

    void InitializeAI() override
    {
        me->SetStandState(UNIT_STAND_STATE_SLEEP);
    }

    void SpellHit(WorldObject* caster, SpellInfo const* spellInfo) override
    {
        if (spellInfo->Id != SPELL_BANDAGING)
            return;

        _casterGUID = caster->GetGUID();
        me->SetStandState(UNIT_STAND_STATE_STAND);
        _events.ScheduleEvent(EVENT_SALUTE, 2s);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_SALUTE:
                    if (Creature* caster = ObjectAccessor::GetCreature(*me, _casterGUID))
                        me->SetFacingToObject(caster);
                    me->HandleEmoteCommand(EMOTE_ONESHOT_SALUTE);
                    _events.ScheduleEvent(EVENT_LEAVE_BEACH, 2s);
                    break;
                case EVENT_LEAVE_BEACH:
                    me->GetMotionMaster()->MovePath(PathId, false);
                    me->DespawnOrUnsummon(Milliseconds(WaitTime));
                    break;
                default:
                    break;
            }
        }
    }
private:
    EventMap _events;
    ObjectGuid _casterGUID;
};

CreatureAI* HealedByLeaderAllianceAISelector(Creature* creature)
{
    if (creature->IsPrivateObject())
        return new npc_survivors_healed_by_leader_beach_private<PATH_LONG_BEACH, 16 * IN_MILLISECONDS>(creature);
    return new NullCreatureAI(creature);
}

CreatureAI* HealedByLeaderHordeAISelector(Creature* creature)
{
    if (creature->IsPrivateObject())
        return new npc_survivors_healed_by_leader_beach_private<PATH_SHORT_BEACH, 9 * IN_MILLISECONDS>(creature);
    return new NullCreatureAI(creature);
}

enum ExilesReachAllianceSurvivorsBeachData
{
    CONVERSATION_STOUTHANDS_BEACH         = 11685,
    CONVERSATION_HUXWORTH_BEACH           = 12128,
    CONVERSATION_KEE_LA_BEACH             = 12127,

    NPC_BJORN_STOUTHANDS_LAYING           = 156609,
    NPC_AUSTIN_HUXWORTH_LAYING            = 156610,
    NPC_KEE_LA_LAYING                     = 156612,
    NPC_KEE_LA_STANDING                   = 151088,
    NPC_BJORN_STOUTHANDS_STANDING         = 151089,
    NPC_AUSTIN_HUXWORTH_STANDING          = 154170,

    PATH_KEE_LA_STANDING                  = ((1052012 * 10) + 1) << 3,
    PATH_BJORN_STOUTHANDS_STANDING        = ((1052013 * 10) + 1) << 3,
    PATH_AUSTIN_HUXWORTH_STANDING         = ((1052014 * 10) + 1) << 3,

    SPELL_BANDAGING_QUEST                 = 297415
};

// 156609 - Bjorn Stouthands
// 156610 - Austin Huxworth
// 156612 - Kee-La
template<uint32 ConversationId>
struct npc_alliance_survivors_beach_laying : public ScriptedAI
{
    npc_alliance_survivors_beach_laying(Creature* creature) : ScriptedAI(creature) { }

    void SpellHit(WorldObject* caster, SpellInfo const* spellInfo) override
    {
        if (spellInfo->Id != SPELL_BANDAGING_QUEST)
            return;

        if (Player* player = caster->ToPlayer())
        {
            player->KilledMonsterCredit(me->GetEntry());
            Conversation::CreateConversation(ConversationId, player, *player, player->GetGUID(), nullptr);
            player->UpdateVisibilityForPlayer(); // Needed to make survivor stand instantly
        }
    }
};

enum ExilesReachHordeSurvivorsBeachData
{
    EVENT_SURVIVORS_HORDE_STAND_AND_TALK        = 1,
    EVENT_SURVIVORS_HORDE_MOVE_TO_GRIMAXE,

    EVENT_SURVIVORS_SALUTE                      = 1,
    EVENT_SURVIVORS_LEAVE_BEACH,

    NPC_BO_LAYING_LAYING                        = 166786,
    NPC_MITHDRAN_LAYING                         = 166791,
    NPC_LANA_JORDAN_LAYING                      = 166796,
    NPC_BO_STANDING                             = 166787,
    NPC_MITHDRAN_STANDING                       = 166792,
    NPC_LANA_JORDAN_STANDING                    = 166797,

    QUEST_FINDING_THE_LOST_EXPEDITION_HORDE     = 59931,
    QUEST_FINDING_THE_LOST_EXPEDITION_ALLIANCE  = 54952,

    PATH_BO_TO_GRIMAXE                          = 10520210,
    PATH_MITHDRAN_TO_GRIMAXE                    = 10520220,
    PATH_LANA_JORDAN_TO_GRIMAXE                 = 10520230,

    PATH_BO_LEAVE_BEACH                         = ((1052021 * 10) + 1) << 3,
    PATH_MITHDRAN_LEAVE_BEACH                   = ((1052022 * 10) + 1) << 3,
    PATH_LANA_JORDAN_LEAVE_BEACH                = ((1052023 * 10) + 1) << 3,

    TALK_HORDE_BEACH_THANK_PLAYER               = 0
};

// 166786 - Bo
struct npc_bo_beach_laying : public ScriptedAI
{
    npc_bo_beach_laying(Creature* creature) : ScriptedAI(creature) { }

    void SpellHit(WorldObject* caster, SpellInfo const* spellInfo) override
    {
        if (spellInfo->Id != SPELL_BANDAGING_QUEST)
            return;

        static const Position BoCloneSpawnPosition = { -448.731f, -2606.03f, 0.602435f, 6.17441f };

        if (Player* player = caster->ToPlayer())
        {
            player->KilledMonsterCredit(me->GetEntry());

            if (Creature* survivor = FindCreatureIgnorePhase(player, "bo_beach", 50.0f))
                survivor->SummonPersonalClone(BoCloneSpawnPosition, TEMPSUMMON_MANUAL_DESPAWN, 0s, 0, 0, player);
        }
    }
};

// 166791 - Mithdran Dawntracker
struct npc_mithran_dawntracker_beach_laying : public ScriptedAI
{
    npc_mithran_dawntracker_beach_laying(Creature* creature) : ScriptedAI(creature) { }

    void SpellHit(WorldObject* caster, SpellInfo const* spellInfo) override
    {
        if (spellInfo->Id != SPELL_BANDAGING_QUEST)
            return;

        static constexpr Position MithranCloneSpawnPosition = { -428.576f, -2593.93f, 0.152832f, 4.849576f };

        if (Player* player = caster->ToPlayer())
        {
            player->KilledMonsterCredit(me->GetEntry());

            if (Creature* survivor = FindCreatureIgnorePhase(player, "mithran_beach", 50.0f))
                survivor->SummonPersonalClone(MithranCloneSpawnPosition, TEMPSUMMON_MANUAL_DESPAWN, 0s, 0, 0, player);
        }
    }
};

// 166796 - Lana Jordan
struct npc_lana_jordan_beach_laying : public ScriptedAI
{
    npc_lana_jordan_beach_laying(Creature* creature) : ScriptedAI(creature) { }

    void SpellHit(WorldObject* caster, SpellInfo const* spellInfo) override
    {
        if (spellInfo->Id != SPELL_BANDAGING_QUEST)
            return;

        static constexpr Position LanaCloneSpawnPosition = { -420.656f, -2600.28f, 0.556646f, 4.046853f };

        if (Player* player = caster->ToPlayer())
        {
            player->KilledMonsterCredit(me->GetEntry());

            if (Creature* survivor = FindCreatureIgnorePhase(player, "lana_jordan_beach", 50.0f))
                survivor->SummonPersonalClone(LanaCloneSpawnPosition, TEMPSUMMON_MANUAL_DESPAWN, 0s, 0, 0, player);
        }
    }
};

// 150228 - Murloc Spearhunter
// 150229 - Murloc Watershaper
struct npc_murloc_spearhunter_watershaper_higher_ground : public ScriptedAI
{
    using ScriptedAI::ScriptedAI;

    void JustEngagedWith(Unit* who) override
    {
        me->GetMotionMaster()->MoveJump(EVENT_JUMP, who->GetPosition(), 16.0f, 0.1f);
    }
};

// 166787 - Bo
// 166792 - Mithdran Dawntracker
// 166797 - Lana Jordan
template<uint32 PathId>
struct npc_horde_survivors_beach_q59930_private : public ScriptedAI
{
    npc_horde_survivors_beach_q59930_private(Creature* creature) : ScriptedAI(creature) { }

    void InitializeAI() override
    {
        me->SetStandState(UNIT_STAND_STATE_SLEEP);
    }

    void JustAppeared() override
    {
        _events.ScheduleEvent(EVENT_SURVIVORS_HORDE_STAND_AND_TALK, 1s);
    }

    void WaypointPathEnded(uint32 /*nodeId*/, uint32 /*pathId*/) override
    {
        me->DespawnOrUnsummon();
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_SURVIVORS_HORDE_STAND_AND_TALK:
                    Talk(TALK_HORDE_BEACH_THANK_PLAYER);
                    me->SetStandState(UNIT_STAND_STATE_STAND);
                    _events.ScheduleEvent(EVENT_SURVIVORS_HORDE_MOVE_TO_GRIMAXE, 6s);
                    break;
                case EVENT_SURVIVORS_HORDE_MOVE_TO_GRIMAXE:
                    me->GetMotionMaster()->MovePath(PathId, false);
                    break;
                default:
                    break;
            }
        }
    }
private:
    EventMap _events;
};

// 151088 - Kee La
// 151089 - Bjorn Stouthands
// 154170 - Austin Huxworth
// 166787 - Bo
// 166792 - Mithdran Dawntracker
// 166797 - Lana Jordan
template<uint32 PathId, uint32 WaitTime>
struct npc_survivors_beach_leave_private : public ScriptedAI
{
    npc_survivors_beach_leave_private(Creature* creature) : ScriptedAI(creature) { }

    void JustAppeared() override
    {
        _events.ScheduleEvent(EVENT_SURVIVORS_SALUTE, 6s);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_SURVIVORS_SALUTE:
                    me->HandleEmoteCommand(EMOTE_ONESHOT_SALUTE);
                    _events.ScheduleEvent(EVENT_SURVIVORS_LEAVE_BEACH, 2s);
                    break;
                case EVENT_SURVIVORS_LEAVE_BEACH:
                    me->GetMotionMaster()->MovePath(PathId, false);
                    me->DespawnOrUnsummon(Milliseconds(WaitTime));
                    break;
                default:
                    break;
            }
        }
    }
private:
    EventMap _events;
};

CreatureAI* BoBeachStandingAISelector(Creature* creature)
{
    if (creature->IsPrivateObject())
    {
        if (Player* privateObjectOwner = ObjectAccessor::GetPlayer(*creature, creature->GetPrivateObjectOwner()))
        {
            if ((privateObjectOwner->GetQuestStatus(QUEST_FINDING_THE_LOST_EXPEDITION_HORDE) == QUEST_STATUS_NONE))
                return new npc_horde_survivors_beach_q59930_private<PATH_BO_TO_GRIMAXE>(creature);
            else
                return new npc_survivors_beach_leave_private<PATH_BO_LEAVE_BEACH, 5 * IN_MILLISECONDS>(creature);
        }
    }

    return new NullCreatureAI(creature);
}

CreatureAI* MithdranBeachStandingAISelector(Creature* creature)
{
    if (creature->IsPrivateObject())
    {
        if (Player* privateObjectOwner = ObjectAccessor::GetPlayer(*creature, creature->GetPrivateObjectOwner()))
        {
            if ((privateObjectOwner->GetQuestStatus(QUEST_FINDING_THE_LOST_EXPEDITION_HORDE) == QUEST_STATUS_NONE))
                return new npc_horde_survivors_beach_q59930_private<PATH_MITHDRAN_TO_GRIMAXE>(creature);
            else
                return new npc_survivors_beach_leave_private<PATH_MITHDRAN_LEAVE_BEACH, 4 * IN_MILLISECONDS>(creature);
        }
    }

    return new NullCreatureAI(creature);
}

CreatureAI* LanaJordanBeachStandingAISelector(Creature* creature)
{
    if (creature->IsPrivateObject())
    {
        if (Player* privateObjectOwner = ObjectAccessor::GetPlayer(*creature, creature->GetPrivateObjectOwner()))
        {
            if ((privateObjectOwner->GetQuestStatus(QUEST_FINDING_THE_LOST_EXPEDITION_HORDE) == QUEST_STATUS_NONE))
                return new npc_horde_survivors_beach_q59930_private<PATH_LANA_JORDAN_TO_GRIMAXE>(creature);
            else
                return new npc_survivors_beach_leave_private<PATH_LANA_JORDAN_LEAVE_BEACH, 5 * IN_MILLISECONDS>(creature);
        }
    }

    return new NullCreatureAI(creature);
}

CreatureAI* KeeLaBeachStandingAISelector(Creature* creature)
{
    if (creature->IsPrivateObject())
        return new npc_survivors_beach_leave_private<PATH_KEE_LA_STANDING, 7 * IN_MILLISECONDS>(creature);
    return new NullCreatureAI(creature);
}

CreatureAI* BjornBeachStandingAISelector(Creature* creature)
{
    if (creature->IsPrivateObject())
        return new npc_survivors_beach_leave_private<PATH_BJORN_STOUTHANDS_STANDING, 4 * IN_MILLISECONDS>(creature);
    return new NullCreatureAI(creature);
}

CreatureAI* AustinBeachStandingAISelector(Creature* creature)
{
    if (creature->IsPrivateObject())
        return new npc_survivors_beach_leave_private<PATH_AUSTIN_HUXWORTH_STANDING, 5 * IN_MILLISECONDS>(creature);
    return new NullCreatureAI(creature);
}

enum LostExpeditionFollowerData
{
    ACTOR_ID_ALLIANCE_SURVIVOR                  = 69830,
    ACTOR_ID_HORDE_SURVIVOR                     = 76283,

    AREA_ABANDONED_CAMP                         = 10452,

    CONVERSATION_LINE_ESCORT_ALLIANCE_SURVIVOR  = 12044,
    CONVERSATION_LINE_ESCORT_HORDE_SURVIVOR     = 14437,
    CONVERSATION_LINE_ESCORT_SURVIVOR_CAMP      = 12058,

    EVENT_INITIAL_SPAWN_CHECK                   = 1,
    EVENT_FOLLOW_PLAYER,

    SPELL_GARRICK_PING                          = 313664,
    SPELL_SUMMON_ADMIRAL_GARRICK_GUARDIAN       = 297295,
    SPELL_SUMMON_WARLORD_GRIMAXE_GUARDIAN       = 325075,

    POINT_CAMP_POSITION                         = 0,
};

static constexpr Position GarrickAbandonedCampPosition = { -249.059006f, -2492.520020f, 18.0742f };
static constexpr Position GrimaxeAbandonedCampPosition = { -249.20117f, -2492.6191f, 17.964903f };

// 165359 - Captain Garrick
// This script is used by Captian Garrick Follower for Finding the Lost Expedition quest
struct npc_garrick_summoned_beach : public ScriptedAI
{
    npc_garrick_summoned_beach(Creature* creature) : ScriptedAI(creature), _reachedCamp(false) {}

    void IsSummonedBy(WorldObject* /*summoner*/) override
    {
        _events.ScheduleEvent(EVENT_INITIAL_SPAWN_CHECK, 1s);
    }

    void SpellHit(WorldObject* /*caster*/, SpellInfo const* spellInfo) override
    {
        if (spellInfo->Id != SPELL_GARRICK_PING)
            return;

        if (_reachedCamp)
            return;

        _reachedCamp = true;

        if (Unit* owner = me->GetOwner())
        {
            Conversation* conversation = Conversation::CreateConversation(CONVERSATION_LINE_ESCORT_SURVIVOR_CAMP, owner, *owner, owner->GetGUID(), nullptr, false);
            conversation->AddActor(ACTOR_ID_ALLIANCE_SURVIVOR, 1, me->GetGUID());
            conversation->Start();

            me->GetMotionMaster()->Remove(FOLLOW_MOTION_TYPE);
            me->GetMotionMaster()->MovePoint(POINT_CAMP_POSITION, GarrickAbandonedCampPosition, false);
        }
    }

    void MovementInform(uint32 uiType, uint32 uiId) override
    {
        if (uiType != POINT_MOTION_TYPE || uiId != POINT_CAMP_POSITION)
            return;

        if (Unit* owner = me->GetOwner())
        {
            owner->CastSpell(owner, SPELL_UPDATE_PHASE_SHIFT);
            owner->RemoveAura(SPELL_SUMMON_ADMIRAL_GARRICK_GUARDIAN);
        }
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_INITIAL_SPAWN_CHECK:
                {
                    Unit* owner = me->GetOwner();
                    if (!owner)
                        break;

                    Creature* survivor = FindCreatureIgnorePhase(owner, "spawn_check");

                    if (!survivor)
                    {
                        if (owner->GetAreaId() != AREA_ABANDONED_CAMP)
                            owner->RemoveAura(SPELL_SUMMON_ADMIRAL_GARRICK_GUARDIAN);
                        else
                            _events.ScheduleEvent(EVENT_FOLLOW_PLAYER, 0s);
                    }
                    else
                    {
                        Conversation* conversation = Conversation::CreateConversation(CONVERSATION_LINE_ESCORT_ALLIANCE_SURVIVOR, owner, *owner, owner->GetGUID(), nullptr, false);
                        conversation->AddActor(ACTOR_ID_ALLIANCE_SURVIVOR, 1, me->GetGUID());
                        conversation->Start();

                        _events.ScheduleEvent(EVENT_FOLLOW_PLAYER, 2s);
                    }
                    break;
                }
                case EVENT_FOLLOW_PLAYER:
                    if (Unit* owner = me->GetOwner())
                        me->GetMotionMaster()->MoveFollow(owner, 0.0f, float(M_PI / 4.0f));
                    break;
                default:
                    break;
            }
        }
    }
private:
    EventMap _events;
    bool _reachedCamp;
};

// 166805 - Warlord Breka Grimaxe
// This script is used by Warlord Grimaxe Follower for Finding the Lost Expedition quest
struct npc_grimaxe_summoned_beach : public ScriptedAI
{
    npc_grimaxe_summoned_beach(Creature* creature) : ScriptedAI(creature), _reachedCamp(false) {}

    void IsSummonedBy(WorldObject* /*summoner*/) override
    {
        _reachedCamp = false;

        _events.ScheduleEvent(EVENT_INITIAL_SPAWN_CHECK, 1s);
    }

    void SpellHit(WorldObject* /*caster*/, SpellInfo const* spellInfo) override
    {
        if (spellInfo->Id != SPELL_GARRICK_PING)
            return;

        if (_reachedCamp)
            return;

        if (Unit* owner = me->GetOwner())
        {
            if (Conversation* conversation = Conversation::CreateConversation(CONVERSATION_LINE_ESCORT_SURVIVOR_CAMP, owner, *owner, owner->GetGUID(), nullptr, false))
            {
                conversation->AddActor(ACTOR_ID_HORDE_SURVIVOR, 3, me->GetGUID());
                conversation->Start();
            }
            me->GetMotionMaster()->Remove(FOLLOW_MOTION_TYPE);
            me->GetMotionMaster()->MovePoint(POINT_CAMP_POSITION, GrimaxeAbandonedCampPosition, false);
        }
    }

    void MovementInform(uint32 uiType, uint32 uiId) override
    {
        if (uiType != POINT_MOTION_TYPE || uiId != POINT_CAMP_POSITION)
            return;

        if (Unit* owner = me->GetOwner())
        {
            owner->CastSpell(owner, SPELL_UPDATE_PHASE_SHIFT);
            owner->RemoveAura(SPELL_SUMMON_WARLORD_GRIMAXE_GUARDIAN);
        }
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_INITIAL_SPAWN_CHECK:
                {
                    Unit* owner = me->GetOwner();
                    if (!owner)
                        break;

                    Creature* survivor = FindCreatureIgnorePhase(owner, "spawn_check");

                    if (!survivor)
                    {
                        if (owner->GetAreaId() != AREA_ABANDONED_CAMP)
                            owner->RemoveAura(SPELL_SUMMON_WARLORD_GRIMAXE_GUARDIAN);
                        else
                            _events.ScheduleEvent(EVENT_FOLLOW_PLAYER, 0s);
                    }
                    else
                    {
                        if (Conversation* conversation = Conversation::CreateConversation(CONVERSATION_LINE_ESCORT_HORDE_SURVIVOR, owner, *owner, owner->GetGUID(), nullptr, false))
                        {
                            conversation->AddActor(ACTOR_ID_HORDE_SURVIVOR, 2, me->GetGUID());
                            conversation->Start();
                        }
                        _events.ScheduleEvent(EVENT_FOLLOW_PLAYER, 2s);
                    }
                    break;
                }
                case EVENT_FOLLOW_PLAYER:
                    if (Unit* owner = me->GetOwner())
                        me->GetMotionMaster()->MoveFollow(owner, 0.0f, float(M_PI / 4.0f));
                    break;
                default:
                    break;
            }
        }
    }
private:
    EventMap _events;
    bool _reachedCamp;
};

// 54952 - Finding the Lost Expedition
// 59931 - Finding the Lost Expedition
class quest_finding_the_lost_expedition : public QuestScript
{
public:
    quest_finding_the_lost_expedition(char const* script) : QuestScript(script) { }

    void HandleQuestStatusChange(Player* player, QuestStatus newStatus, uint32 summonSpellId, std::string_view survivor1StringId, std::string_view survivor2StringId, std::string_view survivor3StringId)
    {
        switch (newStatus)
        {
            case QUEST_STATUS_INCOMPLETE:
                player->CastSpell(player, SPELL_UPDATE_PHASE_SHIFT);
                player->CastSpell(player, summonSpellId);

                if (Creature* survivor1 = FindCreatureIgnorePhase(player, survivor1StringId, 25.0f))
                {
                    Creature* survivor1Personal = survivor1->SummonPersonalClone(survivor1->GetPosition(), TEMPSUMMON_MANUAL_DESPAWN, 0s, 0, 0, player);
                    survivor1Personal->SetScriptStringId("spawn_check");
                }
                if (Creature* survivor2 = FindCreatureIgnorePhase(player, survivor2StringId, 25.0f))
                    survivor2->SummonPersonalClone(survivor2->GetPosition(), TEMPSUMMON_MANUAL_DESPAWN, 0s, 0, 0, player);
                if (Creature* survivor3 = FindCreatureIgnorePhase(player, survivor3StringId, 25.0f))
                    survivor3->SummonPersonalClone(survivor3->GetPosition(), TEMPSUMMON_MANUAL_DESPAWN, 0s, 0, 0, player);

                break;
            case QUEST_STATUS_NONE:
                player->RemoveAura(summonSpellId);
                player->CastSpell(player, SPELL_UPDATE_PHASE_SHIFT);
                break;
            default:
                break;
        }
    }
};

// 54952 - Finding the Lost Expedition
class quest_finding_the_lost_expedition_alliance : public quest_finding_the_lost_expedition
{
public:
    quest_finding_the_lost_expedition_alliance() : quest_finding_the_lost_expedition("quest_finding_the_lost_expedition_alliance") { }

    void OnQuestStatusChange(Player* player, Quest const* /*quest*/, QuestStatus /*oldStatus*/, QuestStatus newStatus) override
    {
        HandleQuestStatusChange(player, newStatus,
            SPELL_SUMMON_ADMIRAL_GARRICK_GUARDIAN,
            "kee_la_beach",
            "bjorn_stouthands_beach",
            "austin_huxworth_beach");
    }
};

// 59931 - Finding the Lost Expedition
class quest_finding_the_lost_expedition_horde : public quest_finding_the_lost_expedition
{
public:
    quest_finding_the_lost_expedition_horde() : quest_finding_the_lost_expedition("quest_finding_the_lost_expedition_horde") { }

    void OnQuestStatusChange(Player* player, Quest const* /*quest*/, QuestStatus /*oldStatus*/, QuestStatus newStatus) override
    {
        HandleQuestStatusChange(player, newStatus,
            SPELL_SUMMON_WARLORD_GRIMAXE_GUARDIAN,
            "bo_beach",
            "mithran_beach",
            "lana_jordan_beach");
    }
};

// 305596 - Summon Admiral Garrick Guardian
// 325076 - Summon Warlord Grimaxe
class spell_summon_survivor_beach : public SpellScript
{
    // @TODO: drop after TARGET_UNK_142 impl

    void SelectTarget(WorldObject*& target)
    {
        Player* caster = GetCaster()->ToPlayer();
        if (!caster)
            return;

        Creature* survivor = FindCreatureIgnorePhase(caster, caster->GetTeam() == ALLIANCE ? "q54952_garrick" : "q59931_grimaxe", 5.0f);
        if (!survivor)
            return;

        target = survivor;
    }

    void Register() override
    {
        OnObjectTargetSelect += SpellObjectTargetSelectFn(spell_summon_survivor_beach::SelectTarget, EFFECT_0, TARGET_DEST_NEARBY_ENTRY_OR_DB);
    }
};

// ******************************************************************
// * Scripting in this section occurs after reaching Abandoned Camp *
// ******************************************************************

enum CaptainGarrickAbandonedCampData
{
    CONVERSATION_QUEST_COOKING_MEAT_COMPLETE_ALLIANCE   = 12863,

    QUEST_COOKING_MEAT_ALLIANCE                         = 55174
};

enum WarlordGrimaxeAbandonedCampData
{
    CONVERSATION_QUEST_COOKING_MEAT_COMPLETE_HORDE      = 14611,

    QUEST_COOKING_MEAT_HORDE                            = 59932
};

enum CookingMeatQuestData
{
    ANIMATION_KIT_INJURED = 14432
};

static constexpr Position InjuredNpcPositionAbandonedCamp = { -245.40973f, -2492.0886f, 18.404648f, 2.4754f };

// 55174 - Cooking Meat
// 59932 - Cooking Meat
class quest_cooking_meat : public QuestScript
{
public:
    quest_cooking_meat(char const* script) : QuestScript(script) { }

    void HandleQuestStatusChange(Player* player, QuestStatus newStatus, uint32 completeConversationId, std::string_view injuredStringId)
    {
        switch (newStatus)
        {
            case QUEST_STATUS_COMPLETE:
            {
                Conversation::CreateConversation(completeConversationId, player, *player, player->GetGUID());
                break;
            }
            case QUEST_STATUS_REWARDED:
            {
                Creature* injured = FindCreatureIgnorePhase(player, injuredStringId);
                if (!injured)
                    break;

                Creature* injuredTemp = injured->SummonPersonalClone(InjuredNpcPositionAbandonedCamp, TEMPSUMMON_TIMED_DESPAWN, 2s, 0, 0, player);
                injuredTemp->SetAIAnimKitId(ANIMATION_KIT_INJURED);
                break;
            }
            default:
                break;
        }
    }
};

// 55174 - Cooking Meat
class quest_cooking_meat_alliance : public quest_cooking_meat
{
public:
    quest_cooking_meat_alliance() : quest_cooking_meat("quest_cooking_meat_alliance") { }

    void OnQuestStatusChange(Player* player, Quest const* /*quest*/, QuestStatus /*oldStatus*/, QuestStatus newStatus) override
    {
        HandleQuestStatusChange(player, newStatus,
            CONVERSATION_QUEST_COOKING_MEAT_COMPLETE_ALLIANCE,
            "alaria_standing_abandoned_camp");
    }
};

// 59932 - Cooking Meat
class quest_cooking_meat_horde : public quest_cooking_meat
{
public:
    quest_cooking_meat_horde() : quest_cooking_meat("quest_cooking_meat_horde") { }

    void OnQuestStatusChange(Player* player, Quest const* /*quest*/, QuestStatus /*oldStatus*/, QuestStatus newStatus) override
    {
        HandleQuestStatusChange(player, newStatus,
            CONVERSATION_QUEST_COOKING_MEAT_COMPLETE_HORDE,
            "wonza_standing_abandoned_camp");
    }
};

struct areatrigger_find_the_lost_expedition : AreaTriggerAI
{
    areatrigger_find_the_lost_expedition(AreaTrigger* areatrigger) : AreaTriggerAI(areatrigger) { }

    void OnUnitEnter(Unit* unit) override
    {
        Player* player = unit->ToPlayer();
        if (!player)
            return;

        if (player->GetQuestStatus(QUEST_FINDING_THE_LOST_EXPEDITION_ALLIANCE) == QUEST_STATUS_COMPLETE || player->GetQuestStatus(QUEST_FINDING_THE_LOST_EXPEDITION_HORDE) == QUEST_STATUS_INCOMPLETE)
            player->CastSpell(player, SPELL_GARRICK_PING);
    }
};

struct areatrigger_find_the_lost_expedition_follower : AreaTriggerAI
{
    areatrigger_find_the_lost_expedition_follower(AreaTrigger* areatrigger) : AreaTriggerAI(areatrigger) { }

    void OnUnitEnter(Unit* unit) override
    {
        Player* player = unit->ToPlayer();
        if (!player)
            return;

        if (player->GetTeam() == ALLIANCE)
        {
            if (player->GetQuestStatus(QUEST_FINDING_THE_LOST_EXPEDITION_ALLIANCE) != QUEST_STATUS_INCOMPLETE)
                return;

            if (player->HasAura(SPELL_SUMMON_ADMIRAL_GARRICK_GUARDIAN))
                return;

            player->CastSpell(player, SPELL_SUMMON_ADMIRAL_GARRICK_GUARDIAN);
        }
        else
        {
            if (player->GetQuestStatus(QUEST_FINDING_THE_LOST_EXPEDITION_HORDE) != QUEST_STATUS_INCOMPLETE)
                return;

            if (player->HasAura(SPELL_SUMMON_WARLORD_GRIMAXE_GUARDIAN))
                return;

            player->CastSpell(player, SPELL_SUMMON_WARLORD_GRIMAXE_GUARDIAN);
        }
    }
};

enum EnhancedCombatTacticsData
{
    SPELL_SUMMON_CAPTAIN_GARRICK_COMBAT = 320211,
    SPELL_SUMMON_WARLORD_GRIMAXE_COMBAT = 325180
};

// 59254 - Enhanced Combat Tactics (Alliance)
// 59339 - Enhanced Combat Tactics (Alliance Monk)
// 59933 - Enhanced Combat Tactics (Horde)
// 59934 - Enhanced Combat Tactics (Horde Monk)
class quest_enhanced_combat_tactics : public QuestScript
{
public:
    quest_enhanced_combat_tactics() : QuestScript("quest_enhanced_combat_tactics") { }

    void OnQuestStatusChange(Player* player, Quest const* /*quest*/, QuestStatus /*oldStatus*/, QuestStatus newStatus) override
    {
        switch (newStatus)
        {
            case QUEST_STATUS_INCOMPLETE:
                player->CastSpell(player, SPELL_UPDATE_PHASE_SHIFT);
                player->CastSpell(player, player->GetTeam() == ALLIANCE ? SPELL_SUMMON_CAPTAIN_GARRICK_COMBAT : SPELL_SUMMON_WARLORD_GRIMAXE_COMBAT);
                break;
            case QUEST_STATUS_NONE:
                player->RemoveAura(player->GetTeam() == ALLIANCE ? SPELL_SUMMON_CAPTAIN_GARRICK_COMBAT : SPELL_SUMMON_WARLORD_GRIMAXE_COMBAT);
                player->UpdateObjectVisibility();
                player->CastSpell(player, SPELL_UPDATE_PHASE_SHIFT);
                break;
            default:
                break;
        }
    }
};

// 320175 - Summon Garrick - Combat Training (DNT)
// 325181 - Summon Grimaxe - Combat Training (DNT)
class spell_summon_combat_trainer : public SpellScript
{
    // @TODO: drop after TARGET_UNK_142 impl

    void SelectTarget(WorldObject*& target)
    {
        Player* caster = GetCaster()->ToPlayer();
        if (!caster)
            return;

        Creature* partner = FindCreatureIgnorePhase(caster, caster->GetTeam() == ALLIANCE ? "garrick_camp" : "grimaxe_camp", 10.0f);
        if (!partner)
            return;

        target = partner;
    }

    void Register() override
    {
        OnObjectTargetSelect += SpellObjectTargetSelectFn(spell_summon_combat_trainer::SelectTarget, EFFECT_0, TARGET_DEST_NEARBY_ENTRY_OR_DB);
    }
};

enum EnhancedCombatTrainerData
{
    ACTOR_ID_ALLIANCE_ENHANCED_TRAINING                 = 74771,
    ACTOR_ID_HORDE_ENHANCED_TRAINING                    = 76285,

    CHARGE_CATEGORY_CHARGE_SPELL                        = 1386,

    CONVERSATION_PREFIGHT_WALK_ENHANCED                 = 13710,
    CONVERSATION_USE_SPELLS_AT_RANGE                    = 13630,
    CONVERSATION_READY_COMBAT                           = 14440,
    // Warrior
    CONVERSATION_READY_COMBAT_WARRIOR                   = 14441,
    CONVERSATION_CHARGE_ZERO_RES_ENHANCED               = 14444,
    CONVERSATION_SLAM_ENHANCED                          = 14447,
    CONVERSATION_CHARGE_ONE_RES_ENHANCED                = 14448,
    CONVERSATION_CHARGE_FINAL_ENHANCED                  = 14449,
    CONVERSATION_CHARGE_KICKBACK                        = 13611,
    // Paladin
    CONVERSATION_HOLY_POWER_ONE_PALADIN                 = 14452,
    CONVERSATION_CRUSADER_STRIKE_ONE_PALADIN            = 14453,
    CONVERSATION_CRUSADER_STRIKE_TWO_PALADIN            = 14454,
    CONVERSATION_SHIELD_SLAM_ONE_PALADIN                = 14455,
    CONVERSATION_HOLY_POWER_TWO_PALADIN                 = 14456,
    CONVERSATION_SHIELD_SLAM_TWO_PALADIN                = 14457,
    CONVERSATION_CRUSADER_STRIKE_THREE_PALADIN          = 14458,
    CONVERSATION_HOLY_POWER_THREE_PALADIN               = 14459,
    // Rogue
    CONVERSATION_SINISTER_STRIKE_ONE_ROGUE              = 14486,
    CONVERSATION_REGULAR_ATTACKS_ROGUE                  = 14487,
    CONVERSATION_THREE_COMBO_POINTS_ROGUE               = 14488,
    CONVERSATION_THREE_COMBO_EVISCERATE_ROGUE           = 14489,
    CONVERSATION_SINISTER_STRIKE_TWO_ROGUE              = 14490,
    CONVERSATION_FOUR_COMBO_POINTS_ROGUE                = 14491,
    CONVERSATION_FOUR_COMBO_EVISCERATE_ROGUE            = 14492,
    CONVERSATION_SINISTER_STRIKE_THREE_ROGUE            = 14493,
    CONVERSATION_FIVE_COMBO_POINTS_ROGUE                = 14494,
    CONVERSATION_FAILED_EVISCERATE_ROGUE                = 14495,
    // Priest
    CONVERSATION_SHADOW_WORD_PAIN_QUEST_CREDIT_PRIEST   = 13892,
    CONVERSATION_SMITE_PRE_COMBAT_PRIEST                = 14460,
    CONVERSATION_SHADOW_WORD_PAIN_PRE_COMBAT_PRIEST     = 14461,
    CONVERSATION_SHADOW_WORD_PAIN_TOO_SOON_PRIEST       = 14462,
    CONVERSATION_SHADOW_WORD_PAIN_FADING_PRIEST         = 14463,
    // Shaman
    CONVERSATION_LIGHTNINGBOLT_FIRST_SHAMAN             = 13631,
    CONVERSATION_PRIMAL_STRIKE_FIRST_SHAMAN             = 13632,
    CONVERSATION_PRIMAL_STRIKE_QUEST_CREDIT_SHAMAN      = 13633,
    CONVERSATION_LIGHTNINGBOLT_RANGE_SHAMAN             = 14475,
    // Mage
    CONVERSATION_FROSTBOLT_MAGE                         = 13634,
    CONVERSATION_FIRE_BLAST_QUEST_CREDIT_MAGE           = 13635,
    CONVERSATION_FROSTBOLT_CLOSE_MAGE                   = 14476,
    CONVERSATION_FIRE_BLAST_MAGE_NO_CREDIT              = 14477,
    // Warlock
    CONVERSATION_CORRUPTION_QUEST_CREDIT_WARLOCK        = 13895,
    CONVERSATION_SHADOW_BOLT_PRE_COMBAT_WARLOCK         = 14465,
    CONVERSATION_CORRUPTION_CAST_PRE_COMBAT_WARLOCK     = 14466,
    CONVERSATION_CORRUPTION_CAST_TOO_SOON_WARLOCK       = 14467,
    CONVERSATION_CORRUPTION_IS_FADING_WARLOCK           = 14468,
    // Druid
    CONVERSATION_MOONFIRE_QUEST_CREDIT_DRUID            = 13893,
    CONVERSATION_WRATH_PRE_COMBAT_DRUID                 = 14471,
    CONVERSATION_MOONFIRE_CAST_PRE_COMBAT_DRUID         = 14472,
    CONVERSATION_MOONFIRE_CAST_TOO_SOON_DRUID           = 14473,
    CONVERSATION_MOONFIRE_WEARING_OFF_DRUID             = 14474,

    // All classes
    EVENT_COMBAT_TRAINING_WALK_AND_TALK                 = 1,
    EVENT_COMBAT_TRAINING_FACE_PLAYER,
    EVENT_COMBAT_RUN_BACK,
    EVENT_COMBAT_TRAINING_END,
    EVENT_COMBAT_CHECK_PLAYER,
    // Rogue
    EVENT_COMBAT_TRAINING_SINISTER_CHECK_ROGUE,
    // Priest, Warlock, Druid
    EVENT_COMBAT_TRAINING_SPELL_FADING,
    // Shaman
    EVENT_COMBAT_TRAINING_RESET_SHAMAN,
    EVENT_COMBAT_TRAINING_AGGRO_CHECK_SHAMAN,
    // Mage
    EVENT_COMBAT_TRAINING_RESET_MAGE,
    EVENT_COMBAT_TRAINING_AGGRO_CHECK_MAGE,

    NPC_ALLIANCE_SPARRING_PARTNER_ENHANCED              = 164577,
    NPC_ALLIANCE_SPARRING_PARTNER_ENHANCED2             = 164775,
    //NPC_HORDE_SPARING_PARTNER_ENHANCED                  = 166916,
    NPC_INVISBUNNY_CAMP                                 = 167761,

    PATH_COMBAT_TRAINER_HOME                            = 10512100,

    POINT_WALK_POINT_ENHANCED_TRAINING                  = 1,
    POINT_RUN_POINT_ENHANCED_TRAINING                   = 2,
    POINT_TRAINING_POINT_ENHANCED_TRAINING              = 3,

    QUEST_ENHANCED_COMBAT_TACTICS_ALLIANCE              = 59254,
    QUEST_ENHANCED_COMBAT_TACTICS_ALLIANCE_MONK         = 59339,
    QUEST_ENHANCED_COMBAT_TACTICS_HORDE                 = 59933,
    QUEST_ENHANCED_COMBAT_TACTICS_HORDE_MONK            = 59934,

    QUEST_OBJECTIVE_HORDE_ABILITIES_PROVEN              = 397255,
    QUEST_OBJECTIVE_HORDE_TIGER_PALM                    = 397258,
    QUEST_OBJECTIVE_HORDE_BLACKOUT_KICK                 = 397259,
    QUEST_OBJECTIVE_ALLIANCE_ABILITIES_PROVEN           = 396220,
    QUEST_OBJECTIVE_ALLIANCE_TIGER_PALM                 = 396353,
    QUEST_OBJECTIVE_ALLIANCE_BLACKOUT_KICK              = 396354,

    SPELL_DRINK_HEALING_POTION                          = 320229,
    SPELL_KNOCKBACK                                     = 320735,
    SPELL_CHARGE_KNOCKBACK_DRUID                        = 320767,
    SPELL_CHARGE                                        = 100,
    SPELL_SLAM                                          = 1464,
    SPELL_CHARGE_KNOCKBACK_WARRIOR                      = 320583,
    SPELL_SHIELD_OF_THE_RIGHTEOUS                       = 53600,
    SPELL_CRUSADER_STRIKE                               = 35395,
    SPELL_SINISTER_STRIKE                               = 1752,
    SPELL_EVISCERATE                                    = 196819,
    SPELL_SMITE                                         = 585,
    SPELL_SHADOW_WORD_PAIN                              = 589,
    SPELL_CHARGE_KNOCKBACK                              = 320605,
    SPELL_PRIMAL_STRIKE                                 = 73899,
    SPELL_LIGHTNING_BOLT                                = 188196,
    SPELL_FIRE_BLAST                                    = 319836,
    SPELL_FROSTBOLT                                     = 116,
    SPELL_CORRUPTION                                    = 172,
    SPELL_SHADOWBOLT                                    = 686,
    SPELL_MOONFIRE                                      = 164812,
    SPELL_WRATH                                         = 5176,
    SPELL_TIGER_PALM                                    = 100780,
    SPELL_BLACKOUT_KICK                                 = 100784,
    SPELL_RANGED_ROOT_DNT                               = 320608,
    SPELL_AGGRO_RADIUS_CHECK_DNT_WARRIOR                = 320741,
    SPELL_AGGRO_RADIUS_CHECK_DNT_PRIEST                 = 320649,
    SPELL_AGGRO_RADIUS_CHECK_DNT_SHAMAN                 = 320705,
    SPELL_AGGRO_RADIUS_CHECK_DNT_WARRIOR_MAGE           = 320741,
    SPELL_AGGRO_RADIUS_CHECK_DNT_WARLOCK                = 320606,
    SPELL_AGGRO_RADIUS_CHECK_DNT_DRUID                  = 320766
};

static constexpr Position EnhancedTrainingWalkPosition = { -250.60243f, -2485.2517f, 17.787413f };
static constexpr Position EnhancedTrainingRunPosition = { -231.5225f, -2480.5276f, 19.019197f };

// 164577 - Alliance Sparring Partner
// 166916 - Horde Sparring Partner
struct npc_sparring_partner_combat_training : public ScriptedAI
{
    npc_sparring_partner_combat_training(Creature* creature) : ScriptedAI(creature), _questID(0), _summonSpellAuraID(0) { }

    virtual void OnReadyPointReached() { }

    virtual void HandleClassEvent(uint32 /*eventId*/) { }

    void JustAppeared() override
    {
        _events.ScheduleEvent(EVENT_COMBAT_TRAINING_WALK_AND_TALK, 2s);
    }

    void IsSummonedBy(WorldObject* summonerWO) override
    {
        Player* player = summonerWO->GetAffectingPlayer();
        if (!player)
            return;

        if (player->GetTeam() == ALLIANCE)
        {
            _summonSpellAuraID = SPELL_SUMMON_CAPTAIN_GARRICK_COMBAT;

            if (player->GetClass() == CLASS_MONK)
                _questID = QUEST_ENHANCED_COMBAT_TACTICS_ALLIANCE_MONK;
            else
                _questID = QUEST_ENHANCED_COMBAT_TACTICS_ALLIANCE;
        }
        else
        {
            _summonSpellAuraID = SPELL_SUMMON_WARLORD_GRIMAXE_COMBAT;

            if (player->GetClass() == CLASS_MONK)
                _questID = QUEST_ENHANCED_COMBAT_TACTICS_HORDE_MONK;
            else
                _questID = QUEST_ENHANCED_COMBAT_TACTICS_HORDE;
        }
    }

    uint8 GetQuestCredits()
    {
        Player* player = me->GetDemonCreatorPlayer();
        if (!player)
            return 0;

        uint32 objectiveId = 0;
        switch (_questID)
        {
            case QUEST_ENHANCED_COMBAT_TACTICS_ALLIANCE:
                objectiveId = QUEST_OBJECTIVE_ALLIANCE_ABILITIES_PROVEN;
                break;
            case QUEST_ENHANCED_COMBAT_TACTICS_HORDE:
                objectiveId = QUEST_OBJECTIVE_HORDE_ABILITIES_PROVEN;
                break;
            default:
                break;
        }

        return player->GetQuestObjectiveData(_questID, objectiveId);
    }

    void EnterEvadeMode(EvadeReason /*why*/) override
    {
        if (!me->IsAlive())
            return;

        me->CombatStop(true);
        EngagementOver();
        me->ResetPlayerDamageReq();
    }

    void MovementInform(uint32 uiType, uint32 uiId) override
    {
        if (uiType != POINT_MOTION_TYPE)
            return;

        switch (uiId)
        {
            case POINT_WALK_POINT_ENHANCED_TRAINING:
                me->SetWalk(false);
                me->GetMotionMaster()->MovePoint(POINT_RUN_POINT_ENHANCED_TRAINING, EnhancedTrainingRunPosition);
                break;
            case POINT_RUN_POINT_ENHANCED_TRAINING:
            {
                std::list<Creature*> sparpoints;
                GetCreatureListWithEntryInGrid(sparpoints, me, NPC_INVISBUNNY_CAMP, 100.0f);
                Trinity::Containers::RandomResize(sparpoints, 1);

                for (Creature* creature : sparpoints)
                    me->GetMotionMaster()->MovePoint(POINT_TRAINING_POINT_ENHANCED_TRAINING, creature->GetPosition());
                break;
            }
            case POINT_TRAINING_POINT_ENHANCED_TRAINING:
            {
                Unit* owner = me->GetDemonCreator();
                if (!owner)
                    break;

                me->SetFacingToObject(owner);
                me->SetImmuneToPC(false);
                me->SetUninteractible(false);
                _events.ScheduleEvent(EVENT_COMBAT_CHECK_PLAYER, 1s);

                OnReadyPointReached();
                break;
            }
            default:
                break;
        }
    }

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo = nullptr*/) override
    {
        damage = me->GetHealth() - 1;

        if (me->HealthBelowPctDamaged(20, damage))
            me->CastSpell(me, SPELL_DRINK_HEALING_POTION);
    }

    void DamageDealt(Unit* target, uint32& damage, DamageEffectType /*damageType*/) override
    {
        if (target->GetHealthPct() < 91)
            damage = 0;
    }

    void StartConversationWithPlayer(uint32 conversationId)
    {
        if (Player* player = me->GetDemonCreatorPlayer())
        {
            Conversation* conversation = Conversation::CreateConversation(conversationId, player, *player, player->GetGUID(), nullptr, false);
            if (!conversation)
                return;

            conversation->AddActor(ACTOR_ID_ALLIANCE_ENHANCED_TRAINING, 0, player->GetTeam() == ALLIANCE ? me->GetGUID() : ObjectGuid::Empty);
            conversation->AddActor(ACTOR_ID_HORDE_ENHANCED_TRAINING, 1, player->GetTeam() == HORDE ? me->GetGUID() : ObjectGuid::Empty);
            conversation->Start();
        }
    }

    void WaypointPathEnded(uint32 /*nodeId*/, uint32 pathId) override
    {
        // Used to check if reached home
        if (pathId == PATH_COMBAT_TRAINER_HOME)
        {
            if (Unit* owner = me->GetDemonCreator())
            {
                owner->CastSpell(owner, SPELL_UPDATE_PHASE_SHIFT);
                owner->RemoveAura(_summonSpellAuraID);
            }
        }
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_COMBAT_TRAINING_WALK_AND_TALK:
                    // Used by all classes
                    me->SetWalk(true);
                    me->GetMotionMaster()->MovePoint(POINT_WALK_POINT_ENHANCED_TRAINING, EnhancedTrainingWalkPosition);
                    StartConversationWithPlayer(CONVERSATION_PREFIGHT_WALK_ENHANCED);
                    break;
                case EVENT_COMBAT_TRAINING_FACE_PLAYER:
                {
                    // Used by all classes
                    if (Unit* owner = me->GetDemonCreator())
                        me->SetFacingToObject(owner);

                    me->SetImmuneToPC(false);
                    me->SetUninteractible(false);
                    break;
                }
                case EVENT_COMBAT_TRAINING_END:
                    // Used by all classes
                    me->SetImmuneToPC(true);
                    me->SetUninteractible(true);
                    me->RemoveAllAuras();
                    if (Unit* owner = me->GetDemonCreator())
                    {
                        me->SetFacingToObject(owner);
                        StartConversationWithPlayer(CONVERSATION_CHARGE_FINAL_ENHANCED);
                        _events.ScheduleEvent(EVENT_COMBAT_RUN_BACK, 4s);
                    }
                    break;
                case EVENT_COMBAT_RUN_BACK:
                    // Used by all classes
                    me->GetMotionMaster()->Clear();
                    me->GetMotionMaster()->MovePath(PATH_COMBAT_TRAINER_HOME, false);
                    break;
                default:
                    HandleClassEvent(eventId);
                    break;
            }
        }

        if (!UpdateVictim())
            return;
    }

protected:
    uint32 _questID;
    uint32 _summonSpellAuraID;
    EventMap _events;
};

// 164577 - Alliance Sparring Partner
// 166916 - Horde Sparring Partner
struct npc_sparring_partner_enhanced_combat_training_warrior : public npc_sparring_partner_combat_training
{
    npc_sparring_partner_enhanced_combat_training_warrior(Creature* creature) : npc_sparring_partner_combat_training(creature), _slamCounter(0), _secondaryCheck(false) { }

    void ResetWarrior(Player* player)
    {
        _slamCounter = 0;
        me->SetImmuneToPC(true);
        me->SetUninteractible(true);
        player->GetSpellHistory()->ResetCharges(CHARGE_CATEGORY_CHARGE_SPELL);
        me->CastSpell(me, SPELL_AGGRO_RADIUS_CHECK_DNT_WARRIOR_MAGE);
        me->CastSpell(me, SPELL_RANGED_ROOT_DNT);
        _events.ScheduleEvent(EVENT_COMBAT_TRAINING_FACE_PLAYER, 1s);
        StartConversationWithPlayer(CONVERSATION_CHARGE_KICKBACK);
    }

    void OnReadyPointReached() override
    {
        StartConversationWithPlayer(CONVERSATION_READY_COMBAT_WARRIOR);
        me->CastSpell(me, SPELL_AGGRO_RADIUS_CHECK_DNT_WARRIOR_MAGE);
        me->CastSpell(me, SPELL_RANGED_ROOT_DNT);
        _secondaryCheck = true;
    }

    void SpellHit(WorldObject* caster, SpellInfo const* spellInfo) override
    {
        Player* player = caster->ToPlayer();
        if (!player)
            return;

        if (spellInfo->Id == SPELL_CHARGE)
        {
            me->RemoveAura(SPELL_RANGED_ROOT_DNT);
            me->RemoveAura(SPELL_AGGRO_RADIUS_CHECK_DNT_WARRIOR_MAGE);

            player->KilledMonsterCredit(NPC_ALLIANCE_SPARRING_PARTNER_ENHANCED);

            switch (GetQuestCredits())
            {
                case 1:
                    StartConversationWithPlayer(CONVERSATION_CHARGE_ZERO_RES_ENHANCED);
                    break;
                case 2:
                    StartConversationWithPlayer(CONVERSATION_CHARGE_ONE_RES_ENHANCED);
                    break;
                case 3:
                    _events.ScheduleEvent(EVENT_COMBAT_TRAINING_END, 1s);
                    break;
                default:
                    break;
            }
        }
        else if (spellInfo->Id == SPELL_SLAM)
        {
            ++_slamCounter;

            if (_slamCounter == 3 && GetQuestCredits())
                ResetWarrior(player);

            if (!_secondaryCheck)
                return;

            _secondaryCheck = false;
            StartConversationWithPlayer(CONVERSATION_SLAM_ENHANCED);
        }
    }

private:
    uint8 _slamCounter;
    bool _secondaryCheck;
};

// 164577 - Alliance Sparring Partner
// 166916 - Horde Sparring Partner
struct npc_sparring_partner_enhanced_combat_training_paladin : public npc_sparring_partner_combat_training
{
    npc_sparring_partner_enhanced_combat_training_paladin(Creature* creature) : npc_sparring_partner_combat_training(creature), _secondaryCheck(false), _holyPowerCheck(false) { }

    void OnReadyPointReached() override
    {
        StartConversationWithPlayer(CONVERSATION_READY_COMBAT);
        _holyPowerCheck = true;
        _secondaryCheck = true;
    }

    void HandleClassEvent(uint32 eventId) override
    {
        switch (eventId)
        {
            case EVENT_COMBAT_CHECK_PLAYER:
            {
                if (_holyPowerCheck) // Used by paladin
                {
                    if (Unit* owner = me->GetDemonCreator())
                    {
                        if (owner->GetPower(POWER_HOLY_POWER) >= 3)
                        {
                            switch (GetQuestCredits())
                            {
                                case 0:
                                    StartConversationWithPlayer(CONVERSATION_HOLY_POWER_ONE_PALADIN);
                                    break;
                                case 1:
                                    StartConversationWithPlayer(CONVERSATION_HOLY_POWER_TWO_PALADIN);
                                    break;
                                case 2:
                                    StartConversationWithPlayer(CONVERSATION_HOLY_POWER_THREE_PALADIN);
                                    break;
                                default:
                                    break;
                            }
                            _holyPowerCheck = false;
                        }
                    }
                }
                _events.ScheduleEvent(EVENT_COMBAT_CHECK_PLAYER, 500ms);
                break;
            }
            default:
                break;
        }
    }

    void SpellHit(WorldObject* caster, SpellInfo const* spellInfo) override
    {
        Player* player = caster->ToPlayer();
        if (!player)
            return;

        if (spellInfo->Id == SPELL_SHIELD_OF_THE_RIGHTEOUS)
        {
            player->KilledMonsterCredit(NPC_ALLIANCE_SPARRING_PARTNER_ENHANCED);

            switch (GetQuestCredits())
            {
                case 1:
                    StartConversationWithPlayer(CONVERSATION_SHIELD_SLAM_ONE_PALADIN);
                    _secondaryCheck = true;
                    break;
                case 2:
                    StartConversationWithPlayer(CONVERSATION_SHIELD_SLAM_TWO_PALADIN);
                    _secondaryCheck = true;
                    break;
                case 3:
                    _events.ScheduleEvent(EVENT_COMBAT_TRAINING_END, 1s);
                    _events.CancelEvent(EVENT_COMBAT_CHECK_PLAYER);
                    break;
                default:
                    break;
            }
        }
        else if (spellInfo->Id == SPELL_CRUSADER_STRIKE)
        {
            if (_secondaryCheck)
            {
                switch (GetQuestCredits())
                {
                    case 0:
                        StartConversationWithPlayer(CONVERSATION_CRUSADER_STRIKE_ONE_PALADIN);
                        break;
                    case 1:
                        StartConversationWithPlayer(CONVERSATION_CRUSADER_STRIKE_TWO_PALADIN);
                        break;
                    case 2:
                        StartConversationWithPlayer(CONVERSATION_CRUSADER_STRIKE_THREE_PALADIN);
                        break;
                    default:
                        break;
                }
                _secondaryCheck = false;
                _holyPowerCheck = true;
            }
        }
    }

private:
    bool _secondaryCheck;
    bool _holyPowerCheck;
};

// 164577 - Alliance Sparring Partner
// 166916 - Horde Sparring Partner
struct npc_sparring_partner_enhanced_combat_training_rogue : public npc_sparring_partner_combat_training
{
    npc_sparring_partner_enhanced_combat_training_rogue(Creature* creature) : npc_sparring_partner_combat_training(creature), _comboPointsCounter(0), _secondaryCheck(false), _comboPointCheck(false) { }

    void OnReadyPointReached() override
    {
        StartConversationWithPlayer(CONVERSATION_READY_COMBAT);
        _comboPointCheck = true;
        _secondaryCheck = true;
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        if (!GetQuestCredits())
            _events.ScheduleEvent(EVENT_COMBAT_TRAINING_SINISTER_CHECK_ROGUE, 8s, 20s);
    }

    void HandleClassEvent(uint32 eventId) override
    {
        switch (eventId)
        {
            case EVENT_COMBAT_TRAINING_SINISTER_CHECK_ROGUE:
                StartConversationWithPlayer(CONVERSATION_REGULAR_ATTACKS_ROGUE);
                _events.ScheduleEvent(EVENT_COMBAT_TRAINING_SINISTER_CHECK_ROGUE, 8s, 20s);
                break;
            case EVENT_COMBAT_CHECK_PLAYER:
            {
                if (_comboPointCheck) // Used by rogue
                {
                    if (Unit* owner = me->GetDemonCreator())
                        _comboPointsCounter = owner->GetPower(POWER_COMBO_POINTS);

                    if (_comboPointsCounter >= (GetQuestCredits() + 3))
                    {
                        switch (GetQuestCredits())
                        {
                            case 0:
                                StartConversationWithPlayer(CONVERSATION_THREE_COMBO_POINTS_ROGUE);
                                break;
                            case 1:
                                StartConversationWithPlayer(CONVERSATION_FOUR_COMBO_POINTS_ROGUE);
                                break;
                            case 2:
                                StartConversationWithPlayer(CONVERSATION_FIVE_COMBO_POINTS_ROGUE);
                                break;
                            default:
                                break;
                        }
                        _comboPointCheck = false;
                    }
                }
                _events.ScheduleEvent(EVENT_COMBAT_CHECK_PLAYER, 500ms);
                break;
            }
            default:
                break;
        }
    }

    void SpellHit(WorldObject* caster, SpellInfo const* spellInfo) override
    {
        Player* player = caster->ToPlayer();
        if (!player)
            return;

        if (spellInfo->Id == SPELL_EVISCERATE)
        {
            _comboPointCheck = true;
            if (_comboPointsCounter >= (GetQuestCredits() + 3))
            {
                player->KilledMonsterCredit(NPC_ALLIANCE_SPARRING_PARTNER_ENHANCED);

                switch (GetQuestCredits())
                {
                    case 1:
                        StartConversationWithPlayer(CONVERSATION_THREE_COMBO_EVISCERATE_ROGUE);
                        _secondaryCheck = true;
                        break;
                    case 2:
                        StartConversationWithPlayer(CONVERSATION_FOUR_COMBO_EVISCERATE_ROGUE);
                        _secondaryCheck = true;
                        break;
                    case 3:
                        _events.ScheduleEvent(EVENT_COMBAT_TRAINING_END, 1s);
                        _events.CancelEvent(EVENT_COMBAT_CHECK_PLAYER);
                        break;
                    default:
                        break;
                }
            }
            else
            {
                StartConversationWithPlayer(CONVERSATION_FAILED_EVISCERATE_ROGUE);
            }
        }
        else if (spellInfo->Id == SPELL_SINISTER_STRIKE)
        {
            if (!GetQuestCredits())
                _events.RescheduleEvent(EVENT_COMBAT_TRAINING_SINISTER_CHECK_ROGUE, 8s, 20s);

            if (_secondaryCheck)
            {
                switch (GetQuestCredits())
                {
                    case 0:
                        StartConversationWithPlayer(CONVERSATION_SINISTER_STRIKE_ONE_ROGUE);
                        break;
                    case 1:
                        StartConversationWithPlayer(CONVERSATION_SINISTER_STRIKE_TWO_ROGUE);
                        break;
                    case 2:
                        StartConversationWithPlayer(CONVERSATION_SINISTER_STRIKE_THREE_ROGUE);
                        break;
                    default:
                        break;
                }
                _secondaryCheck = false;
            }
        }
    }

private:
    uint8 _comboPointsCounter;
    bool _secondaryCheck;
    bool _comboPointCheck;
};

// 164577 - Alliance Sparring Partner
// 166916 - Horde Sparring Partner
struct npc_sparring_partner_enhanced_combat_training_priest : public npc_sparring_partner_combat_training
{
    npc_sparring_partner_enhanced_combat_training_priest(Creature* creature) : npc_sparring_partner_combat_training(creature), _shadowWordPainInPandemicWindow(false), _secondaryCheck(false) { }

    void OnReadyPointReached() override
    {
        StartConversationWithPlayer(CONVERSATION_READY_COMBAT);
        me->CastSpell(me, SPELL_AGGRO_RADIUS_CHECK_DNT_PRIEST);
        me->CastSpell(me, SPELL_RANGED_ROOT_DNT);
        _secondaryCheck = true;
    }

    void HandleClassEvent(uint32 eventId) override
    {
        switch (eventId)
        {
            case EVENT_COMBAT_TRAINING_SPELL_FADING:
            {
                Aura* aura = me->GetAura(SPELL_SHADOW_WORD_PAIN);
                if (!aura)
                    break;

                if (!_shadowWordPainInPandemicWindow)
                {
                    int32 pandemicDuration = CalculatePct(aura->GetMaxDuration(), 30.0f);
                    if (aura->GetDuration() <= pandemicDuration)
                    {
                        _shadowWordPainInPandemicWindow = true;
                        StartConversationWithPlayer(CONVERSATION_SHADOW_WORD_PAIN_FADING_PRIEST);
                    }
                }
                _events.ScheduleEvent(EVENT_COMBAT_TRAINING_SPELL_FADING, 1s);
                break;
            }
            default:
                break;
        }
    }

    void SpellHit(WorldObject* caster, SpellInfo const* spellInfo) override
    {
        Player* player = caster->ToPlayer();
        if (!player)
            return;

        if (spellInfo->Id == SPELL_SHADOW_WORD_PAIN)
        {
            if (_events.GetTimeUntilEvent(EVENT_COMBAT_TRAINING_SPELL_FADING) == Milliseconds::max())
                _events.ScheduleEvent(EVENT_COMBAT_TRAINING_SPELL_FADING, 1s);

            if (!_shadowWordPainInPandemicWindow)
            {
                if (_secondaryCheck)
                {
                    StartConversationWithPlayer(CONVERSATION_SHADOW_WORD_PAIN_PRE_COMBAT_PRIEST);
                    me->SetImmuneToPC(false);
                    me->SetUninteractible(false);
                    me->RemoveAura(SPELL_RANGED_ROOT_DNT);
                    _secondaryCheck = false;
                }
                else
                    StartConversationWithPlayer(CONVERSATION_SHADOW_WORD_PAIN_TOO_SOON_PRIEST);
            }
            else
            {
                _shadowWordPainInPandemicWindow = false;
                player->KilledMonsterCredit(NPC_ALLIANCE_SPARRING_PARTNER_ENHANCED);

                if (player->GetQuestStatus(_questID) == QUEST_STATUS_COMPLETE)
                    _events.ScheduleEvent(EVENT_COMBAT_TRAINING_END, 1s);
                else
                    StartConversationWithPlayer(CONVERSATION_SHADOW_WORD_PAIN_QUEST_CREDIT_PRIEST);
            }
        }
        else if (spellInfo->Id == SPELL_SMITE)
        {
            if (_secondaryCheck)
                StartConversationWithPlayer(CONVERSATION_SMITE_PRE_COMBAT_PRIEST);
        }
    }

private:
    bool _shadowWordPainInPandemicWindow;
    bool _secondaryCheck;
};

// 164577 - Alliance Sparring Partner
// 166916 - Horde Sparring Partner
struct npc_sparring_partner_enhanced_combat_training_shaman : public npc_sparring_partner_combat_training
{
    npc_sparring_partner_enhanced_combat_training_shaman(Creature* creature) : npc_sparring_partner_combat_training(creature), _primalStrikeCounter(0), _secondaryCheck(false) { }

    void OnReadyPointReached() override
    {
        StartConversationWithPlayer(CONVERSATION_READY_COMBAT);
        me->CastSpell(me, SPELL_AGGRO_RADIUS_CHECK_DNT_SHAMAN);
        me->CastSpell(me, SPELL_RANGED_ROOT_DNT);
        _secondaryCheck = true;
    }

    void HandleClassEvent(uint32 eventId) override
    {
        switch (eventId)
        {
            case EVENT_COMBAT_TRAINING_RESET_SHAMAN:
                if (Unit* owner = me->GetDemonCreator())
                {
                    me->CastSpell(owner, SPELL_KNOCKBACK);
                    _events.ScheduleEvent(EVENT_COMBAT_TRAINING_AGGRO_CHECK_SHAMAN, 2s);
                }
                break;
            case EVENT_COMBAT_TRAINING_AGGRO_CHECK_SHAMAN:
                me->CastSpell(me, SPELL_AGGRO_RADIUS_CHECK_DNT_SHAMAN);
                me->SetImmuneToPC(false);
                me->SetUninteractible(false);
                _secondaryCheck = true;
                break;
            default:
                break;
        }
    }

    void SpellHit(WorldObject* caster, SpellInfo const* spellInfo) override
    {
        Player* player = caster->ToPlayer();
        if (!player)
            return;

        if (spellInfo->Id == SPELL_PRIMAL_STRIKE)
        {
            if (!player->IsWithinDist(me, 2.0f))
                return;

            ++_primalStrikeCounter;

            if (_primalStrikeCounter < 3)
            {
                if (_primalStrikeCounter == 1)
                    StartConversationWithPlayer(CONVERSATION_PRIMAL_STRIKE_FIRST_SHAMAN);
                return;
            }

            _primalStrikeCounter = 0;
            player->KilledMonsterCredit(NPC_ALLIANCE_SPARRING_PARTNER_ENHANCED);

            if (player->GetQuestStatus(_questID) == QUEST_STATUS_COMPLETE)
            {
                _events.ScheduleEvent(EVENT_COMBAT_TRAINING_END, 1s);
            }
            else
            {
                StartConversationWithPlayer(CONVERSATION_PRIMAL_STRIKE_QUEST_CREDIT_SHAMAN);
                me->SetImmuneToPC(true);
                me->SetUninteractible(true);
                _events.ScheduleEvent(EVENT_COMBAT_TRAINING_RESET_SHAMAN, 3s);
            }
        }
        else if (spellInfo->Id == SPELL_LIGHTNING_BOLT)
        {
            me->RemoveAura(SPELL_RANGED_ROOT_DNT);
            me->RemoveAura(SPELL_AGGRO_RADIUS_CHECK_DNT_SHAMAN);
            if (_secondaryCheck)
            {
                StartConversationWithPlayer(CONVERSATION_LIGHTNINGBOLT_FIRST_SHAMAN);
                _secondaryCheck = false;
            }
            else
            {
                if (player->IsWithinDist(me, 2.0f))
                    StartConversationWithPlayer(CONVERSATION_LIGHTNINGBOLT_RANGE_SHAMAN);
            }
        }
    }

private:
    uint8 _primalStrikeCounter;
    bool _secondaryCheck;
};

// 164577 - Alliance Sparring Partner
// 166916 - Horde Sparring Partner
struct npc_sparring_partner_enhanced_combat_training_mage : public npc_sparring_partner_combat_training
{
    npc_sparring_partner_enhanced_combat_training_mage(Creature* creature) : npc_sparring_partner_combat_training(creature), _secondaryCheck(true) { }

    void OnReadyPointReached() override
    {
        StartConversationWithPlayer(CONVERSATION_READY_COMBAT);
        me->CastSpell(me, SPELL_AGGRO_RADIUS_CHECK_DNT_WARRIOR_MAGE);
        me->CastSpell(me, SPELL_RANGED_ROOT_DNT);
        _secondaryCheck = true;
    }

    void HandleClassEvent(uint32 eventId) override
    {
        switch (eventId)
        {
            case EVENT_COMBAT_TRAINING_RESET_MAGE:
                if (Unit* owner = me->GetDemonCreator())
                {
                    me->CastSpell(owner, SPELL_KNOCKBACK);
                    _events.ScheduleEvent(EVENT_COMBAT_TRAINING_AGGRO_CHECK_MAGE, 2s);
                }
                break;
            case EVENT_COMBAT_TRAINING_AGGRO_CHECK_MAGE:
                me->CastSpell(me, SPELL_AGGRO_RADIUS_CHECK_DNT_WARRIOR_MAGE);
                me->SetImmuneToPC(false);
                me->SetUninteractible(false);
                _secondaryCheck = true;
                break;
            default:
                break;
        }
    }

    void SpellHit(WorldObject* caster, SpellInfo const* spellInfo) override
    {
        Player* player = caster->ToPlayer();
        if (!player)
            return;

        if (spellInfo->Id == SPELL_FIRE_BLAST)
        {
            if (player->IsWithinDist(me, 2.0f))
            {
                player->KilledMonsterCredit(NPC_ALLIANCE_SPARRING_PARTNER_ENHANCED);

                if (player->GetQuestStatus(_questID) == QUEST_STATUS_COMPLETE)
                {
                    _events.ScheduleEvent(EVENT_COMBAT_TRAINING_END, 1s);
                }
                else
                {
                    StartConversationWithPlayer(CONVERSATION_FIRE_BLAST_QUEST_CREDIT_MAGE);
                    me->SetImmuneToPC(true);
                    me->SetUninteractible(true);
                    _events.ScheduleEvent(EVENT_COMBAT_TRAINING_RESET_MAGE, 4s);
                }
            }
            else
            {
                StartConversationWithPlayer(CONVERSATION_FIRE_BLAST_MAGE_NO_CREDIT);
            }
        }
        else if (spellInfo->Id == SPELL_FROSTBOLT)
        {
            me->RemoveAura(SPELL_RANGED_ROOT_DNT);
            me->RemoveAura(SPELL_AGGRO_RADIUS_CHECK_DNT_WARRIOR_MAGE);
            if (_secondaryCheck)
            {
                StartConversationWithPlayer(CONVERSATION_FROSTBOLT_MAGE);
                _secondaryCheck = false;
            }
            else
            {
                if (player->IsWithinDist(me, 2.0f))
                    StartConversationWithPlayer(CONVERSATION_FROSTBOLT_CLOSE_MAGE);
            }
        }
    }

private:
    bool _secondaryCheck;
};

// 164577 - Alliance Sparring Partner
// 166916 - Horde Sparring Partner
struct npc_sparring_partner_enhanced_combat_training_warlock : public npc_sparring_partner_combat_training
{
    npc_sparring_partner_enhanced_combat_training_warlock(Creature* creature) : npc_sparring_partner_combat_training(creature), _corruptionInPandemicWindow(false), _secondaryCheck(false) { }

    void OnReadyPointReached() override
    {
        StartConversationWithPlayer(CONVERSATION_READY_COMBAT);
        me->CastSpell(me, SPELL_AGGRO_RADIUS_CHECK_DNT_WARLOCK);
        me->CastSpell(me, SPELL_RANGED_ROOT_DNT);
        _secondaryCheck = true;
    }

    void HandleClassEvent(uint32 eventId) override
    {
        switch (eventId)
        {
            case EVENT_COMBAT_TRAINING_SPELL_FADING:
            {
                Aura* aura = me->GetAura(SPELL_CORRUPTION);
                if (!aura)
                    break;

                if (!_corruptionInPandemicWindow)
                {
                    int32 pandemicDuration = CalculatePct(aura->GetMaxDuration(), 30.0f);
                    if (aura->GetDuration() <= pandemicDuration)
                    {
                        _corruptionInPandemicWindow = true;
                        StartConversationWithPlayer(CONVERSATION_CORRUPTION_IS_FADING_WARLOCK);
                    }
                }
                _events.ScheduleEvent(EVENT_COMBAT_TRAINING_SPELL_FADING, 1s);
                break;
            }
            default:
                break;
        }
    }

    void SpellHit(WorldObject* caster, SpellInfo const* spellInfo) override
    {
        Player* player = caster->ToPlayer();
        if (!player)
            return;

        if (spellInfo->Id == SPELL_CORRUPTION)
        {
            if (_events.GetTimeUntilEvent(EVENT_COMBAT_TRAINING_SPELL_FADING) == Milliseconds::max())
                _events.ScheduleEvent(EVENT_COMBAT_TRAINING_SPELL_FADING, 1s);

            if (!_corruptionInPandemicWindow)
            {
                if (_secondaryCheck)
                {
                    StartConversationWithPlayer(CONVERSATION_CORRUPTION_CAST_PRE_COMBAT_WARLOCK);
                    me->SetImmuneToPC(false);
                    me->SetUninteractible(false);
                    me->RemoveAura(SPELL_RANGED_ROOT_DNT);
                    _secondaryCheck = false;
                }
                else
                    StartConversationWithPlayer(CONVERSATION_CORRUPTION_CAST_TOO_SOON_WARLOCK);
            }
            else
            {
                _corruptionInPandemicWindow = false;
                player->KilledMonsterCredit(NPC_ALLIANCE_SPARRING_PARTNER_ENHANCED);

                if (player->GetQuestStatus(_questID) == QUEST_STATUS_COMPLETE)
                    _events.ScheduleEvent(EVENT_COMBAT_TRAINING_END, 1s);
                else
                    StartConversationWithPlayer(CONVERSATION_CORRUPTION_QUEST_CREDIT_WARLOCK);
            }
        }
        else if (spellInfo->Id == SPELL_SHADOWBOLT)
        {
            if (_secondaryCheck)
                StartConversationWithPlayer(CONVERSATION_SHADOW_BOLT_PRE_COMBAT_WARLOCK);
        }
    }

private:
    bool _corruptionInPandemicWindow;
    bool _secondaryCheck;
};

// 164577 - Alliance Sparring Partner
// 166916 - Horde Sparring Partner
struct npc_sparring_partner_enhanced_combat_training_monk : public npc_sparring_partner_combat_training
{
    npc_sparring_partner_enhanced_combat_training_monk(Creature* creature) : npc_sparring_partner_combat_training(creature) { }

    void OnReadyPointReached() override
    {
        StartConversationWithPlayer(CONVERSATION_READY_COMBAT);
    }

    void SpellHit(WorldObject* caster, SpellInfo const* spellInfo) override
    {
        Player* player = caster->ToPlayer();
        if (!player)
            return;

        if (spellInfo->Id == SPELL_TIGER_PALM)
            player->KilledMonsterCredit(NPC_ALLIANCE_SPARRING_PARTNER_ENHANCED);
        else if (spellInfo->Id == SPELL_BLACKOUT_KICK)
            player->KilledMonsterCredit(NPC_ALLIANCE_SPARRING_PARTNER_ENHANCED2);

        if (player->GetQuestStatus(_questID) == QUEST_STATUS_COMPLETE)
            _events.ScheduleEvent(EVENT_COMBAT_TRAINING_END, 1s);
    }
};

// 164577 - Alliance Sparring Partner
// 166916 - Horde Sparring Partner
struct npc_sparring_partner_enhanced_combat_training_druid : public npc_sparring_partner_combat_training
{
    npc_sparring_partner_enhanced_combat_training_druid(Creature* creature) : npc_sparring_partner_combat_training(creature), _hitByMoonfire(false), _moonfireInPandemicWindow(false) { }

    void OnReadyPointReached() override
    {
        StartConversationWithPlayer(CONVERSATION_READY_COMBAT);
        me->CastSpell(me, SPELL_AGGRO_RADIUS_CHECK_DNT_DRUID);
        me->CastSpell(me, SPELL_RANGED_ROOT_DNT);
        _moonfireInPandemicWindow = false;
        _hitByMoonfire = false;
    }

    void HandleClassEvent(uint32 eventId) override
    {
        switch (eventId)
        {
            case EVENT_COMBAT_TRAINING_SPELL_FADING:
            {
                Aura* aura = me->GetAura(SPELL_MOONFIRE);
                if (!aura)
                    break;

                if (!_moonfireInPandemicWindow)
                {
                    int32 pandemicDuration = CalculatePct(aura->GetMaxDuration(), 30.0f);
                    if (aura->GetDuration() <= pandemicDuration)
                    {
                        _moonfireInPandemicWindow = true;
                        StartConversationWithPlayer(CONVERSATION_MOONFIRE_WEARING_OFF_DRUID);
                    }
                }
                _events.ScheduleEvent(EVENT_COMBAT_TRAINING_SPELL_FADING, 1s);
                break;
            }
            default:
                break;
        }
    }

    void SpellHit(WorldObject* caster, SpellInfo const* spellInfo) override
    {
        Player* player = caster->ToPlayer();
        if (!player)
            return;

        if (spellInfo->Id == SPELL_MOONFIRE)
        {
            if (_events.GetTimeUntilEvent(EVENT_COMBAT_TRAINING_SPELL_FADING) == Milliseconds::max())
                _events.ScheduleEvent(EVENT_COMBAT_TRAINING_SPELL_FADING, 1s);

            if (!_moonfireInPandemicWindow)
            {
                if (!_hitByMoonfire)
                {
                    StartConversationWithPlayer(CONVERSATION_MOONFIRE_CAST_PRE_COMBAT_DRUID);
                    me->SetImmuneToPC(false);
                    me->SetUninteractible(false);
                    me->RemoveAura(SPELL_RANGED_ROOT_DNT);
                    _hitByMoonfire = true;
                }
                else
                    StartConversationWithPlayer(CONVERSATION_MOONFIRE_CAST_TOO_SOON_DRUID);
            }
            else
            {
                _moonfireInPandemicWindow = false;
                player->KilledMonsterCredit(NPC_ALLIANCE_SPARRING_PARTNER_ENHANCED);

                if (player->GetQuestStatus(_questID) == QUEST_STATUS_COMPLETE)
                    _events.ScheduleEvent(EVENT_COMBAT_TRAINING_END, 1s);
                else
                    StartConversationWithPlayer(CONVERSATION_MOONFIRE_QUEST_CREDIT_DRUID);
            }
        }
        else if (spellInfo->Id == SPELL_WRATH)
        {
            if (!_hitByMoonfire)
                StartConversationWithPlayer(CONVERSATION_WRATH_PRE_COMBAT_DRUID);
        }
    }

private:
    bool _hitByMoonfire;
    bool _moonfireInPandemicWindow;
};

CreatureAI* SparringPartnerEnhancedCombatTrainingSelector(Creature* creature)
{
    TempSummon* summon = creature->ToTempSummon();
    if (!summon)
        return new NullCreatureAI(creature);

    Unit* summoner = summon->GetSummonerUnit();
    if (!summoner)
        return new NullCreatureAI(creature);

    Player* player = summoner->ToPlayer();
    if (!player)
        return new NullCreatureAI(creature);

    switch (player->GetClass())
    {
        case CLASS_WARRIOR:
            return new npc_sparring_partner_enhanced_combat_training_warrior(creature);
        case CLASS_PALADIN:
            return new npc_sparring_partner_enhanced_combat_training_paladin(creature);
        case CLASS_ROGUE:
            return new npc_sparring_partner_enhanced_combat_training_rogue(creature);
        case CLASS_PRIEST:
            return new npc_sparring_partner_enhanced_combat_training_priest(creature);
        case CLASS_SHAMAN:
            return new npc_sparring_partner_enhanced_combat_training_shaman(creature);
        case CLASS_MAGE:
            return new npc_sparring_partner_enhanced_combat_training_mage(creature);
        case CLASS_WARLOCK:
            return new npc_sparring_partner_enhanced_combat_training_warlock(creature);
        case CLASS_MONK:
            return new npc_sparring_partner_enhanced_combat_training_monk(creature);
        case CLASS_DRUID:
            return new npc_sparring_partner_enhanced_combat_training_druid(creature);
        default:
            return new NullCreatureAI(creature);
    }
}

struct at_aggro_radius_check_enhanced_combat_tactics : AreaTriggerAI
{
    at_aggro_radius_check_enhanced_combat_tactics(AreaTrigger* areatrigger) : AreaTriggerAI(areatrigger) { }

    void OnUnitEnter(Unit* unit) override
    {
        Player* player = unit->ToPlayer();
        if (!player)
            return;

        Unit* caster = at->GetCaster();
        if (!caster)
        {
            at->RemoveFromWorld();
            return;
        }

        caster->SetFacingToObject(player);

        switch (player->GetClass())
        {
            case CLASS_WARRIOR:
                caster->CastSpell(player, SPELL_CHARGE_KNOCKBACK_WARRIOR);
                break;
            case CLASS_PRIEST:
            case CLASS_SHAMAN:
            case CLASS_MAGE:
            case CLASS_WARLOCK:
                caster->CastSpell(player, SPELL_CHARGE_KNOCKBACK);
                break;
            case CLASS_DRUID:
                caster->CastSpell(player, SPELL_CHARGE_KNOCKBACK_DRUID);
                break;
            default:
                break;
        }
    }
};

// 320605 - Charge Knockback (DNT)
class spell_knockback_charge_enhanced_training : public SpellScript
{
    void HandleLaunch(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);
    }

    void HandleEffect(SpellEffIndex effIndex)
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        Player* player = GetHitUnit()->ToPlayer();
        if (!player)
            return;

        Conversation* conversation = Conversation::CreateConversation(GetSpellInfo()->GetEffect(effIndex).MiscValue, player, *player, player->GetGUID(), nullptr, false);
        if (!conversation)
            return;

        conversation->AddActor(ACTOR_ID_ALLIANCE_ENHANCED_TRAINING, 0, player->GetTeam() == ALLIANCE ? caster->GetGUID() : ObjectGuid::Empty);
        conversation->AddActor(ACTOR_ID_HORDE_ENHANCED_TRAINING, 1, player->GetTeam() == ALLIANCE ? ObjectGuid::Empty : caster->GetGUID());
        conversation->Start();
    }

    void Register() override
    {
        OnEffectLaunchTarget += SpellEffectFn(spell_knockback_charge_enhanced_training::HandleLaunch, EFFECT_1, SPELL_EFFECT_CREATE_PRIVATE_CONVERSATION);
        OnEffectHitTarget += SpellEffectFn(spell_knockback_charge_enhanced_training::HandleEffect, EFFECT_1, SPELL_EFFECT_CREATE_PRIVATE_CONVERSATION);
    }
};

enum NorthboundData
{
    CONVERSATION_QUEST_NORTHBOUND_ACCEPT_ALLIANCE       = 12066,
    CONVERSATION_QUEST_NORTHBOUND_ACCEPT_HORDE          = 14499,

    POINT_LEADER_RUN                                    = 0,

    ACTOR_ID_0_NORTHBOUND_ACCEPT_ALLIANCE               = 71310,
    ACTOR_ID_1_NORTHBOUND_ACCEPT_ALLIANCE               = 71297,
    ACTOR_ID_0_NORTHBOUND_ACCEPT_HORDE                  = 79890,
    ACTOR_ID_1_NORTHBOUND_ACCEPT_HORDE                  = 79888,
    ACTOR_ID_0_NORTHBOUND_AREATRIGGER_ALLIANCE          = 71317,
    ACTOR_ID_1_NORTHBOUND_AREATRIGGER_HORDE             = 76319,

    QUEST_NORTHBOND_ALLIANCE                            = 55173,
    QUEST_NORTHBOND_HORDE                               = 59935,

    SPELL_SUMMON_ADMIRAL_GARRICK_GUARDIAN_NORTHBOUND    = 305660,
    SPELL_SUMMON_WARLORD_GRIMAXE_GUARDIAN_NORTHBOUND    = 344382,
    SPELL_LINGER_NORTHBOUND_ALLIANCE                    = 305665,
    SPELL_LINGER_NORTHBOUND_HORDE                       = 344385,
};

static constexpr Position GarrickQuillboarBriarpatchPosition = { -142.62154f, -2641.0364f, 48.775497f };
static constexpr Position GrimaxeQuillboarBriarpatchPosition = { -142.56076f, -2640.9915f, 48.755478f };

// 165360 - Alliance Survivor
// This script is used by Captian Garrick Follower for Northbound quest
struct npc_leader_northbound : public ScriptedAI
{
    npc_leader_northbound(Creature* creature) : ScriptedAI(creature), _conversationId(0), _actorIdOne(0), _actorIdTwo(0), _lingerSpellId(0), _guardianSpellId(0) {}

    void JustAppeared() override
    {
        Player* player = me->GetAffectingPlayer();
        if (!player)
            return;

        player->UpdateVisibilityForPlayer();

        Creature* survivor = FindCreatureIgnorePhase(player, player->GetTeam() == ALLIANCE ? "alaria_standing_abandoned_camp" : "wonza_standing_abandoned_camp", 5.0f);
        if (!survivor)
            return;

        if (player->GetTeam() == ALLIANCE)
        {
            _conversationId = CONVERSATION_QUEST_NORTHBOUND_ACCEPT_ALLIANCE;
            _actorIdOne = ACTOR_ID_0_NORTHBOUND_ACCEPT_ALLIANCE;
            _actorIdTwo = ACTOR_ID_1_NORTHBOUND_ACCEPT_ALLIANCE;
            _runToPosition = GarrickQuillboarBriarpatchPosition;
            _lingerSpellId = SPELL_LINGER_NORTHBOUND_ALLIANCE;
            _guardianSpellId = SPELL_SUMMON_ADMIRAL_GARRICK_GUARDIAN_NORTHBOUND;
        }
        else
        {
            _conversationId = CONVERSATION_QUEST_NORTHBOUND_ACCEPT_HORDE;
            _actorIdOne = ACTOR_ID_0_NORTHBOUND_ACCEPT_HORDE;
            _actorIdTwo = ACTOR_ID_1_NORTHBOUND_ACCEPT_HORDE;
            _runToPosition = GrimaxeQuillboarBriarpatchPosition;
            _lingerSpellId = SPELL_LINGER_NORTHBOUND_HORDE;
            _guardianSpellId = SPELL_SUMMON_WARLORD_GRIMAXE_GUARDIAN_NORTHBOUND;
        }

        Conversation* conversation = Conversation::CreateConversation(_conversationId, player, *player, player->GetGUID(), nullptr, false);
        if (!conversation)
            return;

        conversation->AddActor(0, 0, player->GetGUID());
        conversation->AddActor(_actorIdOne, 1, me->GetGUID());
        conversation->AddActor(_actorIdTwo, 2, survivor->GetGUID());
        conversation->Start();
    }

    void IsSummonedBy(WorldObject* /*summoner*/) override
    {
        _events.ScheduleEvent(EVENT_FOLLOW_PLAYER, 3s);
    }

    void SpellHit(WorldObject* /*caster*/, SpellInfo const* spellInfo) override
    {
        if (spellInfo->Id == _lingerSpellId)
        {
            me->GetMotionMaster()->Remove(FOLLOW_MOTION_TYPE);
            me->GetMotionMaster()->MovePoint(POINT_LEADER_RUN, _runToPosition, false);
        }
        return;
    }

    void MovementInform(uint32 uiType, uint32 uiId) override
    {
        if (uiType != POINT_MOTION_TYPE)
            return;

        if (uiId != POINT_LEADER_RUN)
            return;

        me->SetFacingTo(6.0737457275390625);

        if (Player* player = me->GetAffectingPlayer())
        {
            player->CastSpell(player, SPELL_UPDATE_PHASE_SHIFT);
            player->RemoveAura(_guardianSpellId);
            player->UpdateVisibilityForPlayer();
        }
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_FOLLOW_PLAYER:
                    if (Player* player = me->GetAffectingPlayer())
                        me->GetMotionMaster()->MoveFollow(player, 0.0f, float(M_PI / 4.0f));
                    break;
                default:
                    break;
            }
        }
    }

private:
    EventMap _events;
    uint32 _conversationId;
    uint32 _actorIdOne;
    uint32 _actorIdTwo;
    Position _runToPosition;
    uint32 _lingerSpellId;
    uint32 _guardianSpellId;
};

// 55173 - Northbound
// 59935 - Northbound
class quest_northbound : public QuestScript
{
public:
    quest_northbound(char const* script) : QuestScript(script) { }

    void HandleQuestStatusChange(Player* player, QuestStatus newStatus, uint32 summonSpellId)
    {
        switch (newStatus)
        {
            case QUEST_STATUS_INCOMPLETE:
                player->CastSpell(player, SPELL_UPDATE_PHASE_SHIFT);
                player->CastSpell(player, summonSpellId);
                break;
            case QUEST_STATUS_NONE:
                player->RemoveAura(summonSpellId);
                player->CastSpell(player, SPELL_UPDATE_PHASE_SHIFT);
                player->UpdateVisibilityForPlayer();
                break;
            default:
                break;
        }
    }
};

// 55173 - Northbound
class quest_northbound_alliance : public quest_northbound
{
public:
    quest_northbound_alliance() : quest_northbound("quest_northbound_alliance") { }

    void OnQuestStatusChange(Player* player, Quest const* /*quest*/, QuestStatus /*oldStatus*/, QuestStatus newStatus) override
    {
        HandleQuestStatusChange(player, newStatus, SPELL_SUMMON_ADMIRAL_GARRICK_GUARDIAN_NORTHBOUND);
    }
};

// 59935 - Northbound
class quest_northbound_horde : public quest_northbound
{
public:
    quest_northbound_horde() : quest_northbound("quest_northbound_horde") { }

    void OnQuestStatusChange(Player* player, Quest const* /*quest*/, QuestStatus /*oldStatus*/, QuestStatus newStatus) override
    {
        HandleQuestStatusChange(player, newStatus, SPELL_SUMMON_WARLORD_GRIMAXE_GUARDIAN_NORTHBOUND);
    }
};

// 305661 - Summon Admiral Garrick Guardian Summons Alliance Entry: 165360
// 344383 - Summon Admiral Garrick Guardian Summons Horde Entry: 175034
class spell_summon_leader_northbound : public SpellScript
{
    // @TODO: drop after TARGET_UNK_142 impl

    void SelectTarget(WorldObject*& target)
    {
        Player* caster = GetCaster()->ToPlayer();
        if (!caster)
            return;

        Creature* survivor = FindCreatureIgnorePhase(caster, caster->GetTeam() == ALLIANCE ? "garrick_camp" : "grimaxe_camp", 5.0f);
        if (!survivor)
            return;

        target = survivor;
    }

    void Register() override
    {
        OnObjectTargetSelect += SpellObjectTargetSelectFn(spell_summon_leader_northbound::SelectTarget, EFFECT_0, TARGET_DEST_NEARBY_ENTRY_OR_DB);
    }
};

struct at_northbound_linger : AreaTriggerAI
{
    at_northbound_linger(AreaTrigger* areatrigger) : AreaTriggerAI(areatrigger) { }

    void OnUnitEnter(Unit* unit) override
    {
        Player* player = unit->ToPlayer();
        if (!player)
            return;

        if (player->GetTeam() == ALLIANCE)
        {
            if (player->GetQuestStatus(QUEST_NORTHBOND_ALLIANCE) != QUEST_STATUS_COMPLETE)
                return;

            if (!player->HasAura(SPELL_SUMMON_ADMIRAL_GARRICK_GUARDIAN_NORTHBOUND))
                return;

            if (player->HasAura(SPELL_LINGER_NORTHBOUND_ALLIANCE))
                return;

            player->CastSpell(player, SPELL_LINGER_NORTHBOUND_ALLIANCE);
        }
        else
        {
            if (player->GetQuestStatus(QUEST_NORTHBOND_HORDE) != QUEST_STATUS_COMPLETE)
                return;

            if (!player->HasAura(SPELL_SUMMON_WARLORD_GRIMAXE_GUARDIAN_NORTHBOUND))
                return;

            if (player->HasAura(SPELL_LINGER_NORTHBOUND_HORDE))
                return;

            player->CastSpell(player, SPELL_LINGER_NORTHBOUND_HORDE);
        }
    }
};

// @TODO: drop
// 305665 - Scene Linger (DNT)
// 344385 - Scene Linger (DNT)
class spell_scene_linger_northbound: public SpellScript
{
    void HandleLaunch(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);
    }

    void HandleEffect(SpellEffIndex effIndex)
    {
        Player* player = GetHitUnit()->ToPlayer();
        if (!player)
            return;

        Creature* scout = FindCreatureIgnorePhase(player, player->GetTeam() == ALLIANCE ? "huxworth_briarpatch" : "dawntracker_briarpatch", 100.0f);
        if (!scout)
            return;

        Conversation* conversation = Conversation::CreateConversation(GetSpellInfo()->GetEffect(effIndex).MiscValue, player, *player, player->GetGUID(), nullptr, false);
        if (!conversation)
            return;

        conversation->AddActor(ACTOR_ID_0_NORTHBOUND_AREATRIGGER_ALLIANCE, 0, player->GetTeam() == ALLIANCE ? scout->GetGUID() : ObjectGuid::Empty);
        conversation->AddActor(ACTOR_ID_1_NORTHBOUND_AREATRIGGER_HORDE, 1, player->GetTeam() == ALLIANCE ? ObjectGuid::Empty : scout->GetGUID());
        conversation->Start();
    }

    void Register() override
    {
        OnEffectLaunchTarget += SpellEffectFn(spell_scene_linger_northbound::HandleLaunch, EFFECT_2, SPELL_EFFECT_CREATE_PRIVATE_CONVERSATION);
        OnEffectHitTarget += SpellEffectFn(spell_scene_linger_northbound::HandleEffect, EFFECT_2, SPELL_EFFECT_CREATE_PRIVATE_CONVERSATION);
    }
};

// ***********************************************************************
// * Scripting in this section occurs after reaching Quilboar Briarpatch *
// ***********************************************************************

// Taming the Wild Quest
enum TamingTheWilds
{
    QUEST_TAMING_THE_WILDS_ALLIANCE         = 59342,
    QUEST_TAMING_THE_WILDS_HORDE            = 59937,
    QUEST_TRACKER_TAMING_THE_WILDS_COMPLETE = 55607,

    QUEST_OBJECTIVE_TRAINED_ALLIANCE        = 84761,
    QUEST_OBJECTIVE_TRAINED_HORDE           = 85021,
    QUEST_OBJECTIVE_BEAST_TAMED_ALLIANCE    = 84759,
    QUEST_OBJECTIVE_BEAST_TAMED_HORDE       = 85023,

    SPELL_TAME_BEAST                        = 1515
};

// 59342 - Taming the Wilds
// 59937 - Taming the Wilds
class quest_taming_the_wilds : public QuestScript
{
public:
    quest_taming_the_wilds(char const* script) : QuestScript(script) { }

    void HandleQuestStatusChange(Player* player, QuestStatus newStatus, std::string_view creatureString, uint32 questObjective1, uint32 questObjective2)
    {
        switch (newStatus)
        {
            case QUEST_STATUS_INCOMPLETE:
                if (Creature* survivor = FindCreatureIgnorePhase(player, creatureString, 5.0f))
                    survivor->SummonPersonalClone(survivor->GetPosition(), TEMPSUMMON_MANUAL_DESPAWN, 0s, 0, 0, player);

                // Hacks QUEST_OBJECTIVE_CRITERIA_TREE needs research
                if (player->HasSpell(SPELL_TAME_BEAST))
                    player->UpdateQuestObjectiveProgress(QUEST_OBJECTIVE_CRITERIA_TREE, questObjective1, 1);

                if (player->GetQuestStatus(QUEST_TRACKER_TAMING_THE_WILDS_COMPLETE) == QUEST_STATUS_REWARDED)
                    player->UpdateQuestObjectiveProgress(QUEST_OBJECTIVE_CRITERIA_TREE, questObjective2, 1);
                break;
            default:
                break;
        }
    }
};

// 59342 - Taming The Wild
class quest_taming_the_wilds_alliance : public quest_taming_the_wilds
{
public:
    quest_taming_the_wilds_alliance() : quest_taming_the_wilds("quest_taming_the_wilds_alliance") { }

    void OnQuestStatusChange(Player* player, Quest const* /*quest*/, QuestStatus /*oldStatus*/, QuestStatus newStatus) override
    {
        HandleQuestStatusChange(player, newStatus, "huxworth_briarpatch", QUEST_OBJECTIVE_TRAINED_ALLIANCE, QUEST_OBJECTIVE_BEAST_TAMED_ALLIANCE);
    }
};

// 59937 - Taming The Wild
class quest_taming_the_wilds_horde : public quest_taming_the_wilds
{
public:
    quest_taming_the_wilds_horde() : quest_taming_the_wilds("quest_taming_the_wilds_horde") { }

    void OnQuestStatusChange(Player* player, Quest const* /*quest*/, QuestStatus /*oldStatus*/, QuestStatus newStatus) override
    {
        HandleQuestStatusChange(player, newStatus, "dawntracker_briarpatch", QUEST_OBJECTIVE_TRAINED_HORDE, QUEST_OBJECTIVE_BEAST_TAMED_HORDE);
    }
};

enum TamingTheWildsData
{
    CONVERSATION_HORDE_TRAINER          = 14613,
    CONVERSATION_HORDE_ACTOR            = 76440,

    EVENT_ME_TURN_TO_PLAYER             = 1,
    EVENT_ME_END_OF_CAST                = 2,

    SPELL_TUTORIAL_HEALTH_DNT           = 316840,
    SPELL_LEARNING_TAME_BEAST           = 320852,
    SPELL_LEARN_TAME_BEAST              = 320840,
    SPELL_LEARN_CALL_PET                = 320842,
    SPELL_TAME_BEAST_EFFECT             = 13481,

    SAY_PET_TRAINING_ALLIANCE           = 0,
    SAY_FIND_A_BEAST_ALLIANCE           = 1,
    SAY_FIND_A_BEAST_HORDE              = 0
};

// 154327 - Austin Huxsworth
struct npc_huxsworth_hunter_quest_private : public ScriptedAI
{
    npc_huxsworth_hunter_quest_private(Creature* creature) : ScriptedAI(creature) { }

    void InitializeAI() override
    {
        me->RemoveNpcFlag(UNIT_NPC_FLAG_QUESTGIVER | UNIT_NPC_FLAG_GOSSIP);
    }

    void JustAppeared() override
    {
        me->SetStandState(UNIT_STAND_STATE_STAND);
        _events.ScheduleEvent(EVENT_ME_TURN_TO_PLAYER, 1s);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_ME_TURN_TO_PLAYER:
                    if (Player* player = ObjectAccessor::GetPlayer(*me, me->GetPrivateObjectOwner()))
                    {
                        me->SetFacingToObject(player);
                        me->SetEmoteState(EMOTE_STATE_TALK);
                        Talk(SAY_PET_TRAINING_ALLIANCE);
                        player->CastSpell(player, SPELL_LEARNING_TAME_BEAST);
                        _events.ScheduleEvent(EVENT_ME_END_OF_CAST, 8s);
                    }
                    break;
                case EVENT_ME_END_OF_CAST:
                    if (Player* player = ObjectAccessor::GetPlayer(*me, me->GetPrivateObjectOwner()))
                    {
                        if (player->GetQuestStatus(QUEST_TRACKER_TAMING_THE_WILDS_COMPLETE) != QUEST_STATUS_REWARDED)
                            player->CastSpell(player, SPELL_TUTORIAL_HEALTH_DNT);

                        if (!player->HasSpell(SPELL_TAME_BEAST))
                        {
                            player->UpdateQuestObjectiveProgress(QUEST_OBJECTIVE_CRITERIA_TREE, QUEST_OBJECTIVE_TRAINED_ALLIANCE, 1);
                            player->CastSpell(player, SPELL_LEARN_TAME_BEAST);
                            player->CastSpell(player, SPELL_LEARN_CALL_PET);
                        }

                        me->SetEmoteState(EMOTE_STATE_NONE);
                        Talk(SAY_FIND_A_BEAST_ALLIANCE);
                    }
                    me->DespawnOrUnsummon(4s);
                    break;
                default:
                    break;
            }
        }
    }

private:
    EventMap _events;
};

enum HuxsworthBriarpatchData
{
    CONVERSATION_BRIARPATCH_ALLIANCE        = 12073,

    CONVERSATION_ACTOR_GARRICK_BRIARPATCH   = 71326,
    CONVERSATION_ACTOR_HUXSWORTH_BRIARPATCH = 71327,

    EVENT_HUXSWORTH_GARRICK_CONVERSATION    = 1,
    EVENT_HUXSWORTH_GARRICK_RUN_BRIARPATCH  = 2,
};

static constexpr Position GarrickBriarpatchDespawnPosition = { -112.92383f, -2640.541f, 52.35042f };
static constexpr Position HuxworthBriarpatchDespawnPosition = { -112.61979f, -2645.9775f, 52.22835f };

// 154327 - Austin Huxsworth
struct npc_huxsworth_briarpatch_quest_private : public ScriptedAI
{
    npc_huxsworth_briarpatch_quest_private(Creature* creature) : ScriptedAI(creature) { }

    void InitializeAI() override
    {
        me->RemoveNpcFlag(UNIT_NPC_FLAG_QUESTGIVER | UNIT_NPC_FLAG_GOSSIP);
    }

    void JustAppeared() override
    {
        me->SetStandState(UNIT_STAND_STATE_STAND);
        if (Creature* garrick = ObjectAccessor::GetCreature(*me, _garrickGUID))
            garrick->SetStandState(UNIT_STAND_STATE_STAND);

        _events.ScheduleEvent(EVENT_HUXSWORTH_GARRICK_CONVERSATION, 2s);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_HUXSWORTH_GARRICK_CONVERSATION:
                {
                    if (Unit* player = ObjectAccessor::GetPlayer(*me, me->GetPrivateObjectOwner()))
                    {
                        Conversation* conversation = Conversation::CreateConversation(CONVERSATION_BRIARPATCH_ALLIANCE, player, *player, player->GetGUID(), nullptr, false);
                        if (!conversation)
                            break;

                        conversation->AddActor(CONVERSATION_ACTOR_GARRICK_BRIARPATCH, 0, _garrickGUID);
                        conversation->AddActor(CONVERSATION_ACTOR_HUXSWORTH_BRIARPATCH, 1, me->GetGUID());
                        conversation->Start();
                        _events.ScheduleEvent(EVENT_HUXSWORTH_GARRICK_RUN_BRIARPATCH, 13s);
                    }
                    break;
                }
                case EVENT_HUXSWORTH_GARRICK_RUN_BRIARPATCH:
                    if (Creature* garrick = ObjectAccessor::GetCreature(*me, _garrickGUID))
                    {
                        garrick->GetMotionMaster()->MovePoint(0, GarrickBriarpatchDespawnPosition);
                        garrick->DespawnOrUnsummon(3s);
                    }
                    me->GetMotionMaster()->MovePoint(0, HuxworthBriarpatchDespawnPosition);
                    me->DespawnOrUnsummon(3s);
                    break;
                default:
                    break;
            }
        }
    }

public:
    void SetGarrickGUID(ObjectGuid garrickGUID)
    {
        _garrickGUID = garrickGUID;
    }

private:
    EventMap _events;
    ObjectGuid _garrickGUID;
};

// 166996 - Mithdran Dawntracker
struct npc_dawntracker_hunter_quest_private : public ScriptedAI
{
    npc_dawntracker_hunter_quest_private(Creature* creature) : ScriptedAI(creature) { }

    void InitializeAI() override
    {
        me->RemoveNpcFlag(UNIT_NPC_FLAG_QUESTGIVER | UNIT_NPC_FLAG_GOSSIP);
    }

    void JustAppeared() override
    {
        me->SetStandState(UNIT_STAND_STATE_STAND);
        _events.ScheduleEvent(EVENT_ME_TURN_TO_PLAYER, 1s);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_ME_TURN_TO_PLAYER:
                    if (Unit* player = ObjectAccessor::GetPlayer(*me, me->GetPrivateObjectOwner()))
                    {
                        me->SetFacingToObject(player);
                        me->SetEmoteState(EMOTE_STATE_TALK);
                        Conversation* conversation = Conversation::CreateConversation(CONVERSATION_HORDE_TRAINER, player, *player, player->GetGUID(), nullptr, false);
                        if (conversation)
                        {
                            conversation->AddActor(CONVERSATION_HORDE_ACTOR, 0, me->GetGUID());
                            conversation->Start();
                        }
                        player->CastSpell(player, SPELL_LEARNING_TAME_BEAST);
                        _events.ScheduleEvent(EVENT_ME_END_OF_CAST, 8s);
                    }
                    break;
                case EVENT_ME_END_OF_CAST:
                    if (Player* player = ObjectAccessor::GetPlayer(*me, me->GetPrivateObjectOwner()))
                    {
                        if (player->GetQuestStatus(QUEST_TRACKER_TAMING_THE_WILDS_COMPLETE) != QUEST_STATUS_REWARDED)
                            player->CastSpell(player, SPELL_TUTORIAL_HEALTH_DNT);

                        if (!player->HasSpell(SPELL_TAME_BEAST))
                        {
                            player->UpdateQuestObjectiveProgress(QUEST_OBJECTIVE_CRITERIA_TREE, QUEST_OBJECTIVE_TRAINED_HORDE, 1);
                            player->CastSpell(player, SPELL_LEARN_TAME_BEAST);
                            player->CastSpell(player, SPELL_LEARN_CALL_PET);
                        }
                    }
                    me->SetEmoteState(EMOTE_STATE_NONE);
                    Talk(SAY_FIND_A_BEAST_HORDE);
                    me->DespawnOrUnsummon(4s);
                    break;
                default:
                    break;
            }
        }
    }

private:
    EventMap _events;
};

enum DawntrackerBriarpatch
{
    CONVERSATION_BRIARPATCH_HORDE               = 14513,

    CONVERSATION_ACTOR_GRIMAXE_BRIARPATCH       = 76330,
    CONVERSATION_ACTOR_DAWNTRACKER_BRIARPATCH   = 76331,

    EVENT_DAWNTRACKER_GRIMAXE_CONVERSATION      = 1,
    EVENT_DAWNTRACKER_GRIMAXE_RUN_BRIARPATCH    = 2,
};

static constexpr Position GrimaxeBriarpatchDespawnPosition = { -112.92383f, -2640.541f, 52.35042f };
static constexpr Position DawntrackerBriarpatchDespawnPosition = { -112.61979f, -2645.9775f, 52.22835f };

// 166996 - Mithdran Dawntracker
struct npc_dawntracker_briarpatch_quest_private : public ScriptedAI
{
    npc_dawntracker_briarpatch_quest_private(Creature* creature) : ScriptedAI(creature) { }

    void InitializeAI() override
    {
        me->RemoveNpcFlag(UNIT_NPC_FLAG_QUESTGIVER | UNIT_NPC_FLAG_GOSSIP);
    }

    void JustAppeared() override
    {
        me->SetStandState(UNIT_STAND_STATE_STAND);
        if (Creature* grimaxe = ObjectAccessor::GetCreature(*me, _grimaxeGUID))
            grimaxe->SetStandState(UNIT_STAND_STATE_STAND);

        _events.ScheduleEvent(EVENT_DAWNTRACKER_GRIMAXE_CONVERSATION, 2s);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_DAWNTRACKER_GRIMAXE_CONVERSATION:
                {
                    if (Unit* player = ObjectAccessor::GetPlayer(*me, me->GetPrivateObjectOwner()))
                    {
                        Conversation* conversation = Conversation::CreateConversation(CONVERSATION_BRIARPATCH_HORDE, player, *player, player->GetGUID(), nullptr, false);
                        if (!conversation)
                            break;

                        conversation->AddActor(CONVERSATION_ACTOR_GRIMAXE_BRIARPATCH, 0, _grimaxeGUID);
                        conversation->AddActor(CONVERSATION_ACTOR_DAWNTRACKER_BRIARPATCH, 1, me->GetGUID());
                        conversation->Start();
                        _events.ScheduleEvent(EVENT_HUXSWORTH_GARRICK_RUN_BRIARPATCH, 13s);
                    }
                    break;
                }
                case EVENT_DAWNTRACKER_GRIMAXE_RUN_BRIARPATCH:
                    if (Creature* grimaxe = ObjectAccessor::GetCreature(*me, _grimaxeGUID))
                    {
                        grimaxe->GetMotionMaster()->MovePoint(0, GrimaxeBriarpatchDespawnPosition );
                        grimaxe->DespawnOrUnsummon(2s);
                    }
                    me->GetMotionMaster()->MovePoint(0, DawntrackerBriarpatchDespawnPosition);
                    me->DespawnOrUnsummon(2s);
                    break;
                default:
                    break;
            }
        }
    }

public:
    void SetGrimaxeGUID(ObjectGuid grimaxeGUID)
    {
        _grimaxeGUID = grimaxeGUID;
    }

private:
    EventMap _events;
    ObjectGuid _grimaxeGUID;
};

CreatureAI* HuxsworthBriarpatchSelector(Creature* creature)
{
    if (creature->IsPrivateObject())
    {
        if (Player* privateObjectOwner = ObjectAccessor::GetPlayer(*creature, creature->GetPrivateObjectOwner()))
        {
            if (privateObjectOwner->GetQuestStatus(QUEST_TAMING_THE_WILDS_ALLIANCE) == QUEST_STATUS_INCOMPLETE)
                return new npc_huxsworth_hunter_quest_private(creature);
            else
                return new npc_huxsworth_briarpatch_quest_private(creature);
        }
    }
    return new NullCreatureAI(creature);
}

CreatureAI* DawntrackerBriarpatchSelector(Creature* creature)
{
    if (creature->IsPrivateObject())
    {
        if (Player* privateObjectOwner = ObjectAccessor::GetPlayer(*creature, creature->GetPrivateObjectOwner()))
        {
            if (privateObjectOwner->GetQuestStatus(QUEST_TAMING_THE_WILDS_HORDE) == QUEST_STATUS_INCOMPLETE)
                return new npc_dawntracker_hunter_quest_private(creature);
            else
                return new npc_dawntracker_briarpatch_quest_private(creature);
        }
    }
    return new NullCreatureAI(creature);
}

// 316840 - Tutorial - Health (DNT)
class spell_tutorial_health_dnt_proc_aura : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_TAME_BEAST_EFFECT });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        if (eventInfo.GetSpellInfo() && eventInfo.GetSpellInfo()->Id == SPELL_TAME_BEAST_EFFECT)
            return true;
        return false;
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_tutorial_health_dnt_proc_aura::CheckProc);
    }
};

// 316841 - Tutorial - Health (DNT)
class spell_tutorial_health_dnt : public SpellScript
{
    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        // This shouldn't happen until tame beast spell completes
        if (Player* player = GetCaster()->ToPlayer())
        {
            if (player->GetTeam() == ALLIANCE)
                player->UpdateQuestObjectiveProgress(QUEST_OBJECTIVE_CRITERIA_TREE, QUEST_OBJECTIVE_BEAST_TAMED_ALLIANCE, 1);
            else
                player->UpdateQuestObjectiveProgress(QUEST_OBJECTIVE_CRITERIA_TREE, QUEST_OBJECTIVE_BEAST_TAMED_HORDE, 1);
        }
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_tutorial_health_dnt::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

enum QuilboarQuest
{
    QUEST_DOWN_WITH_THE_QUILBOAR_ALLIANCE            = 55186,
    QUEST_FORBIDDEN_QUILBOAR_NECROMANY_ALLIANCE      = 55184,
    QUEST_DOWN_WITH_THE_QUILBOAR_HORDE               = 59938,
    QUEST_FORBIDDEN_QUILBOAR_NECROMANY_HORDE         = 59939,

    SPELL_VALIDATED_QUEST_ACCEPT_BRIARPATCH_ALLIANCE = 298984,
    SPELL_VALIDATED_QUEST_ACCEPT_BRIARPATCH_HORDE    = 325309
};

// 55186 - Quest Down with the Quilboar "Alliance"
// 55184 - Forbidden Quilboar Necromancy "Alliance"
// 59938 - Quest Down with the Quilboar "Horde"
// 59939 - Forbidden Quilboar Necromancy "Horde"
class quest_briarpatch : public QuestScript
{
public:
    quest_briarpatch(char const* script) : QuestScript(script) { }

    void HandleQuestStatusChange(Player* player, QuestStatus newStatus, uint32 questDown, uint32 questForbidden, uint32 spellValidated)
    {
        switch (newStatus)
        {
            case QUEST_STATUS_INCOMPLETE:
                if (player->GetQuestStatus(questDown) != QUEST_STATUS_NONE && player->GetQuestStatus(questForbidden) != QUEST_STATUS_NONE)
                {
                    player->CastSpell(player, SPELL_UPDATE_PHASE_SHIFT);
                    player->CastSpell(player, spellValidated);
                }
                break;
            case QUEST_STATUS_NONE:
                player->CastSpell(player, SPELL_UPDATE_PHASE_SHIFT);
                break;
            default:
                break;
        }
    }
};

// 55186 - Quest Down with the Quilboar "Alliance"
// 55184 - Forbidden Quilboar Necromancy "Alliance"
class quest_briarpatch_alliance : public quest_briarpatch
{
public:
    quest_briarpatch_alliance() : quest_briarpatch("quest_briarpatch_alliance") { }

    void OnQuestStatusChange(Player* player, Quest const* /*quest*/, QuestStatus /*oldStatus*/, QuestStatus newStatus) override
    {
        HandleQuestStatusChange(player, newStatus, QUEST_DOWN_WITH_THE_QUILBOAR_ALLIANCE, QUEST_FORBIDDEN_QUILBOAR_NECROMANY_ALLIANCE, SPELL_VALIDATED_QUEST_ACCEPT_BRIARPATCH_ALLIANCE);
    }
};

// 59938 - Quest Down with the Quilboar "Horde"
// 59939 - Forbidden Quilboar Necromancy "Horde"
class quest_briarpatch_horde : public quest_briarpatch
{
public:
    quest_briarpatch_horde() : quest_briarpatch("quest_briarpatch_horde") { }

    void OnQuestStatusChange(Player* player, Quest const* /*quest*/, QuestStatus /*oldStatus*/, QuestStatus newStatus) override
    {
        HandleQuestStatusChange(player, newStatus, QUEST_DOWN_WITH_THE_QUILBOAR_HORDE, QUEST_FORBIDDEN_QUILBOAR_NECROMANY_HORDE, SPELL_VALIDATED_QUEST_ACCEPT_BRIARPATCH_HORDE);
    }
};

// 298984 - Validated Quest Accept
class spell_validated_quest_accept_briarpatch_alliance : public SpellScript
{
    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        if (Player* player = GetCaster()->ToPlayer())
        {
            Creature* huxsworth = FindCreatureIgnorePhase(player, "huxworth_briarpatch", 10.0f);
            Creature* garrick = FindCreatureIgnorePhase(player, "garrick_briarpatch", 10.0f);
            if (!huxsworth || !garrick)
                return;

            Creature* huxsworthPersonal = huxsworth->SummonPersonalClone(huxsworth->GetPosition(), TEMPSUMMON_MANUAL_DESPAWN, 0s, 0, 0, player);
            Creature* garrickPersonal = garrick->SummonPersonalClone(garrick->GetPosition(), TEMPSUMMON_MANUAL_DESPAWN, 0s, 0, 0, player);
            if (!huxsworthPersonal || !garrickPersonal)
                return;

            if (npc_huxsworth_briarpatch_quest_private* huxworthAI = CAST_AI(npc_huxsworth_briarpatch_quest_private, huxsworthPersonal->AI()))
                huxworthAI->SetGarrickGUID(garrickPersonal->GetGUID());
        }
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_validated_quest_accept_briarpatch_alliance::HandleScript, EFFECT_0, SPELL_EFFECT_SEND_EVENT);
    }
};

// 325309 - Validated Quest Accept
class spell_validated_quest_accept_briarpatch_horde : public SpellScript
{
    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        if (Player* player = GetCaster()->ToPlayer())
        {
            Creature* dawntracker = FindCreatureIgnorePhase(player, "dawntracker_briarpatch", 10.0f);
            Creature* grimaxe = FindCreatureIgnorePhase(player, "grimaxe_briarpatch", 10.0f);
            if (!dawntracker || !grimaxe)
                return;

            Creature* dawntrackerPersonal = dawntracker->SummonPersonalClone(dawntracker->GetPosition(), TEMPSUMMON_MANUAL_DESPAWN, 0s, 0, 0, player);
            Creature* grimaxePersonal = grimaxe->SummonPersonalClone(grimaxe->GetPosition(), TEMPSUMMON_MANUAL_DESPAWN, 0s, 0, 0, player);
            if (!dawntrackerPersonal || !grimaxePersonal)
                return;

            if (npc_dawntracker_briarpatch_quest_private* dawntrackerAI = CAST_AI(npc_dawntracker_briarpatch_quest_private, dawntrackerPersonal->AI()))
                dawntrackerAI->SetGrimaxeGUID(grimaxePersonal->GetGUID());
        }
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_validated_quest_accept_briarpatch_horde::HandleScript, EFFECT_0, SPELL_EFFECT_SEND_EVENT);
    }
};

enum GeolordData
{
    CONVERSATION_GEOLORD_AGGRO  = 13712,

    CONVERSATION_ACTOR_GEOLORD  = 70670,
    CONVERSATION_ACTOR_LINDIE   = 71238,
    CONVERSATION_ACTOR_CORK     = 75976,

    EVENT_CAST_EARTH_BOLT       = 1,
    EVENT_CAST_UPHEAVAL         = 2,

    NPC_CORK_FIZZLEPOP          = 167008,
    NPC_LINDIE_SPRINGSTOCK      = 154301,
    NPC_INVIS_BUNNY_GEOLORD     = 155371,

    ACTION_FREE_PRISONER        = 1,

    SPELL_NECROTIC_RITUAL_DNT   = 305513,
    SPELL_EARTH_BOLT            = 270453,
    SPELL_UPHEAVAL              = 319273,
};

static constexpr Position PrisonerPosition = { 16.4271f, -2511.82f, 78.8215f, 5.66398f  };

// 151091 - Geolord Grek'og
struct npc_geolord_grekog : public ScriptedAI
{
    npc_geolord_grekog(Creature* creature) : ScriptedAI(creature) { }

    void JustAppeared() override
    {
        uint32 prisonerEntry = NPC_LINDIE_SPRINGSTOCK;

        if (WorldStateMgr::GetValue(WS_TEAM_IN_INSTANCE_HORDE, me->GetMap()) == 1)
            prisonerEntry = NPC_CORK_FIZZLEPOP;

        Creature* bunny = me->FindNearestCreatureWithOptions(25.0f, { .CreatureId = NPC_INVIS_BUNNY_GEOLORD, .IgnorePhases = true });
        if (!bunny)
            return;

        if (Creature* prisoner = bunny->SummonCreature(prisonerEntry, PrisonerPosition, TEMPSUMMON_MANUAL_DESPAWN))
            _prisonerGUID = prisoner->GetGUID();
    }

    void JustDied(Unit* /*killer*/) override
    {
        if (Creature* prisoner = ObjectAccessor::GetCreature(*me, _prisonerGUID))
            prisoner->AI()->DoAction(ACTION_FREE_PRISONER);
    }

    void JustEngagedWith(Unit* who) override
    {
        if (Player* player = who->ToPlayer())
        {
            Conversation::CreateConversation(CONVERSATION_GEOLORD_AGGRO, player, *player, player->GetGUID(), nullptr, true);

            _events.ScheduleEvent(EVENT_CAST_EARTH_BOLT, 3s, 5s);
            _events.ScheduleEvent(EVENT_CAST_UPHEAVAL, 10s, 15s);
        }
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_CAST_EARTH_BOLT:
                {
                    DoCastVictim(SPELL_EARTH_BOLT);
                    _events.ScheduleEvent(EVENT_CAST_EARTH_BOLT, 10s, 12s);
                    break;
                }
                case EVENT_CAST_UPHEAVAL:
                {
                    DoCastSelf(SPELL_UPHEAVAL);
                    _events.ScheduleEvent(EVENT_CAST_UPHEAVAL, 10s, 15s);
                    break;
                }
                default:
                    break;
            }
        }
    }
private:
    EventMap _events;
    ObjectGuid _prisonerGUID;
};

enum BriarpatchPrisonerData
{
    EVENT_RUN_TO_PLAINS = 1,

    SAY_GET_OUT_OF_HERE = 0
};

static constexpr Position BriarpatchPrisonerJumpToPosition = { 19.5174f, -2513.75f, 74.0545f };
static constexpr Position PrisonerBriarpatchDespawnPosition = { 51.005207f, -2485.644f, 78.15223f };

// 167008 - Cork Fizzlepop
// 154301 - Lindie Springstock
struct npc_briarpatch_prisoner : public ScriptedAI
{
    npc_briarpatch_prisoner(Creature* creature) : ScriptedAI(creature) { }

    void JustAppeared() override
    {
        me->SetDisableGravity(true);
        me->SetControlled(true, UNIT_STATE_ROOT);
        me->CastSpell(me, SPELL_NECROTIC_RITUAL_DNT);
    }

    void DoAction(int32 param) override
    {
        if (param == ACTION_FREE_PRISONER)
        {
            me->RemoveAllAuras();
            me->SetDisableGravity(false);
            me->SetControlled(false, UNIT_STATE_ROOT);
            me->GetMotionMaster()->MoveJump(EVENT_JUMP, BriarpatchPrisonerJumpToPosition, 8.0f);
            Talk(SAY_GET_OUT_OF_HERE);
            _events.ScheduleEvent(EVENT_RUN_TO_PLAINS, 4s);
        }
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_RUN_TO_PLAINS:
                    me->GetMotionMaster()->MovePoint(0, PrisonerBriarpatchDespawnPosition);
                    me->DespawnOrUnsummon(5s);
                    break;
                default:
                    break;
            }
        }
    }
private:
    EventMap _events;
};

enum OgreOverseerQuilboarText
{
    SAY_AGGRO = 0,
    SAY_DEATH = 1,
};

enum QuilboarWarriorGeomancerData
{
    EVENT_BRUTAL_STRIKE        = 1,
    EVENT_GEOMANCER_EARTH_BOLT = 1,

    SPELL_NECROTIC_BURST       = 313261,
    SPELL_QUILBOAR_SLEEP_DNT   = 313265,
    SPELL_BRUTAL_STRIKE        = 317383,
    SPELL_GEOMANCER_EARTH_BOLT = 321188
};

// 150237 - Quilboar Warrior
struct npc_quilboar_warrior : public ScriptedAI
{
    npc_quilboar_warrior(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _events.Reset();
    }

    void JustEngagedWith(Unit* who) override
    {
        me->RemoveAura(SPELL_QUILBOAR_SLEEP_DNT);

        if (roll_chance(33.33f))
            Talk(SAY_AGGRO, who);

        _events.ScheduleEvent(EVENT_BRUTAL_STRIKE, 3s, 5s);
    }

    void JustDied(Unit* killer) override
    {
        if (roll_chance(33.33f))
            Talk(SAY_DEATH, killer);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_BRUTAL_STRIKE:
                    DoCastVictim(SPELL_BRUTAL_STRIKE);
                    _events.ScheduleEvent(EVENT_BRUTAL_STRIKE, 8s, 12s);
                    break;
                default:
                    break;
            }
        }
    }

private:
    EventMap _events;
};

// 150238 - Quilboar Geomancer
struct npc_quilboar_geomancer : public ScriptedAI
{
    npc_quilboar_geomancer(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _events.Reset();
    }

    void JustEngagedWith(Unit* who) override
    {
        me->RemoveAura(SPELL_QUILBOAR_SLEEP_DNT);

        if (roll_chance(33.33f))
            Talk(SAY_AGGRO, who);

        _events.ScheduleEvent(EVENT_GEOMANCER_EARTH_BOLT, 3s, 5s);
    }

    void JustDied(Unit* killer) override
    {
        if (roll_chance(33.33f))
            Talk(SAY_DEATH, killer);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_GEOMANCER_EARTH_BOLT:
                    DoCastVictim(SPELL_GEOMANCER_EARTH_BOLT);
                    _events.ScheduleEvent(EVENT_GEOMANCER_EARTH_BOLT, 3s, 10s);
                    break;
                default:
                    break;
            }
        }
    }
private:
    EventMap _events;
};

enum ExilesReachOgreOverseerData
{
    EVENT_OVERSEER_BACKHAND                 = 1,
    EVENT_OVERSEER_EARTHSHATTER             = 2,

    SPELL_BACKHAND                          = 276991,
    SPELL_EARTHSHATTER                      = 319292
};

// 156676 - Ogre Overseer
struct npc_ogre_overseer : public ScriptedAI
{
    npc_ogre_overseer(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _events.Reset();
    }

    void JustEngagedWith(Unit* who) override
    {
        Talk(SAY_AGGRO, who);

        _events.ScheduleEvent(EVENT_OVERSEER_BACKHAND, 5s);
        _events.ScheduleEvent(EVENT_OVERSEER_EARTHSHATTER, 10s, 15s);
    }

    void JustDied(Unit* killer) override
    {
        Talk(SAY_DEATH, killer);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_OVERSEER_BACKHAND:
                    DoCastVictim(SPELL_BACKHAND);
                    _events.ScheduleEvent(EVENT_OVERSEER_BACKHAND, 15s, 20s);
                    break;
                case EVENT_OVERSEER_EARTHSHATTER:
                    DoCastAOE(SPELL_EARTHSHATTER);
                    _events.ScheduleEvent(EVENT_OVERSEER_EARTHSHATTER, 15s, 20s);
                    break;
                default:
                    break;
            }
        }
    }
private:
    EventMap _events;
};

enum BriarpathPlainsConversations
{
    CONVERSATION_DOWN_WITH_THE_QUILLBOAR_COMPLETE_ALLIANCE = 12076,
    CONVERSATION_DOWN_WITH_THE_QUILLBOAR_COMPLETE_HORDE    = 14514
};

struct at_briarpatch_to_plains : AreaTriggerAI
{
    at_briarpatch_to_plains(AreaTrigger* areatrigger) : AreaTriggerAI(areatrigger) { }

    // @TODO: remove when conversation cooldown is implemented
    void StartConversation(Player* player, uint32 conversationId)
    {
        std::vector<WorldObject*> objs;

        Trinity::ObjectEntryAndPrivateOwnerIfExistsCheck check(player->GetGUID(), conversationId);
        Trinity::ConversationListSearcher searcher(PhasingHandler::GetAlwaysVisiblePhaseShift(), objs, check);
        Cell::VisitGridObjects(player, searcher, 100.0f);

        if (objs.empty())
            Conversation::CreateConversation(conversationId, player, *player, player->GetGUID(), nullptr);
    }

    void OnUnitEnter(Unit* unit) override
    {
        Player* player = unit->ToPlayer();
        if (!player)
            return;

        if (player->GetTeam() == ALLIANCE)
        {
            if (player->GetQuestStatus(QUEST_DOWN_WITH_THE_QUILBOAR_ALLIANCE) != QUEST_STATUS_COMPLETE)
                return;

            StartConversation(player, CONVERSATION_DOWN_WITH_THE_QUILLBOAR_COMPLETE_ALLIANCE);
        }
        else
        {
            if (player->GetQuestStatus(QUEST_DOWN_WITH_THE_QUILBOAR_HORDE) != QUEST_STATUS_COMPLETE)
                return;

            StartConversation(player, CONVERSATION_DOWN_WITH_THE_QUILLBOAR_COMPLETE_HORDE);
        }
    }
};

class spell_quilboar_sleep_dnt : public AuraScript
{
    void ApplyEffect(AuraEffect const* /* aurEff */, AuraEffectHandleModes /*mode*/)
    {
        if (Creature* target = GetTarget()->ToCreature())
            target->SetReactState(REACT_PASSIVE);
    }

    void RemoveEffect(AuraEffect const* /* aurEff */, AuraEffectHandleModes /*mode*/)
    {
        if (Creature* target = GetTarget()->ToCreature())
        {
            target->SetReactState(REACT_AGGRESSIVE);
            target->SetStandState(UNIT_STAND_STATE_STAND);
        }
    }

    void Register() override
    {
        OnEffectRemove += AuraEffectRemoveFn(spell_quilboar_sleep_dnt::RemoveEffect, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        AfterEffectApply += AuraEffectApplyFn(spell_quilboar_sleep_dnt::ApplyEffect, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// **************************************************************
// * Scripting in this section occurs after reaching The Plains *
// **************************************************************

enum LindieSpringStockData
{
    CONVERSATION_RESIZE_COPTER_ALLIANCE = 12078,
    CONVERSATION_RESIZE_COPTER_HORDE    = 14516,

    CONVERSATION_ACTOR_GOBLIN_ALLIANCE  = 71345,
    CONVERSATION_ACTOR_GOBLIN_HORDE     = 76337,
    CONVERSATION_ACTOR_COPTER           = 71344,

    EVENT_CONVERSATION                  = 1,
    EVENT_RESIZE_COPTER_1               = 2,
    EVENT_RESIZE_COPTER_2               = 3,
    EVENT_RESIZE_COPTER_3               = 4,
    EVENT_RESIZE_COPTER_4               = 5,
    EVENT_RESIZE_COPTER_5               = 6,
    EVENT_RESIZE_COPTER_6               = 7,

    NPC_LINDIE_SPRINGSTOCK_PLAINS       = 149899,

    QUEST_THE_SCOUT_O_MATIC_5000        = 55193,
    QUEST_THE_CHOPPY_BOOSTER_MK5        = 59940,

    SPELL_RE_SIZING                     = 313269,
    SPELL_GROW_ONE                      = 129310,
    SPELL_GROW_TWO                      = 94214,
    SPELL_GROW_THREE                    = 111701
};

static constexpr Position MiniChopperJumpPosition = { 107.979f, -2414.13f, 95.6243f };

// 149899 - Lindie Springstock
// 167019 - Cork Fizzlepop
struct npc_gnome_goblin_plains_make_copter_private : public ScriptedAI
{
    npc_gnome_goblin_plains_make_copter_private(Creature* creature) : ScriptedAI(creature), _conversationId(0), _conversationActorId(0), _timer(0ms) { }

    void JustAppeared() override
    {
        if (me->GetEntry() == NPC_LINDIE_SPRINGSTOCK_PLAINS)
        {
            _conversationId = CONVERSATION_RESIZE_COPTER_ALLIANCE;
            _conversationActorId = CONVERSATION_ACTOR_GOBLIN_ALLIANCE;
            _timer = 1350ms;
        }
        else
        {
            _conversationId = CONVERSATION_RESIZE_COPTER_HORDE;
            _conversationActorId = CONVERSATION_ACTOR_GOBLIN_HORDE;
            _timer = 750ms;
        }

        _events.ScheduleEvent(EVENT_CONVERSATION, 1s);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_CONVERSATION:
                {
                    if (Player* player = ObjectAccessor::GetPlayer(*me, me->GetPrivateObjectOwner()))
                    {
                        Conversation* conversation = Conversation::CreateConversation(_conversationId, player, *player, player->GetGUID(), nullptr, false);
                        if (!conversation)
                            break;

                        conversation->AddActor(_conversationActorId, 0, me->GetGUID());
                        conversation->AddActor(CONVERSATION_ACTOR_COPTER, 1, _copterGUID);
                        conversation->Start();
                    }

                    if (Creature* copter = ObjectAccessor::GetCreature(*me, _copterGUID))
                        copter->GetMotionMaster()->MoveJump(EVENT_JUMP, MiniChopperJumpPosition, 7.0f, 6.99f);

                    _events.ScheduleEvent(EVENT_RESIZE_COPTER_1, 6s);
                    break;
                }
                case EVENT_RESIZE_COPTER_1:
                {
                    if (Creature* copter = ObjectAccessor::GetCreature(*me, _copterGUID))
                    {
                        me->SetFacingTo(0.488383f);
                        me->CastSpell(copter, SPELL_RE_SIZING);
                        copter->CastSpell(copter, SPELL_GROW_ONE);
                    }
                    _events.ScheduleEvent(EVENT_RESIZE_COPTER_2, 2s);
                    break;
                }
                case EVENT_RESIZE_COPTER_2:
                {
                    if (Creature* copter = ObjectAccessor::GetCreature(*me, _copterGUID))
                        copter->RemoveAura(SPELL_GROW_ONE);

                    _events.ScheduleEvent(EVENT_RESIZE_COPTER_3, _timer);
                    break;
                }
                case EVENT_RESIZE_COPTER_3:
                {
                    if (Creature* copter = ObjectAccessor::GetCreature(*me, _copterGUID))
                        copter->CastSpell(copter, SPELL_GROW_TWO);

                    _events.ScheduleEvent(EVENT_RESIZE_COPTER_4, _timer);
                    break;
                }
                case EVENT_RESIZE_COPTER_4:
                {
                    if (Creature* copter = ObjectAccessor::GetCreature(*me, _copterGUID))
                        copter->RemoveAura(SPELL_GROW_TWO);

                    _events.ScheduleEvent(EVENT_RESIZE_COPTER_5, _timer);
                    break;
                }
                case EVENT_RESIZE_COPTER_5:
                {
                    if (Creature* copter = ObjectAccessor::GetCreature(*me, _copterGUID))
                        copter->CastSpell(copter, SPELL_GROW_THREE);

                    me->CastStop(SPELL_RE_SIZING);
                    me->SetFacingTo(4.50382f);
                    me->DespawnOrUnsummon(27s);
                    _events.ScheduleEvent(EVENT_RESIZE_COPTER_6, _timer);
                    break;
                }
                case EVENT_RESIZE_COPTER_6:
                {
                    if (Creature* copter = ObjectAccessor::GetCreature(*me, _copterGUID))
                    {
                        copter->RemoveAura(SPELL_GROW_THREE);
                        copter->DespawnOrUnsummon(2s);
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }

public:
    void SetCopterGUID(ObjectGuid copterGUID)
    {
        _copterGUID = copterGUID;
    }

private:
    EventMap _events;
    ObjectGuid _copterGUID;
    uint32 _conversationId;
    uint32 _conversationActorId;
    Milliseconds _timer;
};

CreatureAI* LindieSpringstockSelector(Creature* creature)
{
    if (creature->IsPrivateObject())
    {
        if (Player* privateObjectOwner = ObjectAccessor::GetPlayer(*creature, creature->GetPrivateObjectOwner()))
        {
            if (privateObjectOwner->GetQuestStatus(QUEST_THE_SCOUT_O_MATIC_5000) == QUEST_STATUS_INCOMPLETE)
                return new npc_gnome_goblin_plains_make_copter_private(creature);
        }
    }
    return new NullCreatureAI(creature);
}

CreatureAI* CorkFizzlepopSelector(Creature* creature)
{
    if (creature->IsPrivateObject())
    {
        if (Player* privateObjectOwner = ObjectAccessor::GetPlayer(*creature, creature->GetPrivateObjectOwner()))
        {
            if (privateObjectOwner->GetQuestStatus(QUEST_THE_CHOPPY_BOOSTER_MK5) == QUEST_STATUS_INCOMPLETE)
                return new npc_gnome_goblin_plains_make_copter_private(creature);
        }
    }
    return new NullCreatureAI(creature);
}

enum CopterRideData
{
    CONVERSATION_RIDE_TO_OGRE_RUINS_ALLIANCE   = 12083,
    CONVERSATION_RIDE_FROM_OGRE_RUINS_ALLIANCE = 12084,
    CONVERSATION_RIDE_TO_OGRE_RUINS_HORDE      = 14517,
    CONVERSATION_RIDE_FROM_OGRE_RUINS_HORDE    = 14520,

    EVENT_START_SCOUT_OGRE_RUINS               = 1,
    EVENT_RETURN_FROM_OGRE_RUINS               = 2,
    EVENT_TRIGGER_CREW_MOVE                    = 3,

    NPC_SCOUT_O_MATIC_5000                     = 156518,
    NPC_CHOPPY_BOOSTER_MK5                     = 167027,

    PATH_COPTER_TO_RUINS                       = 15652600,
    PATH_COPTER_FROM_RUINS                     = 15652601,
    PATH_WONSA_PLAINS                          = 16790900,
    PATH_BO_PLAINS                             = 16791000,
    PATH_LANA_PLAINS                           = 16791100,
    PATH_JINHAKE_PLAINS                        = 16791200,
    PATH_THROG_PLAINS                          = 16791300,

    SPELL_SCENE_OGRE_RUINS_ALLIANCE            = 321342,
    SPELL_SCENE_OGRE_RUINS_HORDE               = 326626,
    SPELL_ROPED_DNT                            = 303067,
    SPELL_SCOUT_O_MATIC_PING_DNT               = 321340,
    SCOUT_O_MATIC_DESUMMON                     = 305548
};

// 156526 - Scout-o-Matic 5000
struct npc_scoutomatic_5000 : public ScriptedAI
{
    npc_scoutomatic_5000(Creature* creature) : ScriptedAI(creature) { }

    void IsSummonedBy(WorldObject* summoner) override
    {
        if (Player* player = summoner->ToPlayer())
            player->CastSpell(player, SPELL_UPDATE_PHASE_SHIFT);
    }

    void PassengerBoarded(Unit* passenger, int8 /*seatId*/, bool apply) override
    {
        if (apply && passenger->IsPlayer())
        {
            me->CastSpell(passenger, SPELL_ROPED_DNT);
            _events.ScheduleEvent(EVENT_START_SCOUT_OGRE_RUINS, 2s);
        }
    }

    void WaypointPathEnded(uint32 /*nodeId*/, uint32 pathId) override
    {
        Player* player = ObjectAccessor::GetPlayer(*me, me->GetOwnerGUID());
        if (!player)
            return;

        if (pathId == PATH_COPTER_TO_RUINS)
        {
            player->CastSpell(player, SPELL_SCENE_OGRE_RUINS_ALLIANCE);
        }
        else
        {
            player->CastSpell(player, SCOUT_O_MATIC_DESUMMON, CastSpellExtraArgs(TRIGGERED_IGNORE_CASTER_MOUNTED_OR_ON_VEHICLE));
            player->CastSpell(player, SPELL_UPDATE_PHASE_SHIFT);
        }
    }

    void SpellHit(WorldObject* caster, SpellInfo const* spellInfo) override
    {
        if (spellInfo->Id == SPELL_SCOUT_O_MATIC_PING_DNT)
        {
            if (Player* player = caster->ToPlayer())
                player->KilledMonsterCredit(NPC_SCOUT_O_MATIC_5000);

            _events.ScheduleEvent(EVENT_RETURN_FROM_OGRE_RUINS, 1s);
        }
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_START_SCOUT_OGRE_RUINS:
                {
                    if (Player* player = ObjectAccessor::GetPlayer(*me, me->GetOwnerGUID()))
                    {
                        me->SetSpeed(MOVE_WALK, 5.5f);
                        me->GetMotionMaster()->MovePath(PATH_COPTER_TO_RUINS, false);
                        Conversation::CreateConversation(CONVERSATION_RIDE_TO_OGRE_RUINS_ALLIANCE, player, *player, player->GetGUID(), nullptr);
                    }
                    break;
                }
                case EVENT_RETURN_FROM_OGRE_RUINS:
                {
                    if (Player* player = ObjectAccessor::GetPlayer(*me, me->GetOwnerGUID()))
                    {
                        me->SetSpeed(MOVE_WALK, 5.0f);
                        me->GetMotionMaster()->MovePath(PATH_COPTER_FROM_RUINS, false);
                        Conversation::CreateConversation(CONVERSATION_RIDE_FROM_OGRE_RUINS_ALLIANCE, player, *player, player->GetGUID(), nullptr);
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }

private:
    EventMap _events;
};

static constexpr Position HordeCrewPersonalSpawnLocation[] =
{
    { 50.920593f, -2477.466f, 79.444374f, 0.8753076f },
    { 60.51328f, -2476.4822f, 81.034775f, 0.9128374f },
    { 53.08128f, -2476.5154f, 79.822624f, 0.9394135f },
    { 53.281864f, -2480.052f, 79.18003f, 0.78539818f },
    { 53.936935f, -2475.588f, 80.24179f, 0.91009599f }
};

// 167905 - Choppy Booster Mk. 5
struct npc_choppy_booster_scout : public ScriptedAI
{
    npc_choppy_booster_scout(Creature* creature) : ScriptedAI(creature) { }

    void IsSummonedBy(WorldObject* summoner) override
    {
        if (Player* player = summoner->ToPlayer())
            player->CastSpell(player, SPELL_UPDATE_PHASE_SHIFT);
    }

    void PassengerBoarded(Unit* passenger, int8 /*seatId*/, bool apply) override
    {
        if (apply && passenger->IsPlayer())
        {
            me->CastSpell(passenger, SPELL_ROPED_DNT);
            _events.ScheduleEvent(EVENT_START_SCOUT_OGRE_RUINS, 2s);
        }
    }

    void WaypointPathEnded(uint32 /*nodeId*/, uint32 pathId) override
    {
        Player* player = ObjectAccessor::GetPlayer(*me, me->GetOwnerGUID());
        if (!player)
            return;

        if (pathId == PATH_COPTER_TO_RUINS)
        {
            player->CastSpell(player, SPELL_SCENE_OGRE_RUINS_HORDE);
        }
        else
        {
            player->CastSpell(player, SCOUT_O_MATIC_DESUMMON, CastSpellExtraArgs(TRIGGERED_IGNORE_CASTER_MOUNTED_OR_ON_VEHICLE));
            player->CastSpell(player, SPELL_UPDATE_PHASE_SHIFT);
        }
    }

    void SpellHit(WorldObject* caster, SpellInfo const* spellInfo) override
    {
        if (spellInfo->Id == SPELL_SCOUT_O_MATIC_PING_DNT)
        {
            if (Player* player = caster->ToPlayer())
                player->KilledMonsterCredit(NPC_CHOPPY_BOOSTER_MK5);

            _events.ScheduleEvent(EVENT_RETURN_FROM_OGRE_RUINS, 1s);
        }
        return;
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_START_SCOUT_OGRE_RUINS:
                {
                    if (Player* player = ObjectAccessor::GetPlayer(*me, me->GetOwnerGUID()))
                    {
                        me->SetSpeed(MOVE_WALK, 6.0f);
                        me->GetMotionMaster()->MovePath(PATH_COPTER_TO_RUINS, false);
                        Conversation::CreateConversation(CONVERSATION_RIDE_TO_OGRE_RUINS_HORDE, player, *player, player->GetGUID(), nullptr);
                    }
                    break;
                }
                case EVENT_RETURN_FROM_OGRE_RUINS:
                {
                    if (Player* player = ObjectAccessor::GetPlayer(*me, me->GetOwnerGUID()))
                    {
                        me->SetSpeed(MOVE_WALK, 4.5f);
                        me->GetMotionMaster()->MovePath(PATH_COPTER_FROM_RUINS, false);
                        Conversation::CreateConversation(CONVERSATION_RIDE_FROM_OGRE_RUINS_HORDE, player, *player, player->GetGUID(), nullptr);
                        _events.ScheduleEvent(EVENT_TRIGGER_CREW_MOVE, 11s);
                    }
                    break;
                }
                case EVENT_TRIGGER_CREW_MOVE:
                    if (Player* player = ObjectAccessor::GetPlayer(*me, me->GetOwnerGUID()))
                    {
                        if (Creature* wonsa = FindCreatureIgnorePhase(player, "wonsa_darkmaul_plains"))
                            if (Creature* wonsaClone = wonsa->SummonPersonalClone(HordeCrewPersonalSpawnLocation[0], TEMPSUMMON_MANUAL_DESPAWN, 0s, 0, 0, player))
                                wonsaClone->GetMotionMaster()->MovePath(PATH_WONSA_PLAINS, false);

                        if (Creature* bo = FindCreatureIgnorePhase(player, "bo_darkmaul_plains"))
                            if (Creature* boClone = bo->SummonPersonalClone(HordeCrewPersonalSpawnLocation[1], TEMPSUMMON_MANUAL_DESPAWN, 0s, 0, 0, player))
                                boClone->GetMotionMaster()->MovePath(PATH_BO_PLAINS, false);

                        if (Creature* lana = FindCreatureIgnorePhase(player, "lana_darkmaul_plains"))
                            if (Creature* lanaClone = lana->SummonPersonalClone(HordeCrewPersonalSpawnLocation[2], TEMPSUMMON_MANUAL_DESPAWN, 0s, 0, 0, player))
                                lanaClone->GetMotionMaster()->MovePath(PATH_LANA_PLAINS, false);

                        if (Creature* jinhake = FindCreatureIgnorePhase(player, "jinhake_darkmaul_plains"))
                            if (Creature* jinhakeClone = jinhake->SummonPersonalClone(HordeCrewPersonalSpawnLocation[3], TEMPSUMMON_MANUAL_DESPAWN, 0s, 0, 0, player))
                                jinhakeClone->GetMotionMaster()->MovePath(PATH_JINHAKE_PLAINS, false);

                        if (Creature* throg = FindCreatureIgnorePhase(player, "throg_darkmaul_plains"))
                            if (Creature* throgClone = throg->SummonPersonalClone(HordeCrewPersonalSpawnLocation[4], TEMPSUMMON_MANUAL_DESPAWN, 0s, 0, 0, player))
                                throgClone->GetMotionMaster()->MovePath(PATH_THROG_PLAINS, false);
                    }
                    break;
                default:
                    break;
            }
        }
    }

private:
    EventMap _events;
};

CreatureAI* ChoppyBoosterSelector(Creature* creature)
{
    if (Player* player = ObjectAccessor::GetPlayer(*creature, creature->GetOwnerGUID()))
    {
        if (player->GetQuestStatus(QUEST_THE_CHOPPY_BOOSTER_MK5) == QUEST_STATUS_INCOMPLETE)
            return new npc_choppy_booster_scout(creature);
    }
    return new NullCreatureAI(creature);
}

// 167909 - Won'sa
// 167910 - Bo
// 167911 - Lana Jordan
// 167912 - Provisoner Jin'hake
// 167913 - Grunt Throg
struct npc_horde_crew_plains_private : public ScriptedAI
{
    npc_horde_crew_plains_private(Creature* creature) : ScriptedAI(creature) { }

    void WaypointPathEnded(uint32 /*nodeId*/, uint32 /*pathId*/) override
    {
        me->DespawnOrUnsummon();
    }
};

CreatureAI* HordeCrewPlainsSelector(Creature* creature)
{
    if (creature->IsPrivateObject())
        return new npc_horde_crew_plains_private(creature);

    return new NullCreatureAI(creature);
}

static constexpr Position CopterCloneSpawnPosition = { 100.583f, -2417.87f, 90.268f, 0.0f };

// Quest 55193 - The Scout-o-Matic 5000 "Alliance"
// Quest 59940 - The Choppy Booster Mk. 5 "Horde"
class quest_scout_chopper : public QuestScript
{
public:
    quest_scout_chopper(char const* script) : QuestScript(script) { }

    void HandleQuestStatusChange(Player* player, QuestStatus newStatus, std::string_view goblinString, std::string_view copterString)
    {
        switch (newStatus)
        {
            case QUEST_STATUS_INCOMPLETE:
            {
                Creature* goblin = FindCreatureIgnorePhase(player, goblinString, 10.0f);
                Creature* copter = FindCreatureIgnorePhase(player, copterString, 10.0f);
                if (!goblin || !copter)
                    return;

                Creature* goblinPersonal = goblin->SummonPersonalClone(goblin->GetPosition(), TEMPSUMMON_MANUAL_DESPAWN, 0s, 0, 0, player);
                Creature* copterPersonal = copter->SummonPersonalClone(CopterCloneSpawnPosition, TEMPSUMMON_MANUAL_DESPAWN, 0s, 0, 0, player);
                if (!goblinPersonal || !copterPersonal)
                    return;

                copterPersonal->SetObjectScale(0.2f);
                copterPersonal->RemoveNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);

                if (npc_gnome_goblin_plains_make_copter_private* personalAI = CAST_AI(npc_gnome_goblin_plains_make_copter_private, goblinPersonal->AI()))
                    personalAI->SetCopterGUID(copterPersonal->GetGUID());

                player->CastSpell(player, SPELL_UPDATE_PHASE_SHIFT);
                break;
            }
            case QUEST_STATUS_NONE:
                player->CastSpell(player, SPELL_UPDATE_PHASE_SHIFT);
                break;
            default:
                break;
        }
    }
};

// 55193 - The Scout-o-Matic 5000 "Alliance"
class quest_scout_o_matic_5000 : public quest_scout_chopper
{
public:
    quest_scout_o_matic_5000() : quest_scout_chopper("quest_scout_o_matic_5000") { }

    void OnQuestStatusChange(Player* player, Quest const* /*quest*/, QuestStatus /*oldStatus*/, QuestStatus newStatus) override
    {
        HandleQuestStatusChange(player, newStatus, "lindie_springstock_plains", "scout_o_matic");
    }
};

// 59940 - The Choppy Booster Mk. 5 "Horde"
class quest_choppy_booster_mk5 : public quest_scout_chopper
{
public:
    quest_choppy_booster_mk5() : quest_scout_chopper("quest_choppy_booster_mk5") { }

    void OnQuestStatusChange(Player* player, Quest const* /*quest*/, QuestStatus /*oldStatus*/, QuestStatus newStatus) override
    {
        HandleQuestStatusChange(player, newStatus, "cork_fizzlepop_plains", "choppy_booster");
    }
};

// Quest 56034 - Re-sizing the Situation "Alliance"
// Quest 59941 - Re-sizing the Situation "Horde"
enum ResizingQuestData
{
    ACTOR_LINDIE_RESIZING_QUEST                     = 71366,
    ACTOR_CORK_RESIZING_QUEST                       = 76343,

    CONVERSATION_RESIZING_QUEST_ACCEPT              = 12086,
    CONVERSATION_RESIZING_REPORT_BACK               = 12089,

    EVENT_RESIZING_FOLLOW_PLAYER                    = 1,
    EVENT_RESIZING_RUN_HOME                         = 2,

    POINT_HOME_POSITION                             = 0,

    SPELL_SUMMON_LINDIE_SPRINGSTOCK_GUARDIAN_Q56034 = 305750,
    SPELL_SUMMON_CORK_FIZZLEPOP_GUARDIAN_Q59941     = 326634,
    SPELL_LINDIE_DESUMMON_Q56034                    = 305756,
    SPELL_PING_LINDIE_Q56034_Q59941                 = 305754,
};

Position ResizingGuardianPosition = { 100.56077f, -2418.0713f, 90.34765f };

// 156749 - Lindie Springstock
struct npc_lindie_springstock_q56034 : public ScriptedAI
{
    npc_lindie_springstock_q56034(Creature* creature) : ScriptedAI(creature) { }

    void JustAppeared() override
    {
        Unit* owner = me->GetOwner();

        Player* player = owner->ToPlayer();
        if (!player)
            return;

        Conversation* conversation = Conversation::CreateConversation(CONVERSATION_RESIZING_QUEST_ACCEPT, player, *player, player->GetGUID(), nullptr, false);
        if (!conversation)
            return;

        conversation->AddActor(0, 0, player->GetGUID());
        conversation->AddActor(ACTOR_LINDIE_RESIZING_QUEST, 1, me->GetGUID());
        conversation->AddActor(ACTOR_CORK_RESIZING_QUEST, 2, ObjectGuid::Empty);
        conversation->Start();

        _events.ScheduleEvent(EVENT_RESIZING_FOLLOW_PLAYER, 2s);
    }

    void MovementInform(uint32 uiType, uint32 uiId) override
    {
        if (uiType != POINT_MOTION_TYPE)
            return;

        if (uiId != 0)
            return;

        if (Unit* owner = me->GetOwner())
            owner->CastSpell(owner, SPELL_LINDIE_DESUMMON_Q56034);
    }

    void SpellHit(WorldObject* /*caster*/, SpellInfo const* spellInfo) override
    {
        if (spellInfo->Id != SPELL_PING_LINDIE_Q56034_Q59941)
            return;

        _events.ScheduleEvent(EVENT_RESIZING_RUN_HOME, 2s);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_RESIZING_FOLLOW_PLAYER:
                {
                    if (Unit* owner = me->GetOwner())
                        me->GetMotionMaster()->MoveFollow(owner, 0.0f, float(M_PI / 4.0f));
                    break;
                }
                case EVENT_RESIZING_RUN_HOME:
                {
                    me->GetMotionMaster()->MovePoint(POINT_HOME_POSITION, ResizingGuardianPosition);
                    break;
                }
                default:
                    break;
            }
        }
    }

private:
    EventMap _events;
};

// 167915 - Cork Fizzlepop
struct npc_cork_fizzlepop_q59941 : public ScriptedAI
{
    npc_cork_fizzlepop_q59941(Creature* creature) : ScriptedAI(creature) { }

    void JustAppeared() override
    {
        Unit* owner = me->GetOwner();
        if (!owner)
            return;

        Conversation* conversation = Conversation::CreateConversation(CONVERSATION_RESIZING_QUEST_ACCEPT, owner, *owner, owner->GetGUID(), nullptr, false);
        if (!conversation)
            return;

        conversation->AddActor(0, 0, owner->GetGUID());
        conversation->AddActor(ACTOR_LINDIE_RESIZING_QUEST, 1, ObjectGuid::Empty);
        conversation->AddActor(ACTOR_CORK_RESIZING_QUEST, 2, me->GetGUID());
        conversation->Start();

        _events.ScheduleEvent(EVENT_RESIZING_FOLLOW_PLAYER, 2s);
    }

    void MovementInform(uint32 uiType, uint32 uiId) override
    {
        if (uiType != POINT_MOTION_TYPE)
            return;

        if (uiId != 0)
            return;

        if (Unit* owner = me->GetOwner())
        {
            owner->CastSpell(owner, SPELL_UPDATE_PHASE_SHIFT);
            owner->RemoveAura(SPELL_SUMMON_CORK_FIZZLEPOP_GUARDIAN_Q59941);
        }
    }

    void SpellHit(WorldObject* /*caster*/, SpellInfo const* spellInfo) override
    {
        if (spellInfo->Id != SPELL_PING_LINDIE_Q56034_Q59941)
            return;

        Unit* owner = me->GetOwner();
        if (!owner)
            return;

        Conversation* conversation = Conversation::CreateConversation(CONVERSATION_RESIZING_REPORT_BACK, owner, *owner, owner->GetGUID(), nullptr, false);
        if (!conversation)
            return;

        conversation->AddActor(0, 0, owner->GetGUID());
        conversation->AddActor(ACTOR_LINDIE_RESIZING_QUEST, 1, ObjectGuid::Empty);
        conversation->AddActor(ACTOR_CORK_RESIZING_QUEST, 2, me->GetGUID());
        conversation->Start();

        _events.ScheduleEvent(EVENT_RESIZING_RUN_HOME, 2s);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_RESIZING_FOLLOW_PLAYER:
                {
                    if (Unit* owner = me->GetOwner())
                        me->GetMotionMaster()->MoveFollow(owner, 0.0f, float(M_PI / 4.0f));
                    break;
                }
                case EVENT_RESIZING_RUN_HOME:
                {
                    me->GetMotionMaster()->MovePoint(POINT_HOME_POSITION, ResizingGuardianPosition);
                    break;
                }
                default:
                    break;
            }
        }
    }

private:
    EventMap _events;
};

enum ReSizedBoarData
{
    EVENT_BOAR_GROW   = 1,
    EVENT_BOAR_MOVE   = 2,

    SPELL_GROW_Q56034 = 129310,

    SOUND_GROW_Q56034 = 157469
};

Position GiantBoarPosition = { 116.146f, -2430.48f, 90.508415f };

// 156736 - Wandering Boar
struct npc_re_sized_boar_q56034 : public ScriptedAI
{
    npc_re_sized_boar_q56034(Creature* creature) : ScriptedAI(creature) { }

    void JustAppeared() override
    {
        _events.ScheduleEvent(EVENT_BOAR_GROW, 1s);
    }

    void MovementInform(uint32 uiType, uint32 uiId) override
    {
        if (uiType != POINT_MOTION_TYPE)
            return;

        if (uiId != POINT_HOME_POSITION)
            return;

        me->SetFacingTo(0.785398f);

        if (Unit* summoner = me->GetDemonCreator())
            summoner->CastSpell(summoner, SPELL_UPDATE_PHASE_SHIFT);

        me->DespawnOrUnsummon();
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_BOAR_GROW:
                    me->CastSpell(me, SPELL_GROW_Q56034);
                    me->PlayDirectSound(SOUND_GROW_Q56034);
                    _events.ScheduleEvent(EVENT_BOAR_MOVE, 1s);
                    break;
                case EVENT_BOAR_MOVE:
                    me->SetWalk(false);
                    me->GetMotionMaster()->MovePoint(POINT_HOME_POSITION, GiantBoarPosition);
                    break;
                default:
                    break;
            }
        }
    }

private:
    EventMap _events;
};

// Spell 305749 - Summon Admiral Garrick Guardian
// Spell 326635 - Summon Cork (DNT)
class spell_summon_guardian_q56034_q59941 : public SpellScript
{
    // @TODO: drop after TARGET_UNK_142 impl

    void SelectTarget(WorldObject*& target)
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        Player* player = caster->ToPlayer();
        if (!player)
            return;

        Creature* survivor = FindCreatureIgnorePhase(player, player->GetTeam() == ALLIANCE ? "lindie_springstock_plains" : "cork_fizzlepop_plains", 5.0f);
        if (!survivor)
            return;

        target = survivor;
    }

    void Register() override
    {
        OnObjectTargetSelect += SpellObjectTargetSelectFn(spell_summon_guardian_q56034_q59941::SelectTarget, EFFECT_0, TARGET_DEST_NEARBY_ENTRY_OR_DB);
    }
};

enum ReSizingData
{
    NPC_WANDERING_BOAR                               = 156716,
    NPC_LINDIE_SPRINGSTOCK_GUARDIAN                  = 156749,
    NPC_CORK_FIZZLEPOP_GUARDIAN                      = 167915,

    QUEST_RE_SIZING_THE_SITUATION_ALLIANCE           = 56034,
    QUEST_RE_SIZING_THE_SITUATION_HORDE              = 59941,

    QUEST_OBJECTIVE_RE_SIZING_THE_SITUATION_ALLIANCE = 390101,
    QUEST_OBJECTIVE_RE_SIZING_THE_SITUATION_HORDE    = 397274,

    SPELL_RESIZER_HIT_ONE_Q56034_Q59941              = 305724,
    SPELL_RESIZER_HIT_TWO_Q56034_Q59941              = 305721,
    SPELL_RESIZER_HIT_THREE_Q56034                   = 305742,
    SPELL_RESIZING_Q59941                            = 325345,
    SPELL_RE_SIZER_OVERCHARGED_Q56034                = 325347
};

// 305716 - Re-Sizing
class spell_re_sizing_q56034 : public SpellScript
{
    SpellCastResult CheckCast()
    {
        if (!GetExplTargetUnit())
            return SPELL_FAILED_BAD_TARGETS;

        Creature* target = GetExplTargetUnit()->ToCreature();
        if (!target)
            return SPELL_FAILED_BAD_TARGETS;

        if (target->GetEntry() != NPC_WANDERING_BOAR)
            return SPELL_FAILED_BAD_TARGETS;

        return SPELL_CAST_OK;
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_re_sizing_q56034::CheckCast);
    }
};

// 305716 - Re-Sizing
class spell_re_sizing_aura_q56034 : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo(
        {
            SPELL_RESIZER_HIT_ONE_Q56034_Q59941,
            SPELL_RESIZER_HIT_TWO_Q56034_Q59941,
            SPELL_PING_LINDIE_Q56034_Q59941,
        });
    }

    void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Player* player = Object::ToPlayer(GetCaster());
        Creature* creature = Object::ToCreature(GetTarget());
        if (!player || !creature)
            return;

        switch (player->GetQuestObjectiveData(QUEST_RE_SIZING_THE_SITUATION_ALLIANCE, QUEST_OBJECTIVE_RE_SIZING_THE_SITUATION_ALLIANCE))
        {
            case 0:
                player->CastSpell(creature, SPELL_RESIZER_HIT_ONE_Q56034_Q59941, true);
                creature->DespawnOrUnsummon(2s);
                break;
            case 1:
                player->CastSpell(creature, SPELL_RESIZER_HIT_TWO_Q56034_Q59941, true);
                creature->DespawnOrUnsummon();
                break;
            case 2:
                player->CastSpell(creature, SPELL_RESIZER_HIT_THREE_Q56034, true);
                player->CastSpell(player, SPELL_PING_LINDIE_Q56034_Q59941, true);
                creature->DespawnOrUnsummon();
                break;
            default:
                break;
        }
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_re_sizing_aura_q56034::OnRemove, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 305724 - Resizer Hit
// 305721 - Resizer Hit
class spell_resizer_hit_one_two_q56034_q59941 : public SpellScript
{
    void HandleLaunch(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);
    }

    void HandleEffect(SpellEffIndex effIndex)
    {
        Player* player = Object::ToPlayer(GetCaster());
        if (!player)
            return;

        uint32 guardianID = player->GetTeam() == ALLIANCE ? NPC_LINDIE_SPRINGSTOCK_GUARDIAN : NPC_CORK_FIZZLEPOP_GUARDIAN;

        Creature* guardian = player->FindNearestCreatureWithOptions(10.0f, { .CreatureId = guardianID, .OwnerGuid = player->GetGUID() });
        if (!guardian)
            return;

        Conversation* conversation = Conversation::CreateConversation(GetSpellInfo()->GetEffect(effIndex).MiscValue, player, *player, player->GetGUID(), nullptr, false);
        if (!conversation)
            return;

        conversation->AddActor(0, 0, player->GetGUID());
        conversation->AddActor(ACTOR_LINDIE_RESIZING_QUEST, 1, player->GetTeam() == ALLIANCE ? guardian->GetGUID() : ObjectGuid::Empty);
        conversation->AddActor(ACTOR_CORK_RESIZING_QUEST, 2, player->GetTeam() == ALLIANCE ? ObjectGuid::Empty : guardian->GetGUID());
        conversation->Start();
    }

    void Register() override
    {
        OnEffectLaunchTarget += SpellEffectFn(spell_resizer_hit_one_two_q56034_q59941::HandleLaunch, EFFECT_3, SPELL_EFFECT_CREATE_PRIVATE_CONVERSATION);
        OnEffectHitTarget += SpellEffectFn(spell_resizer_hit_one_two_q56034_q59941::HandleEffect, EFFECT_3, SPELL_EFFECT_CREATE_PRIVATE_CONVERSATION);
    }
};

// 305742 - Resizer Hit
class spell_resizer_hit_three_q56034 : public SpellScript
{
    void HandleLaunch(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);
    }

    void HandleEffect(SpellEffIndex effIndex)
    {
        Player* player = Object::ToPlayer(GetCaster());
        if (!player)
            return;

        uint32 guardianID = player->GetTeam() == ALLIANCE ? NPC_LINDIE_SPRINGSTOCK_GUARDIAN : NPC_CORK_FIZZLEPOP_GUARDIAN;

        Creature* guardian = player->FindNearestCreatureWithOptions(10.0f, { .CreatureId = guardianID, .OwnerGuid = player->GetGUID() });
        if (!guardian)
            return;

        Conversation* conversation = Conversation::CreateConversation(GetSpellInfo()->GetEffect(effIndex).MiscValue, player, *player, player->GetGUID(), nullptr, false);
        if (!conversation)
            return;

        conversation->AddActor(0, 0, player->GetGUID());
        conversation->AddActor(ACTOR_LINDIE_RESIZING_QUEST, 1, player->GetTeam() == ALLIANCE ? guardian->GetGUID() : ObjectGuid::Empty);
        conversation->AddActor(ACTOR_CORK_RESIZING_QUEST, 2, player->GetTeam() == ALLIANCE ? ObjectGuid::Empty : guardian->GetGUID());
        conversation->Start();
    }

    void Register() override
    {
        OnEffectLaunchTarget += SpellEffectFn(spell_resizer_hit_three_q56034::HandleLaunch, EFFECT_4, SPELL_EFFECT_CREATE_PRIVATE_CONVERSATION);
        OnEffectHitTarget += SpellEffectFn(spell_resizer_hit_three_q56034::HandleEffect, EFFECT_4, SPELL_EFFECT_CREATE_PRIVATE_CONVERSATION);
    }
};

// 325346 - Re-Sizing
class spell_re_sizing_q59941 : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo(
        {
            SPELL_RESIZING_Q59941,
            SPELL_RE_SIZER_OVERCHARGED_Q56034,
            SPELL_PING_LINDIE_Q56034_Q59941,
        });
    }

    SpellCastResult CheckCast()
    {
        if (!GetExplTargetUnit())
            return SPELL_FAILED_BAD_TARGETS;

        Creature* target = GetExplTargetUnit()->ToCreature();
        if (!target)
            return SPELL_FAILED_BAD_TARGETS;

        if (target->GetEntry() != NPC_WANDERING_BOAR)
            return SPELL_FAILED_BAD_TARGETS;

        return SPELL_CAST_OK;
    }

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        Player* player = Object::ToPlayer(GetCaster());
        Creature* creature = Object::ToCreature(GetHitUnit());
        if (!player || !creature)
            return;

        switch (player->GetQuestObjectiveData(QUEST_RE_SIZING_THE_SITUATION_HORDE, QUEST_OBJECTIVE_RE_SIZING_THE_SITUATION_HORDE))
        {
            case 0:
            case 1:
                player->CastSpell(creature, SPELL_RESIZING_Q59941, true);
                break;
            case 2:
                player->CastSpell(creature, SPELL_RE_SIZER_OVERCHARGED_Q56034, true);
                player->CastSpell(player, SPELL_PING_LINDIE_Q56034_Q59941, true);
                creature->DespawnOrUnsummon(3s);
                break;
            default:
                break;
        }
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_re_sizing_q59941::CheckCast);
        OnEffectHitTarget += SpellEffectFn(spell_re_sizing_q59941::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

// 325345 - Re-Sizing
class spell_re_sizing_aura_q59941 : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo(
        {
            SPELL_RESIZER_HIT_ONE_Q56034_Q59941,
            SPELL_RESIZER_HIT_TWO_Q56034_Q59941
        });
    }

    void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Player* player = Object::ToPlayer(GetCaster());
        Creature* creature = Object::ToCreature(GetTarget());
        if (!player || !creature)
            return;

        switch (player->GetQuestObjectiveData(QUEST_RE_SIZING_THE_SITUATION_HORDE, QUEST_OBJECTIVE_RE_SIZING_THE_SITUATION_HORDE))
        {
            case 0:
                player->CastSpell(creature, SPELL_RESIZER_HIT_ONE_Q56034_Q59941, true);
                creature->DespawnOrUnsummon(2s);
                break;
            case 1:
                player->CastSpell(creature, SPELL_RESIZER_HIT_TWO_Q56034_Q59941, true);
                creature->DespawnOrUnsummon();
                break;
            default:
                break;
        }
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_re_sizing_aura_q59941::OnRemove, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 325368 - Re-sizer Slaughter (DNT)
class spell_re_sizer_slaughter : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo(
        {
            SPELL_UPDATE_PHASE_SHIFT
        });
    }

    void OnApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetTarget()->CastSpell(nullptr, SPELL_UPDATE_PHASE_SHIFT);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectRemoveFn(spell_re_sizer_slaughter::OnApply, EFFECT_0, SPELL_AURA_PLAY_SCENE, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(spell_re_sizer_slaughter::OnApply, EFFECT_0, SPELL_AURA_PLAY_SCENE, AURA_EFFECT_HANDLE_REAL);
    }
};

// 56034 - Re-sizing the Situation "Alliance"
// 59941 - Re-sizing the Situation "Horde"
class quest_resizing_the_situation : public QuestScript
{
public:
    quest_resizing_the_situation(char const* script) : QuestScript(script) { }

    void HandleQuestStatusChange(Player* player, QuestStatus newStatus, uint32 SummonSpell)
    {
        switch (newStatus)
        {
            case QUEST_STATUS_INCOMPLETE:
                player->CastSpell(player, SPELL_UPDATE_PHASE_SHIFT);
                player->CastSpell(player, SummonSpell);
                break;
            case QUEST_STATUS_NONE:
                player->RemoveAura(SummonSpell);
                player->CastSpell(player, SPELL_UPDATE_PHASE_SHIFT);
                break;
            default:
                break;
        }
    }
};

// 56034 - Re-sizing the Situation "Alliance"
class quest_resizing_the_situation_alliance : public quest_resizing_the_situation
{
public:
    quest_resizing_the_situation_alliance() : quest_resizing_the_situation("quest_resizing_the_situation_alliance") { }

    void OnQuestStatusChange(Player* player, Quest const* /*quest*/, QuestStatus /*oldStatus*/, QuestStatus newStatus) override
    {
        HandleQuestStatusChange(player, newStatus, SPELL_SUMMON_LINDIE_SPRINGSTOCK_GUARDIAN_Q56034);
    }
};

// 59941 - Re-sizing the Situation "Horde"
class quest_resizing_the_situation_horde : public quest_resizing_the_situation
{
public:
    quest_resizing_the_situation_horde() : quest_resizing_the_situation("quest_resizing_the_situation_horde") { }

    void OnQuestStatusChange(Player* player, Quest const* /*quest*/, QuestStatus /*oldStatus*/, QuestStatus newStatus) override
    {
        HandleQuestStatusChange(player, newStatus, SPELL_SUMMON_CORK_FIZZLEPOP_GUARDIAN_Q59941);
    }
};

enum QuestRideBoar
{
    NPC_ALLIANCE_CAPTAIN                            = 174955,
    NPC_HENRY_GARRICK_PRISONER                      = 156799,

    SPELL_SUMMON_DARKMAUL_PLAINS_QUESTGIVERS_SUMMON = 305779,
    SPELL_SUMMON_DARKMAUL_PLAINS_QUESTGIVERS_AURA   = 305776,
    SPELL_PING_GARRICK_TORGOK                       = 316982,
    SPELL_REUNION_DNT_ALLIANCE                      = 305893,
    SPELL_RITUAL_SCENE_OGRE_CITADEL_DNT             = 321693,
    SPELL_RITUAL_SCENE_HRUN_BEAM_DNT                = 321692,
    SPELL_RITUAL_SCENE_HARPY_BEAM_DNT               = 321691,
    SPELL_RITUAL_SCENE_MAIN_BEAM_DNT                = 321690
};

static constexpr Position ReDeatherAbandonTeleportPos = { 102.3f, -2422.5f, 90.1f, 0.764454185962677001f };

// 55879 - Ride of the Scientifically Enhanced Boar
class quest_ride_of_the_scientifically_enhanced_boar : public QuestScript
{
public:
    quest_ride_of_the_scientifically_enhanced_boar() : QuestScript("quest_ride_of_the_scientifically_enhanced_boar") { }

    void OnQuestStatusChange(Player* player, Quest const* /*quest*/, QuestStatus /*oldStatus*/, QuestStatus newStatus) override
    {
        switch (newStatus)
        {
            case QUEST_STATUS_INCOMPLETE:
                player->CastSpell(player, SPELL_UPDATE_PHASE_SHIFT);
                player->CastSpell(player, SPELL_SUMMON_DARKMAUL_PLAINS_QUESTGIVERS_SUMMON);
                break;
            case QUEST_STATUS_COMPLETE:
                player->CombatStop();
                player->CastSpell(player, SPELL_PING_GARRICK_TORGOK);
                break;
            case QUEST_STATUS_REWARDED:
                player->CastSpell(player, SPELL_REUNION_DNT_ALLIANCE);
                player->CastSpell(player, SPELL_RITUAL_SCENE_OGRE_CITADEL_DNT);
                player->CastSpell(player, SPELL_RITUAL_SCENE_HRUN_BEAM_DNT);
                player->CastSpell(player, SPELL_RITUAL_SCENE_HARPY_BEAM_DNT);
                player->CastSpell(player, SPELL_RITUAL_SCENE_MAIN_BEAM_DNT);
                player->CastSpell(player, SPELL_UPDATE_PHASE_SHIFT);
                break;
            case QUEST_STATUS_NONE:
                player->RemoveAura(SPELL_RITUAL_SCENE_OGRE_CITADEL_DNT);
                player->RemoveAura(SPELL_RITUAL_SCENE_HRUN_BEAM_DNT);
                player->RemoveAura(SPELL_RITUAL_SCENE_HARPY_BEAM_DNT);
                player->RemoveAura(SPELL_RITUAL_SCENE_MAIN_BEAM_DNT);
                player->RemoveAura(SPELL_SUMMON_DARKMAUL_PLAINS_QUESTGIVERS_AURA);
                player->CastSpell(player, SPELL_UPDATE_PHASE_SHIFT);
                player->NearTeleportTo(ReDeatherAbandonTeleportPos);
                break;
            default:
                break;
        }
    }
};

enum SceneOgreRuinsRideBoar
{
    QUEST_RIDE_ENHANCED_BOAR            = 55879,
    QUEST_RIDE_BOAR_OBJECTIVE_TWO       = 396499,
    QUEST_RIDE_BOAR_OBJECTIVE_TWO_MAX   = 8,

    SPELL_ENHANCED_BOAR_TRAMPLE         = 305557,
    SPELL_ENHANCED_BOAR_CHARGE          = 321627,
    SPELL_ENHANCED_BOAR_KILL_CREDIT     = 321668,
    SPELL_ENHANCED_BOAR_PING_VEHICLE    = 305559,
    SPELL_ENHANCED_BOAR_CHARGE_CONVO    = 305815,
    SPELL_ENHANCED_BOAR_KNOCKBACK       = 306356,
    SPELL_ENHANCED_BOAR_KNOCKBACK_HINT  = 306357
};

// Script scene for Ride of the Scientifically Enhanced Boar quest
class scene_darkmaul_plains_skeleton_army_alliance : public SceneScript
{
public:
    scene_darkmaul_plains_skeleton_army_alliance() : SceneScript("scene_darkmaul_plains_skeleton_army_alliance") { }

    void OnSceneTriggerEvent(Player* player, uint32 /*sceneInstanceID*/, SceneTemplate const* /*sceneTemplate*/, std::string const& triggerName) override
    {
        if (triggerName == "Trampling Time")
        {
            player->CastSpell(player, SPELL_ENHANCED_BOAR_TRAMPLE, true);
            if (Unit* boar = player->GetVehicleBase())
                boar->CastSpell(boar, SPELL_ENHANCED_BOAR_CHARGE, true);
        }
        else if (triggerName == "Big Kill Credit")
        {
            player->CastSpell(player, SPELL_ENHANCED_BOAR_KILL_CREDIT, true);

            if (player->GetQuestObjectiveData(QUEST_RIDE_ENHANCED_BOAR, QUEST_RIDE_BOAR_OBJECTIVE_TWO) == QUEST_RIDE_BOAR_OBJECTIVE_TWO_MAX)
                player->CastSpell(player, SPELL_ENHANCED_BOAR_PING_VEHICLE);
        }
        else if (triggerName == "Conversation")
        {
            player->CastSpell(player, SPELL_ENHANCED_BOAR_CHARGE_CONVO, true);
        }
        else if (triggerName == "Knockback")
        {
            player->CastSpell(player, SPELL_ENHANCED_BOAR_KNOCKBACK, true);
        }
        else if (triggerName == "Hint")
        {
            player->CastSpell(player, SPELL_ENHANCED_BOAR_KNOCKBACK_HINT, true);
        }
    }
};

// Spell 305779 - Summon Darkmaul Plains Questgivers (DNT)
class spell_summon_darkmaul_plains_questgivers_q55879 : public SpellScript
{
    // @TODO: drop after TARGET_UNK_142 impl

    void SelectTarget(WorldObject*& target)
    {
        Player* player = Object::ToPlayer(GetCaster());
        if (!player)
            return;

        Creature* survivor = FindCreatureIgnorePhase(player, "captain_garrick_plains", 5.0f);
        if (!survivor)
            return;

        target = survivor;
    }

    void Register() override
    {
        OnObjectTargetSelect += SpellObjectTargetSelectFn(spell_summon_darkmaul_plains_questgivers_q55879::SelectTarget, EFFECT_0, TARGET_DEST_NEARBY_ENTRY_OR_DB);
    }
};

enum SpellRidingGiantBoar
{
    SPELL_RIDING_GIANT_BOAR_305068 = 305068,
    SPELL_RIDING_GIANT_BOAR_321670 = 321670
};

// 173426 - Riding Giant Boar
class spell_riding_giant_boar_q55879 : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo(
        {
            SPELL_RIDING_GIANT_BOAR_305068,
            SPELL_RIDING_GIANT_BOAR_321670,
            SPELL_UPDATE_PHASE_SHIFT
        });
    }

    void OnAuraRemoveHandler(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Player* player = Object::ToPlayer(GetCaster());
        if (!player)
            return;

        player->RemoveAura(SPELL_RIDING_GIANT_BOAR_305068);
        player->RemoveAura(SPELL_RIDING_GIANT_BOAR_321670);
        player->CastSpell(player, SPELL_UPDATE_PHASE_SHIFT);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_riding_giant_boar_q55879::OnAuraRemoveHandler, EFFECT_0, SPELL_AURA_CONTROL_VEHICLE, AURA_EFFECT_HANDLE_REAL);
    }
};

enum SpellKnockbackHint
{
    ACTOR_ALLIANCE_CAPTAIN = 71372
};

// 305742 - Resizer Hit
class spell_knockback_hint_q56034 : public SpellScript
{
    void HandleLaunch(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);
    }

    void HandleEffect(SpellEffIndex effIndex)
    {
        Player* player = Object::ToPlayer(GetCaster());
        if (!player)
            return;

        Creature* garrick = player->FindNearestCreatureWithOptions(10.0f, { .CreatureId = NPC_ALLIANCE_CAPTAIN, .OwnerGuid = player->GetGUID() });
        if (!garrick)
            return;

        Conversation* conversation = Conversation::CreateConversation(GetSpellInfo()->GetEffect(effIndex).MiscValue, player, *player, player->GetGUID(), nullptr, false);
        if (!conversation)
            return;

        conversation->AddActor(ACTOR_ALLIANCE_CAPTAIN, 0, garrick->GetGUID());
        conversation->Start();
    }

    void Register() override
    {
        OnEffectLaunchTarget += SpellEffectFn(spell_knockback_hint_q56034::HandleLaunch, EFFECT_0, SPELL_EFFECT_CREATE_PRIVATE_CONVERSATION);
        OnEffectHitTarget += SpellEffectFn(spell_knockback_hint_q56034::HandleEffect, EFFECT_0, SPELL_EFFECT_CREATE_PRIVATE_CONVERSATION);
    }
};

enum CaptainGarrickGiantBoar
{
    ACTION_EXIT_BOAR                                        = 1,
    ACTION_START_RIDE_BOAR                                  = 2,

    ACTOR_HENRY_GARRICK_PRISONER                            = 78493,

    CONVERSATION_CAPTAIN_GARRICK_RIDE_BOAR_QUEST_ACCEPT     = 12090,

    CONVERSATION_CAPTAIN_GARRICK_RIDE_BOAR_QUEST_HENRY      = 15615,
    CONVERSATION_CAPTAIN_GARRICK_RIDE_BOAR_QUEST_EXIT       = 12092,

    EVENT_CAPTAIN_GARRICK_RIDE_BOAR                         = 1,
    EVENT_CAPTAIN_GARRICK_RIDE_BOAR_CHECK_OWNER             = 2,
    EVENT_CAPTAIN_GARRICK_RIDE_BOAR_TALK_TO_HENRY           = 3,
    EVENT_CAPTAIN_GARRICK_RIDE_BOAR_HENRY_DESPAWN           = 4,
    EVENT_CAPTAIN_GARRICK_RIDE_BOAR_EXIT_BOAR_CONVERSATION  = 5,
    EVENT_CAPTAIN_GARRICK_VERIFY_BOARD                      = 6,

    NPC_GIANT_BOAR                                          = 156267,

    PHASE_SEE_TORGOK                                        = 14663,

    POINT_HENRY_POSITION                                    = 0,

    SPELL_PING_GARRICK_TO_RIDE_BOAR                         = 316984,
    SPELL_RIDE_VEHICLE_CAPTIAN_BOAR                         = 63315
};

Position MoveToPrisonerPosition = { 232.16145f, -2292.5347f, 80.91198f };

// 174955 - Captain Garrick
struct npc_captain_garrick_q55879 : public ScriptedAI
{
    npc_captain_garrick_q55879(Creature* creature) : ScriptedAI(creature) { }

    void JustAppeared() override
    {
        Player* player = Object::ToPlayer(me->GetOwner());
        if (!player)
            return;

        Conversation* conversation = Conversation::CreateConversation(CONVERSATION_CAPTAIN_GARRICK_RIDE_BOAR_QUEST_ACCEPT, player, *player, player->GetGUID(), nullptr, false);
        if (!conversation)
            return;

        conversation->AddActor(0, 0, player->GetGUID());
        conversation->AddActor(ACTOR_ALLIANCE_CAPTAIN, 1, me->GetGUID());
        conversation->Start();

        me->SetReactState(REACT_PASSIVE);
        me->GetMotionMaster()->MoveFollow(player, 0.0f, float(M_PI / 4.0f));
    }

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo = nullptr*/) override
    {
        if (me->GetHealth() <= damage)
            damage = me->GetHealth() - 1;
    }

    void SpellHit(WorldObject* /*caster*/, SpellInfo const* spellInfo) override
    {
        switch (spellInfo->Id)
        {
            case SPELL_PING_GARRICK_TO_RIDE_BOAR:
            {
                StartRideBoar();
                break;
            }
            case SPELL_ENHANCED_BOAR_PING_VEHICLE:
            {
                PhasingHandler::AddPhase(me, PHASE_SEE_TORGOK, true);
                _events.ScheduleEvent(EVENT_CAPTAIN_GARRICK_RIDE_BOAR_EXIT_BOAR_CONVERSATION, 500ms);
                break;
            }
            case SPELL_PING_GARRICK_TORGOK:
            {
                Player* player = Object::ToPlayer(me->GetOwner());
                if (!player)
                    break;

                Creature* henry = FindCreatureIgnorePhase(me, "henry_garrick_ogre_ruins_prisoner");
                if (!henry)
                    break;

                Creature* henryPersonal = henry->SummonPersonalClone(henry->GetPosition(), TEMPSUMMON_MANUAL_DESPAWN, 0s, 0, 0, player);
                if (!henryPersonal)
                    break;

                me->SetReactState(REACT_AGGRESSIVE);
                _henryGUID = henryPersonal->GetGUID();
                _events.ScheduleEvent(EVENT_CAPTAIN_GARRICK_RIDE_BOAR_TALK_TO_HENRY, 1s);
                break;
            }
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
                case EVENT_CAPTAIN_GARRICK_RIDE_BOAR:
                {
                    Player* player = ObjectAccessor::GetPlayer(*me, _ownerPlayerGUID);
                    if (!player)
                        player = me->SelectNearestPlayer(60.0f);
                    if (!player)
                        break;

                    // 猪是玩家的私有对象，必须借玩家的视角查；加里克自己查会被 IgnoreNotOwnedPrivateObjects 过滤掉
                    Creature* boar = player->FindNearestCreatureWithOptions(30.0f, { .CreatureId = NPC_GIANT_BOAR, .OwnerGuid = player->GetGUID() });
                    if (!boar)
                        boar = player->FindNearestCreature(NPC_GIANT_BOAR, 30.0f);

                    TC_LOG_INFO("scripts", "Q55879 Garrick {} RIDE_BOAR: player={}, boar={}", me->GetGUID().ToString(),
                        player->GetName(), boar ? boar->GetGUID().ToString() : "<none>");

                    if (!boar)
                        break;

                    // 解定身必须做：野猪在玩家上车时被 ROOT，不解玩家没法控制方向
                    boar->SetControlled(false, UNIT_STATE_ROOT);

                    _boarGUID = boar->GetGUID();

                    // 63315 在客户端数据里效果不全（实测加里克上不去），直接走硬编码上车 46598 的 1 号座位
                    me->EnterVehicle(boar, 1);
                    _events.ScheduleEvent(EVENT_CAPTAIN_GARRICK_VERIFY_BOARD, 1500ms);
                    break;
                }
                case EVENT_CAPTAIN_GARRICK_VERIFY_BOARD:
                {
                    if (me->GetVehicle())
                    {
                        TC_LOG_INFO("scripts", "Q55879 Garrick {} boarded boar successfully", me->GetGUID().ToString());
                        break;
                    }

                    if (Creature* boar = ObjectAccessor::GetCreature(*me, _boarGUID))
                    {
                        TC_LOG_INFO("scripts", "Q55879 Garrick {} retry boarding boar {}", me->GetGUID().ToString(), _boarGUID.ToString());
                        me->EnterVehicle(boar, 1);
                    }
                    else
                        TC_LOG_INFO("scripts", "Q55879 Garrick {} retry failed: boar {} gone", me->GetGUID().ToString(), _boarGUID.ToString());
                    break;
                }
                case EVENT_CAPTAIN_GARRICK_RIDE_BOAR_TALK_TO_HENRY:
                {
                    Player* player = Object::ToPlayer(me->GetOwner());
                    if (!player)
                        break;

                    Conversation* conversation = Conversation::CreateConversation(CONVERSATION_CAPTAIN_GARRICK_RIDE_BOAR_QUEST_HENRY, player, *player, player->GetGUID(), nullptr, false);
                    if (!conversation)
                        break;

                    conversation->AddActor(ACTOR_ALLIANCE_CAPTAIN, 0, me->GetGUID());
                    conversation->AddActor(ACTOR_HENRY_GARRICK_PRISONER, 1, _henryGUID);
                    conversation->Start();

                    me->GetMotionMaster()->Clear();
                    me->GetMotionMaster()->MovePoint(POINT_HENRY_POSITION, MoveToPrisonerPosition);
                    _events.ScheduleEvent(EVENT_CAPTAIN_GARRICK_RIDE_BOAR_HENRY_DESPAWN, 18s);
                    break;
                }
                case EVENT_CAPTAIN_GARRICK_RIDE_BOAR_HENRY_DESPAWN:
                {
                    me->DespawnOrUnsummon();
                    if (Creature* henry = ObjectAccessor::GetCreature(*me, _henryGUID))
                        henry->DespawnOrUnsummon();

                    Player* player = Object::ToPlayer(me->GetOwner());
                    if (!player)
                        break;
                    player->CastSpell(player, SPELL_UPDATE_PHASE_SHIFT);
                    break;
                }
                case EVENT_CAPTAIN_GARRICK_RIDE_BOAR_EXIT_BOAR_CONVERSATION:
                {
                    Player* player = Object::ToPlayer(me->GetOwner());
                    if (!player)
                        break;

                    Conversation* conversation = Conversation::CreateConversation(CONVERSATION_CAPTAIN_GARRICK_RIDE_BOAR_QUEST_EXIT, player, *player, player->GetGUID(), nullptr, false);
                    if (!conversation)
                        break;

                    conversation->AddActor(0, 0, player->GetGUID());
                    conversation->AddActor(ACTOR_ALLIANCE_CAPTAIN, 1, me->GetGUID());
                    conversation->Start();
                    break;
                }
                default:
                    break;
            }
        }

        UpdateVictim();
    }

    void DoAction(int32 action) override
    {
        if (action == ACTION_START_RIDE_BOAR)
            StartRideBoar();
    }

private:
    // SPELL_PING_GARRICK_TO_RIDE_BOAR 在部分客户端数据里没有效果，载具那边会直接调 DoAction 兜底，
    // 这里用标记去重，保证两条路同时存在时只上猪一次
    void StartRideBoar()
    {
        if (_boarRideStarted)
            return;

        // 174955 不一定是召唤物（场景/手动生成时 OwnerGUID 是空的），
        // 之前这里直接 return 导致加里克永远不上猪。补一层“最近玩家”兜底。
        Player* player = me->GetCharmerOrOwnerPlayerOrPlayerItself();
        if (!player)
            player = me->SelectNearestPlayer(60.0f);

        TC_LOG_INFO("scripts", "Q55879 Garrick {} StartRideBoar: owner={}, player={}", me->GetGUID().ToString(),
            me->GetOwnerGUID().ToString(), player ? player->GetName() : "<none>");

        if (!player)
            return;

        _boarRideStarted = true;
        _ownerPlayerGUID = player->GetGUID();

        PhasingHandler::InheritPhaseShift(me, player);
        PhasingHandler::ResetPhaseShift(me);

        _events.ScheduleEvent(EVENT_CAPTAIN_GARRICK_RIDE_BOAR, 2s);
    }

    EventMap _events;
    ObjectGuid _henryGUID;
    ObjectGuid _boarGUID;
    ObjectGuid _ownerPlayerGUID;
    bool _boarRideStarted = false;
};

enum GiantBoar
{
    EVENT_GIANT_BOAR_SIZE_ONE           = 1,
    EVENT_GIANT_BOAR_SIZE_TWO           = 2,
    EVENT_GIANT_BOAR_SIZE_THREE         = 3,
    EVENT_GIANT_BOAR_SIZE_FOUR          = 4,
    EVENT_GIANT_BOAR_EJECT_PASSENGERS   = 5,
    EVENT_GIANT_BOAR_UNROOT             = 6,
    EVENT_GIANT_BOAR_BOARD_UNROOT       = 7,
    EVENT_GIANT_BOAR_RESTORE_CONTROL    = 8,

    SOUND_ENLARGE_BOAR                  = 157516,
    SOUND_SHRINK_BOAR                   = 157517,

    VEHICLE_BOAR_SEAT_ONE               = 1
};

// 156267 - Giant Boar
struct npc_giant_boar_vehicle_q55879 : public VehicleAI
{
    npc_giant_boar_vehicle_q55879(Creature* creature) : VehicleAI(creature), _endOfScene(false)
    {
        me->SetOrientation(0.844224f);
    }

    void JustAppeared() override
    {
        me->SetSpeed(MOVE_RUN, 14.0f);
    }

    void PassengerBoarded(Unit* passenger, int8 /*seatId*/, bool apply) override
    {
        if (apply && passenger->IsPlayer())
        {
            // 刚上猪的 2 秒先定身，等加里克爬上来（官方行为：加里克上车后自己会解定身）。
            // 这里额外挂一个 2 秒的兜底解定身，避免加里克没上来时野猪一直 ROOT —— 那样玩家
            // 方向键完全无效、只能点技能，退出载具后客户端也还停在载具状态。
            me->SetControlled(true, UNIT_STATE_ROOT);
            _events.ScheduleEvent(EVENT_GIANT_BOAR_BOARD_UNROOT, 2s);

            passenger->CastSpell(passenger, SPELL_PING_GARRICK_TO_RIDE_BOAR); // Ping Garrick to ride Boar (DNT)
            passenger->CastSpell(passenger, SPELL_UPDATE_PHASE_SHIFT);

            // 316984 在部分客户端数据里是空法术（没有命中就没有 SpellHit），这里直接通知加里克上猪，
            // 加里克那边用标记去重，所以两条路同时存在时也只会执行一次
            NotifyGarrickToRide(passenger->ToPlayer());
        }
        else if (apply && passenger->IsCreature())
        {
            passenger->ChangeSeat(VEHICLE_BOAR_SEAT_ONE);
        }
        else if (!apply && passenger->IsPlayer())
        {
            // 下车：把客户端控制权/视角/移动主体交还玩家本体。
            // 正常路径由 RemoveCharmedBy 完成，这里补一道，避免动作条卡在载具技能页面。
            _events.CancelEvent(EVENT_GIANT_BOAR_BOARD_UNROOT);
            me->SetControlled(false, UNIT_STATE_ROOT);

            Player* player = passenger->ToPlayer();
            // RemoveCharmedBy 里那句 SendRemoveControlBar 带 this != charmer->GetFirstControlled()
            // 的前置条件，载具场景下经常不成立（清 charm 前后指针关系不同），客户端就一直留着
            // 载具技能条。这里显式再发一次空的 PetSpells，客户端收到才会把动作条切回普通页。
            player->SetClientControl(player, true);
            player->SendRemoveControlBar();
            _events.ScheduleEvent(EVENT_GIANT_BOAR_RESTORE_CONTROL, 1s);
        }
    }

    void SpellHit(WorldObject* /*caster*/, SpellInfo const* spellInfo) override
    {
        if (spellInfo->Id == SPELL_ENHANCED_BOAR_PING_VEHICLE)
        {
            me->HandleEmoteCommand(EMOTE_ONESHOT_CUSTOM_SPELL_01);
            me->SetControlled(true, UNIT_STATE_ROOT);
            me->CastSpell(me, SPELL_ENHANCED_BOAR_CHARGE);
            _endOfScene = true;
            _events.ScheduleEvent(EVENT_GIANT_BOAR_SIZE_ONE, 4s);

            if (Unit* owner = me->GetOwner())
                owner->CastSpell(owner, SPELL_UPDATE_PHASE_SHIFT);
        }
    }

    void UpdateAI(uint32 diff) override
    {
        // 注意：上猪/下车用的事件也要跑，所以这里不能再按 _endOfScene 提前 return
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_GIANT_BOAR_BOARD_UNROOT:
                    // 加里克没能在 2 秒内上车时也要解定身，否则玩家开不动这头猪
                    me->SetControlled(false, UNIT_STATE_ROOT);
                    break;
                case EVENT_GIANT_BOAR_RESTORE_CONTROL:
                    if (Unit* owner = me->GetOwner())
                        if (Player* player = owner->ToPlayer())
                            if (!player->GetVehicle())
                                player->SetClientControl(player, true);
                    break;
                case EVENT_GIANT_BOAR_SIZE_ONE:
                    me->PlayDirectSound(SOUND_ENLARGE_BOAR);
                    me->SetObjectScale(1.2f);
                    _events.ScheduleEvent(EVENT_GIANT_BOAR_SIZE_TWO, 1000ms);
                    break;
                case EVENT_GIANT_BOAR_SIZE_TWO:
                    me->PlayDirectSound(SOUND_SHRINK_BOAR);
                    me->SetObjectScale(0.7f);
                    _events.ScheduleEvent(EVENT_GIANT_BOAR_SIZE_THREE, 1000ms);
                    break;
                case EVENT_GIANT_BOAR_SIZE_THREE:
                    me->PlayDirectSound(SOUND_ENLARGE_BOAR);
                    me->SetObjectScale(1.2f);
                    _events.ScheduleEvent(EVENT_GIANT_BOAR_SIZE_FOUR, 500ms);
                    break;
                case EVENT_GIANT_BOAR_SIZE_FOUR:
                    me->PlayDirectSound(SOUND_SHRINK_BOAR);
                    me->SetObjectScale(0.1f);
                    _events.ScheduleEvent(EVENT_GIANT_BOAR_EJECT_PASSENGERS, 500ms);
                    break;
                case EVENT_GIANT_BOAR_EJECT_PASSENGERS:
                    me->RemoveAllAuras();
                    _events.ScheduleEvent(EVENT_GIANT_BOAR_UNROOT, 500ms);
                    break;
                case EVENT_GIANT_BOAR_UNROOT:
                    me->SetControlled(false, UNIT_STATE_ROOT);
                    break;
                default:
                    break;
            }
        }
    }
private:
    void NotifyGarrickToRide(Player* player)
    {
        if (!player)
            return;

        // OwnerGuid 读的是 creature 的 SummonedBy 字段；个人召唤物如果没设置 owner 就查不到，
        // 所以再退一步按条目 + 私有对象过滤查（默认 IgnoreNotOwnedPrivateObjects 会排除别人的猪）
        Creature* garrick = player->FindNearestCreatureWithOptions(60.0f,
            { .CreatureId = NPC_ALLIANCE_CAPTAIN, .OwnerGuid = player->GetGUID() });
        if (!garrick)
            garrick = player->FindNearestCreature(NPC_ALLIANCE_CAPTAIN, 60.0f);

        TC_LOG_INFO("scripts", "Q55879 boar {} boarded by {}: garrick={}", me->GetGUID().ToString(),
            player->GetName(), garrick ? garrick->GetGUID().ToString() : "<not found in 60yd>");

        if (garrick)
            garrick->AI()->DoAction(ACTION_START_RIDE_BOAR);
    }

    EventMap _events;
    bool _endOfScene;
};

enum GiantBoarDisplayQ55879
{
    NPC_GIANT_BOAR_VEHICLE_Q55879   = 156267
};

constexpr float GIANT_BOAR_LOOKUP_RANGE = 40.0f;

// 156595 - Giant Boar（地上的巨型野猪，任务 55879 的点击目标）
// 模板里它只有 SPELLCLICK 旗标、没有载具（VehicleId = 0），所以点它本身坐不上去。
// 点一下：给它召唤一头属于该玩家的个人载具野猪 156267，并把玩家放进 0 号座位，
// 之后 156267 的 npc_giant_boar_vehicle_q55879 接管（定身、ping 加里克、缩放、冲锋）。
struct npc_giant_boar_display_q55879 : public ScriptedAI
{
    npc_giant_boar_display_q55879(Creature* creature) : ScriptedAI(creature) { }

    void OnSpellClick(Unit* clicker, bool /*spellClickHandled*/) override
    {
        Player* player = clicker->ToPlayer();
        if (!player || player->GetVehicleBase())
            return;

        // 与 conditions(type 47, quest 55879, INCOMPLETE) 保持一致：
        // OnSpellClick 不会被 conditions 拦住，这里自己挡一道，避免没接任务就上猪后卡在定身状态
        if (player->GetQuestStatus(QUEST_RIDE_ENHANCED_BOAR) != QUEST_STATUS_INCOMPLETE)
            return;

        Creature* boar = player->FindNearestCreatureWithOptions(GIANT_BOAR_LOOKUP_RANGE,
            { .CreatureId = NPC_GIANT_BOAR_VEHICLE_Q55879, .OwnerGuid = player->GetGUID() });

        if (!boar)
            boar = player->FindNearestCreature(NPC_GIANT_BOAR_VEHICLE_Q55879, GIANT_BOAR_LOOKUP_RANGE);

        if (!boar)
        {
            // privateObjectOwner = 玩家：只有本人看得到这头猪
            boar = player->SummonCreature(NPC_GIANT_BOAR_VEHICLE_Q55879, me->GetPosition(),
                TEMPSUMMON_MANUAL_DESPAWN, 0s, 0, 0, player->GetGUID());

            if (!boar)
                return;
        }

        // SummonCreature 传的是 properties = nullptr，TempSummon::InitStats 会在写 owner 之前就
        // return，导致这头猪的 SummonedBy（GetOwnerGUID）是空的 —— 加里克脚本里
        // FindNearestCreatureWithOptions(... .OwnerGuid = owner) 就永远找不到它，
        // 于是既不上猪、也不解定身，玩家坐在上面完全动不了。这里补上 owner。
        boar->SetOwnerGUID(player->GetGUID());

        // 客户端要靠载具信息（VehicleRecID）才会把 charm 的界面切成载具框 + 载具动作条，
        // 否则一律按“宠物”显示（宠物框 + 带攻击/跟随/停留的宠物条）。
        // 动态召唤的载具走 Creature::CreateFromProto -> CreateVehicleKit(..., loading = true)，
        // 这条路不发 SMSG_SET_VEHICLE_REC_ID，客户端只能从 create block 的 vehicle 位里拿。
        // SummonCreature 的 create block 要等本 tick 的 object update 才 flush，紧接着的
        // EnterVehicle 却会立刻发 SMSG_PET_SPELLS —— 客户端先按宠物建好界面就不会再刷新了。
        // 所以这里手动把 create block 推出去，再显式补一条 vehicle rec id，最后才进载具。
        boar->SendUpdateToPlayer(player);
        if (Vehicle* kit = boar->GetVehicleKit())
            boar->SendSetVehicleRecId(kit->GetVehicleInfo()->ID);

        player->EnterVehicle(boar, 0);
    }
};

enum TorgokData
{
    EVENT_CAST_SPIRIT_BOLT              = 1,
    EVENT_CAST_SOUL_GRASP               = 2,

    SPELL_SPIRIT_BOLT                   = 319294,
    SPELL_SOUL_GRASP                    = 319298
};

// 162817 - Torgok
struct npc_torgok_q55879 : public ScriptedAI
{
    npc_torgok_q55879(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _events.Reset();
    }

    void JustEngagedWith(Unit* who) override
    {
        Talk(SAY_AGGRO, who);

        _events.ScheduleEvent(EVENT_CAST_SPIRIT_BOLT, 4s);
        _events.ScheduleEvent(EVENT_CAST_SOUL_GRASP, 14s);
    }

    void JustDied(Unit* killer) override
    {
        Talk(SAY_DEATH, killer);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_CAST_SPIRIT_BOLT:
                    DoCastVictim(SPELL_SPIRIT_BOLT);
                    _events.ScheduleEvent(EVENT_CAST_SPIRIT_BOLT, 6s);
                    break;
                case EVENT_CAST_SOUL_GRASP:
                    DoCastAOE(SPELL_SOUL_GRASP);
                    _events.ScheduleEvent(EVENT_CAST_SOUL_GRASP, 14s);
                    break;
                default:
                    break;
            }
        }
    }
private:
    EventMap _events;
};

enum PrisonerQuest55879
{
    PATH_PRISONER_TO_GROUND = 80000230
};

// 156799 - Henry Garrick
// 167126 - Shuja Grimaxe
struct npc_prisoner_q55879_private : public ScriptedAI
{
    npc_prisoner_q55879_private(Creature* creature) : ScriptedAI(creature) { }

    void InitializeAI() override
    {
        me->RemoveNpcFlag(UNIT_NPC_FLAG_QUESTGIVER);
    }

    void JustAppeared() override
    {
        me->RemoveAllAuras();
        me->GetMotionMaster()->MovePath(PATH_PRISONER_TO_GROUND, false);
    }
};

CreatureAI* PrisonerQ55879Selector(Creature* creature)
{
    if (creature->IsPrivateObject())
        return new npc_prisoner_q55879_private(creature);
    else
        return new NullCreatureAI(creature);
}

enum TheReDeather
{
    CONVERSATION_ACCEPT_RE_DEATHER_QUEST    = 14525,
    CONVERSATION_RE_DEATHER_EXPLODE         = 14526,

    NPC_HORDE_WARRIOR                       = 167146,
    NPC_SHUJA_GRIMAXE_PRISONER              = 167126,

    QUEST_RE_DEATHER                        = 59942,

    OBJECTIVE_RE_DEATHER_CADAVERS_KILLED    = 397279,

    RE_DEATHER_CADAVERS_KILLED_MAX          = 8,

    SPELL_REUNION_DNT_HORDE                 = 326678,
    SPELL_RE_DEATHER_TEMP_OBJECTIVE_CHECK   = 325394,
    SPELL_RE_DEATHER_ROUGH_LANDING_DNT      = 325401,
    SPELL_RE_DEATHER_SUMMON_GRIMAXE         = 325429
};

// 59942 - The Re-Deather
class quest_the_re_deather : public QuestScript
{
public:
    quest_the_re_deather() : QuestScript("quest_the_re_deather") { }

    void OnQuestStatusChange(Player* player, Quest const* /*quest*/, QuestStatus /*oldStatus*/, QuestStatus newStatus) override
    {
        switch (newStatus)
        {
            case QUEST_STATUS_INCOMPLETE:
                player->CastSpell(player, SPELL_UPDATE_PHASE_SHIFT);
                Conversation::CreateConversation(CONVERSATION_ACCEPT_RE_DEATHER_QUEST, player, *player, player->GetGUID(), nullptr);
                break;
            case QUEST_STATUS_COMPLETE:
                player->CombatStop();
                player->CastSpell(player, SPELL_PING_GARRICK_TORGOK);
                break;
            case QUEST_STATUS_REWARDED:
                player->CastSpell(player, SPELL_REUNION_DNT_HORDE);
                player->CastSpell(player, SPELL_RITUAL_SCENE_OGRE_CITADEL_DNT);
                player->CastSpell(player, SPELL_RITUAL_SCENE_HRUN_BEAM_DNT);
                player->CastSpell(player, SPELL_RITUAL_SCENE_HARPY_BEAM_DNT);
                player->CastSpell(player, SPELL_RITUAL_SCENE_MAIN_BEAM_DNT);
                player->CastSpell(player, SPELL_UPDATE_PHASE_SHIFT);
                break;
            case QUEST_STATUS_NONE:
            {
                player->CastSpell(player, SPELL_RITUAL_SCENE_OGRE_CITADEL_DNT);
                player->RemoveAura(SPELL_RE_DEATHER_SUMMON_GRIMAXE);
                player->RemoveAura(SPELL_RITUAL_SCENE_HRUN_BEAM_DNT);
                player->RemoveAura(SPELL_RITUAL_SCENE_HARPY_BEAM_DNT);
                player->RemoveAura(SPELL_RITUAL_SCENE_MAIN_BEAM_DNT);
                player->RemoveAura(SPELL_RE_DEATHER_TEMP_OBJECTIVE_CHECK);
                player->CastSpell(player, SPELL_UPDATE_PHASE_SHIFT);
                player->NearTeleportTo(ReDeatherAbandonTeleportPos);
                break;
            }
            default:
                break;
        }
    }
};

// 2489
class scene_darkmaul_plains_skeleton_army_horde : public SceneScript
{
public:
    scene_darkmaul_plains_skeleton_army_horde() : SceneScript("scene_darkmaul_plains_skeleton_army_horde") { }

    void OnSceneTriggerEvent(Player* player, uint32 /*sceneInstanceID*/, SceneTemplate const* /*sceneTemplate*/, std::string const& triggerName) override
    {
        if (triggerName == "Big Kill Credit")
        {
            player->CastSpell(player, SPELL_ENHANCED_BOAR_KILL_CREDIT, true);

            if (player->GetQuestObjectiveData(QUEST_RE_DEATHER, OBJECTIVE_RE_DEATHER_CADAVERS_KILLED) == RE_DEATHER_CADAVERS_KILLED_MAX)
            {
                player->CastSpell(player, SPELL_ENHANCED_BOAR_PING_VEHICLE); // Ping Vehicle
                player->CastSpell(player, SPELL_RE_DEATHER_TEMP_OBJECTIVE_CHECK); // Temp Objective Check
                Conversation::CreateConversation(CONVERSATION_RE_DEATHER_EXPLODE, player, *player, player->GetGUID(), nullptr);
            }
        }
        else if (triggerName == "Conversation")
        {
            player->CastSpell(player, SPELL_ENHANCED_BOAR_CHARGE_CONVO, true); // Does nothing but it's blizzlike
        }
        else if (triggerName == "Teleport")
        {
            player->CastSpell(player, SPELL_RE_DEATHER_ROUGH_LANDING_DNT);
        }
    }

    void OnSceneComplete(Player* player, uint32 /*sceneInstanceID*/, SceneTemplate const* /*sceneTemplate*/) override
    {
        player->CastSpell(player, SPELL_UPDATE_PHASE_SHIFT);
    }
};

enum GrimaxeReDeather
{
    ACTOR_SHUJA_GRIMAXE_PRISONER                        = 76004,
    ACTOR_WARLORD_GRIMAXE_Q59942                        = 76357,
    ACTOR_SHUJA_GRIMAXE_FREED                           = 78501,

    CONVERSATION_WARLORD_GRIMAXE_SPAWN_Q59942           = 14527,
    CONVERSATION_WARLORD_GRIMAXE_QUEST_COMPLETE_Q59942  = 15618
};

// 167146 - Warlord Grimaxe
struct npc_warlord_grimaxe_q59942 : public ScriptedAI
{
    npc_warlord_grimaxe_q59942(Creature* creature) : ScriptedAI(creature)
    {
        me->SetUnitFlag(UNIT_FLAG_PLAYER_CONTROLLED);
    }

    void JustAppeared() override
    {
        Player* player = Object::ToPlayer(me->GetOwner());
        if (!player)
            return;

        Creature* shuja = FindCreatureIgnorePhase(me, "shuja_grimaxe_ogre_ruins_prisoner");
        if (!shuja)
            return;

        Conversation* conversation = Conversation::CreateConversation(CONVERSATION_WARLORD_GRIMAXE_SPAWN_Q59942, player, *player, player->GetGUID(), nullptr, false);
        if (!conversation)
            return;

        conversation->AddActor(ACTOR_SHUJA_GRIMAXE_PRISONER, 0, shuja->GetGUID());
        conversation->AddActor(ACTOR_WARLORD_GRIMAXE_Q59942, 1, me->GetGUID());
        conversation->Start();

        me->GetMotionMaster()->MoveFollow(player, 0.0f, float(M_PI / 4.0f));
    }

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo = nullptr*/) override
    {
        if (me->GetHealth() <= damage)
            damage = me->GetHealth() - 1;
    }

    void SpellHit(WorldObject* /*caster*/, SpellInfo const* spellInfo) override
    {
        switch (spellInfo->Id)
        {
            case SPELL_PING_GARRICK_TORGOK:
            {
                Player* player = Object::ToPlayer(me->GetOwner());
                if (!player)
                    break;

                Creature* shuja = FindCreatureIgnorePhase(me, "shuja_grimaxe_ogre_ruins_prisoner");
                if (!shuja)
                    break;

                Creature* shujaPersonal = shuja->SummonPersonalClone(shuja->GetPosition(), TEMPSUMMON_MANUAL_DESPAWN, 0s, 0, 0, player);
                if (!shujaPersonal)
                    break;

                me->SetReactState(REACT_AGGRESSIVE);
                _shujaGUID = shujaPersonal->GetGUID();
                _events.ScheduleEvent(EVENT_CAPTAIN_GARRICK_RIDE_BOAR_TALK_TO_HENRY, 1s);
                break;
            }
            default:
                break;
        }
    }

    void OnDespawn() override
    {
        if (Creature* shuja = ObjectAccessor::GetCreature(*me, _shujaGUID))
            shuja->DespawnOrUnsummon();
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_CAPTAIN_GARRICK_RIDE_BOAR_TALK_TO_HENRY:
                {
                    Player* player = Object::ToPlayer(me->GetOwner());
                    if (!player)
                        break;

                    Conversation* conversation = Conversation::CreateConversation(CONVERSATION_WARLORD_GRIMAXE_QUEST_COMPLETE_Q59942, player, *player, player->GetGUID(), nullptr, false);
                    if (!conversation)
                        break;

                    conversation->AddActor(ACTOR_WARLORD_GRIMAXE_Q59942, 0, me->GetGUID());
                    conversation->AddActor(ACTOR_SHUJA_GRIMAXE_FREED, 1, _shujaGUID);
                    conversation->Start();

                    me->GetMotionMaster()->Clear();
                    me->GetMotionMaster()->MovePoint(POINT_HENRY_POSITION, MoveToPrisonerPosition);
                    _events.ScheduleEvent(EVENT_CAPTAIN_GARRICK_RIDE_BOAR_HENRY_DESPAWN, 22s);
                    break;
                }
                case EVENT_CAPTAIN_GARRICK_RIDE_BOAR_HENRY_DESPAWN:
                {
                    me->DespawnOrUnsummon();
                    if (Creature* shuja = ObjectAccessor::GetCreature(*me, _shujaGUID))
                        shuja->DespawnOrUnsummon();

                    Player* player = Object::ToPlayer(me->GetOwner());
                    if (!player)
                        break;
                    player->CastSpell(player, SPELL_UPDATE_PHASE_SHIFT);
                    break;
                }
                default:
                    break;
            }
        }

        UpdateVictim();
    }
private:
    EventMap _events;
    ObjectGuid _shujaGUID;
};

// ***************************************************************
// * Scripting in this section occurs between Ogre Ruins and Pit *
// ***************************************************************

enum WestwardBound
{
    NPC_BJORN_STOUTHANDS_PIT_ONE    = 156891,
    NPC_ALARIA_PIT_ONE              = 156803,
    NPC_LANAJORDAN_PIT_ONE          = 167225,
    NPC_WONSA_PIT_ONE               = 167226
};

static constexpr Position BjornWestwardBoundRuinsPos = { 192.181f, -2311.44f, 80.6975f, 3.368485450744628906f };
static constexpr Position AlariaWestwardBoundRuinsPos = { 190.953f, -2308.32f, 80.6586f, 2.984513044357299804f };
static constexpr Position LanaWestwardBoundRuinsPos = { 160.486f, -2307.31f, 84.053f, 2.932153224945068359f };
static constexpr Position WonsaWestwardBoundRuinsPos = { 160.431f, -2310.11f, 84.4598f, 3.03687286376953125f };

// 55965 - Quest Westward Bound "Alliance"
// 59948 - Quest Westward Bound "Horde"
class quest_westward_bound : public QuestScript
{
public:
    quest_westward_bound(char const* script) : QuestScript(script) { }

    void HandleQuestStatusChange(Player* player, QuestStatus newStatus, std::string_view creatureStringOne, std::string_view creatureStringTwo, uint32 questEnderEntry, uint32 questEnderCompanionEntry, Position questGiverPos, Position companionPos)
    {
        switch (newStatus)
        {
            case QUEST_STATUS_INCOMPLETE:
            {
                Creature* questEnder = FindCreatureIgnorePhase(player, creatureStringOne, 125.0f);
                if (!questEnder)
                    return;

                Creature* questEnderCompanion = FindCreatureIgnorePhase(player, creatureStringTwo, 125.0f);
                if (!questEnderCompanion)
                    return;

                questEnder->SummonPersonalClone(questGiverPos, TEMPSUMMON_MANUAL_DESPAWN, 0s, 0, 0, player);
                questEnderCompanion->SummonPersonalClone(companionPos, TEMPSUMMON_MANUAL_DESPAWN, 0s, 0, 0, player);

                player->CastSpell(player, SPELL_UPDATE_PHASE_SHIFT);
                break;
            }
            case QUEST_STATUS_NONE:
            {
                player->CastSpell(player, SPELL_UPDATE_PHASE_SHIFT);

                if (Creature* questGiver = player->FindNearestCreatureWithOptions(100.0f, { .CreatureId = questEnderEntry, .IgnorePhases = true, .PrivateObjectOwnerGuid = player->GetGUID() }))
                    questGiver->DespawnOrUnsummon();

                if (Creature* companion = player->FindNearestCreatureWithOptions(100.0f, { .CreatureId = questEnderCompanionEntry, .IgnorePhases = true, .PrivateObjectOwnerGuid = player->GetGUID() }))
                    companion->DespawnOrUnsummon();

                break;
            }
            default:
                break;
        }
    }
};

// 55965 - Quest Westward Bound "Alliance"
class quest_westward_bound_alliance : public quest_westward_bound
{
public:
    quest_westward_bound_alliance() : quest_westward_bound("quest_westward_bound_alliance") { }

    void OnQuestStatusChange(Player* player, Quest const* /*quest*/, QuestStatus /*oldStatus*/, QuestStatus newStatus) override
    {
        HandleQuestStatusChange(player, newStatus, "bjorn_stouthands_pit_pre_quest", "alaria_pit_pre_quest", NPC_BJORN_STOUTHANDS_PIT_ONE, NPC_ALARIA_PIT_ONE, BjornWestwardBoundRuinsPos, AlariaWestwardBoundRuinsPos);
    }
};

// 59948 - Quest Westward Bound "Horde"
class quest_westward_bound_horde : public quest_westward_bound
{
public:
    quest_westward_bound_horde() : quest_westward_bound("quest_westward_bound_horde") { }

    void OnQuestStatusChange(Player* player, Quest const* /*quest*/, QuestStatus /*oldStatus*/, QuestStatus newStatus) override
    {
        HandleQuestStatusChange(player, newStatus, "lana_joran_pit_pre_quest", "wonsa_pit_pre_quest", NPC_LANAJORDAN_PIT_ONE, NPC_WONSA_PIT_ONE, LanaWestwardBoundRuinsPos, WonsaWestwardBoundRuinsPos);
    }
};

enum BjornRunToPit
{
    EVENT_BJORN_RUN_TO_PIT = 1,

    PATH_BJORN_RUN_TO_PIT  = 10518900,

    SAY_BJORN_RUN_TO_PIT   = 0,
    SAY_BJORN_REACHED_PIT  = 1
};

// 156891 - Bjorn Stouthands
struct npc_bjorn_stouthands_q55965_private : public ScriptedAI
{
    npc_bjorn_stouthands_q55965_private(Creature* creature) : ScriptedAI(creature) { }

    void InitializeAI() override
    {
        me->RemoveNpcFlag(UNIT_NPC_FLAG_QUESTGIVER);
    }

    void JustAppeared() override
    {
        _events.ScheduleEvent(EVENT_BJORN_RUN_TO_PIT, 1s);
    }

    void WaypointPathEnded(uint32 /*nodeId*/, uint32 /*pathId*/) override
    {
        Talk(SAY_BJORN_REACHED_PIT);
        me->DespawnOrUnsummon(4s);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_BJORN_RUN_TO_PIT:
                    Talk(SAY_BJORN_RUN_TO_PIT);
                    me->GetMotionMaster()->MovePath(PATH_BJORN_RUN_TO_PIT, false);
                    break;
                default:
                    break;
            }
        }
    }
private:
    EventMap _events;
};

CreatureAI* BjornRuinsSelector(Creature* creature)
{
    if (creature->IsPrivateObject())
        return new npc_bjorn_stouthands_q55965_private(creature);
    else
        return new NullCreatureAI(creature);
}

enum LanaRunToPit
{
    EVENT_LANA_SAY_AT_RUINS = 1,
    EVENT_LANA_RUN_TO_PIT   = 2,

    PATH_LANA_RUN_TO_PIT    = 80000570,

    SAY_LANA_RUN_TO_PIT     = 0,
    SAY_LANA_REACHED_PIT    = 1
};

// 167225 - Lana Jordan
struct npc_lana_jordan_q59948_private : public ScriptedAI
{
    npc_lana_jordan_q59948_private(Creature* creature) : ScriptedAI(creature) { }

    void InitializeAI() override
    {
        me->RemoveNpcFlag(UNIT_NPC_FLAG_QUESTGIVER);
    }

    void JustAppeared() override
    {
        _events.ScheduleEvent(EVENT_LANA_SAY_AT_RUINS, 1s);
    }

    void WaypointPathEnded(uint32 /*nodeId*/, uint32 /*pathId*/) override
    {
        Talk(SAY_LANA_REACHED_PIT);
        me->DespawnOrUnsummon(7s);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_LANA_SAY_AT_RUINS:
                    Talk(SAY_BJORN_RUN_TO_PIT);
                    _events.ScheduleEvent(EVENT_LANA_RUN_TO_PIT, 5s);
                    break;
                case EVENT_LANA_RUN_TO_PIT:
                    me->GetMotionMaster()->MovePath(PATH_LANA_RUN_TO_PIT, false);
                    break;
                default:
                    break;
            }
        }
    }
private:
    EventMap _events;
};

CreatureAI* LanaRuinsSelector(Creature* creature)
{
    if (creature->IsPrivateObject())
        return new npc_lana_jordan_q59948_private(creature);
    else
        return new NullCreatureAI(creature);
}

enum CompanionRunToPit
{
    EVENT_COMPANION_RUN_TO_PIT = 1,

    PATH_ALARIA_RUN_TO_PIT     = 10518890,
    PATH_WONSA_RUN_TO_PIT      = 80000580
};

// 156891 - Alaria
// 167226 - Won'sa
template<uint32 PitPathId>
struct npc_companion_q55965_q59948_private : public ScriptedAI
{
    npc_companion_q55965_q59948_private(Creature* creature) : ScriptedAI(creature) { }

    void JustAppeared() override
    {
        _events.ScheduleEvent(EVENT_COMPANION_RUN_TO_PIT, 1s);
    }

    void WaypointPathEnded(uint32 /*nodeId*/, uint32 /*pathId*/) override
    {
        me->DespawnOrUnsummon(1s);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_COMPANION_RUN_TO_PIT:
                    me->GetMotionMaster()->MovePath(PitPathId, false);
                    break;
                default:
                    break;
            }
        }
    }
private:
    EventMap _events;
};

CreatureAI* AlariaRuinsSelector(Creature* creature)
{
    if (creature->IsPrivateObject())
        return new npc_companion_q55965_q59948_private<PATH_ALARIA_RUN_TO_PIT>(creature);
    else
        return new NullCreatureAI(creature);
}

CreatureAI* WansaRuinsSelector(Creature* creature)
{
    if (creature->IsPrivateObject())
        return new npc_companion_q55965_q59948_private<PATH_WONSA_RUN_TO_PIT>(creature);
    else
        return new NullCreatureAI(creature);
}

// ***************************************************************
// * Hrun's Barrow - Who Lurks in the Pit (55639)                *
// ***************************************************************

enum WhoLurksInThePitQ55639
{
    QUEST_WHO_LURKS_IN_THE_PIT      = 55639,
    // quest_objectives.ID, Order 1: "Defeat Hrun to rescue Ralia Dreamchaser"
    OBJECTIVE_DEFEAT_HRUN           = 391940,

    NPC_RALIA_DREAMCHASER_Q55639    = 156929,
    NPC_HRUN_THE_EXILED_Q55639      = 156900,

    GOSSIP_ACTION_RIDE_RALIA        = 1,

    ACTION_HRUN_DIED                = 1,

    EVENT_RALIA_BOARD               = 1,
    EVENT_RALIA_FIRE                = 2,
    EVENT_RALIA_TIMEOUT             = 3,
    EVENT_RALIA_LINE_SPIDERS        = 4,
    EVENT_RALIA_LINE_CREATURES      = 5,
    EVENT_RALIA_FINISH              = 6,
    EVENT_RALIA_EJECT               = 7,
    EVENT_RALIA_HELP_CHECK          = 8,
    EVENT_RALIA_SHAKE               = 9,
    EVENT_RALIA_SHIFT               = 12,
    EVENT_RALIA_CLIMB_FALLBACK      = 13,
    EVENT_RALIA_ARM_RIDE            = 14,

    // Same spell the Seething Shore battleground casts on every player when the
    // azerite geysers erupt (battleground_seething_shore.cpp: "EarthQuakeCameraShake").
    // It exists on this client and is the only server-side way to shake the
    // camera: 12.x no longer carries a ScreenShake opcode and SpellVisualKit has
    // lost its shake field, so the shake has to come from a spell.
    SPELL_CAMERA_SHAKE              = 248719,

    // waypoint_path from the world database: 22 nodes running from the pit floor
    // (z=-30.5) up the gallery to the surface, 703 yards. Two were added on top
    // (see fix_ralia_path_end_wall.sql) so the last leg climbs the cliff face:
    //   node 22 (86.4, -2244.5, 87.2)      - foot of the wall, 6.6 yd below the lip
    //   node 23 (86.4, -2244.5, 93.7894)   - straight up the wall, onto the ledge
    // Node 23 IS the old end point, so Captain Garrick (spawn 8001001 at 87.0,
    // -2246.5, 94.48) is still two yards away and the ride still stops in his
    // face. The single node 21 -> 22 diagonal it replaced climbed 10.7 yd in one
    // leg and read as the stag sliding up through the rock.
    RALIA_ESCAPE_PATH               = 156929,
    RALIA_ESCAPE_LAST_NODE          = 23,

    // 91010 is the display every other Ralia Dreamchaser in the zone uses
    // (156902 / 156907 / 156932 / 156944 / 156947 / 164907 / 232800), i.e. the
    // night elf model the player actually sees at the camp: bounding radius
    // 0.306, combat reach 1.5 - a standard humanoid.
    //
    // 73350, which the spawn (guid 8001000) carried in creature_template_model,
    // is the shape she takes *after* the escape: radius 1.156, reach 0.7, and
    // on this client it renders as the stag. That is why she was a deer from
    // the moment she spawned, no matter what the script did.
    DISPLAY_RALIA_NIGHTELF          = 91010,
    DISPLAY_RALIA_STAG              = 73350,

    // Last resort if the client has no 91010: 37729 is what the other Ralia in
    // the pit (156930) uses, so the client definitely carries it.
    DISPLAY_RALIA_FALLBACK          = 37729,

    // The purple ritual that holds her over the altar. Retail draws it inside a
    // client scene; server-side the identical look comes from the aura the
    // briarpatch prisoners wear in their creature_template_addon - 305513
    // "Necrotic Ritual" (154301 Lindie Springstock, 153211 Meredy Huntswell,
    // 167008 Cork Fizzlepop). The rune on the floor, the glow and the beams are
    // all the client's own rendering of this one aura, nothing is spawned.
    SPELL_RALIA_NECROTIC_RITUAL     = 305513,

    GO_FLAME_EMITTER                = 1607,
    GO_FLAME_LIFETIME               = 8,

    // The two braziers flanking Hrun's altar already exist in the world; the
    // scene only lights them, so all that gets spawned here is the flame
    // emitter at each of the two prop anchors.
    GO_RITUAL_FIRE_LIFETIME         = 300,

    // Hrun's own ability from the dungeon journal: "Drain Spirit - Drains
    // health from an enemy over 5 sec, transferring it to the caster." That is
    // the purple leash that runs from Ralia to him for the whole scene.
    SPELL_HRUN_DRAIN_SPIRIT         = 319310,

    EVENT_HRUN_DRAIN_LOOKUP         = 10,
    EVENT_HRUN_DRAIN                = 11,

    TEXT_RALIA_SAVED                = 1,
    TEXT_RALIA_SHIELD               = 2,
    TEXT_RALIA_SMALL_SPIDERS        = 3,
    TEXT_RALIA_POOR_CREATURES       = 4,
    TEXT_RALIA_HELP                 = 5,
    TEXT_RALIA_OUT                  = 0
};

constexpr float RALIA_RIDE_SPEED = 16.0f;   // yd/s, about twice a mounted player
constexpr float RALIA_TWO_PI     = 6.2831853f;

// Where she comes down when the ritual breaks: Hrun's own position, so the drop
// reads as "the ritual ends and she falls where he stood". Her spawn row sits
// five yards to the side of him, which is where a plain MoveFall() put her -
// the scene wants her on the altar, not next to it. Hard-coded on purpose:
// ReleaseFromCaptiveState() also runs from the gossip fallback, by which time
// Hrun may already be a despawned corpse.
constexpr float RALIA_DROP_X = 69.4234f;
constexpr float RALIA_DROP_Y = -2121.94f;

// Last node of the escape path - the lip of the wall, right next to Captain
// Garrick (156651, spawn 8001001 at 87 / -2246.5 / 94.48). Kept as constants
// because the ride also finishes on proximity: node 23 is reached by a vertical
// 6.6-yard climb up the cliff face, and a navigation hiccup there must not leave
// the player stuck on her back with the quest unfinished.
//
// The proximity test carries a Z window as well. Node 22 - the foot of that same
// wall - sits at the identical x/y, so a horizontal-only check would close the
// ride the instant she arrived at the bottom of the climb and drop the player
// inside the rock.
constexpr float RALIA_END_X = 86.4066f;
constexpr float RALIA_END_Y = -2244.46f;
constexpr float RALIA_END_Z = 93.7894f;
constexpr float RALIA_END_RADIUS = 8.0f;
constexpr float RALIA_END_Z_WINDOW = 3.0f;

// Where the player is put down once the ride closes: two yards in front of
// Captain Garrick (156651, guid 8001001 at 87 / -2246.5 / 94.48, facing 1.5),
// so the quest hand-in NPC is in plain sight the moment they land. Hard-coded
// rather than derived from the creature: the creature may be missing from the
// world, and the landing must not depend on it.
constexpr float RALIA_LEDGE_X = 87.14f;
constexpr float RALIA_LEDGE_Y = -2244.50f;
constexpr float RALIA_LEDGE_Z = 93.92f;

// Where Garrick himself is put when the ride closes - his own spawn point two
// yards behind the landing spot, but summoned at runtime rather than taken from
// creature guid 8001001, which never renders on the client. Z is the spawn
// row's 94.48; the walkable floor there is 94.148, so he simply settles.
constexpr float GARRICK_LEDGE_X = 87.0f;
constexpr float GARRICK_LEDGE_Y = -2246.5f;
constexpr float GARRICK_LEDGE_Z = 94.48f;
constexpr float GARRICK_LEDGE_O = 1.5f;

// Captain Garrick, the npc the player reports back to at the mouth of the burrow.
constexpr uint32 NPC_CAPTAIN_GARRICK_Q55639 = 156651;

// Node 22, the foot of the wall. She has to get from here to node 23 by climbing
// the face. If she is still standing here a few seconds later the waypoint
// generator has given up on that leg (first attempt used a perfectly vertical
// last leg - dx = dy = 0 - and the ride froze on the spot), so the ride pushes
// her up with a direct move of its own instead of dead-ending the scene.
constexpr float RALIA_WALL_FOOT_X = 84.6f;
constexpr float RALIA_WALL_FOOT_Y = -2238.5f;
constexpr float RALIA_WALL_FOOT_RADIUS = 3.0f;

// Garrick is spawned in this phase. phase_area only hands it out while quest
// 55639 is completed or rewarded (conditions: type 47, state mask 66), which is
// circular - the ride is the objective that completes it. So the arrival grants
// the phase explicitly, otherwise the player is dropped on an empty ledge.
constexpr uint32 RALIA_QUEST_PHASE = 13816;

// How long she stays on her line before the player is put on her back and she
// starts running. creature_text.Duration is only the return value of
// SendChat() for the server and is never sent to the client, so the client
// sizes the bubble by itself - this beat is the only thing that can hold the
// ride back until "You saved my life! Allow me to return the favor! Hop on!"
// has actually been read.
constexpr Milliseconds RALIA_SPEECH_LINE = 6s;

// Click-to-ride is only meant to exist in stag form. It is toggled through
// NpcFlags at runtime because creature_template.npcflag is per template, not
// per form: the spawn carries both GOSSIP (right-click menu) and SPELLCLICK
// (the client's "mount me" pointer, backed by npc_spellclick_spells 156929 ->
// 46598 Ride Vehicle). With SPELLCLICK stripped while she is a night elf the
// client has nothing to click, so the player is never sucked onto her back -
// much cleaner than accepting the board and ejecting a moment later.
constexpr NPCFlags RALIA_RIDE_NPC_FLAGS = static_cast<NPCFlags>(UNIT_NPC_FLAG_GOSSIP | UNIT_NPC_FLAG_SPELLCLICK);

// While Hrun drains her, Ralia hangs over the platform. That is NOT done with
// the hover movement flag any more - see ApplyCaptiveState() for why the
// gravity flag is used instead. Her suspended height now comes from
// creature.position_z alone (guid 8001000), roughly 2.4 yd above the platform
// Hrun stands on.

// Resolved once: 91010 is the model every other Ralia in the zone wears, 37729
// is the fallback the client is guaranteed to carry.
inline uint32 RaliaNightElfDisplayId()
{
    static uint32 resolved = 0;

    if (!resolved)
    {
        if (sCreatureDisplayInfoStore.LookupEntry(DISPLAY_RALIA_NIGHTELF))
            resolved = DISPLAY_RALIA_NIGHTELF;
        else
        {
            resolved = DISPLAY_RALIA_FALLBACK;
            TC_LOG_ERROR("server", "Ralia (156929): display {} is missing on this client, using {} for her night elf form.",
                uint32(DISPLAY_RALIA_NIGHTELF), resolved);
        }
    }

    return resolved;
}

// Puts the rider down on the ledge one beat after the vehicle exit.
//
// Why not teleport straight away: the "let go of her" instruction to the client
// is the vehicle-exit spline that Unit::_ExitVehicle() queues at the highest
// motion priority. Player::TeleportTo() cancels that spline (DisableSpline() +
// InterruptOnTeleport()) in the same tick it is queued, so the client never
// receives the exit and keeps rendering the rider sitting on a creature the
// server has already released. Delaying the move by 800 ms lets the exit packet
// out first; the teleport then rebuilds the movement state on top of it.
class RaliaLedgeDrop : public BasicEvent
{
public:
    explicit RaliaLedgeDrop(ObjectGuid playerGuid) : _playerGuid(playerGuid) { }

    bool Execute(uint64, uint32) override
    {
        Player* player = ObjectAccessor::FindConnectedPlayer(_playerGuid);
        if (!player)
            return true;

        bool stillAttached = !player->m_movementInfo.transport.guid.IsEmpty() || player->GetVehicle();

        // anything the exit left behind has to be gone before the move, or the
        // teleport ships the old transport block straight back to the client
        player->m_movementInfo.transport.Reset();
        player->m_movementInfo.ResetTransport();

        TC_LOG_INFO("server", "Ralia (156929): placing {} on the ledge - still attached {}, at ({:.2f}, {:.2f}, {:.2f}).",
            player->GetName(), stillAttached ? 1 : 0,
            player->GetPositionX(), player->GetPositionY(), player->GetPositionZ());

        player->TeleportTo(player->GetMapId(), RALIA_LEDGE_X, RALIA_LEDGE_Y, RALIA_LEDGE_Z, player->GetOrientation());
        return true;
    }

private:
    ObjectGuid _playerGuid;
};

// Which seat of her kit still holds this rider, or -1. Used by the teardown log
// only: the seat table and m_vehicle can disagree, and that disagreement is the
// whole reason the rider used to stay glued to her back.
static int8 RaliaRiderSeat(Creature* ralia, Player* player)
{
    Vehicle* kit = ralia->GetVehicleKit();
    if (!kit || !player)
        return -1;

    for (int8 seat = 0; seat < 8; ++seat)
        if (kit->GetPassenger(seat) == player)
            return seat;

    return -1;
}

// Closes the ride using nothing but the creature and the rider. Deliberately a
// free function: the AI that starts the ride has twice been found dead by the
// time she reaches the last wall (no waypoint callbacks, no UpdateAI ticks at
// all - only the motion generator kept running), and the scene still has to end
// with the player standing on the ledge with the quest credited.
void EndRaliaEscapeRide(Creature* ralia, Player* player)
{
    if (!ralia)
        return;

    TC_LOG_INFO("server", "Ralia (156929): escape finished at ({:.2f}, {:.2f}, {:.2f}), rider {}.",
        ralia->GetPositionX(), ralia->GetPositionY(), ralia->GetPositionZ(),
        player ? player->GetName() : "<none>");

    TC_LOG_INFO("server", "Ralia (156929): teardown - rider {}, attached to vehicle {}, seat map {}.",
        player ? player->GetName() : "<none>",
        (player && player->GetVehicle()) ? 1 : 0,
        ralia->GetVehicleKit() ? 1 : 0);

    ralia->GetMotionMaster()->Clear();
    ralia->StopMoving();

    // Put the rider down. Four separate things go wrong here and every one of
    // them has to be covered:
    //  1. ExitVehicle() opens with `if (!m_vehicle) return;` and on the live
    //     ride the server already considered the player detached ("attached to
    //     vehicle 0" in the log) while the client kept rendering them sitting
    //     on her back, so the call did nothing.
    //  2. SPELL_AURA_CONTROL_VEHICLE sits on the *vehicle* with the rider as
    //     its caster - HandleAuraControlVehicle() returns early unless the
    //     aura's own target IsVehicle() - so clearing it from the player does
    //     nothing. It has to come off her, keyed by the rider's guid.
    //  3. Vehicle::GetPassenger() has come back empty on a live ride, so
    //     walking the seats is not a reliable way to find the rider either.
    //  4. The seat is CAN_CONTROL, so VehicleJoinEvent::Execute() put a
    //     CHARM_TYPE_VEHICLE charm on the pair after it set m_vehicle. When the
    //     control aura disappears before the exit runs, the vehicle release is
    //     skipped and the rider stays charmed by her - and Player::
    //     StopCastingCharm() cannot be used as the recovery here because it
    //     ABORT()s the whole server on exactly this state. Undo the charm by
    //     hand instead.
    if (player)
    {
        TC_LOG_INFO("server", "Ralia (156929): teardown state - vehicle {}, charmed {}, charmer {}, seat {}, transport {}, control auras ralia {} / player {}.",
            player->GetVehicle() ? 1 : 0,
            player->GetCharmed() ? 1 : 0,
            ralia->GetCharmerGUID().ToString(),
            int32(RaliaRiderSeat(ralia, player)),
            player->m_movementInfo.transport.guid.IsEmpty() ? 0 : 1,
            uint32(ralia->HasAuraType(SPELL_AURA_CONTROL_VEHICLE) ? 1 : 0),
            uint32(player->HasAuraType(SPELL_AURA_CONTROL_VEHICLE) ? 1 : 0));

        // 1) her side first: removing this aura is what triggers _ExitVehicle()
        ralia->RemoveAurasByType(SPELL_AURA_CONTROL_VEHICLE, player->GetGUID());

        // 2) the sanctioned way out, for the case where m_vehicle is still set
        if (player->GetVehicle())
            player->ExitVehicle();

        // 3) the charm the join event left behind
        if (Unit* charmed = player->GetCharmed())
        {
            player->SetClientControl(charmed, false);
            player->SetClientControl(player, true);
            player->SetCharm(charmed, false);
            charmed->RemoveUnitFlag(UNIT_FLAG_POSSESSED);
            charmed->RemoveCharmAuras();
        }

        // 4) rider-side leftovers
        player->RemoveAurasByType(SPELL_AURA_CONTROL_VEHICLE);

        // The riding pose is driven by the movement info's transport reference,
        // NOT by m_vehicle. With m_vehicle already null nothing in the teardown
        // clears it, so the player keeps sitting on thin air while the server
        // believes they are standing on the ground. Reset the transport here so
        // the delayed drop below ships a transport-free movement block.
        player->m_movementInfo.transport.Reset();
        player->m_movementInfo.ResetTransport();
    }

    if (Vehicle* kit = ralia->GetVehicleKit())
    {
        for (int8 seat = 0; seat < 8; ++seat)
        {
            Unit* passenger = kit->GetPassenger(seat);
            if (!passenger)
                continue;

            if (passenger->IsPlayer())
                passenger->ToPlayer()->SetImmuneToNPC(false);

            passenger->ExitVehicle();
        }

        // last resort: anything still attached to her must not stay there, or
        // she despawns two minutes later with a rider tied to a dead seat.
        // No-op when every passenger already left.
        kit->RemoveAllPassengers();
    }

    // The ride is over, so neither of them stays untouchable: she is a normal
    // quest NPC again and the player has to be attackable by NPCs like before.
    ralia->SetImmuneToNPC(false);
    ralia->SetReactState(REACT_AGGRESSIVE);

    if (player)
        player->SetImmuneToNPC(false);

    // A vehicle keeps its base model while a passenger is still on it, so the
    // shift back has to happen strictly after ExitVehicle().
    ralia->SetObjectScale(1.0f);
    ralia->SetDisplayId(RaliaNightElfDisplayId());

    // back in night elf form: nothing to click any more
    ralia->RemoveNpcFlag(RALIA_RIDE_NPC_FLAGS);

    // nothing of the captive suspend should survive onto the ledge
    ralia->SetControlled(false, UNIT_STATE_ROOT);
    ralia->SetDisableGravity(false);

    if (player)
    {
        // No explicit phasing here, and this is important: phasing is a
        // whitelist. The moment the player owns a phase they stop seeing
        // everything that owns none, so handing them 13816 here hid Garrick
        // (PhaseId 0 + always-visible) instead of revealing him. He now carries
        // PHASE_USE_FLAGS_ALWAYS_VISIBLE on his own spawn row, which ignores
        // phasing entirely - see fix_garrick_phase_156651.sql.

        // Hand the player a real move, but not in this tick: the exit spline
        // that tells the client to stop sitting on her is queued at the highest
        // motion priority, and TeleportTo() cancels it (DisableSpline() +
        // InterruptOnTeleport()) before it ever goes out. That is precisely the
        // "server says dismounted, client keeps sitting" state we kept hitting,
        // so the drop waits ~0.8 s in RaliaLedgeDrop.
        if (ralia->GetPositionZ() < RALIA_END_Z - 3.0f)
            TC_LOG_ERROR("server", "Ralia (156929): ride closed {:.1f} yd below the exit - moving the player to the ledge.",
                RALIA_END_Z - ralia->GetPositionZ());

        player->m_Events.AddEventAtOffset(new RaliaLedgeDrop(player->GetGUID()), 800ms);

        // Garrick is the static spawn guid 8001001 on the ledge - and he now
        // actually renders. The reason he never did was neither the model nor
        // the phase: world.conditions holds two OBJECT_ID_VISIBILITY rows
        // (SourceTypeOrReferenceId 32, SourceGroup 5 = TYPEID_UNIT in this
        // core) for entry 156651 and BOTH must pass before the creature is ever
        // sent to a client. One of them only allowed quest 55173 "Northbound"
        // to be untaken, so every player who had already finished it - which is
        // everyone on this quest chain - got him filtered out by
        // CanSeeOrDetect() long before display or phasing mattered. See
        // fix_garrick_visibility_condition_156651.sql (mask 1 -> 65).
        //
        // The summon below is a safety net only: it fires when the spawn is not
        // in the world at all (grid not loaded, row deleted), never to stand in
        // for a spawn that exists.
        if (!player->FindNearestCreature(NPC_CAPTAIN_GARRICK_Q55639, 60.0f))
        {
            if (Creature* garrick = ralia->SummonCreature(NPC_CAPTAIN_GARRICK_Q55639,
                    GARRICK_LEDGE_X, GARRICK_LEDGE_Y, GARRICK_LEDGE_Z, GARRICK_LEDGE_O,
                    TEMPSUMMON_TIMED_OR_DEAD_DESPAWN, 1h))
            {
                PhasingHandler::InheritPhaseShift(garrick, player);
                garrick->UpdateObjectVisibility();
                garrick->SetFacingToObject(player);

                TC_LOG_INFO("server", "Ralia (156929): no Garrick within 60 yd - summoned a stand-in at ({:.2f}, {:.2f}, {:.2f}), display {}.",
                    garrick->GetPositionX(), garrick->GetPositionY(), garrick->GetPositionZ(),
                    garrick->GetDisplayId());
            }
            else
                TC_LOG_ERROR("server", "Ralia (156929): no Garrick within 60 yd and the stand-in could not be summoned.");
        }

        // Put her down with the player instead of leaving her wherever the last
        // waypoint happened to be, so the two of them end the scene together.
        float x = player->GetPositionX() + std::cos(player->GetOrientation()) * 1.8f;
        float y = player->GetPositionY() + std::sin(player->GetOrientation()) * 1.8f;
        ralia->NearTeleportTo(x, y, player->GetPositionZ(), ralia->GetOrientation());
        ralia->SetFacingToObject(player);

        // objective 393740 "Ride Ralia Dreamchaser to escape"
        player->KilledMonsterCredit(NPC_RALIA_DREAMCHASER_Q55639);
    }

    sCreatureTextMgr->SendChat(ralia, TEXT_RALIA_OUT, player, CHAT_MSG_MONSTER_SAY, LANG_UNIVERSAL);

    // Long enough for the player to actually see her back in night elf form;
    // the respawn delay still governs when she returns to the pit.
    ralia->DespawnOrUnsummon(2min, Seconds(ralia->GetRespawnDelay()));
}

// Forward declaration: defined after the AI struct, because the watchdog has to
// know the concrete AI type to reset its ride state when it closes the scene.
void StartRaliaRideWatchdog(Creature* ralia);

// 156929 - Ralia Dreamchaser
//
// This is the Alliance branch of the pit escape; the Horde branch uses Crenna
// Earth-Daughter. Retail plays the ride as a client scene, which a private
// server cannot reproduce, so the ride uses the official waypoint path that
// already ships in the world database: path 156929 holds 23 nodes laid out along
// the spiral gallery. Pathfinding stays enabled (WaypointMovementGenerator's
// default) because the nodes sit 20-55 yards apart and were authored to follow
// the corridor, so each leg can be stitched through the navmesh.
struct npc_ralia_dreamchaser_q55639 : public ScriptedAI
{
    npc_ralia_dreamchaser_q55639(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        // an AI re-init in the middle of the ride would silently wipe the ride
        // state while she keeps running on the motion generator, so say so
        if (_escaping)
            TC_LOG_ERROR("server", "Ralia (156929): AI reset in the middle of the escape at ({:.2f}, {:.2f}, {:.2f}) - ride state dropped.",
                me->GetPositionX(), me->GetPositionY(), me->GetPositionZ());

        _events.Reset();
        _escaping = false;
        _saidHelp = false;
        _finishRequested = false;
        _rideArmed = false;
        _stuckMs = 0;
        _heartbeatMs = 0;
        _escapeeGUID.Clear();
        _ejectGUID.Clear();

        // an evade or respawn in the middle of the scene must not leave the
        // no-combat state from the ride stuck on her
        me->SetImmuneToNPC(false);
        me->SetReactState(REACT_AGGRESSIVE);
    }

    void JustAppeared() override
    {
        // JustAppeared() also runs after a respawn, so this re-applies the
        // captive state every time she comes back with Hrun still alive.
        ApplyCaptiveState();
        _events.ScheduleEvent(EVENT_RALIA_HELP_CHECK, 2s);
    }

    bool OnGossipHello(Player* player) override
    {
        // manual fallback for a player who killed Hrun but missed the ride
        if (player->GetQuestStatus(QUEST_WHO_LURKS_IN_THE_PIT) != QUEST_STATUS_INCOMPLETE)
            return false;

        // Hrun respawns after 300 s, so "is Hrun alive" was the wrong test: it
        // locked the menu again minutes after the kill. Read the quest's own
        // "Defeat Hrun" counter instead, which stays ticked for this player.
        if (player->GetQuestObjectiveData(QUEST_WHO_LURKS_IN_THE_PIT, OBJECTIVE_DEFEAT_HRUN) <= 0)
            return false;

        // She is only rideable as the stag. While the ritual holds her she is a
        // captive hanging over the altar, and during the freed beat she is still
        // a night elf saying her line - neither may be mounted, so no menu.
        if (me->GetDisplayId() != DISPLAY_RALIA_STAG)
            return false;

        ClearGossipMenuFor(player);
        AddGossipItemFor(player, GossipOptionNpc::None, "Take me out of this pit, Ralia!", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_RIDE_RALIA);
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, me);
        return true;
    }

    bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
    {
        if (gossipListId != 0)
            return false;

        CloseGossipMenuFor(player);

        // Do not shift her here: this path can still be refused in
        // PassengerBoarded() when Hrun is alive, and a shift done before the
        // way in has no matching shift back. StartRide() does it once the ride
        // is actually allowed.
        player->EnterVehicle(me, 0);
        return true;
    }

    void DoAction(int32 action) override
    {
        if (action != ACTION_HRUN_DIED || _escaping)
            return;

        _escaping = true;

        // Step 2 starts here: the ritual breaks with him and she drops out of
        // the air. She is still a night elf at this point - the shift comes a
        // beat later, once the player has seen her come down.
        ReleaseFromCaptiveState();
        _events.CancelEvent(EVENT_RALIA_HELP_CHECK);

        // the altar keeps burning behind them while she shifts and calls the
        // player over, so place the braziers before she starts moving
        SpawnRitualBraziers();

        // Step 3 starts here: the burrow comes down the moment he drops, so the
        // pit is already burning while the player climbs on.
        for (uint32 i = 0; i < 6; ++i)
            SpawnFireAt(frand(0.0f, RALIA_TWO_PI), frand(3.0f, 10.0f));

        Player* escapee = me->SelectNearestPlayer(50.0f);
        if (!escapee || escapee->GetQuestStatus(QUEST_WHO_LURKS_IN_THE_PIT) != QUEST_STATUS_INCOMPLETE)
            return;

        _escapeeGUID = escapee->GetGUID();

        // hold the humanoid form until she has actually landed, so the order
        // reads on screen: freed in night elf form -> she drops -> she shifts ->
        // the player is put on her. The fall is ~2.4 yd, about 0.7 s, so 1.2 s
        // leaves her standing on the altar before the stag appears.
        _events.ScheduleEvent(EVENT_RALIA_SHIFT, 1200ms);
    }

    void PassengerBoarded(Unit* passenger, int8 seatId, bool apply) override
    {
        if (seatId != 0 || !passenger->IsPlayer())
            return;

        Player* player = passenger->ToPlayer();

        // Leaving the vehicle for any reason - end of the run, a jump-off, or a
        // refused ride - has to hand the rider's normal targetability back,
        // otherwise the escape would leave them immune to every NPC afterwards.
        if (!apply)
        {
            // The rider leaving on their own is the one path that needs no
            // help from EndRaliaEscapeRide(), so log which of the two it was -
            // it is the difference between "the exit ran" and "the aura
            // vanished and the exit never happened".
            TC_LOG_INFO("server", "Ralia (156929): rider {} left seat {} - vehicle now {}, transport {}.",
                player->GetName(), int32(seatId),
                player->GetVehicle() ? 1 : 0,
                player->m_movementInfo.transport.guid.IsEmpty() ? 0 : 1);

            player->SetImmuneToNPC(false);
            return;
        }

        // She is a vehicle the whole time, so spellclick lands even while the
        // ritual still holds her. The ride only exists in stag form, and only
        // once the "Defeat Hrun" objective is ticked - otherwise a player could
        // mount her and skip the fight. A refused ride changes none of her
        // state: she is put straight back down exactly as she was.
        bool rideable = me->GetDisplayId() == DISPLAY_RALIA_STAG;

        if (!rideable ||
            (!_escaping &&
                player->GetQuestObjectiveData(QUEST_WHO_LURKS_IN_THE_PIT, OBJECTIVE_DEFEAT_HRUN) <= 0))
        {
            _ejectGUID = player->GetGUID();
            _events.ScheduleEvent(EVENT_RALIA_EJECT, 200ms);
            return;
        }

        // covers the gossip path too, where DoAction() never ran.
        // Objective 393740 "Ride Ralia Dreamchaser to escape" is deliberately
        // NOT credited here: the quest wants the player actually carried out of
        // the pit, so it is credited on arrival in FinishEscape().
        _escaping = true;

        // Also covers the gossip path, where DoAction() never ran and this is
        // the only place the rider's guid gets recorded. Everything downstream
        // (FinishEscape, ForceCloseRide) falls back to it when the seat map
        // cannot produce the passenger.
        _escapeeGUID = player->GetGUID();

        StartRide();
    }

    void WaypointReached(uint32 nodeId, uint32 pathId) override
    {
        if (pathId != RALIA_ESCAPE_PATH)
            return;

        // the last three nodes are the only ones that ever misbehaved; log them
        // so a ride that dies on the final climb says exactly where it died
        if (nodeId >= 20)
            TC_LOG_INFO("server", "Ralia (156929): reached waypoint {} at ({:.2f}, {:.2f}, {:.2f}).",
                nodeId, me->GetPositionX(), me->GetPositionY(), me->GetPositionZ());

        // she is at the foot of the last wall: start the watchdog that pushes her
        // up the face if the waypoint generator refuses the climb
        if (nodeId == RALIA_ESCAPE_LAST_NODE - 1)
            _events.ScheduleEvent(EVENT_RALIA_CLIMB_FALLBACK, 3000ms);

        if (nodeId != RALIA_ESCAPE_LAST_NODE)
            return;

        TC_LOG_INFO("server", "Ralia (156929): reached the last waypoint ({}), closing the ride.",
            RALIA_ESCAPE_LAST_NODE);

        RequestFinish();
    }

    // Both the last waypoint and the proximity check land here, so a navigation
    // failure on the final climb still ends the scene instead of leaving the
    // player riding her forever.
    void RequestFinish()
    {
        if (_finishRequested)
            return;

        _finishRequested = true;

        _events.CancelEvent(EVENT_RALIA_TIMEOUT);
        _events.CancelEvent(EVENT_RALIA_FIRE);
        _events.CancelEvent(EVENT_RALIA_SHAKE);

        // Do not tear the vehicle down from inside the waypoint generator's own
        // Update() call; defer one tick so the movement stack unwinds first.
        _events.ScheduleEvent(EVENT_RALIA_FINISH, 50ms);
    }

    // Last line of defence for the ride. Everything above this depends on
    // something that has already failed at least once: the last waypoint
    // callback never fired, the spline stopped yards off the node, and the
    // distance test needs the server to know she is near the exit at all. This
    // only needs her to stand still for five seconds, which no healthy ride
    // does, and it ends the scene at the ledge either way.
    void WatchRideProgress(uint32 diff)
    {
        float x = me->GetPositionX();
        float y = me->GetPositionY();
        float z = me->GetPositionZ();

        // heartbeat: tells "is the script ticking at all" from "the script ticks
        // but the ride is lost", which look identical from in game
        _heartbeatMs += diff;
        if (_heartbeatMs >= 3000)
        {
            _heartbeatMs = 0;

            auto [waypointId, pathId] = me->GetCurrentWaypointInfo();
            TC_LOG_INFO("server", "Ralia (156929) ride: ({:.2f}, {:.2f}, {:.2f}), armed={}, spline done={}, waypoint {}/{}.",
                x, y, z, _rideArmed ? 1 : 0, me->movespline->Finalized() ? 1 : 0, waypointId, pathId);
        }

        if (!_rideArmed)
            return;

        float moved = me->GetDistance(_lastRideX, _lastRideY, _lastRideZ);
        if (moved > 1.0f)
        {
            _lastRideX = x;
            _lastRideY = y;
            _lastRideZ = z;
            _stuckMs = 0;
            return;
        }

        _stuckMs += diff;
        if (_stuckMs < 5000)
            return;

        TC_LOG_ERROR("server", "Ralia (156929): ride stalled for 5 s at ({:.2f}, {:.2f}, {:.2f}) - closing the scene.",
            x, y, z);

        RequestFinish();
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        // Probe: is this AI being ticked at all while somebody is on her back?
        // Every failure of this scene so far has boiled down to that question,
        // and from in game "the AI is dead" and "the AI is alive but its ride
        // state was wiped" look exactly the same.
        if (Vehicle* probeKit = me->GetVehicleKit())
        {
            if (probeKit->GetPassenger(0))
            {
                _tickProbeMs += diff;
                if (_tickProbeMs >= 5000)
                {
                    _tickProbeMs = 0;
                    TC_LOG_INFO("server", "Ralia (156929) tick: ({:.2f}, {:.2f}, {:.2f}), escaping={}, finishPending={}, armed={}.",
                        me->GetPositionX(), me->GetPositionY(), me->GetPositionZ(),
                        _escaping ? 1 : 0, _finishRequested ? 1 : 0, _rideArmed ? 1 : 0);
                }
            }
            else
                _tickProbeMs = 0;
        }

        if (_escaping && !_finishRequested)
            WatchRideProgress(diff);

        // Node 23 is a vertical climb up the last wall. If the spline refuses it
        // the generator simply stops, and without this the player would sit on
        // her back forever with the quest still open - which is exactly what
        // happened before. Finish on horizontal + vertical proximity too, so the
        // ride always closes out even if that final climb is cut short.
        if (_escaping && !_finishRequested)
        {
            float dx = me->GetPositionX() - RALIA_END_X;
            float dy = me->GetPositionY() - RALIA_END_Y;
            float dz = me->GetPositionZ() - RALIA_END_Z;

            if (dx * dx + dy * dy <= RALIA_END_RADIUS * RALIA_END_RADIUS &&
                std::fabs(dz) <= RALIA_END_Z_WINDOW)
            {
                TC_LOG_INFO("server", "Ralia (156929): within {:.1f} yd of the exit at ({:.2f}, {:.2f}, {:.2f}), closing the ride.",
                    RALIA_END_RADIUS, me->GetPositionX(), me->GetPositionY(), me->GetPositionZ());

                RequestFinish();
            }
        }

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_RALIA_SHIFT:
                {
                    // the gossip path may have taken over and shifted her
                    // already; do not repeat the line in that case
                    if (me->GetDisplayId() == DISPLAY_RALIA_STAG)
                        break;

                    // Step 2, second half: she is a druid, this is the shift
                    // from the freed night elf into the stag.
                    TransformIntoStag();
                    sCreatureTextMgr->SendChat(me, TEXT_RALIA_SAVED, nullptr, CHAT_MSG_MONSTER_SAY, LANG_UNIVERSAL);

                    // Step 3: the player is put on her, no click needed - but
                    // only after she has finished talking.
                    _events.ScheduleEvent(EVENT_RALIA_BOARD, RALIA_SPEECH_LINE);
                    break;
                }
                case EVENT_RALIA_BOARD:
                {
                    if (Player* player = ObjectAccessor::GetPlayer(*me, _escapeeGUID))
                    {
                        me->SetFacingToObject(player);
                        player->EnterVehicle(me, 0);
                    }
                    break;
                }
                case EVENT_RALIA_FIRE:
                {
                    // The burrow is coming down behind them. Flames are dropped
                    // in the stag's wake rather than around her, so the wall of
                    // fire chases the ride up the gallery instead of running
                    // ahead of it.
                    float back = me->GetOrientation() + float(M_PI);
                    for (uint32 i = 0; i < 2; ++i)
                        SpawnFireAt(back + frand(-0.85f, 0.85f), frand(3.0f, 12.0f));

                    _events.ScheduleEvent(EVENT_RALIA_FIRE, 700ms);
                    break;
                }
                // EVENT_RALIA_SHAKE is deliberately not handled any more. The
                // shake was the only screen-wide effect the ride applied
                // (spell 248719, repeated every 1.5 s for the whole run), and
                // it is what pulls the client into its cinematic state: the
                // player gets letterbox bars above and below the screen for
                // the entire ride. The collapse is still sold by the flames
                // and the falling rock, so the camera shake is simply gone.
                case EVENT_RALIA_LINE_SPIDERS:
                {
                    sCreatureTextMgr->SendChat(me, TEXT_RALIA_SMALL_SPIDERS, nullptr, CHAT_MSG_MONSTER_SAY, LANG_UNIVERSAL);
                    break;
                }
                case EVENT_RALIA_LINE_CREATURES:
                {
                    sCreatureTextMgr->SendChat(me, TEXT_RALIA_POOR_CREATURES, nullptr, CHAT_MSG_MONSTER_SAY, LANG_UNIVERSAL);
                    break;
                }
                case EVENT_RALIA_FINISH:
                {
                    if (!_escaping)
                        break;

                    // The rider may be unreachable through the seat map even
                    // though they are still on her back, so let FinishEscape()
                    // resolve them (seat map -> scene guid -> nearest player).
                    // Without a rider the scene still closes out, so she does
                    // not stay a stag standing on the ledge.
                    FinishEscape(nullptr);
                    break;
                }
                case EVENT_RALIA_ARM_RIDE:
                {
                    _rideArmed = true;
                    _stuckMs = 0;
                    _lastRideX = me->GetPositionX();
                    _lastRideY = me->GetPositionY();
                    _lastRideZ = me->GetPositionZ();
                    break;
                }
                case EVENT_RALIA_CLIMB_FALLBACK:
                {
                    if (!_escaping || _finishRequested)
                        break;

                    float dx = me->GetPositionX() - RALIA_WALL_FOOT_X;
                    float dy = me->GetPositionY() - RALIA_WALL_FOOT_Y;

                    // still parked at the foot of the wall -> the waypoint
                    // generator is not taking her up, so drive the last leg here
                    if (dx * dx + dy * dy > RALIA_WALL_FOOT_RADIUS * RALIA_WALL_FOOT_RADIUS)
                        break;

                    TC_LOG_ERROR("server", "Ralia (156929): stuck at the foot of the wall ({:.2f}, {:.2f}, {:.2f}) - pushing her up it manually.",
                        me->GetPositionX(), me->GetPositionY(), me->GetPositionZ());

                    me->GetMotionMaster()->MovePoint(0, Position(RALIA_END_X, RALIA_END_Y, RALIA_END_Z, 0.0f), false, {}, RALIA_RIDE_SPEED);
                    break;
                }
                case EVENT_RALIA_TIMEOUT:
                {
                    // safety net: the generator never reported the last node
                    TC_LOG_ERROR("server", "Ralia (156929): escape timed out at ({:.2f}, {:.2f}, {:.2f}).",
                        me->GetPositionX(), me->GetPositionY(), me->GetPositionZ());
                    _events.CancelEvent(EVENT_RALIA_FIRE);
                    _events.CancelEvent(EVENT_RALIA_SHAKE);

                    FinishEscape(nullptr);
                    break;
                }
                case EVENT_RALIA_EJECT:
                {
                    // deferred so we never call ExitVehicle() from inside the
                    // AddPassenger() callback that raised PassengerBoarded()
                    if (Player* player = ObjectAccessor::GetPlayer(*me, _ejectGUID))
                        player->ExitVehicle();

                    _ejectGUID.Clear();

                    // A refused ride must leave her exactly as she was: still
                    // hanging over the altar in night elf form. Without this a
                    // stray click left her stuck as a stag on the floor.
                    RestoreCaptiveForm();
                    break;
                }
                case EVENT_RALIA_HELP_CHECK:
                {
                    CheckForHelpCry();
                    break;
                }
                default:
                    break;
            }
        }
    }

private:
    // GetMapHeight() answers with VMAP_INVALID_HEIGHT_VALUE (-200000) when the
    // probe finds nothing, so fall back to the caster's own plane instead of
    // dropping a prop 200k yards underground.
    float GroundZ(float x, float y) const
    {
        float z = me->GetMapHeight(x, y, me->GetPositionZ() + 2.0f, true, 10.0f);
        if (z < -100000.0f || z > me->GetPositionZ() + 10.0f)
            z = me->GetPositionZ();
        return z;
    }

    void SpawnFireAt(float angle, float dist)
    {
        float x = me->GetPositionX() + std::cos(angle) * dist;
        float y = me->GetPositionY() + std::sin(angle) * dist;

        if (GameObject* flame = me->SummonGameObject(GO_FLAME_EMITTER, x, y, GroundZ(x, y), angle, QuaternionData(), Seconds(GO_FLAME_LIFETIME)))
            flame->SetObjectScale(frand(0.6f, 1.5f));
    }

    // The pit already has two bare braziers standing either side of the altar;
    // the scene only has to light them, so nothing is summoned except the fire
    // itself. Positions are the real prop anchors read in game - a floor probe
    // here comes back with the pit floor about 2.5 yd below the bowls and would
    // bury the flames in the rock.
    void SpawnRitualBraziers()
    {
        static Position const RitualFirePositions[] =
        {
            { 75.475624f, -2118.487793f, -27.793022f, 4.999099f },   // left of the altar
            { 63.658173f, -2124.810303f, -27.700300f, 5.251594f }    // right of the altar
        };

        uint32 spawned = 0;

        for (Position const& pos : RitualFirePositions)
        {
            if (me->SummonGameObject(GO_FLAME_EMITTER, pos.GetPositionX(), pos.GetPositionY(),
                    pos.GetPositionZ(), pos.GetOrientation(), QuaternionData(), Seconds(GO_RITUAL_FIRE_LIFETIME)))
                ++spawned;
        }

        TC_LOG_INFO("server", "Ralia (156929): lit {} ritual flame(s).", spawned);
    }

    // Her night elf form is 91010 (radius 0.306). Resolved at runtime because
    // the client only honours SetDisplayId() when its CreatureDisplayInfo
    // actually carries the id - otherwise the model keeps whatever it already
    // had loaded, which is how she used to stay a stag after the escape.
    uint32 NightElfDisplay() const
    {
        static uint32 resolved = 0;

        if (!resolved)
        {
            if (sCreatureDisplayInfoStore.LookupEntry(DISPLAY_RALIA_NIGHTELF))
                resolved = DISPLAY_RALIA_NIGHTELF;
            else
            {
                resolved = DISPLAY_RALIA_FALLBACK;
                TC_LOG_ERROR("server", "Ralia (156929): display {} is missing on this client, using {} for her night elf form.",
                    uint32(DISPLAY_RALIA_NIGHTELF), resolved);
            }
        }

        return resolved;
    }

    void ApplyCaptiveState()
    {
        // Creature::Respawn() only calls SetDisplayId() when the random-gender
        // roll fires (Creature.cpp:2349), so a creature shifted into the stag
        // keeps the stag model across despawn/respawn. The ritual holds her in
        // her night elf form from start to finish - the shift is what she does
        // once Hrun is dead - so put the humanoid model back unconditionally.
        uint32 elfDisplay = NightElfDisplay();
        if (me->GetDisplayId() != elfDisplay)
        {
            me->SetObjectScale(1.0f);
            me->SetDisplayId(elfDisplay);
        }

        // She spawns as a night elf, i.e. not rideable, so the click-to-ride
        // interaction is locked off here; TransformIntoStag() is the only place
        // that hands it back, and FinishEscape() takes it away again.
        me->RemoveNpcFlag(RALIA_RIDE_NPC_FLAGS);

        // Hrun is only five yards away and stands on the pit floor; if he is
        // gone the ritual is over and she stays on her feet.
        Creature* hrun = me->FindNearestCreature(NPC_HRUN_THE_EXILED_Q55639, 100.0f, true);
        if (!hrun)
            return;

        // The purple rune under her is the client's rendering of the Necrotic
        // Ritual aura, so it goes up first and is re-applied on every respawn -
        // unlike the prisoners she has nothing in creature_template_addon.
        // AddAura() is the very call Creature::LoadCreaturesAddon() uses for the
        // `auras` column (Creature.cpp:2823), so this matches how 154301 gets it
        // and skips cast validation entirely.
        if (!me->HasAura(SPELL_RALIA_NECROTIC_RITUAL))
            me->AddAura(SPELL_RALIA_NECROTIC_RITUAL, me);

        // She hangs over the altar in her night elf form. The shift into a stag
        // is what she does *after* he dies, to carry the player out - it is not
        // her state while the ritual holds her.
        if (me->IsGravityDisabled())
            return;

        // Suspend her the way npc_briarpatch_prisoner (154301) suspends the
        // ritual prisoners, not with SetHover().
        //
        // SetHover(true) sets MOVEMENTFLAG_HOVER, and the client animates a
        // hovering unit with a vertical float of its own. On someone who is
        // supposed to hang dead still that reads as her bobbing up and down,
        // and the ritual rune bound to her model rocks along with it. Hover also
        // fires SetAnimTier(AnimTier::Hover) and SetPlayHoverAnim(true) as two
        // one-shot packets that are never re-sent, which is exactly why the
        // bobbing stopped after leaving the scene's cinematic.
        //
        // DISABLE_GRAVITY keeps her off the floor with none of that attached:
        // no hover movement flag, no hover animation state, so the client just
        // leaves the model standing still in mid-air. Both update flags are
        // passed as false - only the "ignore gravity" part is wanted, and the
        // tier must not be flipped to Fly nor the hover anim flag raised.
        //
        // No floor probe either: her spawn row already carries the suspended Z
        // (2.4 yd above the platform), and GetMapHeight() would pull her down
        // onto the platform below.
        me->SetFacingToObject(hrun);
        me->SetDisableGravity(true, /*updateAnimTier=*/false, /*updatePlayHoverAnim=*/false);
        me->SetControlled(true, UNIT_STATE_ROOT);

        TC_LOG_INFO("server", "Ralia (156929): held in night elf form, gravity disabled at ({:.2f}, {:.2f}, {:.2f}).",
            me->GetPositionX(), me->GetPositionY(), me->GetPositionZ());
    }

    // Undo whatever a refused ride may have changed. Until Hrun dies she is a
    // night elf hanging over the altar - any path that shifts her or drops her
    // to the floor without an actual escape has to put that back.
    void RestoreCaptiveForm()
    {
        if (_escaping)
            return;

        uint32 elfDisplay = NightElfDisplay();
        if (me->GetDisplayId() != elfDisplay)
        {
            me->SetObjectScale(1.0f);
            me->SetDisplayId(elfDisplay);
        }

        // back in night elf form, so also back to not being clickable
        me->RemoveNpcFlag(RALIA_RIDE_NPC_FLAGS);

        if (!me->IsGravityDisabled())
        {
            me->SetDisableGravity(true, /*updateAnimTier=*/false, /*updatePlayHoverAnim=*/false);
            me->SetControlled(true, UNIT_STATE_ROOT);
        }

        // a refused ride has to leave her exactly as the ritual holds her,
        // purple rune included
        if (!me->HasAura(SPELL_RALIA_NECROTIC_RITUAL))
            me->AddAura(SPELL_RALIA_NECROTIC_RITUAL, me);
    }

    void ReleaseFromCaptiveState()
    {
        // the ritual dies with Hrun: the purple goes with it, whether or not
        // she is still suspended at this point
        me->RemoveAura(SPELL_RALIA_NECROTIC_RITUAL);

        if (!me->IsGravityDisabled())
            return;

        // Gravity back on and no longer rooted, then let her drop onto the
        // platform for real instead of teleporting - the drop is part of the
        // scene. SetDisableGravity(false) also restores the ground anim tier
        // and clears the hover anim flag, so nothing of the suspend is left.
        me->SetControlled(false, UNIT_STATE_ROOT);
        me->SetDisableGravity(false);

        // She hangs five yards off the altar, so a plain MoveFall() drops her
        // beside it. Slide her over Hrun's spot at the current height first,
        // then fall - the drop keeps its own motion and still lands on him.
        me->NearTeleportTo(RALIA_DROP_X, RALIA_DROP_Y, me->GetPositionZ(), me->GetOrientation());
        me->GetMotionMaster()->MoveFall();
    }

    void CheckForHelpCry()
    {
        if (_saidHelp)
            return;

        Player* player = me->SelectNearestPlayer(45.0f);
        if (!player)
        {
            _events.ScheduleEvent(EVENT_RALIA_HELP_CHECK, 2s);
            return;
        }

        // stop polling once somebody is close enough to hear her
        _saidHelp = true;

        if (player->GetQuestStatus(QUEST_WHO_LURKS_IN_THE_PIT) == QUEST_STATUS_INCOMPLETE)
            sCreatureTextMgr->SendChat(me, TEXT_RALIA_HELP, player, CHAT_MSG_MONSTER_SAY, LANG_UNIVERSAL);
    }

    void TransformIntoStag()
    {
        // the stag is the only rideable form, so the click-to-ride interaction
        // is handed back here and nowhere else
        me->SetNpcFlag(RALIA_RIDE_NPC_FLAGS);

        if (me->GetDisplayId() == DISPLAY_RALIA_STAG)
            return;

        // 73350 is the display her own spawn row carries: radius 1.156, reach
        // 0.70, and on this client it renders as the white stag. 127129, used
        // here before, is radius 5.36 / reach 8.0 - a giant - which is why the
        // transformed model looked wrong. No down-scaling needed any more.
        me->SetObjectScale(1.0f);
        me->SetDisplayId(DISPLAY_RALIA_STAG);
    }

    void StartRide()
    {
        TC_LOG_INFO("server", "Ralia (156929): starting the escape on waypoint path {}.", uint32(RALIA_ESCAPE_PATH));

        // gossip fallback: make sure the hover flag is gone before she runs
        ReleaseFromCaptiveState();
        TransformIntoStag();
        _finishRequested = false;
        _events.CancelEvent(EVENT_RALIA_BOARD);
        _events.CancelEvent(EVENT_RALIA_SHIFT);
        sCreatureTextMgr->SendChat(me, TEXT_RALIA_SHIELD, nullptr, CHAT_MSG_MONSTER_SAY, LANG_UNIVERSAL);

        // The escape is a scripted ride, not a fight. Mobs left alive in the
        // burrow must not drag either of them into combat while it plays out:
        // she is made immune to NPCs and her own fight is dropped, and the
        // rider gets the same immunity. The rider's half matters because a
        // passenger is still a valid attack target - damage aimed at them is
        // redirected onto the vehicle, so without it every swing would put the
        // player straight back into combat even though she takes no damage.
        me->SetReactState(REACT_PASSIVE);
        me->AttackStop();
        me->SetImmuneToNPC(true);

        if (Vehicle* kit = me->GetVehicleKit())
        {
            if (Unit* rider = kit->GetPassenger(0))
            {
                if (rider->IsPlayer())
                {
                    rider->ToPlayer()->SetImmuneToNPC(true);
                    rider->ToPlayer()->CombatStopWithPets(true);
                }
            }
        }

        me->SetWalk(false);
        me->GetMotionMaster()->Clear();

        // Diagnostics for the last leg: a waypoint path that was not reloaded
        // (fewer nodes than expected) and a spline the unit refuses to advance
        // along both look identical in game - she simply stops. Log what the
        // path actually holds so the two can be told apart from the log alone.
        if (WaypointPath const* escapePath = sWaypointMgr->GetPath(RALIA_ESCAPE_PATH))
        {
            WaypointNode const& last = escapePath->Nodes.back();
            TC_LOG_INFO("server", "Ralia (156929): path {} has {} node(s), last one at node {} ({:.2f}, {:.2f}, {:.2f}).",
                uint32(RALIA_ESCAPE_PATH), uint32(escapePath->Nodes.size()), last.Id, last.X, last.Y, last.Z);
        }
        else
            TC_LOG_ERROR("server", "Ralia (156929): waypoint path {} is NOT loaded on this server.", uint32(RALIA_ESCAPE_PATH));

        me->GetMotionMaster()->MovePath(RALIA_ESCAPE_PATH, false, {}, RALIA_RIDE_SPEED);

        // arm the watchdog three seconds in, so the stationary ticks before the
        // spline launches are not mistaken for a stall
        _rideArmed = false;
        _stuckMs = 0;
        _heartbeatMs = 0;
        _lastRideX = me->GetPositionX();
        _lastRideY = me->GetPositionY();
        _lastRideZ = me->GetPositionZ();
        _events.ScheduleEvent(EVENT_RALIA_ARM_RIDE, 3s);

        // AI-independent watchdog. Everything else in this file depends on this
        // AI being ticked; this does not, and the ride must end even if the AI
        // is gone by the time she reaches the last wall.
        StartRaliaRideWatchdog(me);

        _events.ScheduleEvent(EVENT_RALIA_FIRE, 1200ms);
        // No EVENT_RALIA_SHAKE: the camera-shake spell is what puts the
        // client into cinematic mode, i.e. the letterbox bars. See the note
        // in HandleEvents() where the case used to be.
        _events.ScheduleEvent(EVENT_RALIA_LINE_SPIDERS, 8s);
        _events.ScheduleEvent(EVENT_RALIA_LINE_CREATURES, 22s);
        _events.ScheduleEvent(EVENT_RALIA_TIMEOUT, 180s);
    }

    // The burrow collapsing is a camera shake for the rider. 12.x dropped both
    // the ScreenShake opcode and the shake field of SpellVisualKit, so the only
    // server-side route left is a spell, and 248719 is the one this realm
    // already uses for exactly that (Seething Shore azerite eruption).
    void ShakeRiderCamera()
    {
        Vehicle* kit = me->GetVehicleKit();
        if (!kit)
            return;

        Unit* passenger = kit->GetPassenger(0);
        if (!passenger || !passenger->IsPlayer())
            return;

        passenger->ToPlayer()->CastSpell(passenger, SPELL_CAMERA_SHAKE, true);
    }

    // Single place that decides who the rider is. The seat map is tried first,
    // but Vehicle::GetPassenger(0) has come back empty on a live ride - the
    // player is still on her back, she keeps running, yet the lookup finds
    // nothing. The guid captured when the scene started is the real fallback:
    // it is the player who triggered ACTION_HRUN_DIED and cannot drift.
    Player* ResolveRider()
    {
        if (Vehicle* kit = me->GetVehicleKit())
            for (int8 seat = 0; seat < 8; ++seat)
                if (Unit* passenger = kit->GetPassenger(seat))
                    if (passenger->IsPlayer())
                        return passenger->ToPlayer();

        if (Player* player = ObjectAccessor::GetPlayer(*me, _escapeeGUID))
            return player;

        return me->SelectNearestPlayer(50.0f);
    }

    // Step 4: the ride ends at the mouth of the burrow. The player is put down
    // automatically, she shifts back to her night elf form, and only now does
    // the "Ride Ralia Dreamchaser to escape" objective get credited.
    //
    // The scene itself is handled by EndRaliaEscapeRide(), which needs no AI
    // state: see its comment for why.
    void FinishEscape(Unit* passenger)
    {
        if (!_escaping)
            return;

        Player* player = (passenger && passenger->IsPlayer()) ? passenger->ToPlayer() : nullptr;

        // resolve before the guid is cleared below - once the seat map comes
        // back empty it is the only handle left on the rider
        if (!player)
            player = ResolveRider();

        _events.Reset();
        _escaping = false;
        _escapeeGUID.Clear();
        _rideArmed = false;
        _stuckMs = 0;

        EndRaliaEscapeRide(me, player);
    }

public:
    // Called by RaliaRideWatchdog when the ride has stopped making progress.
    // Its job is only to drop the AI-side state (so the flavour events stop
    // firing and a later timeout cannot credit the ride twice) and hand over to
    // EndRaliaEscapeRide().
    void ForceCloseRide()
    {
        // resolve first: the teardown below clears the guid
        Player* rider = ResolveRider();

        _events.Reset();
        _escaping = false;
        _finishRequested = false;
        _escapeeGUID.Clear();
        _rideArmed = false;
        _stuckMs = 0;

        EndRaliaEscapeRide(me, rider);
    }

    EventMap _events;
    ObjectGuid _escapeeGUID;
    ObjectGuid _ejectGUID;
    bool _escaping = false;
    bool _saidHelp = false;
    bool _finishRequested = false;

    // Ride watchdog. The last leg has failed in every way a movement can fail:
    // a vertical-only spline that does not advance, a spline that ends three
    // yards short of the node, and a waypoint callback that never arrives. None
    // of those may leave the player sitting on her back with the quest open, so
    // the ride is finished from UpdateAI alone once she stops making progress.
    bool _rideArmed = false;
    uint32 _stuckMs = 0;
    uint32 _heartbeatMs = 0;
    uint32 _tickProbeMs = 0;
    float _lastRideX = 0.0f;
    float _lastRideY = 0.0f;
    float _lastRideZ = 0.0f;
};

// Ticks off Object::m_Events, i.e. it does not need the AI to be alive. Watches
// her position while somebody is riding and closes the ride once she has not
// moved for five seconds, which no healthy ride does. This is the only part of
// the escape that has never failed to run.
class RaliaRideWatchdog : public BasicEvent
{
public:
    explicit RaliaRideWatchdog(Creature* ralia) : _ralia(ralia) { }

    bool Execute(uint64 /*time*/, uint32 diff) override
    {
        if (!_ralia || !_ralia->IsInWorld())
            return true;

        Vehicle* kit = _ralia->GetVehicleKit();
        Unit* passenger = kit ? kit->GetPassenger(0) : nullptr;
        if (!passenger || !passenger->IsPlayer())
            return true;    // nobody riding any more, stop watching

        float x = _ralia->GetPositionX();
        float y = _ralia->GetPositionY();
        float z = _ralia->GetPositionZ();

        if (!_hasLast)
        {
            _hasLast = true;
            _lastX = x;
            _lastY = y;
            _lastZ = z;
        }
        else if (_ralia->GetDistance(_lastX, _lastY, _lastZ) > 2.0f)
        {
            _lastX = x;
            _lastY = y;
            _lastZ = z;
            _stallMs = 0;
        }
        else
            _stallMs += diff;

        _elapsedMs += diff;

        if (_stallMs < 5000 && _elapsedMs < 150000)
            return false;   // still riding, keep watching

        TC_LOG_ERROR("server", "Ralia (156929): ride watchdog closing the scene at ({:.2f}, {:.2f}, {:.2f}) after {} ms.",
            x, y, z, _elapsedMs);

        if (npc_ralia_dreamchaser_q55639* ai = dynamic_cast<npc_ralia_dreamchaser_q55639*>(_ralia->AI()))
        {
            ai->ForceCloseRide();
            return true;
        }

        // the AI is gone entirely (which is what the ride keeps running into):
        // close the scene with nothing but the creature and the rider
        EndRaliaEscapeRide(_ralia, passenger->ToPlayer());
        return true;
    }

private:
    Creature* _ralia;
    float _lastX = 0.0f;
    float _lastY = 0.0f;
    float _lastZ = 0.0f;
    bool _hasLast = false;
    uint32 _stallMs = 0;
    uint32 _elapsedMs = 0;
};

void StartRaliaRideWatchdog(Creature* ralia)
{
    ralia->m_Events.AddEventAtOffset(new RaliaRideWatchdog(ralia), 1500ms);
}

// 156900 - Hrun the Exiled
//
// He is already mid-ritual when the player reaches the pit floor, so the drain
// leash has to be there before the fight starts, not after. Drain Spirit is his
// own ability (dungeon journal: "Drains health from an enemy over 5 sec,
// transferring it to the caster"), which means the client renders the beam and
// the shadow aura unaided - the script only has to keep re-casting it, because
// a 5 s channel is broken by the melee he is also doing.
struct npc_hrun_the_exiled_q55639 : public ScriptedAI
{
    npc_hrun_the_exiled_q55639(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _events.Reset();
        _raliaGUID.Clear();
        _saidLowHealth = false;

        if (me->IsAlive())
            _events.ScheduleEvent(EVENT_HRUN_DRAIN_LOOKUP, 1s);
    }

    void JustEngagedWith(Unit* who) override
    {
        sCreatureTextMgr->SendChat(me, 0, who, CHAT_MSG_MONSTER_SAY, LANG_UNIVERSAL);
    }

    void JustDied(Unit* killer) override
    {
        // stop draining first, otherwise the leash survives him for one tick
        _events.Reset();

        // "You will be kindling for the flame!" - his last act is the fire that
        // the escape ride is scripted against
        sCreatureTextMgr->SendChat(me, 2, killer, CHAT_MSG_MONSTER_SAY, LANG_UNIVERSAL);

        // dying frees Ralia, who immediately carries the player out
        if (Creature* ralia = me->FindNearestCreature(NPC_RALIA_DREAMCHASER_Q55639, 60.0f, true))
            ralia->AI()->DoAction(ACTION_HRUN_DIED);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_HRUN_DRAIN_LOOKUP:
                {
                    Creature* ralia = me->FindNearestCreature(NPC_RALIA_DREAMCHASER_Q55639, 40.0f, true);
                    if (!ralia)
                    {
                        _events.ScheduleEvent(EVENT_HRUN_DRAIN_LOOKUP, 2s);
                        break;
                    }

                    _raliaGUID = ralia->GetGUID();
                    DrainRalia();
                    break;
                }
                case EVENT_HRUN_DRAIN:
                {
                    DrainRalia();
                    break;
                }
                default:
                    break;
            }
        }

        if (!UpdateVictim())
            return;

        if (!_saidLowHealth && me->HealthBelowPct(35))
        {
            _saidLowHealth = true;
            sCreatureTextMgr->SendChat(me, 1, nullptr, CHAT_MSG_MONSTER_SAY, LANG_UNIVERSAL);
        }

        me->DoMeleeAttackIfReady();
    }

private:
    void DrainRalia()
    {
        Creature* ralia = ObjectAccessor::GetCreature(*me, _raliaGUID);
        if (!ralia || !ralia->IsAlive())
            return;

        // The drain is a performance: she has to survive it to carry the player
        // out, and the 7.5 damage a full cast deals would otherwise grind her
        // down while the player works their way to the bottom of the pit.
        if (ralia->GetHealthPct() < 80.0f)
            ralia->SetHealth(ralia->GetMaxHealth());

        me->CastSpell(ralia, SPELL_HRUN_DRAIN_SPIRIT, true);

        // channel is 5 s; re-cast a touch early so the beam never visibly gaps
        // and a channel broken by his melee is picked straight back up
        _events.ScheduleEvent(EVENT_HRUN_DRAIN, 4s);
    }

    EventMap _events;
    ObjectGuid _raliaGUID;
    bool _saidLowHealth = false;
};

// ---------------------------------------------------------------------------
// TEMPORARY diagnostic - remove once Garrick (156651) is settled.
//
// The NPC undeniably exists server side: FindNearestCreature returns it, the
// summon path logs a creature right under the player, phasing reports the same
// shift - and the client still draws nothing. HaveAtClient() is the server side
// mirror of the client's visible-object set, so it splits "the server never
// pushed it" from "it was pushed and the client refused to render it", which
// are completely different bugs. Every line is logged only when the answer
// changes, so this cannot flood the log.
// ---------------------------------------------------------------------------
class ExilesReachGarrickProbe : public WorldScript
{
public:
    ExilesReachGarrickProbe() : WorldScript("ExilesReachGarrickProbe") { }

    void OnUpdate(uint32 diff) override
    {
        _timer += diff;

        if (_timer < 5000)
            return;

        _timer = 0;

        Map* map = sMapMgr->FindMap(2175, 0);

        if (!map)
            return;

        map->DoOnPlayers([this](Player* player)
        {
            if (!player->IsInWorld())
                return;

            Creature* garrick = player->FindNearestCreature(NPC_CAPTAIN_GARRICK_Q55639, 60.0f);

            if (!garrick)
            {
                if (_missingReported)
                    return;

                _missingReported = true;
                _haveLast = false;

                TC_LOG_INFO("server", "Garrick probe: no creature {} within 60 yd of {}.", NPC_CAPTAIN_GARRICK_Q55639, player->GetName());
                return;
            }

            _missingReported = false;

            bool atClient = player->HaveAtClient(garrick);

            if (_haveLast && _lastGuid == garrick->GetGUID() && _lastAtClient == atClient)
                return;

            _haveLast = true;
            _lastGuid = garrick->GetGUID();
            _lastAtClient = atClient;

            TC_LOG_INFO("server", "Garrick probe: {} display {} in world {} dist {:.1f} | can see {} | at client {} | phases player {} / npc {} | same phase {}.",
                garrick->GetGUID().ToString(), garrick->GetDisplayId(),
                garrick->IsInWorld() ? 1 : 0, player->GetDistance(garrick),
                player->CanSeeOrDetect(garrick) ? 1 : 0, atClient ? 1 : 0,
                player->GetPhaseShift().GetPhases().size(),
                garrick->GetPhaseShift().GetPhases().size(),
                garrick->InSamePhase(player) ? 1 : 0);

            // Which single check inside CanSeeOrDetect() is rejecting it.
            TC_LOG_INFO("server", "Garrick probe detail: private {} ownerVis {} conditions {} spawnTracking {} alive {} timeSync {}.",
                garrick->IsPrivateObject() ? 1 : 0,
                garrick->CheckPrivateObjectOwnerVisibility(player) ? 1 : 0,
                sConditionMgr->IsObjectMeetingVisibilityByObjectIdConditions(garrick, player) ? 1 : 0,
                garrick->GetSpawnTrackingStateDataForPlayer(player) ? 1 : 0,
                garrick->IsAlive() ? 1 : 0,
                player->HasPlayerLocalFlag(PLAYER_LOCAL_FLAG_OVERRIDE_TRANSPORT_SERVER_TIME) ? 1 : 0);
        });
    }

private:
    uint32 _timer = 0;
    bool _missingReported = false;
    bool _haveLast = false;
    bool _lastAtClient = false;
    ObjectGuid _lastGuid;
};

// ***************************************************************
// * A Druid's Form (59350 / 59951)                              *
// ***************************************************************
//
// The quest asks the player to "Commune with the Druid Stone" and its only
// objective is monster 174832 - a scene kill credit, not a real spawn. The
// world DB has no spawn row for the stone (gameobject 346113 exists in
// gameobject_template, but nothing places it anywhere), there is no
// areatrigger and no scene_template record on map 2175, and nothing in this
// file ever touches 174832. In game that reads exactly like the quest arrow
// does: a patch of empty ground where standing does nothing.
//
// The stone itself is a client-side prop, so there is nothing to click server
// side. The objective is therefore handed out by proximity. These three spots
// are the quest's own POI points (quest_poi_points for 59350/59951).
namespace
{
    constexpr uint32 QUEST_A_DRUIDS_FORM_ALLIANCE   = 59350;
    constexpr uint32 QUEST_A_DRUIDS_FORM_HORDE      = 59951;
    constexpr uint32 OBJECTIVE_COMMUNE_DRUID_STONE  = 396388;
    constexpr uint32 NPC_DRUID_STONE_CREDIT         = 174832;
    constexpr float  DRUID_STONE_RANGE              = 14.0f;

    Position const DruidStoneSpots[] =
    {
        { 321.0f, -2057.0f, 112.0f, 0.0f },
        { 323.0f, -2052.0f, 113.0f, 0.0f },
        { 182.0f, -2288.0f,  82.0f, 0.0f },
    };
}

class ExilesReachDruidStone : public WorldScript
{
public:
    ExilesReachDruidStone() : WorldScript("ExilesReachDruidStone") { }

    void OnUpdate(uint32 diff) override
    {
        _timer += diff;

        if (_timer < 1000)
            return;

        _timer = 0;

        Map* map = sMapMgr->FindMap(2175, 0);

        if (!map)
            return;

        map->DoOnPlayers([](Player* player)
        {
            if (!player->IsInWorld())
                return;

            uint32 quest = player->GetQuestStatus(QUEST_A_DRUIDS_FORM_ALLIANCE) == QUEST_STATUS_INCOMPLETE
                ? QUEST_A_DRUIDS_FORM_ALLIANCE
                : QUEST_A_DRUIDS_FORM_HORDE;

            if (player->GetQuestStatus(quest) != QUEST_STATUS_INCOMPLETE)
                return;

            if (player->GetQuestObjectiveData(quest, OBJECTIVE_COMMUNE_DRUID_STONE) > 0)
                return;

            for (Position const& spot : DruidStoneSpots)
            {
                if (player->GetDistance(spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ()) > DRUID_STONE_RANGE)
                    continue;

                player->KilledMonsterCredit(NPC_DRUID_STONE_CREDIT);

                TC_LOG_INFO("server", "Druid Stone: {} communed at ({:.1f}, {:.1f}, {:.1f}) for quest {}.",
                    player->GetName(), player->GetPositionX(), player->GetPositionY(), player->GetPositionZ(), quest);

                break;
            }
        });
    }

private:
    uint32 _timer = 0;
};

void AddSC_zone_exiles_reach()
{
    // Ship
    RegisterSpellScript(spell_attention_exiles_reach_tutorial);
    new q59926_warming_up();
    new q56775_warming_up();
    new quest_stand_your_ground();
    RegisterCreatureAI(npc_sparring_partner_exiles_reach);
    RegisterSpellScript(spell_summon_sparring_partner);
    new FactoryCreatureScript<CreatureAI, &CaptainGarrickAISelector>("npc_captain_garrick");
    RegisterPrivatePublicCreatureAIPair("npc_warlord_grimaxe_lower_ship", npc_ship_captain_warming_up_private, NullCreatureAI);
    RegisterPrivatePublicCreatureAIPair("npc_warlord_grimaxe_upper_ship", npc_ship_captain_brace_for_impact_private, NullCreatureAI);
    RegisterPrivatePublicCreatureAIPair("npc_cole_ship", npc_first_mate_brace_for_impact_private, npc_first_mate_stand_your_ground);
    RegisterPrivatePublicCreatureAIPair("npc_throg_ship", npc_first_mate_brace_for_impact_private, npc_first_mate_stand_your_ground);
    RegisterPrivatePublicCreatureAIPair("npc_crew_ship", npc_crew_ship_private, NullCreatureAI);
    RegisterPrivatePublicCreatureAIPair("npc_pet_ship", npc_pet_ship_private, NullCreatureAI);
    new quest_brace_for_impact();
    new player_exiles_reach_ship_crash();
    new scene_alliance_and_horde_ship();
    // Beach
    RegisterSpellScript(spell_knocked_down_exiles_reach_beach);
    new scene_alliance_and_horde_crash();
    RegisterSpellScriptWithArgs(spell_crash_landed_generic<NPC_CAPTAIN_GARRICK_BEACH>, "spell_crash_landed_alliance");
    RegisterSpellScriptWithArgs(spell_crash_landed_generic<NPC_WARLORD_BREKA_GRIMAXE_BEACH>, "spell_crash_landed_horde");
    new FactoryCreatureScript<CreatureAI, &CaptainGarrickBeachAISelector>("npc_captain_garrick_beach");
    new FactoryCreatureScript<CreatureAI, &WarlordGrimaxeBeachAISelector>("npc_warlord_grimaxe_beach");
    new FactoryCreatureScript<CreatureAI, &HealedByLeaderAllianceAISelector>("npc_healed_by_leader_alliance_beach");
    new FactoryCreatureScript<CreatureAI, &HealedByLeaderHordeAISelector>("npc_healed_by_leader_horde_beach");
    new GenericCreatureScript<npc_alliance_survivors_beach_laying<CONVERSATION_STOUTHANDS_BEACH>>("npc_bjorn_stouthands_beach_laying");
    new GenericCreatureScript<npc_alliance_survivors_beach_laying<CONVERSATION_HUXWORTH_BEACH>>("npc_austin_huxworth_beach_laying");
    new GenericCreatureScript<npc_alliance_survivors_beach_laying<CONVERSATION_KEE_LA_BEACH>>("npc_kee_la_beach_laying");
    // Note: alliance survivor do not need a script for Emergency First Aid quest
    RegisterCreatureAI(npc_bo_beach_laying);
    RegisterCreatureAI(npc_mithran_dawntracker_beach_laying);
    RegisterCreatureAI(npc_lana_jordan_beach_laying);
    RegisterCreatureAI(npc_murloc_spearhunter_watershaper_higher_ground);
    new FactoryCreatureScript<CreatureAI, &BoBeachStandingAISelector>("npc_bo_beach_standing");
    new FactoryCreatureScript<CreatureAI, &MithdranBeachStandingAISelector>("npc_mithdran_dawntracker_beach_standing");
    new FactoryCreatureScript<CreatureAI, &LanaJordanBeachStandingAISelector>("npc_lana_jordan_beach_standing");
    new FactoryCreatureScript<CreatureAI, &KeeLaBeachStandingAISelector>("npc_kee_la_beach_standing");
    new FactoryCreatureScript<CreatureAI, &BjornBeachStandingAISelector>("npc_bjorn_stouthands_beach_standing");
    new FactoryCreatureScript<CreatureAI, &AustinBeachStandingAISelector>("npc_austin_huxworth_beach_standing");
    RegisterCreatureAI(npc_garrick_summoned_beach);
    RegisterCreatureAI(npc_grimaxe_summoned_beach);
    new quest_finding_the_lost_expedition_alliance();
    new quest_finding_the_lost_expedition_horde();
    RegisterSpellScript(spell_summon_survivor_beach);
    // Abandoned Camp
    new quest_cooking_meat_alliance();
    new quest_cooking_meat_horde();
    RegisterAreaTriggerAI(areatrigger_find_the_lost_expedition);
    RegisterAreaTriggerAI(areatrigger_find_the_lost_expedition_follower);
    // Quest Enhanced Combat Tactics
    new quest_enhanced_combat_tactics();
    RegisterSpellScript(spell_summon_combat_trainer);
    new FactoryCreatureScript<CreatureAI, &SparringPartnerEnhancedCombatTrainingSelector>("npc_sparring_partner_combat_training");
    RegisterAreaTriggerAI(at_aggro_radius_check_enhanced_combat_tactics);
    RegisterSpellScript(spell_knockback_charge_enhanced_training);
    // Quest Northbound
    RegisterCreatureAI(npc_leader_northbound);
    new quest_northbound_alliance();
    new quest_northbound_horde();
    RegisterSpellScript(spell_summon_leader_northbound);
    RegisterAreaTriggerAI(at_northbound_linger);
    RegisterSpellScript(spell_scene_linger_northbound);
    // Quest Taming The Wilds
    new quest_taming_the_wilds_alliance();
    new quest_taming_the_wilds_horde();
    new FactoryCreatureScript<CreatureAI, &HuxsworthBriarpatchSelector>("npc_huxsworth_briarpatch");
    new FactoryCreatureScript<CreatureAI, &DawntrackerBriarpatchSelector>("npc_dawntracker_briarpatch");
    RegisterSpellScript(spell_tutorial_health_dnt_proc_aura);
    RegisterSpellScript(spell_tutorial_health_dnt);
    // Briarpatch
    new quest_briarpatch_alliance();
    new quest_briarpatch_horde();
    RegisterSpellScript(spell_validated_quest_accept_briarpatch_alliance);
    RegisterSpellScript(spell_validated_quest_accept_briarpatch_horde);
    RegisterCreatureAI(npc_geolord_grekog);
    new GenericCreatureScript<npc_briarpatch_prisoner>("npc_cork_fizzlepop_briarpatch");
    new GenericCreatureScript<npc_briarpatch_prisoner>("npc_lindie_springstock_briarpatch");
    RegisterCreatureAI(npc_quilboar_warrior);
    RegisterCreatureAI(npc_quilboar_geomancer);
    RegisterCreatureAI(npc_ogre_overseer);
    RegisterAreaTriggerAI(at_briarpatch_to_plains);
    RegisterSpellScript(spell_quilboar_sleep_dnt);
    // Plains scouting quest
    new quest_scout_o_matic_5000();
    new quest_choppy_booster_mk5();
    new FactoryCreatureScript<CreatureAI, &LindieSpringstockSelector>("npc_lindie_springstock_plains");
    new FactoryCreatureScript<CreatureAI, &CorkFizzlepopSelector>("npc_cork_fizzlepop_plains");
    RegisterCreatureAI(npc_scoutomatic_5000);
    new FactoryCreatureScript<CreatureAI, &ChoppyBoosterSelector>("npc_choppy_booster");
    new FactoryCreatureScript<CreatureAI, &HordeCrewPlainsSelector>("npc_horde_crew_plains");
    // Plains Resizing the situation
    new quest_resizing_the_situation_alliance();
    new quest_resizing_the_situation_horde();
    RegisterCreatureAI(npc_lindie_springstock_q56034);
    RegisterCreatureAI(npc_cork_fizzlepop_q59941);
    RegisterCreatureAI(npc_re_sized_boar_q56034);
    RegisterSpellScript(spell_summon_guardian_q56034_q59941);
    RegisterSpellAndAuraScriptPair(spell_re_sizing_q56034, spell_re_sizing_aura_q56034);
    RegisterSpellScript(spell_resizer_hit_one_two_q56034_q59941);
    RegisterSpellScript(spell_resizer_hit_three_q56034);
    RegisterSpellScript(spell_re_sizing_q59941);
    RegisterSpellScript(spell_re_sizing_aura_q59941);
    RegisterSpellScript(spell_re_sizer_slaughter);
    // Ride Boar
    new quest_ride_of_the_scientifically_enhanced_boar();
    new scene_darkmaul_plains_skeleton_army_alliance();
    RegisterSpellScript(spell_summon_darkmaul_plains_questgivers_q55879);
    RegisterSpellScript(spell_riding_giant_boar_q55879);
    RegisterSpellScript(spell_knockback_hint_q56034);
    RegisterCreatureAI(npc_captain_garrick_q55879);
    RegisterCreatureAI(npc_giant_boar_vehicle_q55879);
    RegisterCreatureAI(npc_giant_boar_display_q55879);
    RegisterCreatureAI(npc_torgok_q55879);
    new FactoryCreatureScript<CreatureAI, &PrisonerQ55879Selector>("npc_prisoner_q55879");
    // The Re-Deather
    new quest_the_re_deather();
    new scene_darkmaul_plains_skeleton_army_horde();
    RegisterCreatureAI(npc_warlord_grimaxe_q59942);
    // Westward Bound
    new quest_westward_bound_alliance();
    new quest_westward_bound_horde();
    new FactoryCreatureScript<CreatureAI, &BjornRuinsSelector>("npc_bjorn_stouthands_q55965");
    new FactoryCreatureScript<CreatureAI, &LanaRuinsSelector>("npc_lana_jordan_q59948");
    new FactoryCreatureScript<CreatureAI, &AlariaRuinsSelector>("npc_alaria_q55965");
    new FactoryCreatureScript<CreatureAI, &WansaRuinsSelector>("npc_wonsa_q59948");
    // Who Lurks in the Pit
    RegisterCreatureAI(npc_ralia_dreamchaser_q55639);
    RegisterCreatureAI(npc_hrun_the_exiled_q55639);
    // TEMPORARY Garrick visibility diagnostic
    new ExilesReachGarrickProbe();
    // A Druid's Form - the stone has no spawn row, so the objective is
    // credited by proximity (see the class comment).
    new ExilesReachDruidStone();
}
