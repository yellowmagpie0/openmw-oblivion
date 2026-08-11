#include <components/esm4/common.hpp>
#include <components/esm4/loadinfo.hpp>
#include <components/esm4/loadqust.hpp>
#include <components/esm4/reader.hpp>
#include <components/esm4/readerutils.hpp>
#include <components/files/istreamptr.hpp>
#include <components/obscript/compiler.hpp>
#include <components/obscript/corpus.hpp>
#include <components/obscript/lexer.hpp>
#include <components/obscript/parser.hpp>
#include <components/obscript/scda.hpp>
#include <components/obscript/vm.hpp>
#include <components/toutf8/toutf8.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    class TestRuntimeHost final : public ObScript::RuntimeHost
    {
    public:
        std::map<std::string, ObScript::Value, std::less<>> mExternal;
        std::map<std::pair<std::string, std::string>, ObScript::Value> mMembers;
        std::vector<std::string> mCalls;
        std::function<void(const ObScript::RuntimeContext&)> mOnNested;

        ObScript::Value resolveName(std::string_view name, const ObScript::RuntimeContext&) override
        {
            if (const auto found = mExternal.find(name); found != mExternal.end())
                return found->second;
            return ObScript::ReferenceValue{ ESM::FormKey::content("memory.esm", 2), std::string(name) };
        }

        ObScript::Value loadMember(
            const ObScript::Value& target, std::string_view name, const ObScript::RuntimeContext&) override
        {
            const auto key = std::pair(ObScript::valueString(target), std::string(name));
            if (const auto found = mMembers.find(key); found != mMembers.end())
                return found->second;
            return std::int64_t{};
        }

        void storeExternal(
            std::string_view name, const ObScript::Value& value, const ObScript::RuntimeContext&) override
        {
            mExternal[std::string(name)] = value;
        }

        void storeMember(const ObScript::Value& target, std::string_view name, const ObScript::Value& value,
            const ObScript::RuntimeContext&) override
        {
            mMembers[{ ObScript::valueString(target), std::string(name) }] = value;
        }

        ObScript::Value call(std::string_view name, const std::optional<ObScript::Value>& target,
            const std::vector<ObScript::Value>& arguments, const ObScript::RuntimeContext& context,
            const ObScript::SourceLocation&) override
        {
            mCalls.push_back(std::string(name));
            if (name == "nested" && mOnNested)
            {
                mOnNested(context);
                return std::int64_t{};
            }
            if (name == "bump")
            {
                const auto key = std::pair(ObScript::valueString(*target), std::string("value"));
                mMembers[key] = ObScript::asInteger(mMembers[key]) + ObScript::asInteger(arguments.at(0));
                return std::int64_t{ 1 };
            }
            if (name == "unsupported")
                throw ObScript::RuntimeError("OBSV100", "unsupported test command", std::string(name));
            return std::int64_t{};
        }
    };

    template <class T>
    void append(std::vector<char>& result, const T& value)
    {
        const auto* begin = reinterpret_cast<const char*>(&value);
        result.insert(result.end(), begin, begin + sizeof(value));
    }

    void appendSubRecord(std::vector<char>& result, std::uint32_t type, const std::vector<char>& data)
    {
        append(result, type);
        append(result, static_cast<std::uint16_t>(data.size()));
        result.insert(result.end(), data.begin(), data.end());
    }

    template <class T>
    void appendSubRecord(std::vector<char>& result, std::uint32_t type, const T& data)
    {
        std::vector<char> bytes;
        append(bytes, data);
        appendSubRecord(result, type, bytes);
    }

    void appendRecord(std::vector<char>& result, std::uint32_t type, std::uint32_t id, const std::vector<char>& data)
    {
        append(result, type);
        append(result, static_cast<std::uint32_t>(data.size()));
        append(result, std::uint32_t{ 0 });
        append(result, id);
        append(result, std::uint32_t{ 0 });
        result.insert(result.end(), data.begin(), data.end());
    }

    std::vector<char> makePluginWithRepeatedScripts()
    {
        std::vector<char> result;
        std::vector<char> header;
        std::vector<char> hedr;
        append(hedr, 1.0f);
        append(hedr, std::int32_t{ 3 });
        append(hedr, std::uint32_t{ 0x800 });
        appendSubRecord(header, ESM::fourCC("HEDR"), hedr);
        appendRecord(result, ESM4::REC_TES4, 0, header);

        const std::string source = "begin gamemode\r\nreturn\r\nend\r\n";
        const std::vector<char> sourceBytes(source.begin(), source.end());
        const std::vector<char> bytecode{ 0x10, 0, 0, 0, 0x1e, 0, 0, 0, 0x11, 0, 0, 0 };
        ESM4::ScriptHeader scriptHeader{};
        scriptHeader.compiledSize = bytecode.size();
        scriptHeader.refCount = 1;
        const std::uint32_t reference = 0x42;

        std::vector<char> info;
        for (unsigned i = 0; i < 2; ++i)
        {
            appendSubRecord(info, ESM::fourCC("SCHR"), scriptHeader);
            appendSubRecord(info, ESM::fourCC("SCDA"), bytecode);
            appendSubRecord(info, ESM::fourCC("SCTX"), sourceBytes);
            appendSubRecord(info, ESM::fourCC("SCRO"), reference);
        }
        appendRecord(result, ESM4::REC_INFO, 0x1234, info);

        std::vector<char> quest;
        appendSubRecord(quest, ESM::fourCC("EDID"), std::vector<char>{ 'T', 'e', 's', 't', 'Q', 0 });
        const std::uint16_t stage = 20;
        appendSubRecord(quest, ESM::fourCC("INDX"), stage);
        for (unsigned i = 0; i < 2; ++i)
        {
            appendSubRecord(quest, ESM::fourCC("QSDT"), std::vector<char>{ 0 });
            appendSubRecord(quest, ESM::fourCC("SCHR"), scriptHeader);
            appendSubRecord(quest, ESM::fourCC("SCDA"), bytecode);
            appendSubRecord(quest, ESM::fourCC("SCTX"), sourceBytes);
            appendSubRecord(quest, ESM::fourCC("SCRO"), reference);
        }
        appendRecord(result, ESM4::REC_QUST, 0x2345, quest);
        return result;
    }

    struct LoadedEmbeddedScripts
    {
        ESM4::DialogInfo mInfo;
        ESM4::Quest mQuest;
    };

    LoadedEmbeddedScripts readEmbeddedScripts()
    {
        const std::vector<char> data = makePluginWithRepeatedScripts();
        auto stream = std::make_unique<std::stringstream>(
            std::string(data.begin(), data.end()), std::ios::in | std::ios::binary);
        const ToUTF8::StatelessUtf8Encoder encoder(ToUTF8::WINDOWS_1252);
        ESM4::Reader reader(std::move(stream), "memory.esm", nullptr, &encoder, true);
        LoadedEmbeddedScripts result;
        auto recordVisitor = [&](ESM4::Reader& value) {
            value.getRecordData();
            if (value.hdr().record.typeId == ESM4::REC_INFO)
                result.mInfo.load(value);
            else if (value.hdr().record.typeId == ESM4::REC_QUST)
                result.mQuest.load(value);
            else
                return false;
            return true;
        };
        ESM4::ReaderUtils::readAll(reader, recordVisitor, [](ESM4::Reader&) {});
        return result;
    }

    ObScript::ScriptUnit makeUnit(std::uint32_t ordinal, std::string source)
    {
        ObScript::ScriptUnit result;
        result.mId.mOwner = ESM::FormKey::content("memory.esm", 0x1234);
        result.mId.mRevisionPlugin = "memory.esm";
        result.mId.mContext = ObScript::ExecutionContext::DialogueResult;
        result.mId.mOrdinal = ordinal;
        result.mDefinition.scriptSource = source;
        result.mDefinition.sourceData = std::vector<std::uint8_t>(source.begin(), source.end());
        return result;
    }

    TEST(ObScriptFrontend, whitespaceAndLineEndingsDoNotChangeAstOrIr)
    {
        const std::vector<std::string> sources{
            "scn Test\nshort count\nbegin GameMode\nset count to 1 + 2 * 3\nif GetDisabled == 0\nEnable\nendif\nend\n",
            "\r\nscn Test ; comment\r\n\tshort\tcount\r\nbegin GameMode\r\n"
            " set count to (1 + (2 * 3))\r\nif GetDisabled==0\r\n Enable\r\nendif\r\nend\r\n",
        };
        const ObScript::Script first = ObScript::Parser{}.parse(sources[0]);
        const ObScript::Script second = ObScript::Parser{}.parse(sources[1]);
        EXPECT_EQ(ObScript::canonical(first), ObScript::canonical(second));

        ObScript::ScriptUnitId id;
        id.mOwner = ESM::FormKey::content("memory.esm", 1);
        id.mRevisionPlugin = "memory.esm";
        const ObScript::Program firstProgram = ObScript::Compiler{}.compile(id, first);
        const ObScript::Program secondProgram = ObScript::Compiler{}.compile(id, second);
        EXPECT_EQ(ObScript::canonical(firstProgram), ObScript::canonical(secondProgram));
        EXPECT_EQ(ObScript::fingerprint(ObScript::canonical(first)), "fnv1a64:19eaf376f5f36ea7");
        EXPECT_EQ(ObScript::fingerprint(ObScript::canonical(firstProgram)), "fnv1a64:94e1c309fb0c1d26");
    }

    TEST(ObScriptVm, executesTypedLocalsControlFlowAndImmediateHostMutation)
    {
        const ObScript::Script script = ObScript::Parser{}.parse(R"(
scn VmTest
short count
float elapsed
begin GameMode
set count to 1.9
set elapsed to count + 0.5
set globalValue to 10
if globalValue == 10
  target.Set value to 7
  target.bump 2
endif
end
)");
        ObScript::ScriptUnitId id;
        id.mOwner = ESM::FormKey::content("memory.esm", 1);
        id.mRevisionPlugin = "memory.esm";
        const ObScript::Program program = ObScript::Compiler{}.compile(id, script);
        std::vector<ObScript::Value> locals = ObScript::VirtualMachine::makeLocals(program);
        TestRuntimeHost host;
        ObScript::RuntimeContext context;
        context.mUnit = id;
        context.mEvent = "gamemode";
        const ObScript::ExecutionReport report
            = ObScript::VirtualMachine{}.execute(program, program.mEntryPoints.at(0), locals, host, context);
        ASSERT_TRUE(report.mCompleted);
        EXPECT_TRUE(report.mDiagnostics.empty());
        EXPECT_EQ(ObScript::asInteger(locals.at(0)), 1);
        EXPECT_DOUBLE_EQ(ObScript::asNumber(locals.at(1)), 1.5);
        EXPECT_EQ(ObScript::asInteger(host.mExternal.at("globalvalue")), 10);
        ASSERT_EQ(host.mMembers.size(), 1);
        EXPECT_EQ(ObScript::asInteger(host.mMembers.begin()->second), 9);
        ASSERT_EQ(host.mCalls.size(), 1);
        EXPECT_EQ(host.mCalls[0], "bump");
    }

    TEST(ObScriptVm, reportsHostFailuresWithStableProgramContext)
    {
        const ObScript::Script script
            = ObScript::Parser{}.parse("scn VmTest\nbegin OnActivate\nunsupported\nend\n");
        ObScript::ScriptUnitId id;
        id.mOwner = ESM::FormKey::content("memory.esm", 3);
        id.mRevisionPlugin = "memory.esm";
        const ObScript::Program program = ObScript::Compiler{}.compile(id, script);
        std::vector<ObScript::Value> locals = ObScript::VirtualMachine::makeLocals(program);
        TestRuntimeHost host;
        ObScript::RuntimeContext context;
        context.mUnit = id;
        context.mEvent = "onactivate";
        context.mSequence = 42;
        const ObScript::ExecutionReport report
            = ObScript::VirtualMachine{}.execute(program, program.mEntryPoints.at(0), locals, host, context);
        ASSERT_FALSE(report.mCompleted);
        ASSERT_EQ(report.mDiagnostics.size(), 1);
        EXPECT_EQ(report.mDiagnostics[0].mCode, "OBSV100");
        EXPECT_EQ(report.mDiagnostics[0].mCommand, "unsupported");
        EXPECT_EQ(report.mDiagnostics[0].mSequence, 42);
        EXPECT_EQ(report.mDiagnostics[0].mUnit, id);
    }

    TEST(ObScriptVm, nullReferenceLocalsCompareEqualToNumericZero)
    {
        const auto unit = makeUnit(7, "ref target\nbegin gamemode\nif target != 0\nfail\nendif\nend");
        const auto result = ObScript::CompilationCache{}.compile(unit);
        ASSERT_TRUE(result.mProgram);
        auto locals = ObScript::VirtualMachine::makeLocals(*result.mProgram);
        TestRuntimeHost host;
        ObScript::RuntimeContext context;
        context.mUnit = result.mProgram->mUnit;
        context.mEvent = "gamemode";
        const auto report = ObScript::VirtualMachine{}.execute(*result.mProgram,
            result.mProgram->mEntryPoints.front(), locals, host, context);
        EXPECT_TRUE(report.mCompleted);
        EXPECT_TRUE(report.mDiagnostics.empty());
        EXPECT_TRUE(host.mCalls.empty());
    }

    TEST(ObScriptVm, nestedExecutionCompletesBeforeCallerResumes)
    {
        const auto parentUnit = makeUnit(8, "begin gamemode\nnested\nset observed to shared\nend");
        const auto childUnit = makeUnit(9, "begin onactivate\nset shared to 11\nend");
        const auto parent = ObScript::CompilationCache{}.compile(parentUnit);
        const auto child = ObScript::CompilationCache{}.compile(childUnit);
        ASSERT_TRUE(parent.mProgram);
        ASSERT_TRUE(child.mProgram);
        auto parentLocals = ObScript::VirtualMachine::makeLocals(*parent.mProgram);
        auto childLocals = ObScript::VirtualMachine::makeLocals(*child.mProgram);
        TestRuntimeHost host;
        ObScript::VirtualMachine vm;
        ObScript::ExecutionReport nestedReport;
        host.mOnNested = [&](const ObScript::RuntimeContext& parentContext) {
            ObScript::RuntimeContext childContext;
            childContext.mUnit = child.mProgram->mUnit;
            childContext.mEvent = "onactivate";
            childContext.mSequence = parentContext.mSequence + 1;
            childContext.mDepth = parentContext.mDepth + 1;
            nestedReport = vm.execute(*child.mProgram, child.mProgram->mEntryPoints.front(),
                childLocals, host, childContext);
        };
        ObScript::RuntimeContext context;
        context.mUnit = parent.mProgram->mUnit;
        context.mEvent = "gamemode";
        context.mSequence = 40;
        context.mDepth = 1;
        const auto report = vm.execute(*parent.mProgram,
            parent.mProgram->mEntryPoints.front(), parentLocals, host, context);
        ASSERT_TRUE(nestedReport.mCompleted);
        EXPECT_TRUE(nestedReport.mDiagnostics.empty());
        ASSERT_TRUE(report.mCompleted);
        EXPECT_TRUE(report.mDiagnostics.empty());
        EXPECT_EQ(ObScript::asInteger(host.mExternal.at("shared")), 11);
        EXPECT_EQ(ObScript::asInteger(host.mExternal.at("observed")), 11);
        ASSERT_EQ(host.mCalls.size(), 1);
        EXPECT_EQ(host.mCalls.front(), "nested");
    }

    TEST(ObScriptVm, integerBoundaryArithmeticIsDeterministic)
    {
        const auto unit = makeUnit(10, R"(
long value
long quotient
long remainder
begin gamemode
set value to 9223372036854775807
set value to value + 1
set quotient to value / -1
set remainder to value % -1
end
)");
        const auto result = ObScript::CompilationCache{}.compile(unit);
        ASSERT_TRUE(result.mProgram);
        auto locals = ObScript::VirtualMachine::makeLocals(*result.mProgram);
        TestRuntimeHost host;
        ObScript::RuntimeContext context;
        context.mUnit = result.mProgram->mUnit;
        context.mEvent = "gamemode";
        const auto report = ObScript::VirtualMachine{}.execute(*result.mProgram,
            result.mProgram->mEntryPoints.front(), locals, host, context);
        ASSERT_TRUE(report.mCompleted);
        EXPECT_TRUE(report.mDiagnostics.empty());
        EXPECT_EQ(ObScript::asInteger(locals.at(0)), std::numeric_limits<std::int64_t>::min());
        EXPECT_EQ(ObScript::asInteger(locals.at(1)), std::numeric_limits<std::int64_t>::min());
        EXPECT_EQ(ObScript::asInteger(locals.at(2)), 0);
        EXPECT_EQ(ObScript::asInteger(static_cast<double>(std::numeric_limits<std::int64_t>::max())),
            std::numeric_limits<std::int64_t>::max());
    }

    TEST(ObScriptFrontend, parsesVanillaLabelsReferencesAndControlFlowQuirks)
    {
        const ObScript::Script script = ObScript::Parser{}.parse(R"(
scn 1stScript
ref target
begin OnActivate, player
target.Set linkedTarget to player
if == 0
  Activate target
else if target.GetDisabled
  target.Enable
else
  return
endif
endif
end
)");
        ASSERT_EQ(script.mBlocks.size(), 1);
        ASSERT_EQ(script.mBlocks[0].mBody.size(), 3);
        EXPECT_EQ(script.mBlocks[0].mBody[0].mKind, ObScript::StatementKind::Set);
        EXPECT_EQ(script.mBlocks[0].mBody[1].mKind, ObScript::StatementKind::If);
        ASSERT_EQ(script.mBlocks[0].mBody[1].mClauses.size(), 3);
        EXPECT_EQ(script.mBlocks[0].mBody[1].mClauses[0].mCondition->mChildren[0].mKind,
            ObScript::ExpressionKind::Missing);
        EXPECT_EQ(script.mBlocks[0].mBody[2].mKind, ObScript::StatementKind::StrayKeyword);
    }

    TEST(ObScriptFrontend, semanticAnalysisTypesLocalsAndRecordsCoverage)
    {
        const ObScript::Script script = ObScript::Parser{}.parse(R"(
scn Test
short count
float elapsed
ref target
begin GameMode
set count to 1.5
set elapsed to count + 0.25
if target.GetDisabled && GetSecondsPassed > 0
  target.Enable
endif
end
)");
        ObScript::ScriptUnitId id;
        id.mOwner = ESM::FormKey::content("memory.esm", 1);
        id.mRevisionPlugin = "memory.esm";
        ObScript::CoverageRegistry coverage;
        const ObScript::Program program = ObScript::Compiler(&coverage).compile(id, script);
        ASSERT_EQ(program.mLocals.size(), 3);
        ASSERT_EQ(program.mEntryPoints.size(), 1);
        EXPECT_NE(std::find_if(program.mEntryPoints[0].mCode.begin(), program.mEntryPoints[0].mCode.end(),
                      [](const ObScript::Instruction& value) {
                          return value.mOpcode == ObScript::OpCode::Convert
                              && value.mType == ObScript::ValueType::Short;
                      }),
            program.mEntryPoints[0].mCode.end());
        EXPECT_EQ(coverage.entries().at("getdisabled").mConditionUses, 1);
        EXPECT_EQ(coverage.entries().at("getsecondspassed").mConditionUses, 1);
        EXPECT_EQ(coverage.entries().at("enable").mCommandUses, 1);
    }

    TEST(ObScriptFrontend, diagnosticsAreStructuredAndDeterministic)
    {
        const std::string source = "scn Test\nbegin gamemode\nset (1 + 2) to 3\nend\n";
        const ObScript::ScriptUnit first = makeUnit(0, source);
        ObScript::CompilationCache cache;
        const ObScript::CompilationResult failed = cache.compile(first);
        ASSERT_EQ(failed.mDiagnostics.size(), 1);
        EXPECT_EQ(failed.mDiagnostics[0].mDiagnostic.mCode, "OBSS002");
        EXPECT_EQ(failed.mDiagnostics[0].mDiagnostic.mLocation.mLine, 3);
        const ObScript::CompilationResult cached = cache.compile(first);
        EXPECT_TRUE(cached.mCacheHit);
        EXPECT_EQ(cached.mDiagnostics[0].mDiagnostic, failed.mDiagnostics[0].mDiagnostic);
    }

    TEST(ObScriptFrontend, preservesVanillaDuplicateDeclarationsInSourceOrder)
    {
        const ObScript::Script script = ObScript::Parser{}.parse(
            "scn Test\nshort timer\nfloat timer\nbegin gamemode\nset timer to 1.5\nend\n");
        ObScript::ScriptUnitId id;
        id.mOwner = ESM::FormKey::content("memory.esm", 1);
        id.mRevisionPlugin = "memory.esm";
        const ObScript::Program program = ObScript::Compiler{}.compile(id, script);
        ASSERT_EQ(program.mLocals.size(), 2);
        EXPECT_EQ(program.mLocals[0].mType, ObScript::VariableType::Short);
        EXPECT_EQ(program.mLocals[1].mType, ObScript::VariableType::Float);
        EXPECT_EQ(program.mEntryPoints[0].mCode.back().mIndex, 0);
    }

    TEST(ObScriptFrontend, cacheIdentitySeparatesRepeatedPayloads)
    {
        ObScript::CompilationCache cache;
        ObScript::ScriptUnit first = makeUnit(0, "begin gamemode\nreturn\nend\n");
        first.mDefinition.globalReferences.push_back(ESM::FormKey::content("memory.esm", 0x42));
        const ObScript::ScriptUnit second = makeUnit(1, "begin gamemode\nreturn\nend\n");
        const ObScript::CompilationResult firstResult = cache.compile(first);
        EXPECT_FALSE(firstResult.mCacheHit);
        ASSERT_TRUE(firstResult.mProgram);
        EXPECT_EQ(firstResult.mProgram->mReferences, first.mDefinition.globalReferences);
        EXPECT_FALSE(cache.compile(second).mCacheHit);
        EXPECT_TRUE(cache.compile(first).mCacheHit);
        EXPECT_EQ(cache.size(), 2);
        ObScript::ScriptUnit changed = first;
        changed.mDefinition.globalReferences[0] = ESM::FormKey::content("memory.esm", 0x43);
        EXPECT_THROW(cache.compile(changed), std::runtime_error);
    }

    TEST(ObScriptFrontend, decodesOrdinaryAndReferenceQualifiedScda)
    {
        const std::vector<std::uint8_t> data{
            0x10, 0, 0, 0,
            0x1c, 0, 3, 0, 0x15, 0, 3, 0, 'r', 1, 0,
            0x11, 0, 0, 0,
        };
        const ObScript::BytecodeDecoder decoder;
        const std::vector<ObScript::DecodedBytecodeInstruction> decoded = decoder.decode(data);
        ASSERT_EQ(decoded.size(), 3);
        EXPECT_EQ(decoded[1].mCallingReference, 3);
        EXPECT_EQ(decoded[1].mOpcode, 0x15);
        EXPECT_EQ(decoder.decodeAtoms(decoded).mFormReferences, 2);
        std::vector<std::uint8_t> truncated = data;
        truncated.pop_back();
        EXPECT_THROW(decoder.decode(truncated), std::runtime_error);
    }

    TEST(ObScriptFrontend, boundedMalformedAndMutationCorporaFailStructurally)
    {
        const std::string original
            = "scn MutationProbe\nshort value\nbegin gamemode\nif GetDisabled == 0\nset value to 1\nendif\nend\n";
        for (std::size_t offset = 0; offset < original.size(); ++offset)
        {
            std::string mutated = original;
            mutated[offset] ^= static_cast<char>(0x5a);
            try
            {
                static_cast<void>(ObScript::Parser{}.parse(mutated));
            }
            catch (const ObScript::FrontendError& error)
            {
                EXPECT_FALSE(error.diagnostic().mCode.empty());
                EXPECT_GE(error.diagnostic().mLocation.mLine, 1);
            }
        }
        const std::string invalidByte(1, static_cast<char>(0xff));
        try
        {
            static_cast<void>(ObScript::Lexer{}.tokenize(invalidByte));
            FAIL() << "Invalid source byte was accepted";
        }
        catch (const ObScript::FrontendError& error)
        {
            EXPECT_EQ(error.diagnostic().mCode, "OBSL002");
        }

        const std::vector<std::uint8_t> bytecode{
            0x10, 0, 0, 0,
            0x1c, 0, 3, 0, 0x15, 0, 3, 0, 'r', 1, 0,
            0x11, 0, 0, 0,
        };
        const ObScript::BytecodeDecoder decoder;
        for (std::size_t size = 0; size <= bytecode.size(); ++size)
        {
            try
            {
                static_cast<void>(decoder.decode(std::span(bytecode).first(size)));
            }
            catch (const std::runtime_error& error)
            {
                EXPECT_FALSE(std::string_view(error.what()).empty());
            }
        }
    }

    TEST(ObScriptCorpus, retainsExactRepeatedInfoAndQuestPayloadsAsSeparateUnits)
    {
        const LoadedEmbeddedScripts loaded = readEmbeddedScripts();
        ASSERT_EQ(loaded.mInfo.mResultScripts.size(), 2);
        ASSERT_EQ(loaded.mQuest.mResultScripts.size(), 2);
        EXPECT_EQ(loaded.mInfo.mResultScripts[0].sourceData, loaded.mInfo.mResultScripts[1].sourceData);
        EXPECT_EQ(loaded.mInfo.mResultScripts[0].compiledData, loaded.mInfo.mResultScripts[1].compiledData);
        ASSERT_EQ(loaded.mInfo.mResultScripts[0].globalReferences.size(), 1);
        EXPECT_EQ(loaded.mInfo.mResultScripts[0].globalReferences[0], ESM::FormKey::content("memory.esm", 0x42));
        EXPECT_EQ(loaded.mQuest.mResultScripts[0].stage, 20);
        EXPECT_EQ(loaded.mQuest.mResultScripts[0].stageEntry, 0);
        EXPECT_EQ(loaded.mQuest.mResultScripts[1].stageEntry, 1);

        ObScript::Corpus corpus;
        corpus.add(loaded.mInfo, "memory.esm");
        corpus.add(loaded.mQuest, "memory.esm");
        corpus.finalize();
        ASSERT_EQ(corpus.units().size(), 4);
        EXPECT_EQ(corpus.sourceCount(), 4);
        EXPECT_EQ(corpus.compiledCount(), 4);
        for (std::size_t i = 1; i < corpus.units().size(); ++i)
            EXPECT_NE(corpus.units()[i - 1].mId.serialize(), corpus.units()[i].mId.serialize());
    }
}
