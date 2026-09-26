#ifndef OPENMW_ESM4_LOADSKIL_H
#define OPENMW_ESM4_LOADSKIL_H

#include "loadrawrecord.hpp"

#include <components/esm/defs.hpp>

#include <array>
#include <optional>
#include <span>

namespace ESM4
{
    struct SkillData
    {
        std::uint32_t mActorValue;
        std::uint32_t mGoverningAttribute;
        std::uint32_t mSpecialization;
        std::array<float, 2> mUseValues;
    };

    SkillData decodeSkillData(std::span<const std::uint8_t> bytes);

    // TES4 only. Keep descriptions, mastery text and unknown fields losslessly.
    struct Skill : RawRecord
    {
        static constexpr ESM::RecNameInts sRecordId = ESM::REC_SKIL4;
        std::optional<std::uint32_t> mIndex;
        std::optional<SkillData> mData;

        void load(Reader& reader);
    };

    // Resolve the complete winning inventory by native AV, never FormId or
    // editor name. Missing/duplicate definitions must not use TES3 defaults.
    std::array<const Skill*, 21> resolveSkillDefinitions(std::span<const Skill* const> skills);
}

#endif
