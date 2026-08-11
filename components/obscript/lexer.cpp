#include "lexer.hpp"

#include <array>
#include <unordered_set>

namespace ObScript
{
    namespace
    {
        bool isAlpha(char value)
        {
            return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') || value == '_';
        }

        bool isDigit(char value)
        {
            return value >= '0' && value <= '9';
        }

        bool isName(char value)
        {
            return isAlpha(value) || isDigit(value);
        }

        bool isJunk(char value)
        {
            constexpr std::string_view values = "`'!:?@#$^&|[]{}~\\";
            return values.find(value) != std::string_view::npos;
        }

        const std::unordered_set<std::string> sKeywords{
            "scn", "scriptname", "begin", "end", "if", "elseif", "else", "endif", "set", "to",
            "return", "short", "int", "long", "float", "ref",
        };
    }

    std::vector<Token> Lexer::tokenize(std::string_view source) const
    {
        std::vector<Token> result;
        std::size_t offset = 0;
        std::uint32_t line = 1;
        std::uint32_t column = 1;
        const auto location = [&]() { return SourceLocation{ offset, line, column }; };
        const auto add = [&](TokenKind kind, std::size_t start, std::size_t size, SourceLocation at) {
            result.push_back({ kind, std::string(source.substr(start, size)), at });
        };
        auto advance = [&](std::size_t count) {
            offset += count;
            column += static_cast<std::uint32_t>(count);
        };

        while (offset < source.size())
        {
            const char ch = source[offset];
            if (ch == ';')
            {
                while (offset < source.size() && source[offset] != '\n' && source[offset] != '\r')
                    advance(1);
                continue;
            }
            if (ch == '\r' || ch == '\n')
            {
                const SourceLocation at = location();
                if (ch == '\r' && offset + 1 < source.size() && source[offset + 1] == '\n')
                    offset += 2;
                else
                    ++offset;
                result.push_back({ TokenKind::Newline, "\n", at });
                ++line;
                column = 1;
                continue;
            }
            if (ch == ' ' || ch == '\t')
            {
                advance(1);
                continue;
            }

            const SourceLocation at = location();
            if (isDigit(ch))
            {
                const std::size_t start = offset;
                while (offset < source.size() && isDigit(source[offset]))
                    advance(1);
                if (offset < source.size() && isAlpha(source[offset]))
                {
                    while (offset < source.size() && isName(source[offset]))
                        advance(1);
                    add(TokenKind::Name, start, offset - start, at);
                    continue;
                }
                if (offset < source.size() && source[offset] == '.')
                {
                    const std::size_t dot = offset;
                    advance(1);
                    while (offset < source.size() && isDigit(source[offset]))
                        advance(1);
                    if (offset == source.size() || !isAlpha(source[offset]))
                    {
                        add(TokenKind::Float, start, offset - start, at);
                        continue;
                    }
                    offset = dot;
                    column = at.mColumn + static_cast<std::uint32_t>(dot - start);
                }
                add(TokenKind::Integer, start, offset - start, at);
                continue;
            }
            if (ch == '.' && offset + 1 < source.size() && isDigit(source[offset + 1]))
            {
                const std::size_t start = offset;
                advance(1);
                while (offset < source.size() && isDigit(source[offset]))
                    advance(1);
                if (offset == source.size() || !isAlpha(source[offset]))
                {
                    add(TokenKind::Float, start, offset - start, at);
                    continue;
                }
                offset = start;
                column = at.mColumn;
            }
            if (ch == '"')
            {
                const std::size_t start = offset;
                advance(1);
                while (offset < source.size() && source[offset] != '"' && source[offset] != '\r'
                    && source[offset] != '\n')
                    advance(1);
                if (offset >= source.size() || source[offset] != '"')
                    throw FrontendError({ DiagnosticSeverity::Error, "OBSL001", "Unterminated string literal", at });
                advance(1);
                add(TokenKind::String, start + 1, offset - start - 2, at);
                continue;
            }

            constexpr std::array<std::string_view, 7> multi{ "&&", "||", "==", "!=", "<=", ">=", ":=" };
            bool matched = false;
            for (const std::string_view value : multi)
            {
                if (source.substr(offset, value.size()) == value)
                {
                    add(TokenKind::Operator, offset, value.size(), at);
                    advance(value.size());
                    matched = true;
                    break;
                }
            }
            if (matched)
                continue;
            if (std::string_view("<>+-*/%(),.=").find(ch) != std::string_view::npos)
            {
                add(TokenKind::Operator, offset, 1, at);
                advance(1);
                continue;
            }
            if (isJunk(ch))
            {
                do
                    advance(1);
                while (offset < source.size() && isJunk(source[offset]));
                continue;
            }
            if (isAlpha(ch))
            {
                const std::size_t start = offset;
                while (offset < source.size() && isName(source[offset]))
                    advance(1);
                const std::string value(source.substr(start, offset - start));
                const std::string lower = lowerCase(value);
                if (sKeywords.contains(lower))
                    result.push_back({ TokenKind::Keyword, lower, at });
                else
                    result.push_back({ TokenKind::Name, value, at });
                continue;
            }

            throw FrontendError({ DiagnosticSeverity::Error, "OBSL002",
                "Unexpected character at byte " + std::to_string(offset), at });
        }

        result.push_back({ TokenKind::Newline, "\n", { offset, line, column } });
        result.push_back({ TokenKind::End, {}, { offset, line, column } });
        return result;
    }
}
