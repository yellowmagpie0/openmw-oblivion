#include "scda.hpp"

#include <array>
#include <cstring>
#include <stdexcept>

namespace ObScript
{
    namespace
    {
        std::uint16_t read16(std::span<const std::uint8_t> data, std::size_t offset)
        {
            if (offset + 2 > data.size())
                throw std::runtime_error("Truncated SCDA 16-bit field at offset " + std::to_string(offset));
            return static_cast<std::uint16_t>(data[offset] | (static_cast<std::uint16_t>(data[offset + 1]) << 8));
        }

        void countStatements(const std::vector<Statement>& statements, std::map<std::uint16_t, std::size_t>& result)
        {
            for (const Statement& statement : statements)
            {
                switch (statement.mKind)
                {
                    case StatementKind::Set:
                        ++result[0x15];
                        break;
                    case StatementKind::If:
                        ++result[0x16];
                        ++result[0x19];
                        for (std::size_t i = 1; i < statement.mClauses.size(); ++i)
                            ++result[statement.mClauses[i].mCondition ? 0x18 : 0x17];
                        for (const Clause& clause : statement.mClauses)
                            countStatements(clause.mBody, result);
                        break;
                    case StatementKind::Return:
                        ++result[0x1e];
                        break;
                    default:
                        break;
                }
            }
        }

        std::map<std::uint16_t, std::size_t> sourceStructure(const Script& source)
        {
            std::map<std::uint16_t, std::size_t> result;
            if (source.mName)
                ++result[0x1d];
            result[0x10] = source.mBlocks.size();
            result[0x11] = source.mBlocks.size();
            for (const EventBlock& block : source.mBlocks)
                countStatements(block.mBody, result);
            countStatements(source.mStray, result);
            return result;
        }
    }

    bool BytecodeComparison::structureMatches() const
    {
        for (const StructuralComparison& value : mStructure)
        {
            if (!value.matches())
                return false;
        }
        return true;
    }

    std::vector<DecodedBytecodeInstruction> BytecodeDecoder::decode(std::span<const std::uint8_t> data) const
    {
        std::vector<DecodedBytecodeInstruction> result;
        std::size_t offset = 0;
        while (offset < data.size())
        {
            const std::size_t instructionOffset = offset;
            const std::uint16_t opcode = read16(data, offset);
            const std::uint16_t operand = read16(data, offset + 2);
            offset += 4;
            DecodedBytecodeInstruction instruction;
            instruction.mOffset = instructionOffset;
            instruction.mOpcode = opcode;
            std::size_t payloadSize = operand;
            if (opcode == 0x1c)
            {
                // A reference-qualified command uses the otherwise length-shaped
                // word as its SCRO table index, followed by a normal command
                // opcode and payload length.
                instruction.mCallingReference = operand;
                instruction.mOpcode = read16(data, offset);
                payloadSize = read16(data, offset + 2);
                offset += 4;
            }
            if (payloadSize > data.size() - offset)
                throw std::runtime_error("SCDA instruction payload exceeds bytecode at offset "
                    + std::to_string(instructionOffset));
            instruction.mPayload.assign(data.begin() + static_cast<std::ptrdiff_t>(offset),
                data.begin() + static_cast<std::ptrdiff_t>(offset + payloadSize));
            offset += payloadSize;
            result.push_back(std::move(instruction));
        }
        return result;
    }

    DecodedAtomCounts BytecodeDecoder::decodeAtoms(
        const std::vector<DecodedBytecodeInstruction>& instructions) const
    {
        DecodedAtomCounts result;
        for (const DecodedBytecodeInstruction& instruction : instructions)
        {
            if (instruction.mCallingReference)
                ++result.mFormReferences;
            const std::vector<std::uint8_t>& data = instruction.mPayload;
            for (std::size_t i = 0; i < data.size();)
            {
                const std::uint8_t marker = data[i];
                if ((marker == 's' || marker == 'l' || marker == 'f') && i + 3 <= data.size())
                {
                    ++result.mLocalReferences;
                    i += 3;
                }
                else if ((marker == 'r' || marker == 'G') && i + 3 <= data.size())
                {
                    ++result.mFormReferences;
                    i += 3;
                }
                else if (marker == 'n' && i + 5 <= data.size())
                {
                    ++result.mIntegerLiterals;
                    i += 5;
                }
                else if (marker == 'z' && i + 9 <= data.size())
                {
                    ++result.mFloatLiterals;
                    i += 9;
                }
                else
                    ++i;
            }
        }
        return result;
    }

    BytecodeComparison BytecodeDecoder::compare(
        const Script& source, std::span<const std::uint8_t> data, std::optional<std::uint32_t> headerSize) const
    {
        BytecodeComparison result;
        result.mInstructions = decode(data);
        result.mDecoded = true;
        result.mHeaderSizeMatches = !headerSize || *headerSize == data.size();
        result.mAtoms = decodeAtoms(result.mInstructions);
        const std::map<std::uint16_t, std::size_t> expected = sourceStructure(source);
        std::map<std::uint16_t, std::size_t> actual;
        for (const DecodedBytecodeInstruction& instruction : result.mInstructions)
        {
            // Reference-qualified calls retain their nested opcode and do not
            // contribute the 0x1c wrapper to control-flow comparison.
            ++actual[instruction.mOpcode];
        }
        constexpr std::array<std::uint16_t, 9> opcodes{ 0x1d, 0x10, 0x11, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1e };
        for (const std::uint16_t opcode : opcodes)
            result.mStructure.push_back({ opcode, expected.contains(opcode) ? expected.at(opcode) : 0,
                actual.contains(opcode) ? actual.at(opcode) : 0 });
        return result;
    }
}
