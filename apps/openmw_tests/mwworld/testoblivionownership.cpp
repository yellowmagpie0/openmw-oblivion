#include <gtest/gtest.h>

#include "oblivionworldfixture.hpp"
#include "apps/openmw/mwclass/npc.hpp"
#include "apps/openmw/mwclass/esm4npc.hpp"
#include "apps/openmw/mwclass/esm4interactive.hpp"
#include "apps/openmw/mwworld/cellstore.hpp"
#include "apps/openmw/mwworld/oblivionscriptmanager.hpp"
#include "apps/openmw/mwworld/esmstore.hpp"
#include "apps/openmw/mwworld/player.hpp"
#include "apps/openmw/mwworld/worldmodel.hpp"
#include <components/esm4/common.hpp>
#include <components/esm4/loadcell.hpp>
#include <components/esm4/loadclas.hpp>
#include <components/esm4/loadrace.hpp>
#include <components/esm4/runtimestate.hpp>
#include <components/esm3/loadclas.hpp>
#include <components/esm3/loadrace.hpp>

#include <algorithm>
#include <array>

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
