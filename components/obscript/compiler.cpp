#include "compiler.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace ObScript
{
    namespace
    {
        ValueType valueType(VariableType value)
        {
            switch (value)
            {
                case VariableType::Short:
                    return ValueType::Short;
                case VariableType::Integer:
                    return ValueType::Integer;
                case VariableType::Long:
                    return ValueType::Long;
                case VariableType::Float:
                    return ValueType::Float;
                case VariableType::Reference:
                    return ValueType::Reference;
            }
            return ValueType::Unknown;
        }

        std::string memberName(const Expression& value)
        {
            if (value.mKind == ExpressionKind::Name || value.mKind == ExpressionKind::String)
                return value.mValue;
            if (value.mKind == ExpressionKind::Integer)
                return std::to_string(value.mInteger);
            if (value.mKind == ExpressionKind::Member && value.mChildren.size() == 2)
                return memberName(value.mChildren[1]);
            return canonical(value);
        }

        std::string runtimeArgument(const Expression& value)
        {
            switch (value.mKind)
            {
                case ExpressionKind::Name:
                    return lowerCase(value.mValue);
                case ExpressionKind::String:
                    return value.mValue;
                case ExpressionKind::Integer:
                    return std::to_string(value.mInteger);
                default:
                    return canonical(value);
            }
        }

        Instruction instruction(OpCode opcode, ValueType type, const SourceLocation& location)
        {
            Instruction result;
            result.mOpcode = opcode;
            result.mType = type;
            result.mLocation = location;
            return result;
        }

        bool looksLikeFunction(std::string_view value)
        {
            const std::string name = lowerCase(value);
            constexpr std::array<std::string_view, 7> prefixes{ "get", "is", "has", "can", "which", "exists",
                "menumode" };
            return std::any_of(prefixes.begin(), prefixes.end(),
                [&](std::string_view prefix) { return name.starts_with(prefix); });
        }

        std::string referenceFingerprint(const std::vector<ESM::FormKey>& references)
        {
            std::string canonical;
            for (const ESM::FormKey& reference : references)
            {
                const std::string serialized = reference.serialize();
                canonical += std::to_string(serialized.size());
                canonical += ':';
                canonical += serialized;
            }
            return fingerprint(canonical);
        }
    }

    Program Compiler::compile(const ScriptUnitId& unit, const Script& script)
    {
        mUnit = unit;
        Program result;
        result.mUnit = unit;
        result.mScriptName = script.mName;
        for (const VariableDeclaration& declaration : script.mVariables)
            addLocal(declaration, result);
        for (const EventBlock& block : script.mBlocks)
            collectLocals(block.mBody, result);
        collectLocals(script.mStray, result);

        for (const EventBlock& block : script.mBlocks)
        {
            EntryPoint entry;
            entry.mEvent = block.mEvent;
            for (const Expression& argument : block.mArguments)
            {
                entry.mArguments.push_back(canonical(argument));
                entry.mRuntimeArguments.push_back(runtimeArgument(argument));
            }
            compileStatements(block.mBody, entry, result);
            result.mEntryPoints.push_back(std::move(entry));
        }
        if (!script.mStray.empty())
        {
            EntryPoint entry;
            entry.mEvent = "__stray";
            compileStatements(script.mStray, entry, result);
            result.mEntryPoints.push_back(std::move(entry));
        }
        return result;
    }

    void Compiler::collectLocals(const std::vector<Statement>& statements, Program& program)
    {
        for (const Statement& statement : statements)
        {
            if (statement.mKind == StatementKind::VariableDeclaration && statement.mDeclaration)
                addLocal(*statement.mDeclaration, program);
            for (const Clause& clause : statement.mClauses)
                collectLocals(clause.mBody, program);
        }
    }

    void Compiler::addLocal(const VariableDeclaration& declaration, Program& program)
    {
        // The vanilla compiler retains duplicate declarations (including
        // declarations with the same name but different types) in its local
        // table. Preserve all of them in source order; name lookup remains
        // deterministic and resolves to the first declaration.
        program.mLocals.push_back({ declaration.mName, declaration.mType });
    }

    ValueType Compiler::compileExpression(
        const Expression& value, EntryPoint& entry, const Program& program, CoverageRole role)
    {
        switch (value.mKind)
        {
            case ExpressionKind::Missing:
                entry.mCode.push_back(instruction(OpCode::PushMissing, ValueType::Integer, value.mLocation));
                return ValueType::Integer;
            case ExpressionKind::Integer:
            {
                Instruction item = instruction(OpCode::PushInteger, ValueType::Long, value.mLocation);
                item.mInteger = value.mInteger;
                entry.mCode.push_back(std::move(item));
                return ValueType::Long;
            }
            case ExpressionKind::Float:
            {
                Instruction item = instruction(OpCode::PushFloat, ValueType::Float, value.mLocation);
                item.mFloat = value.mFloat;
                entry.mCode.push_back(std::move(item));
                return ValueType::Float;
            }
            case ExpressionKind::String:
            {
                Instruction item = instruction(OpCode::PushString, ValueType::String, value.mLocation);
                item.mText = value.mValue;
                entry.mCode.push_back(std::move(item));
                return ValueType::String;
            }
            case ExpressionKind::Name:
            {
                if (const std::optional<std::uint32_t> local = findLocal(value.mValue, program))
                {
                    Instruction item = instruction(
                        OpCode::LoadLocal, valueType(program.mLocals[*local].mType), value.mLocation);
                    item.mIndex = *local;
                    entry.mCode.push_back(std::move(item));
                    return valueType(program.mLocals[*local].mType);
                }
                if (looksLikeFunction(value.mValue))
                {
                    Instruction item = instruction(OpCode::Call, ValueType::Unknown, value.mLocation);
                    item.mText = lowerCase(value.mValue);
                    entry.mCode.push_back(std::move(item));
                    recordCall(value.mValue, role);
                    return ValueType::Unknown;
                }
                Instruction item = instruction(OpCode::LoadReference, ValueType::Reference, value.mLocation);
                item.mText = lowerCase(value.mValue);
                entry.mCode.push_back(std::move(item));
                return ValueType::Reference;
            }
            case ExpressionKind::Negate:
            {
                const ValueType type = compileExpression(value.mChildren.at(0), entry, program, role);
                entry.mCode.push_back(instruction(OpCode::Negate, type, value.mLocation));
                return type;
            }
            case ExpressionKind::Binary:
            {
                const ValueType left = compileExpression(value.mChildren.at(0), entry, program, role);
                const ValueType right = compileExpression(value.mChildren.at(1), entry, program, role);
                const bool comparison = value.mValue == "==" || value.mValue == "!=" || value.mValue == "<"
                    || value.mValue == ">" || value.mValue == "<=" || value.mValue == ">=" || value.mValue == "&&"
                    || value.mValue == "||";
                const ValueType type = comparison ? ValueType::Boolean
                                                  : (left == ValueType::Float || right == ValueType::Float
                                                            ? ValueType::Float
                                                            : ValueType::Long);
                Instruction item = instruction(OpCode::Binary, type, value.mLocation);
                item.mText = value.mValue;
                entry.mCode.push_back(std::move(item));
                return type;
            }
            case ExpressionKind::Member:
            {
                compileExpression(value.mChildren.at(0), entry, program, role);
                const std::string name = memberName(value.mChildren.at(1));
                if (looksLikeFunction(name))
                {
                    Instruction item = instruction(OpCode::Call, ValueType::Unknown, value.mLocation);
                    item.mText = lowerCase(name);
                    item.mMemberCall = true;
                    entry.mCode.push_back(std::move(item));
                    recordCall(name, role);
                    return ValueType::Unknown;
                }
                Instruction item = instruction(OpCode::LoadMember, ValueType::Unknown, value.mLocation);
                item.mText = lowerCase(name);
                entry.mCode.push_back(std::move(item));
                return ValueType::Unknown;
            }
            case ExpressionKind::Call:
            {
                const Expression& callee = value.mChildren.at(0);
                std::string name;
                bool member = false;
                if (callee.mKind == ExpressionKind::Member && callee.mChildren.size() == 2)
                {
                    member = true;
                    compileExpression(callee.mChildren[0], entry, program, role);
                    name = memberName(callee.mChildren[1]);
                }
                else if (callee.mKind == ExpressionKind::Name)
                    name = callee.mValue;
                else
                {
                    compileExpression(callee, entry, program, role);
                    name = "__expression";
                }
                for (std::size_t i = 1; i < value.mChildren.size(); ++i)
                    compileExpression(value.mChildren[i], entry, program, role);
                Instruction item = instruction(OpCode::Call, ValueType::Unknown, value.mLocation);
                item.mText = lowerCase(name);
                item.mArgumentCount = static_cast<std::uint32_t>(value.mChildren.size() - 1);
                item.mMemberCall = member;
                entry.mCode.push_back(std::move(item));
                if (name != "__expression")
                    recordCall(name, role);
                return ValueType::Unknown;
            }
        }
        throw std::logic_error("Unknown ObScript expression kind");
    }

    void Compiler::compileStatements(
        const std::vector<Statement>& statements, EntryPoint& entry, const Program& program)
    {
        for (const Statement& statement : statements)
            compileStatement(statement, entry, program);
    }

    void Compiler::compileStatement(const Statement& statement, EntryPoint& entry, const Program& program)
    {
        switch (statement.mKind)
        {
            case StatementKind::VariableDeclaration:
            case StatementKind::StrayKeyword:
            case StatementKind::Junk:
                return;
            case StatementKind::Return:
                entry.mCode.push_back(instruction(OpCode::Return, ValueType::Void, statement.mLocation));
                return;
            case StatementKind::Expression:
            {
                const Expression& expression = statement.mValue.value();
                if (expression.mKind == ExpressionKind::Name && !findLocal(expression.mValue, program))
                {
                    Instruction item = instruction(OpCode::Call, ValueType::Unknown, expression.mLocation);
                    item.mText = lowerCase(expression.mValue);
                    entry.mCode.push_back(std::move(item));
                    recordCall(expression.mValue, CoverageRole::Command);
                }
                else if (expression.mKind == ExpressionKind::Member && expression.mChildren.size() == 2)
                {
                    compileExpression(expression.mChildren[0], entry, program, CoverageRole::Command);
                    Instruction item = instruction(OpCode::Call, ValueType::Unknown, expression.mLocation);
                    item.mText = lowerCase(memberName(expression.mChildren[1]));
                    item.mMemberCall = true;
                    entry.mCode.push_back(std::move(item));
                    recordCall(memberName(expression.mChildren[1]), CoverageRole::Command);
                }
                else
                {
                    compileExpression(expression, entry, program, CoverageRole::Command);
                    entry.mCode.push_back(instruction(OpCode::Discard, ValueType::Void, expression.mLocation));
                }
                return;
            }
            case StatementKind::Set:
            {
                const Expression& target = statement.mTarget.value();
                if (target.mKind == ExpressionKind::Member && target.mChildren.size() == 2)
                    compileExpression(target.mChildren[0], entry, program, CoverageRole::Command);
                const ValueType source
                    = compileExpression(statement.mValue.value(), entry, program, CoverageRole::Command);
                if (target.mKind == ExpressionKind::Name)
                {
                    if (const std::optional<std::uint32_t> local = findLocal(target.mValue, program))
                    {
                        const ValueType destination = valueType(program.mLocals[*local].mType);
                        if (source != destination && destination != ValueType::Reference)
                            entry.mCode.push_back(instruction(OpCode::Convert, destination, target.mLocation));
                        Instruction item = instruction(OpCode::StoreLocal, destination, target.mLocation);
                        item.mIndex = *local;
                        entry.mCode.push_back(std::move(item));
                    }
                    else
                    {
                        Instruction item = instruction(OpCode::StoreExternal, source, target.mLocation);
                        item.mText = lowerCase(target.mValue);
                        entry.mCode.push_back(std::move(item));
                    }
                }
                else if (target.mKind == ExpressionKind::Member && target.mChildren.size() == 2)
                {
                    Instruction item = instruction(OpCode::StoreMember, source, target.mLocation);
                    item.mText = lowerCase(memberName(target.mChildren[1]));
                    entry.mCode.push_back(std::move(item));
                }
                else
                    throw FrontendError({ DiagnosticSeverity::Error, "OBSS002",
                        "Set target is not a variable or member", target.mLocation });
                return;
            }
            case StatementKind::If:
            {
                std::vector<std::size_t> endJumps;
                for (std::size_t i = 0; i < statement.mClauses.size(); ++i)
                {
                    const Clause& clause = statement.mClauses[i];
                    std::optional<std::size_t> falseJump;
                    if (clause.mCondition)
                    {
                        compileExpression(*clause.mCondition, entry, program, CoverageRole::Condition);
                        falseJump = entry.mCode.size();
                        entry.mCode.push_back(
                            instruction(OpCode::JumpIfFalse, ValueType::Void, clause.mLocation));
                    }
                    compileStatements(clause.mBody, entry, program);
                    if (i + 1 < statement.mClauses.size())
                    {
                        endJumps.push_back(entry.mCode.size());
                        entry.mCode.push_back(instruction(OpCode::Jump, ValueType::Void, clause.mLocation));
                    }
                    if (falseJump)
                        entry.mCode[*falseJump].mIndex = static_cast<std::uint32_t>(entry.mCode.size());
                }
                const std::uint32_t end = static_cast<std::uint32_t>(entry.mCode.size());
                for (const std::size_t jump : endJumps)
                    entry.mCode[jump].mIndex = end;
                return;
            }
        }
    }

    std::optional<std::uint32_t> Compiler::findLocal(std::string_view name, const Program& program) const
    {
        const std::string key = lowerCase(name);
        for (std::size_t i = 0; i < program.mLocals.size(); ++i)
        {
            if (lowerCase(program.mLocals[i].mName) == key)
                return static_cast<std::uint32_t>(i);
        }
        return std::nullopt;
    }

    void Compiler::recordCall(std::string_view name, CoverageRole role)
    {
        if (mCoverage != nullptr)
            mCoverage->record(name, role, mUnit.mContext);
    }

    CompilationResult CompilationCache::compile(const ScriptUnit& unit)
    {
        if (!unit.mDefinition.sourceData)
        {
            CompilationResult result;
            result.mDiagnostics.push_back({ { DiagnosticSeverity::Error, "OBSC001",
                                                "Script unit has SCDA but no portable SCTX source", {} },
                unit.mId, {} });
            return result;
        }
        const std::string sourceFingerprint = fingerprint(*unit.mDefinition.sourceData);
        const std::string referencesFingerprint = referenceFingerprint(unit.mDefinition.globalReferences);
        if (const auto it = mEntries.find(unit.mId); it != mEntries.end())
        {
            if (it->second.mSourceFingerprint != sourceFingerprint
                || it->second.mReferenceFingerprint != referencesFingerprint)
                throw std::runtime_error("ObScript cache identity reused with different compilation input: "
                    + unit.mId.serialize());
            CompilationResult result = it->second;
            result.mCacheHit = true;
            return result;
        }

        CompilationResult result;
        result.mSourceFingerprint = sourceFingerprint;
        result.mReferenceFingerprint = referencesFingerprint;
        try
        {
            auto ast = std::make_shared<Script>(Parser{}.parse(unit.mDefinition.scriptSource));
            result.mAstFingerprint = fingerprint(canonical(*ast));
            auto program = std::make_shared<Program>(Compiler(mCoverage).compile(unit.mId, *ast));
            program->mReferences = unit.mDefinition.globalReferences;
            result.mProgramFingerprint = fingerprint(canonical(*program));
            result.mAst = std::move(ast);
            result.mProgram = std::move(program);
        }
        catch (const FrontendError& error)
        {
            result.mDiagnostics.push_back({ error.diagnostic(), unit.mId, {} });
        }
        mEntries.emplace(unit.mId, result);
        return result;
    }
}
