#ifndef OPENMW_COMPONENTS_OBSCRIPT_AST_H
#define OPENMW_COMPONENTS_OBSCRIPT_AST_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace ObScript
{
    struct SourceLocation
    {
        std::size_t mOffset = 0;
        std::uint32_t mLine = 1;
        std::uint32_t mColumn = 1;

        friend bool operator==(const SourceLocation&, const SourceLocation&) = default;
    };

    enum class DiagnosticSeverity : std::uint8_t
    {
        Warning,
        Error,
    };

    struct Diagnostic
    {
        DiagnosticSeverity mSeverity = DiagnosticSeverity::Error;
        std::string mCode;
        std::string mMessage;
        SourceLocation mLocation;

        friend bool operator==(const Diagnostic&, const Diagnostic&) = default;
    };

    class FrontendError : public std::runtime_error
    {
    public:
        explicit FrontendError(Diagnostic diagnostic);

        const Diagnostic& diagnostic() const { return mDiagnostic; }

    private:
        Diagnostic mDiagnostic;
    };

    enum class VariableType : std::uint8_t
    {
        Short,
        Integer,
        Long,
        Float,
        Reference,
    };

    enum class ExpressionKind : std::uint8_t
    {
        Missing,
        Integer,
        Float,
        String,
        Name,
        Negate,
        Binary,
        Member,
        Call,
    };

    struct Expression
    {
        ExpressionKind mKind = ExpressionKind::Missing;
        SourceLocation mLocation;
        std::string mValue;
        std::int64_t mInteger = 0;
        double mFloat = 0;
        std::vector<Expression> mChildren;

        friend bool operator==(const Expression&, const Expression&) = default;
    };

    struct VariableDeclaration
    {
        VariableType mType = VariableType::Short;
        std::string mName;
        SourceLocation mLocation;

        friend bool operator==(const VariableDeclaration&, const VariableDeclaration&) = default;
    };

    enum class StatementKind : std::uint8_t
    {
        VariableDeclaration,
        Set,
        If,
        Return,
        Expression,
        StrayKeyword,
        Junk,
    };

    struct Statement;

    struct Clause
    {
        std::optional<Expression> mCondition;
        std::vector<Statement> mBody;
        SourceLocation mLocation;

        friend bool operator==(const Clause&, const Clause&) = default;
    };

    struct Statement
    {
        StatementKind mKind = StatementKind::Junk;
        SourceLocation mLocation;
        std::optional<VariableDeclaration> mDeclaration;
        std::optional<Expression> mTarget;
        std::optional<Expression> mValue;
        std::vector<Clause> mClauses;
        std::string mKeyword;

        friend bool operator==(const Statement&, const Statement&) = default;
    };

    struct EventBlock
    {
        std::string mEvent;
        SourceLocation mLocation;
        std::vector<Expression> mArguments;
        std::vector<Statement> mBody;

        friend bool operator==(const EventBlock&, const EventBlock&) = default;
    };

    struct Script
    {
        std::optional<std::string> mName;
        std::vector<VariableDeclaration> mVariables;
        std::vector<EventBlock> mBlocks;
        std::vector<Statement> mStray;

        friend bool operator==(const Script&, const Script&) = default;
    };

    std::string_view toString(VariableType value);
    std::string canonical(const Script& value);
    std::string canonical(const Expression& value);
    std::string fingerprint(std::string_view value);
    std::string fingerprint(const std::vector<std::uint8_t>& value);
    std::string lowerCase(std::string_view value);
}

#endif
