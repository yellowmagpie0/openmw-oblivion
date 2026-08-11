#ifndef OPENMW_COMPONENTS_OBSCRIPT_COMPILER_H
#define OPENMW_COMPONENTS_OBSCRIPT_COMPILER_H

#include "parser.hpp"
#include "program.hpp"

#include <map>

namespace ObScript
{
    class Compiler
    {
    public:
        explicit Compiler(CoverageRegistry* coverage = nullptr)
            : mCoverage(coverage)
        {
        }

        Program compile(const ScriptUnitId& unit, const Script& script);

    private:
        void collectLocals(const std::vector<Statement>& statements, Program& program);
        void addLocal(const VariableDeclaration& declaration, Program& program);
        ValueType compileExpression(
            const Expression& value, EntryPoint& entry, const Program& program, CoverageRole role);
        void compileStatements(const std::vector<Statement>& statements, EntryPoint& entry, const Program& program);
        void compileStatement(const Statement& statement, EntryPoint& entry, const Program& program);
        std::optional<std::uint32_t> findLocal(std::string_view name, const Program& program) const;
        void recordCall(std::string_view name, CoverageRole role);

        CoverageRegistry* mCoverage = nullptr;
        ScriptUnitId mUnit;
    };

    class CompilationCache
    {
    public:
        explicit CompilationCache(CoverageRegistry* coverage = nullptr)
            : mCoverage(coverage)
        {
        }

        CompilationResult compile(const ScriptUnit& unit);
        std::size_t size() const { return mEntries.size(); }

    private:
        std::map<ScriptUnitId, CompilationResult> mEntries;
        CoverageRegistry* mCoverage = nullptr;
    };
}

#endif
