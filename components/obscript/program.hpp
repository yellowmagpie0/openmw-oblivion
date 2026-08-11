#ifndef OPENMW_COMPONENTS_OBSCRIPT_PROGRAM_H
#define OPENMW_COMPONENTS_OBSCRIPT_PROGRAM_H

#include "ast.hpp"
#include "corpus.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace ObScript
{
    enum class ValueType : std::uint8_t
    {
        Void,
        Short,
        Integer,
        Long,
        Float,
        Boolean,
        String,
        Reference,
        Unknown,
    };

    enum class OpCode : std::uint8_t
    {
        PushMissing,
        PushInteger,
        PushFloat,
        PushString,
        LoadLocal,
        LoadReference,
        LoadMember,
        Negate,
        Binary,
        Call,
        Convert,
        StoreLocal,
        StoreExternal,
        StoreMember,
        JumpIfFalse,
        Jump,
        Return,
        Discard,
    };

    struct Instruction
    {
        OpCode mOpcode = OpCode::PushMissing;
        ValueType mType = ValueType::Unknown;
        SourceLocation mLocation;
        std::string mText;
        std::int64_t mInteger = 0;
        double mFloat = 0;
        std::uint32_t mIndex = 0;
        std::uint32_t mArgumentCount = 0;
        bool mMemberCall = false;

        friend bool operator==(const Instruction&, const Instruction&) = default;
    };

    struct Local
    {
        std::string mName;
        VariableType mType = VariableType::Short;

        friend bool operator==(const Local&, const Local&) = default;
    };

    struct EntryPoint
    {
        std::string mEvent;
        std::vector<std::string> mArguments;
        // Plain, case-folded event filters used by the runtime. The canonical
        // expression encodings above remain the portable IR representation and
        // keep the M6 Program fingerprint stable.
        std::vector<std::string> mRuntimeArguments;
        std::vector<Instruction> mCode;

        friend bool operator==(const EntryPoint&, const EntryPoint&) = default;
    };

    struct Program
    {
        ScriptUnitId mUnit;
        std::optional<std::string> mScriptName;
        std::vector<Local> mLocals;
        // Stable SCRO reference table retained from the owning script unit.
        // The runtime resolves names and compiled operands through these keys.
        std::vector<ESM::FormKey> mReferences;
        std::vector<EntryPoint> mEntryPoints;

        friend bool operator==(const Program&, const Program&) = default;
    };

    enum class CoverageRole : std::uint8_t
    {
        Command,
        Condition,
    };

    struct CoverageEntry
    {
        std::string mName;
        std::uint64_t mCommandUses = 0;
        std::uint64_t mConditionUses = 0;
        std::set<ExecutionContext> mContexts;

        friend bool operator==(const CoverageEntry&, const CoverageEntry&) = default;
    };

    class CoverageRegistry
    {
    public:
        void record(std::string_view name, CoverageRole role, ExecutionContext context);
        const std::map<std::string, CoverageEntry>& entries() const { return mEntries; }

    private:
        std::map<std::string, CoverageEntry> mEntries;
    };

    struct CompilationDiagnostic
    {
        Diagnostic mDiagnostic;
        ScriptUnitId mUnit;
        std::string mCommand;

        friend bool operator==(const CompilationDiagnostic&, const CompilationDiagnostic&) = default;
    };

    struct CompilationResult
    {
        std::shared_ptr<const Script> mAst;
        std::shared_ptr<const Program> mProgram;
        std::string mSourceFingerprint;
        std::string mReferenceFingerprint;
        std::string mAstFingerprint;
        std::string mProgramFingerprint;
        std::vector<CompilationDiagnostic> mDiagnostics;
        bool mCacheHit = false;
    };

    std::string_view toString(ValueType value);
    std::string_view toString(OpCode value);
    std::string canonical(const Program& value);
}

#endif
