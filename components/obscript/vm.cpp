#include "vm.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <sstream>

namespace ObScript
{
    namespace
    {
        RuntimeDiagnostic diagnostic(const RuntimeContext& context, const SourceLocation& location,
            std::string code, std::string message, std::string command = {})
        {
            return { std::move(code), std::move(message), std::move(command), location, context.mUnit,
                context.mEvent, context.mSequence };
        }

        Value pop(std::vector<Value>& stack)
        {
            if (stack.empty())
                throw RuntimeError("OBSV001", "ObScript operand stack underflow");
            Value result = std::move(stack.back());
            stack.pop_back();
            return result;
        }

        bool equalValues(const Value& left, const Value& right)
        {
            if (isNumeric(left) && isNumeric(right))
            {
                if (!std::holds_alternative<double>(left) && !std::holds_alternative<double>(right))
                    return asInteger(left) == asInteger(right);
                return asNumber(left) == asNumber(right);
            }
            if (const auto* l = std::get_if<ReferenceValue>(&left))
            {
                if (const auto* r = std::get_if<ReferenceValue>(&right))
                    return (!l->mKey.isNull() || !r->mKey.isNull()) ? l->mKey == r->mKey : l->mName == r->mName;
                if (isNumeric(right))
                    return !asBoolean(left) && asNumber(right) == 0;
                return false;
            }
            if (std::holds_alternative<ReferenceValue>(right) && isNumeric(left))
                return !asBoolean(right) && asNumber(left) == 0;
            if (const auto* l = std::get_if<std::string>(&left))
                if (const auto* r = std::get_if<std::string>(&right))
                    return *l == *r;
            return left.index() == right.index() && std::holds_alternative<std::monostate>(left);
        }

        std::int64_t wrapped(std::uint64_t value)
        {
            return std::bit_cast<std::int64_t>(value);
        }

        std::int64_t wrappedAdd(std::int64_t left, std::int64_t right)
        {
            return wrapped(static_cast<std::uint64_t>(left) + static_cast<std::uint64_t>(right));
        }

        std::int64_t wrappedSubtract(std::int64_t left, std::int64_t right)
        {
            return wrapped(static_cast<std::uint64_t>(left) - static_cast<std::uint64_t>(right));
        }

        std::int64_t wrappedMultiply(std::int64_t left, std::int64_t right)
        {
            return wrapped(static_cast<std::uint64_t>(left) * static_cast<std::uint64_t>(right));
        }

        std::int64_t wrappedNegate(std::int64_t value)
        {
            return wrapped(std::uint64_t{} - static_cast<std::uint64_t>(value));
        }

        Value binary(std::string_view op, const Value& left, const Value& right, ValueType type)
        {
            if (op == "==")
                return std::int64_t(equalValues(left, right));
            if (op == "!=")
                return std::int64_t(!equalValues(left, right));
            if (op == "&&")
                return std::int64_t(asBoolean(left) && asBoolean(right));
            if (op == "||")
                return std::int64_t(asBoolean(left) || asBoolean(right));
            if (op == "<")
                return std::int64_t(asNumber(left) < asNumber(right));
            if (op == ">")
                return std::int64_t(asNumber(left) > asNumber(right));
            if (op == "<=")
                return std::int64_t(asNumber(left) <= asNumber(right));
            if (op == ">=")
                return std::int64_t(asNumber(left) >= asNumber(right));

            if (type == ValueType::Float)
            {
                const double l = asNumber(left);
                const double r = asNumber(right);
                if (op == "+")
                    return l + r;
                if (op == "-")
                    return l - r;
                if (op == "*")
                    return l * r;
                if (op == "/")
                    return r == 0 ? 0.0 : l / r;
                if (op == "%")
                    return r == 0 ? 0.0 : std::fmod(l, r);
            }
            else
            {
                const std::int64_t l = asInteger(left);
                const std::int64_t r = asInteger(right);
                if (op == "+")
                    return wrappedAdd(l, r);
                if (op == "-")
                    return wrappedSubtract(l, r);
                if (op == "*")
                    return wrappedMultiply(l, r);
                if (op == "/")
                    return r == 0 ? std::int64_t{}
                                  : (l == std::numeric_limits<std::int64_t>::min() && r == -1 ? l : l / r);
                if (op == "%")
                    return r == 0 || (l == std::numeric_limits<std::int64_t>::min() && r == -1)
                        ? std::int64_t{}
                        : l % r;
            }
            throw RuntimeError("OBSV002", "Unknown ObScript binary operator '" + std::string(op) + "'");
        }
    }

    RuntimeError::RuntimeError(std::string code, std::string message, std::string command)
        : std::runtime_error(std::move(message))
        , mCode(std::move(code))
        , mCommand(std::move(command))
    {
    }

    bool isNumeric(const Value& value)
    {
        return std::holds_alternative<std::monostate>(value) || std::holds_alternative<std::int64_t>(value)
            || std::holds_alternative<double>(value);
    }

    bool asBoolean(const Value& value)
    {
        if (std::holds_alternative<std::monostate>(value))
            return false;
        if (const auto* integer = std::get_if<std::int64_t>(&value))
            return *integer != 0;
        if (const auto* number = std::get_if<double>(&value))
            return *number != 0;
        if (const auto* string = std::get_if<std::string>(&value))
            return !string->empty();
        const ReferenceValue& reference = std::get<ReferenceValue>(value);
        return !reference.mKey.isNull() || !reference.mName.empty();
    }

    std::int64_t asInteger(const Value& value)
    {
        if (std::holds_alternative<std::monostate>(value))
            return 0;
        if (const auto* integer = std::get_if<std::int64_t>(&value))
            return *integer;
        if (const auto* number = std::get_if<double>(&value))
        {
            if (!std::isfinite(*number))
                return 0;
            if (*number <= static_cast<double>(std::numeric_limits<std::int64_t>::min()))
                return std::numeric_limits<std::int64_t>::min();
            if (*number >= static_cast<double>(std::numeric_limits<std::int64_t>::max()))
                return std::numeric_limits<std::int64_t>::max();
            return static_cast<std::int64_t>(*number);
        }
        throw RuntimeError("OBSV003", "ObScript value is not numeric");
    }

    double asNumber(const Value& value)
    {
        if (std::holds_alternative<std::monostate>(value))
            return 0;
        if (const auto* integer = std::get_if<std::int64_t>(&value))
            return static_cast<double>(*integer);
        if (const auto* number = std::get_if<double>(&value))
            return *number;
        throw RuntimeError("OBSV003", "ObScript value is not numeric");
    }

    std::string valueString(const Value& value)
    {
        if (std::holds_alternative<std::monostate>(value))
            return {};
        if (const auto* integer = std::get_if<std::int64_t>(&value))
            return std::to_string(*integer);
        if (const auto* number = std::get_if<double>(&value))
        {
            std::ostringstream stream;
            stream.precision(17);
            stream << *number;
            return stream.str();
        }
        if (const auto* string = std::get_if<std::string>(&value))
            return *string;
        const ReferenceValue& reference = std::get<ReferenceValue>(value);
        return reference.mKey.isNull() ? reference.mName : reference.mKey.serialize();
    }

    Value convert(Value value, ValueType type)
    {
        switch (type)
        {
            case ValueType::Short:
                return std::int64_t(std::clamp<std::int64_t>(asInteger(value),
                    std::numeric_limits<std::int16_t>::min(), std::numeric_limits<std::int16_t>::max()));
            case ValueType::Integer:
                return std::int64_t(std::clamp<std::int64_t>(asInteger(value),
                    std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()));
            case ValueType::Long:
                return asInteger(value);
            case ValueType::Float:
                return asNumber(value);
            case ValueType::Boolean:
                return std::int64_t(asBoolean(value));
            case ValueType::String:
                return valueString(value);
            case ValueType::Reference:
                if (std::holds_alternative<ReferenceValue>(value))
                    return value;
                return ReferenceValue{ {}, valueString(value) };
            case ValueType::Void:
            case ValueType::Unknown:
                return value;
        }
        return value;
    }

    VirtualMachine::VirtualMachine(ExecutionLimits limits)
        : mLimits(limits)
    {
        if (mLimits.mMaximumInstructions == 0 || mLimits.mMaximumStack == 0)
            throw std::invalid_argument("ObScript VM limits must be non-zero");
    }

    std::vector<Value> VirtualMachine::makeLocals(const Program& program)
    {
        std::vector<Value> result;
        result.reserve(program.mLocals.size());
        for (const Local& local : program.mLocals)
        {
            if (local.mType == VariableType::Float)
                result.emplace_back(0.0);
            else if (local.mType == VariableType::Reference)
                result.emplace_back(ReferenceValue{});
            else
                result.emplace_back(std::int64_t{});
        }
        return result;
    }

    ExecutionReport VirtualMachine::execute(const Program& program, const EntryPoint& entry,
        std::vector<Value>& locals, RuntimeHost& host, const RuntimeContext& context) const
    {
        ExecutionReport report;
        if (locals.size() != program.mLocals.size())
        {
            report.mDiagnostics.push_back(diagnostic(context, {}, "OBSV004",
                "ObScript local-state shape does not match Program"));
            return report;
        }

        std::vector<Value> stack;
        std::size_t pc = 0;
        try
        {
            while (pc < entry.mCode.size())
            {
                if (++report.mInstructionCount > mLimits.mMaximumInstructions)
                    throw RuntimeError("OBSV005", "ObScript instruction limit exceeded");
                const Instruction& item = entry.mCode[pc++];
                switch (item.mOpcode)
                {
                    case OpCode::PushMissing:
                        stack.emplace_back(std::int64_t{});
                        break;
                    case OpCode::PushInteger:
                        stack.emplace_back(item.mInteger);
                        break;
                    case OpCode::PushFloat:
                        stack.emplace_back(item.mFloat);
                        break;
                    case OpCode::PushString:
                        stack.emplace_back(item.mText);
                        break;
                    case OpCode::LoadLocal:
                        if (item.mIndex >= locals.size())
                            throw RuntimeError("OBSV006", "ObScript local index is out of range");
                        stack.push_back(locals[item.mIndex]);
                        break;
                    case OpCode::LoadReference:
                        stack.push_back(host.resolveName(item.mText, context));
                        break;
                    case OpCode::LoadMember:
                    {
                        Value target = pop(stack);
                        stack.push_back(host.loadMember(target, item.mText, context));
                        break;
                    }
                    case OpCode::Negate:
                    {
                        Value value = pop(stack);
                        stack.push_back(item.mType == ValueType::Float ? Value(-asNumber(value))
                                                                      : Value(wrappedNegate(asInteger(value))));
                        break;
                    }
                    case OpCode::Binary:
                    {
                        Value right = pop(stack);
                        Value left = pop(stack);
                        stack.push_back(binary(item.mText, left, right, item.mType));
                        break;
                    }
                    case OpCode::Call:
                    {
                        std::vector<Value> arguments(item.mArgumentCount);
                        for (std::size_t i = arguments.size(); i > 0; --i)
                            arguments[i - 1] = pop(stack);
                        std::optional<Value> target;
                        if (item.mMemberCall)
                            target = pop(stack);
                        stack.push_back(host.call(item.mText, target, arguments, context, item.mLocation));
                        break;
                    }
                    case OpCode::Convert:
                    {
                        Value value = pop(stack);
                        stack.push_back(convert(std::move(value), item.mType));
                        break;
                    }
                    case OpCode::StoreLocal:
                        if (item.mIndex >= locals.size())
                            throw RuntimeError("OBSV006", "ObScript local index is out of range");
                        locals[item.mIndex] = convert(pop(stack), item.mType);
                        break;
                    case OpCode::StoreExternal:
                        host.storeExternal(item.mText, pop(stack), context);
                        break;
                    case OpCode::StoreMember:
                    {
                        Value value = pop(stack);
                        Value target = pop(stack);
                        host.storeMember(target, item.mText, value, context);
                        break;
                    }
                    case OpCode::JumpIfFalse:
                    {
                        const bool condition = asBoolean(pop(stack));
                        if (!condition)
                        {
                            if (item.mIndex > entry.mCode.size())
                                throw RuntimeError("OBSV007", "ObScript branch target is out of range");
                            pc = item.mIndex;
                        }
                        break;
                    }
                    case OpCode::Jump:
                        if (item.mIndex > entry.mCode.size())
                            throw RuntimeError("OBSV007", "ObScript branch target is out of range");
                        pc = item.mIndex;
                        break;
                    case OpCode::Return:
                        report.mCompleted = true;
                        report.mReturned = true;
                        return report;
                    case OpCode::Discard:
                        static_cast<void>(pop(stack));
                        break;
                }
                if (stack.size() > mLimits.mMaximumStack)
                    throw RuntimeError("OBSV008", "ObScript operand stack limit exceeded");
            }
            report.mCompleted = true;
        }
        catch (const RuntimeError& error)
        {
            const SourceLocation location
                = pc == 0 || pc > entry.mCode.size() ? SourceLocation{} : entry.mCode[pc - 1].mLocation;
            report.mDiagnostics.push_back(
                diagnostic(context, location, error.code(), error.what(), error.command()));
        }
        catch (const std::exception& error)
        {
            const SourceLocation location
                = pc == 0 || pc > entry.mCode.size() ? SourceLocation{} : entry.mCode[pc - 1].mLocation;
            report.mDiagnostics.push_back(
                diagnostic(context, location, "OBSV999", error.what()));
        }
        return report;
    }
}
