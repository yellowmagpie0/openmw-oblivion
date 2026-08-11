#ifndef OPENMW_COMPONENTS_OBSCRIPT_SCDA_H
#define OPENMW_COMPONENTS_OBSCRIPT_SCDA_H

#include "ast.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace ObScript
{
    struct DecodedBytecodeInstruction
    {
        std::size_t mOffset = 0;
        std::uint16_t mOpcode = 0;
        std::optional<std::uint16_t> mCallingReference;
        std::vector<std::uint8_t> mPayload;

        friend bool operator==(const DecodedBytecodeInstruction&, const DecodedBytecodeInstruction&) = default;
    };

    struct DecodedAtomCounts
    {
        std::size_t mIntegerLiterals = 0;
        std::size_t mFloatLiterals = 0;
        std::size_t mLocalReferences = 0;
        std::size_t mFormReferences = 0;
    };

    struct StructuralComparison
    {
        std::uint16_t mOpcode = 0;
        std::size_t mSourceCount = 0;
        std::size_t mBytecodeCount = 0;

        bool matches() const { return mSourceCount == mBytecodeCount; }
    };

    struct BytecodeComparison
    {
        bool mDecoded = false;
        bool mHeaderSizeMatches = false;
        std::vector<DecodedBytecodeInstruction> mInstructions;
        DecodedAtomCounts mAtoms;
        std::vector<StructuralComparison> mStructure;

        bool structureMatches() const;
    };

    class BytecodeDecoder
    {
    public:
        std::vector<DecodedBytecodeInstruction> decode(std::span<const std::uint8_t> data) const;
        DecodedAtomCounts decodeAtoms(const std::vector<DecodedBytecodeInstruction>& instructions) const;
        BytecodeComparison compare(
            const Script& source, std::span<const std::uint8_t> data, std::optional<std::uint32_t> headerSize) const;
    };
}

#endif
