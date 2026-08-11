#include "program.hpp"

#include <bit>
#include <iomanip>
#include <sstream>

namespace ObScript
{
    namespace
    {
        void atom(std::ostringstream& stream, std::string_view value)
        {
            stream << value.size() << ':';
            stream.write(value.data(), static_cast<std::streamsize>(value.size()));
        }
    }

    void CoverageRegistry::record(std::string_view name, CoverageRole role, ExecutionContext context)
    {
        const std::string key = lowerCase(name);
        CoverageEntry& entry = mEntries[key];
        if (entry.mName.empty())
            entry.mName = key;
        if (role == CoverageRole::Command)
            ++entry.mCommandUses;
        else
            ++entry.mConditionUses;
        entry.mContexts.insert(context);
    }

    std::string_view toString(ValueType value)
    {
        switch (value)
        {
            case ValueType::Void:
                return "void";
            case ValueType::Short:
                return "short";
            case ValueType::Integer:
                return "int";
            case ValueType::Long:
                return "long";
            case ValueType::Float:
                return "float";
            case ValueType::Boolean:
                return "bool";
            case ValueType::String:
                return "string";
            case ValueType::Reference:
                return "ref";
            case ValueType::Unknown:
                return "unknown";
        }
        return "invalid";
    }

    std::string_view toString(OpCode value)
    {
        switch (value)
        {
            case OpCode::PushMissing:
                return "push-missing";
            case OpCode::PushInteger:
                return "push-int";
            case OpCode::PushFloat:
                return "push-float";
            case OpCode::PushString:
                return "push-string";
            case OpCode::LoadLocal:
                return "load-local";
            case OpCode::LoadReference:
                return "load-ref";
            case OpCode::LoadMember:
                return "load-member";
            case OpCode::Negate:
                return "negate";
            case OpCode::Binary:
                return "binary";
            case OpCode::Call:
                return "call";
            case OpCode::Convert:
                return "convert";
            case OpCode::StoreLocal:
                return "store-local";
            case OpCode::StoreExternal:
                return "store-external";
            case OpCode::StoreMember:
                return "store-member";
            case OpCode::JumpIfFalse:
                return "jump-if-false";
            case OpCode::Jump:
                return "jump";
            case OpCode::Return:
                return "return";
            case OpCode::Discard:
                return "discard";
        }
        return "invalid";
    }

    std::string canonical(const Program& value)
    {
        std::ostringstream stream;
        stream << "(P";
        atom(stream, value.mUnit.serialize());
        stream << (value.mScriptName ? '1' : '0');
        if (value.mScriptName)
            atom(stream, lowerCase(*value.mScriptName));
        stream << '[';
        for (const Local& local : value.mLocals)
        {
            stream << "(L";
            atom(stream, lowerCase(local.mName));
            atom(stream, toString(local.mType));
            stream << ')';
        }
        stream << "][";
        for (const ESM::FormKey& reference : value.mReferences)
            atom(stream, reference.serialize());
        stream << "][";
        for (const EntryPoint& entry : value.mEntryPoints)
        {
            stream << "(E";
            atom(stream, lowerCase(entry.mEvent));
            stream << '[';
            for (const std::string& argument : entry.mArguments)
                atom(stream, argument);
            stream << "][";
            for (const Instruction& instruction : entry.mCode)
            {
                stream << "(I";
                atom(stream, toString(instruction.mOpcode));
                atom(stream, toString(instruction.mType));
                atom(stream, instruction.mText);
                stream << instruction.mInteger << ':' << std::hex << std::setw(16) << std::setfill('0')
                       << std::bit_cast<std::uint64_t>(instruction.mFloat) << std::dec << ':' << instruction.mIndex
                       << ':' << instruction.mArgumentCount << ':' << (instruction.mMemberCall ? '1' : '0') << ')';
            }
            stream << "])";
        }
        stream << "])";
        return stream.str();
    }
}
