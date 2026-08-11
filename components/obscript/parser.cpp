#include "parser.hpp"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <iterator>
#include <limits>

namespace ObScript
{
    namespace
    {
        bool isDeclarationKeyword(std::string_view value)
        {
            return value == "short" || value == "int" || value == "long" || value == "float" || value == "ref";
        }

        VariableType variableType(std::string_view value)
        {
            if (value == "short")
                return VariableType::Short;
            if (value == "int")
                return VariableType::Integer;
            if (value == "long")
                return VariableType::Long;
            if (value == "float")
                return VariableType::Float;
            return VariableType::Reference;
        }

        Expression node(ExpressionKind kind, const SourceLocation& location, std::string value = {})
        {
            Expression result;
            result.mKind = kind;
            result.mLocation = location;
            result.mValue = std::move(value);
            return result;
        }

        Expression binary(Token op, Expression left, Expression right)
        {
            Expression result = node(ExpressionKind::Binary, op.mLocation, std::move(op.mValue));
            result.mChildren.push_back(std::move(left));
            result.mChildren.push_back(std::move(right));
            return result;
        }
    }

    Script Parser::parse(std::string_view source)
    {
        return parse(Lexer{}.tokenize(source));
    }

    Script Parser::parse(std::vector<Token> tokens)
    {
        mTokens = std::move(tokens);
        mIndex = 0;
        Script result;
        skipNewlines();
        if (peek().mKind == TokenKind::Keyword && (peek().mValue == "scn" || peek().mValue == "scriptname"))
        {
            take();
            result.mName = expect(TokenKind::Name).mValue;
            skipRestOfLine();
            endOfLine();
        }
        while (peek().mKind != TokenKind::End)
        {
            skipNewlines();
            if (peek().mKind == TokenKind::End)
                break;
            if (peek().mKind == TokenKind::Keyword && isDeclarationKeyword(peek().mValue))
            {
                const Token type = take();
                const Token name = expect(TokenKind::Name);
                result.mVariables.push_back({ variableType(type.mValue), name.mValue, type.mLocation });
                skipRestOfLine();
                endOfLine();
            }
            else if (isKeyword("begin"))
                result.mBlocks.push_back(parseBlock());
            else
                result.mStray.push_back(parseStatement());
        }
        return result;
    }

    const Token& Parser::peek(std::size_t offset) const
    {
        return mTokens[std::min(mIndex + offset, mTokens.size() - 1)];
    }

    Token Parser::take()
    {
        const Token result = peek();
        if (result.mKind != TokenKind::End)
            ++mIndex;
        return result;
    }

    std::optional<Token> Parser::accept(TokenKind kind, std::string_view value)
    {
        if (peek().mKind != kind || (!value.empty() && lowerCase(peek().mValue) != value))
            return std::nullopt;
        return take();
    }

    Token Parser::expect(TokenKind kind, std::string_view value)
    {
        if (std::optional<Token> result = accept(kind, value))
            return std::move(*result);
        std::string expected = value.empty() ? std::to_string(static_cast<unsigned>(kind)) : std::string(value);
        throw FrontendError({ DiagnosticSeverity::Error, "OBSP001",
            "Expected " + expected + ", got '" + peek().mValue + "'", peek().mLocation });
    }

    void Parser::skipNewlines()
    {
        while (accept(TokenKind::Newline))
        {
        }
    }

    void Parser::skipRestOfLine()
    {
        while (peek().mKind != TokenKind::Newline && peek().mKind != TokenKind::End)
            take();
    }

    void Parser::endOfLine()
    {
        if (peek().mKind == TokenKind::End)
            return;
        expect(TokenKind::Newline);
        skipNewlines();
    }

    bool Parser::isKeyword(std::string_view value) const
    {
        return peek().mKind == TokenKind::Keyword && peek().mValue == value;
    }

    EventBlock Parser::parseBlock()
    {
        const Token begin = expect(TokenKind::Keyword, "begin");
        Token event;
        if (peek().mKind == TokenKind::Name)
            event = take();
        else
            event = expect(TokenKind::Keyword);
        EventBlock result{ event.mValue, begin.mLocation, {}, {} };
        while (peek().mKind != TokenKind::Newline && peek().mKind != TokenKind::End)
        {
            if (!accept(TokenKind::Operator, ","))
                result.mArguments.push_back(parseExpression());
        }
        endOfLine();
        result.mBody = parseStatements({ "end" });
        expect(TokenKind::Keyword, "end");
        skipRestOfLine();
        endOfLine();
        return result;
    }

    std::vector<Statement> Parser::parseStatements(const std::set<std::string>& until)
    {
        std::vector<Statement> result;
        while (true)
        {
            skipNewlines();
            if (peek().mKind == TokenKind::End
                || (peek().mKind == TokenKind::Keyword && until.contains(peek().mValue)))
                return result;
            result.push_back(parseStatement());
        }
    }

    Statement Parser::parseStatement()
    {
        const Token& first = peek();
        if (first.mKind == TokenKind::Keyword)
        {
            if (first.mValue == "endif" || first.mValue == "elseif" || first.mValue == "else")
            {
                const Token keyword = take();
                skipRestOfLine();
                endOfLine();
                Statement result;
                result.mKind = StatementKind::StrayKeyword;
                result.mLocation = keyword.mLocation;
                result.mKeyword = keyword.mValue;
                return result;
            }
            if (first.mValue == "set")
                return parseSet();
            if (first.mValue == "if")
                return parseIf();
            if (first.mValue == "return")
            {
                const Token value = take();
                skipRestOfLine();
                endOfLine();
                Statement result;
                result.mKind = StatementKind::Return;
                result.mLocation = value.mLocation;
                return result;
            }
            if (isDeclarationKeyword(first.mValue))
            {
                const Token type = take();
                const Token name = expect(TokenKind::Name);
                skipRestOfLine();
                endOfLine();
                Statement result;
                result.mKind = StatementKind::VariableDeclaration;
                result.mLocation = type.mLocation;
                result.mDeclaration = VariableDeclaration{ variableType(type.mValue), name.mValue, type.mLocation };
                return result;
            }
        }
        if (first.mKind == TokenKind::Operator && first.mValue != "(")
        {
            const SourceLocation at = first.mLocation;
            skipRestOfLine();
            endOfLine();
            Statement result;
            result.mKind = StatementKind::Junk;
            result.mLocation = at;
            return result;
        }
        if (first.mKind == TokenKind::Name && peek(1).mKind == TokenKind::Operator && peek(1).mValue == "."
            && peek(2).mKind == TokenKind::Keyword && peek(2).mValue == "set")
        {
            const Token baseToken = take();
            Expression base = node(ExpressionKind::Name, baseToken.mLocation, baseToken.mValue);
            take();
            const Token set = take();
            Expression target = node(ExpressionKind::Member, set.mLocation);
            target.mChildren.push_back(std::move(base));
            target.mChildren.push_back(parsePostfix());
            expect(TokenKind::Keyword, "to");
            Expression value = parseExpression();
            endOfLine();
            Statement result;
            result.mKind = StatementKind::Set;
            result.mLocation = set.mLocation;
            result.mTarget = std::move(target);
            result.mValue = std::move(value);
            return result;
        }
        Expression expression = parseCommandLine();
        const SourceLocation at = expression.mLocation;
        endOfLine();
        Statement result;
        result.mKind = StatementKind::Expression;
        result.mLocation = at;
        result.mValue = std::move(expression);
        return result;
    }

    Statement Parser::parseSet()
    {
        const Token set = expect(TokenKind::Keyword, "set");
        Expression target = parsePostfix();
        expect(TokenKind::Keyword, "to");
        Expression value = parseExpression();
        endOfLine();
        Statement result;
        result.mKind = StatementKind::Set;
        result.mLocation = set.mLocation;
        result.mTarget = std::move(target);
        result.mValue = std::move(value);
        return result;
    }

    Statement Parser::parseIf()
    {
        const Token keyword = expect(TokenKind::Keyword, "if");
        Expression condition = parseExpression();
        endOfLine();
        Statement result;
        result.mKind = StatementKind::If;
        result.mLocation = keyword.mLocation;
        result.mClauses.push_back(
            { std::move(condition), parseStatements({ "elseif", "else", "endif", "end" }), keyword.mLocation });
        while (true)
        {
            if (isKeyword("elseif"))
            {
                const Token clause = take();
                Expression next = parseExpression();
                endOfLine();
                result.mClauses.push_back({ std::move(next),
                    parseStatements({ "elseif", "else", "endif", "end" }), clause.mLocation });
            }
            else if (isKeyword("else"))
            {
                const Token clause = take();
                if (isKeyword("if"))
                {
                    take();
                    Expression next = parseExpression();
                    endOfLine();
                    result.mClauses.push_back({ std::move(next),
                        parseStatements({ "elseif", "else", "endif", "end" }), clause.mLocation });
                }
                else
                {
                    skipRestOfLine();
                    endOfLine();
                    result.mClauses.push_back(
                        { std::nullopt, parseStatements({ "endif", "end" }), clause.mLocation });
                }
            }
            else
                break;
        }
        if (isKeyword("endif"))
        {
            take();
            skipRestOfLine();
            endOfLine();
        }
        return result;
    }

    Expression Parser::parseCommandLine()
    {
        Expression result = parseExpression();
        std::vector<Expression> extra;
        while (peek().mKind != TokenKind::Newline && peek().mKind != TokenKind::End)
        {
            if (!accept(TokenKind::Operator, ","))
                extra.push_back(parseExpression());
        }
        if (!extra.empty())
        {
            if (result.mKind != ExpressionKind::Call)
            {
                Expression call = node(ExpressionKind::Call, result.mLocation);
                call.mChildren.push_back(std::move(result));
                result = std::move(call);
            }
            result.mChildren.insert(result.mChildren.end(), std::make_move_iterator(extra.begin()),
                std::make_move_iterator(extra.end()));
        }
        return result;
    }

    Expression Parser::parseExpression()
    {
        return parseOr();
    }

    Expression Parser::parseOr()
    {
        Expression result = parseAnd();
        while (std::optional<Token> op = accept(TokenKind::Operator, "||"))
            result = binary(std::move(*op), std::move(result), parseAnd());
        return result;
    }

    Expression Parser::parseAnd()
    {
        Expression result = parseComparison();
        while (std::optional<Token> op = accept(TokenKind::Operator, "&&"))
            result = binary(std::move(*op), std::move(result), parseComparison());
        return result;
    }

    Expression Parser::parseComparison()
    {
        const auto isComparison = [](const Token& value) {
            return value.mKind == TokenKind::Operator
                && (value.mValue == "==" || value.mValue == "!=" || value.mValue == "<" || value.mValue == ">"
                    || value.mValue == "<=" || value.mValue == ">=");
        };
        Expression result;
        if (isComparison(peek()))
            result = node(ExpressionKind::Missing, peek().mLocation);
        else
            result = parseAddition();
        while (isComparison(peek()))
        {
            Token op = take();
            result = binary(std::move(op), std::move(result), parseAddition());
        }
        return result;
    }

    Expression Parser::parseAddition()
    {
        Expression result = parseMultiplication();
        while (peek().mKind == TokenKind::Operator && (peek().mValue == "+" || peek().mValue == "-"))
        {
            Token op = take();
            result = binary(std::move(op), std::move(result), parseMultiplication());
        }
        return result;
    }

    Expression Parser::parseMultiplication()
    {
        Expression result = parseUnary();
        while (peek().mKind == TokenKind::Operator
            && (peek().mValue == "*" || peek().mValue == "/" || peek().mValue == "%"))
        {
            Token op = take();
            result = binary(std::move(op), std::move(result), parseUnary());
        }
        return result;
    }

    Expression Parser::parseUnary()
    {
        if (std::optional<Token> op = accept(TokenKind::Operator, "-"))
        {
            Expression result = node(ExpressionKind::Negate, op->mLocation);
            result.mChildren.push_back(parseUnary());
            return result;
        }
        return parseCall();
    }

    Expression Parser::parseCall()
    {
        Expression result = parsePostfix();
        std::vector<Expression> arguments;
        while (true)
        {
            if (peek().mKind == TokenKind::Operator && peek().mValue == "," && !arguments.empty())
            {
                take();
                continue;
            }
            if (peek().mKind == TokenKind::Name || peek().mKind == TokenKind::Integer
                || peek().mKind == TokenKind::Float || peek().mKind == TokenKind::String)
            {
                arguments.push_back(parsePostfix());
                continue;
            }
            break;
        }
        if (!arguments.empty())
        {
            Expression call = node(ExpressionKind::Call, result.mLocation);
            call.mChildren.push_back(std::move(result));
            call.mChildren.insert(call.mChildren.end(), std::make_move_iterator(arguments.begin()),
                std::make_move_iterator(arguments.end()));
            result = std::move(call);
        }
        return result;
    }

    Expression Parser::parsePostfix()
    {
        Expression result = parsePrimary();
        while (accept(TokenKind::Operator, "."))
        {
            Expression member;
            if (peek().mKind == TokenKind::Keyword)
            {
                const Token value = take();
                member = node(ExpressionKind::Name, value.mLocation, value.mValue);
            }
            else
                member = parsePrimary();
            Expression next = node(ExpressionKind::Member, result.mLocation);
            next.mChildren.push_back(std::move(result));
            next.mChildren.push_back(std::move(member));
            result = std::move(next);
        }
        return result;
    }

    Expression Parser::parsePrimary()
    {
        const Token token = peek();
        if (token.mKind == TokenKind::Integer)
        {
            take();
            Expression result = node(ExpressionKind::Integer, token.mLocation);
            const auto [end, error] = std::from_chars(
                token.mValue.data(), token.mValue.data() + token.mValue.size(), result.mInteger);
            if (error != std::errc{} || end != token.mValue.data() + token.mValue.size())
                throw FrontendError(
                    { DiagnosticSeverity::Error, "OBSP002", "Integer literal is out of range", token.mLocation });
            return result;
        }
        if (token.mKind == TokenKind::Float)
        {
            take();
            Expression result = node(ExpressionKind::Float, token.mLocation);
            char* end = nullptr;
            result.mFloat = std::strtod(token.mValue.c_str(), &end);
            if (end != token.mValue.c_str() + token.mValue.size())
                throw FrontendError(
                    { DiagnosticSeverity::Error, "OBSP003", "Invalid floating-point literal", token.mLocation });
            return result;
        }
        if (token.mKind == TokenKind::String)
        {
            take();
            return node(ExpressionKind::String, token.mLocation, token.mValue);
        }
        if (accept(TokenKind::Operator, "("))
        {
            Expression result = parseExpression();
            std::vector<Expression> arguments;
            while (peek().mKind != TokenKind::Newline && peek().mKind != TokenKind::End
                && !(peek().mKind == TokenKind::Operator && peek().mValue == ")"))
            {
                if (!accept(TokenKind::Operator, ","))
                    arguments.push_back(parseExpression());
            }
            expect(TokenKind::Operator, ")");
            if (!arguments.empty())
            {
                Expression call = node(ExpressionKind::Call, result.mLocation);
                call.mChildren.push_back(std::move(result));
                call.mChildren.insert(call.mChildren.end(), std::make_move_iterator(arguments.begin()),
                    std::make_move_iterator(arguments.end()));
                result = std::move(call);
            }
            return result;
        }
        if (token.mKind == TokenKind::Name || (token.mKind == TokenKind::Keyword && token.mValue == "to"))
        {
            take();
            return node(ExpressionKind::Name, token.mLocation, token.mValue);
        }
        throw FrontendError({ DiagnosticSeverity::Error, "OBSP004",
            "Unexpected token '" + token.mValue + "' in expression", token.mLocation });
    }
}
