#ifndef OPENMW_COMPONENTS_ESM4_FORMIDFIELDS_H
#define OPENMW_COMPONENTS_ESM4_FORMIDFIELDS_H

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace ESM4
{
    // Bit 0 denotes a FormID in CTDA/CTDT parameter 1 and bit 1 denotes a
    // FormID in parameter 2.  A zero result means that the parameter is a
    // numeric/enum value (or that the function is not in the TES4 table).
    std::uint8_t conditionParameterFormIdMask(std::uint32_t function);

    // Return byte offsets of TES4 FormIDs embedded in a subrecord payload.
    // This covers lossless official-Oblivion records, packed conditions, and
    // FormID-bearing fields deliberately skipped by typed loaders.
    std::vector<std::size_t> findFormIdOffsets(
        std::uint32_t recordType, std::uint32_t subRecordType, std::span<const std::uint8_t> data);

    // Conservative fast check used to avoid buffering ordinary skipped data.
    bool mayContainFormIds(std::uint32_t recordType, std::uint32_t subRecordType);
}

#endif
