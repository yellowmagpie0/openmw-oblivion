#ifndef OPENMW_ESM4_PHYSICALBLENDSETTINGS_H
#define OPENMW_ESM4_PHYSICALBLENDSETTINGS_H

#include <array>
#include <cstddef>
#include <cstdint>

namespace ESM4
{
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

    // The body ID is bits8..12 of the packed native world-object filter.
    // Negative results mean native transition setup skips that body.
    float physicalBlendDurationForFilter(
        const PhysicalBlendDurationTables& tables, std::uint32_t filter, bool getUp);
}

#endif
