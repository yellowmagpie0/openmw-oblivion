#include "ast.hpp"

#include <bit>
#include <iomanip>
#include <limits>
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

        void writeExpression(std::ostringstream& stream, const Expression& value)
        {
            stream << '(';
            switch (value.mKind)
            {
                case ExpressionKind::Missing:
                    stream << 'M';
                    break;
                case ExpressionKind::Integer:
                    stream << 'I' << value.mInteger;
                    break;
                case ExpressionKind::Float:
                    stream << 'F' << std::hex << std::setw(16) << std::setfill('0')
                           << std::bit_cast<std::uint64_t>(value.mFloat) << std::dec;
                    break;
                case ExpressionKind::String:
                    stream << 'S';
                    atom(stream, value.mValue);
                    break;
                case ExpressionKind::Name:
                    stream << 'N';
                    atom(stream, value.mValue);
                    break;
                case ExpressionKind::Negate:
                    stream << '-';
                    break;
                case ExpressionKind::Binary:
                    stream << 'B';
                    atom(stream, value.mValue);
                    break;
                case ExpressionKind::Member:
                    stream << 'D';
                    break;
                case ExpressionKind::Call:
                    stream << 'C';
                    break;
            }
            for (const Expression& child : value.mChildren)
                writeExpression(stream, child);
            stream << ')';
        }

        void writeDeclaration(std::ostringstream& stream, const VariableDeclaration& value)
        {
            stream << "(V";
            atom(stream, toString(value.mType));
            atom(stream, value.mName);
            stream << ')';
        }

        void writeStatements(std::ostringstream& stream, const std::vector<Statement>& statements);

        void writeStatement(std::ostringstream& stream, const Statement& value)
        {
            stream << '(';
            switch (value.mKind)
            {
                case StatementKind::VariableDeclaration:
                    stream << 'V';
                    if (value.mDeclaration)
                    {
                        atom(stream, toString(value.mDeclaration->mType));
                        atom(stream, value.mDeclaration->mName);
                    }
                    break;
                case StatementKind::Set:
                    stream << 'S';
                    if (value.mTarget)
                        writeExpression(stream, *value.mTarget);
                    if (value.mValue)
                        writeExpression(stream, *value.mValue);
                    break;
                case StatementKind::If:
                    stream << 'I';
                    for (const Clause& clause : value.mClauses)
                    {
                        stream << "(K" << (clause.mCondition ? '1' : '0');
                        if (clause.mCondition)
                            writeExpression(stream, *clause.mCondition);
                        writeStatements(stream, clause.mBody);
                        stream << ')';
                    }
                    break;
                case StatementKind::Return:
                    stream << 'R';
                    break;
                case StatementKind::Expression:
                    stream << 'E';
                    if (value.mValue)
                        writeExpression(stream, *value.mValue);
                    break;
                case StatementKind::StrayKeyword:
                    stream << 'K';
                    atom(stream, value.mKeyword);
                    break;
                case StatementKind::Junk:
                    stream << 'J';
                    break;
            }
            stream << ')';
        }

        void writeStatements(std::ostringstream& stream, const std::vector<Statement>& statements)
        {
            stream << '[';
            for (const Statement& statement : statements)
                writeStatement(stream, statement);
            stream << ']';
        }
    }

    FrontendError::FrontendError(Diagnostic diagnostic)
        : std::runtime_error(diagnostic.mMessage)
        , mDiagnostic(std::move(diagnostic))
    {
    }

    std::string_view toString(VariableType value)
    {
        switch (value)
        {
            case VariableType::Short:
                return "short";
            case VariableType::Integer:
                return "int";
            case VariableType::Long:
                return "long";
            case VariableType::Float:
                return "float";
            case VariableType::Reference:
                return "ref";
        }
        return "invalid";
    }

    std::string canonical(const Script& value)
    {
        std::ostringstream stream;
        stream << "(S" << (value.mName ? '1' : '0');
        if (value.mName)
            atom(stream, *value.mName);
        stream << '[';
        for (const VariableDeclaration& declaration : value.mVariables)
            writeDeclaration(stream, declaration);
        stream << "][";
        for (const EventBlock& block : value.mBlocks)
        {
            stream << "(B";
            atom(stream, block.mEvent);
            stream << '[';
            for (const Expression& argument : block.mArguments)
                writeExpression(stream, argument);
            stream << ']';
            writeStatements(stream, block.mBody);
            stream << ')';
        }
        stream << ']';
        writeStatements(stream, value.mStray);
        stream << ')';
        return stream.str();
    }

    std::string canonical(const Expression& value)
    {
        std::ostringstream stream;
        writeExpression(stream, value);
        return stream.str();
    }

    std::string fingerprint(std::string_view value)
    {
        std::uint64_t hash = 14695981039346656037ull;
        for (const unsigned char byte : value)
        {
            hash ^= byte;
            hash *= 1099511628211ull;
        }
        std::ostringstream stream;
        stream << "fnv1a64:" << std::hex << std::setw(16) << std::setfill('0') << hash;
        return stream.str();
    }

    std::string fingerprint(const std::vector<std::uint8_t>& value)
    {
        if (value.empty())
            return fingerprint(std::string_view{});
        return fingerprint(std::string_view(reinterpret_cast<const char*>(value.data()), value.size()));
    }

    std::string lowerCase(std::string_view value)
    {
        std::string result(value);
        for (char& ch : result)
        {
            if (ch >= 'A' && ch <= 'Z')
                ch = static_cast<char>(ch - 'A' + 'a');
        }
        return result;
    }
}
