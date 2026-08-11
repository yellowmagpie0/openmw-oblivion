#ifndef OPENMW_COMPONENTS_OBSCRIPT_PARSER_H
#define OPENMW_COMPONENTS_OBSCRIPT_PARSER_H

#include "ast.hpp"
#include "lexer.hpp"

#include <initializer_list>
#include <set>

namespace ObScript
{
    class Parser
    {
    public:
        Script parse(std::string_view source);
        Script parse(std::vector<Token> tokens);

    private:
        const Token& peek(std::size_t offset = 0) const;
        Token take();
        std::optional<Token> accept(TokenKind kind, std::string_view value = {});
        Token expect(TokenKind kind, std::string_view value = {});
        void skipNewlines();
        void skipRestOfLine();
        void endOfLine();
        bool isKeyword(std::string_view value) const;

        EventBlock parseBlock();
        std::vector<Statement> parseStatements(const std::set<std::string>& until);
        Statement parseStatement();
        Statement parseSet();
        Statement parseIf();
        Expression parseCommandLine();
        Expression parseExpression();
        Expression parseOr();
        Expression parseAnd();
        Expression parseComparison();
        Expression parseAddition();
        Expression parseMultiplication();
        Expression parseUnary();
        Expression parseCall();
        Expression parsePostfix();
        Expression parsePrimary();

        std::vector<Token> mTokens;
        std::size_t mIndex = 0;
    };
}

#endif
