#ifndef OPENMW_COMPONENTS_OBSCRIPT_LEXER_H
#define OPENMW_COMPONENTS_OBSCRIPT_LEXER_H

#include "ast.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace ObScript
{
    enum class TokenKind : std::uint8_t
    {
        Newline,
        Integer,
        Float,
        String,
        Name,
        Keyword,
        Operator,
        End,
    };

    struct Token
    {
        TokenKind mKind = TokenKind::End;
        std::string mValue;
        SourceLocation mLocation;

        friend bool operator==(const Token&, const Token&) = default;
    };

    class Lexer
    {
    public:
        std::vector<Token> tokenize(std::string_view source) const;
    };
}

#endif
