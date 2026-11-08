#include "apps/openmw/mwworld/oblivionactorstats.hpp"
#include "apps/openmw/mwmechanics/oblivionplayerhostility.hpp"
#include <components/esm4/loadbsgn.hpp>
#include <utility>
#include "oblivionworldfixture.hpp"
#include "inventorywindowmanager.hpp"

#include "apps/openmw/mwclass/esm4npc.hpp"
#include "apps/openmw/mwclass/esm4interactive.hpp"
#include <components/esm4/loadrefr.hpp>
#include "apps/openmw/mwclass/npc.hpp"
#include "apps/openmw/mwclass/misc.hpp"
#include "apps/openmw/mwclass/potion.hpp"
#include <components/esm4/loadalch.hpp>
#include <components/esm3/loadalch.hpp>
#include "apps/openmw/mwclass/armor.hpp"
#include "apps/openmw/mwclass/weapon.hpp"
#include "apps/openmw/mwworld/containerstore.hpp"
#include "apps/openmw/mwworld/manualref.hpp"
#include "apps/openmw/mwdialogue/dialoguemanagerimp.hpp"
#include "apps/openmw/mwmechanics/oblivionai.hpp"
#include "apps/openmw/mwmechanics/mechanicsmanagerimp.hpp"
#include "apps/openmw/mwmechanics/movement.hpp"
#include "apps/openmw/mwmechanics/disease.hpp"
#include "apps/openmw/mwmechanics/oblivioncombat.hpp"
#include "apps/openmw/mwmechanics/oblivionmelee.hpp"
#include "apps/openmw/mwphysics/physicssystem.hpp"
#include "apps/openmw/mwworld/cellstore.hpp"
#include "apps/openmw/mwworld/esmstore.hpp"
#include "apps/openmw/mwworld/oblivionscriptmanager.hpp"
#include "apps/openmw/mwworld/globals.hpp"
#include "apps/openmw/mwstate/statemanagerimp.hpp"
#include <components/settings/values.hpp>
#include <components/esm3/dialoguestate.hpp>
#include <components/esm3/esmreader.hpp>
#include <components/esm3/esmwriter.hpp>
#include <components/loadinglistener/loadinglistener.hpp>
#include <sstream>
#include "apps/openmw/mwworld/worldmodel.hpp"

#include <components/compiler/extensions.hpp>
#include <components/esm4/common.hpp>
#include <components/esm4/loadclas.hpp>
#include <components/esm4/loadcsty.hpp>
#include <components/esm4/loadcell.hpp>
#include <components/esm4/loadglob.hpp>
#include <components/esm4/loadgmst.hpp>
#include <components/esm4/loadskil.hpp>
#include <components/esm3/loadrace.hpp>
#include <components/esm3/loadclas.hpp>
#include "apps/openmw/mwworld/oblivioncharactergen.hpp"
#include "apps/openmw/mwworld/obliviondooranimation.hpp"
#include "apps/openmw/mwworld/oblivionenablestate.hpp"
#include "apps/openmw/mwworld/oblivionbounditems.hpp"
#include "apps/openmw/mwworld/oblivionprofileservices.hpp"
#include <components/esm4/loadmgef.hpp>
#include <components/esm4/loadspel.hpp>
#include <components/esm4/loadarmo.hpp>
#include <components/esm4/loadweap.hpp>
#include <components/esm4/magiceffectid.hpp>
#include <components/esm4/inventorymechanics.hpp>
#include <components/esm4/referenceflags.hpp>
#include "apps/openmw/mwworld/player.hpp"
#include <components/esm4/loadfact.hpp>
#include <components/esm4/loadachr.hpp>
#include <components/esm4/loadinfo.hpp>
#include <components/esm4/loaddial.hpp>
#include <components/esm4/loadnpc.hpp>
#include <components/esm4/loadpack.hpp>
#include <components/esm4/loadqust.hpp>
#include <components/esm4/loadrace.hpp>
#include <components/translation/translation.hpp>
#include <components/vfs/filesystemarchive.hpp>
#include <components/sceneutil/positionattitudetransform.hpp>
#include <components/sceneutil/lightmanager.hpp>
#include <components/sceneutil/shadow.hpp>
#include <components/resource/scenemanager.hpp>
#include <components/shader/shadermanager.hpp>

#include <osg/Geode>
#include <osg/Geometry>
#include <osgDB/WriteFile>

#include <gtest/gtest.h>

#include <functional>
#include <tuple>
#include <cmath>
#include <limits>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    template<class T> std::string bytes(const T& value)
    {
        return {reinterpret_cast<const char*>(&value), sizeof(value)};
    }
    std::string sub(std::uint32_t tag, const std::string& data)
    {
        return bytes(tag) + bytes(static_cast<std::uint16_t>(data.size())) + data;
    }
    std::string record(std::uint32_t tag, std::uint32_t id, const std::string& data)
    {
        return bytes(tag) + bytes(static_cast<std::uint32_t>(data.size())) + bytes(0u) + bytes(id) + bytes(0u) + data;
    }
    std::string topic(std::uint32_t id, const std::string& name, const std::string& label)
    {
        return record(ESM4::REC_DIAL, id, sub(ESM::fourCC("EDID"), name + '\0')
            + sub(ESM::fourCC("FULL"), label + '\0') + sub(ESM::fourCC("DATA"), std::string(1, '\0')));
    }
    std::string response(std::uint32_t id, unsigned char flags, std::uint32_t next = 0, int stage = -1)
    {
        auto data = sub(ESM::fourCC("DATA"), std::string{char(0), char(0), char(flags)})
            + sub(ESM::fourCC("QSTI"), bytes(0x900u));
        ESM4::TargetResponseData text{};
        text.responseNo = 1;
        data += sub(ESM::fourCC("TRDT"), bytes(text).substr(0, 16))
            + sub(ESM::fourCC("NAM1"), std::string("First response\0", 15));
        if (next)
        {
            text.responseNo = 2;
            data += sub(ESM::fourCC("TRDT"), bytes(text).substr(0, 16))
                + sub(ESM::fourCC("NAM1"), std::string("Second response\0", 16))
                + sub(ESM::fourCC("TCLT"), bytes(next))
                + sub(ESM::fourCC("TCLT"), bytes(0x905u));
        }
        if (stage >= 0)
        {
            ESM4::AIPackage::CTDA condition{};
            condition.fnIndex = 58;
            condition.param1 = 0x900;
            condition.compValue = static_cast<float>(stage);
            data += sub(ESM::fourCC("CTDA"), bytes(condition));
        }
        data += sub(ESM::fourCC("SCTX"), next ? "setstage MenuQuest 20\n" : "setstage MenuQuest 30\n");
        return record(ESM4::REC_INFO, id, data);
    }
    std::string group(std::uint32_t parent, const std::string& info)
    {
        return bytes(ESM::fourCC("GRUP")) + bytes(static_cast<std::uint32_t>(20 + info.size()))
            + bytes(parent) + bytes(7u) + bytes(0u) + info;
    }
    std::string menuRecords(unsigned char flags)
    {
        return record(ESM4::REC_QUST, 0x900, sub(ESM::fourCC("EDID"), std::string("MenuQuest\0", 10))
            + sub(ESM::fourCC("DATA"), std::string{char(1), char(50)}))
            + topic(0x901, "GREETING", "Greeting") + group(0x901, response(0x902, flags, 0x903))
            + topic(0x903, "ContinueTopic", "Continue through the passage")
            + group(0x903, response(0x904, ESM4::INFO_Goodbye, 0, 20))
            + topic(0x905, "HiddenTopic", "This question must stay hidden")
            + group(0x905, response(0x906, ESM4::INFO_Goodbye, 0, 99))
            + record(ESM4::REC_MISC, 0xf, sub(ESM::fourCC("EDID"), std::string("Gold001\0", 8))
                + sub(ESM::fourCC("DATA"), bytes(1u) + bytes(0.f)));
    }
    ESM::FormKey key(std::uint32_t id) { return ESM::FormKey::content("headless.esm", id); }

    std::string hitSourceRecords()
    {
        const auto children = [](unsigned kind, const std::string& data, unsigned label = 0xb20) {
            return bytes(ESM::fourCC("GRUP")) + bytes(static_cast<std::uint32_t>(20 + data.size()))
                + bytes(label) + bytes(kind) + bytes(0u) + data;
        };
        const std::array<float, 6> position{0,-60,0,0,0,0};
        return children(0, record(ESM4::REC_CELL, 0xb20,
            sub(ESM::fourCC("EDID"), std::string("HitTestRoom\0", 12))
            + sub(ESM::fourCC("DATA"), bytes(std::uint16_t{ESM4::CELL_Interior}))), ESM4::REC_CELL)
            + children(6, children(9, record(ESM4::REC_ACHR, 0xa01,
                sub(ESM::fourCC("EDID"), std::string("HitSourceRef\0", 13))
                + sub(ESM::fourCC("NAME"), bytes(0x800u))
                + sub(ESM::fourCC("DATA"), bytes(position)))));
    }

    struct Callback : MWBase::DialogueManager::ResponseCallback
    {
        std::vector<std::string> mResponses;
        std::function<void()> mObserver;
        void addResponse(std::string_view, std::string_view text) override
        {
            mResponses.emplace_back(text);
            if (mObserver) mObserver();
        }
    };

    struct MenuFixture;
    void prepareSaveableMenuPlayer(MenuFixture& f);

    struct MenuFixture
    {
        TestingOpenMW::NativeWorldFixture mFixture;
        Compiler::Extensions mExtensions;
        Translation::Storage mTranslation;
        MWDialogue::DialogueManager mMenu{mExtensions, mTranslation};
        MWWorld::Ptr mActor;
        explicit MenuFixture(unsigned char flags = ESM4::INFO_SayOnce, bool shifted = false,
            const std::string& hitScript = {})
            : mFixture(shifted, false, !hitScript.empty(), hitScript, {}, menuRecords(flags)
                + (hitScript.empty() ? std::string{} : hitSourceRecords()), hitScript.empty() ? 8 : 10)
        {
            auto& world = mFixture.mWorld;
            MWClass::Npc::registerSelf();
            MWClass::Miscellaneous::registerSelf();
            MWClass::ESM4Npc::registerSelf();
            world.setupPlayer();
            const auto* base = world.getStore().search<ESM4::Npc>(key(0x800));
            ESM4::Race race{};
            race.mId = {0x810, base->mId.mContentFile};
            race.mAttribMale = {40, 40, 40, 40, 40, 40, 40, 40};
            race.mAttribFemale = race.mAttribMale;
            world.getStore().getWritable<ESM4::Race>().insertStatic(race, key(0x810));
            auto npc = *base;
            npc.mRace = race.mId;
            world.getStore().getWritable<ESM4::Npc>().insertStatic(npc, key(0x800));
            base = world.getStore().search<ESM4::Npc>(key(0x800));
            ESM4::ActorCharacter placed{};
            placed.mId = {0xa00, base->mId.mContentFile};
            placed.mFormKey = key(0xa00);
            placed.mBaseObj = base->mId;
            placed.mBaseKey = key(0x800);
            // Native history needs a real saved reference, not a draft-only Ptr.
            ESM4::Cell nativeCell{};
            nativeCell.mId = ESM::RefId(ESM::FormId{0xb32, base->mId.mContentFile});
            nativeCell.mFormKey = key(0xb32);
            nativeCell.mCellFlags = ESM4::CELL_Interior;
            nativeCell.mEditorId = "DialogueMenuRoom";
            world.getStore().getWritable<ESM4::Cell>().insertStatic(nativeCell, nativeCell.mFormKey);
            placed.mParent = nativeCell.mId;
            placed.mParentKey = nativeCell.mFormKey;
            world.getStore().getWritable<ESM4::ActorCharacter>().insertStatic(placed, placed.mFormKey);
            MWWorld::LiveCellRef<ESM4::Npc> live(placed, base);
            auto& cell = world.getWorldModel().getCell(nativeCell.mId);
            mActor = MWWorld::Ptr(cell.insert(&live), &cell);
            world.getWorldModel().registerPtr(mActor);
            world.getPlayer().setCell(&cell);
            if (!world.activateOblivionActor(mActor)) throw std::runtime_error("cannot activate menu actor");
            prepareSaveableMenuPlayer(*this);
        }
        int stage() const
        {
            ESM4::RuntimeState state;
            mFixture.mWorld.getOblivionScriptManager()->capture(state);
            for (const auto& quest : state.mQuests)
                if (quest.mQuest == key(0x900)) return quest.mStage;
            return -1;
        }
        std::uint64_t sequence() const
        {
            ESM4::RuntimeState state;
            mFixture.mWorld.getOblivionScriptManager()->capture(state);
            return state.mScriptEventSequence;
        }
    };
    void prepareSaveableMenuPlayer(MenuFixture& f)
    {
        auto& world = f.mFixture.mWorld;
        auto& store = world.getStore();
        const auto file = f.mActor.get<ESM4::Npc>()->mBase->mId.mContentFile;
        ESM4::Class cls{};
        cls.mId = {0xb30, file};
        cls.mData.mFavoredAttributes = {0, 1};
        for (std::size_t i = 0; i < 7; ++i) cls.mData.mMajorSkills[i] = 12 + i;
        store.getWritable<ESM4::Class>().insertStatic(cls, key(0xb30));
        ESM::Race sharedRace; sharedRace.blank(); sharedRace.mId = ESM::RefId(ESM::FormId{0x810, file});
        store.getWritable<ESM::Race>().insertStatic(sharedRace);
        ESM::Class sharedClass; sharedClass.blank(); sharedClass.mId = ESM::RefId(cls.mId);
        store.getWritable<ESM::Class>().insertStatic(sharedClass);
        auto playerBase = *world.getPlayerPtr().get<ESM::NPC>()->mBase;
        playerBase.mRace = sharedRace.mId; playerBase.mClass = sharedClass.mId;
        auto prepared = store.preparePlayerRecord(playerBase);
        world.getPlayer().set(prepared.commit());
        ESM4::Cell cell{};
        cell.mId = ESM::RefId(ESM::FormId{0xb31, file});
        cell.mFormKey = key(0xb31);
        cell.mEditorId = "CrimeGoldRoom";
        cell.mCellFlags = ESM4::CELL_Interior;
        store.getWritable<ESM4::Cell>().insertStatic(cell, cell.mFormKey);
        auto& room = world.getWorldModel().getCell(cell.mId);
        f.mActor = f.mActor.getCell()->moveTo(f.mActor, &room);
        world.getWorldModel().registerPtr(f.mActor);
        world.getPlayer().setCell(&room);
        auto source = *f.mActor.get<ESM4::Npc>()->mBase;
        source.mId = {7, file + 1}; source.mFormKey = ESM::FormKey::content("Oblivion.esm", 7);
        store.getWritable<ESM4::Npc>().insertStatic(source, source.mFormKey);
        if (!world.initializeOblivionPlayerActor()) throw std::runtime_error("cannot initialize saveable menu Player");
    }

}

TEST(OblivionDialogueMenu, ScriptSetCrimeGoldRoutesActorBountyAndPreservesFractionalDeltaAcrossSave)
{
    for (bool shifted : {false, true})
    {
        MenuFixture f(ESM4::INFO_SayOnce, shifted);
        auto& world = f.mFixture.mWorld;
        auto& store = world.getStore();
        auto& combat = *world.getOblivionCombatService();
        auto& scripts = *world.getOblivionScriptManager();
        const auto settings = MWWorld::resolveOblivionPlayerDynamicBaseSettings(store);
        for (int scenario : {0, 1, 2}) // NPC, Player normal realm, Player alternate realm.
        {
            SCOPED_TRACE(scenario);
            const bool player = scenario != 0;
            const auto actor = player ? world.getPlayerPtr() : f.mActor;
            const auto actorKey = player ? ESM::FormKey::dynamic("player", 1) : actor.getCellRef().getFormKey();
            auto values = *combat.findActorValues(actorKey);
            values.mBounty = ESM4::CrimeBountyState{.25f, player ? .5f : 0.f};
            values.mPlayerInShiveringIsles = scenario == 2;
            values.mValues[37].mModifiers = {.5f, -3.25f, -.25f};
            if (player) combat.publishPlayerValues(world.getPlayer(), values, settings);
            else combat.publishNonPlayerValues(actor, values);
            ObScript::RuntimeContext context; context.mSelf = actorKey;
            scripts.call("SetCrimeGold", {}, {std::int64_t{10}}, context, {});
            const auto* changed = combat.findActorValues(actorKey);
            ASSERT_TRUE(changed && changed->mBounty);
            EXPECT_EQ(changed->mValues[37].mModifiers, values.mValues[37].mModifiers);
            EXPECT_EQ(changed->mBounty->mNormal, scenario == 2 ? .25f : 9.25f);
            EXPECT_EQ(changed->mBounty->mShiveringIsles, scenario == 2 ? 9.5f : player ? .5f : 0.f);
            EXPECT_DOUBLE_EQ(ObScript::asNumber(scripts.call("GetCrimeGold", {}, {}, context, {})),
                scenario == 2 ? 9.5 : 9.25);
            const auto snapshot = world.captureOblivionRuntimeState();
            const auto restored = ESM4::RuntimeState::deserializeBinary(snapshot.serializeBinary());
            const auto saved = std::ranges::find(restored.mNativeActorValues, actorKey,
                &ESM4::RuntimeActorValues::mActor);
            ASSERT_NE(saved, restored.mNativeActorValues.end());
            EXPECT_EQ(saved->mBounty, changed->mBounty);
            scripts.call("SetCrimeGold", {}, {std::int64_t{-5}}, context, {});
            EXPECT_EQ(combat.crimeBounty(actorKey), scenario == 2 ? -5.f : 0.f);
            EXPECT_THROW(scripts.call("SetCrimeGold", {}, {}, context, {}), ObScript::RuntimeError);
            EXPECT_THROW(scripts.call("SetCrimeGold", {}, {std::int64_t{1}, std::int64_t{2}}, context, {}),
                ObScript::RuntimeError);
        }
    }
}

TEST(OblivionDialogueMenu, FactionRankDifferenceUsesSecondActorAndZeroForMissingMembership)
{
    for (const bool shifted : {false, true})
    {
        MenuFixture f(ESM4::INFO_SayOnce, shifted);
        auto& world = f.mFixture.mWorld;
        auto& store = world.getStore();
        const auto base = *f.mActor.get<ESM4::Npc>()->mBase;
        ESM4::Faction faction{};
        faction.mId = {0xd00, base.mId.mContentFile}; faction.mFormKey = key(0xd00);
        faction.mFactionFlags = 0;
        store.getWritable<ESM4::Faction>().insertStatic(faction, faction.mFormKey);
        auto peer = base; peer.mId = {0xd01, base.mId.mContentFile}; peer.mFormKey = key(0xd01);
        store.getWritable<ESM4::Npc>().insertStatic(peer, peer.mFormKey);
        ESM4::ActorCharacter placed{};
        placed.mId = {0xd03, base.mId.mContentFile}; placed.mFormKey = key(0xd03);
        placed.mBaseObj = peer.mId; placed.mBaseKey = peer.mFormKey;
        store.getWritable<ESM4::ActorCharacter>().insertStatic(placed, placed.mFormKey);
        auto& scripts = *world.getOblivionScriptManager();
        ObScript::RuntimeContext context; context.mSelf = f.mActor.getCellRef().getFormKey();
        ESM4::PackageCondition condition{};
        condition.mFunction = 60;
        condition.mParameter1.mReferenceKey = faction.mFormKey;
        condition.mParameter2.mReferenceKey = placed.mFormKey;
        condition.mOperator = ESM4::ConditionOperator::Equal;
        for (const auto [selfRank, peerRank, expected] : std::vector<std::tuple<int,int,int>>{
                 {5,2,3}, {2,5,-3}, {-1,5,0}, {5,-1,0}, {-1,-1,0}, {-2,3,-5}})
        {
            SCOPED_TRACE(selfRank);
            SCOPED_TRACE(peerRank);
            ASSERT_TRUE(world.setOblivionFactionRank(context.mSelf, faction.mFormKey, selfRank));
            ASSERT_TRUE(world.setOblivionFactionRank(placed.mFormKey, faction.mFormKey, peerRank));
            const std::vector<ObScript::Value> args{
                ObScript::ReferenceValue{faction.mFormKey, {}}, ObScript::ReferenceValue{placed.mFormKey, {}}};
            EXPECT_EQ(ObScript::asInteger(scripts.call("GetFactionRankDifference", {}, args, context, {})), expected);
            condition.mComparisonValue = expected;
            EXPECT_EQ(world.getOblivionAiService()->evaluateDialogueConditions(f.mActor, world.getPlayerPtr(),
                std::span(&condition, 1)), ESM4::ConditionResult::True);
        }
        condition.mParameter2.mReferenceKey = key(0xdead);
        condition.mComparisonValue = 0;
        EXPECT_EQ(world.getOblivionAiService()->evaluateDialogueConditions(f.mActor, world.getPlayerPtr(),
            std::span(&condition,1)), ESM4::ConditionResult::True);
    }
}

TEST(OblivionDialogueMenu, ScriptFactionRankSharesBaseAndRestoresMembershipWithoutEditingContent)
{
    for (bool shifted : {false, true})
    {
        MenuFixture f(ESM4::INFO_SayOnce, shifted);
        auto& world = f.mFixture.mWorld;
        auto& store = world.getStore();
        auto& combat = *world.getOblivionCombatService();
        auto& scripts = *world.getOblivionScriptManager();
        auto base = *f.mActor.get<ESM4::Npc>()->mBase;
        ESM4::Faction existing{};
        existing.mId = {0xd04, base.mId.mContentFile}; existing.mFormKey = key(0xd04);
        existing.mFactionFlags = 0;
        store.getWritable<ESM4::Faction>().insertStatic(existing, existing.mFormKey);
        base.mFactions = {{existing.mId.toUint32(), 2, 0, 0, 0}};
        base.mFaction = base.mFactions.front();
        store.getWritable<ESM4::Npc>().insertStatic(base, base.mFormKey);
        const auto file = base.mId.mContentFile;
        ESM4::Faction faction{};
        faction.mId = {0xd00, file}; faction.mFormKey = key(0xd00); faction.mFactionFlags = 0;
        store.getWritable<ESM4::Faction>().insertStatic(faction, faction.mFormKey);
        auto otherBase = base; otherBase.mId = {0xd01, file}; otherBase.mFormKey = key(0xd01);
        otherBase.mEditorId = "OtherActor";
        store.getWritable<ESM4::Npc>().insertStatic(otherBase, otherBase.mFormKey);
        for (unsigned id : {0xd02u, 0xd03u})
        {
            ESM4::ActorCharacter placed{};
            placed.mId = {id, file}; placed.mFormKey = key(id);
            placed.mBaseObj = id == 0xd02 ? base.mId : otherBase.mId;
            placed.mBaseKey = id == 0xd02 ? base.mFormKey : otherBase.mFormKey;
            store.getWritable<ESM4::ActorCharacter>().insertStatic(placed, placed.mFormKey);
        }
        ObScript::RuntimeContext context; context.mSelf = f.mActor.getCellRef().getFormKey();
        const ObScript::Value fact = ObScript::ReferenceValue{faction.mFormKey, {}};
        const auto checkRank = [&](int rank) {
            EXPECT_EQ(world.getOblivionFactionRank(context.mSelf, faction.mFormKey), rank);
            EXPECT_EQ(world.getOblivionFactionRank(key(0xd02), faction.mFormKey), rank);
            EXPECT_EQ(world.getOblivionFactionRank(key(0xd03), faction.mFormKey), -1);
            EXPECT_EQ(ObScript::asInteger(scripts.call("GetFactionRank", {}, {fact}, context, {})), rank);
            ESM4::PackageCondition condition{};
            condition.mFunction = 73; condition.mParameter1.mReferenceKey = faction.mFormKey;
            condition.mOperator = ESM4::ConditionOperator::Equal; condition.mComparisonValue = rank;
            EXPECT_EQ(world.getOblivionAiService()->evaluateDialogueConditions(f.mActor, world.getPlayerPtr(),
                std::span(&condition, 1)), ESM4::ConditionResult::True);
            condition.mFunction = 71; condition.mComparisonValue = rank != -1;
            EXPECT_EQ(world.getOblivionAiService()->evaluateDialogueConditions(f.mActor, world.getPlayerPtr(),
                std::span(&condition, 1)), ESM4::ConditionResult::True);
        };
        checkRank(-1);
        scripts.call("SetFactionRank", {}, {fact, std::int64_t{1}}, context, {});
        checkRank(1);
        EXPECT_EQ(f.mActor.getClass().getPrimaryFaction(f.mActor), ESM::RefId(faction.mId));
        EXPECT_EQ(f.mActor.getClass().getPrimaryFactionRank(f.mActor), 1);
        EXPECT_EQ(world.getOblivionFactionRank(context.mSelf, existing.mFormKey), 2);
        EXPECT_EQ(store.search<ESM4::Npc>(base.mFormKey)->mFactions.size(), base.mFactions.size());
        const auto saved = ESM4::RuntimeState::deserializeBinary(world.captureOblivionRuntimeState().serializeBinary());
        EXPECT_NO_THROW(world.prepareOblivionSaveState(saved));
        scripts.call("SetFactionRank", {}, {fact, std::int64_t{-1}}, context, {});
        checkRank(-1);
        EXPECT_EQ(f.mActor.getClass().getPrimaryFaction(f.mActor), ESM::RefId(existing.mId));
        EXPECT_EQ(f.mActor.getClass().getPrimaryFactionRank(f.mActor), 2);
        ASSERT_TRUE(world.setOblivionFactionRank(context.mSelf, existing.mFormKey, -1));
        EXPECT_TRUE(f.mActor.getClass().getPrimaryFaction(f.mActor).empty());
        EXPECT_EQ(f.mActor.getClass().getPrimaryFactionRank(f.mActor), -1);
        const auto empty = ESM4::RuntimeState::deserializeBinary(world.captureOblivionRuntimeState().serializeBinary());
        const auto* replacement = combat.getBaseFactions(base.mFormKey);
        ASSERT_TRUE(replacement); EXPECT_TRUE(replacement->empty());
        combat.restore(saved, store); checkRank(1);
        combat.restore(empty, store); checkRank(-1);
        // Absent -1 inserts the native signed byte; a second -1 removes it.
        scripts.call("SetFactionRank", {}, {fact, std::int64_t{-1}}, context, {});
        ASSERT_EQ(world.getOblivionActorFactions(base.mFormKey).size(), 1);
        scripts.call("SetFactionRank", {}, {fact, std::int64_t{257}}, context, {});
        checkRank(1);
        const auto before = world.captureOblivionRuntimeState().mNativeActorBases;
        EXPECT_THROW(scripts.call("SetFactionRank", {}, {ObScript::ReferenceValue{base.mFormKey, {}},
            std::int64_t{1}}, context, {}), ObScript::RuntimeError);
        EXPECT_EQ(world.captureOblivionRuntimeState().mNativeActorBases, before);
        auto bad = saved;
        bad.mNativeActorBases.front().mFactions->front().mFaction = base.mFormKey;
        EXPECT_THROW(world.prepareOblivionSaveState(bad), std::exception);
    }
}

TEST(OblivionDialogueMenu, ActualLoaderResolvesChoicesAfterResultsAndRejectsStaleCallbacks)
{
    for (const bool shifted : {false, true})
    {
        SCOPED_TRACE(shifted);
        MenuFixture f(ESM4::INFO_SayOnce, shifted);
        Callback callback;
        const auto* loaded = f.mFixture.mWorld.getStore().search<ESM4::DialogInfo>(key(0x902));
        ASSERT_NE(loaded, nullptr);
        EXPECT_EQ(loaded->mChoiceTopics, (std::vector<ESM::FormKey>{key(0x903), key(0x905)}));
        ASSERT_TRUE(f.mMenu.startDialogue(f.mActor, &callback));
        EXPECT_EQ(callback.mResponses, (std::vector<std::string>{"First response", "Second response"}));
        EXPECT_EQ(f.stage(), 20);
        EXPECT_TRUE(f.mMenu.isInChoice());
        ASSERT_EQ(f.mMenu.getChoices().size(), 1u);
        EXPECT_EQ(f.mMenu.getChoices()[0].first, "Continue through the passage");
        const int answer = f.mMenu.getChoices()[0].second;
        f.mMenu.questionAnswered(answer, &callback);
        EXPECT_EQ(f.stage(), 30);
        EXPECT_TRUE(f.mMenu.isGoodbye());
        EXPECT_FALSE(f.mMenu.isInChoice());
        EXPECT_TRUE(f.mMenu.getChoices().empty());
        EXPECT_EQ(callback.mResponses.size(), 3u);
        const auto sequence = f.sequence();
        f.mMenu.questionAnswered(answer, &callback);
        EXPECT_EQ(callback.mResponses.size(), 3u);
        EXPECT_EQ(f.stage(), 30);
        EXPECT_EQ(f.sequence(), sequence);
        const auto* scripts = f.mFixture.mWorld.getOblivionScriptManager();
        EXPECT_TRUE(scripts->hasSpokenDialogueInfo(key(0x902)));
        EXPECT_TRUE(scripts->hasSpokenDialogueInfo(key(0x904)));
        EXPECT_FALSE(scripts->hasSpokenDialogueInfo(key(0x906)));
        f.mMenu.goodbyeSelected();
        EXPECT_FALSE(f.mMenu.startDialogue(f.mActor, &callback)); // SayOnce owns root history.
    }
}

TEST(OblivionDialogueMenu, RetainsActualPendingChoicesAndDoesNotReuseIdsAcrossMenus)
{
    MenuFixture f(0);
    Callback callback;
    ASSERT_TRUE(f.mMenu.startDialogue(f.mActor, &callback));
    const auto choices = f.mMenu.getChoices();
    ASSERT_EQ(choices.size(), 1u);
    auto retained = f.mMenu.retainRuntime();
    EXPECT_THROW(f.mMenu.questionAnswered(choices[0].second, &callback), std::logic_error);
    EXPECT_EQ(f.stage(), 20);
    ASSERT_TRUE(retained.detach());
    EXPECT_TRUE(f.mMenu.getChoices().empty());
    EXPECT_FALSE(f.mMenu.isInChoice());
    ASSERT_TRUE(retained.restore());
    EXPECT_EQ(f.mMenu.getChoices(), choices);
    f.mMenu.questionAnswered(choices[0].second, &callback);
    EXPECT_EQ(f.stage(), 30);
    f.mMenu.goodbyeSelected();
    // Start a separate fixture quest run without resetting the menu's ID
    // allocator. Native SetStage cannot rewind a completed stage30 to20.
    auto* scripts = f.mFixture.mWorld.getOblivionScriptManager();
    ESM4::RuntimeState reset;
    scripts->capture(reset);
    for (auto& quest : reset.mQuests)
        if (quest.mQuest == key(0x900)) { quest.mStage = 0; quest.mCompletedStages.clear(); }
    scripts->restore(reset);
    ASSERT_TRUE(f.mMenu.startDialogue(f.mActor, &callback));
    ASSERT_EQ(f.mMenu.getChoices().size(), 1u);
    const int next = f.mMenu.getChoices()[0].second;
    EXPECT_NE(next, choices[0].second);
    const auto count = callback.mResponses.size();
    const auto sequence = f.sequence();
    f.mMenu.questionAnswered(choices[0].second, &callback);
    EXPECT_EQ(f.stage(), 20);
    EXPECT_EQ(callback.mResponses.size(), count);
    EXPECT_EQ(f.sequence(), sequence);
    f.mMenu.questionAnswered(next, &callback);
    EXPECT_EQ(f.stage(), 30);
}

TEST(OblivionDialogueMenu, CallbackClearCannotContinueOrRunANonImmediateResult)
{
    MenuFixture f;
    Callback callback;
    callback.mObserver = [&] { f.mMenu.clear(); };
    EXPECT_FALSE(f.mMenu.startDialogue(f.mActor, &callback));
    EXPECT_EQ(f.stage(), 0);
    EXPECT_FALSE(f.mFixture.mWorld.getOblivionScriptManager()->hasSpokenDialogueInfo(key(0x902)));
    EXPECT_TRUE(f.mMenu.getChoices().empty());
    callback.mObserver = {};
    EXPECT_TRUE(f.mMenu.startDialogue(f.mActor, &callback));
    EXPECT_EQ(f.stage(), 20);
}

TEST(OblivionDialogueMenu, ImmediateAndResponseResultsExecuteAtTheirOwnPresentationBoundary)
{
    for (const bool immediate : {false, true})
    {
        MenuFixture f(ESM4::INFO_SayOnce | (immediate ? ESM4::INFO_RunImmediately : 0));
        Callback callback;
        callback.mObserver = [&] { EXPECT_EQ(f.stage(), immediate ? 20 : 0); };
        ASSERT_TRUE(f.mMenu.startDialogue(f.mActor, &callback));
        EXPECT_EQ(f.stage(), 20);
        EXPECT_TRUE(f.mFixture.mWorld.getOblivionScriptManager()->hasSpokenDialogueInfo(key(0x902)));
    }
}

TEST(OblivionDialogueMenu, PlayerSexConditionUsesCurrentPlayerInsteadOfSpeaker)
{
    MenuFixture f;
    auto& world = f.mFixture.mWorld;
    auto& ai = *world.getOblivionAiService();
    for (const bool male : {true, false, true})
    {
        auto base = *world.getPlayerPtr().get<ESM::NPC>()->mBase;
        base.setIsMale(male);
        auto prepared = world.getStore().preparePlayerRecord(base);
        world.getPlayer().set(prepared.commit());
        for (const int sex : {0, 1, 2})
        {
            ESM4::PackageCondition condition{};
            condition.mFunction = 131;
            condition.mParameter1.mNumber = sex;
            condition.mOperator = ESM4::ConditionOperator::Equal;
            condition.mComparisonValue = 1;
            EXPECT_EQ(ai.evaluateDialogueConditions(f.mActor, {}, std::span(&condition, 1)),
                sex == (male ? 0 : 1) ? ESM4::ConditionResult::True : ESM4::ConditionResult::False);
        }
    }
}

TEST(OblivionDialogueMenu, SameSexUsesOppositeOrExplicitActorAndCurrentPlayerSex)
{
    for (const bool shifted : {false, true})
        for (const bool female : {false, true})
        {
            MenuFixture f(ESM4::INFO_SayOnce, shifted);
            auto& world = f.mFixture.mWorld;
            auto& ai = *world.getOblivionAiService();
            auto npc = *f.mActor.get<ESM4::Npc>()->mBase;
            npc.mBaseConfig.tes4.flags = (npc.mBaseConfig.tes4.flags & ~ESM4::Npc::TES4_Female)
                | (female ? ESM4::Npc::TES4_Female : 0);
            world.getStore().getWritable<ESM4::Npc>().insertStatic(npc, key(0x800));
            ASSERT_EQ(f.mActor.get<ESM4::Npc>()->mBase, world.getStore().search<ESM4::Npc>(key(0x800)));
            ESM4::PackageCondition condition{};
            condition.mFunction = 44;
            condition.mOperator = ESM4::ConditionOperator::Equal;
            condition.mComparisonValue = 1;
            condition.mParameter1.mIsFormId = true;
            const auto query = [&](const MWWorld::Ptr& listener) {
                return ai.evaluateDialogueConditions(f.mActor, listener, std::span(&condition, 1));
            };
            for (const bool playerFemale : {false, true, false})
            {
                auto base = *world.getPlayerPtr().get<ESM::NPC>()->mBase;
                base.setIsMale(!playerFemale);
                auto prepared = world.getStore().preparePlayerRecord(base);
                world.getPlayer().set(prepared.commit());
                const auto expected = female == playerFemale ? ESM4::ConditionResult::True
                                                            : ESM4::ConditionResult::False;
                for (const bool runOnTarget : {false, true})
                {
                    condition.mRunOnTarget = runOnTarget;
                    condition.mParameter1.mReferenceKey = {};
                    EXPECT_EQ(query(world.getPlayerPtr()), expected);
                    condition.mParameter1.mReferenceKey = key(0xa00);
                    EXPECT_EQ(query(world.getPlayerPtr()), runOnTarget ? expected : ESM4::ConditionResult::True);
                }
                condition.mRunOnTarget = false;
                condition.mParameter1.mReferenceKey = {};
                EXPECT_EQ(query({}), ESM4::ConditionResult::False);
                condition.mParameter1.mReferenceKey = key(0xdead);
                EXPECT_EQ(query(world.getPlayerPtr()), ESM4::ConditionResult::False);
            }
        }
}

TEST(OblivionDialogueMenu, SameRaceUsesCanonicalIdentityAndCurrentPlayerRace)
{
    for (const bool shifted : {false, true})
        for (const bool nullRace : {false, true})
        {
            MenuFixture f(ESM4::INFO_SayOnce, shifted);
            auto& world = f.mFixture.mWorld;
            auto& ai = *world.getOblivionAiService();
            auto& store = world.getStore();
            auto other = *store.search<ESM4::Race>(key(0x810));
            other.mId.mIndex = 0x811; // Same name/data, different native race identity.
            store.getWritable<ESM4::Race>().insertStatic(other, key(0x811));
            auto npc = *f.mActor.get<ESM4::Npc>()->mBase;
            if (nullRace) npc.mRace = {};
            store.getWritable<ESM4::Npc>().insertStatic(npc, key(0x800));
            ESM4::PackageCondition condition{};
            condition.mFunction = 43;
            condition.mOperator = ESM4::ConditionOperator::Equal;
            condition.mComparisonValue = 1;
            condition.mParameter1.mIsFormId = true;
            for (const unsigned id : {0x810u, 0x811u, 0x810u, 0u})
            {
                auto base = *world.getPlayerPtr().get<ESM::NPC>()->mBase;
                base.mRace = id ? ESM::RefId(ESM::FormId{id, npc.mId.mContentFile}) : ESM::RefId{};
                auto prepared = store.preparePlayerRecord(base);
                world.getPlayer().set(prepared.commit());
                const auto expected = id == (nullRace ? 0u : 0x810u) ? ESM4::ConditionResult::True
                                                                    : ESM4::ConditionResult::False;
                for (const bool runOnTarget : {false, true})
                {
                    condition.mRunOnTarget = runOnTarget;
                    condition.mParameter1.mReferenceKey = {};
                    EXPECT_EQ(ai.evaluateDialogueConditions(f.mActor, world.getPlayerPtr(),
                        std::span(&condition, 1)), expected);
                    condition.mParameter1.mReferenceKey = key(0xa00);
                    EXPECT_EQ(ai.evaluateDialogueConditions(f.mActor, world.getPlayerPtr(),
                        std::span(&condition, 1)), runOnTarget ? expected : ESM4::ConditionResult::True);
                }
            }
        }
}

TEST(OblivionDialogueMenu, UnloadedNpcPairConditionsReadCurrentPlayerWithoutMaterializingSpeaker)
{
    MenuFixture f;
    auto& world = f.mFixture.mWorld;
    auto& store = world.getStore();
    auto& ai = *world.getOblivionAiService();
    ESM4::AIPackage package{};
    package.mId = {0xe51, f.mActor.get<ESM4::Npc>()->mBase->mId.mContentFile};
    package.mFormKey = key(0xe51);
    package.mPackageType = ESM4::AIPackageType::Wander;
    ESM4::PackageCondition condition{};
    condition.mFunction = 44;
    condition.mOperator = ESM4::ConditionOperator::Equal;
    condition.mComparisonValue = 1;
    condition.mParameter1.mIsFormId = true;
    package.mCanonicalConditions = {condition};
    store.getWritable<ESM4::AIPackage>().insertStatic(package, package.mFormKey);
    auto npc = *f.mActor.get<ESM4::Npc>()->mBase;
    npc.mAIPackageKeys = {package.mFormKey};
    store.getWritable<ESM4::Npc>().insertStatic(npc, key(0x800));
    ESM4::ActorCharacter cold{};
    cold.mId = {0xe52, npc.mId.mContentFile};
    cold.mFormKey = key(0xe52);
    cold.mBaseObj = npc.mId;
    cold.mBaseKey = key(0x800);
    cold.mParent = f.mActor.getCell()->getCell()->getId();
    cold.mParentKey = ESM::FormKeyResolver(world.getContentFiles()).toFormKey(
        *cold.mParent.getIf<ESM::FormId>());
    store.getWritable<ESM4::ActorCharacter>().insertStatic(cold, cold.mFormKey);
    auto saved = world.captureOblivionRuntimeState();
    ESM4::RuntimeActorAiState actor;
    actor.mActor = cold.mFormKey;
    actor.mBase = cold.mBaseKey;
    actor.mCell = actor.mLastValidCell = cold.mParentKey;
    actor.mTarget = ESM::FormKey::dynamic("player", 1);
    actor.mTier = ESM4::ProcessTier::Low;
    saved.mActorAi = {actor};
    const bool female = (npc.mBaseConfig.tes4.flags & ESM4::Npc::TES4_Female) != 0;
    auto other = *store.search<ESM4::Race>(key(0x810));
    other.mId.mIndex = 0x811;
    store.getWritable<ESM4::Race>().insertStatic(other, key(0x811));
    for (const unsigned function : {43u, 44u})
        for (const bool runOnTarget : {false, true})
            for (const bool matches : {true, false, true})
            {
                condition.mFunction = function;
                condition.mRunOnTarget = runOnTarget;
                package.mCanonicalConditions = {condition};
                store.getWritable<ESM4::AIPackage>().insertStatic(package, package.mFormKey);
                auto base = *world.getPlayerPtr().get<ESM::NPC>()->mBase;
                base.setIsMale(!(matches ? female : !female));
                base.mRace = ESM::RefId(matches ? npc.mRace : other.mId);
                auto prepared = store.preparePlayerRecord(base);
                world.getPlayer().set(prepared.commit());
                ASSERT_NO_THROW(ai.restore(saved));
                ai.updateUnloaded(.5f);
                const auto* selected = ai.state(cold.mFormKey);
                ASSERT_TRUE(selected);
                EXPECT_EQ(selected->mConditionResult, matches ? ESM4::ConditionResult::True
                                                             : ESM4::ConditionResult::False);
                EXPECT_TRUE(world.getWorldModel().getPtr(cold.mId).isEmpty());
            }
}

TEST(OblivionDialogueMenu, PlayerBirthsignConditionTracksCurrentCanonicalChoice)
{
    MenuFixture f;
    auto& world = f.mFixture.mWorld;
    for (const unsigned id : {0x910u, 0x911u})
    {
        ESM4::BirthSign sign{};
        sign.mId = {id, 0};
        world.getStore().getWritable<ESM4::BirthSign>().insertStatic(sign, key(id));
    }
    const auto query = [&](unsigned expected) {
        ESM4::PackageCondition condition{};
        condition.mFunction = 224;
        condition.mParameter1.mIsFormId = true;
        condition.mParameter1.mReferenceKey = key(expected);
        condition.mOperator = ESM4::ConditionOperator::Equal;
        condition.mComparisonValue = 1;
        return world.getOblivionAiService()->evaluateDialogueConditions(f.mActor, {}, std::span(&condition, 1));
    };
    world.getPlayer().setBirthSign({});
    EXPECT_EQ(query(0x910), ESM4::ConditionResult::False);
    for (const unsigned id : {0x910u, 0x911u, 0x910u})
    {
        world.getPlayer().setBirthSign(ESM::RefId(ESM::FormId{id, 0}));
        EXPECT_EQ(query(id), ESM4::ConditionResult::True);
        EXPECT_EQ(query(id == 0x910 ? 0x911 : 0x910), ESM4::ConditionResult::False);
        EXPECT_EQ(query(0x912), ESM4::ConditionResult::False);
    }
}

TEST(OblivionDialogueMenu, TurnArrestConditionAndCommandConsumeTheSameOneShotSignal)
{
    MenuFixture f;
    auto& world = f.mFixture.mWorld;
    auto& combat = *world.getOblivionCombatService();
    auto& ai = *world.getOblivionAiService();
    ESM4::PackageCondition condition{};
    condition.mFunction = 329;
    condition.mOperator = ESM4::ConditionOperator::Equal;
    condition.mComparisonValue = 1;
    const auto query = [&] { return ai.evaluateDialogueConditions(f.mActor, world.getPlayerPtr(),
        std::span(&condition, 1)); };
    EXPECT_EQ(query(), ESM4::ConditionResult::False);
    combat.requestTurnArrest();
    combat.beginScriptActorFrame(); // This global is not the per-frame SceneIsComplex flag.
    EXPECT_EQ(query(), ESM4::ConditionResult::True);
    EXPECT_EQ(query(), ESM4::ConditionResult::False);
    auto* scripts = world.getOblivionScriptManager();
    ObScript::RuntimeContext context;
    context.mSelf = f.mActor.getCellRef().getFormKey();
    combat.requestTurnArrest();
    EXPECT_EQ(ObScript::asInteger(scripts->call("IsTurnArrest", {}, {}, context, {})), 1);
    EXPECT_EQ(query(), ESM4::ConditionResult::False);
    combat.requestTurnArrest();
    auto copy = combat;
    EXPECT_TRUE(copy.consumeTurnArrest());
    EXPECT_TRUE(combat.consumeTurnArrest());
    combat.requestTurnArrest();
    combat.clear();
    EXPECT_FALSE(combat.consumeTurnArrest());
}

TEST(OblivionDialogueMenu, CrimeGoldQueriesUseSubjectLegalBountyAndLiveInventoryWithoutSpending)
{
    MenuFixture f;
    auto& world = f.mFixture.mWorld;
    auto& combat = *world.getOblivionCombatService();
    auto& inventory = f.mActor.getClass().getContainerStore(f.mActor);
    auto* scripts = world.getOblivionScriptManager();
    ObScript::RuntimeContext context;
    context.mSelf = f.mActor.getCellRef().getFormKey();
    const auto check = [&](float storedBounty, double expectedBounty, bool canPay) {
        auto values = *combat.findActorValues(context.mSelf);
        values.mBounty = ESM4::CrimeBountyState{storedBounty, 0.f};
        // Legal bounty is independently owned, not the generic AV modifier view.
        values.mValues[37].mModifiers = {.5f, -3.25f, -.25f};
        combat.publishNonPlayerValues(f.mActor, values);
        for (const int function : {116, 127})
        {
            ESM4::PackageCondition condition{};
            condition.mFunction = function;
            condition.mOperator = ESM4::ConditionOperator::Equal;
            condition.mComparisonValue = function == 116 ? expectedBounty : double(canPay);
            EXPECT_EQ(world.getOblivionAiService()->evaluateDialogueConditions(
                f.mActor, world.getPlayerPtr(), std::span(&condition, 1)), ESM4::ConditionResult::True);
        }
        EXPECT_DOUBLE_EQ(ObScript::asNumber(scripts->call("GetCrimeGold", {}, {}, context, {})), expectedBounty);
        EXPECT_EQ(ObScript::asInteger(scripts->call("CanPayCrimeGold", {}, {}, context, {})), canPay);
        EXPECT_EQ(combat.crimeBounty(context.mSelf), expectedBounty);
    };
    check(0, 0, true); // Equality at zero rejects the shipped cannot-pay greeting.
    check(.25f, 1, false); // Native legal query rounds a positive sub-one bounty up.
    inventory.add(MWWorld::ContainerStore::sGoldId, 5, false);
    check(5, 5, true);
    check(5.5f, 5.5, false);
    check(4.5f, 4.5, true);
    EXPECT_EQ(inventory.count(MWWorld::ContainerStore::sGoldId), 5);
}

TEST(OblivionDialogueMenu, GuardConditionReadsNativeClassFlagRatherThanActorName)
{
    MenuFixture f;
    auto& world = f.mFixture.mWorld;
    const auto* base = world.getStore().search<ESM4::Npc>(key(0x800));
    const auto npc = *base;
    ESM4::Class cls{};
    cls.mId = {0x811, npc.mId.mContentFile};
    for (const unsigned flags : {0u, 1u, 2u, 3u, 4u})
    {
        cls.mData.mFlags = flags;
        world.getStore().getWritable<ESM4::Class>().insertStatic(cls, key(0x811));
        auto replacement = npc;
        replacement.mClass = cls.mId;
        replacement.mFullName = "Definitely a Guard";
        world.getStore().getWritable<ESM4::Npc>().insertStatic(replacement, key(0x800));
        ESM4::PackageCondition condition{};
        condition.mFunction = 125;
        condition.mOperator = ESM4::ConditionOperator::Equal;
        condition.mComparisonValue = (flags & 2u) ? 1 : 0;
        EXPECT_EQ(world.getOblivionAiService()->evaluateDialogueConditions(
            f.mActor, world.getPlayerPtr(), std::span(&condition, 1)), ESM4::ConditionResult::True);
    }
}

TEST(OblivionDialogueMenu, AbsentTrespassPackageRejectsWarningGreetings)
{
    for (const bool shifted : {false, true})
    {
        MenuFixture f(ESM4::INFO_SayOnce, shifted);
        auto& world = f.mFixture.mWorld;
        ESM4::PackageCondition condition{};
        condition.mFunction = 144;
        condition.mOperator = ESM4::ConditionOperator::Equal;
        // Original warning count is owned by a transient package, not by
        // ownership classification or the currently selected authored PACK.
        for (const int level : {-1, 0, 1, 2})
        {
            condition.mComparisonValue = level;
            EXPECT_EQ(world.getOblivionAiService()->evaluateDialogueConditions(
                f.mActor, world.getPlayerPtr(), std::span(&condition, 1)),
                level == -1 ? ESM4::ConditionResult::True : ESM4::ConditionResult::False);
        }
    }
}

TEST(OblivionDialogueMenu, PlayerMiscStatsShareUnsignedScriptConditionAndSavedState)
{
    for (const bool shifted : {false, true})
    {
        MenuFixture f(ESM4::INFO_SayOnce, shifted);
        auto& world = f.mFixture.mWorld;
        auto& scripts = *world.getOblivionScriptManager();
        const auto read = [&](int index) {
            return ObScript::asInteger(scripts.call("GetPCMiscStat", {}, {std::int64_t(index)}, {}, {}));
        };
        const auto modify = [&](int index, std::int64_t delta) {
            scripts.call("ModPCMiscStat", {}, {std::int64_t(index), delta}, {}, {});
        };
        const auto initial = world.captureOblivionRuntimeState().serializeBinary();
        EXPECT_EQ(read(13), 0);
        EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), initial);
        for (const int index : {0, 13, 33})
        {
            modify(index, -1);
            EXPECT_EQ(read(index), 4294967295LL);
            modify(index, 1);
            EXPECT_EQ(read(index), 0);
        }
        modify(13, 3);
        ESM4::PackageCondition condition{};
        condition.mFunction = 312;
        condition.mParameter1.mNumber = 13;
        condition.mOperator = ESM4::ConditionOperator::GreaterOrEqual;
        condition.mComparisonValue = 3;
        EXPECT_EQ(world.getOblivionAiService()->evaluateDialogueConditions(
            f.mActor, world.getPlayerPtr(), std::span(&condition, 1)), ESM4::ConditionResult::True);
        const auto saved = ESM4::RuntimeState::deserializeBinary(world.captureOblivionRuntimeState().serializeBinary());
        EXPECT_NO_THROW(world.prepareOblivionSaveState(saved));
        auto invalid = saved;
        invalid.mPlayer.mActorValues["pc_misc_stat.13"] = .5;
        EXPECT_THROW(world.prepareOblivionSaveState(invalid), std::invalid_argument);
        EXPECT_EQ(read(13), 3);
        EXPECT_THROW(read(-1), std::invalid_argument);
        EXPECT_THROW(read(34), std::invalid_argument);
        EXPECT_THROW(modify(13, 2147483648LL), ObScript::RuntimeError);
        modify(13, 1);
        EXPECT_EQ(read(13), 4);
        ESM::ESMWriter writer;
        auto stream = std::make_unique<std::stringstream>();
        writer.setFormatVersion(ESM::CurrentSaveGameFormatVersion);
        writer.save(*stream);
        writer.startRecord(ESM::REC_T4ST); saved.save(writer); writer.endRecord(ESM::REC_T4ST);
        ESM::ESMReader reader;
        reader.open(std::move(stream), "misc-stat-fixture");
        ASSERT_EQ(reader.getRecName(), ESM::REC_T4ST); reader.getRecHeader();
        world.readRecord(reader, ESM::REC_T4ST);
        ASSERT_NO_THROW(world.applyOblivionRuntimeState());
        EXPECT_EQ(read(13), 3);
    }
}

TEST(OblivionDialogueMenu, MurdererFlagIsPlayerOwnedIndependentOfCounterAndRestoresFromSave)
{
    for (const bool shifted : {false, true})
    {
        MenuFixture f(ESM4::INFO_SayOnce, shifted);
        auto& world = f.mFixture.mWorld;
        auto& scripts = *world.getOblivionScriptManager();
        auto& ai = *world.getOblivionAiService();
        ESM4::PackageCondition condition{};
        condition.mFunction = 176;
        condition.mOperator = ESM4::ConditionOperator::Equal;
        condition.mComparisonValue = 1;
        const auto query = [&] {
            for (const auto& self : {ESM::FormKey{}, key(0xa00), ESM::FormKey::dynamic("player", 1)})
            {
                ObScript::RuntimeContext context; context.mSelf = self;
                EXPECT_EQ(ObScript::asInteger(scripts.call("IsPCAMurderer", {}, {}, context, {})),
                    scripts.playerIsMurderer() ? 1 : 0);
            }
            for (const bool target : {false, true})
            {
                condition.mRunOnTarget = target;
                const auto expected = scripts.playerIsMurderer() ? ESM4::ConditionResult::True
                                                               : ESM4::ConditionResult::False;
                EXPECT_EQ(ai.evaluateDialogueConditions(f.mActor, {}, std::span(&condition, 1)), expected);
                EXPECT_EQ(ai.evaluateNativeIdleConditions(f.mActor, std::span(&condition, 1), {}), expected);
            }
        };
        const auto initial = world.captureOblivionRuntimeState().serializeBinary();
        EXPECT_FALSE(scripts.playerIsMurderer());
        query();
        EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), initial);
        scripts.call("ModPCMiscStat", {}, {std::int64_t(32), std::int64_t(-1)}, {}, {});
        EXPECT_FALSE(scripts.playerIsMurderer()); // Modifying the counter does not notify a murder.
        scripts.notifyPlayerMurder();
        EXPECT_TRUE(scripts.playerIsMurderer());
        EXPECT_EQ(scripts.playerMiscStat(32), 0u); // Original wrap does not clear Player+611.
        query();
        const auto saved = ESM4::RuntimeState::deserializeBinary(world.captureOblivionRuntimeState().serializeBinary());
        EXPECT_NO_THROW(world.prepareOblivionSaveState(saved));
        for (const double invalid : {-1.0, .5, 2.0, std::numeric_limits<double>::infinity(),
                 std::numeric_limits<double>::quiet_NaN()})
        {
            auto bad = saved;
            bad.mPlayer.mActorValues["crime.murderer"] = invalid;
            if (std::isfinite(invalid))
                EXPECT_THROW(world.prepareOblivionSaveState(bad), std::invalid_argument);
            else
                EXPECT_THROW(world.prepareOblivionSaveState(bad), std::runtime_error);
            EXPECT_TRUE(scripts.playerIsMurderer());
        }
        const auto load = [&](const ESM4::RuntimeState& state) {
            ESM::ESMWriter writer;
            auto stream = std::make_unique<std::stringstream>();
            writer.setFormatVersion(ESM::CurrentSaveGameFormatVersion); writer.save(*stream);
            writer.startRecord(ESM::REC_T4ST); state.save(writer); writer.endRecord(ESM::REC_T4ST);
            ESM::ESMReader reader; reader.open(std::move(stream), "murderer-state-fixture");
            ASSERT_EQ(reader.getRecName(), ESM::REC_T4ST); reader.getRecHeader();
            world.readRecord(reader, ESM::REC_T4ST);
            ASSERT_NO_THROW(world.applyOblivionRuntimeState());
        };
        auto legacy = saved;
        legacy.mPlayer.mActorValues.erase("crime.murderer");
        load(legacy);
        EXPECT_FALSE(scripts.playerIsMurderer());
        query();
        load(saved);
        EXPECT_TRUE(scripts.playerIsMurderer());
        EXPECT_EQ(scripts.playerMiscStat(32), 0u);
        query();
        auto clean = saved;
        clean.mPlayer.mActorValues["crime.murderer"] = 0;
        load(clean);
        EXPECT_FALSE(scripts.playerIsMurderer());
        query();
    }
}

TEST(OblivionDialogueMenu, InfamyConditionSharesReadOnlyPlayerAuthorityWithScriptQuery)
{
    MenuFixture f;
    auto& world = f.mFixture.mWorld;
    auto& scripts = *world.getOblivionScriptManager();
    auto& ai = *world.getOblivionAiService();
    ESM4::PackageCondition condition{};
    condition.mFunction = 251;
    condition.mOperator = ESM4::ConditionOperator::GreaterOrEqual;
    condition.mComparisonValue = 50;
    for (const int infamy : {0, 75, 25})
    {
        if (infamy) ASSERT_TRUE(world.modifyOblivionPlayerInfamy(infamy - scripts.playerInfamy()));
        EXPECT_EQ(scripts.playerInfamy(), infamy);
        ObScript::RuntimeContext context; context.mSelf = key(0xa00);
        EXPECT_EQ(ObScript::asInteger(scripts.call("GetPCInfamy", {}, {}, context, {})), infamy);
        const auto before = world.captureOblivionRuntimeState().serializeBinary();
        for (const bool target : {false, true})
        {
            condition.mRunOnTarget = target;
            const auto expected = infamy >= 50 ? ESM4::ConditionResult::True : ESM4::ConditionResult::False;
            EXPECT_EQ(ai.evaluateDialogueConditions(f.mActor, {}, std::span(&condition, 1)), expected);
            EXPECT_EQ(ai.evaluateNativeIdleConditions(f.mActor, std::span(&condition, 1), {}), expected);
        }
        EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), before);
    }
}

TEST(OblivionDialogueMenu, DispositionConditionUsesOppositeActorAndHonorsExplicitOverride)
{
    MenuFixture f;
    auto& world = f.mFixture.mWorld;
    auto& ai = *world.getOblivionAiService();
    const auto actor = f.mActor.getCellRef().getFormKey();
    const auto player = ESM::FormKey::dynamic("player", 1);
    ASSERT_TRUE(world.modifyOblivionPlayerInfamy(0));
    ASSERT_TRUE(world.modifyOblivionDisposition(actor, player, 20));
    const auto forward = world.getOblivionScriptDisposition(actor, player);
    ASSERT_TRUE(forward);
    ASSERT_GT(*forward, 0);
    ESM4::PackageCondition condition{};
    condition.mFunction = 76;
    condition.mOperator = ESM4::ConditionOperator::Equal;
    condition.mComparisonValue = *forward;
    condition.mParameter1.mIsFormId = true;
    EXPECT_EQ(ai.evaluateDialogueConditions(f.mActor, world.getPlayerPtr(), std::span(&condition, 1)),
        ESM4::ConditionResult::True);
    condition.mParameter1.mReferenceKey = player;
    EXPECT_EQ(ai.evaluateDialogueConditions(f.mActor, world.getPlayerPtr(), std::span(&condition, 1)),
        ESM4::ConditionResult::True);
    const auto reverse = world.getOblivionScriptDisposition(player, actor);
    ASSERT_TRUE(reverse);
    condition.mRunOnTarget = true;
    condition.mParameter1.mReferenceKey = {};
    condition.mComparisonValue = *reverse;
    EXPECT_EQ(ai.evaluateDialogueConditions(f.mActor, world.getPlayerPtr(), std::span(&condition, 1)),
        ESM4::ConditionResult::True);
    condition.mRunOnTarget = false;
    condition.mParameter1.mReferenceKey = key(0xdead);
    condition.mComparisonValue = 0;
    EXPECT_EQ(ai.evaluateDialogueConditions(f.mActor, world.getPlayerPtr(), std::span(&condition, 1)),
        ESM4::ConditionResult::True);

    // Exercise the low-process adapter with the same owned inputs/cache.
    ESM4::AIPackage package{};
    package.mId = {0xe54, f.mActor.get<ESM4::Npc>()->mBase->mId.mContentFile};
    package.mFormKey = key(0xe54);
    package.mPackageType = ESM4::AIPackageType::Wander;
    condition.mParameter1.mReferenceKey = {};
    condition.mComparisonValue = *forward;
    package.mCanonicalConditions = {condition};
    world.getStore().getWritable<ESM4::AIPackage>().insertStatic(package, package.mFormKey);
    auto npc = *f.mActor.get<ESM4::Npc>()->mBase;
    npc.mAIPackageKeys = {package.mFormKey};
    world.getStore().getWritable<ESM4::Npc>().insertStatic(npc, key(0x800));
    auto saved = world.captureOblivionRuntimeState();
    ESM4::RuntimeActorAiState cold{};
    cold.mActor = actor; cold.mBase = key(0x800);
    cold.mCell = cold.mLastValidCell = ESM::FormKeyResolver(world.getContentFiles()).toFormKey(
        *f.mActor.getCell()->getCell()->getId().getIf<ESM::FormId>());
    cold.mTarget = player;
    cold.mTier = ESM4::ProcessTier::Low;
    saved.mActorAi = {cold};
    const auto actorId = ESM::FormKeyResolver(world.getContentFiles()).toFormId(actor);
    ASSERT_TRUE(actorId);
    ASSERT_EQ(world.getWorldModel().getPtr(*actorId), f.mActor);
    world.getWorldModel().deregisterLiveCellRef(*f.mActor.getBase());
    EXPECT_TRUE(world.getWorldModel().getPtr(*actorId).isEmpty());
    ASSERT_NO_THROW(ai.restore(saved));
    ai.updateUnloaded(.5f);
    const auto* selected = ai.state(actor);
    ASSERT_TRUE(selected);
    EXPECT_EQ(selected->mConditionResult, ESM4::ConditionResult::True);
    EXPECT_TRUE(world.getWorldModel().getPtr(*actorId).isEmpty());
    world.getWorldModel().registerPtr(f.mActor);
}

TEST(OblivionDialogueMenu, EmptyOwnedCrimeLedgerRejectsCrimeGreeting)
{
    MenuFixture f;
    auto& world = f.mFixture.mWorld;
    ASSERT_TRUE(world.getOblivionCombatService()->crimeContracts().mIncidents.empty());
    ESM4::PackageCondition condition{};
    condition.mFunction = 122;
    condition.mOperator = ESM4::ConditionOperator::Equal;
    condition.mParameter1.mIsFormId = true;
    condition.mParameter1.mReferenceKey = ESM::FormKey::dynamic("player", 1);
    for (const int offense : {-1, 0, 1, 2, 3, 4, 5, 6})
    {
        condition.mParameter2.mNumber = offense;
        EXPECT_EQ(world.getOblivionAiService()->evaluateDialogueConditions(
            f.mActor, world.getPlayerPtr(), std::span(&condition, 1)), ESM4::ConditionResult::True);
    }
}

TEST(OblivionDialogueMenu, OwnedCellConditionUsesNpcIdentityAndRequiredFactionRank)
{
    for (const bool shifted : {false, true})
    {
        MenuFixture f(ESM4::INFO_SayOnce, shifted);
        auto& world = f.mFixture.mWorld;
        auto& store = world.getStore();
        const auto npc = *store.search<ESM4::Npc>(key(0x800));
        ESM4::Cell cell{};
        cell.mId = ESM::RefId(ESM::FormId{0x812, npc.mId.mContentFile});
        cell.mFormKey = key(0x812);
        cell.mEditorId = "OwnedCellConditionRoom";
        cell.mCellFlags = ESM4::CELL_Interior;
        store.getWritable<ESM4::Cell>().insertStatic(cell, cell.mFormKey);
        auto& resident = world.getWorldModel().getCell(cell.mId);
        f.mActor = f.mActor.getCell()->moveTo(f.mActor, &resident);
        world.getWorldModel().registerPtr(f.mActor);
        ESM4::PackageCondition condition{};
        condition.mFunction = 146;
        condition.mOperator = ESM4::ConditionOperator::Equal;
        const auto check = [&](bool expected) {
            condition.mComparisonValue = expected ? 1 : 0;
            EXPECT_EQ(world.getOblivionAiService()->evaluateDialogueConditions(
                f.mActor, world.getPlayerPtr(), std::span(&condition, 1)), ESM4::ConditionResult::True);
        };
        check(false); // Being allowed in an unowned cell does not make it yours.
        cell.mOwner = npc.mId;
        store.getWritable<ESM4::Cell>().insertStatic(cell, cell.mFormKey);
        check(true);
        auto otherNpc = npc;
        otherNpc.mId = {0x813, npc.mId.mContentFile};
        otherNpc.mFormKey = key(0x813);
        otherNpc.mEditorId = "OtherCellOwner";
        store.getWritable<ESM4::Npc>().insertStatic(otherNpc, key(0x813));
        cell.mOwner = otherNpc.mId;
        store.getWritable<ESM4::Cell>().insertStatic(cell, cell.mFormKey);
        check(false);
        ESM4::Faction faction{};
        faction.mId = {0x814, npc.mId.mContentFile};
        faction.mFormKey = key(0x814);
        faction.mFactionFlags = 0;
        store.getWritable<ESM4::Faction>().insertStatic(faction, key(0x814));
        cell.mOwner = faction.mId;
        for (const int required : {-1, 0, 2})
        {
            cell.mOwnershipRank = required;
            store.getWritable<ESM4::Cell>().insertStatic(cell, cell.mFormKey);
            for (const int rank : {-1, 0, 1, 2, 3})
            {
                auto actorBase = npc;
                actorBase.mFactions = {{faction.mId.toUint32(), static_cast<std::int8_t>(rank), 0, 0, 0}};
                store.getWritable<ESM4::Npc>().insertStatic(actorBase, key(0x800));
                check(rank >= (required == -1 ? 0 : required));
            }
        }
    }
}

TEST(OblivionDialogueMenu, TrespassingUsesCellPolicyAndNativeSubjectMembership)
{
    MenuFixture f;
    auto& world = f.mFixture.mWorld;
    auto& store = world.getStore();
    auto npc = *store.search<ESM4::Npc>(key(0x800));
    ESM4::Cell cell{};
    cell.mId = ESM::RefId(ESM::FormId{0x815, npc.mId.mContentFile});
    cell.mFormKey = key(0x815);
    cell.mEditorId = "TrespassConditionRoom";
    cell.mCellFlags = ESM4::CELL_Interior;
    store.getWritable<ESM4::Cell>().insertStatic(cell, cell.mFormKey);
    auto& resident = world.getWorldModel().getCell(cell.mId);
    f.mActor = f.mActor.getCell()->moveTo(f.mActor, &resident);
    world.getWorldModel().registerPtr(f.mActor);
    world.getPlayer().setCell(&resident);
    ESM4::PackageCondition condition{};
    condition.mFunction = 145;
    condition.mOperator = ESM4::ConditionOperator::Equal;
    const auto check = [&](bool expected) {
        store.getWritable<ESM4::Cell>().insertStatic(cell, cell.mFormKey);
        condition.mComparisonValue = expected ? 1 : 0;
        EXPECT_EQ(world.getOblivionAiService()->evaluateDialogueConditions(
            f.mActor, world.getPlayerPtr(), std::span(&condition, 1)), ESM4::ConditionResult::True);
    };
    check(false);
    cell.mOwner = npc.mId;
    check(false);
    auto other = npc;
    other.mId = {0x816, npc.mId.mContentFile};
    other.mFormKey = key(0x816);
    other.mEditorId = "TrespassRoomOwner";
    store.getWritable<ESM4::Npc>().insertStatic(other, other.mFormKey);
    cell.mOwner = other.mId;
    check(true);
    for (const auto flag : {ESM4::CELL_Public, ESM4::CELL_HandChgd})
    {
        cell.mCellFlags = ESM4::CELL_Interior | flag;
        check(false);
    }
    cell.mCellFlags = ESM4::CELL_Interior;
    ESM4::GlobalVariable global{};
    global.mId = {0x818, npc.mId.mContentFile};
    global.mType = 'f';
    global.mValue = 0; // Identity grants permission; its numeric value is irrelevant.
    store.getWritable<ESM4::GlobalVariable>().insertStatic(global, key(0x818));
    cell.mGlobal = global.mId;
    check(false);
    cell.mGlobal = {};
    ESM4::Class cls{};
    cls.mId = {0x817, npc.mId.mContentFile};
    cls.mData.mFlags = 2;
    store.getWritable<ESM4::Class>().insertStatic(cls, key(0x817));
    npc.mClass = cls.mId;
    store.getWritable<ESM4::Npc>().insertStatic(npc, key(0x800));
    check(false);
    cls.mData.mFlags = 0;
    store.getWritable<ESM4::Class>().insertStatic(cls, key(0x817));
    check(true);
    ESM4::Faction faction{};
    faction.mId = {0x819, npc.mId.mContentFile};
    faction.mFormKey = key(0x819);
    faction.mFactionFlags = 8;
    store.getWritable<ESM4::Faction>().insertStatic(faction, faction.mFormKey);
    cell.mOwner = faction.mId;
    cell.mOwnershipRank = 2;
    npc.mFactions = {{faction.mId.toUint32(), 2, 0, 0, 0}};
    store.getWritable<ESM4::Npc>().insertStatic(npc, key(0x800));
    check(false); // Raw flag8 does not filter NPC membership.
    condition.mRunOnTarget = true;
    check(true); // The actual Player query filters this faction to rank-1.
}

TEST(OblivionDialogueMenu, ClassDefaultQueryTracksUsesAndRetainsRealClassIdentityAcrossDisabledTracking)
{
    for (const bool shifted : {false, true})
    {
        MenuFixture f(ESM4::INFO_SayOnce, shifted);
        auto& world = f.mFixture.mWorld;
        auto& store = world.getStore();
        const auto file = store.search<ESM4::Npc>(key(0x800))->mId.mContentFile;
        for (const auto [id, specialization] : {std::pair{0x820u, 0u}, std::pair{0x821u, 2u},
                 std::pair{0x822u, 0u}, std::pair{0x823u, 0u}})
        {
            ESM4::Class cls{};
            cls.mId = {id, file};
            cls.mData.mFavoredAttributes = {0, 1};
            cls.mData.mSpecialization = specialization;
            for (std::size_t i = 0; i < 7; ++i) cls.mData.mMajorSkills[i] = 12 + i;
            store.getWritable<ESM4::Class>().insertStatic(cls, key(id));
            ESM::Class shared;
            shared.blank();
            shared.mId = ESM::RefId(cls.mId);
            shared.mData.mSpecialization = specialization;
            store.getWritable<ESM::Class>().insertStatic(shared);
        }
        unsigned settingId = 0x840;
        for (const auto [name, id] : {std::pair{"iClassCharactergenClass", 0x820u},
                 std::pair{"iClassAcrobat", 0x821u}, std::pair{"iClassWarrior", 0x822u}})
        {
            ESM4::GameSetting setting{};
            setting.mId = {settingId, file};
            setting.mEditorId = name;
            setting.mData = static_cast<std::int32_t>(id);
            store.getWritable<ESM4::GameSetting>().insertStatic(setting, key(settingId++));
        }
        for (unsigned i = 0; i < 21; ++i)
        {
            ESM4::Skill skill{};
            skill.mId = {0x850 + i, file};
            skill.mFormKey = key(0x850 + i);
            skill.mIndex = 12 + i;
            skill.mData = ESM4::SkillData{12 + i, 0, i / 7, {1.f, 2.f}};
            store.getWritable<ESM4::Skill>().insertStatic(skill, skill.mFormKey);
        }
        auto race = *store.search<ESM4::Race>(key(0x810));
        race.mTES4SkillBonuses.emplace();
        for (auto& bonus : *race.mTES4SkillBonuses) bonus = {-1, 0};
        store.getWritable<ESM4::Race>().insertStatic(race, key(0x810));
        ESM::Race sharedRace;
        sharedRace.blank();
        sharedRace.mId = ESM::RefId(race.mId);
        store.getWritable<ESM::Race>().insertStatic(sharedRace);
        ESM4::Npc playerSource{};
        playerSource.mId = {7, file + 1};
        playerSource.mFormKey = ESM::FormKey::content("Oblivion.esm", 7);
        playerSource.mIsTES4 = true;
        playerSource.mData.attribs = {40, 40, 40, 40, 40, 40, 40, 40};
        playerSource.mData.skills.handToHand = 5;
        playerSource.mData.health = 80;
        playerSource.mBaseConfig.tes4.fatigue = 160;
        store.getWritable<ESM4::Npc>().insertStatic(playerSource, playerSource.mFormKey);
        const auto selectClass = [&](unsigned id) {
            auto base = *world.getPlayerPtr().get<ESM::NPC>()->mBase;
            base.mClass = ESM::RefId(ESM::FormId{id, file});
            base.mRace = ESM::RefId(race.mId);
            auto prepared = store.preparePlayerRecord(base);
            world.getPlayer().set(prepared.commit());
        };
        selectClass(0x820);
        ASSERT_TRUE(world.initializeOblivionPlayerActor());
        auto* scripts = world.getOblivionScriptManager();
        ObScript::RuntimeContext context;
        context.mSelf = f.mActor.getCellRef().getFormKey();
        const std::vector<ObScript::Value> enabled{std::int64_t(1)};
        scripts->call("SetInCharGen", {}, enabled, context, {});
        EXPECT_TRUE(MWWorld::oblivionIsClassDefault(world, key(0x821)));
        EXPECT_EQ(MWWorld::oblivionClassDefaultMatch(world), 0u);
        ASSERT_TRUE(world.getPlayer().getOblivionCharacterGenerationState());
        EXPECT_EQ(world.getPlayer().getOblivionCharacterGenerationState()->mRecommendedClass, key(0x821));
        MWWorld::recordOblivionCharacterGenerationSkillUse(world.getPlayerPtr(), ESM::Skill::HandToHand, 0, 3.f);
        EXPECT_EQ((*world.getPlayer().getOblivionCharacterGenerationState()->mSkillUses)[5], 3);
        EXPECT_TRUE(MWWorld::oblivionIsClassDefault(world, key(0x822)));
        EXPECT_EQ(MWWorld::oblivionClassDefaultMatch(world), 1u);
        EXPECT_EQ(world.getPlayer().getOblivionCharacterGenerationState()->mRecommendedClass, key(0x822));
        selectClass(0x822);
        EXPECT_EQ(ObScript::asInteger(scripts->call("GetClassDefaultMatch", {}, {}, context, {})), 2);
        selectClass(0x823);
        // The current class differs but shares specialization with the recommendation.
        // Identity condition228 must use the recommendation and ignore its actor subject.
        EXPECT_TRUE(MWWorld::oblivionIsClassDefault(world, key(0x822)));
        EXPECT_FALSE(MWWorld::oblivionIsClassDefault(world, key(0x823)));
        EXPECT_FALSE(MWWorld::oblivionIsClassDefault(world, key(0x800)));
        const std::vector<ObScript::Value> classArgument{ObScript::ReferenceValue{key(0x822), {}}};
        EXPECT_EQ(ObScript::asInteger(scripts->call("GetIsClassDefault", {}, classArgument, context, {})), 1);
        ESM4::PackageCondition identity{};
        identity.mFunction = 228;
        identity.mParameter1.mReferenceKey = key(0x822);
        identity.mOperator = ESM4::ConditionOperator::Equal;
        identity.mComparisonValue = 1;
        for (const bool target : {false, true})
        {
            identity.mRunOnTarget = target;
            EXPECT_EQ(world.getOblivionAiService()->evaluateDialogueConditions(
                f.mActor, world.getPlayerPtr(), std::span(&identity, 1)), ESM4::ConditionResult::True);
        }
        identity.mParameter1.mReferenceKey = key(0x823);
        EXPECT_EQ(world.getOblivionAiService()->evaluateDialogueConditions(
            f.mActor, world.getPlayerPtr(), std::span(&identity, 1)), ESM4::ConditionResult::False);
        ESM4::PackageCondition condition{};
        condition.mFunction = 229;
        condition.mOperator = ESM4::ConditionOperator::Equal;
        condition.mComparisonValue = 1;
        EXPECT_EQ(world.getOblivionAiService()->evaluateDialogueConditions(
            f.mActor, world.getPlayerPtr(), std::span(&condition, 1)), ESM4::ConditionResult::True);
        const std::vector<ObScript::Value> disabled{std::int64_t(0)};
        scripts->call("SetInCharGen", {}, disabled, context, {});
        const auto saved = world.getPlayer().getOblivionCharacterGenerationState();
        EXPECT_FALSE(saved->mSkillUses);
        EXPECT_EQ(saved->mRecommendedClass, key(0x822));
        EXPECT_EQ(MWWorld::oblivionClassDefaultMatch(world), 1u);
        MWWorld::recordOblivionCharacterGenerationSkillUse(world.getPlayerPtr(), ESM::Skill::HandToHand, 0, 10.f);
        EXPECT_EQ(world.getPlayer().getOblivionCharacterGenerationState(), saved);

    }
}

TEST(OblivionDialogueMenu, LearnedNativeTopicsUseConditionsAndPersistAcrossContentRemapping)
{
    std::string savedTopics;
    for (bool shifted : {false, true})
    {
        MenuFixture f(0, shifted);
        f.mFixture.mEnvironment.setDialogueManager(f.mMenu);
        auto& store = f.mFixture.mWorld.getStore();
        auto greeting = *store.search<ESM4::DialogInfo>(key(0x902));
        greeting.mChoiceTopics.clear();
        store.getWritable<ESM4::DialogInfo>().insertStatic(greeting, key(0x902));
        Callback callback;
        ASSERT_TRUE(f.mMenu.startDialogue(f.mActor, &callback));
        EXPECT_FALSE(f.mMenu.isInChoice());
        EXPECT_FALSE(f.mMenu.isGoodbye());
        EXPECT_TRUE(f.mMenu.getChoices().empty());
        EXPECT_TRUE(f.mMenu.getAvailableTopics().empty());
        auto& scripts = *f.mFixture.mWorld.getOblivionScriptManager();
        ObScript::RuntimeContext context;
        context.mSelf = f.mActor.getCellRef().getFormKey();
        if (!shifted)
        {
            for (auto id : {0x903u, 0x905u, 0x903u})
                scripts.call("AddTopic", {}, {ObScript::ReferenceValue{key(id), {}}}, context, {});
            EXPECT_THROW(scripts.call("AddTopic", {}, {ObScript::ReferenceValue{key(0x800), {}}}, context, {}),
                ObScript::RuntimeError);
            ESM::ESMWriter writer;
            std::stringstream stream;
            writer.setFormatVersion(ESM::CurrentSaveGameFormatVersion);
            writer.save(stream);
            Loading::Listener listener;
            f.mMenu.write(writer, listener);
            writer.close();
            savedTopics = stream.str();
        }
        auto stream = std::make_unique<std::stringstream>(savedTopics);
        ESM::ESMReader reader;
        reader.open(std::move(stream), "native-learned-topics");
        const auto file = store.search<ESM4::Dialogue>(key(0x903))->mId.mContentFile;
        std::map<int, int> mapping{{0, file}};
        reader.setContentFileMapping(&mapping);
        ASSERT_EQ(reader.getRecName(), ESM::REC_DIAS);
        reader.getRecHeader();
        f.mMenu.readRecord(reader, ESM::REC_DIAS);
        EXPECT_EQ(f.mMenu.getAvailableTopics(), (std::list<std::string>{"Continue through the passage"}));

        // Prepared restoration must retain native IDs too, and duplicates must
        // remain one learned topic after actual serialization/remapping.
        ESM::DialogueState state;
        state.mKnownTopics = {ESM::RefId(ESM::FormId{0x903, file}), ESM::RefId(ESM::FormId{0x905, file})};
        auto restore = f.mMenu.prepareDialogueState(std::move(state), store);
        f.mMenu.clear();
        ASSERT_TRUE(restore());
        ASSERT_TRUE(f.mMenu.startDialogue(f.mActor, &callback));
        EXPECT_EQ(f.mMenu.getAvailableTopics(), (std::list<std::string>{"Continue through the passage"}));
        const auto responses = callback.mResponses.size();
        f.mMenu.keywordSelected("This question must stay hidden", &callback);
        EXPECT_EQ(callback.mResponses.size(), responses); // Learned, but its stage99 condition fails.
        f.mMenu.keywordSelected("CONTINUE THROUGH THE PASSAGE", &callback);
        EXPECT_EQ(f.stage(), 30);
        EXPECT_EQ(callback.mResponses.size(), responses + 1);
        EXPECT_TRUE(f.mMenu.isGoodbye());
        EXPECT_TRUE(f.mMenu.getAvailableTopics().empty());
    }
}

TEST(OblivionDialogueMenu, ScriptedPlayerConversationUsesGreetingResultsAndActorHistory)
{
    struct Windows : TestingOpenMW::InventoryWindowManager
    {
        std::function<void(const MWWorld::Ptr&)> mOpen;
        unsigned mCalls = 0;
        void pushGuiMode(MWGui::GuiMode mode, const MWWorld::Ptr& actor) override
        {
            EXPECT_EQ(mode, MWGui::GM_Dialogue);
            ++mCalls;
            mOpen(actor);
        }
    };
    for (const bool shifted : {false, true})
    {
        Windows windows;
        MenuFixture f(ESM4::INFO_SayOnce, shifted);
        auto& world = f.mFixture.mWorld;
        f.mFixture.mEnvironment.setWindowManager(windows);
        f.mFixture.mEnvironment.setDialogueManager(f.mMenu);
        Callback callback;
        windows.mOpen = [&](const MWWorld::Ptr& actor) {
            EXPECT_EQ(actor, f.mActor);
            EXPECT_TRUE(f.mMenu.startDialogue(actor, &callback));
        };
        auto& scripts = *world.getOblivionScriptManager();
        ObScript::RuntimeContext context;
        context.mSelf = f.mActor.getCellRef().getFormKey();
        const ObScript::Value player = ObScript::ReferenceValue{ESM::FormKey::dynamic("player", 1), {}};
        EXPECT_EQ(ObScript::asInteger(scripts.call("GetTalkedToPC", {}, {}, context, {})), 0);
        EXPECT_EQ(ObScript::asInteger(scripts.call("StartConversation", {}, {player}, context, {})), 1);
        EXPECT_EQ(windows.mCalls, 1u);
        EXPECT_EQ(callback.mResponses, (std::vector<std::string>{"First response", "Second response"}));
        EXPECT_EQ(f.stage(), 20);
        EXPECT_EQ(ObScript::asInteger(scripts.call("GetTalkedToPC", {}, {}, context, {})), 1);
        EXPECT_FALSE(world.getPlayerPtr().getClass().getCreatureStats(world.getPlayerPtr()).hasTalkedToPlayer());
        // An explicit topic uses its own condition/result, not GREETING.
        EXPECT_EQ(ObScript::asInteger(scripts.call("StartConversation", {},
            {player, ObScript::ReferenceValue{key(0x903), {}}}, context, {})), 1);
        EXPECT_EQ(windows.mCalls, 2u);
        EXPECT_EQ(f.stage(), 30);
        f.mActor.getRefData().disable();
        EXPECT_EQ(ObScript::asInteger(scripts.call("StartConversation", {}, {player}, context, {})), 0);
        EXPECT_EQ(windows.mCalls, 2u);
    }
}

TEST(OblivionDialogueMenu, ScriptedPlayerTopicBypassesGreetingAndUsesOriginalResults)
{
    struct Windows : TestingOpenMW::InventoryWindowManager
    {
        std::function<void(const MWWorld::Ptr&)> mOpen;
        unsigned mCalls = 0;
        void pushGuiMode(MWGui::GuiMode mode, const MWWorld::Ptr& actor) override
        {
            EXPECT_EQ(mode, MWGui::GM_Dialogue);
            ++mCalls;
            mOpen(actor);
        }
    };
    for (const bool shifted : {false, true})
    {
        SCOPED_TRACE(shifted);
        Windows windows;
        MenuFixture f(ESM4::INFO_Goodbye, shifted);
        f.mFixture.mEnvironment.setWindowManager(windows);
        f.mFixture.mEnvironment.setDialogueManager(f.mMenu);
        Callback callback;
        windows.mOpen = [&](const auto& actor) { EXPECT_TRUE(f.mMenu.startDialogue(actor, &callback)); };
        auto& scripts = *f.mFixture.mWorld.getOblivionScriptManager();
        ObScript::RuntimeContext context;
        context.mSelf = f.mActor.getCellRef().getFormKey();
        const ObScript::Value player = ObScript::ReferenceValue{ESM::FormKey::dynamic("player", 1), {}};
        const auto start = [&](const ESM::FormKey& topic) {
            return ObScript::asInteger(scripts.call("StartConversation", {},
                {player, ObScript::ReferenceValue{topic, {}}}, context, {}));
        };
        // No eligible response must neither open the UI nor leave a pending root.
        EXPECT_EQ(start(key(0x905)), 0);
        EXPECT_EQ(start(key(0xdead)), 0);
        EXPECT_EQ(windows.mCalls, 0u);
        EXPECT_TRUE(callback.mResponses.empty());
        EXPECT_EQ(f.stage(), 0);
        // The explicit response requires stage20; it ends at30. GREETING has
        // two different responses and would set stage20 again.
        scripts.call("SetStage", {}, {ObScript::ReferenceValue{key(0x900), {}}, std::int64_t(20)}, context, {});
        EXPECT_EQ(start(key(0x903)), 1);
        EXPECT_EQ(windows.mCalls, 1u);
        EXPECT_EQ(callback.mResponses, (std::vector<std::string>{"First response"}));
        EXPECT_TRUE(f.mMenu.isGoodbye());
        EXPECT_EQ(f.stage(), 30);
        EXPECT_EQ(ObScript::asInteger(scripts.call("GetTalkedToPC", {}, {}, context, {})), 1);
        ESM4::RuntimeState state;
        scripts.capture(state);
        EXPECT_NE(std::find(state.mSpokenDialogueInfos.begin(), state.mSpokenDialogueInfos.end(), key(0x904)),
            state.mSpokenDialogueInfos.end());
        EXPECT_EQ(std::find(state.mSpokenDialogueInfos.begin(), state.mSpokenDialogueInfos.end(), key(0x902)),
            state.mSpokenDialogueInfos.end());
        // The override is consumed: an ordinary subsequent opening uses GREETING.
        callback.mResponses.clear();
        EXPECT_TRUE(f.mMenu.startDialogue(f.mActor, &callback));
        EXPECT_EQ(callback.mResponses, (std::vector<std::string>{"First response", "Second response"}));
        EXPECT_EQ(f.stage(), 30); // A lower stage result does not undo original quest progress.
        windows.mOpen = [](const auto&) { throw std::runtime_error("UI opening failed"); };
        EXPECT_THROW(start(key(0x901)), std::runtime_error);
        callback.mResponses.clear();
        EXPECT_TRUE(f.mMenu.startDialogue(f.mActor, &callback));
        EXPECT_EQ(callback.mResponses.size(), 2u);
    }
}

TEST(OblivionDialogueMenu, NativeMessageBoxRetainsButtonsAfterFormatArgumentsAndReadsUiSelection)
{
    struct Windows : TestingOpenMW::InventoryWindowManager
    {
        std::string mText;
        std::vector<std::string> mButtons;
        bool mBlock = true;
        int mChoice = -1;
        void interactiveMessageBox(std::string_view text, const std::vector<std::string>& buttons,
            bool block, int) override
        {
            mText = text;
            mButtons = buttons;
            mBlock = block;
        }
        int readPressedButton() override { return std::exchange(mChoice, -1); }
    };
    for (bool shifted : {false, true})
    {
        Windows windows;
        MenuFixture f(ESM4::INFO_SayOnce, shifted);
        f.mFixture.mEnvironment.setWindowManager(windows);
        auto& scripts = *f.mFixture.mWorld.getOblivionScriptManager();
        ObScript::RuntimeContext context;
        context.mSelf = f.mActor.getCellRef().getFormKey();
        const std::vector<ObScript::Value> exit{
            std::string("Before exiting the sewers, you may revise your character."),
            std::string("Edit Race"), std::string("Edit Birthsign"), std::string("Edit Class"),
            std::string("Finished - Exit Sewers")};
        scripts.call("MessageBox", {}, exit, context, {});
        EXPECT_EQ(windows.mText, std::get<std::string>(exit[0]));
        EXPECT_EQ(windows.mButtons, (std::vector<std::string>{"Edit Race", "Edit Birthsign", "Edit Class",
            "Finished - Exit Sewers"}));
        EXPECT_FALSE(windows.mBlock);
        EXPECT_EQ(ObScript::asInteger(scripts.call("GetButtonPressed", {}, {}, context, {})), -1);
        windows.mChoice = 3;
        auto unrelated = context;
        unrelated.mSelf = ESM::FormKey::dynamic("player", 1);
        EXPECT_EQ(ObScript::asInteger(scripts.call("GetButtonPressed", {}, {}, unrelated, {})), -1);
        EXPECT_EQ(windows.mChoice, 3);
        auto retained = scripts.retainRuntime();
        ASSERT_TRUE(retained.detach());
        EXPECT_EQ(ObScript::asInteger(scripts.call("GetButtonPressed", {}, {}, context, {})), -1);
        EXPECT_EQ(windows.mChoice, 3);
        ASSERT_TRUE(retained.restore());
        EXPECT_EQ(ObScript::asInteger(scripts.call("GetButtonPressed", {}, {}, context, {})), 3);
        EXPECT_EQ(ObScript::asInteger(scripts.call("GetButtonPressed", {}, {}, context, {})), -1);

        // Quest/global calls without a reference use script identity, rather
        // than letting an unrelated quest's unconditional poll steal a result.
        auto global = context;
        global.mSelf = key(0x9000);
        global.mUnit.mOwner = key(0x9001);
        scripts.call("MessageBox", {}, exit, global, {});
        windows.mChoice = 2;
        auto otherGlobal = global;
        otherGlobal.mUnit.mOwner = key(0x9002);
        EXPECT_EQ(ObScript::asInteger(scripts.call("GetButtonPressed", {}, {}, otherGlobal, {})), -1);
        EXPECT_EQ(windows.mChoice, 2);
        auto sameScript = global;
        sameScript.mSelf = key(0x9003);
        EXPECT_EQ(ObScript::asInteger(scripts.call("GetButtonPressed", {}, {}, sameScript, {})), 2);
        EXPECT_EQ(ObScript::asInteger(scripts.call("GetButtonPressed", {}, {}, global, {})), -1);

        const std::vector<ObScript::Value> formatted{std::string("%s: %.0f / %03d (100%%)"),
            std::string("Score"), 2.25, std::int64_t(7), std::string("Continue"), std::string("Cancel")};
        scripts.call("MessageBox", {}, formatted, context, {});
        EXPECT_EQ(windows.mText, "Score: 2 / 007 (100%)");
        EXPECT_EQ(windows.mButtons, (std::vector<std::string>{"Continue", "Cancel"}));
        std::string toast;
        windows.mMessageCallback = [&](std::string_view text, MWGui::ShowInDialogueMode) { toast = text; };
        scripts.call("MessageBox", {}, {std::string("Button = %.0f"), std::int64_t(3)}, context, {});
        EXPECT_EQ(toast, "Button = 3");
        scripts.call("Message", {}, {std::string("Value %d"), std::int64_t(4)}, context, {});
        EXPECT_EQ(toast, "Value 4");
        EXPECT_THROW(scripts.call("MessageBox", {}, {std::string("Value %g")}, context, {}), ObScript::RuntimeError);
        EXPECT_THROW(scripts.call("MessageBox", {}, {std::string("Text"), std::int64_t(4)}, context, {}),
            ObScript::RuntimeError);
    }
}

TEST(OblivionDialogueMenu, NativeWeekdayUsesElapsedDaysAndPcAchievementHasNoSideEffects)
{
    for (bool shifted : {false, true})
    {
        MenuFixture f(ESM4::INFO_SayOnce, shifted);
        auto& world = f.mFixture.mWorld;
        auto& scripts = *world.getOblivionScriptManager();
        ObScript::RuntimeContext context;
        context.mSelf = f.mActor.getCellRef().getFormKey();
        for (const auto [days, expected] : std::vector<std::pair<int, int>>{
                 {0, 0}, {1, 1}, {6, 6}, {7, 0}, {8, 1}, {12, 5}, {365, 1}, {10000, 4}})
        {
            world.setGlobalInt(MWWorld::Globals::sDaysPassed, days);
            EXPECT_EQ(ObScript::asInteger(scripts.call("GetDayOfWeek", {}, {}, context, {})), expected);
        }
        // A changed counter is read immediately, without a cached weekday.
        world.setGlobalInt(MWWorld::Globals::sDaysPassed, 7);
        EXPECT_EQ(ObScript::asInteger(scripts.call("GetDayOfWeek", {}, {}, context, {})), 0);
        // PC AddAchievement must not alter inventory, quests or character state.
        const auto inventory = world.captureOblivionActorInventory(world.getPlayerPtr());
        const auto stage = f.stage();
        const auto sequence = f.sequence();
        EXPECT_NO_THROW(scripts.call("AddAchievement", {}, {std::int64_t(1)}, context, {}));
        EXPECT_EQ(world.captureOblivionActorInventory(world.getPlayerPtr()), inventory);
        EXPECT_EQ(f.stage(), stage);
        EXPECT_EQ(f.sequence(), sequence);
        EXPECT_EQ(ObScript::asInteger(scripts.call("GetDayOfWeek", {}, {}, context, {})), 0);
    }
}

TEST(OblivionDialogueMenu, ScriptAutosaveWaitsForSaveableUiAndRetainsTheOwningGame)
{
    struct Windows : TestingOpenMW::InventoryWindowManager
    {
        bool mAllowed = false;
        bool isSavingAllowed() const override { return mAllowed; }
    } windows;
    MenuFixture f;
    f.mFixture.mEnvironment.setWindowManager(windows);
    struct Saver : MWState::StateManager
    {
        using MWState::StateManager::StateManager;
        std::vector<std::string> mSaves;
        void quickSave(std::string name) override { mSaves.push_back(std::move(name)); }
    } saver(f.mFixture.mDirectory / "autosave-test", {"headless.esm"});
    f.mFixture.mEnvironment.setStateManager(saver);
    auto& scripts = *f.mFixture.mWorld.getOblivionScriptManager();
    ObScript::RuntimeContext context;
    saver.resumeGame();
    scripts.call("Autosave", {}, {}, context, {});
    scripts.call("Autosave", {}, {}, context, {});
    saver.update(0);
    EXPECT_TRUE(saver.mSaves.empty());
    auto retained = saver.retainRuntime();
    ASSERT_TRUE(retained.detach());
    windows.mAllowed = true;
    saver.update(0);
    EXPECT_TRUE(saver.mSaves.empty());
    ASSERT_TRUE(retained.restore());
    saver.update(0);
    EXPECT_EQ(saver.mSaves, (std::vector<std::string>{"Autosave"}));
    saver.update(0);
    EXPECT_EQ(saver.mSaves.size(), 1u);

    // A request from an incoming game cannot appear in the retained original.
    auto incoming = saver.retainRuntime();
    ASSERT_TRUE(incoming.detach());
    saver.resumeGame();
    saver.requestAutosave();
    ASSERT_TRUE(incoming.restore());
    saver.update(0);
    EXPECT_EQ(saver.mSaves.size(), 1u);

    struct RestoreSetting
    {
        bool mValue = Settings::saves().mAutosave;
        ~RestoreSetting() { Settings::saves().mAutosave.set(mValue); }
    } restoreSetting;
    Settings::saves().mAutosave.set(false);
    saver.requestAutosave();
    saver.update(0);
    EXPECT_EQ(saver.mSaves.size(), 1u);
}

TEST(OblivionDialogueMenu, CharacterGenerationHistoryClearsWithPlayer)
{
    MenuFixture f;
    auto& player = f.mFixture.mWorld.getPlayer();
    ESM4::CharacterGenerationState state;
    state.mTracking = true;
    state.mRecommendedClass = key(0x822);
    player.setOblivionCharacterGenerationState(state);
    player.clear();
    EXPECT_FALSE(player.getOblivionCharacterGenerationState());
}

namespace
{
    std::string switchRecords(bool enableParents = false, const std::string& switchScript = {})
    {
        const std::array<std::uint32_t, 6> local{1, 0, 0, 0, 1, 0};
        const std::array<float, 6> position{};
        const auto children = [](unsigned kind, const std::string& data, unsigned label = 0xb04) {
            return bytes(ESM::fourCC("GRUP")) + bytes(static_cast<std::uint32_t>(20 + data.size()))
                + bytes(label) + bytes(kind) + bytes(0u) + data;
        };
        const auto placed = [&](unsigned id, unsigned parent, unsigned flags, bool disabled) {
            auto data = sub(ESM::fourCC("EDID"), std::string("EnableChild") + std::to_string(id) + '\0')
                + sub(ESM::fourCC("NAME"), bytes(0xb00u)) + sub(ESM::fourCC("DATA"), bytes(position));
            if (parent) data += sub(ESM::fourCC("XESP"), bytes(parent) + bytes(flags));
            auto result = record(ESM4::REC_REFR, id, data);
            if (disabled) result.replace(8, 4, bytes(std::uint32_t{ESM4::Rec_Disabled}));
            return result;
        };
        auto switchRef = record(ESM4::REC_REFR, 0xb02, sub(ESM::fourCC("EDID"), std::string("TestSwitchRef\0", 14))
            + sub(ESM::fourCC("NAME"), bytes(0xb00u)) + sub(ESM::fourCC("DATA"), bytes(position))
            + (enableParents ? sub(ESM::fourCC("XESP"), bytes(0xa00u) + bytes(0xc6e000u)) : std::string{}));
        if (enableParents) switchRef.replace(8, 4, bytes(std::uint32_t{ESM4::Rec_Disabled}));
        const auto extras = enableParents ? placed(0xb06, 0xa00, 0xc6e001, false)
            + placed(0xb07, 0xb06, 1, true) : std::string{};
        return record(ESM4::REC_ACTI, 0xb00, sub(ESM::fourCC("EDID"), std::string("TestSwitchBase\0", 15))
                + sub(ESM::fourCC("SCRI"), bytes(0xb01u)))
            + record(ESM4::REC_SCPT, 0xb01, sub(ESM::fourCC("EDID"), std::string("TestSwitchScript\0", 17))
                + sub(ESM::fourCC("SLSD"), bytes(local)) + sub(ESM::fourCC("SCVR"), std::string("calls\0", 6))
                + sub(ESM::fourCC("SCTX"), switchScript.empty() ? "scn TestSwitchScript\nshort calls\nbegin onActivate\n"
                    "if IsActionRef TutorialUserRef == 1\nset calls to calls + 1\nendif\nend\n" : switchScript))
            + children(0, record(ESM4::REC_CELL, 0xb04, sub(ESM::fourCC("EDID"), std::string("SwitchTestRoom\0", 15))
                + sub(ESM::fourCC("DATA"), bytes(std::uint16_t{ESM4::CELL_Interior})))
            + children(6, children(9, switchRef + extras
            + record(ESM4::REC_ACHR, 0xa00, sub(ESM::fourCC("EDID"), std::string("TutorialUserRef\0", 16))
                + sub(ESM::fourCC("NAME"), bytes(0x800u)) + sub(ESM::fourCC("DATA"), bytes(position))))), ESM4::REC_CELL)
            + record(ESM4::REC_PACK, 0xb03, sub(ESM::fourCC("EDID"), std::string("UseSwitch\0", 10))
                + sub(ESM::fourCC("PKDT"), bytes(0u) + std::string{char(8), char(0xff), char(0), char(20)})
                + sub(ESM::fourCC("PSDT"), std::string{char(0xff), char(0xff), char(0), char(0xff)} + bytes(0u))
                + sub(ESM::fourCC("PLDT"), bytes(0u) + bytes(0xb02u) + bytes(0u))
                + sub(ESM::fourCC("PTDT"), bytes(0u) + bytes(0xb02u) + bytes(1u)));
    }

    struct SwitchFixture
    {
        TestingOpenMW::NativeWorldFixture mFixture;
        MWWorld::Ptr mActor, mSwitch;
        explicit SwitchFixture(bool shifted = false, bool enableParents = false,
            const std::string& extraRecords = {}, unsigned extraCount = 0, bool selectPackage = true,
            const std::string& actorScript = {}, const std::string& switchScript = {})
            : mFixture(shifted, false, !actorScript.empty(), actorScript, {}, switchRecords(enableParents, switchScript) + extraRecords,
                (enableParents ? 8 : 6) + extraCount)
        {
            auto& world = mFixture.mWorld;
            auto& store = world.getStore();
            MWClass::Npc::registerSelf();
            MWClass::ESM4Npc::registerSelf();
            MWClass::ESM4Activator::registerSelf();
            world.setupPlayer();
            auto npc = *store.search<ESM4::Npc>(key(0x800));
            const auto file = npc.mId.mContentFile;
            ESM4::Race race{};
            race.mId = {0x810, file};
            race.mAttribMale = {40,40,40,40,40,40,40,40};
            race.mAttribFemale = race.mAttribMale;
            store.getWritable<ESM4::Race>().insertStatic(race, key(0x810));
            npc.mRace = race.mId;
            store.getWritable<ESM4::Npc>().insertStatic(npc, key(0x800));
            ESM4::Class cls{};
            cls.mId = {0xb05, file};
            cls.mData.mFavoredAttributes = {0, 1};
            for (std::size_t i = 0; i < 7; ++i) cls.mData.mMajorSkills[i] = 12 + i;
            store.getWritable<ESM4::Class>().insertStatic(cls, key(0xb05));
            ESM::Race sharedRace;
            sharedRace.blank();
            sharedRace.mId = ESM::RefId(race.mId);
            store.getWritable<ESM::Race>().insertStatic(sharedRace);
            ESM::Class sharedClass;
            sharedClass.blank();
            sharedClass.mId = ESM::RefId(cls.mId);
            store.getWritable<ESM::Class>().insertStatic(sharedClass);
            auto playerBase = *world.getPlayerPtr().get<ESM::NPC>()->mBase;
            playerBase.mRace = ESM::RefId(race.mId);
            playerBase.mClass = ESM::RefId(cls.mId);
            auto prepared = store.preparePlayerRecord(playerBase);
            world.getPlayer().set(prepared.commit());
            auto& cell = world.getWorldModel().getCell(ESM::RefId(ESM::FormId{0xb04, file}));
            world.getPlayer().setCell(&cell);
            mActor = cell.getPtr(ESM::RefId(npc.mId));
            const auto* base = store.search<ESM4::Activator>(key(0xb00));
            mSwitch = cell.getPtr(ESM::RefId(base->mId));
            if (mActor.isEmpty() || mSwitch.isEmpty()) throw std::runtime_error("missing switch fixture");
            world.getWorldModel().registerPtr(mActor);
            world.getWorldModel().registerPtr(mSwitch);
            if (!world.activateOblivionActor(mActor)) throw std::runtime_error("cannot activate switch user");
            if (!selectPackage) return;
            auto& ai = *world.getOblivionAiService();
            if (!ai.addScriptPackage(mActor, key(0xb03))) throw std::runtime_error("missing switch package");
            auto state = capture();
            for (auto& row : state.mActorAi)
                if (row.mActor == key(0xa00)) row.mPhase = ESM4::PackagePhase::Act;
            ai.restore(state);
        }
        ESM4::RuntimeState capture()
        {
            ESM4::RuntimeState state;
            state.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
            state.mPlayer.mCell = key(0xb04);
            state.mPlayer.mRace = key(0x810);
            state.mPlayer.mClass = ESM::FormKey::dynamic("fixture-class", 1);
            mFixture.mWorld.getOblivionAiService()->capture(state);
            return state;
        }
        int calls()
        {
            return mFixture.mWorld.getOblivionScriptManager()->scriptVariable(key(0xb02), 1).value_or(0);
        }
        void ready()
        {
            auto& ai = *mFixture.mWorld.getOblivionAiService();
            for (int i = 0; i < 20; ++i) ai.update(mActor, .1f, true);
        }
    };
}

TEST(OblivionTutorialAction, ScriptedActivationHonorsActivatorAndRunScriptArguments)
{
    for (bool shifted : {false, true})
    {
        SwitchFixture f(shifted, false, {}, 0, false);
        auto& scripts = *f.mFixture.mWorld.getOblivionScriptManager();
        ObScript::RuntimeContext context;
        context.mSelf = key(0xb02);
        context.mActionReference = ESM::FormKey::dynamic("player", 1);
        const ObScript::ReferenceValue actor{key(0xa00), "user"};
        // Explicit reference overrides the callback's Player action reference.
        scripts.call("Activate", {}, {actor, std::int64_t(1)}, context, {});
        EXPECT_EQ(f.calls(), 1);
        scripts.call("Activate", {}, {actor, std::int64_t(0)}, context, {});
        scripts.call("Activate", {}, {actor}, context, {});
        EXPECT_EQ(f.calls(), 1);
        context.mActionReference = key(0xa00);
        scripts.call("Activate", {}, {ObScript::ReferenceValue{{}, "null"}, std::int64_t(1)}, context, {});
        EXPECT_EQ(f.calls(), 2);
        scripts.call("Activate", ObScript::ReferenceValue{{}, "null"}, {actor, std::int64_t(1)}, context, {});
        EXPECT_EQ(f.calls(), 2); // A null parent ends the chain instead of targeting self.
    }
}

TEST(OblivionTutorialAction, ScriptedTrapActivatorCanBeANonActorAndDefaultActivationDoesNotRecurse)
{
    for (bool shifted : {false, true})
    {
        SwitchFixture f(shifted, false, {}, 0, false, {},
            "scn TestSwitchScript\nshort calls\nbegin onActivate\n"
            "if IsActionRef TestSwitchRef == 1\nset calls to calls + 1\nendif\n"
            "Activate\nend\n");
        auto& scripts = *f.mFixture.mWorld.getOblivionScriptManager();
        ObScript::RuntimeContext context;
        context.mSelf = key(0xa00);
        context.mActionReference = ESM::FormKey::dynamic("player", 1);
        const ObScript::ReferenceValue wire{key(0xb02), "wire"};
        scripts.call("Activate", wire, {wire, std::int64_t(1)}, context, {});
        EXPECT_EQ(f.calls(), 1);
        scripts.call("Activate", wire, {wire, std::int64_t(1)}, context, {});
        EXPECT_EQ(f.calls(), 2);
    }
}

TEST(OblivionTutorialAction, DefaultPlayerActivationInsideOnActivateDoesNotReenterScript)
{
    for (bool shifted : {false, true})
    {
        TestingOpenMW::InventoryWindowManager windows;
        int defaultActions = 0;
        windows.mMessageCallback = [&](std::string_view, MWGui::ShowInDialogueMode) { ++defaultActions; };
        SwitchFixture f(shifted, false, {}, 0, false, {},
            "scn TestSwitchScript\nshort calls\nbegin onActivate\n"
            "set calls to calls + 1\nActivate\nend\n");
        f.mFixture.mEnvironment.setWindowManager(windows);
        f.mFixture.mWorld.initializePhysics(new osg::Group);
        auto& scripts = *f.mFixture.mWorld.getOblivionScriptManager();
        ObScript::RuntimeContext context;
        context.mSelf = key(0xb02);
        const ObScript::ReferenceValue player{ESM::FormKey::dynamic("player", 1), "player"};
        scripts.call("Activate", {}, {player, std::int64_t(1)}, context, {});
        EXPECT_EQ(f.calls(), 1);
        scripts.call("Activate", {}, {player, std::int64_t(0)}, context, {});
        EXPECT_EQ(f.calls(), 1);
        EXPECT_EQ(defaultActions, 2);
    }
}

TEST(OblivionTutorialAction, PlacedSwitchUsesActualActorAndCommitsOnceAfterIteration)
{
    SwitchFixture f;
    auto& ai = *f.mFixture.mWorld.getOblivionAiService();
    f.ready();
    ASSERT_EQ(ai.state(f.mActor)->mPhase, ESM4::PackagePhase::Wait);
    ASSERT_TRUE(ai.state(f.mActor)->mActionReserved);
    EXPECT_EQ(f.calls(), 0);
    const auto waiting = f.capture();
    ai.restore(waiting); // A pending timed action survives save-state restoration.
    ai.commitPendingTransitions();
    EXPECT_EQ(f.calls(), 1);
    EXPECT_EQ(ai.state(f.mActor)->mPhase, ESM4::PackagePhase::Complete);
    ai.commitPendingTransitions();
    f.ready();
    ai.commitPendingTransitions();
    EXPECT_EQ(f.calls(), 1);
    const auto completed = f.capture();
    ai.restore(completed);
    ai.commitPendingTransitions();
    EXPECT_EQ(f.calls(), 1);
}

TEST(OblivionTutorialAction, LootContactDoesNotRequireMorrowindDiseaseCatalog)
{
    for (bool shifted : {false, true})
    {
        SwitchFixture f(shifted);
        auto& world = f.mFixture.mWorld;
        ASSERT_EQ(world.getGameProfile(), ESM::GameProfile::Oblivion);
        ASSERT_EQ(world.getStore().get<ESM::GameSetting>().search("fDiseaseXferChance"), nullptr);
        // ActionOpen runs this contact hook before publishing the container UI.
        // Original TES4 content has no Morrowind disease-transfer GMST.
        EXPECT_NO_THROW(MWMechanics::diseaseContact(world.getPlayerPtr(), f.mActor));
    }
}

TEST(OblivionTutorialAction, RestoreResumesNeverSelectedActors)
{
    for (bool shifted : {false, true})
        for (bool highProcess : {false, true})
            for (bool legacyEmptyAction : {false, true})
            {
                SCOPED_TRACE(::testing::Message() << shifted << ":" << highProcess << ":" << legacyEmptyAction);
                SwitchFixture f(shifted, false, {}, 0, false);
                auto& world = f.mFixture.mWorld;
                auto npc = *world.getStore().search<ESM4::Npc>(key(0x800));
                npc.mAIPackageKeys = {key(0xb03)};
                world.getStore().getWritable<ESM4::Npc>().insertStatic(npc, key(0x800));
                auto& ai = *world.getOblivionAiService();
                auto saved = f.capture();
                auto actor = std::ranges::find(saved.mActorAi, key(0xa00), &ESM4::RuntimeActorAiState::mActor);
                ASSERT_NE(actor, saved.mActorAi.end());
                ASSERT_TRUE(actor->mPackage.isNull());
                ASSERT_EQ(actor->mSelectionGeneration, 0u);
                ASSERT_EQ(actor->mPhase, ESM4::PackagePhase::Select);
                // Older restores advanced an empty initial selection into Act.
                if (legacyEmptyAction) actor->mPhase = ESM4::PackagePhase::Act;
                ai.restore(ESM4::RuntimeState::deserializeBinary(saved.serializeBinary()));
                // Promotion may first reconcile the saved low-process position.
                for (int step = 0; step < 2 && ai.state(f.mActor)->mPackage.isNull(); ++step)
                    ai.update(f.mActor, 1.f / 30.f, highProcess);
                ASSERT_EQ(ai.state(f.mActor)->mPackage, key(0xb03));
                EXPECT_EQ(ai.state(f.mActor)->mSource, ESM4::PackageSource::Base);
                EXPECT_GT(ai.state(f.mActor)->mSelectionGeneration, 0u);
            }
}

TEST(OblivionTutorialAction, FollowHoldsWithinAuthoredRangeAndRoutesToTheLeader)
{
    for (const bool shifted : {false, true})
        for (const bool high : {false, true})
        {
            SCOPED_TRACE(shifted);
            SCOPED_TRACE(high);
            const auto follow = record(ESM4::REC_PACK, 0xd60,
                sub(ESM::fourCC("PKDT"), bytes(0u) + std::string{char(1), char(0xff), char(0), char(20)})
                + sub(ESM::fourCC("PSDT"), std::string{char(0xff), char(0xff), char(0), char(0xff)} + bytes(0u))
                + sub(ESM::fourCC("PTDT"), bytes(0u) + bytes(0x14u) + bytes(350u)));
            SwitchFixture f(shifted, false, follow, 1, false);
            auto& world = f.mFixture.mWorld;
            MWMechanics::MechanicsManager mechanics;
            f.mFixture.mEnvironment.setMechanicsManager(mechanics);
            world.initializePhysics(new osg::Group);
            auto pack = *world.getStore().search<ESM4::AIPackage>(key(0xd60));
            // Only Oblivion.esm:14 is a native Player alias; this fixture
            // is headless.esm, so bind its target to the runtime identity.
            pack.mTargetData.mReferenceKey = ESM::FormKey::dynamic("player", 1);
            world.getStore().getWritable<ESM4::AIPackage>().insertStatic(pack, key(0xd60));
            auto& ai = *world.getOblivionAiService();
            const auto initial = f.mActor.getRefData().getPosition().asVec3();
            ASSERT_TRUE(ai.addScriptPackage(f.mActor, key(0xd60)));
            EXPECT_EQ(ai.state(f.mActor)->mDestinationPosition.asVec3(), initial);
            for (int n = 0; n < 8; ++n) ai.update(f.mActor, .25f, high);
            EXPECT_EQ(ai.state(f.mActor)->mPhase, ESM4::PackagePhase::Wait);
            EXPECT_EQ(f.mActor.getRefData().getPosition().asVec3(), initial);
            auto position = world.getPlayerPtr().getRefData().getPosition();
            position.pos[0] = 1000;
            world.getPlayerPtr().getRefData().setPosition(position);
            for (int n = 0; n < 8; ++n) ai.update(f.mActor, .25f, high);
            EXPECT_EQ(ai.state(f.mActor)->mDestinationPosition.asVec3(), position.asVec3());
            EXPECT_EQ(ai.state(f.mActor)->mTarget, ESM::FormKey::dynamic("player",1));
            EXPECT_EQ(ai.state(f.mActor)->mPackage, key(0xd60));
        }
}

TEST(OblivionTutorialAction, RemoveScriptPackageResumesAuthoredListAndContinuesOriginalResult)
{
    for (const bool shifted : {false, true})
        for (const bool hasBase : {false, true})
        {
            SCOPED_TRACE(shifted);
            SCOPED_TRACE(hasBase);
            const auto pack = [&](unsigned id, unsigned flags, char type) {
                return record(ESM4::REC_PACK, id,
                    sub(ESM::fourCC("PKDT"), bytes(flags) + std::string{type, char(0xff), char(0), char(20)})
                    + sub(ESM::fourCC("PSDT"), std::string{char(0xff), char(0xff), char(0), char(0xff)} + bytes(0u))
                    + sub(ESM::fourCC("PLDT"), bytes(0u) + bytes(0xb02u) + bytes(0u)));
            };
            SwitchFixture f(shifted, false, pack(0xd60, 4, 9) + pack(0xd61, 0, 6), 2, false,
                "scn ActivationReceiver\nshort calls\nbegin onActivate\n"
                "RemoveScriptPackage\nEvaluatePackage\nset calls to calls + 1\nend\n");
            auto& world = f.mFixture.mWorld;
            auto npc = *world.getStore().search<ESM4::Npc>(key(0x800));
            npc.mAIPackageKeys = hasBase ? std::vector{key(0xd61)} : std::vector<ESM::FormKey>{};
            world.getStore().getWritable<ESM4::Npc>().insertStatic(npc, key(0x800));
            auto& ai = *world.getOblivionAiService();
            ASSERT_TRUE(ai.addScriptPackage(f.mActor, key(0xd60)));
            ASSERT_EQ(ai.state(f.mActor)->mScriptPackage, key(0xd60));
            auto saved = f.capture();
            ai.restore(saved); // An override restored from a save is removable too.
            auto& scripts = *world.getOblivionScriptManager();
            ASSERT_TRUE(scripts.dispatchObjectEvent(f.mActor, "onactivate"));
            EXPECT_EQ(scripts.scriptVariable(f.mActor.getCellRef().getFormKey(), 1), 1.f);
            ASSERT_TRUE(ai.state(f.mActor)->mScriptPackage.isNull());
            EXPECT_EQ(ai.state(f.mActor)->mPackage, hasBase ? key(0xd61) : ESM::FormKey{});
            const auto generation = ai.state(f.mActor)->mSelectionGeneration;
            ObScript::RuntimeContext context;
            context.mSelf = f.mActor.getCellRef().getFormKey();
            EXPECT_EQ(ObScript::asInteger(scripts.call("RemoveScriptPackage", {}, {}, context, {})), 0);
            EXPECT_EQ(ai.state(f.mActor)->mSelectionGeneration, generation);
            saved = f.capture();
            ai.restore(saved);
            EXPECT_TRUE(ai.state(f.mActor)->mScriptPackage.isNull());
            EXPECT_EQ(ai.state(f.mActor)->mPackage, hasBase ? key(0xd61) : ESM::FormKey{});
        }
}

TEST(OblivionTutorialAction, WaitingWanderReevaluatesHigherPriorityConditionalPackage)
{
    for (bool shifted : {false, true})
    {
        ESM4::AIPackage::CTDA condition{};
        condition.fnIndex = 1; // GetDistance the colocated switch == 0
        condition.param1 = 0xb02;
        const auto pack = [&](unsigned id, char type, const std::string& conditions) {
            return record(ESM4::REC_PACK, id,
                sub(ESM::fourCC("PKDT"), bytes(0u) + std::string{type, char(0xff), char(0), char(20)})
                + sub(ESM::fourCC("PSDT"), std::string{char(0xff), char(0xff), char(0), char(0xff)} + bytes(0u))
                + sub(ESM::fourCC("PTDT"), bytes(0u) + bytes(0xb02u) + bytes(200u)) + conditions);
        };
        SwitchFixture f(shifted, false, pack(0xd60, 0, sub(ESM::fourCC("CTDA"), bytes(condition)))
            + pack(0xd61, 5, {}), 2);
        auto& world = f.mFixture.mWorld;
        world.initializePhysics(new osg::Group);
        auto npc = *world.getStore().search<ESM4::Npc>(key(0x800));
        npc.mAIPackageKeys = {key(0xd60), key(0xd61)};
        world.getStore().getWritable<ESM4::Npc>().insertStatic(npc, key(0x800));
        auto& ai = *world.getOblivionAiService();
        auto saved = f.capture();
        auto actor = std::ranges::find(saved.mActorAi, key(0xa00), &ESM4::RuntimeActorAiState::mActor);
        ASSERT_NE(actor, saved.mActorAi.end());
        actor->mScriptPackage = {};
        actor->mPackage = key(0xd61);
        actor->mPackageType = ESM4::AIPackageType::Wander;
        actor->mProcedure = ESM4::PackageProcedure::Wander;
        actor->mListIndex = 1;
        actor->mSource = ESM4::PackageSource::Base;
        actor->mPhase = ESM4::PackagePhase::Wait;
        actor->mDurationRemaining = 24;
        ai.restore(saved);
        // No explicit EvaluatePackage: normal updates must notice eligibility.
        for (int n = 0; n < 40 && ai.state(f.mActor)->mPackage != key(0xd60); ++n)
            ai.update(f.mActor, .05f, false);
        ASSERT_EQ(ai.state(f.mActor)->mPackage, key(0xd60));
    }
}

TEST(OblivionTutorialAction, FindActivatorPreservesPendingActionAndDispatchesOnce)
{
    for (const bool shifted : {false, true})
    {
        const auto find = record(ESM4::REC_PACK, 0xd60,
            sub(ESM::fourCC("EDID"), std::string("FindSwitch\0", 11))
            + sub(ESM::fourCC("PKDT"), bytes(0u) + std::string{char(0), char(0xff), char(0), char(20)})
            + sub(ESM::fourCC("PSDT"), std::string{char(0xff), char(0xff), char(0), char(0xff)} + bytes(0u))
            + sub(ESM::fourCC("PTDT"), bytes(0u) + bytes(0xb02u) + bytes(200u)));
        SwitchFixture f(shifted, false, find, 1);
        auto& ai = *f.mFixture.mWorld.getOblivionAiService();
        ASSERT_TRUE(ai.addScriptPackage(f.mActor, key(0xd60)));
        auto saved = f.capture();
        auto actor = std::ranges::find(saved.mActorAi, key(0xa00), &ESM4::RuntimeActorAiState::mActor);
        ASSERT_NE(actor, saved.mActorAi.end());
        actor->mPhase = ESM4::PackagePhase::Path;
        actor->mTarget = key(0xb02);
        actor->mTier = ESM4::ProcessTier::High;
        auto position = f.mActor.getRefData().getPosition();
        position.pos[0] = 100; // Outside path-arrival tolerance, inside activation range.
        f.mActor.getRefData().setPosition(position);
        actor->mLastValidPosition = position;
        ai.restore(saved);
        ai.update(f.mActor, .05f, true);
        ASSERT_EQ(ai.state(f.mActor)->mPhase, ESM4::PackagePhase::Arrive);
        saved = f.capture();
        actor = std::ranges::find(saved.mActorAi, key(0xa00), &ESM4::RuntimeActorAiState::mActor);
        actor->mPhase = ESM4::PackagePhase::Act;
        actor->mTarget = key(0xb02);
        actor->mTier = ESM4::ProcessTier::Low;
        ai.restore(saved);
        ai.update(f.mActor, .05f, false);
        ai.commitPendingTransitions();
        ASSERT_EQ(ai.state(f.mActor)->mPhase, ESM4::PackagePhase::Act);
        EXPECT_EQ(f.calls(), 0);
        saved = f.capture();
        actor = std::ranges::find(saved.mActorAi, key(0xa00), &ESM4::RuntimeActorAiState::mActor);
        actor->mTier = ESM4::ProcessTier::High;
        ai.restore(ESM4::RuntimeState::deserializeBinary(saved.serializeBinary()));
        ai.commitPendingTransitions();
        EXPECT_EQ(f.calls(), 1);
        EXPECT_EQ(ai.state(f.mActor)->mPhase, ESM4::PackagePhase::Complete);
        ai.restore(ESM4::RuntimeState::deserializeBinary(f.capture().serializeBinary()));
        ai.commitPendingTransitions();
        EXPECT_EQ(f.calls(), 1);
    }
}

TEST(OblivionTutorialAction, SwitchDisabledOrMovedAfterReservationCannotActivate)
{
    for (const bool disabled : {false, true})
    {
        SwitchFixture f;
        auto& ai = *f.mFixture.mWorld.getOblivionAiService();
        f.ready();
        if (disabled) f.mSwitch.getRefData().disable();
        else
        {
            auto position = f.mSwitch.getRefData().getPosition();
            position.pos[0] = 10000.f;
            f.mSwitch.getRefData().setPosition(position);
        }
        ai.commitPendingTransitions();
        EXPECT_EQ(f.calls(), 0);
        EXPECT_EQ(ai.state(f.mActor)->mPhase, ESM4::PackagePhase::Interrupted);
    }
}

TEST(OblivionTutorialAction, PreviouslyUnresolvedSwitchRestartsWhenItsActualTargetIsAvailable)
{
    for (const auto reason : {"interrupted", "action-target-unresolved"})
    {
        SwitchFixture f;
        auto& ai = *f.mFixture.mWorld.getOblivionAiService();
        auto state = f.capture();
        for (auto& row : state.mActorAi)
            if (row.mActor == key(0xa00))
            {
                row.mPhase = ESM4::PackagePhase::Interrupted;
                row.mInterruptionReason = reason;
            }
        ai.restore(state);
        // One fixed step proves restart. The real course covers locomotion.
        ai.update(f.mActor, 1.f / 30.f, true);
        EXPECT_EQ(ai.state(f.mActor)->mPhase, ESM4::PackagePhase::Resolve);
        EXPECT_EQ(f.calls(), 0);
    }
}

namespace
{
    struct AnimatedDoorFixture : SwitchFixture
    {
        MWWorld::Ptr mDoor;
        explicit AnimatedDoorFixture(bool openByDefault = false)
        {
            auto& world = mFixture.mWorld;
            auto& store = world.getStore();
            MWClass::ESM4Door::registerSelf();
            const auto file = store.search<ESM4::Npc>(key(0x800))->mId.mContentFile;
            ESM4::Door base{};
            base.mId = {0xc00, file};
            store.getWritable<ESM4::Door>().insertStatic(base, key(0xc00));
            ESM4::Reference ref{};
            ref.mOpenByDefault = openByDefault;
            ref.mIsLocked = true;
            ref.mLockLevel = 100;
            ref.mId = {0xc01, file};
            ref.mFormKey = key(0xc01);
            ref.mBaseObj = base.mId;
            ref.mBaseKey = key(0xc00);
            ref.mParent = mActor.getCell()->getCell()->getId();
            ref.mParentKey = key(0xb04);
            store.getWritable<ESM4::Reference>().insertStatic(ref, ref.mFormKey);
            MWWorld::LiveCellRef<ESM4::Door> live(ref, store.search<ESM4::Door>(key(0xc00)));
            mDoor = MWWorld::Ptr(mActor.getCell()->insert(&live), mActor.getCell());
            world.getWorldModel().registerPtr(mDoor);
        }
        int query()
        {
            ObScript::RuntimeContext context;
            context.mSelf = key(0xc01);
            return ObScript::asInteger(mFixture.mWorld.getOblivionScriptManager()->call(
                "GetOpenState", {}, {}, context, {}));
        }
    };
}

TEST(OblivionTutorialDoor, RestoredAnimationOwnerReportsDoorTransitionAndEndpointWithoutReferenceRotation)
{
    AnimatedDoorFixture f;
    auto& saved = f.mDoor.getRefData().getAnimationState().mScriptedAnims;
    saved.emplace_back();
    auto& pose = saved.front();
    for (const auto group : {"open", "close"})
    {
        pose.mGroup = group;
        pose.mAbsolute = false;
        for (const float progress : {0.f, .5f, 1.f})
        {
            pose.mTime = progress;
            const bool open = pose.mGroup == "open";
            EXPECT_EQ(f.query(), open ? (progress == 1 ? 1 : 2) : (progress == 1 ? 3 : 4));
            EXPECT_FLOAT_EQ(f.mDoor.getRefData().getPosition().rot[2], 0.f);
        }
    }
}

TEST(OblivionTutorialDoor, HingeFallbackAndUnknownAbsolutePoseDoNotInventAnimatedState)
{
    AnimatedDoorFixture f;
    auto& world = f.mFixture.mWorld;
    EXPECT_FALSE(MWWorld::oblivionAnimatedDoorState(world, f.mDoor));
    EXPECT_EQ(f.query(), 3);
    auto position = f.mDoor.getRefData().getPosition();
    position.rot[2] = .5f;
    f.mDoor.getRefData().setPosition(position);
    EXPECT_EQ(f.query(), 1);
    f.mDoor.getClass().setDoorState(f.mDoor, MWWorld::DoorState::Opening);
    EXPECT_EQ(f.query(), 2);
    f.mDoor.getClass().setDoorState(f.mDoor, MWWorld::DoorState::Closing);
    EXPECT_EQ(f.query(), 4);
    f.mDoor.getClass().setDoorState(f.mDoor, MWWorld::DoorState::Idle);
    auto& saved = f.mDoor.getRefData().getAnimationState().mScriptedAnims;
    saved.emplace_back();
    saved.front().mGroup = "open";
    saved.front().mTime = 1;
    saved.front().mAbsolute = true;
    EXPECT_FALSE(MWWorld::oblivionAnimatedDoorState(world, f.mDoor));
    EXPECT_EQ(f.query(), 1); // The hinge still owns the rotated reference.
    saved.front().mAbsolute = false;
    f.mDoor.getClass().setDoorState(f.mDoor, MWWorld::DoorState::Closing);
    EXPECT_EQ(f.query(), 4); // A real hinge transition overrides an old saved pose.
    f.mDoor.getClass().setDoorState(f.mDoor, MWWorld::DoorState::Idle);
    saved.front().mGroup = "forward";
    EXPECT_FALSE(MWWorld::oblivionAnimatedDoorState(world, f.mDoor));
    EXPECT_FALSE(MWWorld::oblivionAnimatedDoorState(world, f.mSwitch));
}

TEST(OblivionDialogueMenu, FactionReactionCommandChangesOnlyTheDirectedWinningPairIncludingZero)
{
    for (const bool shifted : {false, true})
    {
        SwitchFixture f(shifted);
        auto& world = f.mFixture.mWorld;
        const auto* npc = world.getStore().search<ESM4::Npc>(key(0x800));
        ESM4::Faction a{}, b{};
        a.mId = {0xd00,npc->mId.mContentFile}; a.mFormKey = key(0xd00); a.mEditorId = "FactionA";
        b.mId = {0xd01,npc->mId.mContentFile}; b.mFormKey = key(0xd01); b.mEditorId = "FactionB";
        a.mRelationships = {{b.mFormKey, -40}};
        b.mRelationships = {{a.mFormKey, 17}};
        world.getStore().getWritable<ESM4::Faction>().insertStatic(a,a.mFormKey);
        world.getStore().getWritable<ESM4::Faction>().insertStatic(b,b.mFormKey);
        auto* scripts = world.getOblivionScriptManager();
        ObScript::RuntimeContext context;
        std::vector<ObScript::Value> args{ObScript::ReferenceValue{a.mFormKey,{}},ObScript::ReferenceValue{b.mFormKey,{}},std::int64_t(30)};
        EXPECT_EQ(world.getOblivionFactionReaction(a.mFormKey,b.mFormKey), -40);
        for (const auto value : {30, -100, 0})
        {
            args[2] = std::int64_t(value);
            scripts->call("SetFactionReaction", {}, args, context, {});
            EXPECT_EQ(world.getOblivionFactionReaction(a.mFormKey,b.mFormKey), value);
            EXPECT_EQ(world.getOblivionFactionReaction(b.mFormKey,a.mFormKey), 17);
            const std::vector<ObScript::Value> query{ObScript::ReferenceValue{a.mFormKey,{}},ObScript::ReferenceValue{b.mFormKey,{}}};
            EXPECT_EQ(ObScript::asInteger(scripts->call("GetFactionReaction", {}, query, context, {})), value);
        }
        const auto snapshot = world.captureOblivionRuntimeState();
        EXPECT_EQ(snapshot.mFactionReactions.at({a.mFormKey,b.mFormKey}), 0);
        EXPECT_EQ(ESM4::RuntimeState::deserializeBinary(snapshot.serializeBinary()).mFactionReactions,
            snapshot.mFactionReactions);
        EXPECT_NO_THROW(world.prepareOblivionSaveState(snapshot));
        auto bad = snapshot;
        bad.mFactionReactions[{a.mFormKey,key(0x800)}] = 30;
        EXPECT_THROW(world.prepareOblivionSaveState(bad), std::runtime_error);
        EXPECT_EQ(world.getStore().search<ESM4::Faction>(a.mFormKey)->mRelationships.front().mModifier,-40);
        EXPECT_FALSE(world.setOblivionFactionReaction(a.mFormKey,key(0x800),30));
        EXPECT_FALSE(world.getOblivionFactionReaction(a.mFormKey,key(0x800)));
    }
}

TEST(OblivionDialogueMenu, ProcessAlertCommandAndConditionShareSavedStateWithoutUsingAlarm)
{
    for (bool shifted : {false,true})
    {
        SwitchFixture f(shifted);
        auto& world = f.mFixture.mWorld;
        auto& combat = *world.getOblivionCombatService();
        auto* scripts = world.getOblivionScriptManager();
        ObScript::RuntimeContext context; context.mSelf = f.mActor.getCellRef().getFormKey();
        auto& stats = f.mActor.getClass().getCreatureStats(f.mActor);
        stats.setAlarmed(true);
        const auto query = [&]() { return ObScript::asInteger(scripts->call("GetIsAlerted", {}, {}, context, {})); };
        EXPECT_EQ(query(),0);
        for (const auto raw : {1,2,255,0,-1,std::numeric_limits<std::int32_t>::min(),
                 std::numeric_limits<std::int32_t>::max()})
        {
            const std::vector<ObScript::Value> args{std::int64_t(raw)};
            scripts->call("SetAlert", {}, args, context, {});
            EXPECT_EQ(query(),raw>0);
            EXPECT_TRUE(stats.isAlarmed());
            ESM4::PackageCondition condition{};
            condition.mFunction = 91; condition.mOperator = ESM4::ConditionOperator::Equal;
            condition.mComparisonValue = raw>0;
            EXPECT_EQ(world.getOblivionAiService()->evaluateDialogueConditions(
                f.mActor,world.getPlayerPtr(),std::span(&condition,1)),ESM4::ConditionResult::True);
        }
        auto low = *combat.findActorValues(context.mSelf);
        low.mProcess = ESM4::ActorValueProcess::Low;
        low.mProcessAction.reset(); low.mProcessKnockedState.reset();
        combat.publishNonPlayerValues(f.mActor,low);
        EXPECT_TRUE(world.setOblivionAlerted(context.mSelf,1));
        EXPECT_TRUE(world.getOblivionAlerted(context.mSelf));
        const auto snapshot = world.captureOblivionRuntimeState();
        const auto restored = ESM4::RuntimeState::deserializeBinary(snapshot.serializeBinary());
        const auto owner = std::find_if(restored.mNativeActorValues.begin(),restored.mNativeActorValues.end(),
            [&](const auto& row) { return row.mActor==context.mSelf; });
        ASSERT_NE(owner,restored.mNativeActorValues.end()); EXPECT_EQ(owner->mProcessAlerted,true);
        EXPECT_NO_THROW(world.prepareOblivionSaveState(restored));
        context.mSelf = key(0xdead);
        EXPECT_FALSE(world.setOblivionAlerted(context.mSelf,1));
        const std::vector<ObScript::Value> noProcess{std::int64_t(1)};
        scripts->call("SetAlert", {}, noProcess, context, {});
        EXPECT_EQ(query(),0);
        EXPECT_EQ(combat.findActorValues(context.mSelf),nullptr);
    }
}

TEST(OblivionEnableParent, NativeBitZeroOverridesOwnFlagAndCyclesAreExplicit)
{
    const auto parent = key(0xc00), child = key(0xc01), grandchild = key(0xc02);
    for (bool parentEnabled : {false, true}) for (bool ownEnabled : {false, true})
    for (unsigned flags : {0u, 1u, 2u, 3u, 0xc6e000u, 0xc6e001u})
    {
        std::map<ESM::FormKey, MWWorld::OblivionEnableReference> nodes{
            {parent, {parentEnabled, {}, false}}, {child, {ownEnabled, parent, (flags & 1u) != 0}},
            {grandchild, {false, child, true}}};
        const auto lookup = [&](const auto& k) -> std::optional<MWWorld::OblivionEnableReference> {
            const auto found = nodes.find(k);
            return found == nodes.end() ? std::nullopt : std::optional(found->second);
        };
        const bool expected = parentEnabled != bool(flags & 1u);
        EXPECT_EQ(MWWorld::resolveOblivionEnableState(child, lookup), expected);
        EXPECT_EQ(MWWorld::resolveOblivionEnableState(grandchild, lookup), !expected);
        nodes.erase(parent);
        EXPECT_EQ(MWWorld::resolveOblivionEnableState(child, lookup), ownEnabled);
        nodes[parent] = {parentEnabled, grandchild, false};
        EXPECT_THROW(MWWorld::resolveOblivionEnableState(child, lookup), std::runtime_error);
    }
}

TEST(OblivionEnableParent, GetParentRefReturnsImmediateEnableParentRatherThanOwningCell)
{
    for (bool shifted : {false, true})
    {
        SwitchFixture f(shifted, true);
        auto& scripts = *f.mFixture.mWorld.getOblivionScriptManager();
        const auto query = [&](unsigned reference) {
            ObScript::RuntimeContext context;
            context.mSelf = key(reference);
            return std::get<ObScript::ReferenceValue>(scripts.call("GetParentRef", {}, {}, context, {})).mKey;
        };
        EXPECT_EQ(query(0xb02), key(0xa00));
        EXPECT_EQ(query(0xb06), key(0xa00)); // Inversion does not change parent identity.
        EXPECT_EQ(query(0xb07), key(0xb06)); // Return the immediate parent, not the root.
        EXPECT_TRUE(query(0xa00).isNull()); // Owning CELL is not an enable parent.
        EXPECT_TRUE(query(0xdead).isNull());

        ObScript::RuntimeContext context;
        context.mSelf = key(0xb02);
        const auto parent = scripts.call("GetParentRef", {}, {}, context, {});
        EXPECT_NO_THROW(scripts.call("EvaluatePackage", parent, {}, context, {}));
    }
}

TEST(OblivionEnableParent, LoadedLinksScriptQueriesAndSavedFlagsFollowRootChanges)
{
    for (bool shifted : {false, true})
    {
        SwitchFixture f(shifted, true);
        auto& world = f.mFixture.mWorld;
        auto& cell = *f.mActor.getCell();
        const auto resident = [&](unsigned id) {
            MWWorld::Ptr result;
            cell.forEach([&](const MWWorld::Ptr& ptr) {
                if (ptr.getCellRef().getFormKey() == key(id)) result = ptr;
                return true;
            }, true);
            return result;
        };
        const auto inverse = resident(0xb06), grandchild = resident(0xb07);
        ASSERT_FALSE(inverse.isEmpty()); ASSERT_FALSE(grandchild.isEmpty());
        EXPECT_FALSE(f.mSwitch.getRefData().isEnabled());
        EXPECT_EQ(world.getOblivionReferenceEnabled(key(0xb02)), true);
        world.synchronizeOblivionEnableParents(&cell);
        const auto check = [&](bool rootEnabled) {
            EXPECT_EQ(f.mActor.getRefData().isEnabled(), rootEnabled);
            EXPECT_EQ(f.mSwitch.getRefData().isEnabled(), rootEnabled);
            EXPECT_EQ(inverse.getRefData().isEnabled(), !rootEnabled);
            EXPECT_EQ(grandchild.getRefData().isEnabled(), rootEnabled);
            const auto saved = world.captureOblivionRuntimeState();
            for (const auto& ptr : {f.mSwitch, inverse, grandchild})
            {
                const auto k = ptr.getCellRef().getFormKey();
                ObScript::RuntimeContext context; context.mSelf = k;
                EXPECT_EQ(ObScript::asInteger(world.getOblivionScriptManager()->call(
                    "GetDisabled", {}, {}, context, {})), !ptr.getRefData().isEnabled());
                const auto found = std::find_if(saved.mReferences.begin(), saved.mReferences.end(),
                    [&](const auto& row) { return row.mKey == k; });
                ASSERT_NE(found, saved.mReferences.end());
                EXPECT_EQ(found->mEnabled, ptr.getRefData().isEnabled());
            }
            EXPECT_NO_THROW(world.prepareOblivionSaveState(saved));
        };
        check(true);
        ObScript::RuntimeContext root; root.mSelf = f.mActor.getCellRef().getFormKey();
        world.getOblivionScriptManager()->call("Disable", {}, {}, root, {});
        check(false);
        auto& ai = *world.getOblivionAiService();
        world.getWorldModel().deregisterLiveCellRef(*f.mActor.mRef);
        ASSERT_NE(ai.state(root.mSelf), nullptr);
        const auto before = *ai.state(root.mSelf);
        ai.updateUnloaded(1.f);
        const auto after = *ai.state(root.mSelf);
        EXPECT_EQ(after.mNextLowProcessTick, before.mNextLowProcessTick);
        EXPECT_EQ(after.mSelectionGeneration, before.mSelectionGeneration);
        EXPECT_EQ(after.mActionTimer, before.mActionTimer);
        world.getOblivionScriptManager()->call("Enable", {}, {}, root, {});
        check(true);
    }
}

namespace
{
    std::string sameCellTeleportRecords(bool reachableExit, bool foreignCell = false, bool lockedOwner = false)
    {
        const auto group = [](unsigned kind, const std::string& data, unsigned label = 0xb04) {
            return bytes(ESM::fourCC("GRUP")) + bytes(static_cast<std::uint32_t>(20 + data.size()))
                + bytes(label) + bytes(kind) + bytes(0u) + data;
        };
        const auto placed = [&](unsigned id, unsigned base, float x, float z,
                                unsigned target = 0, float landing = 0.f) {
            const std::array<float, 6> position{x,0,z,0,0,0};
            const std::array<float, 6> arrival{landing,0,0,0,0,0};
            auto data = sub(ESM::fourCC("NAME"), bytes(base))
                + sub(ESM::fourCC("DATA"), bytes(position));
            if (target) data += sub(ESM::fourCC("XTEL"), bytes(target) + bytes(arrival));
            if (lockedOwner && id == 0xd01)
            {
                std::string lock(12,'\0');
                lock[0] = 64;
                data += sub(ESM::fourCC("XLOC"),lock) + sub(ESM::fourCC("XOWN"),bytes(0x800u));
            }
            return record(ESM4::REC_REFR, id, data);
        };
        // Two disconnected components. Door model origins are above the
        // floor; XTEL landing positions and PGRD approaches are on the floor.
        std::string points;
        for (float x : (foreignCell ? std::vector<float>{0.f,128.f} : std::vector<float>{0.f,128.f,1024.f,1280.f}))
            points += bytes(x) + bytes(0.f) + bytes(0.f) + std::string{char(1),char(0),char(0),char(0)};
        const std::array<std::int16_t, 4> links{1,0,3,2};
        const auto pathgrid = record(ESM4::REC_PGRD, 0xd04,
            sub(ESM::fourCC("DATA"), bytes(std::int16_t{static_cast<std::int16_t>(foreignCell ? 2 : 4)}))
            + sub(ESM::fourCC("PGRP"), points) + sub(ESM::fourCC("PGRR"), bytes(links).substr(0,foreignCell ? 4 : 8)));
        const auto package = record(ESM4::REC_PACK, 0xd05,
            sub(ESM::fourCC("PKDT"), bytes(4u) + std::string{char(6),char(0),char(0),char(0)})
            + sub(ESM::fourCC("PSDT"), std::string{char(0xff),char(0xff),char(0),char(0xff)} + bytes(0u))
            + sub(ESM::fourCC("PLDT"), bytes(0u) + bytes(0xd03u) + bytes(0u)));
        std::string foreign;
        const auto destination = placed(0xd02,0xd00,1024,160,0xd01,128)
            + placed(0xd03,0xb00,1280,0);
        if (foreignCell)
        {
            std::string destinationPoints;
            for (float x : {1024.f,1280.f})
                destinationPoints += bytes(x) + bytes(0.f) + bytes(0.f) + std::string{char(1),char(0),char(0),char(0)};
            foreign = group(0,record(ESM4::REC_CELL,0xd06,
                sub(ESM::fourCC("EDID"),std::string("TeleportDestination\0",20))
                + sub(ESM::fourCC("DATA"),bytes(std::uint16_t{ESM4::CELL_Interior}))),ESM4::REC_CELL)
                + group(6,group(9,destination + record(ESM4::REC_PGRD,0xd07,
                    sub(ESM::fourCC("DATA"),bytes(std::int16_t{2}))
                    + sub(ESM::fourCC("PGRP"),destinationPoints)
                    + sub(ESM::fourCC("PGRR"),bytes(links).substr(0,4))),0xd06),0xd06);
        }
        return record(ESM4::REC_DOOR, 0xd00, sub(ESM::fourCC("EDID"), std::string("TestTeleportDoor\0",17)))
            + group(6, group(9, placed(0xd01,0xd00,128,160,0xd02,reachableExit ? 1024 : 0)
                + (foreignCell ? std::string{} : destination) + pathgrid)) + foreign + package;
    }
}

TEST(OblivionTutorialDoor, SameCellTeleportBridgesComponentsAndRestartsBeforeDoorWithoutEarlyCompletion)
{
    for (bool shifted : {false,true}) for (bool reachableExit : {false,true})
    {
        SCOPED_TRACE(shifted);
        SCOPED_TRACE(reachableExit);
        SwitchFixture f(shifted,false,sameCellTeleportRecords(reachableExit),6);
        auto& world = f.mFixture.mWorld;
        auto& ai = *world.getOblivionAiService();
        const auto actor = f.mActor.getCellRef().getFormKey();
        ASSERT_TRUE(ai.addScriptPackage(f.mActor,key(0xd05)));
        world.getWorldModel().deregisterLiveCellRef(*f.mActor.mRef);
        for (int i=0;i<80 && ai.state(actor)->mPhase != ESM4::PackagePhase::Door;++i)
            ai.updateUnloaded(.25f);
        if (!reachableExit)
        {
            EXPECT_NE(ai.state(actor)->mPhase,ESM4::PackagePhase::Door);
            EXPECT_TRUE(ai.state(actor)->mLastTransitionDoor.isNull());
            EXPECT_LT(ai.state(actor)->mLastValidPosition.pos[0],256);
            continue;
        }
        ASSERT_EQ(ai.state(actor)->mPhase,ESM4::PackagePhase::Door);
        EXPECT_EQ(ai.state(actor)->mDoor,key(0xd01));
        auto saved = f.capture();
        EXPECT_TRUE(saved.mPendingPackageDone.empty());
        ai.restore(ESM4::RuntimeState::deserializeBinary(saved.serializeBinary()));
        for (int i=0;i<4 && ai.state(actor)->mLastTransitionDoor.isNull();++i) ai.updateUnloaded(.25f);
        ASSERT_EQ(ai.state(actor)->mLastTransitionDoor,key(0xd01));
        EXPECT_EQ(ai.state(actor)->mCell,key(0xb04));
        EXPECT_FLOAT_EQ(ai.state(actor)->mLastValidPosition.pos[0],1024);
        EXPECT_FLOAT_EQ(ai.state(actor)->mLastValidPosition.pos[2],0); // XTEL landing, not model origin160.
        EXPECT_TRUE(f.capture().mPendingPackageDone.empty());
        for (int i=0;i<80 && f.capture().mPendingPackageDone.empty();++i) ai.updateUnloaded(.25f);
        const auto complete = f.capture();
        ASSERT_EQ(complete.mPendingPackageDone.size(),1u);
        EXPECT_EQ(complete.mPendingPackageDone.front().mPackage,key(0xd05));
        EXPECT_NEAR(ai.state(actor)->mLastValidPosition.pos[0],1280,64);
        EXPECT_EQ(ai.state(actor)->mLastTransitionDoor,key(0xd01));
    }
}

namespace
{
    struct BoundDefinitionFixture : SwitchFixture
    {
        explicit BoundDefinitionFixture(bool shifted, std::uint32_t weaponEffect = ESM4::EFI_BWMA)
            : SwitchFixture(shifted)
        {
            auto& store = mFixture.mWorld.getStore();
            auto npc = *store.search<ESM4::Npc>(key(0x800));
            const auto file = npc.mId.mContentFile;
            ESM4::Armor armor{}; armor.mId = {0xe10,file}; armor.mArmorFlags = 0x003c;
            armor.mData.health = 1500; store.getWritable<ESM4::Armor>().insertStatic(armor,key(0xe10));
            armor.mId = {0xe11,file}; armor.mArmorFlags = 0x0003; armor.mData.health = 750;
            store.getWritable<ESM4::Armor>().insertStatic(armor,key(0xe11));
            ESM4::Weapon weapon{}; weapon.mId = {0xe12,file}; weapon.mData.health = 4200;
            store.getWritable<ESM4::Weapon>().insertStatic(weapon,key(0xe12));
            const std::array<std::uint32_t,3> codes{ESM4::EFI_MYTH,ESM4::EFI_MYHL,weaponEffect};
            ESM4::Spell spell{}; spell.mId = {0xe00,file}; spell.mFormKey = key(0xe00);
            spell.mData = ESM4::SpellData{4,0,0,1,{}};
            for (unsigned i=0;i<3;++i)
            {
                ESM4::EffectSetting effect{}; effect.mId = {0xe20+i,file}; effect.mFormKey = key(0xe20+i);
                effect.mEffectCode = codes[i]; effect.mData.emplace();
                effect.mData->mFlags = i==2 ? 0x12912 : 0x22112;
                effect.mData->mAssociatedForm = key(0xe10+i);
                store.getWritable<ESM4::EffectSetting>().insertStatic(effect,key(0xe20+i));
                ESM4::SpellEffect input{}; input.mId = codes[i]; input.mDuration = 1000;
                input.mActorValue = 8; // Associated items, never Health VMOD.
                spell.mEffects.push_back(input);
            }
            store.getWritable<ESM4::Spell>().insertStatic(spell,key(0xe00));
            npc.mSpell = {spell.mId,spell.mId}; // Multiple sources must not duplicate grants.
            store.getWritable<ESM4::Npc>().insertStatic(npc,key(0x800));
        }
    };

    struct LiveBoundFixture : BoundDefinitionFixture
    {
        explicit LiveBoundFixture(bool shifted) : BoundDefinitionFixture(shifted)
        {
            MWClass::Armor::registerSelf();
            MWClass::Weapon::registerSelf();
            auto& store = mFixture.mWorld.getStore();
            for (unsigned i = 0; i < 2; ++i)
            {
                const auto& native = *store.get<ESM4::Armor>().searchStatic(key(0xe10+i));
                ESM::Armor armor;
                armor.blank();
                armor.mId = ESM::RefId(native.mId);
                armor.mData.mType = i ? ESM::Armor::Helmet : ESM::Armor::Cuirass;
                armor.mData.mHealth = native.mData.health;
                store.insertStatic(armor);
            }
            ESM::Weapon weapon;
            weapon.blank();
            weapon.mId = ESM::RefId(store.get<ESM4::Weapon>().searchStatic(key(0xe12))->mId);
            weapon.mData.mType = ESM::Weapon::BluntOneHand;
            weapon.mData.mHealth = 4200;
            store.insertStatic(weapon);
        }
    };
}

TEST(OblivionBoundItems, AbilityDefinitionsResolveItemsAndSlotsWithoutVmodOrDurationReinterpretation)
{
    for (bool shifted : {false,true})
    for (const auto weaponEffect : {ESM4::EFI_BWMA, ESM4::EFI_BWDA})
    {
        BoundDefinitionFixture f(shifted, weaponEffect); auto& store = f.mFixture.mWorld.getStore();
        const auto inputs = MWWorld::resolveOblivionNpcBoundAbilityInputs(store,key(0x800));
        ASSERT_EQ(inputs.size(),1u); EXPECT_EQ(inputs.front().mSpell,key(0xe00));
        ASSERT_EQ(inputs.front().mEffects.size(),3u);
        const std::array<double,3> health{1500,750,4200};
        const std::array<std::uint32_t,3> slots{0x3c,3,ESM4::InventorySlotWeapon};
        for (unsigned i=0;i<3;++i)
        {
            const auto& effect = inputs.front().mEffects[i];
            EXPECT_EQ(effect.mEffectIndex,i); EXPECT_EQ(effect.mItem.mBase,key(0xe10+i));
            EXPECT_EQ(effect.mItem.mBoundSource,(ESM4::BoundItemSource{key(0xe00),i}));
            EXPECT_EQ(effect.mItem.mCount,1); EXPECT_EQ(effect.mItem.mCondition,health[i]);
            EXPECT_EQ(effect.mItem.mEquippedSlots,slots[i]); EXPECT_TRUE(effect.mItem.mOwner.isNull());
        }
        auto spell = *store.get<ESM4::Spell>().searchStatic(key(0xe00));
        for (auto& effect : spell.mEffects) effect.mDuration=0;
        store.getWritable<ESM4::Spell>().insertStatic(spell,key(0xe00));
        EXPECT_EQ(MWWorld::resolveOblivionNpcBoundAbilityInputs(store,key(0x800)),inputs);
        for (unsigned type : {0u,1u})
        {
            spell.mData->mType=type; store.getWritable<ESM4::Spell>().insertStatic(spell,key(0xe00));
            EXPECT_TRUE(MWWorld::resolveOblivionNpcBoundAbilityInputs(store,key(0x800)).empty());
        }
    }
}

TEST(OblivionBoundItems, MalformedOrUnsupportedAssociatedItemDefinitionsReject)
{
    for (unsigned scenario=0;scenario<6;++scenario)
    {
        SCOPED_TRACE(scenario);
        BoundDefinitionFixture f(false); auto& store = f.mFixture.mWorld.getStore();
        auto effect = *store.get<ESM4::EffectSetting>().searchStatic(key(0xe20));
        auto spell = *store.get<ESM4::Spell>().searchStatic(key(0xe00));
        if (scenario==0) effect.mData->mAssociatedForm=key(0xdead);
        if (scenario==1) effect.mData->mFlags=0x12912; // Weapon association for MYTH armor.
        if (scenario==2) spell.mEffects.front().mRange=1;
        if (scenario==3) spell.mEffects.front().mArea=1;
        if (scenario==4) spell.mEffects.push_back({ESM4::EFI_FOHE,1,0,0,0,8,{}});
        if (scenario==5)
        {
            auto armor = *store.get<ESM4::Armor>().searchStatic(key(0xe11));
            armor.mArmorFlags=0x003c; store.getWritable<ESM4::Armor>().insertStatic(armor,key(0xe11));
        }
        store.getWritable<ESM4::EffectSetting>().insertStatic(effect,key(0xe20));
        store.getWritable<ESM4::Spell>().insertStatic(spell,key(0xe00));
        EXPECT_THROW(MWWorld::resolveOblivionNpcBoundAbilityInputs(store,key(0x800)),std::invalid_argument);
    }
}

TEST(OblivionBoundItems, InitialGrantWaitsForPlayerPlacementAndPublishesOnceOnRetry)
{
    for (const bool shifted : {false, true})
    {
        SCOPED_TRACE(shifted);
        LiveBoundFixture f(shifted);
        auto& world = f.mFixture.mWorld;
        auto* cell = world.getPlayerPtr().getCell();
        ASSERT_NE(cell, nullptr);
        world.getPlayer().setCell(nullptr);
        const auto before = world.captureOblivionActorInventory(f.mActor);
        EXPECT_NO_THROW(world.activateOblivionActor(f.mActor));
        EXPECT_EQ(world.captureOblivionActorInventory(f.mActor), before);
        world.getPlayer().setCell(cell);
        world.updateOblivionBoundEquipment(f.mActor);
        const auto granted = world.captureOblivionActorInventory(f.mActor);
        ASSERT_EQ(granted.size(), 3u);
        const auto saved = world.captureOblivionRuntimeState();
        ASSERT_EQ(saved.mNativeBoundAbilities.at(f.mActor.getCellRef().getFormKey()).size(), 1u);
        EXPECT_NO_THROW(saved.serializeBinary());
        world.updateOblivionBoundEquipment(f.mActor);
        EXPECT_EQ(world.captureOblivionActorInventory(f.mActor), granted);
        EXPECT_EQ(world.captureOblivionRuntimeState().mNativeBoundAbilities, saved.mNativeBoundAbilities);
    }
}

TEST(OblivionBoundItems, LiveGrantPublishesOwnershipInventoryCacheAndEquipmentObserverTogether)
{
    for (const bool shifted : {false, true})
    {
        SCOPED_TRACE(shifted);
        LiveBoundFixture f(shifted);
        auto& world = f.mFixture.mWorld;
        // An ordinary item with the same base must remain separate from the
        // effect's new instance, including its original fractional condition.
        ASSERT_EQ(world.oblivionAddActorItem(f.mActor, key(0xe10), 1), 1);
        ASSERT_TRUE(world.oblivionEquipActorItem(f.mActor, key(0xe10), true));
        auto& inventory = f.mActor.getClass().getInventoryStore(f.mActor);
        inventory.getSlot(MWWorld::InventoryStore::Slot_Cuirass)->getCellRef().setNativeItemCondition(121.5f);
        struct Observer : MWWorld::InventoryStoreListener
        {
            MWWorld::World& mWorld;
            MWWorld::Ptr mActor;
            unsigned mCalls = 0;
            Observer(MWWorld::World& world, MWWorld::Ptr actor) : mWorld(world), mActor(actor) {}
            void equipmentChanged() override
            {
                ++mCalls;
                const auto state = mWorld.captureOblivionRuntimeState();
                EXPECT_EQ(state.mVersion, ESM4::CurrentRuntimeStateVersion);
                EXPECT_EQ(state.mNativeBoundAbilities.at(mActor.getCellRef().getFormKey()).size(), 1u);
                EXPECT_NO_THROW(state.serializeBinary());
                EXPECT_EQ(MWClass::ESM4Npc::getEquippedArmor(mActor).size(), 2u);
            }
        } observer(world, f.mActor);
        inventory.setInvListener(&observer);
        ASSERT_TRUE(world.activateOblivionActor(f.mActor));
        EXPECT_EQ(observer.mCalls, 1u);
        auto saved = world.captureOblivionRuntimeState();
        const auto roundtrip = ESM4::RuntimeState::deserializeBinary(saved.serializeBinary());
        EXPECT_EQ(roundtrip.mNativeBoundAbilities, saved.mNativeBoundAbilities);
        const auto& owned = saved.mNativeBoundAbilities.at(f.mActor.getCellRef().getFormKey());
        ASSERT_EQ(owned.size(), 1u);
        ASSERT_EQ(owned[0].mDisplacedEquipment.size(), 1u);
        EXPECT_EQ(owned[0].mDisplacedEquipment[0].mCondition, 121.5);
        const auto snapshot = world.captureOblivionActorInventory(f.mActor);
        ASSERT_EQ(snapshot.size(), 4u);
        EXPECT_EQ(ESM4::inventoryCount(snapshot, key(0xe10)), 2);
        for (auto it = inventory.begin(); it != inventory.end(); ++it)
        {
            EXPECT_EQ(world.getWorldModel().getPtr(it->getCellRef().getRefNum()), *it);
            EXPECT_EQ(it->getContainerStore(), &inventory);
        }
        inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight)->getCellRef().setNativeItemCondition(4190.5f);
        const auto worn = world.captureOblivionActorInventory(f.mActor);
        world.updateOblivionBoundEquipment(f.mActor);
        EXPECT_EQ(world.captureOblivionActorInventory(f.mActor), worn);
        EXPECT_EQ(observer.mCalls, 1u); // No repeat publication or repair.
        inventory.setInvListener(nullptr);
        const ESM::FormKeyResolver resolver(world.getContentFiles());
        EXPECT_NO_THROW(MWWorld::validateOblivionBoundEquipment(saved, world.getStore(), resolver));
        auto bad = saved;
        bad.mNativeBoundAbilities.begin()->second[0].mSpell = key(0xdead);
        EXPECT_THROW(MWWorld::validateOblivionBoundEquipment(bad, world.getStore(), resolver), std::invalid_argument);
        bad = saved;
        for (auto& ref : bad.mReferences)
            if (ref.mKey == f.mActor.getCellRef().getFormKey())
                for (auto& item : ref.mInventory)
                    if (item.mBoundSource) { item.mBase = key(0xe10); item.mBoundSource->mEffectIndex = 99; break; }
        EXPECT_THROW(MWWorld::validateOblivionBoundEquipment(bad, world.getStore(), resolver), std::invalid_argument);
    }
}

TEST(OblivionBoundItems, TerminalDeathRemovesGearButEssentialKnockoutRetainsIt)
{
    for (const bool essential : {false, true})
    {
        SCOPED_TRACE(essential);
        LiveBoundFixture f(false);
        auto& world = f.mFixture.mWorld;
        auto& combat = *world.getOblivionCombatService();
        ASSERT_EQ(world.oblivionAddActorItem(f.mActor, key(0xe10), 1), 1);
        ASSERT_TRUE(world.oblivionEquipActorItem(f.mActor, key(0xe10), true));
        world.updateOblivionBoundEquipment(f.mActor);
        const auto before = world.captureOblivionActorInventory(f.mActor);
        ASSERT_EQ(before.size(), 4u);
        ASSERT_TRUE(combat.killNonPlayer(f.mActor, ESM::FormKey::dynamic("player", 1), essential, {}));
        world.removeOblivionDeadBoundEquipment();
        const auto after = world.captureOblivionActorInventory(f.mActor);
        if (essential)
        {
            EXPECT_EQ(after, before);
            EXPECT_FALSE(world.captureOblivionRuntimeState().mNativeBoundAbilities.empty());
        }
        else
        {
            ASSERT_EQ(after.size(), 1u);
            EXPECT_FALSE(after[0].mBoundSource);
            EXPECT_EQ(after[0].mBase, key(0xe10));
            EXPECT_EQ(after[0].mEquippedSlots, 0x3cu);
            EXPECT_TRUE(world.captureOblivionRuntimeState().mNativeBoundAbilities.empty());
            EXPECT_EQ(MWClass::ESM4Npc::getEquippedArmor(f.mActor).size(), 1u);
            world.updateOblivionBoundEquipment(f.mActor);
            EXPECT_EQ(world.captureOblivionActorInventory(f.mActor), after);
        }
    }
}

TEST(OblivionBoundItems, DeathCleanupIncludesDispositionActorsRegisteredAfterTheGrant)
{
    for (const bool shifted : {false, true})
    {
        SCOPED_TRACE(shifted);
        LiveBoundFixture f(shifted);
        auto& world = f.mFixture.mWorld;
        auto& store = world.getStore();
        world.updateOblivionBoundEquipment(f.mActor);
        ASSERT_FALSE(world.captureOblivionRuntimeState().mNativeBoundAbilities.empty());

        // The prison dialogue changes a newly encountered NPC's disposition
        // after the assassins' bound gear has already been granted.
        auto npc = *store.search<ESM4::Npc>(key(0x800));
        npc.mId.mIndex = 0xe30;
        npc.mFormKey = key(0xe30);
        npc.mSpell.clear();
        store.getWritable<ESM4::Npc>().insertStatic(npc, key(0xe30));
        const auto* base = store.search<ESM4::Npc>(key(0xe30));
        ESM4::ActorCharacter placed{};
        placed.mId = {0xa02, base->mId.mContentFile};
        placed.mFormKey = key(0xa02);
        placed.mBaseObj = base->mId;
        placed.mBaseKey = key(0xe30);
        store.getWritable<ESM4::ActorCharacter>().insertStatic(placed, placed.mFormKey);
        MWWorld::LiveCellRef<ESM4::Npc> live(placed, base);
        auto& cell = *f.mActor.getCell();
        const MWWorld::Ptr lateActor(cell.insert(&live), &cell);
        world.getWorldModel().registerPtr(lateActor);
        ASSERT_TRUE(world.activateOblivionActor(lateActor));
        const auto lateKey = lateActor.getCellRef().getFormKey();
        ASSERT_TRUE(world.modifyOblivionDisposition(lateKey, lateKey, 20));
        const auto before = world.captureOblivionRuntimeState();
        EXPECT_NO_THROW(before.serializeBinary());
        ASSERT_TRUE(world.getOblivionCombatService()->killNonPlayer(f.mActor, lateKey, false, {}));
        ASSERT_NO_THROW(world.removeOblivionDeadBoundEquipment());
        const auto after = world.captureOblivionRuntimeState();
        EXPECT_TRUE(after.mNativeBoundAbilities.empty());
        EXPECT_EQ(after.mDispositionModifiers, before.mDispositionModifiers);
        EXPECT_FALSE(after.mPendingDeathEvents.empty());
        EXPECT_NO_THROW(after.serializeBinary());
        for (const auto& item : world.captureOblivionActorInventory(f.mActor))
            EXPECT_FALSE(item.mBoundSource);
    }
}

TEST(OblivionBoundItems, SourceRemovalRestoresOrdinaryItemAndGenericRemovalRejectsBeforeMutation)
{
    LiveBoundFixture f(false);
    auto& world = f.mFixture.mWorld;
    ASSERT_EQ(world.oblivionAddActorItem(f.mActor, key(0xe10), 1), 1);
    ASSERT_TRUE(world.oblivionEquipActorItem(f.mActor, key(0xe10), true));
    world.updateOblivionBoundEquipment(f.mActor);
    const auto before = world.captureOblivionActorInventory(f.mActor);
    EXPECT_THROW(world.oblivionRemoveActorItem(f.mActor, key(0xe12), 1), std::invalid_argument);
    EXPECT_EQ(world.captureOblivionActorInventory(f.mActor), before);
    auto npc = *world.getStore().search<ESM4::Npc>(key(0x800));
    npc.mSpell.clear();
    world.getStore().getWritable<ESM4::Npc>().insertStatic(npc, key(0x800));
    world.updateOblivionBoundEquipment(f.mActor);
    const auto after = world.captureOblivionActorInventory(f.mActor);
    ASSERT_EQ(after.size(), 1u);
    EXPECT_EQ(after[0].mBase, key(0xe10));
    EXPECT_EQ(after[0].mEquippedSlots, 0x3cu);
    EXPECT_TRUE(world.captureOblivionRuntimeState().mNativeBoundAbilities.empty());
}

TEST(OblivionMeleeScripts, AcquiredHitPublishesDamageBeforeFilteredCallbackAndCannotReplayAfterRestore)
{
    for (const bool shifted : {false, true}) for (unsigned scenario = 0; scenario < 10; ++scenario)
    {
        SCOPED_TRACE(shifted);
        SCOPED_TRACE(scenario);
        const bool miss = scenario == 2 || scenario == 6;
        const bool hostile = scenario == 3 || scenario == 5 || scenario == 6;
        const bool killInScript = scenario == 5;
        const bool playerHit = scenario >= 7;
        const bool stopInScript = scenario == 9;
        const bool matching = scenario != 1;
        const std::string script = "scn ActivationReceiver\nshort calls\nBegin OnHit "
            + std::string(playerHit || !matching ? "player" : "HitSourceRef")
            + "\nif GetAV Health < 100\nset calls to calls + 1\nsetstage MenuQuest 20\n"
            + (killInScript ? "kill\n" : "") + (stopInScript ? "stopcombat\n" : "") + "endif\nEnd\n";
        MenuFixture f(ESM4::INFO_SayOnce, shifted, script);
        auto& fixture = f.mFixture;
        auto& world = fixture.mWorld;
        auto& store = world.getStore();
        auto& combat = *world.getOblivionCombatService();
        auto& scripts = *world.getOblivionScriptManager();
        const auto* base = f.mActor.get<ESM4::Npc>()->mBase;
        auto& cell = world.getWorldModel().getCell(ESM::RefId(ESM::FormId{0xb20, base->mId.mContentFile}));
        MWWorld::Ptr attacker = cell.getPtr(ESM::RefId(base->mId));
        ASSERT_FALSE(attacker.isEmpty());
        f.mActor = f.mActor.getCell()->moveTo(f.mActor, &cell);
        world.getWorldModel().registerPtr(f.mActor);
        world.getPlayer().setCell(&cell);
        world.getWorldModel().registerPtr(attacker);
        ASSERT_EQ(store.findEsm4FormKey("HitSourceRef"), key(0xa01));
        ASSERT_TRUE(world.activateOblivionActor(attacker));
        attacker.getClass().getCreatureStats(attacker).setDrawState(MWMechanics::DrawState::Weapon);
        if (scenario >= 3)
        {
            // StartCombat captures actual World state before creating the first
            // controller, so this fixture also needs a real Player profile.
            ESM4::Class cls{};
            cls.mId = {0xb30, base->mId.mContentFile};
            cls.mData.mFavoredAttributes = {0, 1};
            for (std::size_t i = 0; i < 7; ++i) cls.mData.mMajorSkills[i] = 12 + i;
            store.getWritable<ESM4::Class>().insertStatic(cls, key(0xb30));
            ESM::Race sharedRace; sharedRace.blank();
            sharedRace.mId = ESM::RefId(ESM::FormId{0x810, base->mId.mContentFile});
            store.getWritable<ESM::Race>().insertStatic(sharedRace);
            ESM::Class sharedClass; sharedClass.blank(); sharedClass.mId = ESM::RefId(cls.mId);
            store.getWritable<ESM::Class>().insertStatic(sharedClass);
            auto playerBase = *world.getPlayerPtr().get<ESM::NPC>()->mBase;
            playerBase.mRace = sharedRace.mId; playerBase.mClass = sharedClass.mId;
            auto prepared = store.preparePlayerRecord(playerBase);
            world.getPlayer().set(prepared.commit());
            world.getPlayer().setCell(&cell);
        }

        const auto npcSource = attacker;
        if (playerHit)
        {
            auto playerSource = *base;
            playerSource.mId = {7,base->mId.mContentFile + 1};
            playerSource.mFormKey = ESM::FormKey::content("Oblivion.esm",7);
            store.getWritable<ESM4::Npc>().insertStatic(playerSource,playerSource.mFormKey);
            ASSERT_TRUE(world.initializeOblivionPlayerActor());
            attacker = world.getPlayerPtr();
            attacker.getRefData().setPosition(npcSource.getRefData().getPosition());
            attacker.getClass().getCreatureStats(attacker).setDrawState(MWMechanics::DrawState::Weapon);
        }

        if (scenario == 7)
        {
            ESM4::CombatStyle style{};
            style.mId = {0xd70,base->mId.mContentFile};
            style.mStandard.emplace();
            style.mStandard->mAttackChance = 100;
            style.mStandard->mFlags = 34;
            style.mStandard->mDoNotAcquire = true;
            store.getWritable<ESM4::CombatStyle>().insertStatic(style,key(0xd70));
            auto npc = *store.search<ESM4::Npc>(key(0x800));
            npc.mCombatStyle = style.mId;
            store.getWritable<ESM4::Npc>().insertStatic(npc,key(0x800));
        }

        // A real public Physics/VFS admission supplies the contact bodies.
        // There is no renderer or mocked contact-acquisition result.
        osg::ref_ptr<osg::Geode> model = new osg::Geode;
        osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry;
        osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array;
        for (const float z : {0.f, 128.f})
            for (const auto xy : {osg::Vec2f(-16,-16), osg::Vec2f(16,-16), osg::Vec2f(16,16), osg::Vec2f(-16,16)})
                vertices->push_back(osg::Vec3f(xy.x(), xy.y(), z));
        geometry->setVertexArray(vertices);
        osg::ref_ptr<osg::DrawElementsUInt> triangles = new osg::DrawElementsUInt(GL_TRIANGLES);
        for (const unsigned index : {0u,2u,1u,0u,3u,2u,4u,5u,6u,4u,6u,7u,
                 0u,1u,5u,0u,5u,4u,1u,2u,6u,1u,6u,5u,2u,3u,7u,2u,7u,6u,3u,0u,4u,3u,4u,7u})
            triangles->push_back(index);
        geometry->addPrimitiveSet(triangles);
        model->addDrawable(geometry);
        ASSERT_TRUE(osgDB::writeNodeFile(*model, (fixture.mDirectory / "hit-actor.osgt").string()));
        fixture.mVfs.addArchive(std::make_unique<VFS::FileSystemArchive>(fixture.mDirectory));
        fixture.mVfs.buildIndex();
        auto* scene = fixture.mResources.getSceneManager();
        scene->setShaderPath(std::filesystem::path(OPENMW_PROJECT_SOURCE_DIR) / "files/shaders");
        auto defines = Shader::getDefaultDefines();
        for (const auto& [name, value] : SceneUtil::ShadowManager::getShadowsDisabledDefines())
            defines[name] = value;
        osg::ref_ptr<SceneUtil::LightManager> lights
            = new SceneUtil::LightManager(SceneUtil::LightSettings{}, &fixture.mResources);
        for (const auto& [name, value] : lights->getLightDefines()) defines[name] = value;
        scene->getShaderManager().setGlobalDefines(defines);
        auto& physics = world.initializePhysics(new osg::Group);
        for (const auto& ptr : {attacker, f.mActor})
        {
            ptr.getRefData().setBaseNode(new SceneUtil::PositionAttitudeTransform);
            physics.addActor(ptr, VFS::Path::Normalized("hit-actor.osgt"));
        }
        const auto actor = playerHit ? ESM::FormKey::dynamic("player",1) : attacker.getCellRef().getFormKey();
        const auto victim = f.mActor.getCellRef().getFormKey();
        if (scenario >= 3)
        {
            const std::array residents{npcSource, f.mActor};
            combat.setNonPlayerBaseValue(f.mActor, 34, 100, residents);
            if (playerHit)
            {
                if (scenario != 8) { ASSERT_TRUE(world.executeOblivionStartCombat(f.mActor,npcSource)); }
            }
            else if (hostile) ASSERT_TRUE(world.executeOblivionStartCombat(attacker, f.mActor));
            else combat.engage(actor, victim); // An undirected graph edge is insufficient.
        }
        if (miss) f.mActor.getRefData().disable();
        const auto strike = combat.beginMeleeStrike(actor, ESM4::MeleeStrikeKind::Left, "handtohandattackleft");
        ASSERT_TRUE(combat.bindMeleePlayback(strike, actor));
        ASSERT_TRUE(combat.advanceOrdinaryMeleePhase(strike, actor, 0, .3f, {0,.2f,.6f,1}));
        const float before = combat.getNonPlayerValue(f.mActor, 8);
        const int stageBefore = f.stage();
        ASSERT_EQ(before, 100);
        auto result = MWMechanics::commitOblivionMeleeContact(world, strike, attacker, f.mActor, 200, 0, 0, false);
        ASSERT_TRUE(result);
        EXPECT_EQ(result->mVictim.isEmpty(), miss);
        const float after = combat.getNonPlayerValue(f.mActor, 8);
        if (miss) EXPECT_EQ(after, before);
        else EXPECT_LT(after, before);
        const int expected = matching && !miss ? 1 : 0;
        EXPECT_EQ(scripts.scriptVariable(victim, 1), expected);
        EXPECT_EQ(f.stage(), expected ? 20 : stageBefore);
        const bool responds = (hostile && !miss && !killInScript) || (playerHit && scenario == 7);
        EXPECT_EQ(world.isOblivionCombatTarget(victim, actor).value_or(false), responds);
        if (responds) { EXPECT_EQ(world.getOblivionCombatTarget(victim), actor); }
        if (killInScript) { EXPECT_EQ(combat.findActorLife(victim)->mPhase, ESM4::ActorLifePhase::Dead); }
        if (scenario == 7)
        {
            // The defender's directed target survives the other actor's
            // StopCombat, while the symmetric service edge is deliberately
            // removed. Its next actual AI update must repair bookkeeping.
            ASSERT_TRUE(MWMechanics::updateOblivionMeleeAi(world,f.mActor,true,.05f));
            ASSERT_TRUE(combat.findMeleeState(victim)->mAiIntent);
            ASSERT_TRUE(combat.stopCombat(actor));
            ASSERT_FALSE(combat.isInCombatWith(victim,actor));
            ASSERT_EQ(world.getOblivionCombatTarget(victim),actor);
            ASSERT_TRUE(MWMechanics::updateOblivionMeleeAi(world,f.mActor,true,.05f));
            EXPECT_TRUE(combat.isInCombatWith(victim,actor));
            ASSERT_TRUE(combat.findMeleeState(victim)->mAiIntent);
            EXPECT_EQ(combat.findMeleeState(victim)->mAiIntent->mTarget,actor);
        }
        const auto afterResponseRandom = combat.combatRandomState();
        EXPECT_FALSE(MWMechanics::commitOblivionMeleeContact(world, strike, attacker, f.mActor, 200, 0, 0, false));
        EXPECT_EQ(scripts.scriptVariable(victim, 1), expected);
        EXPECT_EQ(combat.combatRandomState(), afterResponseRandom);
        EXPECT_EQ(world.isOblivionCombatTarget(victim, actor).value_or(false), responds);

        ESM4::RuntimeState saved;
        saved.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
        saved.mPlayer.mCell = key(0xb20);
        saved.mPlayer.mRace = key(0x810);
        saved.mPlayer.mClass = ESM::FormKey::dynamic("fixture-class", 1);
        for (const auto& ptr : {npcSource, f.mActor})
        {
            ESM4::RuntimeReferenceState reference;
            reference.mKey = ptr.getCellRef().getFormKey();
            reference.mBase = key(0x800);
            reference.mCell = key(0xb20);
            reference.mPosition = ptr.getRefData().getPosition();
            reference.mEnabled = ptr.getRefData().isEnabled();
            saved.mReferences.push_back(reference);
        }
        combat.capture(saved);
        scripts.capture(saved);
        const auto restored = ESM4::RuntimeState::deserializeBinary(saved.serializeBinary());
        combat.restore(restored, store);
        scripts.restore(restored);
        EXPECT_FALSE(MWMechanics::commitOblivionMeleeContact(world, strike, attacker, f.mActor, 200, 0, 0, false));
        EXPECT_EQ(combat.getNonPlayerValue(f.mActor, 8), after);
        EXPECT_EQ(scripts.scriptVariable(victim, 1), expected);
    }
}

TEST(OblivionTutorialAction, NativeResurrectionRestoresBaseInventoryAndPreservesPositionAndDeathHistory)
{
    for (const bool shifted : {false, true})
    {
        const auto gold = record(ESM4::REC_MISC, 0xf,
            sub(ESM::fourCC("EDID"), std::string("Gold001\0", 8))
            + sub(ESM::fourCC("DATA"), bytes(1u) + bytes(0.f)));
        SwitchFixture f(shifted, false, gold, 1, false);
        auto& world = f.mFixture.mWorld;
        auto& combat = *world.getOblivionCombatService();
        auto& scripts = *world.getOblivionScriptManager();
        auto& store = world.getStore();
        MWClass::Miscellaneous::registerSelf();
        auto npc = *store.search<ESM4::Npc>(key(0x800));
        const auto goldId = ESM::FormId{0xf, npc.mId.mContentFile}.toUint32();
        npc.mInventory = {{goldId, 3}};
        store.getWritable<ESM4::Npc>().insertStatic(npc, key(0x800));
        auto& target = f.mActor.getClass().getInventoryStore(f.mActor);
        MWWorld::ManualRef goldStack(store, MWWorld::ContainerStore::sGoldId, 17);
        target.clear();
        target.add(goldStack.getPtr(), 17, false);
        ObScript::RuntimeContext context;
        context.mSelf = f.mActor.getCellRef().getFormKey();
        scripts.call("SetEssential", {}, {ObScript::ReferenceValue{key(0x800), {}}, std::int64_t(0)}, context, {});
        const auto health = combat.getNonPlayerValue(f.mActor, 8);
        auto position = f.mActor.getRefData().getPosition();
        position.pos[0] = 1100;
        f.mActor.getRefData().setPosition(position);
        const auto count = [&] { return target.count(MWWorld::ContainerStore::sGoldId); };
        ASSERT_EQ(count(), 17);
        for (const bool preserve : {false, true})
        {
            ASSERT_TRUE(combat.setProcessAlerted(context.mSelf, true));
            ASSERT_TRUE(world.killOblivionActor(f.mActor, ESM::FormKey::dynamic("player", 1)));
            const auto deaths = combat.getDeadCount(key(0x800));
            const auto queued = world.captureOblivionRuntimeState().mPendingDeathEvents;
            ASSERT_TRUE(f.mActor.getClass().getCreatureStats(f.mActor).isDead());
            // A later unresolved base item must reject before publishing any reset.
            npc.mInventory.push_back({ESM::FormId{0xdead, npc.mId.mContentFile}.toUint32(), 1});
            store.getWritable<ESM4::Npc>().insertStatic(npc, key(0x800));
            EXPECT_THROW(scripts.call("Resurrect", {}, {}, context, {}), ObScript::RuntimeError);
            EXPECT_TRUE(f.mActor.getClass().getCreatureStats(f.mActor).isDead());
            EXPECT_EQ(combat.getDeadCount(key(0x800)), deaths);
            EXPECT_EQ(count(), preserve ? 3 : 17);
            npc.mInventory.pop_back();
            store.getWritable<ESM4::Npc>().insertStatic(npc, key(0x800));
            // With no attached body, argument1 follows the original full-reset fallback.
            ASSERT_NO_THROW(scripts.call("Resurrect", {}, {std::int64_t(preserve)}, context, {}));
            EXPECT_FALSE(f.mActor.getClass().getCreatureStats(f.mActor).isDead());
            EXPECT_EQ(combat.getNonPlayerValue(f.mActor, 8), health);
            EXPECT_EQ(count(), 3);
            EXPECT_EQ(f.mActor.getRefData().getPosition().pos[0], 1100);
            EXPECT_EQ(combat.getDeadCount(key(0x800)), deaths);
            EXPECT_EQ(world.captureOblivionRuntimeState().mPendingDeathEvents, queued);
            EXPECT_EQ(combat.findActorValues(context.mSelf)->mProcess, ESM4::ActorValueProcess::Low);
            EXPECT_FALSE(combat.getProcessAlerted(context.mSelf));
            EXPECT_FALSE(combat.findMeleeState(context.mSelf));
            EXPECT_FALSE(combat.actorRagdoll(context.mSelf));
        }
    }
}

TEST(OblivionEssential, ScriptBaseFlagAppliesToCachedAndNewReferencesAndSurvivesRestore)
{
    for (const bool shifted : {false, true})
    {
        SwitchFixture f(shifted);
        auto& world = f.mFixture.mWorld;
        auto& combat = *world.getOblivionCombatService();
        auto* scripts = world.getOblivionScriptManager();
        auto& store = world.getStore();
        ObScript::RuntimeContext context;
        context.mSelf = f.mActor.getCellRef().getFormKey();
        const auto base = key(0x800);
        const bool original = f.mActor.getClass().isEssential(f.mActor); // Cache before changing flag.
        const auto set = [&](bool essential) {
            const std::vector<ObScript::Value> args{ObScript::ReferenceValue{base, {}}, std::int64_t(essential)};
            scripts->call("SetEssential", {}, args, context, {});
        };
        for (const auto raw : {std::numeric_limits<std::int32_t>::min(), -1, 0, 1, 2,
                 std::numeric_limits<std::int32_t>::max()})
        {
            const std::vector<ObScript::Value> args{ObScript::ReferenceValue{base, {}}, std::int64_t(raw)};
            scripts->call("SetEssential", {}, args, context, {});
            EXPECT_EQ(f.mActor.getClass().isEssential(f.mActor), raw > 0);
        }
        set(!original);
        EXPECT_EQ(f.mActor.getClass().isEssential(f.mActor), !original);
        EXPECT_EQ(world.getOblivionEssentialOverride(base), !original);
        EXPECT_EQ(combat.getBaseEssential(base), !original);
        EXPECT_FALSE(world.setOblivionEssential(context.mSelf, true)); // Base scope, never reference alias.
        EXPECT_FALSE(world.setOblivionEssential(key(0xdead), true));
        const auto* npc = store.search<ESM4::Npc>(base);
        EXPECT_EQ((npc->mBaseConfig.tes4.flags & ESM4::Npc::TES4_Essential) != 0, original);
        ESM4::ActorCharacter placed{};
        placed.mId = {0xa02, npc->mId.mContentFile}; placed.mFormKey = key(0xa02);
        placed.mBaseObj = npc->mId; placed.mBaseKey = base;
        store.getWritable<ESM4::ActorCharacter>().insertStatic(placed, placed.mFormKey);
        MWWorld::LiveCellRef<ESM4::Npc> live(placed, npc);
        auto& cell = *f.mActor.getCell();
        const MWWorld::Ptr next(cell.insert(&live), &cell);
        world.getWorldModel().registerPtr(next);
        EXPECT_EQ(next.getClass().isEssential(next), !original);
        const auto snapshot = ESM4::RuntimeState::deserializeBinary(world.captureOblivionRuntimeState().serializeBinary());
        ASSERT_EQ(snapshot.mNativeActorBases.size(), 1u);
        EXPECT_EQ(snapshot.mNativeActorBases.front().mEssential, !original);
        EXPECT_TRUE(snapshot.mNativeActorBases.front().mValues.empty());
        EXPECT_NO_THROW(world.prepareOblivionSaveState(snapshot));
        set(original);
        EXPECT_EQ(next.getClass().isEssential(next), original);
        combat.restore(snapshot, store);
        EXPECT_EQ(f.mActor.getClass().isEssential(f.mActor), !original);
        EXPECT_EQ(next.getClass().isEssential(next), !original);
        auto legacy = snapshot;
        legacy.mNativeActorBases.clear(); legacy.mVersion = 83;
        combat.restore(legacy, store);
        EXPECT_EQ(f.mActor.getClass().isEssential(f.mActor), original);
        set(false);
        ASSERT_TRUE(world.killOblivionActor(f.mActor, {}));
        ASSERT_NE(combat.findActorLife(context.mSelf), nullptr);
        EXPECT_EQ(combat.findActorLife(context.mSelf)->mPhase, ESM4::ActorLifePhase::Dead);
    }
}

TEST(OblivionQuestStages, NestedPrerequisitesCannotLowerStageOrReplayUnrepeatableResultsAfterRestore)
{
    for (const bool shifted : {false, true}) for (const bool repeat : {false, true})
    {
        const auto stage = [&](std::uint16_t number, const std::string& source) {
            return sub(ESM::fourCC("INDX"), bytes(number))
                + sub(ESM::fourCC("QSDT"), std::string(1, '\0'))
                + sub(ESM::fourCC("SCTX"), source);
        };
        const auto quest = record(ESM4::REC_QUST, 0xd10,
            sub(ESM::fourCC("EDID"), std::string("StageFlow\0", 10))
            + sub(ESM::fourCC("DATA"), std::string{char(repeat ? ESM4::Quest::Flag_AllowRepeatStages : 0), char(50)})
            + stage(10, "getstage StageFlow\n")
            + stage(20, "setstage StageFlow 10\n")
            + stage(30, "setstage StageFlow 20\n"));
        SwitchFixture f(shifted, false, quest, 1);
        auto& world = f.mFixture.mWorld;
        auto* scripts = world.getOblivionScriptManager();
        ObScript::RuntimeContext context;
        const auto set = [&](int value) {
            const std::vector<ObScript::Value> args{ObScript::ReferenceValue{key(0xd10), {}}, std::int64_t(value)};
            scripts->call("SetStage", {}, args, context, {});
        };
        const auto query = [&] {
            const std::vector<ObScript::Value> args{ObScript::ReferenceValue{key(0xd10), {}}};
            return ObScript::asInteger(scripts->call("GetStage", {}, args, context, {}));
        };
        const auto sequence = [&] { ESM4::RuntimeState state; scripts->capture(state); return state.mScriptEventSequence; };
        const auto before = sequence();
        set(30);
        EXPECT_EQ(query(), 30);
        EXPECT_EQ(sequence() - before, 3u);
        auto saved = world.captureOblivionRuntimeState();
        const auto row = std::ranges::find(saved.mQuests, key(0xd10), &ESM4::RuntimeQuestState::mQuest);
        ASSERT_NE(row, saved.mQuests.end());
        EXPECT_EQ(row->mCompletedStages, (std::vector<std::int32_t>{10,20,30}));
        set(30);
        EXPECT_EQ(query(), 30);
        EXPECT_EQ(sequence() - before, repeat ? 6u : 3u);
        scripts->restore(ESM4::RuntimeState::deserializeBinary(saved.serializeBinary()));
        EXPECT_EQ(query(), 30);
        set(20);
        EXPECT_EQ(query(), 30);
        EXPECT_EQ(sequence() - before, repeat ? 5u : 3u);
    }
}

namespace
{
    std::string conversationRecords(unsigned nextSpeaker = 0, bool cycle = false)
    {
        const auto info = [&](unsigned id, unsigned speaker, unsigned listener,
                              unsigned next, unsigned flags, int stage,
                              const std::vector<unsigned>& links, int result) {
            auto data = sub(ESM::fourCC("DATA"), std::string{char(1), char(next), char(flags)})
                + sub(ESM::fourCC("QSTI"), bytes(0xd10u));
            ESM4::TargetResponseData text{};
            text.responseNo = 1;
            data += sub(ESM::fourCC("TRDT"), bytes(text).substr(0,16))
                + sub(ESM::fourCC("NAM1"), std::string("Conversation response\0",22));
            for (auto [target, base] : {std::pair{false, speaker}, std::pair{true, listener}})
            {
                ESM4::AIPackage::CTDA condition{};
                condition.condition = target ? 2 : 0;
                condition.fnIndex = 72;
                condition.param1 = base;
                condition.compValue = 1.f;
                data += sub(ESM::fourCC("CTDA"), bytes(condition));
            }
            if (stage >= 0)
            {
                ESM4::AIPackage::CTDA condition{};
                condition.fnIndex = 58;
                condition.param1 = 0xd10;
                condition.compValue = stage;
                data += sub(ESM::fourCC("CTDA"), bytes(condition));
            }
            for (unsigned link : links) data += sub(ESM::fourCC("TCLT"), bytes(link));
            data += sub(ESM::fourCC("SCTX"), "setstage ConversationFlow " + std::to_string(result) + "\n");
            return record(ESM4::REC_INFO, id, data);
        };
        const unsigned other = nextSpeaker == 1 ? 0x800 : 0xd11;
        const unsigned target = nextSpeaker == 1 ? 0xd11 : 0x800;
        const unsigned once = cycle ? 0 : ESM4::INFO_SayOnce;
        return record(ESM4::REC_QUST, 0xd10,
                   sub(ESM::fourCC("EDID"), std::string("ConversationFlow\0",17))
                   + sub(ESM::fourCC("DATA"), std::string{char(1),char(50)}))
            + topic(0xd20,"HELLO","Hello")
            + group(0xd20, info(0xd21,0x800,0xd11,nextSpeaker,once,cycle ? -1 : 0,{0xd30,0xd40},10))
            + topic(0xd30,"IneligibleConversation","Ineligible")
            + group(0xd30, info(0xd31,other,target,0,once,99,{},99))
            + topic(0xd40,"ConversationReply","Reply")
            + group(0xd40, info(0xd41,other,target,0,once,cycle ? -1 : 10,{0xd50},20))
            + topic(0xd50,"GOODBYE","Goodbye")
            + group(0xd50, info(0xd51,target,other,cycle ? 1 : 0,cycle ? 0 : ESM4::INFO_Goodbye | once,
                  cycle ? -1 : 20,{0xd20},30));
    }

    MWWorld::Ptr conversationPartner(SwitchFixture& f)
    {
        auto& world = f.mFixture.mWorld;
        auto npc = *f.mActor.get<ESM4::Npc>()->mBase;
        npc.mId.mIndex = 0xd11;
        npc.mFormKey = key(0xd11);
        world.getStore().getWritable<ESM4::Npc>().insertStatic(npc,npc.mFormKey);
        const auto* base = world.getStore().search<ESM4::Npc>(key(0xd11));
        ESM4::ActorCharacter placed{};
        placed.mId = {0xa02,base->mId.mContentFile};
        placed.mFormKey = key(0xa02);
        placed.mBaseObj = base->mId;
        placed.mBaseKey = key(0xd11);
        world.getStore().getWritable<ESM4::ActorCharacter>().insertStatic(placed,placed.mFormKey);
        MWWorld::LiveCellRef<ESM4::Npc> live(placed,base);
        auto& cell = *f.mActor.getCell();
        auto result = MWWorld::Ptr(cell.insert(&live),&cell);
        world.getWorldModel().registerPtr(result);
        if (!world.activateOblivionActor(result)) throw std::runtime_error("cannot activate conversation listener");
        return result;
    }
}

TEST(OblivionConversation, LinkedNpcResponsesUseSpeakerTargetConditionsResultsAndPersistedHistory)
{
    for (const bool shifted : {false,true}) for (unsigned nextSpeaker : {0u,1u,2u})
    {
        SCOPED_TRACE(shifted);
        SCOPED_TRACE(nextSpeaker);
        SwitchFixture f(shifted,false,conversationRecords(nextSpeaker),9);
        const auto listener = conversationPartner(f);
        auto* scripts = f.mFixture.mWorld.getOblivionScriptManager();
        ObScript::RuntimeContext context;
        context.mSelf = key(0xa00);
        const auto start = [&](const ESM::FormKey& target) {
            const std::vector<ObScript::Value> args{ObScript::ReferenceValue{target,{}}};
            return ObScript::asInteger(scripts->call("StartConversation",{},args,context,{}));
        };
        EXPECT_EQ(start(key(0xa00)),0); // Self is not a conversation partner.
        EXPECT_EQ(start(listener.getCellRef().getFormKey()),3);
        auto saved = f.mFixture.mWorld.captureOblivionRuntimeState();
        const auto quest = std::ranges::find(saved.mQuests,key(0xd10),&ESM4::RuntimeQuestState::mQuest);
        ASSERT_NE(quest,saved.mQuests.end());
        EXPECT_EQ(quest->mStage,30);
        for (unsigned id : {0xd21u,0xd41u,0xd51u}) EXPECT_TRUE(scripts->hasSpokenDialogueInfo(key(id)));
        EXPECT_FALSE(scripts->hasSpokenDialogueInfo(key(0xd31)));
        const auto sequence = saved.mScriptEventSequence;
        scripts->restore(ESM4::RuntimeState::deserializeBinary(saved.serializeBinary()));
        EXPECT_EQ(start(listener.getCellRef().getFormKey()),0);
        scripts->capture(saved);
        EXPECT_EQ(saved.mScriptEventSequence,sequence);
    }
}

TEST(OblivionConversation, LinkedCycleStopsWithoutReplayingAResponse)
{
    SwitchFixture f(false,false,conversationRecords(0,true),9);
    const auto listener = conversationPartner(f);
    auto* scripts = f.mFixture.mWorld.getOblivionScriptManager();
    ObScript::RuntimeContext context;
    context.mSelf = key(0xa00);
    const std::vector<ObScript::Value> args{ObScript::ReferenceValue{listener.getCellRef().getFormKey(),{}}};
    EXPECT_EQ(ObScript::asInteger(scripts->call("StartConversation",{},args,context,{})),3);
}

TEST(OblivionConversation, FindNpcActionWaitsForResidentLineOfSightAcrossRestore)
{
    for (bool shifted : {false,true})
    {
        const auto find = record(ESM4::REC_PACK,0xd60,
            sub(ESM::fourCC("EDID"),std::string("FindConversationPartner\0",24))
            + sub(ESM::fourCC("PKDT"),bytes(0u) + std::string{char(0),char(0xff),char(0),char(20)})
            + sub(ESM::fourCC("PSDT"),std::string{char(0xff),char(0xff),char(0),char(0xff)} + bytes(0u))
            + sub(ESM::fourCC("PTDT"),bytes(0u) + bytes(0xa02u) + bytes(200u)));
        SwitchFixture f(shifted,false,conversationRecords() + find,10);
        const auto listener = conversationPartner(f);
        auto& ai = *f.mFixture.mWorld.getOblivionAiService();
        ASSERT_TRUE(ai.addScriptPackage(f.mActor,key(0xd60)));
        auto saved = f.capture();
        auto actor = std::ranges::find(saved.mActorAi,key(0xa00),&ESM4::RuntimeActorAiState::mActor);
        ASSERT_NE(actor,saved.mActorAi.end());
        actor->mPhase = ESM4::PackagePhase::Act;
        actor->mTarget = listener.getCellRef().getFormKey();
        actor->mTier = ESM4::ProcessTier::Low;
        ai.restore(saved);
        ai.update(f.mActor,.05f,false);
        ai.commitPendingTransitions(); // Low process cannot start speech.
        saved = f.capture();
        actor = std::ranges::find(saved.mActorAi,key(0xa00),&ESM4::RuntimeActorAiState::mActor);
        ASSERT_NE(actor,saved.mActorAi.end());
        EXPECT_EQ(actor->mPhase,ESM4::PackagePhase::Act);
        actor->mTier = ESM4::ProcessTier::High;
        ai.restore(saved);
        ai.commitPendingTransitions(); // Headless actors have no resident LOS.
        auto after = f.capture();
        actor = std::ranges::find(after.mActorAi,key(0xa00),&ESM4::RuntimeActorAiState::mActor);
        ASSERT_NE(actor,after.mActorAi.end());
        EXPECT_EQ(actor->mPhase,ESM4::PackagePhase::Act);
        EXPECT_EQ(actor->mPackage,key(0xd60));
        EXPECT_FALSE(f.mFixture.mWorld.getOblivionScriptManager()->hasSpokenDialogueInfo(key(0xd21)));
        ai.restore(after);
        ai.commitPendingTransitions();
        after = f.capture();
        actor = std::ranges::find(after.mActorAi,key(0xa00),&ESM4::RuntimeActorAiState::mActor);
        ASSERT_NE(actor,after.mActorAi.end());
        EXPECT_EQ(actor->mPhase,ESM4::PackagePhase::Act);
        EXPECT_TRUE(after.mPendingPackageDone.empty());
    }
}

TEST(OblivionConversation, FindStopsWithinAuthoredRangeBeforeWalkingIntoMovingPartner)
{
    for (bool shifted : {false,true})
    {
        const auto find = record(ESM4::REC_PACK,0xd60,
            sub(ESM::fourCC("EDID"),std::string("FindConversationPartner\0",24))
            + sub(ESM::fourCC("PKDT"),bytes(0u) + std::string{char(0),char(0xff),char(0),char(20)})
            + sub(ESM::fourCC("PSDT"),std::string{char(0xff),char(0xff),char(0),char(0xff)} + bytes(0u))
            + sub(ESM::fourCC("PTDT"),bytes(0u) + bytes(0xa02u) + bytes(200u)));
        SwitchFixture f(shifted,false,conversationRecords() + find,10);
        auto& world = f.mFixture.mWorld;
        const auto listener = conversationPartner(f);
        auto position = listener.getRefData().getPosition();
        position.pos[1] = 150;
        listener.getRefData().setPosition(position);
        // A real public Physics/VFS admission supplies the contact bodies.
        // There is no renderer or mocked contact-acquisition result.
        osg::ref_ptr<osg::Geode> model = new osg::Geode;
        osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry;
        osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array;
        for (const float z : {0.f, 128.f})
            for (const auto xy : {osg::Vec2f(-16,-16), osg::Vec2f(16,-16), osg::Vec2f(16,16), osg::Vec2f(-16,16)})
                vertices->push_back(osg::Vec3f(xy.x(), xy.y(), z));
        geometry->setVertexArray(vertices);
        osg::ref_ptr<osg::DrawElementsUInt> triangles = new osg::DrawElementsUInt(GL_TRIANGLES);
        for (const unsigned index : {0u,2u,1u,0u,3u,2u,4u,5u,6u,4u,6u,7u,
                 0u,1u,5u,0u,5u,4u,1u,2u,6u,1u,6u,5u,2u,3u,7u,2u,7u,6u,3u,0u,4u,3u,4u,7u})
            triangles->push_back(index);
        geometry->addPrimitiveSet(triangles);
        model->addDrawable(geometry);
        ASSERT_TRUE(osgDB::writeNodeFile(*model, (f.mFixture.mDirectory / "hit-actor.osgt").string()));
        f.mFixture.mVfs.addArchive(std::make_unique<VFS::FileSystemArchive>(f.mFixture.mDirectory));
        f.mFixture.mVfs.buildIndex();
        auto* scene = f.mFixture.mResources.getSceneManager();
        scene->setShaderPath(std::filesystem::path(OPENMW_PROJECT_SOURCE_DIR) / "files/shaders");
        auto defines = Shader::getDefaultDefines();
        for (const auto& [name, value] : SceneUtil::ShadowManager::getShadowsDisabledDefines())
            defines[name] = value;
        osg::ref_ptr<SceneUtil::LightManager> lights
            = new SceneUtil::LightManager(SceneUtil::LightSettings{}, &f.mFixture.mResources);
        for (const auto& [name, value] : lights->getLightDefines()) defines[name] = value;
        scene->getShaderManager().setGlobalDefines(defines);
        auto& physics = world.initializePhysics(new osg::Group);
        for (const auto& ptr : {listener, f.mActor})
        {
            ptr.getRefData().setBaseNode(new SceneUtil::PositionAttitudeTransform);
            physics.addActor(ptr, VFS::Path::Normalized("hit-actor.osgt"));
        }

        ASSERT_TRUE(world.getLOS(f.mActor,listener));
        auto& ai = *world.getOblivionAiService();
        ASSERT_TRUE(ai.addScriptPackage(f.mActor,key(0xd60)));
        auto saved = f.capture();
        auto actor = std::ranges::find(saved.mActorAi,key(0xa00),&ESM4::RuntimeActorAiState::mActor);
        ASSERT_NE(actor,saved.mActorAi.end());
        actor->mPhase = ESM4::PackagePhase::Path;
        actor->mTarget = listener.getCellRef().getFormKey();
        actor->mTier = ESM4::ProcessTier::High;
        ai.restore(saved);
        const auto before = f.mActor.getRefData().getPosition();
        ai.update(f.mActor,.05f,true);
        ASSERT_EQ(ai.state(key(0xa00))->mPhase,ESM4::PackagePhase::Arrive);
        EXPECT_EQ(f.mActor.getRefData().getPosition(),before);
        EXPECT_TRUE(f.capture().mPendingPackageDone.empty());
        ai.update(f.mActor,.05f,true);
        ASSERT_EQ(ai.state(key(0xa00))->mPhase,ESM4::PackagePhase::Act);
        // Restore before commit: speech and package completion occur once.
        ai.restore(ESM4::RuntimeState::deserializeBinary(f.capture().serializeBinary()));
        ai.commitPendingTransitions();
        EXPECT_EQ(ai.state(key(0xa00))->mPhase,ESM4::PackagePhase::Complete);
        EXPECT_TRUE(world.getOblivionScriptManager()->hasSpokenDialogueInfo(key(0xd21)));
        const auto after = f.capture();
        ai.restore(ESM4::RuntimeState::deserializeBinary(after.serializeBinary()));
        ai.commitPendingTransitions();
        EXPECT_EQ(f.capture().mSpokenDialogueInfos,after.mSpokenDialogueInfos);
        EXPECT_EQ(f.capture().mQuests,after.mQuests);
    }
}

TEST(OblivionTutorialDoor, ForeignCellTeleportUsesFloorApproachAndFinishesOnlyAtDestinationAfterRestore)
{
    for (bool shifted : {false,true})
    {
        SwitchFixture f(shifted,false,sameCellTeleportRecords(true,true),8);
        auto& world = f.mFixture.mWorld;
        auto& ai = *world.getOblivionAiService();
        const auto actor = f.mActor.getCellRef().getFormKey();
        ASSERT_TRUE(ai.addScriptPackage(f.mActor,key(0xd05)));
        world.getWorldModel().deregisterLiveCellRef(*f.mActor.mRef);
        for (int i=0;i<80 && ai.state(actor)->mPhase != ESM4::PackagePhase::Door;++i) ai.updateUnloaded(.25f);
        ASSERT_EQ(ai.state(actor)->mPhase,ESM4::PackagePhase::Door);
        EXPECT_EQ(ai.state(actor)->mDoor,key(0xd01));
        EXPECT_EQ(ai.state(actor)->mCell,key(0xb04));
        EXPECT_FLOAT_EQ(ai.state(actor)->mLastValidPosition.pos[2],0);
        EXPECT_TRUE(f.capture().mPendingPackageDone.empty());
        const auto saved = f.capture();
        ai.restore(ESM4::RuntimeState::deserializeBinary(saved.serializeBinary()));
        for (int i=0;i<4 && ai.state(actor)->mLastTransitionDoor.isNull();++i) ai.updateUnloaded(.25f);
        ASSERT_EQ(ai.state(actor)->mLastTransitionDoor,key(0xd01));
        EXPECT_EQ(ai.state(actor)->mCell,key(0xd06));
        EXPECT_FLOAT_EQ(ai.state(actor)->mLastValidPosition.pos[0],1024);
        EXPECT_FLOAT_EQ(ai.state(actor)->mLastValidPosition.pos[2],0);
        EXPECT_TRUE(f.capture().mPendingPackageDone.empty());
        for (int i=0;i<80 && f.capture().mPendingPackageDone.empty();++i) ai.updateUnloaded(.25f);
        const auto complete = f.capture();
        ASSERT_EQ(complete.mPendingPackageDone.size(),1u);
        EXPECT_EQ(complete.mPendingPackageDone.front().mPackage,key(0xd05));
        EXPECT_NEAR(ai.state(actor)->mLastValidPosition.pos[0],1280,64);
    }
}

TEST(OblivionTutorialDoor, OwnedLockedTeleportRetainsLockAcrossAbstractTransitionAndActivationDenial)
{
    for (bool shifted : {false,true})
    {
        MWClass::ESM4Door::registerSelf();
        SwitchFixture f(shifted,false,sameCellTeleportRecords(true,true,true),8);
        auto& world = f.mFixture.mWorld;
        auto& ai = *world.getOblivionAiService();
        const auto actor = f.mActor.getCellRef().getFormKey();
        ASSERT_TRUE(ai.addScriptPackage(f.mActor,key(0xd05)));
        world.getWorldModel().deregisterLiveCellRef(*f.mActor.mRef);
        for (int i=0;i<80 && ai.state(actor)->mPhase != ESM4::PackagePhase::Door;++i) ai.updateUnloaded(.25f);
        ASSERT_EQ(ai.state(actor)->mPhase,ESM4::PackagePhase::Door);
        const auto saved = f.capture();
        world.getWorldModel().registerPtr(f.mActor);
        ai.restore(saved);
        auto* base = world.getStore().search<ESM4::Door>(key(0xd00));
        ASSERT_NE(base,nullptr);
        const auto door = f.mActor.getCell()->getPtr(ESM::RefId(base->mId));
        ASSERT_FALSE(door.isEmpty());
        ASSERT_TRUE(door.getCellRef().isLocked());
        // Ordinary activation has no key and leaves both the actor and lock.
        world.activateOblivionReferenceDefault(door,f.mActor);
        EXPECT_EQ(f.mActor.getCell(),world.getPlayerPtr().getCell());
        EXPECT_TRUE(door.getCellRef().isLocked());
        // The headless fixture proves authorization and ordinary-activation
        // denial. Rendered course covers the resident World/Scene cell move.
        world.getWorldModel().deregisterLiveCellRef(*f.mActor.mRef);
        for (int i=0;i<4 && ai.state(actor)->mLastTransitionDoor.isNull();++i) ai.updateUnloaded(.25f);
        EXPECT_EQ(ai.state(actor)->mLastTransitionDoor,key(0xd01));
        EXPECT_EQ(ai.state(actor)->mCell,key(0xd06));
        EXPECT_FLOAT_EQ(ai.state(actor)->mLastValidPosition.pos[0],1024);
        EXPECT_FLOAT_EQ(ai.state(actor)->mLastValidPosition.pos[2],0);
        EXPECT_TRUE(door.getCellRef().isLocked());
        EXPECT_TRUE(f.capture().mPendingPackageDone.empty());
    }
}

TEST(OblivionTutorialAction, PotionUseRestoresNativeDamageAndConsumesExactlyOne)
{
    for (bool shifted : {false, true})
    {
        TestingOpenMW::InventoryWindowManager windows;
        SwitchFixture f(shifted);
        f.mFixture.mEnvironment.setWindowManager(windows);
        auto& world = f.mFixture.mWorld;
        auto& store = world.getStore();
        auto& service = *world.getOblivionCombatService();
        const auto actor = f.mActor;
        const auto actorKey = actor.getCellRef().getFormKey();
        const auto file = store.search<ESM4::Npc>(key(0x800))->mId.mContentFile;
        ESM4::Potion potion{};
        potion.mId = {0xc00, file};
        potion.mEffects = {{ESM::fourCC("REHE"), 20, 0, 0, 0, 8, {}}};
        store.getWritable<ESM4::Potion>().insertStatic(potion, key(0xc00));
        ESM4::EffectSetting effect{};
        effect.mId = {0xc01, file};
        effect.mEffectCode = ESM::fourCC("REHE");
        effect.mData = ESM4::EffectSettingData{0x01000870, 10.f, 8, 5, -1};
        store.getWritable<ESM4::EffectSetting>().insertStatic(effect, key(0xc01));
        ESM::Potion shared{};
        shared.blank();
        shared.mId = ESM::RefId(potion.mId);
        store.insertStatic(shared);
        MWClass::Potion::registerSelf();
        auto& inventory = actor.getClass().getContainerStore(actor);
        const auto item = *inventory.add(shared.mId, 3, false);
        service.changeNonPlayerHealth(actor, -30.f, {}, false, {});
        const auto before = *service.findActorValues(actorKey);
        ASSERT_TRUE(actor.getClass().consume(item, actor));
        EXPECT_EQ(inventory.count(shared.mId), 2);
        EXPECT_FLOAT_EQ(service.getNonPlayerValue(actor, 8),
            ESM4::composeActorValue(before.mValues[8], before.mOwner, before.mProcess) + 20.f);
        EXPECT_EQ(service.findActorValues(actorKey)->mValues[8].mModifiers[0], before.mValues[8].mModifiers[0]);
        EXPECT_EQ(service.findActorValues(actorKey)->mValues[8].mModifiers[1], before.mValues[8].mModifiers[1]);
        ASSERT_TRUE(actor.getClass().consume(item, actor));
        EXPECT_EQ(inventory.count(shared.mId), 1);
        EXPECT_FLOAT_EQ(service.findActorValues(actorKey)->mValues[8].mModifiers[2].value_or(0.f), 0.f);
        const auto healed = *service.findActorValues(actorKey);
        // A supported first effect cannot leak through an unsupported tail.
        potion.mEffects.push_back({ESM::fourCC("RESP"), 50, 0, 10, 0, 9, {}});
        store.getWritable<ESM4::Potion>().insertStatic(potion, key(0xc00));
        EXPECT_FALSE(actor.getClass().consume(item, actor));
        EXPECT_EQ(inventory.count(shared.mId), 1);
        EXPECT_EQ(*service.findActorValues(actorKey), healed);
        ESM4::RuntimeState saved;
        saved.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
        saved.mPlayer.mCell = key(0xb04);
        saved.mPlayer.mRace = key(0x810);
        saved.mPlayer.mClass = key(0xb05);
        ESM4::RuntimeReferenceState savedActor;
        savedActor.mKey = actorKey;
        savedActor.mBase = healed.mBase;
        savedActor.mCell = saved.mPlayer.mCell;
        saved.mReferences.push_back(savedActor);
        service.capture(saved);
        MWMechanics::OblivionCombatService restored;
        restored.restore(ESM4::RuntimeState::deserializeBinary(saved.serializeBinary()));
        EXPECT_EQ(*restored.findActorValues(actorKey), healed);
    }
}

TEST(OblivionTutorialAction, ArmorFacadeUsesNativeClassAndConditionWithoutMorrowindSettings)
{
    for (bool shifted : {false, true})
    {
        SwitchFixture f(shifted);
        auto& store = f.mFixture.mWorld.getStore();
        ASSERT_EQ(store.get<ESM::GameSetting>().search("iCuirassWeight"), nullptr);
        ASSERT_EQ(store.get<ESM::GameSetting>().search("iBaseArmorSkill"), nullptr);
        ESM4::Armor native{};
        native.mId = {0xc10, store.search<ESM4::Npc>(key(0x800))->mId.mContentFile};
        native.mGeneralFlags = ESM4::Armor::TYPE_TES4 | ESM4::Armor::TES4_HeavyArmor;
        native.mArmorFlags = ESM4::Armor::TES4_UpperBody;
        native.mData = {1000, 10, 300, .1f};
        store.getWritable<ESM4::Armor>().insertStatic(native, key(0xc10));
        ESM::Armor shared{};
        shared.blank();
        shared.mId = ESM::RefId(native.mId);
        shared.mData.mType = ESM::Armor::Cuirass;
        shared.mData.mWeight = .1f; // Weight does not choose the native armor class.
        shared.mData.mHealth = 300;
        shared.mData.mArmor = 1000;
        store.insertStatic(shared);
        MWClass::Armor::registerSelf();
        MWWorld::ManualRef ref(store, shared.mId);
        auto item = ref.getPtr();
        const auto& cls = static_cast<const MWClass::Armor&>(item.getClass());
        EXPECT_EQ(cls.getEquipmentSkill(item, false), ESM::Skill::HeavyArmor);
        EXPECT_NO_THROW(cls.getUpSoundId(item));
        EXPECT_NO_THROW(cls.getDownSoundId(item));
        const float full = cls.getSkillAdjustedArmorRating(item, f.mActor, &store);
        EXPECT_GT(full, 0.f);
        EXPECT_LE(full, 10.f); // Authored1000 is hundredths, not 1000 armor units.
        item.getCellRef().setNativeItemCondition(150.f);
        EXPECT_NEAR(cls.getSkillAdjustedArmorRating(item, f.mActor, &store), full * .5f, .001f);
        item.getCellRef().setNativeItemCondition(0.f);
        EXPECT_FLOAT_EQ(cls.getSkillAdjustedArmorRating(item, f.mActor, &store), 0.f);
        MWWorld::ESMStore incoming;
        native.mGeneralFlags = ESM4::Armor::TYPE_TES4;
        native.mData.weight = 200.f;
        incoming.getWritable<ESM4::Armor>().insertStatic(native, key(0xc10));
        EXPECT_EQ(cls.getEquipmentSkill(item, &incoming), ESM::Skill::LightArmor);
        EXPECT_EQ(cls.getEquipmentSkill(item, &store), ESM::Skill::HeavyArmor);
    }
}

TEST(OblivionTutorialAction, PackageDoneActorValueNameDoesNotResolveDialogueTopic)
{
    for (bool shifted : {false, true})
    {
        const auto topic = record(ESM4::REC_DIAL, 0xc20,
            sub(ESM::fourCC("EDID"), std::string("Confidence\0", 11))
            + sub(ESM::fourCC("DATA"), std::string(1, '\0')));
        SwitchFixture f(shifted, false, topic, 1, false,
            "scn ActivationReceiver\nshort calls\nbegin OnPackageDone\n"
            "setav confidence 50\nset calls to GetAV confidence\nend\n");
        auto& world = f.mFixture.mWorld;
        ASSERT_EQ(world.getStore().findEsm4FormKey("Confidence"), key(0xc20));
        auto& scripts = *world.getOblivionScriptManager();
        ASSERT_TRUE(scripts.dispatchObjectEvent(f.mActor, "onpackagedone"));
        EXPECT_EQ(world.getOblivionCombatService()->getNonPlayerValue(f.mActor, 34), 50.f);
        EXPECT_EQ(scripts.scriptVariable(f.mActor.getCellRef().getFormKey(), 1), 50.f);
    }
}

TEST(OblivionBoundItems, BoundDaggerRequiresWeaponAssociation)
{
    BoundDefinitionFixture f(false, ESM4::EFI_BWDA);
    auto& store = f.mFixture.mWorld.getStore();
    const auto inputs = MWWorld::resolveOblivionNpcBoundAbilityInputs(store, key(0x800));
    ASSERT_EQ(inputs.size(), 1u);
    ASSERT_EQ(inputs.front().mEffects.size(), 3u);
    EXPECT_EQ(inputs.front().mEffects.back().mEffectCode, ESM4::EFI_BWDA);
    EXPECT_EQ(inputs.front().mEffects.back().mItem.mEquippedSlots, ESM4::InventorySlotWeapon);
    auto effect = *store.get<ESM4::EffectSetting>().searchStatic(key(0xe22));
    effect.mData->mFlags = 0x22112; // An armor association cannot supply a dagger.
    store.getWritable<ESM4::EffectSetting>().insertStatic(effect, key(0xe22));
    EXPECT_THROW(MWWorld::resolveOblivionNpcBoundAbilityInputs(store, key(0x800)), std::invalid_argument);
}

TEST(OblivionBoundItems, LaterWeaponAbilityWinsWithoutDroppingEarlierArmorOrSourceIndices)
{
    for (const bool daggerLast : {false, true})
    {
        SCOPED_TRACE(daggerLast);
        BoundDefinitionFixture f(false);
        auto& store = f.mFixture.mWorld.getStore();
        auto daggerEffect = *store.get<ESM4::EffectSetting>().searchStatic(key(0xe22));
        daggerEffect.mId = {0xe30, daggerEffect.mId.mContentFile};
        daggerEffect.mFormKey = key(0xe30);
        daggerEffect.mEffectCode = ESM4::EFI_BWDA;
        store.getWritable<ESM4::EffectSetting>().insertStatic(daggerEffect, key(0xe30));
        auto daggerSpell = *store.get<ESM4::Spell>().searchStatic(key(0xe00));
        daggerSpell.mId = {0xe01, daggerSpell.mId.mContentFile};
        daggerSpell.mFormKey = key(0xe01);
        daggerSpell.mEffects = {daggerSpell.mEffects.back()};
        daggerSpell.mEffects.front().mId = ESM4::EFI_BWDA;
        store.getWritable<ESM4::Spell>().insertStatic(daggerSpell, key(0xe01));
        auto npc = *store.search<ESM4::Npc>(key(0x800));
        const auto mace = store.get<ESM4::Spell>().searchStatic(key(0xe00))->mId;
        npc.mSpell = daggerLast ? std::vector<ESM::FormId>{mace, daggerSpell.mId}
                               : std::vector<ESM::FormId>{daggerSpell.mId, mace};
        store.getWritable<ESM4::Npc>().insertStatic(npc, key(0x800));
        const auto inputs = MWWorld::resolveOblivionNpcBoundAbilityInputs(store, key(0x800));
        ASSERT_EQ(inputs.size(), daggerLast ? 2u : 1u);
        ASSERT_GE(inputs.front().mEffects.size(), 2u);
        EXPECT_EQ(inputs.front().mEffects[0].mEffectCode, ESM4::EFI_MYTH);
        EXPECT_EQ(inputs.front().mEffects[1].mEffectCode, ESM4::EFI_MYHL);
        unsigned weapons = 0;
        for (const auto& ability : inputs)
            for (const auto& effect : ability.mEffects)
                if (effect.mItem.mEquippedSlots == ESM4::InventorySlotWeapon)
                {
                    ++weapons;
                    EXPECT_EQ(effect.mItem.mBoundSource, (ESM4::BoundItemSource{
                        key(daggerLast ? 0xe01 : 0xe00), daggerLast ? 0u : 2u}));
                }
        EXPECT_EQ(weapons, 1u);
    }
}

TEST(OblivionTutorialDoor, DefaultOpenPoseRetainsLockAndSavedCloseOverridesAuthoredOpen)
{
    AnimatedDoorFixture f(true);
    auto& world = f.mFixture.mWorld;
    EXPECT_EQ(f.query(), 1);
    EXPECT_TRUE(f.mDoor.getCellRef().isLocked());
    auto snapshot = world.captureOblivionRuntimeState();
    const auto findDoor = [](auto& state) -> auto& {
        return *std::find_if(state.mReferences.begin(), state.mReferences.end(),
            [](const auto& ref) { return ref.mKey == key(0xc01); });
    };
    const auto& captured = findDoor(snapshot);
    EXPECT_EQ(std::get<std::string>(captured.mCustomState.at("obscript.animation_group")), "open");
    EXPECT_EQ(std::get<double>(captured.mCustomState.at("obscript.animation_progress")), 1.0);
    EXPECT_EQ(captured.mLockLevel, 100);
    EXPECT_NO_THROW(world.prepareOblivionSaveState(
        ESM4::RuntimeState::deserializeBinary(snapshot.serializeBinary())));
    // The same animation owner used by scripts replaces the initial pose.
    auto& pose = f.mDoor.getRefData().getAnimationState().mScriptedAnims.front();
    pose.mGroup = "close";
    pose.mTime = 1.f;
    world.getOblivionScriptManager()->persistAnimationState(key(0xc01), "close", 1, false, true);
    EXPECT_EQ(f.query(), 3);
    snapshot = world.captureOblivionRuntimeState();
    EXPECT_EQ(std::get<std::string>(findDoor(snapshot).mCustomState.at("obscript.animation_group")), "close");
    EXPECT_TRUE(f.mDoor.getCellRef().isLocked());
}

TEST(OblivionTutorialGreeting, NativeFightScoreDistinguishesFriendlyPositiveAggressionWithoutStartingCombat)
{
    for (bool shifted : {false, true})
    {
        SwitchFixture f(shifted);
        auto& world = f.mFixture.mWorld;
        auto& combat = *world.getOblivionCombatService();
        // The headless fixture's Player form is800; supply the canonical
        // constructor input explicitly, as the native actor-stats fixtures do.
        ESM4::Npc playerForm{};
        playerForm.mId = {7, 0};
        playerForm.mFormKey = ESM::FormKey::content("Oblivion.esm", 7);
        playerForm.mIsTES4 = true;
        playerForm.mData.attribs = {40,40,40,40,40,40,40,40};
        playerForm.mData.health = 100;
        playerForm.mBaseConfig.tes4.baseSpell = 80;
        playerForm.mBaseConfig.tes4.fatigue = 160;
        world.getStore().getWritable<ESM4::Npc>().insertStatic(playerForm, playerForm.mFormKey);
        combat.initializePlayerActor(world.getPlayer(), world.getStore());
        auto values = *combat.findActorValues(ESM::FormKey::dynamic("player", 1));
        values.mBounty.emplace();
        values.mReputation.emplace();
        combat.publishPlayerValues(world.getPlayer(), values,
            MWWorld::resolveOblivionPlayerDynamicBaseSettings(world.getStore()));
        const std::array<MWWorld::Ptr, 1> residents{f.mActor};
        const auto random = combat.combatRandomState();
        for (int aggression : {0, 5, 100})
        {
            combat.setNonPlayerBaseValue(f.mActor, 33, aggression, residents);
            const auto score = MWMechanics::oblivionPlayerFightScore(world, f.mActor);
            ASSERT_TRUE(score);
            if (aggression == 100) EXPECT_GT(*score, 0);
            else EXPECT_LE(*score, 0);
            EXPECT_FALSE(world.hasOblivionCombatController(f.mActor.getCellRef().getFormKey()).value_or(false));
            EXPECT_EQ(combat.combatRandomState(), random);
        }
        EXPECT_FALSE(MWMechanics::oblivionPlayerFightScore(world, {}));
    }
}

TEST(OblivionTutorialAction, PackageWaitPersistsAndStopWaitingResumesWithoutReplacingSelection)
{
    for (const bool shifted : {false, true})
        for (const bool high : {false, true})
        {
            SCOPED_TRACE(shifted);
            SCOPED_TRACE(high);
            const auto makePack = [](unsigned id, unsigned flags, char type) {
                return record(ESM4::REC_PACK, id,
                    sub(ESM::fourCC("PKDT"), bytes(flags) + std::string{type, char(0xff), char(0), char(20)})
                    + sub(ESM::fourCC("PSDT"), std::string{char(0xff), char(0xff), char(0), char(0xff)} + bytes(0u))
                    + sub(ESM::fourCC("PTDT"), bytes(0u) + bytes(0x14u) + bytes(350u)));
            };
            SwitchFixture f(shifted, false, makePack(0xd60, 0x10000, 1) + makePack(0xd61, 0, 1)
                + makePack(0xd62, 0, 5), 3, false);
            auto& world = f.mFixture.mWorld;
            MWMechanics::MechanicsManager mechanics;
            f.mFixture.mEnvironment.setMechanicsManager(mechanics);
            world.initializePhysics(new osg::Group);
            auto pack = *world.getStore().search<ESM4::AIPackage>(key(0xd60));
            pack.mTargetData.mReferenceKey = ESM::FormKey::dynamic("player", 1);
            world.getStore().getWritable<ESM4::AIPackage>().insertStatic(pack, key(0xd60));
            auto& ai = *world.getOblivionAiService();
            auto& scripts = *world.getOblivionScriptManager();
            ObScript::RuntimeContext context; context.mSelf = f.mActor.getCellRef().getFormKey();
            const auto command = [&](const char* name, unsigned id) {
                return ObScript::asInteger(scripts.call(name, {}, {ObScript::ReferenceValue{key(id), {}}}, context, {}));
            };
            ASSERT_TRUE(ai.addScriptPackage(f.mActor, key(0xd60)));
            ASSERT_TRUE(ai.isPackageWaiting(key(0xd60))); // Authored true.
            const auto generation = ai.state(f.mActor)->mSelectionGeneration;
            EXPECT_EQ(command("StopWaiting", 0xd61), 0); // Original second command is a no-op.
            EXPECT_TRUE(ai.isPackageWaiting(key(0xd60)));
            EXPECT_EQ(command("Wait", 0xd62), 0); // Wrong package type.
            auto playerPosition = world.getPlayerPtr().getRefData().getPosition();
            playerPosition.pos[0] = 1000;
            world.getPlayerPtr().getRefData().setPosition(playerPosition);
            const auto initial = f.mActor.getRefData().getPosition().asVec3();
            for (int n = 0; n < 8; ++n) ai.update(f.mActor, .25f, high);
            EXPECT_EQ(ai.state(f.mActor)->mPhase, ESM4::PackagePhase::Wait);
            EXPECT_EQ(f.mActor.getRefData().getPosition().asVec3(), initial);
            EXPECT_EQ(command("Wait", 0xd60), 1);
            auto saved = f.capture();
            ASSERT_TRUE(saved.mPackageWaiting.at(key(0xd60)));
            ai.clear(); ai.restore(saved);
            EXPECT_TRUE(ai.isPackageWaiting(key(0xd60)));
            EXPECT_EQ(command("StopWaiting", 0xd60), 1);
            EXPECT_FALSE(ai.isPackageWaiting(key(0xd60)));
            EXPECT_EQ(command("StopWaiting", 0xd61), 0);
            saved = f.capture();
            EXPECT_FALSE(saved.mPackageWaiting.at(key(0xd60)));
            ai.restore(saved);
            EXPECT_FALSE(ai.isPackageWaiting(key(0xd60))); // False shadows authored true after restore.
            for (int n = 0; n < 8; ++n) ai.update(f.mActor, .25f, high);
            EXPECT_EQ(ai.state(f.mActor)->mDestinationPosition.asVec3(), playerPosition.asVec3());
            EXPECT_EQ(ai.state(f.mActor)->mPackage, key(0xd60));
            EXPECT_EQ(ai.state(f.mActor)->mSelectionGeneration, generation);
            saved = f.capture(); saved.mPackageWaiting[key(0xd62)] = true;
            EXPECT_THROW(ai.prepareRestore(saved), std::invalid_argument);
            EXPECT_FALSE(ai.isPackageWaiting(key(0xd60)));
            auto retained = ai.retainRuntime();
            ASSERT_TRUE(retained.detach());
            EXPECT_TRUE(ai.isPackageWaiting(key(0xd60))); // Detached graph sees content default.
            ASSERT_TRUE(retained.restore());
            EXPECT_FALSE(ai.isPackageWaiting(key(0xd60)));
        }
}

TEST(OblivionTutorialAction, EnableQueryDiscardsBorrowedIndexAfterBackingStorageIsReleased)
{
    for (const bool shifted : {false, true})
    {
        SCOPED_TRACE(shifted);
        SwitchFixture f(shifted);
        auto& world = f.mFixture.mWorld;
        std::vector<ESM4::RuntimeReferenceState> references(1);
        references.front().mKey = key(0xa00);
        references.front().mEnabled = false;
        const MWMechanics::OblivionSavedReferenceIndex borrowed(references);
        // Model a package action reallocating the World's saved reference
        // vector before another actor queries enable-parent state. ASan must
        // see no access to the old index's freed FormKeys.
        references.clear();
        references.shrink_to_fit();
        EXPECT_EQ(world.getOblivionReferenceEnabled(key(0xa00), &borrowed), true);
        f.mActor.getRefData().disable();
        EXPECT_EQ(world.getOblivionReferenceEnabled(key(0xa00), &borrowed), false);
    }
}

TEST(OblivionTutorialAction, FollowCellContextReevaluatesAndRestoredStaleRouteKeepsMandatoryPackage)
{
    MWClass::ESM4Door::registerSelf();
    for (const bool shifted : {false, true})
        for (const bool mandatory : {false, true})
            for (const auto restoredPhase : {std::optional<ESM4::PackagePhase>{},
                     std::optional{ESM4::PackagePhase::Path}, std::optional{ESM4::PackagePhase::Stalled}})
                for (const bool high : {false, true})
                {
                    SCOPED_TRACE(shifted);
                    SCOPED_TRACE(mandatory);
                    SCOPED_TRACE(restoredPhase ? static_cast<unsigned>(*restoredPhase) : 999u);
                    SCOPED_TRACE(high);
                    const auto follow = [](unsigned id, unsigned flags, unsigned cell) {
                        ESM4::AIPackage::CTDA condition{};
                        condition.fnIndex = 67; // GetInCell
                        condition.param1 = cell;
                        condition.compValue = 1.f;
                        return record(ESM4::REC_PACK, id,
                            sub(ESM::fourCC("PKDT"), bytes(flags)
                                + std::string{char(1), char(0xff), char(0), char(20)})
                            + sub(ESM::fourCC("PSDT"),
                                std::string{char(0xff), char(0xff), char(0), char(0xff)} + bytes(0u))
                            + sub(ESM::fourCC("PTDT"), bytes(0u) + bytes(0x14u) + bytes(350u))
                            + sub(ESM::fourCC("CTDA"), bytes(condition)));
                    };
                    SwitchFixture f(shifted, false, sameCellTeleportRecords(true, true)
                        + follow(0xd60, mandatory ? 4 : 0, 0xb04) + follow(0xd61, 0, 0xd06), 10, false);
                    auto& world = f.mFixture.mWorld;
                    MWMechanics::MechanicsManager mechanics;
                    f.mFixture.mEnvironment.setMechanicsManager(mechanics);
                    world.initializePhysics(new osg::Group);
                    auto& store = world.getStore();
                    auto npc = *store.search<ESM4::Npc>(key(0x800));
                    npc.mAIPackageKeys = {key(0xd60), key(0xd61)};
                    store.getWritable<ESM4::Npc>().insertStatic(npc, key(0x800));
                    for (unsigned id : {0xd60u, 0xd61u})
                    {
                        auto pack = *store.search<ESM4::AIPackage>(key(id));
                        pack.mTargetData.mReferenceKey = ESM::FormKey::dynamic("player", 1);
                        store.getWritable<ESM4::AIPackage>().insertStatic(pack, key(id));
                    }
                    auto& ai = *world.getOblivionAiService();
                    ASSERT_TRUE(ai.evaluatePackage(f.mActor));
                    for (int n = 0; n < 8; ++n) ai.update(f.mActor, .25f, high);
                    ASSERT_EQ(ai.state(f.mActor)->mPackage, key(0xd60));
                    const auto generation = ai.state(f.mActor)->mSelectionGeneration;
                    const auto file = f.mActor.get<ESM4::Npc>()->mBase->mId.mContentFile;
                    auto& destination = world.getWorldModel().getCell(ESM::RefId(ESM::FormId{0xd06, file}));
                    destination.load();
                    // Use real CellStore/WorldModel transfer without a renderer.
                    f.mActor = f.mActor.getCell()->moveTo(f.mActor, &destination);
                    auto position = f.mActor.getRefData().getPosition();
                    position.pos[0] = 1024;
                    f.mActor.getRefData().setPosition(position);
                    world.getPlayer().setCell(&destination);
                    world.getPlayerPtr().getRefData().setPosition(position);
                    if (restoredPhase)
                    {
                        auto saved = f.capture();
                        auto& row = *std::find_if(saved.mActorAi.begin(), saved.mActorAi.end(),
                            [](const auto& value) { return value.mActor == key(0xa00); });
                        row.mCell = row.mLastValidCell = key(0xd06);
                        row.mLastValidPosition = position;
                        row.mPathgrid = key(0xd04); // Previous cell's consumed corridor.
                        row.mPathNode = 1;
                        row.mPhase = *restoredPhase;
                        row.mRepathAttempts = *restoredPhase == ESM4::PackagePhase::Stalled ? 8 : 0;
                        row.mInterruptionReason = *restoredPhase == ESM4::PackagePhase::Stalled
                            ? "bounded-repath-exhausted" : "";
                        ai.restore(ESM4::RuntimeState::deserializeBinary(saved.serializeBinary()));
                    }
                    for (int n = 0; n < 8; ++n) ai.update(f.mActor, .25f, high);
                    const auto* after = ai.state(f.mActor);
                    ASSERT_NE(after, nullptr);
                    EXPECT_EQ(after->mCell, key(0xd06));
                    EXPECT_EQ(after->mPackage, mandatory ? key(0xd60) : key(0xd61));
                    EXPECT_TRUE(after->mScriptPackage.isNull());
                    EXPECT_EQ(after->mTarget, ESM::FormKey::dynamic("player", 1));
                    EXPECT_NE(after->mPhase, ESM4::PackagePhase::Stalled);
                    EXPECT_TRUE(after->mInterruptionReason.empty());
                    if (mandatory) EXPECT_EQ(after->mSelectionGeneration, generation);
                    else EXPECT_GT(after->mSelectionGeneration, generation);
                }
}

TEST(OblivionTutorialDoor, UnavailableForeignRouteNeverDrivesTowardInteriorCoordinates)
{
    MWClass::ESM4Door::registerSelf();
    for (const bool shifted : {false, true})
    {
        SCOPED_TRACE(shifted);
        auto records = sameCellTeleportRecords(true, true, true);
        const auto oldOwner = sub(ESM::fourCC("XOWN"), bytes(0x800u));
        const auto ownerAt = records.find(oldOwner);
        ASSERT_NE(ownerAt, std::string::npos);
        records.replace(ownerAt, oldOwner.size(), sub(ESM::fourCC("XOWN"), bytes(0xd70u)));
        records += record(ESM4::REC_NPC_, 0xd70,
            sub(ESM::fourCC("EDID"), std::string("OtherDoorOwner\0",15)));
        const std::array<float,6> oldGoal{1280,0,0,0,0,0};
        const std::array<float,6> newGoal{0,1280,0,0,0,0};
        const auto oldGoalSub = sub(ESM::fourCC("DATA"), bytes(oldGoal));
        const auto goalAt = records.find(oldGoalSub);
        ASSERT_NE(goalAt, std::string::npos);
        records.replace(goalAt, oldGoalSub.size(), sub(ESM::fourCC("DATA"), bytes(newGoal)));
        SwitchFixture f(shifted, false, records, 9, false);
        auto& world = f.mFixture.mWorld;
        MWMechanics::MechanicsManager mechanics;
        f.mFixture.mEnvironment.setMechanicsManager(mechanics);
        world.initializePhysics(new osg::Group);
        auto& ai = *world.getOblivionAiService();
        ASSERT_TRUE(ai.addScriptPackage(f.mActor, key(0xd05)));
        for (int n = 0; n < 8; ++n) ai.update(f.mActor, .25f, true);
        const auto* state = ai.state(f.mActor);
        ASSERT_NE(state, nullptr);
        EXPECT_EQ(state->mCell, key(0xb04));
        EXPECT_EQ(state->mDestinationCell, key(0xd06));
        EXPECT_NE(state->mPhase, ESM4::PackagePhase::Complete);
        const auto& movement = f.mActor.getClass().getMovementSettings(f.mActor);
        EXPECT_FLOAT_EQ(movement.mPosition[0], 0.f);
        EXPECT_FLOAT_EQ(movement.mPosition[1], 0.f);
        EXPECT_FLOAT_EQ(movement.mPosition[2], 0.f);
        EXPECT_EQ(f.mActor.getRefData().getPosition().asVec3(), osg::Vec3f());
    }
}

TEST(OblivionTutorialDoor, DoorChainIncludesDirectedExteriorStreetBetweenTeleports)
{
    MWClass::ESM4Door::registerSelf();
    for (const bool shifted : {false, true})
        for (const bool connected : {false, true})
        {
            SCOPED_TRACE(shifted);
            SCOPED_TRACE(connected);
            const auto group = [](unsigned kind, unsigned label, const std::string& data) {
                return bytes(ESM::fourCC("GRUP")) + bytes(static_cast<std::uint32_t>(20 + data.size()))
                    + bytes(label) + bytes(kind) + bytes(0u) + data;
            };
            const auto placed = [](unsigned id, unsigned base, float x, unsigned target = 0, float landing = 0) {
                auto data = sub(ESM::fourCC("NAME"), bytes(base))
                    + sub(ESM::fourCC("DATA"), bytes(std::array<float,6>{x,0,0,0,0,0}));
                if (target) data += sub(ESM::fourCC("XTEL"), bytes(target)
                    + bytes(std::array<float,6>{landing,0,0,0,0,0}));
                return record(ESM4::REC_REFR, id, data);
            };
            auto extra = sameCellTeleportRecords(true, true);
            extra += group(0, ESM4::REC_CELL, record(ESM4::REC_CELL, 0xd08,
                sub(ESM::fourCC("DATA"), bytes(std::uint16_t{ESM4::CELL_Interior}))));
            extra += group(6, 0xd08, group(9, 0xd08, placed(0xd10,0xd00,4096,0xd11,8192)));
            extra += group(0, ESM4::REC_CELL, record(ESM4::REC_CELL, 0xd16,
                sub(ESM::fourCC("DATA"), bytes(std::uint16_t{ESM4::CELL_Interior}))));
            extra += group(6, 0xd16, group(9, 0xd16,
                placed(0xd11,0xd00,8192,0xd10,4096) + placed(0xd13,0xb00,8256)));
            SwitchFixture f(shifted, false, extra, 13, false);
            auto& world = f.mFixture.mWorld;
            auto& store = world.getStore();
            for (unsigned id : {0xd06u, 0xd08u})
            {
                auto cell = *store.search<ESM4::Cell>(key(id));
                cell.mCellFlags = 0;
                cell.mParent = ESM::RefId(ESM::FormId{0xd80, f.mActor.get<ESM4::Npc>()->mBase->mId.mContentFile});
                store.getWritable<ESM4::Cell>().insertStatic(cell, key(id));
            }
            auto package = *store.search<ESM4::AIPackage>(key(0xd05));
            package.mLocationData.mReferenceKey = key(0xd13);
            store.getWritable<ESM4::AIPackage>().insertStatic(package, key(0xd05));
            auto& paths = store.getOblivionPathgridService();
            auto first = *store.search<ESM4::Pathgrid>(key(0xd07));
            if (connected) first.mForeign.push_back({1,0,4096,0,0});
            // A reverse-only link must not make the outward journey valid.
            ESM4::Pathgrid second{};
            second.mFormKey = key(0xd09); second.mData = 1;
            second.mNodes = {{4096,0,0,0,0,0}};
            second.mForeign = {{0,0,1280,0,0}};
            ESM4::Pathgrid shop{};
            shop.mFormKey = key(0xd17); shop.mData = 2;
            shop.mNodes = {{8192,0,0,1,0,0},{8256,0,0,1,0,0}};
            shop.mLinks = {{0,1},{1,0}};
            paths.registerPathgrid(first,key(0xd06));
            paths.registerPathgrid(second,key(0xd08));
            paths.registerPathgrid(shop,key(0xd16));
            paths.resolveForeignLinks(1.f);
            auto& ai = *world.getOblivionAiService();
            ASSERT_TRUE(ai.addScriptPackage(f.mActor,key(0xd05)));
            world.getWorldModel().deregisterLiveCellRef(*f.mActor.mRef);
            for (int i=0; i<8 && ai.state(key(0xa00))->mDoor.isNull(); ++i) ai.updateUnloaded(.25f);
            const auto* state = ai.state(key(0xa00));
            ASSERT_NE(state,nullptr);
            EXPECT_EQ(state->mDoor, connected ? key(0xd01) : ESM::FormKey{});
            EXPECT_EQ(state->mCell,key(0xb04));
            EXPECT_TRUE(f.capture().mPendingPackageDone.empty());
            if (connected)
                for (bool enabled : {false, true})
                {
                    SCOPED_TRACE(enabled);
                    // Reuse the same service after caching the street index.
                    // Node overlays must remove and restore its directed edge.
                    ASSERT_TRUE(ai.setPathPoint(key(0xd07), 1, enabled));
                    world.getWorldModel().registerPtr(f.mActor);
                    ASSERT_TRUE(ai.addScriptPackage(f.mActor,key(0xd05)));
                    world.getWorldModel().deregisterLiveCellRef(*f.mActor.mRef);
                    for (int i=0; i<8 && ai.state(key(0xa00))->mDoor.isNull(); ++i) ai.updateUnloaded(.25f);
                    EXPECT_EQ(ai.state(key(0xa00))->mDoor, enabled ? key(0xd01) : ESM::FormKey{});
                    EXPECT_TRUE(f.capture().mPendingPackageDone.empty());
                }
        }
}

TEST(OblivionTutorialDoor, AbstractHandoffRetriesPhysicalStallWithoutReplacingIntentOrBypassingLockedDoor)
{
    MWClass::ESM4Door::registerSelf();
    for (bool shifted : {false, true})
        for (bool locked : {false, true})
            for (bool physical : {false, true})
            {
                SCOPED_TRACE(shifted);
                SCOPED_TRACE(locked);
                SCOPED_TRACE(physical);
                auto records = sameCellTeleportRecords(true, true, locked);
                if (locked)
                {
                    const auto owner = sub(ESM::fourCC("XOWN"), bytes(0x800u));
                    const auto at = records.find(owner);
                    ASSERT_NE(at, std::string::npos);
                    records.replace(at, owner.size(), sub(ESM::fourCC("XOWN"), bytes(0xd70u)));
                    records += record(ESM4::REC_NPC_, 0xd70,
                        sub(ESM::fourCC("EDID"), std::string("OtherDoorOwner\0",15)));
                }
                SwitchFixture f(shifted, false, records, locked ? 9 : 8, false);
                auto& world = f.mFixture.mWorld;
                auto& ai = *world.getOblivionAiService();
                ASSERT_TRUE(ai.addScriptPackage(f.mActor, key(0xd05)));
                auto saved = f.capture();
                auto& row = *std::find_if(saved.mActorAi.begin(), saved.mActorAi.end(),
                    [](const auto& value) { return value.mActor == key(0xa00); });
                const auto generation = row.mSelectionGeneration;
                row.mPhase = ESM4::PackagePhase::Stalled;
                row.mPathgrid = key(0xd04);
                row.mPathNode = 0;
                row.mRouteGeneration = world.getStore().getOblivionPathgridService().generation(key(0xd04));
                row.mNoProgressSeconds = 30.f;
                row.mInterruptionReason = physical ? "bounded-repath-exhausted" : "route-unavailable";
                ai.restore(ESM4::RuntimeState::deserializeBinary(saved.serializeBinary()));
                world.getWorldModel().deregisterLiveCellRef(*f.mActor.mRef);
                for (int n = 0; n < 4; ++n) ai.updateUnloaded(.25f);
                const auto* after = ai.state(key(0xa00));
                ASSERT_NE(after, nullptr);
                EXPECT_EQ(after->mSelectionGeneration, generation);
                EXPECT_EQ(after->mPackage, key(0xd05));
                EXPECT_EQ(after->mScriptPackage, key(0xd05));
                EXPECT_EQ(after->mCell, key(0xb04));
                EXPECT_TRUE(f.capture().mPendingPackageDone.empty());
                if (!physical || locked)
                {
                    EXPECT_EQ(after->mPhase, ESM4::PackagePhase::Stalled);
                    EXPECT_EQ(after->mLastValidPosition.asVec3(), osg::Vec3f());
                    EXPECT_EQ(after->mInterruptionReason, "route-unavailable");
                }
                else
                {
                    EXPECT_NE(after->mPhase, ESM4::PackagePhase::Stalled);
                    EXPECT_EQ(after->mDoor, key(0xd01));
                    for (int n = 0; n < 160 && f.capture().mPendingPackageDone.empty(); ++n)
                        ai.updateUnloaded(.25f);
                    EXPECT_EQ(ai.state(key(0xa00))->mCell, key(0xd06));
                    EXPECT_EQ(ai.state(key(0xa00))->mSelectionGeneration, generation);
                    EXPECT_EQ(f.capture().mPendingPackageDone.size(), 1u);
                }
            }
}

TEST(OblivionTutorialDoor, OriginalDoorLandingIsValidatedBeforeDeferredResidentPromotion)
{
    MWClass::ESM4Door::registerSelf();
    for (bool shifted : {false, true})
        for (int variant = 0; variant < 4; ++variant)
        {
            SCOPED_TRACE(shifted);
            SCOPED_TRACE(variant);
            SwitchFixture f(shifted, false, sameCellTeleportRecords(true, true), 8, false);
            auto& world = f.mFixture.mWorld;
            MWMechanics::MechanicsManager mechanics;
            f.mFixture.mEnvironment.setMechanicsManager(mechanics);
            world.initializePhysics(new osg::Group);
            auto& ai = *world.getOblivionAiService();
            ASSERT_TRUE(ai.addScriptPackage(f.mActor, key(0xd05)));
            world.getWorldModel().deregisterLiveCellRef(*f.mActor.mRef);
            for (int n = 0; n < 80 && ai.state(key(0xa00))->mLastTransitionDoor.isNull(); ++n)
                ai.updateUnloaded(.25f);
            ASSERT_EQ(ai.state(key(0xa00))->mLastTransitionDoor, key(0xd01));
            ASSERT_EQ(ai.state(key(0xa00))->mCell, key(0xd06));
            auto saved = f.capture();
            auto& row = *std::find_if(saved.mActorAi.begin(), saved.mActorAi.end(),
                [](const auto& value) { return value.mActor == key(0xa00); });
            const auto generation = row.mSelectionGeneration;
            const auto landing = row.mLastValidPosition.asVec3();
            if (variant == 1) row.mDoorCooldown = 0.f;
            if (variant == 2) row.mLastTransitionDoor = key(0xd02); // Reverse edge lands in the other cell.
            if (variant == 3) row.mLastValidPosition.pos[0] += 5000.f;
            world.getWorldModel().registerPtr(f.mActor); // Resident Ptr still belongs to the old room.
            ai.restore(ESM4::RuntimeState::deserializeBinary(saved.serializeBinary()));
            ai.update(f.mActor, .05f, true);
            const auto* after = ai.state(key(0xa00));
            ASSERT_NE(after, nullptr);
            EXPECT_EQ(after->mPackage, key(0xd05));
            EXPECT_EQ(after->mSelectionGeneration, generation);
            EXPECT_EQ(after->mCell, variant == 0 ? key(0xd06) : key(0xb04));
            EXPECT_EQ(after->mLastValidPosition.asVec3(), variant == 0 ? landing : osg::Vec3f());
            // Promotion is staged until the world can safely transfer CellStores.
            EXPECT_EQ(f.mActor.getRefData().getPosition().asVec3(), osg::Vec3f());
        }
}

TEST(OblivionTutorialAction, UnavailableCombatTargetResetCanBeFollowedByOrdinaryCombatCleanup)
{
    for (bool shifted : {false, true})
    {
        SCOPED_TRACE(shifted);
        SwitchFixture f(shifted);
        auto& ai = *f.mFixture.mWorld.getOblivionAiService();
        const auto before = *ai.state(f.mActor);
        // The optimized conditional reset used to leave the first route's
        // empty FormKey string with a null data pointer. The next ordinary
        // melee cleanup then crashed even though the actor was not fighting.
        for (int n = 0; n < 32; ++n)
        {
            ai.approachCombatTarget(f.mActor, {}, 1.f / 30.f);
            ai.clearCombatRoute(f.mActor);
        }
        EXPECT_EQ(*ai.state(f.mActor), before);
        EXPECT_NO_THROW(ESM4::RuntimeState::deserializeBinary(f.capture().serializeBinary()));
    }
}
