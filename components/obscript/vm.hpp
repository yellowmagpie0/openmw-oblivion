#ifndef OPENMW_COMPONENTS_OBSCRIPT_VM_H
#define OPENMW_COMPONENTS_OBSCRIPT_VM_H

#include "program.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace ObScript
{
    struct ReferenceValue
    {
        ESM::FormKey mKey;
        std::string mName;

        friend bool operator==(const ReferenceValue&, const ReferenceValue&) = default;
    };

    using Value = std::variant<std::monostate, std::int64_t, double, std::string, ReferenceValue>;

    struct RuntimeContext
    {
        ScriptUnitId mUnit;
        ESM::FormKey mInstance;
        ESM::FormKey mSelf;
        ESM::FormKey mActionReference;
        std::string mEvent;
        std::vector<ReferenceValue> mEventArguments;
        double mSecondsPassed = 0;
        std::uint64_t mSequence = 0;
        std::uint32_t mDepth = 0;
    };

    struct RuntimeDiagnostic
    {
        std::string mCode;
        std::string mMessage;
        std::string mCommand;
        SourceLocation mLocation;
        ScriptUnitId mUnit;
        std::string mEvent;
        std::uint64_t mSequence = 0;

        friend bool operator==(const RuntimeDiagnostic&, const RuntimeDiagnostic&) = default;
    };

    class RuntimeError : public std::runtime_error
    {
    public:
        RuntimeError(std::string code, std::string message, std::string command = {});

        const std::string& code() const { return mCode; }
        const std::string& command() const { return mCommand; }

    private:
        std::string mCode;
        std::string mCommand;
    };

    class RuntimeHost
    {
    public:
        virtual ~RuntimeHost() = default;

        virtual Value resolveName(std::string_view name, const RuntimeContext& context) = 0;
        virtual Value loadMember(const Value& target, std::string_view name, const RuntimeContext& context) = 0;
        virtual void storeExternal(std::string_view name, const Value& value, const RuntimeContext& context) = 0;
        virtual void storeMember(
            const Value& target, std::string_view name, const Value& value, const RuntimeContext& context)
            = 0;
        virtual Value call(std::string_view name, const std::optional<Value>& target,
            const std::vector<Value>& arguments, const RuntimeContext& context, const SourceLocation& location)
            = 0;
    };

    struct ExecutionLimits
    {
        std::size_t mMaximumInstructions = 1'000'000;
        std::size_t mMaximumStack = 16'384;
    };

    struct ExecutionReport
    {
        bool mCompleted = false;
        bool mReturned = false;
        std::size_t mInstructionCount = 0;
        std::vector<RuntimeDiagnostic> mDiagnostics;
    };

    class VirtualMachine
    {
    public:
        explicit VirtualMachine(ExecutionLimits limits = {});

        ExecutionReport execute(const Program& program, const EntryPoint& entry, std::vector<Value>& locals,
            RuntimeHost& host, const RuntimeContext& context) const;

        static std::vector<Value> makeLocals(const Program& program);

    private:
        ExecutionLimits mLimits;
    };

    bool isNumeric(const Value& value);
    bool asBoolean(const Value& value);
    std::int64_t asInteger(const Value& value);
    double asNumber(const Value& value);
    std::string valueString(const Value& value);
    Value convert(Value value, ValueType type);
}

#endif
