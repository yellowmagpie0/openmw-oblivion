#include "apps/openmw/mwworld/esmstore.hpp"
#include "apps/openmw/mwworld/oblivioncombatmagic.hpp"
#include "apps/openmw/mwworld/oblivionmiscconsumables.hpp"

#include <components/esm4/common.hpp>
#include <components/esm4/reader.hpp>
#include <components/esm4/loadspel.hpp>
#include <components/esm4/loadingr.hpp>
#include <components/esm4/loadweap.hpp>
#include <components/esm4/loadench.hpp>
#include <components/esm4/loadbook.hpp>
#include <components/esm4/loadalch.hpp>
#include <components/esm4/loadmisc.hpp>
#include <components/esm4/runtimestate.hpp>
#include <limits>
#include <components/loadinglistener/loadinglistener.hpp>

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <sstream>
#include <vector>

namespace
{
    template <class T> std::string bytes(const T& value)
    {
        return {reinterpret_cast<const char*>(&value), sizeof(value)};
    }

    std::string sub(std::uint32_t tag, const std::string& data)
    {
        return bytes(tag) + bytes(static_cast<std::uint16_t>(data.size())) + data;
    }

    std::string record(std::uint32_t tag, std::uint32_t id, const std::string& data, bool deleted = false)
    {
        return bytes(tag) + bytes(static_cast<std::uint32_t>(data.size()))
            + bytes(deleted ? std::uint32_t(ESM4::Rec_Deleted) : 0u) + bytes(id) + bytes(0u) + data;
    }

    std::string effect(std::uint32_t id = 0x1887, std::uint32_t flags = 0x01000d73, std::int32_t resistance = 66)
    {
        const auto data = bytes(flags) + bytes(475.f) + bytes(48u) + bytes(3u) + bytes(resistance)
            + std::string(44, '\0'); // Full64-byte TES4 DATA; presentation/counter tail irrelevant to this query.
        return record(ESM4::REC_MGEF, id, sub(ESM::fourCC("EDID"), std::string("PARA\0", 5))
            + sub(ESM::fourCC("DATA"), data));
    }

    std::string spell(std::uint32_t duration = 10)
    {
        const auto spit = bytes(0u) + bytes(0u) + bytes(0u) + std::string("\1\0\0\0", 4);
        const auto efit = bytes(ESM::fourCC("PARA")) + bytes(0u) + bytes(0u) + bytes(duration) + bytes(0u) + bytes(48u);
        return record(ESM4::REC_SPEL, 0x137, sub(ESM::fourCC("SPIT"), spit)
            + sub(ESM::fourCC("EFID"), bytes(ESM::fourCC("PARA"))) + sub(ESM::fourCC("EFIT"), efit));
    }

    struct Fixture
    {
        MWWorld::ESMStore mStore;
        Loading::Listener mListener;
        // A preceding file shifts the resolved base index;000137 is a stable
        // Oblivion.esm key, not resolved index0 or any spell with that local ID.
        const std::map<std::string, int> mIndices{{"prefix.esm", 0}, {"oblivion.esm", 1},
            {"patch.esp", 2}, {"other.esm", 3}, {"final.esp", 4}};

        void load(const std::string& name, const std::vector<std::string>& masters,
            const std::vector<std::string>& records)
        {
            auto header = sub(ESM::fourCC("HEDR"), bytes(1.f)
                + bytes(static_cast<std::uint32_t>(records.size())) + bytes(0x2000u));
            for (const auto& master : masters)
                header += sub(ESM::fourCC("MAST"), master + '\0')
                    + sub(ESM::fourCC("DATA"), std::string(8, '\0'));
            auto data = record(ESM4::REC_TES4, 0, header);
            for (const auto& item : records)
                data += item;
            auto stream = std::make_unique<std::stringstream>(data, std::ios::in | std::ios::binary);
            ESM4::Reader reader(std::move(stream), name, nullptr, nullptr, true);
            reader.setModIndex(mIndices.at(name));
            reader.updateModIndices(mIndices);
            mStore.loadESM4(reader, &mListener);
        }

        bool supported() const { return MWWorld::supportsOblivionDefaultCombatParalysis(mStore); }
    };
}

TEST(OblivionCombatMagicStore, SelectsStableDefaultSpellAndWinningOverridesThroughActualRecordLoading)
{
    Fixture fixture;
    EXPECT_FALSE(fixture.supported()); // Do not invent a missing winning MGEF.
    fixture.load("oblivion.esm", {}, {effect()});
    EXPECT_TRUE(fixture.supported()); // No000137 selects the native fallback.
    fixture.load("other.esm", {}, {spell(99)});
    EXPECT_TRUE(fixture.supported()); // Same local ID in another plugin is unrelated.
    const auto before = fixture.mStore.getFormKeyIndex().serialize();
    EXPECT_TRUE(fixture.supported());
    EXPECT_EQ(fixture.mStore.getFormKeyIndex().serialize(), before);
}

TEST(OblivionCombatMagicStore, AuthoredDefaultSpellUsesWinningOverrides)
{
    Fixture fixture;
    fixture.load("oblivion.esm", {}, {effect(), spell()});
    EXPECT_TRUE(fixture.supported());
    fixture.load("patch.esp", {"oblivion.esm"}, {spell(11)});
    EXPECT_FALSE(fixture.supported()); // Winning authored default cannot silently use duration10.
    fixture.load("final.esp", {"oblivion.esm", "patch.esp"}, {spell()});
    EXPECT_TRUE(fixture.supported());
    const auto before = fixture.mStore.getFormKeyIndex().serialize();
    EXPECT_TRUE(fixture.supported());
    EXPECT_EQ(fixture.mStore.getFormKeyIndex().serialize(), before);
}

TEST(OblivionCombatMagicStore, DeletedDefaultSpellUsesFallbackButDeletedParalysisDefinitionStaysUnsupported)
{
    Fixture fixture;
    fixture.load("oblivion.esm", {}, {effect(), spell(11)});
    EXPECT_FALSE(fixture.supported());
    fixture.load("patch.esp", {"oblivion.esm"}, {record(ESM4::REC_SPEL, 0x137, {}, true)});
    EXPECT_TRUE(fixture.supported());
    fixture.load("final.esp", {"oblivion.esm", "patch.esp"}, {record(ESM4::REC_MGEF, 0x1887, {}, true)});
    EXPECT_FALSE(fixture.supported());
}

TEST(OblivionCombatMagicStore, ResolvesWinningEffectPropertiesAndRejectsAmbiguousCodes)
{
    Fixture fixture;
    fixture.load("oblivion.esm", {}, {effect()});
    EXPECT_TRUE(fixture.supported());
    fixture.load("patch.esp", {"oblivion.esm"}, {effect(0x1887, 0x01000d73, 64)});
    EXPECT_FALSE(fixture.supported());
    fixture.load("final.esp", {"oblivion.esm", "patch.esp"}, {effect()});
    EXPECT_TRUE(fixture.supported());
    fixture.load("other.esm", {}, {effect(0x800)});
    EXPECT_FALSE(fixture.supported()); // Different winning keys with PARA code are ambiguous.
}

TEST(OblivionCombatMagicStore, OtherDefaultFormTypeUsesOriginalSpellItemCastFallback)
{
    Fixture fixture;
    const auto gmst = record(ESM4::REC_GMST, 0x137,
        sub(ESM::fourCC("EDID"), std::string("iTest\0", 6)) + sub(ESM::fourCC("DATA"), bytes(1u)));
    fixture.load("oblivion.esm", {}, {effect(), gmst});
    EXPECT_TRUE(fixture.supported());
}

namespace
{
    std::string healingEffect()
    {
        const auto data = bytes(0x01000870u) + bytes(10.f) + bytes(8u) + bytes(5u) + bytes(-1)
            + std::string(44, '\0');
        return record(ESM4::REC_MGEF, 0x188c,
            sub(ESM::fourCC("EDID"), std::string("REHE\0", 5))
            + sub(ESM::fourCC("FULL"), std::string("Restore Health\0", 15))
            + sub(ESM::fourCC("ICON"), std::string("magic\\restorehealth.dds\0", 24))
            + sub(ESM::fourCC("DATA"), data));
    }

    std::string healingSpell(std::uint8_t flags = 0)
    {
        const auto spit = bytes(0u) + bytes(24u) + bytes(0u) + bytes(flags) + std::string(3, '\0');
        const auto efit = bytes(ESM::fourCC("REHE")) + bytes(8u) + bytes(0u) + bytes(0u) + bytes(0u) + bytes(8u);
        return record(ESM4::REC_SPEL, 0x136, sub(ESM::fourCC("SPIT"), spit)
            + sub(ESM::fourCC("FULL"), std::string("Heal Minor Wounds\0", 18))
            + sub(ESM::fourCC("EFID"), bytes(ESM::fourCC("REHE"))) + sub(ESM::fourCC("EFIT"), efit));
    }

    std::string floatSetting(std::uint32_t id, const std::string& name, float value)
    {
        return record(ESM4::REC_GMST, id,
            sub(ESM::fourCC("EDID"), name + '\0') + sub(ESM::fourCC("DATA"), bytes(value)));
    }

    MWWorld::OblivionSpellPresentation describeHealing(const Fixture& fixture)
    {
        const auto* value = fixture.mStore.get<ESM4::Spell>().search(ESM::FormId{0x136, 1});
        if (!value) throw std::runtime_error("missing fixture spell");
        return MWWorld::describeOblivionSpell(fixture.mStore, *value, {5, 5, 5, 5, 5, 5}, 50);
    }
}

TEST(OblivionCombatMagicStore, DisplaysTutorialHealingWithWinningNativeCostSettings)
{
    Fixture fixture;
    fixture.load("oblivion.esm", {}, {healingEffect(), healingSpell(),
        floatSetting(0xcd320, "fMagicCostScale", 1.28f),
        floatSetting(0x6c64c, "fMagicCasterSkillCostBase", .2f)});
    auto value = describeHealing(fixture);
    ASSERT_TRUE(value.mCost);
    EXPECT_FLOAT_EQ(*value.mCost, 19.f); // Independent original-executable tutorial cost case.
    EXPECT_EQ(value.mDescription, "Restore Health 8 pts on Self");
    EXPECT_EQ(value.mIcon, "magic\\restorehealth.dds");
    fixture.load("patch.esp", {"oblivion.esm"}, {healingSpell(1)});
    value = describeHealing(fixture);
    ASSERT_TRUE(value.mCost);
    EXPECT_NEAR(*value.mCost, 32.16f, .001f); // Manual flag uses authored24, before display rounding.
}

TEST(OblivionCombatMagicStore, MissingAndDeletedEffectsDoNotInventSpellCost)
{
    Fixture fixture;
    fixture.load("oblivion.esm", {}, {healingSpell()});
    EXPECT_FALSE(describeHealing(fixture).mCost);
    Fixture complete;
    complete.load("oblivion.esm", {}, {healingSpell(), healingEffect()});
    EXPECT_TRUE(describeHealing(complete).mCost);
    complete.load("patch.esp", {"oblivion.esm"}, {record(ESM4::REC_MGEF, 0x188c, {}, true)});
    EXPECT_FALSE(describeHealing(complete).mCost);
}

namespace
{
    std::string shockEffect(float speed = 2.f, std::uint32_t id = 0x189b)
    {
        const auto data = bytes(0x25001c75u) + bytes(7.8f) + bytes(8u) + bytes(2u) + bytes(68)
            + std::string(8, '\0') + bytes(speed) + std::string(32, '\0');
        return record(ESM4::REC_MGEF, id,
            sub(ESM::fourCC("EDID"), std::string("SHDG\0", 5))
            + sub(ESM::fourCC("MODL"), std::string("MagicEffects\\shockbolt.NIF") + '\0')
            + sub(ESM::fourCC("DATA"), data));
    }

    std::string staffEnchantment(std::uint32_t magnitude = 5, std::uint32_t duration = 0)
    {
        const auto enit = bytes(1u) + bytes(13u) + bytes(13u) + std::string(4, '\0');
        const auto efit = bytes(ESM::fourCC("SHDG")) + bytes(magnitude) + bytes(0u)
            + bytes(duration) + bytes(2u) + bytes(8u);
        return record(ESM4::REC_ENCH, 0x66c42, sub(ESM::fourCC("ENIT"), enit)
            + sub(ESM::fourCC("EFID"), bytes(ESM::fourCC("SHDG"))) + sub(ESM::fourCC("EFIT"), efit));
    }

    std::string staff()
    {
        const auto data = bytes(4u) + bytes(1.f) + bytes(1.f) + bytes(0u) + bytes(50u)
            + bytes(20u) + bytes(8.f) + bytes(std::uint16_t{0});
        return record(ESM4::REC_WEAP, 0x66c45, sub(ESM::fourCC("DATA"), data)
            + sub(ESM::fourCC("ENAM"), bytes(0x66c42u))
            + sub(ESM::fourCC("ANAM"), bytes(std::uint16_t{150})));
    }

    auto resolveStaff(const Fixture& fixture)
    {
        const auto* weapon = fixture.mStore.get<ESM4::Weapon>().search(ESM::FormId{0x66c45, 1});
        if (!weapon) throw std::runtime_error("missing fixture staff");
        return MWWorld::resolveOblivionStaffCastInputs(fixture.mStore, *weapon);
    }
}

TEST(OblivionCombatMagicStore, StaffTargetDamageUsesWinningEnchantmentAndAsset)
{
    Fixture fixture;
    fixture.load("oblivion.esm", {}, {shockEffect(), staffEnchantment(), staff()});
    const auto before = fixture.mStore.getFormKeyIndex().serialize();
    auto input = resolveStaff(fixture);
    ASSERT_TRUE(input);
    EXPECT_EQ(input->mEnchantment, ESM::FormKey::content("Oblivion.esm", 0x66c42));
    EXPECT_FLOAT_EQ(input->mAuthoredChargeCost, 13);
    EXPECT_FLOAT_EQ(input->mMaximumCharge, 150);
    EXPECT_FLOAT_EQ(input->mDamage.mMagnitude, 5);
    EXPECT_EQ(input->mDamage.mResistanceActorValue, 68);
    EXPECT_FLOAT_EQ(input->mDamage.mProjectileSpeedFactor, 2);
    EXPECT_EQ(input->mDamage.mModel, "MagicEffects\\shockbolt.NIF");
    EXPECT_EQ(fixture.mStore.getFormKeyIndex().serialize(), before);
    fixture.load("patch.esp", {"oblivion.esm"}, {staffEnchantment(7), shockEffect(1.5f)});
    input = resolveStaff(fixture);
    ASSERT_TRUE(input);
    EXPECT_FLOAT_EQ(input->mDamage.mMagnitude, 7);
    EXPECT_FLOAT_EQ(input->mDamage.mProjectileSpeedFactor, 1.5f);
    fixture.load("final.esp", {"oblivion.esm", "patch.esp"},
        {record(ESM4::REC_ENCH, 0x66c42, {}, true)});
    EXPECT_FALSE(resolveStaff(fixture));
}

TEST(OblivionCombatMagicStore, StaffTargetDamageRejectsTimedAmbiguousAndInvalidProjectiles)
{
    Fixture fixture;
    fixture.load("oblivion.esm", {}, {shockEffect(), staffEnchantment(5, 2), staff()});
    EXPECT_FALSE(resolveStaff(fixture)); // No partial immediate substitution for a timed effect.
    fixture.load("patch.esp", {"oblivion.esm"}, {staffEnchantment()});
    ASSERT_TRUE(resolveStaff(fixture));
    fixture.load("other.esm", {}, {shockEffect(2, 0x800)});
    EXPECT_FALSE(resolveStaff(fixture));

    for (float speed : {0.f, -1.f, std::numeric_limits<float>::infinity()})
    {
        Fixture invalid;
        invalid.load("oblivion.esm", {}, {shockEffect(speed), staffEnchantment(), staff()});
        EXPECT_FALSE(resolveStaff(invalid));
    }
    Fixture deleted;
    deleted.load("oblivion.esm", {}, {shockEffect(), staffEnchantment(), staff()});
    deleted.load("patch.esp", {"oblivion.esm"}, {record(ESM4::REC_MGEF, 0x189b, {}, true)});
    EXPECT_FALSE(resolveStaff(deleted));
}

namespace
{
    std::string fireDefinition()
    {
        return record(ESM4::REC_MGEF, 0x1872,
            sub(ESM::fourCC("EDID"), std::string("FIDG\0", 5))
            + sub(ESM::fourCC("DATA"), bytes(0x21001c75u) + bytes(7.5f)
                + bytes(8u) + bytes(2u) + bytes(std::int32_t(61)) + std::string(44, '\0')));
    }

    std::string fireSpell(unsigned magnitude = 6, unsigned duration = 2, unsigned range = 1, unsigned area = 0)
    {
        return record(ESM4::REC_SPEL, 0x689b0,
            sub(ESM::fourCC("SPIT"), bytes(0u) + bytes(14u) + bytes(0u) + bytes(0x40u))
            + sub(ESM::fourCC("EFID"), bytes(ESM::fourCC("FIDG")))
            + sub(ESM::fourCC("EFIT"), bytes(ESM::fourCC("FIDG")) + bytes(magnitude)
                + bytes(area) + bytes(duration) + bytes(range) + bytes(8u)));
    }
}

TEST(OblivionCombatMagicStore, EnvironmentalFireUsesWinningTimedTouchProfileWithoutPartialEffects)
{
    Fixture fixture;
    fixture.load("oblivion.esm", {}, {fireDefinition(), fireSpell()});
    const auto key = ESM::FormKey::content("oblivion.esm", 0x689b0);
    const auto resolve = [&] {
        const auto* spell = fixture.mStore.get<ESM4::Spell>().search(key);
        return spell ? MWWorld::resolveOblivionEnvironmentalFire(fixture.mStore, *spell)
            : std::optional<MWWorld::OblivionEnvironmentalFire>{};
    };
    ASSERT_TRUE(resolve());
    EXPECT_EQ(resolve()->mSpell, key);
    EXPECT_FLOAT_EQ(resolve()->mMagnitude, 6);
    EXPECT_FLOAT_EQ(resolve()->mDuration, 2);
    fixture.load("patch.esp", {"oblivion.esm"}, {fireSpell(9, 3)});
    ASSERT_TRUE(resolve());
    EXPECT_FLOAT_EQ(resolve()->mMagnitude, 9);
    EXPECT_FLOAT_EQ(resolve()->mDuration, 3);
    fixture.load("final.esp", {"oblivion.esm", "patch.esp"},
        {record(ESM4::REC_SPEL, 0x689b0, {}, true)});
    EXPECT_FALSE(resolve());

    for (const auto& input : {fireSpell(0), fireSpell(6, 0), fireSpell(6, 2, 0),
             fireSpell(6, 2, 2), fireSpell(6, 2, 1, 10)})
    {
        Fixture invalid;
        invalid.load("oblivion.esm", {}, {fireDefinition(), input});
        EXPECT_FALSE(MWWorld::resolveOblivionEnvironmentalFire(invalid.mStore,
            *invalid.mStore.get<ESM4::Spell>().search(key)));
    }
    Fixture ambiguous;
    ambiguous.load("oblivion.esm", {}, {fireDefinition(), fireSpell()});
    ambiguous.load("other.esm", {}, {fireDefinition()});
    EXPECT_FALSE(MWWorld::resolveOblivionEnvironmentalFire(ambiguous.mStore,
        *ambiguous.mStore.get<ESM4::Spell>().search(key)));
}

TEST(OblivionCombatMagicStore, EnvironmentalFireOwnershipRoundTripsAndRejectsMalformedTimersAtomically)
{
    ESM4::RuntimeReferenceState reference;
    reference.mCustomState["unrelated"] = std::int64_t(7);
    const MWWorld::OblivionEnvironmentalFire effect{
        ESM::FormKey::content("oblivion.esm", 0x689b0), 6, 2, .5f};
    EXPECT_FALSE(MWWorld::readOblivionEnvironmentalFire(reference));
    MWWorld::writeOblivionEnvironmentalFire(reference, effect);
    EXPECT_EQ(MWWorld::readOblivionEnvironmentalFire(reference), effect);
    const auto before = reference;
    for (const auto& key : {"native.fire.spell", "native.fire.duration", "native.fire.elapsed"})
    {
        auto partial = before;
        partial.mCustomState.erase(key);
        EXPECT_THROW(MWWorld::readOblivionEnvironmentalFire(partial), std::invalid_argument);
    }
    auto unknown = before;
    unknown.mCustomState["native.fire.unknown"] = 0.;
    EXPECT_THROW(MWWorld::readOblivionEnvironmentalFire(unknown), std::invalid_argument);
    for (float elapsed : {-1.f, 2.f, std::numeric_limits<float>::quiet_NaN()})
    {
        auto bad = effect; bad.mElapsed = elapsed;
        EXPECT_THROW(MWWorld::writeOblivionEnvironmentalFire(reference, bad), std::invalid_argument);
        EXPECT_EQ(reference, before);
        auto malformed = before; malformed.mCustomState["native.fire.elapsed"] = double(elapsed);
        EXPECT_THROW(MWWorld::readOblivionEnvironmentalFire(malformed), std::invalid_argument);
    }
    MWWorld::writeOblivionEnvironmentalFire(reference, std::nullopt);
    EXPECT_FALSE(MWWorld::readOblivionEnvironmentalFire(reference));
    EXPECT_EQ(reference.mCustomState.size(), 1u);
    EXPECT_EQ(std::get<std::int64_t>(reference.mCustomState.at("unrelated")), 7);
}

TEST(OblivionMagicContent, IngredientUseAdmitsOnlyFirstRestoreFatigueAndRechecksWinningContent)
{
    Fixture fixture;
    const auto definition = [&](std::uint32_t flags) {
        const auto data = bytes(flags) + bytes(2.f) + bytes(10u) + bytes(5u) + bytes(-1)
            + std::string(44, '\0');
        return record(ESM4::REC_MGEF, 0x188b, sub(ESM::fourCC("EDID"), std::string("REFA\0", 5))
            + sub(ESM::fourCC("DATA"), data));
    };
    const auto efit = bytes(ESM::fourCC("REFA")) + bytes(15u) + bytes(0u) + bytes(1u) + bytes(0u) + bytes(10u);
    fixture.load("oblivion.esm", {}, {definition(0x01000870), record(ESM4::REC_INGR, 0x33681,
        sub(ESM::fourCC("EFID"), bytes(ESM::fourCC("REFA"))) + sub(ESM::fourCC("EFIT"), efit))});
    const auto key = ESM::FormKey::content("oblivion.esm", 0x33681);
    const auto* original = fixture.mStore.search<ESM4::Ingredient>(key);
    ASSERT_NE(original, nullptr);
    EXPECT_TRUE(MWWorld::supportsOblivionIngredientRestoration(fixture.mStore, *original));
    for (int alteration = 0; alteration < 7; ++alteration)
    {
        auto changed = *original;
        if (alteration == 0) changed.mEffects.front().mId = ESM::fourCC("DGAT");
        if (alteration == 1) changed.mEffects.front().mRange = 1;
        if (alteration == 2) changed.mEffects.front().mActorValue = 8;
        if (alteration == 3) changed.mEffects.clear();
        if (alteration == 4) changed.mScriptId = ESM::FormId{0x900, 1};
        if (alteration == 5) changed.mEffects.front().mMagnitude = 30;
        if (alteration == 6) changed.mEffects.front().mDuration = 5;
        EXPECT_FALSE(MWWorld::supportsOblivionIngredientRestoration(fixture.mStore, changed));
    }
    ESM4::RuntimeState saved;
    ESM4::RuntimeActorValues actor;
    actor.mIngredientRestoration.push_back({key, .25f}); saved.mNativeActorValues.push_back(actor);
    EXPECT_NO_THROW(MWWorld::validateOblivionIngredientRestoration(fixture.mStore, saved));
    fixture.load("patch.esp", {"oblivion.esm"}, {definition(0x01000871)});
    EXPECT_FALSE(MWWorld::supportsOblivionIngredientRestoration(fixture.mStore, *original));
    EXPECT_THROW(MWWorld::validateOblivionIngredientRestoration(fixture.mStore, saved), std::invalid_argument);
}

namespace
{
    std::string flashBoltEnchantment(unsigned type = 0, unsigned duration = 0, unsigned magnitude = 20)
    {
        return record(ESM4::REC_ENCH, 0x5c057,
            sub(ESM::fourCC("ENIT"), bytes(type) + bytes(52u) + bytes(52u) + bytes(0u))
            + sub(ESM::fourCC("EFID"), bytes(ESM::fourCC("FIDG")))
            + sub(ESM::fourCC("EFIT"), bytes(ESM::fourCC("FIDG")) + bytes(magnitude)
                + bytes(0u) + bytes(duration) + bytes(2u) + bytes(8u)));
    }

    std::string flashBoltBook(unsigned flags = 1)
    {
        return record(ESM4::REC_BOOK, 0x888bf,
            sub(ESM::fourCC("DATA"), bytes(static_cast<std::uint8_t>(flags))
                + bytes(std::int8_t{-1}) + bytes(0u) + bytes(.1f))
            + sub(ESM::fourCC("ENAM"), bytes(0x5c057u)));
    }

    std::string flashBoltEffect()
    {
        const auto data = bytes(0x21001c75u) + bytes(7.5f) + bytes(8u) + bytes(2u) + bytes(61)
            + std::string(8, '\0') + bytes(1.f) + std::string(32, '\0');
        return record(ESM4::REC_MGEF, 0x1872,
            sub(ESM::fourCC("EDID"), std::string("FIDG\0", 5))
            + sub(ESM::fourCC("MODL"), std::string("MagicEffects\\fireball.NIF") + '\0')
            + sub(ESM::fourCC("DATA"), data));
    }

    auto resolveFlashBolt(const Fixture& fixture)
    {
        const auto* book = fixture.mStore.get<ESM4::Book>().search(ESM::FormId{0x888bf, 1});
        if (!book) throw std::runtime_error("missing fixture scroll");
        return MWWorld::resolveOblivionScrollDamage(fixture.mStore, *book);
    }
}

TEST(OblivionCombatMagicStore, FlashBoltScrollRequiresCompleteWinningNativeItemAndEffect)
{
    Fixture fixture;
    fixture.load("oblivion.esm", {}, {flashBoltBook(), flashBoltEnchantment(), flashBoltEffect()});
    ASSERT_TRUE(resolveFlashBolt(fixture));
    EXPECT_FLOAT_EQ(resolveFlashBolt(fixture)->mMagnitude, 20.f);
    fixture.load("patch.esp", {"oblivion.esm"}, {flashBoltEnchantment(1)});
    EXPECT_FALSE(resolveFlashBolt(fixture)); // Staff ENCH cannot masquerade as a scroll.
    fixture.load("final.esp", {"oblivion.esm", "patch.esp"}, {flashBoltEnchantment()});
    EXPECT_TRUE(resolveFlashBolt(fixture));
}

TEST(OblivionCombatMagicStore, FlashBoltScrollRejectsUnsupportedShapesAndDeletedSources)
{
    for (const auto& records : std::vector<std::vector<std::string>>{
        {flashBoltBook(0), flashBoltEnchantment(), flashBoltEffect()},
        {flashBoltBook(), flashBoltEnchantment(0, 1), flashBoltEffect()},
        {flashBoltBook(), flashBoltEnchantment(0, 0, 10), flashBoltEffect()},
        {flashBoltBook(), flashBoltEnchantment()}})
    {
        Fixture fixture;
        fixture.load("oblivion.esm", {}, records);
        EXPECT_FALSE(resolveFlashBolt(fixture));
    }
    Fixture fixture;
    fixture.load("oblivion.esm", {}, {flashBoltBook(), flashBoltEnchantment(), flashBoltEffect()});
    fixture.load("patch.esp", {"oblivion.esm"}, {record(ESM4::REC_ENCH, 0x5c057, {}, true)});
    EXPECT_FALSE(resolveFlashBolt(fixture));
}

namespace
{
    std::string chameleonBook()
    {
        return record(ESM4::REC_BOOK, 0x84aeb,
            sub(ESM::fourCC("DATA"), bytes(std::uint8_t{1}) + bytes(std::int8_t{-1}) + bytes(0u) + bytes(.1f))
            + sub(ESM::fourCC("ENAM"), bytes(0x5b044u)));
    }
    std::string chameleonEnchantment(unsigned duration = 30, unsigned range = 0)
    {
        return record(ESM4::REC_ENCH, 0x5b044,
            sub(ESM::fourCC("ENIT"), bytes(0u) + bytes(116u) + bytes(116u) + bytes(0u))
            + sub(ESM::fourCC("EFID"), bytes(ESM::fourCC("CHML")))
            + sub(ESM::fourCC("EFIT"), bytes(ESM::fourCC("CHML")) + bytes(25u)
                + bytes(0u) + bytes(duration) + bytes(range) + bytes(46u)));
    }
    std::string chameleonDefinition(unsigned flags = 0x0100187a)
    {
        return record(ESM4::REC_MGEF, 0x185c,
            sub(ESM::fourCC("EDID"), std::string("CHML\0", 5))
            + sub(ESM::fourCC("DATA"), bytes(flags) + bytes(.63f) + bytes(46u)
                + bytes(3u) + bytes(std::int32_t{-1}) + std::string(44, '\0')));
    }
    auto resolveChameleon(const Fixture& fixture)
    {
        const auto* book = fixture.mStore.get<ESM4::Book>().search(ESM::FormId{0x84aeb, 1});
        if (!book) throw std::runtime_error("missing fixture Chameleon scroll");
        return MWWorld::resolveOblivionScrollChameleon(fixture.mStore, *book);
    }
}

TEST(OblivionCombatMagicStore, OriginalChameleonRequiresCompleteRecoverableSelfProfile)
{
    Fixture fixture;
    fixture.load("oblivion.esm", {}, {chameleonBook(), chameleonEnchantment()});
    EXPECT_FALSE(resolveChameleon(fixture));
    fixture.load("patch.esp", {"oblivion.esm"}, {chameleonDefinition()});
    ASSERT_TRUE(resolveChameleon(fixture));
    EXPECT_EQ(*resolveChameleon(fixture), ESM::FormKey::content("Oblivion.esm", 0x5b044));
    fixture.load("final.esp", {"oblivion.esm", "patch.esp"}, {chameleonEnchantment(31)});
    EXPECT_FALSE(resolveChameleon(fixture));
}

TEST(OblivionCombatMagicStore, ChameleonDoesNotSubstituteTargetDamagingOrDeletedEffects)
{
    for (const auto& records : std::vector<std::vector<std::string>>{
        {chameleonBook(), chameleonEnchantment(30, 2), chameleonDefinition()},
        {chameleonBook(), chameleonEnchantment(), chameleonDefinition(0x01001878)},
        {chameleonBook(), chameleonEnchantment(), chameleonDefinition(0x0100197a)}})
    {
        Fixture fixture;
        fixture.load("oblivion.esm", {}, records);
        EXPECT_FALSE(resolveChameleon(fixture));
    }
    Fixture fixture;
    fixture.load("oblivion.esm", {}, {chameleonBook(), chameleonEnchantment(), chameleonDefinition()});
    fixture.load("patch.esp", {"oblivion.esm"}, {record(ESM4::REC_ENCH, 0x5b044, {}, true)});
    EXPECT_FALSE(resolveChameleon(fixture));
}

namespace
{
    std::string meadPotion(unsigned duration = 60, unsigned attribute = 2, unsigned flags = 0xffffff03)
    {
        auto body = sub(ESM::fourCC("DATA"), bytes(1.f))
            + sub(ESM::fourCC("ENIT"), bytes(2) + bytes(flags));
        for (unsigned av : {1u, attribute})
            body += sub(ESM::fourCC("EFID"), bytes(ESM::fourCC("DRAT")))
                + sub(ESM::fourCC("EFIT"), bytes(ESM::fourCC("DRAT")) + bytes(1u)
                    + bytes(0u) + bytes(duration) + bytes(0u) + bytes(av));
        body += sub(ESM::fourCC("EFID"), bytes(ESM::fourCC("REFA")))
            + sub(ESM::fourCC("EFIT"), bytes(ESM::fourCC("REFA")) + bytes(2u)
                + bytes(0u) + bytes(0u) + bytes(0u) + bytes(10u));
        return record(ESM4::REC_ALCH, 0xb1201, body);
    }

    std::string drainAttributeDefinition(unsigned flags = 0x00101c77)
    {
        return record(ESM4::REC_MGEF, 0x186b,
            sub(ESM::fourCC("EDID"), std::string("DRAT\0", 5))
            + sub(ESM::fourCC("DATA"), bytes(flags) + bytes(.7f) + bytes(0u)
                + bytes(2u) + bytes(std::int32_t{64}) + std::string(44, '\0')));
    }

    std::string fatigueRestorationDefinition()
    {
        return record(ESM4::REC_MGEF, 0x188b,
            sub(ESM::fourCC("EDID"), std::string("REFA\0", 5))
            + sub(ESM::fourCC("DATA"), bytes(0x01000870u) + bytes(2.f) + bytes(10u)
                + bytes(5u) + bytes(std::int32_t{-1}) + std::string(44, '\0')));
    }

    auto resolveMead(const Fixture& fixture)
    {
        const auto* potion = fixture.mStore.get<ESM4::Potion>().search(ESM::FormId{0xb1201, 1});
        if (!potion) throw std::runtime_error("missing fixture Mead");
        return MWWorld::resolveOblivionPotionAttributes(fixture.mStore, *potion);
    }
}

TEST(OblivionCombatMagicStore, OriginalMeadRequiresAllThreeEffectsAndTheirWinningDefinitions)
{
    Fixture fixture;
    fixture.load("oblivion.esm", {}, {meadPotion()});
    EXPECT_FALSE(resolveMead(fixture));
    fixture.load("patch.esp", {"oblivion.esm"}, {drainAttributeDefinition(), fatigueRestorationDefinition()});
    const auto resolved = resolveMead(fixture);
    ASSERT_TRUE(resolved);
    EXPECT_EQ(resolved->mPotion, ESM::FormKey::content("Oblivion.esm", 0xb1201));
    EXPECT_FLOAT_EQ(resolved->mDuration, 60.f);
    EXPECT_EQ(resolved->mModifiers, (std::vector<ESM4::PotionAttributeModifier>{{1, -1.f}, {2, -1.f}}));
    const auto* potion = fixture.mStore.get<ESM4::Potion>().search(ESM::FormId{0xb1201, 1});
    ASSERT_NE(potion, nullptr);
    EXPECT_FALSE(MWWorld::resolveOblivionPotionRestoration(fixture.mStore, *potion));
    const auto previous = *potion;
    fixture.load("final.esp", {"oblivion.esm", "patch.esp"}, {record(ESM4::REC_ALCH, 0xb1201, {}, true)});
    EXPECT_EQ(fixture.mStore.get<ESM4::Potion>().search(ESM::FormId{0xb1201, 1}), nullptr);
    EXPECT_FALSE(MWWorld::resolveOblivionPotionAttributes(fixture.mStore, previous));
}

TEST(OblivionCombatMagicStore, MeadRejectsChangedAttributesLifetimeFoodFlagsAndRecoverySemantics)
{
    for (const auto& records : std::vector<std::vector<std::string>>{
        {meadPotion(61), drainAttributeDefinition(), fatigueRestorationDefinition()},
        {meadPotion(60, 3), drainAttributeDefinition(), fatigueRestorationDefinition()},
        {meadPotion(60, 2, 1), drainAttributeDefinition(), fatigueRestorationDefinition()},
        {meadPotion(), drainAttributeDefinition(0x00101c75), fatigueRestorationDefinition()}})
    {
        Fixture fixture;
        fixture.load("oblivion.esm", {}, records);
        EXPECT_FALSE(resolveMead(fixture));
    }
}

namespace
{
    std::string welkynd(std::uint32_t id = 0x191, bool scripted = false, std::string name = "WelkyndStone")
    {
        auto data = sub(ESM::fourCC("EDID"), name + '\0')
            + sub(ESM::fourCC("DATA"), bytes(50u) + bytes(1.f));
        if (scripted) data += sub(ESM::fourCC("SCRI"), bytes(0x123u));
        return record(ESM4::REC_MISC, id, data);
    }

    bool supportedStone(const Fixture& fixture, ESM::FormId id = {0x191, 1})
    {
        const auto* item = fixture.mStore.get<ESM4::MiscItem>().search(id);
        return item && MWWorld::isOblivionWelkyndStone(fixture.mStore, *item);
    }
}

TEST(OblivionMiscConsumableStore, OriginalBuiltinRoleUsesStableContentIdentityAndWinningMetadata)
{
    Fixture fixture;
    fixture.load("oblivion.esm", {}, {welkynd()});
    ASSERT_TRUE(supportedStone(fixture));
    // Native special-item role survives ordinary presentation overrides.
    fixture.load("patch.esp", {"oblivion.esm"}, {welkynd(0x191, false, "ChangedDisplayEditorId")});
    EXPECT_TRUE(supportedStone(fixture));
}

TEST(OblivionMiscConsumableStore, DisplayNameAndLocalIdDoNotGrantAnotherContentFileTheBuiltinRole)
{
    Fixture fixture;
    fixture.load("oblivion.esm", {}, {welkynd(0x192)});
    EXPECT_FALSE(supportedStone(fixture, {0x192, 1}));
    fixture.load("other.esm", {}, {welkynd()});
    EXPECT_FALSE(supportedStone(fixture, {0x191, 3}));
}

TEST(OblivionMiscConsumableStore, ScriptedProfileAndDeletedWinningSourceDoNotPartiallyUse)
{
    Fixture fixture;
    fixture.load("oblivion.esm", {}, {welkynd(0x191, true)});
    EXPECT_FALSE(supportedStone(fixture));
    fixture.load("patch.esp", {"oblivion.esm"}, {welkynd()});
    ASSERT_TRUE(supportedStone(fixture));
    const auto previous = *fixture.mStore.get<ESM4::MiscItem>().search(ESM::FormId{0x191, 1});
    fixture.load("final.esp", {"oblivion.esm", "patch.esp"}, {record(ESM4::REC_MISC, 0x191, {}, true)});
    EXPECT_FALSE(supportedStone(fixture));
    EXPECT_FALSE(MWWorld::isOblivionWelkyndStone(fixture.mStore, previous));
}

namespace
{
    std::string starOfWest(std::uint32_t id = 0x47ade, std::uint32_t magnitude = 100,
        std::uint32_t type = 2)
    {
        const auto spit = bytes(type) + bytes(0u) + bytes(4u) + bytes(0x0b2fbd01u);
        const auto efit = bytes(ESM::fourCC("ABFA")) + bytes(magnitude) + bytes(0u)
            + bytes(0u) + bytes(1u) + bytes(10u);
        return record(ESM4::REC_SPEL, id, sub(ESM::fourCC("SPIT"), spit)
            + sub(ESM::fourCC("EFID"), bytes(ESM::fourCC("ABFA"))) + sub(ESM::fourCC("EFIT"), efit));
    }

    std::string absorbFatigueEffect()
    {
        const auto data = bytes(0x01003825u) + bytes(6.f) + bytes(10u) + bytes(5u) + bytes(64)
            + std::string(44, '\0');
        return record(ESM4::REC_MGEF, 0x8ca, sub(ESM::fourCC("EDID"), std::string("ABFA\0", 5))
            + sub(ESM::fourCC("DATA"), data));
    }

    const ESM4::Spell& loadedStar(const Fixture& fixture)
    {
        return *fixture.mStore.get<ESM4::Spell>().search(ESM::FormId{0x47ade, 1});
    }
}

TEST(OblivionCombatMagicStore, OriginalGreaterPowerHasZeroCostAndDoesNotRequireMasterRestoration)
{
    Fixture fixture;
    fixture.load("oblivion.esm", {}, {starOfWest(), absorbFatigueEffect()});
    ASSERT_TRUE(MWWorld::supportsOblivionStarOfWest(fixture.mStore, loadedStar(fixture)));
    const auto description = MWWorld::describeOblivionSpell(fixture.mStore, loadedStar(fixture), {}, 0);
    ASSERT_TRUE(description.mCost);
    EXPECT_FLOAT_EQ(*description.mCost, 0.f);
    // The original upper flag bytes contain padding; only the defined byte is semantic.
    auto copied = loadedStar(fixture);
    copied.mData->mFlags = 1;
    EXPECT_TRUE(MWWorld::supportsOblivionStarOfWest(fixture.mStore, copied));
    copied.mData->mType = 0;
    EXPECT_FALSE(MWWorld::supportsOblivionStarOfWest(fixture.mStore, copied));
    copied = loadedStar(fixture);
    copied.mFormKey = ESM::FormKey::content("other.esm", 0x47ade);
    EXPECT_FALSE(MWWorld::supportsOblivionStarOfWest(fixture.mStore, copied));
}

TEST(OblivionCombatMagicStore, GreaterPowerRejectsChangedOrDeletedWinningProfiles)
{
    Fixture fixture;
    fixture.load("oblivion.esm", {}, {starOfWest(), absorbFatigueEffect()});
    fixture.load("patch.esp", {"oblivion.esm"}, {starOfWest(0x47ade, 101)});
    EXPECT_FALSE(MWWorld::supportsOblivionStarOfWest(fixture.mStore, loadedStar(fixture)));
    fixture.load("final.esp", {"oblivion.esm", "patch.esp"}, {starOfWest(),
        record(ESM4::REC_MGEF, 0x8ca, {}, true)});
    EXPECT_FALSE(MWWorld::supportsOblivionStarOfWest(fixture.mStore, loadedStar(fixture)));
}

TEST(OblivionMagicContent, VoiceOfEmperorRequiresCompleteWinningOriginalPowerAndEffect)
{
    const auto makeSpell = [](std::uint32_t magnitude, std::uint32_t duration) {
        const auto efit = bytes(ESM::fourCC("CHRM")) + bytes(magnitude) + bytes(0u)
            + bytes(duration) + bytes(2u) + bytes(6u);
        return record(ESM4::REC_SPEL, 0x47add,
            sub(ESM::fourCC("SPIT"), bytes(2u) + bytes(0u) + bytes(1u) + bytes(1u))
            + sub(ESM::fourCC("EFID"), bytes(ESM::fourCC("CHRM"))) + sub(ESM::fourCC("EFIT"), efit));
    };
    const auto definition = [](std::uint32_t flags) {
        const auto data = bytes(flags) + bytes(.2f) + bytes(6u) + bytes(3u) + bytes(64)
            + bytes(1u) + bytes(0u) + bytes(1.f) + std::string(32, '\0');
        return record(ESM4::REC_MGEF, 0x5e1d6,
            sub(ESM::fourCC("EDID"), std::string("CHRM\0", 5)) + sub(ESM::fourCC("DATA"), data)
            + sub(ESM::fourCC("MODL"), std::string("MagicEffects\\illusion.NIF\0", 26)));
    };
    const auto key = ESM::FormKey::content("oblivion.esm", 0x47add);
    Fixture fixture;
    fixture.load("oblivion.esm", {}, {definition(0x01000c62), makeSpell(30, 30)});
    const auto* power = fixture.mStore.get<ESM4::Spell>().search(key);
    ASSERT_NE(power, nullptr);
    const auto resolved = MWWorld::resolveOblivionVoiceOfEmperor(fixture.mStore, *power);
    ASSERT_TRUE(resolved);
    EXPECT_EQ(resolved->mEffectCode, ESM::fourCC("CHRM"));
    EXPECT_EQ(resolved->mMagnitude, 30);
    EXPECT_EQ(resolved->mProjectileSpeedFactor, 1);
    EXPECT_EQ(resolved->mModel, "MagicEffects\\illusion.NIF");
    EXPECT_EQ(MWWorld::describeOblivionSpell(fixture.mStore, *power, {}, 50).mCost, 0.f);
    fixture.load("patch.esp", {"oblivion.esm"}, {makeSpell(30, 31)});
    EXPECT_FALSE(MWWorld::resolveOblivionVoiceOfEmperor(fixture.mStore,
        *fixture.mStore.get<ESM4::Spell>().search(key)));
    Fixture wrongDefinition;
    wrongDefinition.load("oblivion.esm", {}, {definition(0x01000c63), makeSpell(30, 30)});
    EXPECT_FALSE(MWWorld::resolveOblivionVoiceOfEmperor(wrongDefinition.mStore,
        *wrongDefinition.mStore.get<ESM4::Spell>().search(key)));
}

TEST(OblivionMagicContent, CharmBelongsOnlyToRecipientAndCasterAndRejectsPartialOrExpiredOwnership)
{
    ESM4::RuntimeState state;
    ESM4::RuntimeReferenceState reference;
    reference.mKey = ESM::FormKey::content("oblivion.esm", 0x4bb63);
    reference.mCustomState["unrelated"] = std::int64_t(7);
    const MWWorld::OblivionCharm effect{ESM::FormKey::content("oblivion.esm", 0x47add),
        ESM::FormKey::dynamic("player", 1), 30, 10};
    MWWorld::writeOblivionCharm(reference, effect);
    EXPECT_EQ(MWWorld::readOblivionCharm(reference), effect);
    state.mReferences.push_back(reference);
    EXPECT_EQ(MWWorld::oblivionCharmModifier(state, reference.mKey, effect.mCaster), 30);
    EXPECT_EQ(MWWorld::oblivionCharmModifier(state, reference.mKey, reference.mKey), 0);
    EXPECT_EQ(MWWorld::oblivionCharmModifier(state, effect.mCaster, reference.mKey), 0);
    for (const auto* key : {"native.charm.spell", "native.charm.caster", "native.charm.magnitude", "native.charm.elapsed"})
    {
        auto partial = reference;
        partial.mCustomState.erase(key);
        EXPECT_THROW(MWWorld::readOblivionCharm(partial), std::invalid_argument);
    }
    auto unknown = reference;
    unknown.mCustomState["native.charm.unknown"] = 0.;
    EXPECT_THROW(MWWorld::readOblivionCharm(unknown), std::invalid_argument);
    for (float elapsed : {-1.f, 30.f, std::numeric_limits<float>::quiet_NaN()})
    {
        auto invalid = effect;
        invalid.mElapsed = elapsed;
        const auto before = reference;
        EXPECT_THROW(MWWorld::writeOblivionCharm(reference, invalid), std::invalid_argument);
        EXPECT_EQ(reference, before);
    }
    MWWorld::writeOblivionCharm(reference, std::nullopt);
    EXPECT_FALSE(MWWorld::readOblivionCharm(reference));
    EXPECT_EQ(reference.mCustomState.size(), 1u);
}
