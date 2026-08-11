#include "obscript.hpp"

#include "arguments.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <components/esm4/reader.hpp>
#include <components/esm4/readerutils.hpp>
#include <components/esm4/records.hpp>
#include <components/files/conversion.hpp>
#include <components/files/openfile.hpp>
#include <components/misc/strings/lower.hpp>
#include <components/obscript/ast.hpp>
#include <components/obscript/compiler.hpp>
#include <components/obscript/corpus.hpp>
#include <components/obscript/scda.hpp>
#include <components/toutf8/toutf8.hpp>

namespace EsmTool
{
    namespace
    {
        std::string jsonEscape(std::string_view value)
        {
            std::ostringstream stream;
            for (const unsigned char c : value)
            {
                switch (c)
                {
                    case '"':
                        stream << "\\\"";
                        break;
                    case '\\':
                        stream << "\\\\";
                        break;
                    case '\b':
                        stream << "\\b";
                        break;
                    case '\f':
                        stream << "\\f";
                        break;
                    case '\n':
                        stream << "\\n";
                        break;
                    case '\r':
                        stream << "\\r";
                        break;
                    case '\t':
                        stream << "\\t";
                        break;
                    default:
                        if (c < 0x20)
                            stream << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                                   << static_cast<unsigned>(c) << std::dec;
                        else
                            stream << static_cast<char>(c);
                }
            }
            return stream.str();
        }

        void appendAtom(std::string& result, std::string_view value)
        {
            result += std::to_string(value.size());
            result += ':';
            result.append(value);
        }

        void appendPayload(std::string& result, const std::optional<std::vector<std::uint8_t>>& value)
        {
            result += value ? '1' : '0';
            if (!value)
                return;
            result += std::to_string(value->size());
            result += ':';
            if (!value->empty())
                result.append(reinterpret_cast<const char*>(value->data()), value->size());
        }

        std::string optionalFingerprint(const std::optional<std::vector<std::uint8_t>>& value)
        {
            return value ? ObScript::fingerprint(*value) : std::string{};
        }

        std::string pluginName(const std::filesystem::path& value)
        {
            return Files::pathToUnicodeString(value.filename());
        }

        void scanCorpus(const std::vector<std::filesystem::path>& files, ObScript::Corpus& corpus)
        {
            std::map<std::string, int> fileToModIndex;
            for (std::size_t i = 0; i < files.size(); ++i)
            {
                const std::string normalized = Misc::StringUtils::lowerCase(pluginName(files[i]));
                if (!fileToModIndex.emplace(normalized, static_cast<int>(i)).second)
                    throw std::runtime_error("Duplicate ObScript corpus plugin name: " + normalized);
            }

            const ToUTF8::StatelessUtf8Encoder encoder(ToUTF8::WINDOWS_1252);
            for (std::size_t i = 0; i < files.size(); ++i)
            {
                auto stream = Files::openBinaryInputFileStream(files[i]);
                if (!stream->is_open())
                    throw std::runtime_error("Unable to open ObScript corpus plugin: "
                        + Files::pathToUnicodeString(files[i]));
                ESM4::Reader reader(std::move(stream), files[i], nullptr, &encoder, true);
                reader.setModIndex(static_cast<int>(i));
                reader.updateModIndices(fileToModIndex);
                const std::string revision = pluginName(files[i]);
                auto recordVisitor = [&](ESM4::Reader& value) {
                    switch (value.hdr().record.typeId)
                    {
                        case ESM4::REC_SCPT:
                        {
                            value.getRecordData();
                            ESM4::Script script;
                            script.load(value);
                            corpus.add(script, revision);
                            return true;
                        }
                        case ESM4::REC_INFO:
                        {
                            value.getRecordData();
                            ESM4::DialogInfo info;
                            info.load(value);
                            corpus.add(info, revision);
                            return true;
                        }
                        case ESM4::REC_QUST:
                        {
                            value.getRecordData();
                            ESM4::Quest quest;
                            quest.load(value);
                            corpus.add(quest, revision);
                            return true;
                        }
                        default:
                            return false;
                    }
                };
                ESM4::ReaderUtils::readAll(reader, recordVisitor, [](ESM4::Reader&) {});
            }
            corpus.finalize();
        }

        struct ScdaStats
        {
            std::size_t mDecoded = 0;
            std::size_t mHeaderSizeMatches = 0;
            std::size_t mStructureMatches = 0;
            std::size_t mInstructionCount = 0;
            std::size_t mIntegerLiterals = 0;
            std::size_t mFloatLiterals = 0;
            std::size_t mLocalReferences = 0;
            std::size_t mFormReferences = 0;
        };

        struct UnitAudit
        {
            const ObScript::ScriptUnit* mUnit = nullptr;
            ObScript::CompilationResult mCompilation;
            bool mCacheStable = false;
            bool mScdaDecoded = false;
            bool mScdaHeaderSizeMatches = false;
            bool mScdaStructureMatches = false;
            std::string mScdaError;
        };
    }

    int obscriptTes4(const Arguments& info)
    {
        try
        {
            ObScript::Corpus corpus;
            scanCorpus(info.filenames, corpus);

            ObScript::CoverageRegistry coverage;
            ObScript::CompilationCache cache(&coverage);
            const ObScript::BytecodeDecoder bytecode;
            std::vector<UnitAudit> audits;
            audits.reserve(corpus.units().size());
            ScdaStats scda;
            std::size_t failures = 0;
            std::size_t sourceOnly = 0;
            std::size_t compiledOnly = 0;
            std::size_t sourceBytes = 0;
            std::size_t compiledBytes = 0;
            std::size_t referenceCount = 0;
            std::string corpusCanonical;

            for (const ObScript::ScriptUnit& unit : corpus.units())
            {
                UnitAudit audit;
                audit.mUnit = &unit;
                appendAtom(corpusCanonical, unit.mId.serialize());
                corpusCanonical += 'R';
                corpusCanonical += std::to_string(unit.mDefinition.globalReferences.size());
                corpusCanonical += ':';
                for (const ESM::FormKey& reference : unit.mDefinition.globalReferences)
                    appendAtom(corpusCanonical, reference.serialize());
                referenceCount += unit.mDefinition.globalReferences.size();
                appendPayload(corpusCanonical, unit.mDefinition.sourceData);
                appendPayload(corpusCanonical, unit.mDefinition.compiledData);
                if (unit.mDefinition.sourceData)
                    sourceBytes += unit.mDefinition.sourceData->size();
                if (unit.mDefinition.compiledData)
                    compiledBytes += unit.mDefinition.compiledData->size();
                if (unit.mDefinition.sourceData && !unit.mDefinition.compiledData)
                    ++sourceOnly;
                if (!unit.mDefinition.sourceData && unit.mDefinition.compiledData)
                    ++compiledOnly;

                audit.mCompilation = cache.compile(unit);
                const ObScript::CompilationResult repeat = cache.compile(unit);
                audit.mCacheStable = repeat.mCacheHit
                    && repeat.mSourceFingerprint == audit.mCompilation.mSourceFingerprint
                    && repeat.mReferenceFingerprint == audit.mCompilation.mReferenceFingerprint
                    && repeat.mAstFingerprint == audit.mCompilation.mAstFingerprint
                    && repeat.mProgramFingerprint == audit.mCompilation.mProgramFingerprint
                    && repeat.mDiagnostics == audit.mCompilation.mDiagnostics;
                if (!audit.mCacheStable || !audit.mCompilation.mDiagnostics.empty())
                    ++failures;

                if (unit.mDefinition.compiledData)
                {
                    try
                    {
                        const std::optional<std::uint32_t> headerSize = unit.mDefinition.hasHeader
                            ? std::optional<std::uint32_t>(unit.mDefinition.scriptHeader.compiledSize)
                            : std::nullopt;
                        if (audit.mCompilation.mAst)
                        {
                            const ObScript::BytecodeComparison comparison = bytecode.compare(
                                *audit.mCompilation.mAst, *unit.mDefinition.compiledData, headerSize);
                            audit.mScdaDecoded = comparison.mDecoded;
                            audit.mScdaHeaderSizeMatches = comparison.mHeaderSizeMatches;
                            audit.mScdaStructureMatches = comparison.structureMatches();
                            scda.mInstructionCount += comparison.mInstructions.size();
                            scda.mIntegerLiterals += comparison.mAtoms.mIntegerLiterals;
                            scda.mFloatLiterals += comparison.mAtoms.mFloatLiterals;
                            scda.mLocalReferences += comparison.mAtoms.mLocalReferences;
                            scda.mFormReferences += comparison.mAtoms.mFormReferences;
                        }
                        else
                        {
                            const auto instructions = bytecode.decode(*unit.mDefinition.compiledData);
                            audit.mScdaDecoded = true;
                            audit.mScdaHeaderSizeMatches = !headerSize
                                || *headerSize == unit.mDefinition.compiledData->size();
                            scda.mInstructionCount += instructions.size();
                        }
                        scda.mDecoded += audit.mScdaDecoded;
                        scda.mHeaderSizeMatches += audit.mScdaHeaderSizeMatches;
                        scda.mStructureMatches += audit.mScdaStructureMatches;
                        if (!audit.mScdaDecoded || !audit.mScdaHeaderSizeMatches)
                            ++failures;
                    }
                    catch (const std::exception& error)
                    {
                        audit.mScdaError = error.what();
                        ++failures;
                    }
                }
                audits.push_back(std::move(audit));
            }

            std::filesystem::create_directories(info.outname.parent_path().empty()
                    ? std::filesystem::current_path()
                    : info.outname.parent_path());
            std::ofstream report(info.outname);
            if (!report)
                throw std::runtime_error("Unable to write ObScript corpus report");

            report << "{\n  \"schema_version\": 1,\n  \"plugins\": [";
            for (std::size_t i = 0; i < info.filenames.size(); ++i)
            {
                if (i != 0)
                    report << ',';
                report << "\n    \"" << jsonEscape(Files::pathToUnicodeString(info.filenames[i])) << '"';
            }
            report << "\n  ],\n  \"unit_count\": " << corpus.units().size()
                   << ",\n  \"source_count\": " << corpus.sourceCount()
                   << ",\n  \"compiled_count\": " << corpus.compiledCount()
                   << ",\n  \"source_only_count\": " << sourceOnly
                   << ",\n  \"compiled_only_count\": " << compiledOnly
                   << ",\n  \"source_payload_bytes\": " << sourceBytes
                   << ",\n  \"compiled_payload_bytes\": " << compiledBytes
                   << ",\n  \"reference_count\": " << referenceCount
                   << ",\n  \"corpus_fingerprint\": \"" << ObScript::fingerprint(corpusCanonical) << "\",\n"
                   << "  \"frontend_failures\": " << failures << ",\n  \"cache_entries\": " << cache.size()
                   << ",\n  \"contexts\": {";
            bool firstContext = true;
            for (const auto& [context, count] : corpus.contextCounts())
            {
                if (!firstContext)
                    report << ',';
                report << "\n    \"" << ObScript::toString(context) << "\": " << count;
                firstContext = false;
            }
            report << "\n  },\n  \"scda\": {\"decoded\": " << scda.mDecoded
                   << ", \"header_size_matches\": " << scda.mHeaderSizeMatches
                   << ", \"structure_matches\": " << scda.mStructureMatches
                   << ", \"instruction_count\": " << scda.mInstructionCount
                   << ", \"integer_literals\": " << scda.mIntegerLiterals
                   << ", \"float_literals\": " << scda.mFloatLiterals
                   << ", \"local_references\": " << scda.mLocalReferences
                   << ", \"form_references\": " << scda.mFormReferences << "},\n  \"coverage\": [";
            std::size_t coverageIndex = 0;
            for (const auto& [name, entry] : coverage.entries())
            {
                if (coverageIndex++ != 0)
                    report << ',';
                report << "\n    {\"name\": \"" << jsonEscape(name) << "\", \"command_uses\": "
                       << entry.mCommandUses << ", \"condition_uses\": " << entry.mConditionUses
                       << ", \"contexts\": [";
                std::size_t contextIndex = 0;
                for (const ObScript::ExecutionContext context : entry.mContexts)
                {
                    if (contextIndex++ != 0)
                        report << ", ";
                    report << '"' << ObScript::toString(context) << '"';
                }
                report << "]}";
            }
            report << "\n  ],\n  \"units\": [";
            for (std::size_t i = 0; i < audits.size(); ++i)
            {
                const UnitAudit& audit = audits[i];
                const ObScript::ScriptUnit& unit = *audit.mUnit;
                if (i != 0)
                    report << ',';
                report << "\n    {\"id\": \"" << jsonEscape(unit.mId.serialize()) << "\", \"owner\": \""
                       << unit.mId.mOwner.serialize() << "\", \"plugin\": \""
                       << jsonEscape(unit.mId.mRevisionPlugin) << "\", \"context\": \""
                       << ObScript::toString(unit.mId.mContext) << "\", \"stage\": ";
                if (unit.mId.mStage)
                    report << *unit.mId.mStage;
                else
                    report << "null";
                report << ", \"stage_entry\": ";
                if (unit.mId.mStageEntry)
                    report << *unit.mId.mStageEntry;
                else
                    report << "null";
                report << ", \"ordinal\": " << unit.mId.mOrdinal << ", \"editor_id\": \""
                       << jsonEscape(unit.mEditorId) << "\", \"references\": [";
                for (std::size_t j = 0; j < unit.mDefinition.globalReferences.size(); ++j)
                {
                    if (j != 0)
                        report << ", ";
                    report << '"' << unit.mDefinition.globalReferences[j].serialize() << '"';
                }
                report << "], \"source_payload_fingerprint\": ";
                if (unit.mDefinition.sourceData)
                    report << '"' << optionalFingerprint(unit.mDefinition.sourceData) << '"';
                else
                    report << "null";
                report << ", \"compiled_payload_fingerprint\": ";
                if (unit.mDefinition.compiledData)
                    report << '"' << optionalFingerprint(unit.mDefinition.compiledData) << '"';
                else
                    report << "null";
                report << ", \"source_fingerprint\": \"" << audit.mCompilation.mSourceFingerprint
                       << "\", \"reference_fingerprint\": \""
                       << audit.mCompilation.mReferenceFingerprint
                       << "\", \"ast_fingerprint\": \"" << audit.mCompilation.mAstFingerprint
                       << "\", \"program_fingerprint\": \"" << audit.mCompilation.mProgramFingerprint
                       << "\", \"cache_stable\": " << (audit.mCacheStable ? "true" : "false")
                       << ", \"scda_decoded\": " << (audit.mScdaDecoded ? "true" : "false")
                       << ", \"scda_header_size_matches\": "
                       << (audit.mScdaHeaderSizeMatches ? "true" : "false")
                       << ", \"scda_structure_matches\": "
                       << (audit.mScdaStructureMatches ? "true" : "false") << ", \"scda_error\": \""
                       << jsonEscape(audit.mScdaError) << "\", \"diagnostics\": [";
                for (std::size_t j = 0; j < audit.mCompilation.mDiagnostics.size(); ++j)
                {
                    const ObScript::CompilationDiagnostic& diagnostic = audit.mCompilation.mDiagnostics[j];
                    if (j != 0)
                        report << ',';
                    report << "{\"severity\": \""
                           << (diagnostic.mDiagnostic.mSeverity == ObScript::DiagnosticSeverity::Error ? "error"
                                                                                                      : "warning")
                           << "\", \"code\": \"" << diagnostic.mDiagnostic.mCode << "\", \"message\": \""
                           << jsonEscape(diagnostic.mDiagnostic.mMessage) << "\", \"line\": "
                           << diagnostic.mDiagnostic.mLocation.mLine << ", \"column\": "
                           << diagnostic.mDiagnostic.mLocation.mColumn << '}';
                }
                report << "], \"source\": ";
                if (unit.mDefinition.sourceData)
                    report << '"' << jsonEscape(unit.mDefinition.scriptSource) << '"';
                else
                    report << "null";
                report << '}';
            }
            report << "\n  ]\n}\n";
            if (!report)
                throw std::runtime_error("Unable to finish ObScript corpus report");

            std::cout << "TES4 ObScript corpus: units=" << corpus.units().size() << " source="
                      << corpus.sourceCount() << " compiled=" << corpus.compiledCount() << " coverage="
                      << coverage.entries().size() << " failures=" << failures << " fingerprint="
                      << ObScript::fingerprint(corpusCanonical) << '\n';
            return failures == 0 && compiledOnly == 0 ? 0 : 2;
        }
        catch (const std::exception& error)
        {
            std::cerr << "TES4 ObScript corpus audit failed: " << error.what() << '\n';
            return -1;
        }
    }
}
