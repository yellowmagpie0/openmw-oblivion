#include <components/esm3/esmwriter.hpp>
#include <components/esm3/esmreader.hpp>
#include <sstream>
#include "apps/openmw/mwmechanics/oblivionai.hpp"
#include <components/esm4/loadpack.hpp>
#include <components/esm4/aiphase.hpp>
#include <gtest/gtest.h>

#include "oblivionworldfixture.hpp"
#include "apps/openmw/mwworld/obliviontrespass.hpp"
#include <components/esm4/loadachr.hpp>
#include "apps/openmw/mwclass/npc.hpp"
#include "apps/openmw/mwclass/esm4npc.hpp"
#include "apps/openmw/mwclass/esm4interactive.hpp"
#include "apps/openmw/mwworld/cellstore.hpp"
#include "apps/openmw/mwworld/oblivionscriptmanager.hpp"
#include "apps/openmw/mwworld/esmstore.hpp"
#include "apps/openmw/mwworld/player.hpp"
#include "apps/openmw/mwworld/worldmodel.hpp"
#include "apps/openmw/mwworld/globalvariablename.hpp"
#include <components/esm4/common.hpp>
#include <components/esm4/loadcell.hpp>
#include <components/esm4/loadclas.hpp>
#include <components/esm4/loadrace.hpp>
#include <components/esm4/runtimestate.hpp>
#include <components/esm3/loadclas.hpp>
#include <components/esm3/loadrace.hpp>

#include <algorithm>
#include <array>
#include <tuple>

namespace
{
    template <class T> std::string bytes(const T& value)
    { return {reinterpret_cast<const char*>(&value), sizeof(value)}; }

    std::string sub(std::uint32_t tag, const std::string& data)
    { return bytes(tag) + bytes(static_cast<std::uint16_t>(data.size())) + data; }

    std::string record(std::uint32_t tag, std::uint32_t id, const std::string& data)
    { return bytes(tag) + bytes(static_cast<std::uint32_t>(data.size())) + bytes(0u) + bytes(id) + bytes(0u) + data; }

    std::string group(unsigned kind, unsigned label, const std::string& data)
    { return bytes(ESM::fourCC("GRUP")) + bytes(static_cast<std::uint32_t>(20 + data.size()))
        + bytes(label) + bytes(kind) + bytes(0u) + data; }

    ESM::FormKey key(unsigned value) { return ESM::FormKey::content("headless.esm", value); }
    ObScript::Value ref(unsigned value) { return ObScript::ReferenceValue{key(value), {}}; }

    std::string ownershipRecords()
    {
        const std::array<float, 6> position{};
        return record(ESM4::REC_FACT, 0xb03, sub(ESM::fourCC("EDID"), std::string("OwnerFaction\0", 13))
                + sub(ESM::fourCC("DATA"), std::string(1, '\0')))
            + record(ESM4::REC_DOOR, 0xb01, sub(ESM::fourCC("EDID"), std::string("OwnedDoor\0", 10)))
            + group(0, ESM4::REC_CELL, record(ESM4::REC_CELL, 0xb00,
                sub(ESM::fourCC("EDID"), std::string("OwnershipRoom\0", 14))
                + sub(ESM::fourCC("DATA"), bytes(std::uint16_t{ESM4::CELL_Interior}))))
            + group(6, 0xb00, group(9, 0xb00, record(ESM4::REC_REFR, 0xb02,
                sub(ESM::fourCC("NAME"), bytes(0xb01u)) + sub(ESM::fourCC("DATA"), bytes(position)))));
    }

    void setupPlayer(MWWorld::World& world)
    {
        MWClass::Npc::registerSelf();
        MWClass::ESM4Npc::registerSelf();
        MWClass::ESM4Door::registerSelf();
        world.setupPlayer();
        ESM::FormKeyResolver resolver(world.getContentFiles());
        auto& store = world.getStore();
        ESM4::Race race{};
        race.mId = *resolver.toFormId(key(0xb04));
        race.mAttribMale = race.mAttribFemale = {40,40,40,40,40,40,40,40};
        store.getWritable<ESM4::Race>().insertStatic(race, key(0xb04));
        ESM4::Class characterClass{};
        characterClass.mId = *resolver.toFormId(key(0xb05));
        characterClass.mData.mFavoredAttributes = {0,1};
        for (std::size_t i=0;i<7;++i) characterClass.mData.mMajorSkills[i] = 12+i;
        store.getWritable<ESM4::Class>().insertStatic(characterClass, key(0xb05));
        ESM::Race sharedRace;
        sharedRace.blank(); sharedRace.mId = ESM::RefId(race.mId);
        store.getWritable<ESM::Race>().insertStatic(sharedRace);
        ESM::Class sharedClass;
        sharedClass.blank(); sharedClass.mId = ESM::RefId(characterClass.mId);
        store.getWritable<ESM::Class>().insertStatic(sharedClass);
        auto player = *world.getPlayerPtr().get<ESM::NPC>()->mBase;
        player.mRace = sharedRace.mId;
        player.mClass = sharedClass.mId;
        auto metadata = store.preparePlayerRecord(player);
        world.getPlayer().set(metadata.commit());
        auto& room = world.getWorldModel().getCell(ESM::RefId(*resolver.toFormId(key(0xb00))));
        world.getPlayer().setCell(&room);
        const auto door = room.getPtr(ESM::RefId(*resolver.toFormId(key(0xb01))));
        if (door.isEmpty()) throw std::runtime_error("ownership fixture has no native door");
        world.getWorldModel().registerPtr(door);
    }
}

TEST(OblivionOwnership, ScriptOwnerUsesFactionOrNpcBaseAndSurvivesNativeEncodingWithShiftedContent)
{
    for (bool shifted : {false, true})
    {
        SCOPED_TRACE(shifted);
        TestingOpenMW::NativeWorldFixture fixture(shifted, false, false, {}, {}, ownershipRecords(), 4);
        auto& world = fixture.mWorld;
        setupPlayer(world);
        auto& host = *world.getOblivionScriptManager();
        ESM::FormKeyResolver resolver(world.getContentFiles());
        const auto placed = *resolver.toFormId(key(0xb02));
        EXPECT_EQ(ObScript::asInteger(host.call("SetOwnership", ref(0xb02), {ref(0xb03)}, {}, {})), 0);
        const auto ptr = world.getWorldModel().getPtr(placed);
        ASSERT_FALSE(ptr.isEmpty());
        EXPECT_EQ(ptr.getCellRef().getOwner(), ESM::RefId(*resolver.toFormId(key(0xb03))));
        auto state = world.captureOblivionRuntimeState();
        auto restored = ESM4::RuntimeState::deserializeBinary(state.serializeBinary());
        const auto owner = std::ranges::find(restored.mReferences, key(0xb02), &ESM4::RuntimeReferenceState::mKey);
        ASSERT_NE(owner, restored.mReferences.end());
        EXPECT_EQ(owner->mOwner, key(0xb03));
        // The omitted/zero optional owner means the Player's NPC base, not
        // its runtime reference identity or the target's base.
        for (const auto& arguments : {std::vector<ObScript::Value>{}, std::vector<ObScript::Value>{std::int64_t{0}}})
        {
            host.call("SetOwnership", ref(0xb02), arguments, {}, {});
            EXPECT_EQ(ptr.getCellRef().getOwner(), ESM::RefId(*resolver.toFormId(key(0x800))));
            state = world.captureOblivionRuntimeState();
            restored = ESM4::RuntimeState::deserializeBinary(state.serializeBinary());
            const auto current = std::ranges::find(restored.mReferences, key(0xb02), &ESM4::RuntimeReferenceState::mKey);
            ASSERT_NE(current, restored.mReferences.end());
            EXPECT_EQ(current->mOwner, key(0x800));
        }
    }
}

TEST(OblivionOwnership, InvalidOwnerKindsAndArgumentsRejectWithoutChangingOwnership)
{
    TestingOpenMW::NativeWorldFixture fixture(false, false, false, {}, {}, ownershipRecords(), 4);
    auto& world = fixture.mWorld;
    setupPlayer(world);
    auto& host = *world.getOblivionScriptManager();
    host.call("SetOwnership", ref(0xb02), {ref(0xb03)}, {}, {});
    const auto before = world.captureOblivionRuntimeState().mReferences;
    for (const auto& arguments : {std::vector<ObScript::Value>{ref(0xb01)},
             std::vector<ObScript::Value>{ref(0xb02)}, std::vector<ObScript::Value>{std::string("UnknownOwner")},
             std::vector<ObScript::Value>{std::int64_t{1}}, std::vector<ObScript::Value>{ref(0xb03), ref(0xb03)}})
    {
        EXPECT_THROW(host.call("SetOwnership", ref(0xb02), arguments, {}, {}), ObScript::RuntimeError);
        EXPECT_EQ(world.captureOblivionRuntimeState().mReferences, before);
    }
}

namespace
{
    std::string exitRecords(std::uint16_t sourceFlags, std::uint16_t destinationFlags,
        std::uint8_t level, bool missingDestination)
    {
        const std::array<float, 6> position{};
        std::string teleport = bytes(missingDestination ? 0xdeadu : 0xb12u) + bytes(position);
        std::string lock(12, '\0'); lock[0] = static_cast<char>(level);
        return record(ESM4::REC_FACT, 0xb03, sub(ESM::fourCC("EDID"), std::string("OwnerFaction\0", 13))
                + sub(ESM::fourCC("DATA"), std::string(1, '\0')))
            + record(ESM4::REC_DOOR, 0xb01, sub(ESM::fourCC("EDID"), std::string("OwnedDoor\0", 10)))
            + group(0, ESM4::REC_CELL, record(ESM4::REC_CELL, 0xb00,
                sub(ESM::fourCC("EDID"), std::string("OwnershipRoom\0", 14))
                + sub(ESM::fourCC("DATA"), bytes(sourceFlags))
                + sub(ESM::fourCC("XOWN"), bytes(0xb03u))))
            + group(6, 0xb00, group(9, 0xb00, record(ESM4::REC_REFR, 0xb02,
                sub(ESM::fourCC("NAME"), bytes(0xb01u)) + sub(ESM::fourCC("DATA"), bytes(position))
                + sub(ESM::fourCC("XTEL"), teleport) + sub(ESM::fourCC("XLOC"), lock))))
            + group(0, ESM4::REC_CELL, record(ESM4::REC_CELL, 0xb10,
                sub(ESM::fourCC("EDID"), std::string("ExitRoom\0", 9))
                + sub(ESM::fourCC("DATA"), bytes(destinationFlags))))
            + group(6, 0xb10, group(9, 0xb10, record(ESM4::REC_REFR, 0xb12,
                sub(ESM::fourCC("NAME"), bytes(0xb01u)) + sub(ESM::fourCC("DATA"), bytes(position)))));
    }
}

TEST(OblivionOwnership, PlayerTrespassExitUsesResolvedDestinationAndPreservesLock)
{
    for (bool shifted : {false, true})
        for (const auto flags : {0, ESM4::CELL_Interior | ESM4::CELL_Public,
                 int(ESM4::CELL_Interior), ESM4::CELL_Interior | ESM4::CELL_HandChgd})
            for (unsigned level : {40, 100, 101})
            {
                SCOPED_TRACE(::testing::Message() << shifted << ":" << flags << ":" << level);
                TestingOpenMW::NativeWorldFixture fixture(shifted, false, false, {}, {},
                    exitRecords(ESM4::CELL_Interior, flags, level, false), 6);
                auto& world = fixture.mWorld;
                setupPlayer(world);
                const auto player = world.getPlayerPtr();
                const auto door = world.getWorldModel().getPtr(*ESM::FormKeyResolver(world.getContentFiles()).toFormId(key(0xb02)));
                ASSERT_TRUE(door.getCellRef().isLocked());
                ASSERT_EQ(MWWorld::oblivionActorTrespassing(world, player, {}, key(0xb00)), true);
                const auto before = world.captureOblivionRuntimeState().serializeBinary();
                EXPECT_EQ(MWWorld::oblivionPlayerHasDoorExitPermission(world, door, player),
                    level != 100 && (!(flags & ESM4::CELL_Interior) || (flags & ESM4::CELL_Public)));
                EXPECT_FALSE(MWWorld::oblivionPlayerHasDoorExitPermission(world, door, {}));
                ESM4::ActorCharacter placed{};
                placed.mId = *ESM::FormKeyResolver(world.getContentFiles()).toFormId(key(0xa00));
                placed.mFormKey = key(0xa00);
                const auto* base = world.getStore().search<ESM4::Npc>(key(0x800));
                placed.mBaseObj = base->mId;
                placed.mBaseKey = key(0x800);
                MWWorld::LiveCellRef<ESM4::Npc> npc(placed, base);
                EXPECT_FALSE(MWWorld::oblivionPlayerHasDoorExitPermission(world, door, MWWorld::Ptr(&npc, nullptr)));
                EXPECT_TRUE(door.getCellRef().isLocked());
                EXPECT_EQ(door.getCellRef().getLockLevel(), static_cast<int>(level));
                EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), before);
            }
}

TEST(OblivionOwnership, PlayerExitRejectsUnresolvedDestinationAndDoesNotGrantPublicCellPermission)
{
    for (const bool publicSource : {false, true})
        for (unsigned level : {40, 100})
        {
            TestingOpenMW::NativeWorldFixture fixture(false, false, false, {}, {},
                exitRecords(ESM4::CELL_Interior | (publicSource ? ESM4::CELL_Public : 0),
                    ESM4::CELL_Interior | ESM4::CELL_Public, level, true), 6);
            auto& world = fixture.mWorld;
            setupPlayer(world);
            const auto door = world.getWorldModel().getPtr(*ESM::FormKeyResolver(world.getContentFiles()).toFormId(key(0xb02)));
            if (!publicSource && level != 100)
                EXPECT_THROW(MWWorld::oblivionPlayerHasDoorExitPermission(world, door, world.getPlayerPtr()), std::invalid_argument);
            else
                EXPECT_FALSE(MWWorld::oblivionPlayerHasDoorExitPermission(world, door, world.getPlayerPtr()));
            EXPECT_TRUE(door.getCellRef().isLocked());
        }
}

TEST(OblivionScriptGlobals, CalendarReadsLiveOwnerWithoutRequiringSaveReload)
{
    for (bool floatCalendar : {false, true})
    {
        SCOPED_TRACE(floatCalendar);
        std::string globals;
        const std::array<std::tuple<const char*, char, float>, 7> declarations{{
            {"GameDaysPassed", 's', 4}, {"GameHour", floatCalendar ? 'f' : 's', 13.5f},
            {"GameDay", 's', 30}, {"GameMonth", 's', 7}, {"GameYear", 's', 433},
            {"TimeScale", floatCalendar ? 'f' : 's', 30}, {"GameHourCounter", 's', 10}
        }};
        unsigned id = 0xc00;
        for (const auto& [name, type, value] : declarations)
            globals += record(ESM4::REC_GLOB, id++, sub(ESM::fourCC("EDID"), std::string(name) + '\0')
                + sub(ESM::fourCC("FNAM"), std::string(1, type)) + sub(ESM::fourCC("FLTV"), bytes(value)));
        TestingOpenMW::NativeWorldFixture fixture(false, false, false, {}, {}, ownershipRecords() + globals, 11);
        auto& world = fixture.mWorld;
        setupPlayer(world);
        auto& host = *world.getOblivionScriptManager();
        // Creating script-owned state captures the old calendar projection.
        host.storeExternal("GameHourCounter", std::int64_t{11}, {});
        ASSERT_EQ(ObScript::asInteger(host.resolveName("GameDaysPassed", {})), 4);
        world.setGlobalInt(MWWorld::GlobalVariableName(std::string_view("GameDaysPassed")), 6);
        world.setGlobalFloat(MWWorld::GlobalVariableName(std::string_view("GameHour")), 18.75f);
        world.setGlobalInt(MWWorld::GlobalVariableName(std::string_view("GameDay")), 1);
        world.setGlobalInt(MWWorld::GlobalVariableName(std::string_view("GameMonth")), 8);
        world.setGlobalInt(MWWorld::GlobalVariableName(std::string_view("GameYear")), 434);
        world.setGlobalFloat(MWWorld::GlobalVariableName(std::string_view("TimeScale")), 60.f);
        const auto before = world.captureOblivionRuntimeState().serializeBinary();
        EXPECT_EQ(ObScript::asInteger(host.resolveName("GameDaysPassed", {})), 6);
        EXPECT_DOUBLE_EQ(ObScript::asNumber(host.resolveName("GameHour", {})), floatCalendar ? 18.75 : 18.0);
        EXPECT_EQ(std::holds_alternative<double>(host.resolveName("GameHour", {})), floatCalendar);
        EXPECT_EQ(ObScript::asInteger(host.resolveName("GameDay", {})), 1);
        EXPECT_EQ(ObScript::asInteger(host.resolveName("GameMonth", {})), 8);
        EXPECT_EQ(ObScript::asInteger(host.resolveName("GameYear", {})), 434);
        EXPECT_DOUBLE_EQ(ObScript::asNumber(host.resolveName("TimeScale", {})), 60);
        EXPECT_EQ(ObScript::asInteger(host.resolveName("GameHourCounter", {})), 11);
        EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), before);
    }
}

namespace
{
    std::string packageDoorRecords(unsigned owner)
    {
        const std::array<float, 6> position{};
        const auto cell = [&](unsigned id, const char* name, unsigned owningNpc) {
            return group(0, ESM4::REC_CELL, record(ESM4::REC_CELL, id,
                sub(ESM::fourCC("EDID"), std::string(name) + '\0')
                + sub(ESM::fourCC("DATA"), bytes(std::uint16_t{ESM4::CELL_Interior}))
                + sub(ESM::fourCC("XOWN"), bytes(owningNpc))));
        };
        const auto door = [&](unsigned id, unsigned destination, unsigned level) {
            std::string data = sub(ESM::fourCC("NAME"), bytes(0xb01u))
                + sub(ESM::fourCC("DATA"), bytes(position));
            if (destination) data += sub(ESM::fourCC("XTEL"), bytes(destination) + bytes(position));
            if (level)
            {
                std::string lock(12, '\0'); lock[0] = static_cast<char>(level);
                data += sub(ESM::fourCC("XLOC"), lock);
            }
            return record(ESM4::REC_REFR, id, data);
        };
        return record(ESM4::REC_FACT, 0xb03, sub(ESM::fourCC("EDID"), std::string("PolicyOwners") + '\0')
                + sub(ESM::fourCC("DATA"), std::string(1, '\0')))
            + record(ESM4::REC_DOOR, 0xb01, sub(ESM::fourCC("EDID"), std::string("BoundaryDoor") + '\0'))
            + cell(0xb00, "OwnershipRoom", owner)
            + group(6, 0xb00, group(9, 0xb00,
                door(0xb02, 0xb12, 0) + door(0xb33, 0, 90)))
            + cell(0xb10, "OldResidentRoom", 0x800)
            + group(6, 0xb10, group(9, 0xb10,
                door(0xb12, 0xb02, 30) + door(0xb35, 0xb36, 60) + door(0xb36, 0xb35, 0)))
            + cell(0xb21, "OldActorProjection", 0x800)
            + group(6, 0xb21, group(9, 0xb21, record(ESM4::REC_ACHR, 0xb20,
                    sub(ESM::fourCC("NAME"), bytes(0x800u))
                    + sub(ESM::fourCC("DATA"), bytes(position)))));
    }

    ESM4::RuntimeState packageArrivalState(MWWorld::World& world)
    {
        auto state = world.captureOblivionRuntimeState();
        ESM4::RuntimeActorAiState actor;
        actor.mActor = key(0xb20); actor.mBase = key(0x800);
        actor.mPackage = key(0xb40);
        actor.mCell = actor.mLastValidCell = actor.mDestinationCell = key(0xb00);
        actor.mHasDestination = true;
        actor.mPackageType = ESM4::AIPackageType::Wander;
        actor.mProcedure = ESM4::PackageProcedure::Wander;
        actor.mSource = ESM4::PackageSource::Base;
        actor.mConditionResult = ESM4::ConditionResult::True;
        actor.mPhase = ESM4::PackagePhase::Arrive;
        actor.mTier = ESM4::ProcessTier::Low;
        actor.mDurationRemaining = 6;
        state.mActorAi = {actor};
        return state;
    }

    void installDoorPackage(MWWorld::World& world)
    {
        ESM4::AIPackage package{};
        package.mId = *ESM::FormKeyResolver(world.getContentFiles()).toFormId(key(0xb40));
        package.mFormKey = key(0xb40);
        package.mPackageType = ESM4::AIPackageType::Wander;
        package.mPackageFlags = ESM4::decodePackageFlags(0x110); // Jiv: unlock at location, lock at end.
        package.mScheduleData.mStartHour = -1;
        world.getStore().getWritable<ESM4::AIPackage>().insertStatic(package, package.mFormKey);
        auto native = *world.getStore().search<ESM4::Npc>(key(0x800));
        native.mAIPackageKeys = {package.mFormKey};
        native.mRace = *ESM::FormKeyResolver(world.getContentFiles()).toFormId(key(0xb04));
        native.mClass = *ESM::FormKeyResolver(world.getContentFiles()).toFormId(key(0xb05));
        world.getStore().getWritable<ESM4::Npc>().insertStatic(native, native.mFormKey);
    }

    MWWorld::Ptr boundaryDoor(MWWorld::World& world)
    {
        auto& cell = world.getWorldModel().getCell(ESM::RefId(
            *ESM::FormKeyResolver(world.getContentFiles()).toFormId(key(0xb10))));
        MWWorld::Ptr result;
        cell.forEach([&](const MWWorld::Ptr& ptr) {
            if (ptr.getCellRef().getFormKey() == key(0xb12)) result = ptr;
            return true;
        });
        if (result.isEmpty()) throw std::runtime_error("missing package boundary door");
        world.getWorldModel().registerPtr(result);
        return result;
    }
}

TEST(OblivionOwnership, PackageArrivalUsesAiCellAndChangesOnlyOwnedBoundaryLocks)
{
    for (const bool shifted : {false, true})
    for (const bool warm : {false, true})
    for (const auto [owner, rank, owns] : std::array{
        std::tuple{0x800u, 0, true}, std::tuple{0xb03u, 0, true},
        std::tuple{0xb03u, -1, false}, std::tuple{7u, 0, false}})
    {
        SCOPED_TRACE(shifted);
        SCOPED_TRACE(warm);
        SCOPED_TRACE(owner);
        SCOPED_TRACE(rank);
        TestingOpenMW::NativeWorldFixture fixture(shifted, false, false, {}, {},
            packageDoorRecords(owner), 11);
        auto& world = fixture.mWorld;
        setupPlayer(world);
        installDoorPackage(world);
        auto native = *world.getStore().search<ESM4::Npc>(key(0x800));
        native.mFactions.push_back({ESM::FormKeyResolver(world.getContentFiles()).toFormId(key(0xb03))->toUint32(),
            static_cast<std::int8_t>(rank), 0, 0, 0});
        world.getStore().getWritable<ESM4::Npc>().insertStatic(native, native.mFormKey);
        if (warm) { EXPECT_TRUE(boundaryDoor(world).getCellRef().isLocked()); }
        auto& ai = *world.getOblivionAiService();
        const auto before = world.captureOblivionRuntimeState();
        ai.restore(packageArrivalState(world));
        ai.updateUnloaded(.25f);
        EXPECT_TRUE(world.getWorldModel().getPtr(
            *ESM::FormKeyResolver(world.getContentFiles()).toFormId(key(0xb20))).isEmpty());
        if (owns && !warm)
        {
            const auto pending = world.captureOblivionRuntimeState();
            const auto it = std::find_if(pending.mReferences.begin(), pending.mReferences.end(),
                [&](const auto& reference) { return reference.mKey == key(0xb12); });
            ASSERT_NE(it, pending.mReferences.end());
            EXPECT_TRUE(std::get<bool>(it->mCustomState.at("native.package.lock_projection_pending")));
            EXPECT_EQ(ESM4::RuntimeState::deserializeBinary(pending.serializeBinary()).mReferences,
                pending.mReferences);
        }
        ASSERT_TRUE(ai.state(key(0xb20)));
        EXPECT_EQ(ai.state(key(0xb20))->mCell, key(0xb00));
        EXPECT_EQ(ai.state(key(0xb20))->mPhase, ESM4::PackagePhase::Wait);
        const auto door = boundaryDoor(world);
        EXPECT_EQ(door.getCellRef().isLocked(), !owns);
        EXPECT_EQ(door.getCellRef().getLockLevel(), owns ? -30 : 30);
        auto after = world.captureOblivionRuntimeState();
        EXPECT_EQ(after.mNativeCrime, before.mNativeCrime);
        EXPECT_EQ(after.mPlayer.mInventory, before.mPlayer.mInventory);
        // The inner side has no authored lock. A physical prison-style lock
        // and another room's teleport lock must not be created/changed.
        for (const auto [id, level] : {std::pair{0xb02u, 0}, {0xb33u, 90}, {0xb35u, 60}})
        {
            const auto it = std::find_if(after.mReferences.begin(), after.mReferences.end(),
                [&](const auto& reference) { return reference.mKey == key(id); });
            ASSERT_NE(it, after.mReferences.end());
            EXPECT_EQ(it->mLockLevel, level);
        }
    }
}

TEST(OblivionOwnership, PackageDoorEndRelocksOnceAndRestoredWaitDoesNotReplayArrival)
{
    TestingOpenMW::NativeWorldFixture fixture(false, false, false, {}, {}, packageDoorRecords(0x800), 11);
    auto& world = fixture.mWorld;
    setupPlayer(world);
    installDoorPackage(world);
    auto& ai = *world.getOblivionAiService();
    ai.restore(packageArrivalState(world));
    ai.updateUnloaded(.25f);
    const auto door = boundaryDoor(world);
    ASSERT_FALSE(door.getCellRef().isLocked());
    auto saved = world.captureOblivionRuntimeState();
    ai.restore(ESM4::RuntimeState::deserializeBinary(saved.serializeBinary()));
    // A manual/script lock after arrival must survive restoration of the
    // already-waiting package. Arrival is an event, not a per-frame repair.
    door.getCellRef().lock(45);
    ai.updateUnloaded(.25f);
    EXPECT_TRUE(door.getCellRef().isLocked());
    EXPECT_EQ(door.getCellRef().getLockLevel(), 45);
    door.getCellRef().unlock();
    auto ended = world.captureOblivionRuntimeState();
    auto& actor = *std::find_if(ended.mActorAi.begin(), ended.mActorAi.end(),
        [&](const auto& value) { return value.mActor == key(0xb20); });
    actor.mDurationRemaining = 0;
    ai.restore(ended);
    ai.updateUnloaded(.25f);
    EXPECT_TRUE(door.getCellRef().isLocked());
    EXPECT_EQ(door.getCellRef().getLockLevel(), 45);
    EXPECT_EQ(ai.state(key(0xb20))->mPhase, ESM4::PackagePhase::Complete);
    door.getCellRef().unlock();
    ai.updateUnloaded(.25f); // No next package: ending must not be replayed.
    EXPECT_FALSE(door.getCellRef().isLocked());
}

TEST(OblivionOwnership, DeferredPackageLockSurvivesWorldRestoreAndRejectsMalformedProjectionBeforeCommit)
{
    TestingOpenMW::NativeWorldFixture fixture(false, false, false, {}, {}, packageDoorRecords(0x800), 11);
    auto& world = fixture.mWorld;
    setupPlayer(world);
    installDoorPackage(world);
    auto& ai = *world.getOblivionAiService();
    ai.restore(packageArrivalState(world));
    ai.updateUnloaded(.25f);
    const auto saved = world.captureOblivionRuntimeState();
    const auto savedDoor = std::find_if(saved.mReferences.begin(), saved.mReferences.end(),
        [&](const auto& reference) { return reference.mKey == key(0xb12); });
    ASSERT_NE(savedDoor, saved.mReferences.end());
    ASSERT_TRUE(savedDoor->mCustomState.contains("native.package.lock_projection_pending"));
    for (const bool badFlag : {false, true})
    {
        auto invalid = saved;
        auto& door = *std::find_if(invalid.mReferences.begin(), invalid.mReferences.end(),
            [&](const auto& reference) { return reference.mKey == key(0xb12); });
        if (badFlag) door.mCustomState["native.package.lock_projection_pending"] = std::int64_t(1);
        else door.mCustomState.erase("locked");
        EXPECT_THROW(ai.prepareRestore(invalid), std::invalid_argument);
        EXPECT_EQ(world.captureOblivionRuntimeState().serializeBinary(), saved.serializeBinary());
    }
    ESM::ESMWriter writer;
    auto stream = std::make_unique<std::stringstream>();
    writer.setFormatVersion(ESM::CurrentSaveGameFormatVersion);
    writer.save(*stream);
    writer.startRecord(ESM::REC_T4ST);
    saved.save(writer);
    writer.endRecord(ESM::REC_T4ST);
    ESM::ESMReader reader;
    reader.open(std::move(stream), "package-lock-restore");
    ASSERT_EQ(reader.getRecName(), ESM::REC_T4ST);
    reader.getRecHeader();
    world.readRecord(reader, ESM::REC_T4ST);
    ASSERT_NO_THROW(world.applyOblivionRuntimeState());
    const auto door = boundaryDoor(world);
    EXPECT_FALSE(door.getCellRef().isLocked());
    EXPECT_EQ(door.getCellRef().getLockLevel(), -30);
    auto projected = world.captureOblivionRuntimeState();
    const auto projectedDoor = std::find_if(projected.mReferences.begin(), projected.mReferences.end(),
        [&](const auto& reference) { return reference.mKey == key(0xb12); });
    ASSERT_NE(projectedDoor, projected.mReferences.end());
    EXPECT_FALSE(projectedDoor->mCustomState.contains("native.package.lock_projection_pending"));
    door.getCellRef().lock(55);
    world.getWorldModel().registerPtr(door);
    EXPECT_EQ(door.getCellRef().getLockLevel(), 55);
    EXPECT_TRUE(door.getCellRef().isLocked()); // Registration cannot replay the old unlock.
}

TEST(OblivionOwnership, InterruptibleAmbushReevaluatesChangedConditionsAndReleasesItsDoor)
{
    for (const bool mustComplete : {false, true})
    for (const auto phase : {ESM4::PackagePhase::Wait, ESM4::PackagePhase::ReadyForM15Combat})
    {
        SCOPED_TRACE(mustComplete);
        SCOPED_TRACE(static_cast<unsigned>(phase));
        TestingOpenMW::NativeWorldFixture fixture(false, false, false, {}, {}, packageDoorRecords(0x800), 11);
        auto& world = fixture.mWorld;
        setupPlayer(world);
        installDoorPackage(world);
        auto package = *world.getStore().search<ESM4::AIPackage>(key(0xb40));
        package.mPackageType = ESM4::AIPackageType::Ambush;
        package.mPackageFlags = ESM4::decodePackageFlags(0x10 | (mustComplete ? 4 : 0));
        ESM4::PackageCondition condition{};
        condition.mFunction = 68; // GetIsClass: class now matches, making Equals0 false.
        condition.mComparisonValue = 0;
        condition.mOperator = ESM4::ConditionOperator::Equal;
        condition.mParameter1.mIsFormId = true;
        condition.mParameter1.mReferenceKey = key(0xb05);
        package.mCanonicalConditions = {condition};
        world.getStore().getWritable<ESM4::AIPackage>().insertStatic(package, package.mFormKey);
        const auto door = boundaryDoor(world);
        door.getCellRef().unlock();
        auto state = packageArrivalState(world);
        auto& actor = state.mActorAi.front();
        actor.mPackageType = ESM4::AIPackageType::Ambush;
        actor.mProcedure = ESM4::PackageProcedure::Ambush;
        actor.mPhase = phase;
        actor.mTarget = state.mPlayer.mReference;
        actor.mTargetBase = ESM::FormKey::dynamic("player-base", 1);
        auto& ai = *world.getOblivionAiService();
        ai.restore(state);
        ai.updateUnloaded(.25f);
        EXPECT_EQ(ai.state(key(0xb20))->mPackage, mustComplete ? key(0xb40) : ESM::FormKey{});
        EXPECT_EQ(door.getCellRef().isLocked(), !mustComplete);
    }
}
