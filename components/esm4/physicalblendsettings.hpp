#ifndef OPENMW_ESM4_PHYSICALBLENDSETTINGS_H
#define OPENMW_ESM4_PHYSICALBLENDSETTINGS_H

#include "physicalcombat.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace ESM4
{
    // Original88EB60 constructor values, before authored load and link.
    struct PhysicalBlendCollisionState
    {
        std::uint16_t mFlags = 0x41;
        PhysicalBlendGains mGains{0.f, 1.f};
        std::uint32_t mRequestedMotion = 8;
    };

    using PhysicalBlendGainTable = std::array<PhysicalBlendGains, 32>;
    // Initial PE data only. The caller supplies the resolved runtime table.
    inline constexpr PhysicalBlendGainTable InitialPhysicalBlendGainTable = [] {
        PhysicalBlendGainTable result{};
        for (auto& gains : result)
            gains = {1.f, 1.f};
        return result;
    }();

    // HIT configuration is a different table from the all-one post-link
    // table above. Settings are already parsed by the configuration adapter.
    struct PhysicalHitBlendSettings
    {
        PhysicalBlendGains mHead{.4f, .6f};
        PhysicalBlendGains mBody{1.f, 1.f};
        PhysicalBlendGains mSpine1{.6f, .8f};
        PhysicalBlendGains mSpine2{.5f, .7f};
        PhysicalBlendGains mLUpperArm{.2f, .5f};
        PhysicalBlendGains mLForeArm{1.f, 1.f};
        PhysicalBlendGains mLHand{1.f, 1.f};
        PhysicalBlendGains mRUpperArm{.2f, .5f};
        PhysicalBlendGains mRForeArm{1.f, 1.f};
        PhysicalBlendGains mRHand{1.f, 1.f};
        float mMinimumHierarchy = .3f;
        float mMinimumVelocity = .95f;
    };

    // Original PE data before the resolved settings producer53A1B0 runs.
    inline constexpr PhysicalBlendGainTable InitialPhysicalHitBlendGainTable = [] {
        PhysicalBlendGainTable result{};
        for (auto& gain : result)
            gain = {.2f, .9f};
        result[1] = {.3f, .9f};
        for (std::size_t i : {2u, 3u, 4u})
            result[i] = {.2f, .8f};
        for (std::size_t i : {5u, 6u, 7u, 11u, 12u, 13u})
            result[i] = {.4f, .9f};
        result[22] = {1.f, 1.f};
        return result;
    }();

    struct PhysicalHitBlendConfiguration
    {
        PhysicalBlendGainTable mGains;
        float mMinimumHierarchy;
        float mMinimumVelocity;
    };

    // Preserve unconfigured body IDs, including ID22. Finite raw gains and
    // minima are stored without clamping; this does not parse/discover INI files.
    PhysicalHitBlendConfiguration resolvePhysicalHitBlendConfiguration(
        const PhysicalBlendGainTable& previous, const PhysicalHitBlendSettings& settings);

    struct PhysicalBlendFloatSettingResult
    {
        float mValue;
        bool mAccepted;
    };

    // Original4A8800 float reader, after the OS supplies processed profile text.
    // Missing text formats the previous value to six decimal places. Empty or
    // malformed text preserves it; accepted finite values retain native decimal
    // rounding and signed zero. Nonfinite results throw before publication.
    // The 256-byte native profile buffer limits text to255 bytes. File discovery,
    // Windows INI preprocessing and collection open/close remain adapter-owned.
    PhysicalBlendFloatSettingResult readPhysicalBlendFloatSetting(
        float previous, std::optional<std::string_view> processedProfileValue);

    // Full88ECD0 link gain/flag result, after caller resolves body ownership.
    // Ignore authored gains; missing wrapper/body selects entry0. Preserve the
    // existing requested-motion field and validate only the selected gains.
    // Scene flags, deferred links/refcounts and physical bodies are caller-owned.
    PhysicalBlendCollisionState resolvePhysicalBlendCollisionAfterLink(
        PhysicalBlendCollisionState loaded, bool hasResolvedBody, std::uint32_t packedFilter,
        const PhysicalBlendGainTable& resolvedGains);

    // Original BlendSettings.ini HIT values, not TES4 GMSTs. Configuration
    // import supplies explicit finite overrides to the table resolver.
    struct PhysicalBlendDurationSettings
    {
        float mGetUpTime = 1.f;
        float mKnockdownTime = 0.25f;
    };

    struct PhysicalBlendDurationTables
    {
        std::array<float, 32> mGetUp;
        std::array<float, 32> mKnockdown;
    };

    inline constexpr PhysicalBlendDurationTables InitialPhysicalBlendDurationTables = [] {
        PhysicalBlendDurationTables result{};
        for (std::size_t i = 0; i < 32; ++i)
        {
            result.mGetUp[i] = i < 25 ? 1.f : -1.f;
            result.mKnockdown[i] = i < 25 ? 0.25f : -1.f;
        }
        return result;
    }();

    // Original53A40C applies configured values only to currently nonnegative
    // entries. Negative per-bone entries remain disabled on later updates too.
    // Zero is a supported key interval, not a replacement duration.
    PhysicalBlendDurationTables resolvePhysicalBlendDurationTables(
        const PhysicalBlendDurationTables& previous, const PhysicalBlendDurationSettings& settings);

    struct PhysicalKnockdownBlend
    {
        std::array<PhysicalBlendKey, 2> mKeys;
        float mStartKey;
        float mStopKey;
        std::uint16_t mControllerFlags;
        PhysicalBlendClock mClock;
    };

    // Original8AB440 selected-controller setup. The caller resolves the node,
    // controller and body filter and handles missing/disabled/immediate paths.
    // Retain both keys at zero duration and reset Start's time sentinels without
    // clearing stored elapsed time. No attachment, velocity or body mutation.
    PhysicalKnockdownBlend preparePhysicalKnockdownBlend(PhysicalBlendGains current,
        float duration, float startKey, std::uint16_t controllerFlags, PhysicalBlendClock previousClock);

    struct PhysicalBlendKeyBounds
    {
        float mStartKey, mStopKey;
    };

    // Original8AABE0 updates inherited bounds using the first/last stored keys.
    // Load invokes it after each insertion; transition setup may invoke it once
    // after creating every key. Preserve that distinction and raw signed zeros.
    // Empty keys reset both bounds to +0; unused gains/middle times are ignored.
    // Reversed finite bounds are raw native state, not clock admission.
    PhysicalBlendKeyBounds resolvePhysicalBlendKeyBounds(
        PhysicalBlendKeyBounds previous, std::span<const PhysicalBlendKey> keys);

    struct PhysicalBlendControllerState
    {
        PhysicalBlendTiming mTiming{0, 1.f, 0.f, 0.f, 0.f};
        std::vector<PhysicalBlendKey> mKeys;
        PhysicalBlendClock mClock;
        std::uint32_t mCursor = 0;
        PhysicalBlendGains mCachedGains{-1.f, -1.f};
        // Original controller+0x60: reset0, selected knockdown setup2.
        std::uint32_t mSetupState = 0;
        friend bool operator==(const PhysicalBlendControllerState&, const PhysicalBlendControllerState&) = default;
    };

    struct PhysicalBlendControllerUpdate
    {
        PhysicalBlendControllerState mController;
        PhysicalBlendTimeCache mTimeCache;
        std::optional<PhysicalBlendGains> mTargetGains;
        bool mRemoveVelocityController;
    };

    // Full8AAD60 state transition, with target/velocity-controller identity
    // resolved by the runtime owner. Returns an atomic publication candidate;
    // actual node writes and velocity-controller removal remain with the owner.
    // The blend controller remains attached after completion.
    PhysicalBlendControllerUpdate advancePhysicalBlendController(const PhysicalBlendControllerState& controller,
        const PhysicalBlendTimeCache& timeCache, bool hasTarget, std::optional<PhysicalBlendGains> targetGains,
        bool hasVelocityController, float inputTime);

    // The body ID is bits8..12 of the packed native world-object filter.
    // Negative results mean native transition setup skips that body.
    float physicalBlendDurationForFilter(
        const PhysicalBlendDurationTables& tables, std::uint32_t filter, bool getUp);
}

#endif
