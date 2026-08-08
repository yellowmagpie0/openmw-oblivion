#ifndef OPENMW_COMPONENTS_ESM4_FORMIDFIELDS_H
#define OPENMW_COMPONENTS_ESM4_FORMIDFIELDS_H

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace ESM4
{
    // Return byte offsets of TES4 FormIDs embedded in a subrecord payload.
    // This covers lossless official-Oblivion records, packed conditions, and
    // FormID-bearing fields deliberately skipped by typed loaders.
    std::vector<std::size_t> findFormIdOffsets(
        std::uint32_t recordType, std::uint32_t subRecordType, std::span<const std::uint8_t> data);

    // Conservative fast check used to avoid buffering ordinary skipped data.
    bool mayContainFormIds(std::uint32_t recordType, std::uint32_t subRecordType);
}

#endif
