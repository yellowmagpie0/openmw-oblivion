#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QString>

#include <components/compiler/context.hpp>
#include <components/compiler/errorhandler.hpp>
#include <components/compiler/exception.hpp>
#include <components/compiler/extensions.hpp>
#include <components/compiler/extensions0.hpp>
#include <components/compiler/fileparser.hpp>
#include <components/compiler/locals.hpp>
#include <components/compiler/scanner.hpp>
#include <components/compiler/scriptparser.hpp>
#include <components/compiler/tokenloc.hpp>
#include <components/esm/defs.hpp>
#include <components/esm3/dialoguecondition.hpp>
#include <components/esm3/esmreader.hpp>
#include <components/esm3/esmwriter.hpp>
#include <components/esm3/formatversion.hpp>
#include <components/esm3/cellref.hpp>
#include <components/esm3/loadcell.hpp>
#include <components/esm3/loaddial.hpp>
#include <components/esm3/loadinfo.hpp>
#include <components/esm3/loadland.hpp>
#include <components/esm3/loadmisc.hpp>
#include <components/esm3/loadnpc.hpp>
#include <components/esm3/loadscpt.hpp>
#include <components/files/openfile.hpp>
#include <components/toutf8/toutf8.hpp>

namespace
{
    constexpr std::string_view sServerName = "openmw-cs-mcp";
    constexpr std::string_view sServerVersion = "0.3.0";

    QString fromUtf8(std::string_view value)
    {
        return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
    }

    QString refIdString(const ESM::RefId& value)
    {
        return fromUtf8(value.toString());
    }

    std::string lowerAscii(std::string_view value)
    {
        std::string result(value);
        std::transform(result.begin(), result.end(), result.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return result;
    }

    std::string keyFor(const ESM::RefId& value)
    {
        return lowerAscii(value.toString());
    }

    std::string keyFor(const QString& value)
    {
        return lowerAscii(value.toUtf8().toStdString());
    }

    bool containsInsensitive(std::string_view haystack, std::string_view needle)
    {
        return lowerAscii(haystack).find(lowerAscii(needle)) != std::string::npos;
    }

    bool containsInsensitive(const QString& haystack, const QString& needle)
    {
        return haystack.contains(needle, Qt::CaseInsensitive);
    }

    QString matchingExcerpt(const QString& text, const QString& query, qsizetype radius = 500)
    {
        const qsizetype match = text.indexOf(query, 0, Qt::CaseInsensitive);
        if (match < 0)
            return text.left(radius * 2);
        const qsizetype begin = std::max<qsizetype>(0, match - radius);
        const qsizetype length = std::min<qsizetype>(text.size() - begin, radius * 2 + query.size());
        return (begin > 0 ? QString("…") : QString()) + text.mid(begin, length)
            + (begin + length < text.size() ? QString("…") : QString());
    }

    QString unquote(QString value)
    {
        value = value.trimmed();
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
            value = value.mid(1, value.size() - 2);
        return value;
    }

    QString dialogueTypeName(ESM::Dialogue::Type type)
    {
        switch (type)
        {
            case ESM::Dialogue::Topic:
                return "topic";
            case ESM::Dialogue::Voice:
                return "voice";
            case ESM::Dialogue::Greeting:
                return "greeting";
            case ESM::Dialogue::Persuasion:
                return "persuasion";
            case ESM::Dialogue::Journal:
                return "journal";
            default:
                return "unknown";
        }
    }

    QString questStatusName(ESM::DialInfo::QuestStatus status)
    {
        switch (status)
        {
            case ESM::DialInfo::QS_Name:
                return "name";
            case ESM::DialInfo::QS_Finished:
                return "finished";
            case ESM::DialInfo::QS_Restart:
                return "restart";
            default:
                return "none";
        }
    }

    QString conditionFunctionName(ESM::DialogueCondition::Function function)
    {
        using Function = ESM::DialogueCondition::Function;
        switch (function)
        {
            case Function::Function_Global:
                return "global";
            case Function::Function_Local:
                return "local";
            case Function::Function_Journal:
                return "journal";
            case Function::Function_Item:
                return "item";
            case Function::Function_Dead:
                return "dead";
            case Function::Function_NotId:
                return "not_id";
            case Function::Function_NotFaction:
                return "not_faction";
            case Function::Function_NotClass:
                return "not_class";
            case Function::Function_NotRace:
                return "not_race";
            case Function::Function_NotCell:
                return "not_cell";
            case Function::Function_NotLocal:
                return "not_local";
            default:
                return QString("function_%1").arg(static_cast<int>(function));
        }
    }

    QString comparisonName(ESM::DialogueCondition::Comparison comparison)
    {
        using Comparison = ESM::DialogueCondition::Comparison;
        switch (comparison)
        {
            case Comparison::Comp_Eq:
                return "==";
            case Comparison::Comp_Ne:
                return "!=";
            case Comparison::Comp_Gt:
                return ">";
            case Comparison::Comp_Ge:
                return ">=";
            case Comparison::Comp_Ls:
                return "<";
            case Comparison::Comp_Le:
                return "<=";
            default:
                return "none";
        }
    }

    QJsonValue conditionValue(const ESM::DialogueCondition& condition)
    {
        if (const auto* value = std::get_if<int32_t>(&condition.mValue))
            return *value;
        return std::get<float>(condition.mValue);
    }

    QJsonObject conditionJson(const ESM::DialogueCondition& condition)
    {
        return {
            { "index", condition.mIndex },
            { "function", conditionFunctionName(condition.mFunction) },
            { "function_index", static_cast<int>(condition.mFunction) },
            { "variable", fromUtf8(condition.mVariable) },
            { "comparison", comparisonName(condition.mComparison) },
            { "value", conditionValue(condition) },
        };
    }

    QJsonObject infoJson(const ESM::DialInfo& info, const QString& topicId, const QString& source)
    {
        QJsonArray conditions;
        for (const auto& condition : info.mSelects)
            conditions.append(conditionJson(condition));

        QJsonObject result{
            { "topic_id", topicId },
            { "info_id", refIdString(info.mId) },
            { "previous_info_id", refIdString(info.mPrev) },
            { "next_info_id", refIdString(info.mNext) },
            { "dialogue_type", dialogueTypeName(static_cast<ESM::Dialogue::Type>(info.mData.mType)) },
            { "disposition_or_journal_index", info.mData.mDisposition },
            { "quest_status", questStatusName(info.mQuestStatus) },
            { "response", fromUtf8(info.mResponse) },
            { "result_script", fromUtf8(info.mResultScript) },
            { "conditions", conditions },
            { "source", source },
        };

        const auto addRef = [&](const char* name, const ESM::RefId& value) {
            if (!value.empty())
                result.insert(name, refIdString(value));
        };
        addRef("actor", info.mActor);
        addRef("race", info.mRace);
        addRef("class", info.mClass);
        addRef("faction", info.mFaction);
        addRef("player_faction", info.mPcFaction);
        addRef("cell", info.mCell);
        if (!info.mSound.empty())
            result.insert("sound", fromUtf8(info.mSound));
        if (info.mFactionLess)
            result.insert("factionless", true);
        return result;
    }

    struct ContentFile
    {
        std::filesystem::path mPath;
        QString mName;
        std::uintmax_t mSize = 0;
    };

    struct InfoRecord
    {
        ESM::DialInfo mValue;
        QString mSource;
        bool mDeleted = false;
    };

    struct DialogueRecord
    {
        ESM::Dialogue mValue;
        QString mSource;
        bool mDeleted = false;
        std::map<std::string, InfoRecord> mInfos;
    };

    struct ScriptRecord
    {
        ESM::Script mValue;
        QString mSource;
        bool mDeleted = false;
    };

    struct NpcRecord
    {
        ESM::NPC mValue;
        QString mSource;
        bool mDeleted = false;
    };

    struct MiscRecord
    {
        ESM::Miscellaneous mValue;
        QString mSource;
        bool mDeleted = false;
    };

    struct CellReferenceRecord
    {
        ESM::CellRef mValue;
        QString mSource;
        bool mDeleted = false;
        bool mMoved = false;
    };

    struct CellRecord
    {
        ESM::Cell mValue;
        QString mSource;
        bool mDeleted = false;
        std::map<std::string, CellReferenceRecord> mReferences;
    };

    struct LandRecord
    {
        ESM::Land mValue;
        QString mSource;
        bool mDeleted = false;
    };

    struct RawField
    {
        QString mTag;
        QString mText;
    };

    struct RawRecord
    {
        QString mType;
        QString mId;
        QString mSource;
        std::vector<RawField> mFields;
    };

    struct Configuration
    {
        std::vector<std::filesystem::path> mDataPaths;
        std::vector<QString> mContentNames;
        ToUTF8::FromType mEncoding = ToUTF8::WINDOWS_1252;
    };

    Configuration readConfiguration(const std::filesystem::path& path)
    {
        std::ifstream stream(path);
        if (!stream)
            throw std::runtime_error("Could not open OpenMW config: " + path.string());

        Configuration result;
        std::string rawLine;
        while (std::getline(stream, rawLine))
        {
            const QString line = QString::fromUtf8(rawLine).trimmed();
            if (line.isEmpty() || line.startsWith('#'))
                continue;
            const qsizetype separator = line.indexOf('=');
            if (separator < 1)
                continue;
            const QString name = line.left(separator).trimmed();
            const QString value = unquote(line.mid(separator + 1));
            if (name == "data")
                result.mDataPaths.emplace_back(value.toStdString());
            else if (name == "content")
                result.mContentNames.push_back(value);
            else if (name == "encoding")
            {
                const QString lower = value.toLower();
                if (lower == "win1250")
                    result.mEncoding = ToUTF8::WINDOWS_1250;
                else if (lower == "win1251")
                    result.mEncoding = ToUTF8::WINDOWS_1251;
                else
                    result.mEncoding = ToUTF8::WINDOWS_1252;
            }
        }

        if (result.mDataPaths.empty())
            throw std::runtime_error("The OpenMW config contains no data= path");
        if (result.mContentNames.empty())
            throw std::runtime_error("The OpenMW config contains no content= files");
        return result;
    }

    std::filesystem::path resolveContentFile(
        const QString& name, const std::vector<std::filesystem::path>& dataPaths)
    {
        for (auto it = dataPaths.rbegin(); it != dataPaths.rend(); ++it)
        {
            const std::filesystem::path candidate = *it / name.toStdString();
            if (std::filesystem::is_regular_file(candidate))
                return candidate;

            std::error_code ec;
            for (const auto& entry : std::filesystem::directory_iterator(*it, ec))
            {
                if (ec)
                    break;
                if (QString::fromStdString(entry.path().filename().string()).compare(name, Qt::CaseInsensitive) == 0)
                    return entry.path();
            }
        }
        throw std::runtime_error("Could not resolve content file from data paths: " + name.toStdString());
    }

    std::vector<QString> extractPrintableRuns(const std::vector<char>& bytes)
    {
        std::vector<QString> result;
        std::set<QString> seen;
        std::size_t begin = 0;
        while (begin < bytes.size())
        {
            while (begin < bytes.size())
            {
                const unsigned char c = static_cast<unsigned char>(bytes[begin]);
                if (c >= 32 && c != 127)
                    break;
                ++begin;
            }
            std::size_t end = begin;
            bool hasLetter = false;
            while (end < bytes.size())
            {
                const unsigned char c = static_cast<unsigned char>(bytes[end]);
                if (c < 32 || c == 127)
                    break;
                hasLetter = hasLetter || std::isalpha(c) != 0;
                ++end;
            }
            if (end - begin >= 3 && hasLetter)
            {
                QString value = QString::fromLatin1(bytes.data() + begin, static_cast<qsizetype>(end - begin)).trimmed();
                if (!value.isEmpty() && seen.insert(value).second)
                    result.push_back(std::move(value));
            }
            begin = end + 1;
        }
        return result;
    }

    class Database
    {
        ToUTF8::Utf8Encoder mEncoder;
        std::vector<ContentFile> mContentFiles;
        std::map<std::string, DialogueRecord> mDialogues;
        std::map<std::string, ScriptRecord> mScripts;
        std::map<std::string, NpcRecord> mNpcs;
        std::map<std::string, MiscRecord> mMiscellaneous;
        std::map<std::string, CellRecord> mCells;
        std::map<std::pair<int, int>, LandRecord> mLands;
        std::vector<RawRecord> mRawRecords;
        std::set<std::string> mIds;
        std::set<std::string> mGlobalIds;
        std::map<std::string, QString> mIdTypes;
        std::map<std::string, QString> mIdNames;
        std::map<std::string, QString> mIdModels;

        void loadFile(const ContentFile& source, int contentIndex)
        {
            ESM::ESMReader reader;
            reader.setEncoder(&mEncoder);
            reader.setIndex(contentIndex);
            reader.open(source.mPath);
            std::string currentDialogue;

            while (reader.hasMoreRecs())
            {
                const ESM::NAME recordName = reader.getRecName();
                std::uint32_t flags = 0;
                reader.getRecHeader(flags);

                if (recordName.toInt() == ESM::REC_DIAL)
                {
                    ESM::Dialogue value;
                    bool deleted = false;
                    value.load(reader, deleted);
                    currentDialogue = keyFor(value.mId);
                    mIds.insert(currentDialogue);
                    auto& record = mDialogues[currentDialogue];
                    record.mValue = std::move(value);
                    record.mSource = source.mName;
                    record.mDeleted = deleted;
                    continue;
                }

                if (recordName.toInt() == ESM::REC_INFO)
                {
                    ESM::DialInfo value;
                    bool deleted = false;
                    value.load(reader, deleted);
                    mIds.insert(keyFor(value.mId));
                    if (!currentDialogue.empty())
                    {
                        auto& record = mDialogues[currentDialogue].mInfos[keyFor(value.mId)];
                        record.mValue = std::move(value);
                        record.mSource = source.mName;
                        record.mDeleted = deleted;
                    }
                    continue;
                }

                currentDialogue.clear();
                if (recordName.toInt() == ESM::REC_SCPT)
                {
                    ESM::Script value;
                    bool deleted = false;
                    value.load(reader, deleted);
                    auto& record = mScripts[keyFor(value.mId)];
                    record.mValue = std::move(value);
                    record.mSource = source.mName;
                    record.mDeleted = deleted;
                    continue;
                }

                if (recordName.toInt() == ESM::REC_NPC_)
                {
                    ESM::NPC value;
                    bool deleted = false;
                    value.load(reader, deleted);
                    const std::string key = keyFor(value.mId);
                    mIds.insert(key);
                    mIdTypes[key] = "NPC_";
                    mIdNames[key] = fromUtf8(value.mName);
                    mNpcs[key] = { value, source.mName, deleted };
                    RawRecord raw{ "NPC_", refIdString(value.mId), source.mName, {} };
                    raw.mFields = {
                        { "NAME", refIdString(value.mId) }, { "FNAM", fromUtf8(value.mName) },
                        { "RNAM", refIdString(value.mRace) }, { "CNAM", refIdString(value.mClass) },
                        { "ANAM", refIdString(value.mFaction) }, { "BNAM", refIdString(value.mHead) },
                        { "KNAM", refIdString(value.mHair) }, { "SCRI", refIdString(value.mScript) },
                    };
                    mRawRecords.push_back(std::move(raw));
                    continue;
                }

                if (recordName.toInt() == ESM::REC_MISC)
                {
                    ESM::Miscellaneous value;
                    bool deleted = false;
                    value.load(reader, deleted);
                    const std::string key = keyFor(value.mId);
                    mIds.insert(key);
                    mIdTypes[key] = "MISC";
                    mIdNames[key] = fromUtf8(value.mName);
                    mIdModels[key] = fromUtf8(value.mModel.getOriginal());
                    mMiscellaneous[key] = { value, source.mName, deleted };
                    RawRecord raw{ "MISC", refIdString(value.mId), source.mName, {} };
                    raw.mFields = {
                        { "NAME", refIdString(value.mId) }, { "FNAM", fromUtf8(value.mName) },
                        { "MODL", fromUtf8(value.mModel.getOriginal()) },
                        { "ITEX", fromUtf8(value.mIcon.getOriginal()) }, { "SCRI", refIdString(value.mScript) },
                    };
                    mRawRecords.push_back(std::move(raw));
                    continue;
                }

                if (recordName.toInt() == ESM::REC_CELL)
                {
                    ESM::Cell value;
                    bool deleted = false;
                    value.load(reader, deleted, false);
                    RawRecord raw{ "CELL",
                        value.mName.empty() ? refIdString(value.mId) : fromUtf8(value.mName), source.mName, {} };
                    raw.mFields.push_back({ "NAME", fromUtf8(value.mName) });
                    raw.mFields.push_back({ "GRID", QString("%1,%2").arg(value.mData.mX).arg(value.mData.mY) });
                    raw.mFields.push_back({ "RGNN", refIdString(value.mRegion) });

                    std::vector<CellReferenceRecord> references;
                    while (reader.hasMoreSubs())
                    {
                        ESM::CellRef cellRef;
                        ESM::MovedCellRef movedRef;
                        bool refDeleted = false;
                        bool moved = false;
                        if (reader.peekNextSub("FRMR") || reader.peekNextSub("MVRF"))
                        {
                            if (ESM::Cell::getNextRef(reader, cellRef, refDeleted, movedRef, moved))
                            {
                                references.push_back({ cellRef, source.mName, refDeleted, moved });
                                raw.mFields.push_back({ "NAME", refIdString(cellRef.mRefID) });
                                continue;
                            }
                        }
                        reader.getSubName();
                        reader.skipHSub();
                    }

                    const std::string cellKey = keyFor(value.mId);
                    auto& cell = mCells[cellKey];
                    cell.mValue = value;
                    cell.mSource = source.mName;
                    cell.mDeleted = deleted;
                    for (CellReferenceRecord& reference : references)
                    {
                        const ESM::RefNum& refNum = reference.mValue.mRefNum;
                        const std::string refKey
                            = std::to_string(refNum.mContentFile) + ":" + std::to_string(refNum.mIndex);
                        cell.mReferences[refKey] = std::move(reference);
                    }
                    mRawRecords.push_back(std::move(raw));
                    continue;
                }

                if (recordName.toInt() == ESM::REC_LAND)
                {
                    ESM::Land value;
                    bool deleted = false;
                    value.load(reader, deleted);
                    mLands[{ value.mX, value.mY }] = { value, source.mName, deleted };
                    RawRecord raw{ "LAND", QString("#%1 %2").arg(value.mX).arg(value.mY), source.mName, {} };
                    raw.mFields.push_back({ "INTV", QString("%1,%2").arg(value.mX).arg(value.mY) });
                    mRawRecords.push_back(std::move(raw));
                    continue;
                }

                RawRecord raw;
                raw.mType = fromUtf8(recordName.toStringView());
                raw.mSource = source.mName;
                while (reader.hasMoreSubs())
                {
                    reader.getSubName();
                    const ESM::NAME subName = reader.retSubName();
                    reader.getSubHeader();
                    const std::uint32_t size = reader.getSubSize();
                    if (size > 65536)
                    {
                        reader.skip(size);
                        continue;
                    }
                    std::vector<char> bytes(size);
                    if (size > 0)
                        reader.getExact(bytes.data(), bytes.size());
                    const auto runs = extractPrintableRuns(bytes);
                    for (const QString& run : runs)
                    {
                        raw.mFields.push_back({ fromUtf8(subName.toStringView()), run });
                        if (raw.mId.isEmpty() && subName.toInt() == ESM::SREC_NAME)
                            raw.mId = run;
                    }
                }
                if (!raw.mId.isEmpty())
                {
                    const std::string idKey = keyFor(raw.mId);
                    mIds.insert(idKey);
                    mIdTypes[idKey] = raw.mType;
                    for (const RawField& field : raw.mFields)
                    {
                        if (field.mTag == "FNAM")
                            mIdNames[idKey] = field.mText;
                        else if (field.mTag == "MODL")
                            mIdModels[idKey] = field.mText;
                    }
                    if (recordName.toInt() == ESM::REC_GLOB)
                        mGlobalIds.insert(keyFor(raw.mId));
                }
                if (!raw.mFields.empty())
                    mRawRecords.push_back(std::move(raw));
            }
        }

    public:
        explicit Database(const Configuration& configuration)
            : mEncoder(configuration.mEncoding)
        {
            for (const QString& name : configuration.mContentNames)
            {
                ContentFile source;
                source.mPath = resolveContentFile(name, configuration.mDataPaths);
                source.mName = name;
                source.mSize = std::filesystem::file_size(source.mPath);
                mContentFiles.push_back(source);
                loadFile(mContentFiles.back(), static_cast<int>(mContentFiles.size()) - 1);
            }
        }

        const std::vector<ContentFile>& contentFiles() const { return mContentFiles; }
        const std::map<std::string, DialogueRecord>& dialogues() const { return mDialogues; }
        const std::map<std::string, ScriptRecord>& scripts() const { return mScripts; }
        const std::map<std::string, NpcRecord>& npcs() const { return mNpcs; }
        const std::map<std::string, MiscRecord>& miscellaneous() const { return mMiscellaneous; }
        const std::map<std::string, CellRecord>& cells() const { return mCells; }
        const std::map<std::pair<int, int>, LandRecord>& lands() const { return mLands; }
        const std::vector<RawRecord>& rawRecords() const { return mRawRecords; }
        ToUTF8::Utf8Encoder& encoder() { return mEncoder; }
        bool hasId(const ESM::RefId& id) const { return mIds.contains(keyFor(id)); }
        bool hasGlobal(const std::string& id) const { return mGlobalIds.contains(lowerAscii(id)); }
        QString recordType(const ESM::RefId& id) const
        {
            const auto it = mIdTypes.find(keyFor(id));
            return it == mIdTypes.end() ? QString() : it->second;
        }
        QString recordName(const ESM::RefId& id) const
        {
            const auto it = mIdNames.find(keyFor(id));
            return it == mIdNames.end() ? QString() : it->second;
        }
        QString recordModel(const ESM::RefId& id) const
        {
            const auto it = mIdModels.find(keyFor(id));
            return it == mIdModels.end() ? QString() : it->second;
        }

        const DialogueRecord& dialogue(const QString& id) const
        {
            const auto it = mDialogues.find(keyFor(id));
            if (it == mDialogues.end() || it->second.mDeleted)
                throw std::runtime_error("Dialogue or quest not found: " + id.toStdString());
            return it->second;
        }

        const ScriptRecord& script(const QString& id) const
        {
            const auto it = mScripts.find(keyFor(id));
            if (it == mScripts.end() || it->second.mDeleted)
                throw std::runtime_error("Script not found: " + id.toStdString());
            return it->second;
        }

        const NpcRecord& npc(const QString& id) const
        {
            const auto it = mNpcs.find(keyFor(id));
            if (it == mNpcs.end() || it->second.mDeleted)
                throw std::runtime_error("NPC not found: " + id.toStdString());
            return it->second;
        }

        const MiscRecord& misc(const QString& id) const
        {
            const auto it = mMiscellaneous.find(keyFor(id));
            if (it == mMiscellaneous.end() || it->second.mDeleted)
                throw std::runtime_error("MISC not found: " + id.toStdString());
            return it->second;
        }
    };

    class ValidationContext final : public Compiler::Context
    {
        const Database& mDatabase;
        const std::set<std::string>& mPluginIds;

    public:
        ValidationContext(const Database& database, const std::set<std::string>& pluginIds)
            : mDatabase(database)
            , mPluginIds(pluginIds)
        {
        }

        bool canDeclareLocals() const override { return true; }

        char getGlobalType(const std::string& name) const override
        {
            return mDatabase.hasGlobal(name) ? 'l' : ' ';
        }

        std::pair<char, bool> getMemberType(const std::string&, const ESM::RefId& id) const override
        {
            // Resolving the exact attached script requires the live editor model. Treat members of a known
            // reference as longs here; the validator still catches unknown references and all syntax errors.
            return isId(id) ? std::make_pair('l', true) : std::make_pair(' ', false);
        }

        bool isId(const ESM::RefId& id) const override
        {
            const std::string key = keyFor(id);
            return key == "player" || key == "playerref" || mPluginIds.contains(key) || mDatabase.hasId(id);
        }
    };

    class ValidationErrorHandler final : public Compiler::ErrorHandler
    {
        QJsonArray mDiagnostics;

        void report(const std::string& message, const Compiler::TokenLoc& loc, Type type) override
        {
            mDiagnostics.append(QJsonObject{
                { "severity", type == ErrorMessage ? "error" : "warning" },
                { "message", fromUtf8(message) },
                { "literal", fromUtf8(loc.mLiteral) },
                { "line", loc.mLine + 1 },
                { "column", loc.mColumn },
            });
        }

        void report(const std::string& message, Type type) override
        {
            mDiagnostics.append(QJsonObject{
                { "severity", type == ErrorMessage ? "error" : "warning" },
                { "message", fromUtf8(message) },
            });
        }

    public:
        const QJsonArray& diagnostics() const { return mDiagnostics; }
    };

    QJsonObject compileSource(
        const QString& source, bool fullScript, ValidationContext& context, const Compiler::Extensions& extensions)
    {
        ValidationErrorHandler errors;
        errors.setWarningsMode(1);
        bool threw = false;
        QString exception;
        try
        {
            std::istringstream input(source.toStdString() + "\n");
            Compiler::Scanner scanner(errors, input, &extensions);
            if (fullScript)
            {
                Compiler::FileParser parser(errors, context);
                scanner.scan(parser);
            }
            else
            {
                Compiler::Locals locals;
                Compiler::ScriptParser parser(errors, context, locals, false);
                scanner.scan(parser);
            }
        }
        catch (const Compiler::SourceException&)
        {
            threw = true;
        }
        catch (const std::exception& error)
        {
            threw = true;
            exception = fromUtf8(error.what());
        }

        QJsonArray diagnostics = errors.diagnostics();
        if (!exception.isEmpty())
            diagnostics.append(QJsonObject{ { "severity", "error" }, { "message", exception } });
        int errorCount = errors.countErrors() + (exception.isEmpty() ? 0 : 1);
        if (threw && errorCount == 0)
        {
            ++errorCount;
            diagnostics.append(
                QJsonObject{ { "severity", "error" }, { "message", "OpenMW compiler rejected the source" } });
        }
        return {
            { "valid", !threw && errorCount == 0 },
            { "error_count", errorCount },
            { "warning_count", errors.countWarnings() },
            { "diagnostics", diagnostics },
        };
    }

    std::vector<const InfoRecord*> orderedInfos(const DialogueRecord& dialogue)
    {
        std::vector<const InfoRecord*> result;
        std::set<std::string> added;
        const InfoRecord* current = nullptr;
        for (const auto& [key, info] : dialogue.mInfos)
        {
            if (!info.mDeleted && info.mValue.mPrev.empty())
            {
                current = &info;
                break;
            }
        }
        while (current != nullptr)
        {
            const std::string currentKey = keyFor(current->mValue.mId);
            if (!added.insert(currentKey).second)
                break;
            result.push_back(current);
            if (current->mValue.mNext.empty())
                break;
            const auto next = dialogue.mInfos.find(keyFor(current->mValue.mNext));
            current = next != dialogue.mInfos.end() && !next->second.mDeleted ? &next->second : nullptr;
        }
        for (const auto& [key, info] : dialogue.mInfos)
        {
            if (!info.mDeleted && added.insert(key).second)
                result.push_back(&info);
        }
        if (dialogue.mValue.mType == ESM::Dialogue::Journal)
        {
            std::stable_sort(result.begin(), result.end(), [](const InfoRecord* left, const InfoRecord* right) {
                return left->mValue.mData.mJournalIndex < right->mValue.mData.mJournalIndex;
            });
        }
        return result;
    }

    bool infoReferences(const ESM::DialInfo& info, const QString& value)
    {
        if (containsInsensitive(fromUtf8(info.mResultScript), value))
            return true;
        return std::any_of(info.mSelects.begin(), info.mSelects.end(), [&](const ESM::DialogueCondition& condition) {
            return QString::fromUtf8(condition.mVariable).compare(value, Qt::CaseInsensitive) == 0;
        });
    }

    QJsonObject loadOrderJson(const Database& database)
    {
        QJsonArray files;
        for (const auto& content : database.contentFiles())
        {
            files.append(QJsonObject{
                { "name", content.mName },
                { "path", QString::fromStdString(content.mPath.string()) },
                { "size_bytes", static_cast<qint64>(content.mSize) },
            });
        }
        return { { "content_files", files } };
    }

    QJsonObject findQuests(const Database& database, const QJsonObject& arguments)
    {
        const QString query = arguments.value("query").toString();
        const int limit = std::clamp(arguments.value("limit").toInt(50), 1, 200);
        QJsonArray quests;
        for (const auto& [key, dialogue] : database.dialogues())
        {
            if (dialogue.mDeleted || dialogue.mValue.mType != ESM::Dialogue::Journal)
                continue;
            const QString id = refIdString(dialogue.mValue.mId);
            bool match = query.isEmpty() || containsInsensitive(id, query);
            QString title;
            int entryCount = 0;
            for (const InfoRecord* info : orderedInfos(dialogue))
            {
                ++entryCount;
                if (info->mValue.mQuestStatus == ESM::DialInfo::QS_Name)
                    title = fromUtf8(info->mValue.mResponse);
                match = match || containsInsensitive(fromUtf8(info->mValue.mResponse), query);
            }
            if (!match)
                continue;
            quests.append(QJsonObject{
                { "quest_id", id },
                { "title", title.isEmpty() ? id : title },
                { "entry_count", entryCount },
                { "source", dialogue.mSource },
            });
            if (quests.size() >= limit)
                break;
        }
        return { { "quests", quests }, { "count", quests.size() } };
    }

    void collectQuotedTerms(const QString& text, std::set<QString>& result)
    {
        qsizetype start = 0;
        while ((start = text.indexOf('"', start)) >= 0)
        {
            const qsizetype end = text.indexOf('"', start + 1);
            if (end < 0)
                break;
            const QString term = text.mid(start + 1, end - start - 1).trimmed();
            if (term.size() >= 2 && term.size() <= 128)
                result.insert(term);
            start = end + 1;
        }
    }

    std::set<int> journalStagesInScript(const std::string& scriptText, const QString& questId)
    {
        QString normalized = fromUtf8(scriptText);
        normalized.replace('"', ' ');
        normalized.replace(',', ' ');
        const QStringList tokens = normalized.simplified().split(' ', Qt::SkipEmptyParts);
        std::set<int> result;
        for (qsizetype i = 0; i + 2 < tokens.size(); ++i)
        {
            if (tokens[i].compare("Journal", Qt::CaseInsensitive) != 0
                || tokens[i + 1].compare(questId, Qt::CaseInsensitive) != 0)
                continue;
            bool ok = false;
            const int stage = tokens[i + 2].toInt(&ok);
            if (ok)
                result.insert(stage);
        }
        return result;
    }

    QJsonObject getQuest(const Database& database, const QJsonObject& arguments)
    {
        const QString questId = arguments.value("quest_id").toString();
        if (questId.isEmpty())
            throw std::runtime_error("quest_id is required");
        const DialogueRecord& quest = database.dialogue(questId);
        if (quest.mValue.mType != ESM::Dialogue::Journal)
            throw std::runtime_error("The requested dialogue is not a journal/quest: " + questId.toStdString());

        QJsonArray journalEntries;
        QString title;
        for (const InfoRecord* entry : orderedInfos(quest))
        {
            journalEntries.append(infoJson(entry->mValue, refIdString(quest.mValue.mId), entry->mSource));
            if (entry->mValue.mQuestStatus == ESM::DialInfo::QS_Name)
                title = fromUtf8(entry->mValue.mResponse);
        }

        QJsonArray dialogueInfos;
        QJsonArray scripts;
        std::set<QString> searchTerms;
        for (const auto& [key, dialogue] : database.dialogues())
        {
            if (dialogue.mDeleted || dialogue.mValue.mType == ESM::Dialogue::Journal)
                continue;
            for (const InfoRecord* info : orderedInfos(dialogue))
            {
                if (!infoReferences(info->mValue, questId))
                    continue;
                dialogueInfos.append(infoJson(info->mValue, refIdString(dialogue.mValue.mId), info->mSource));
                collectQuotedTerms(fromUtf8(info->mValue.mResultScript), searchTerms);
                if (!info->mValue.mActor.empty())
                    searchTerms.insert(refIdString(info->mValue.mActor));
                for (const auto& condition : info->mValue.mSelects)
                {
                    if (!condition.mVariable.empty()
                        && QString::fromUtf8(condition.mVariable).compare(questId, Qt::CaseInsensitive) != 0)
                        searchTerms.insert(QString::fromUtf8(condition.mVariable));
                }
            }
        }
        for (const auto& [key, script] : database.scripts())
        {
            if (script.mDeleted || !containsInsensitive(script.mValue.mScriptText, questId.toStdString()))
                continue;
            const QString sourceText = fromUtf8(script.mValue.mScriptText);
            scripts.append(QJsonObject{
                { "script_id", refIdString(script.mValue.mId) },
                { "source_text", sourceText },
                { "source", script.mSource },
            });
            collectQuotedTerms(sourceText, searchTerms);
        }
        searchTerms.erase(questId);
        QJsonArray terms;
        for (const QString& term : searchTerms)
            terms.append(term);

        return {
            { "quest_id", refIdString(quest.mValue.mId) },
            { "title", title.isEmpty() ? refIdString(quest.mValue.mId) : title },
            { "source", quest.mSource },
            { "journal_entries", journalEntries },
            { "related_dialogue_infos", dialogueInfos },
            { "related_scripts", scripts },
            { "suggested_record_search_terms", terms },
        };
    }

    QJsonObject getAreaQuests(const Database& database, const QJsonObject& arguments)
    {
        const QString area = arguments.value("area").toString().trimmed();
        const QString scope = arguments.value("scope").toString("starts").toLower();
        if (area.size() < 2)
            throw std::runtime_error("area must contain at least two characters");
        if (scope != "starts" && scope != "all")
            throw std::runtime_error("scope must be starts or all");

        std::set<std::string> npcIds;
        std::set<std::string> recordIds;
        for (const RawRecord& record : database.rawRecords())
        {
            if (!record.mId.isEmpty())
                recordIds.insert(keyFor(record.mId));
            if (record.mType == "NPC_" && !record.mId.isEmpty())
                npcIds.insert(keyFor(record.mId));
        }

        QJsonArray cells;
        std::set<std::string> areaObjects;
        for (const RawRecord& record : database.rawRecords())
        {
            if (record.mType != "CELL" || !containsInsensitive(record.mId, area))
                continue;
            cells.append(record.mId);
            for (const RawField& field : record.mFields)
            {
                if (field.mTag == "NAME" && recordIds.contains(keyFor(field.mText)))
                    areaObjects.insert(keyFor(field.mText));
            }
        }
        if (cells.isEmpty())
            throw std::runtime_error("No cells matched area: " + area.toStdString());

        std::set<std::string> areaActors;
        for (const std::string& object : areaObjects)
        {
            if (npcIds.contains(object))
                areaActors.insert(object);
        }

        std::set<std::string> areaScripts;
        for (const RawRecord& record : database.rawRecords())
        {
            if (!areaObjects.contains(keyFor(record.mId)))
                continue;
            for (const RawField& field : record.mFields)
            {
                if (field.mTag == "SCRI")
                    areaScripts.insert(keyFor(field.mText));
            }
        }

        std::map<std::string, QString> journalIds;
        for (const auto& [key, dialogue] : database.dialogues())
        {
            if (!dialogue.mDeleted && dialogue.mValue.mType == ESM::Dialogue::Journal)
                journalIds.emplace(key, refIdString(dialogue.mValue.mId));
        }

        struct AreaQuestEvidence
        {
            std::set<QString> mActors;
            std::set<QString> mScripts;
            std::set<int> mResultStages;
            QJsonArray mDialogueEvidence;
        };
        std::map<std::string, AreaQuestEvidence> found;

        const auto findScriptJournals = [&](const std::string& scriptText, auto&& callback) {
            const std::string lower = lowerAscii(scriptText);
            for (const auto& [questKey, questId] : journalIds)
            {
                if (lower.find(questKey) != std::string::npos)
                    callback(questKey, questId);
            }
        };

        for (const auto& [topicKey, dialogue] : database.dialogues())
        {
            if (dialogue.mDeleted || dialogue.mValue.mType == ESM::Dialogue::Journal)
                continue;
            for (const InfoRecord* info : orderedInfos(dialogue))
            {
                const std::string actorKey = keyFor(info->mValue.mActor);
                const bool localActor = !actorKey.empty() && areaActors.contains(actorKey);
                const bool localCell = !info->mValue.mCell.empty()
                    && containsInsensitive(refIdString(info->mValue.mCell), area);
                if (!localActor && !localCell)
                    continue;

                std::set<std::string> infoQuests;
                for (const ESM::DialogueCondition& condition : info->mValue.mSelects)
                {
                    if (condition.mFunction != ESM::DialogueCondition::Function_Journal)
                        continue;
                    const std::string questKey = lowerAscii(condition.mVariable);
                    if (journalIds.contains(questKey))
                        infoQuests.insert(questKey);
                }
                findScriptJournals(info->mValue.mResultScript,
                    [&](const std::string& questKey, const QString&) { infoQuests.insert(questKey); });

                for (const std::string& questKey : infoQuests)
                {
                    auto& evidence = found[questKey];
                    const std::set<int> stages
                        = journalStagesInScript(info->mValue.mResultScript, journalIds.at(questKey));
                    evidence.mResultStages.insert(stages.begin(), stages.end());
                    QJsonArray stageValues;
                    for (int stage : stages)
                        stageValues.append(stage);
                    if (localActor)
                        evidence.mActors.insert(refIdString(info->mValue.mActor));
                    evidence.mDialogueEvidence.append(QJsonObject{
                        { "topic_id", refIdString(dialogue.mValue.mId) },
                        { "info_id", refIdString(info->mValue.mId) },
                        { "actor", refIdString(info->mValue.mActor) },
                        { "response", fromUtf8(info->mValue.mResponse) },
                        { "result_journal_indices", stageValues },
                    });
                }
            }
        }

        for (const std::string& scriptKey : areaScripts)
        {
            const auto scriptIt = database.scripts().find(scriptKey);
            if (scriptIt == database.scripts().end() || scriptIt->second.mDeleted)
                continue;
            findScriptJournals(scriptIt->second.mValue.mScriptText,
                [&](const std::string& questKey, const QString& questId) {
                    auto& evidence = found[questKey];
                    evidence.mScripts.insert(refIdString(scriptIt->second.mValue.mId));
                    const std::set<int> stages
                        = journalStagesInScript(scriptIt->second.mValue.mScriptText, questId);
                    evidence.mResultStages.insert(stages.begin(), stages.end());
                });
        }

        QJsonArray actors;
        for (const std::string& actorKey : areaActors)
        {
            for (const RawRecord& record : database.rawRecords())
            {
                if (record.mType == "NPC_" && keyFor(record.mId) == actorKey)
                {
                    actors.append(record.mId);
                    break;
                }
            }
        }

        std::vector<QJsonObject> questObjects;
        for (const auto& [questKey, evidence] : found)
        {
            const DialogueRecord& quest = database.dialogue(journalIds.at(questKey));
            QString title;
            int firstIndex = std::numeric_limits<int>::max();
            int lastIndex = std::numeric_limits<int>::min();
            int firstProgressIndex = std::numeric_limits<int>::max();
            std::set<int> finishedIndices;
            int entryCount = 0;
            for (const InfoRecord* entry : orderedInfos(quest))
            {
                ++entryCount;
                firstIndex = std::min(firstIndex, entry->mValue.mData.mJournalIndex);
                lastIndex = std::max(lastIndex, entry->mValue.mData.mJournalIndex);
                if (entry->mValue.mQuestStatus == ESM::DialInfo::QS_Name)
                    title = fromUtf8(entry->mValue.mResponse);
                else
                    firstProgressIndex = std::min(firstProgressIndex, entry->mValue.mData.mJournalIndex);
                if (entry->mValue.mQuestStatus == ESM::DialInfo::QS_Finished)
                    finishedIndices.insert(entry->mValue.mData.mJournalIndex);
            }
            bool startsHere = false;
            bool finishesHere = false;
            for (int stage : evidence.mResultStages)
            {
                startsHere = startsHere || (firstProgressIndex != std::numeric_limits<int>::max()
                    && stage <= firstProgressIndex);
                finishesHere = finishesHere || finishedIndices.contains(stage);
            }
            if (scope == "starts" && !startsHere)
                continue;
            QString role = "mentions";
            if (startsHere)
                role = "starts";
            else if (finishesHere)
                role = "finishes";
            else if (!evidence.mResultStages.empty())
                role = "advances";
            QJsonArray questActors;
            for (const QString& actor : evidence.mActors)
                questActors.append(actor);
            QJsonArray questScripts;
            for (const QString& script : evidence.mScripts)
                questScripts.append(script);
            QJsonArray resultStages;
            for (int stage : evidence.mResultStages)
                resultStages.append(stage);
            const QString questId = refIdString(quest.mValue.mId);
            questObjects.push_back(QJsonObject{
                { "quest_id", questId },
                { "title", title.isEmpty() ? questId : title },
                { "entry_count", entryCount },
                { "first_index", entryCount > 0 ? firstIndex : 0 },
                { "last_index", entryCount > 0 ? lastIndex : 0 },
                { "local_actors", questActors },
                { "local_scripts", questScripts },
                { "local_result_indices", resultStages },
                { "role", role },
                { "dialogue_evidence", evidence.mDialogueEvidence },
            });
        }
        std::sort(questObjects.begin(), questObjects.end(), [](const QJsonObject& left, const QJsonObject& right) {
            return left.value("title").toString().compare(right.value("title").toString(), Qt::CaseInsensitive)
                < 0;
        });
        QJsonArray quests;
        for (const QJsonObject& quest : questObjects)
            quests.append(quest);
        return {
            { "area", area },
            { "scope", scope },
            { "matched_cells", cells },
            { "actors", actors },
            { "quests", quests },
            { "quest_count", quests.size() },
            { "method", "Quests referenced by dialogue or attached scripts belonging to actors/objects placed in matching cells." },
        };
    }

    QJsonObject positionJson(const ESM::Position& position)
    {
        constexpr double radiansToDegrees = 57.29577951308232;
        return {
            { "x", position.pos[0] }, { "y", position.pos[1] }, { "z", position.pos[2] },
            { "rotation_radians",
                QJsonObject{ { "x", position.rot[0] }, { "y", position.rot[1] }, { "z", position.rot[2] } } },
            { "rotation_degrees",
                QJsonObject{ { "x", position.rot[0] * radiansToDegrees },
                    { "y", position.rot[1] * radiansToDegrees },
                    { "z", position.rot[2] * radiansToDegrees } } },
        };
    }

    QJsonObject npcJson(const ESM::NPC& npc, const QString& source)
    {
        QJsonArray inventory;
        for (const ESM::ContItem& item : npc.mInventory.mList)
            inventory.append(QJsonObject{ { "item_id", refIdString(item.mItem) }, { "count", item.mCount } });
        QJsonArray spells;
        for (const ESM::RefId& spell : npc.mSpells.mList)
            spells.append(refIdString(spell));
        return {
            { "npc_id", refIdString(npc.mId) },
            { "name", fromUtf8(npc.mName) },
            { "race", refIdString(npc.mRace) },
            { "class", refIdString(npc.mClass) },
            { "faction", refIdString(npc.mFaction) },
            { "script", refIdString(npc.mScript) },
            { "head", refIdString(npc.mHead) },
            { "hair", refIdString(npc.mHair) },
            { "female", !npc.isMale() },
            { "essential", (npc.mFlags & ESM::NPC::Essential) != 0 },
            { "respawn", (npc.mFlags & ESM::NPC::Respawn) != 0 },
            { "autocalc", (npc.mFlags & ESM::NPC::Autocalc) != 0 },
            { "level", npc.mNpdt.mLevel },
            { "disposition", npc.mNpdt.mDisposition },
            { "reputation", npc.mNpdt.mReputation },
            { "gold", npc.mNpdt.mGold },
            { "ai", QJsonObject{ { "hello", npc.mAiData.mHello }, { "fight", npc.mAiData.mFight },
                        { "flee", npc.mAiData.mFlee }, { "alarm", npc.mAiData.mAlarm },
                        { "services", npc.mAiData.mServices } } },
            { "inventory", inventory },
            { "spells", spells },
            { "source", source },
        };
    }

    QJsonObject landJson(const LandRecord& record)
    {
        const int dataFlags = ESM::Land::DATA_VHGT | ESM::Land::DATA_VNML | ESM::Land::DATA_VCLR
            | ESM::Land::DATA_VTEX;
        const ESM::Land::LandData* data = record.mValue.getLandData(dataFlags);
        QJsonObject result{
            { "grid_x", record.mValue.mX },
            { "grid_y", record.mValue.mY },
            { "source", record.mSource },
            { "deleted", record.mDeleted },
            { "data_types", record.mValue.mDataTypes },
        };
        if (data == nullptr || !(data->mDataLoaded & ESM::Land::DATA_VHGT))
        {
            result.insert("height_data", false);
            return result;
        }

        double total = 0;
        for (float height : data->mHeights)
            total += height;
        QJsonArray samples;
        for (int y = 0; y < ESM::Land::LAND_SIZE; y += 8)
        {
            QJsonArray row;
            for (int x = 0; x < ESM::Land::LAND_SIZE; x += 8)
                row.append(data->mHeights[y * ESM::Land::LAND_SIZE + x]);
            samples.append(row);
        }

        QJsonObject textureCounts;
        if (data->mDataLoaded & ESM::Land::DATA_VTEX)
        {
            std::map<int, int> counts;
            for (std::uint16_t texture : data->mTextures)
                ++counts[texture];
            for (const auto& [texture, count] : counts)
                textureCounts.insert(QString::number(texture), count);
        }
        result.insert("height_data", true);
        result.insert("vertices_per_side", ESM::Land::LAND_SIZE);
        result.insert("vertex_spacing", ESM::Land::REAL_SIZE / (ESM::Land::LAND_SIZE - 1));
        result.insert("min_height", data->mMinHeight);
        result.insert("max_height", data->mMaxHeight);
        result.insert("average_height", total / ESM::Land::LAND_NUM_VERTS);
        result.insert("height_samples_9x9", samples);
        result.insert("texture_index_counts", textureCounts);
        return result;
    }

    QJsonObject cellSummaryJson(const Database& database, const CellRecord& record)
    {
        int referenceCount = 0;
        int npcCount = 0;
        std::map<QString, int> types;
        for (const auto& [key, reference] : record.mReferences)
        {
            if (reference.mDeleted)
                continue;
            ++referenceCount;
            const QString type = database.recordType(reference.mValue.mRefID);
            ++types[type.isEmpty() ? QString("unknown") : type];
            if (type == "NPC_")
                ++npcCount;
        }
        QJsonObject typeCounts;
        for (const auto& [type, count] : types)
            typeCounts.insert(type, count);
        const ESM::Cell& cell = record.mValue;
        QJsonObject result{
            { "cell_id", refIdString(cell.mId) },
            { "name", fromUtf8(cell.mName) },
            { "interior", !cell.isExterior() },
            { "region", refIdString(cell.mRegion) },
            { "has_water", cell.hasWater() },
            { "water_height", cell.mWater },
            { "reference_count", referenceCount },
            { "npc_count", npcCount },
            { "reference_type_counts", typeCounts },
            { "source", record.mSource },
        };
        if (cell.isExterior())
        {
            result.insert("grid_x", cell.mData.mX);
            result.insert("grid_y", cell.mData.mY);
        }
        return result;
    }

    QJsonObject findEnvironments(const Database& database, const QJsonObject& arguments)
    {
        const QString query = arguments.value("query").toString().trimmed();
        const int limit = std::clamp(arguments.value("limit").toInt(100), 1, 500);
        QJsonArray cells;
        for (const auto& [key, record] : database.cells())
        {
            if (record.mDeleted)
                continue;
            const ESM::Cell& cell = record.mValue;
            if (!query.isEmpty() && !containsInsensitive(fromUtf8(cell.mName), query)
                && !containsInsensitive(refIdString(cell.mId), query)
                && !containsInsensitive(refIdString(cell.mRegion), query))
                continue;
            cells.append(cellSummaryJson(database, record));
            if (cells.size() >= limit)
                break;
        }
        return { { "cells", cells }, { "count", cells.size() }, { "truncated", cells.size() >= limit } };
    }

    std::optional<float> terrainHeightAt(const ESM::Cell& cell, const ESM::Land::LandData* data, float worldX, float worldY)
    {
        if (!cell.isExterior() || data == nullptr || !(data->mDataLoaded & ESM::Land::DATA_VHGT))
            return std::nullopt;
        const float cellMinX = static_cast<float>(cell.mData.mX * ESM::Cell::sSize);
        const float cellMinY = static_cast<float>(cell.mData.mY * ESM::Cell::sSize);
        const float spacing = static_cast<float>(ESM::Land::REAL_SIZE) / (ESM::Land::LAND_SIZE - 1);
        const float vertexX = std::clamp((worldX - cellMinX) / spacing, 0.f, static_cast<float>(ESM::Land::LAND_SIZE - 1));
        const float vertexY = std::clamp((worldY - cellMinY) / spacing, 0.f, static_cast<float>(ESM::Land::LAND_SIZE - 1));
        const int x0 = static_cast<int>(std::floor(vertexX));
        const int y0 = static_cast<int>(std::floor(vertexY));
        const int x1 = std::min(x0 + 1, ESM::Land::LAND_SIZE - 1);
        const int y1 = std::min(y0 + 1, ESM::Land::LAND_SIZE - 1);
        const float tx = vertexX - x0;
        const float ty = vertexY - y0;
        const auto height = [&](int x, int y) { return data->mHeights[y * ESM::Land::LAND_SIZE + x]; };
        const float lower = height(x0, y0) * (1 - tx) + height(x1, y0) * tx;
        const float upper = height(x0, y1) * (1 - tx) + height(x1, y1) * tx;
        return lower * (1 - ty) + upper * ty;
    }

    QJsonObject getEnvironment(const Database& database, const QJsonObject& arguments)
    {
        const QString area = arguments.value("area").toString().trimmed();
        const QString cellId = arguments.value("cell_id").toString().trimmed();
        const bool hasGrid = arguments.contains("grid_x") && arguments.contains("grid_y");
        const int gridX = arguments.value("grid_x").toInt();
        const int gridY = arguments.value("grid_y").toInt();
        const int radius = std::clamp(arguments.value("radius").toInt(0), 0, 10);
        const int maxReferences = std::clamp(arguments.value("max_references").toInt(2000), 1, 10000);
        if (area.isEmpty() && cellId.isEmpty() && !hasGrid)
            throw std::runtime_error("Provide area, cell_id, or both grid_x and grid_y");

        std::vector<const CellRecord*> selected;
        for (const auto& [key, record] : database.cells())
        {
            if (record.mDeleted)
                continue;
            const ESM::Cell& cell = record.mValue;
            bool match = false;
            if (!area.isEmpty())
                match = containsInsensitive(fromUtf8(cell.mName), area) || containsInsensitive(refIdString(cell.mRegion), area);
            if (!cellId.isEmpty())
                match = match || refIdString(cell.mId).compare(cellId, Qt::CaseInsensitive) == 0
                    || fromUtf8(cell.mName).compare(cellId, Qt::CaseInsensitive) == 0;
            if (hasGrid && cell.isExterior())
                match = match || (std::abs(cell.mData.mX - gridX) <= radius && std::abs(cell.mData.mY - gridY) <= radius);
            if (match)
                selected.push_back(&record);
        }
        if (selected.empty())
            throw std::runtime_error("No cells matched the environment selector");
        std::sort(selected.begin(), selected.end(), [](const CellRecord* left, const CellRecord* right) {
            if (left->mValue.isExterior() != right->mValue.isExterior())
                return left->mValue.isExterior() > right->mValue.isExterior();
            if (left->mValue.mData.mY != right->mValue.mData.mY)
                return left->mValue.mData.mY < right->mValue.mData.mY;
            if (left->mValue.mData.mX != right->mValue.mData.mX)
                return left->mValue.mData.mX < right->mValue.mData.mX;
            return left->mValue.mName < right->mValue.mName;
        });

        QJsonArray cells;
        int emittedReferences = 0;
        for (const CellRecord* record : selected)
        {
            QJsonObject cellObject = cellSummaryJson(database, *record);
            const ESM::Cell& cell = record->mValue;
            const LandRecord* cellLand = nullptr;
            const ESM::Land::LandData* cellLandData = nullptr;
            if (cell.isExterior())
            {
                const auto land = database.lands().find({ cell.mData.mX, cell.mData.mY });
                if (land != database.lands().end())
                {
                    cellLand = &land->second;
                    cellLandData = land->second.mValue.getLandData(ESM::Land::DATA_VHGT);
                }
            }
            cellObject.insert("flags", cell.mData.mFlags);
            cellObject.insert("map_color", static_cast<qint64>(cell.mMapColor));
            cellObject.insert("ambient", QJsonObject{ { "present", cell.mHasAmbi },
                                             { "ambient_color", static_cast<qint64>(cell.mAmbi.mAmbient) },
                                             { "sunlight_color", static_cast<qint64>(cell.mAmbi.mSunlight) },
                                             { "fog_color", static_cast<qint64>(cell.mAmbi.mFog) },
                                             { "fog_density", cell.mAmbi.mFogDensity } });

            QJsonArray references;
            QJsonArray localNpcs;
            QJsonObject bounds;
            bool hasBounds = false;
            float minX = 0, minY = 0, minZ = 0, maxX = 0, maxY = 0, maxZ = 0;
            for (const auto& [refKey, reference] : record->mReferences)
            {
                if (reference.mDeleted || emittedReferences >= maxReferences)
                    continue;
                const ESM::CellRef& ref = reference.mValue;
                const QString type = database.recordType(ref.mRefID);
                QJsonObject value{
                    { "reference_number", QJsonObject{ { "index", static_cast<qint64>(ref.mRefNum.mIndex) },
                                                { "content_file", ref.mRefNum.mContentFile } } },
                    { "base_id", refIdString(ref.mRefID) },
                    { "record_type", type },
                    { "name", database.recordName(ref.mRefID) },
                    { "model", database.recordModel(ref.mRefID) },
                    { "scale", ref.mScale },
                    { "position", positionJson(ref.mPos) },
                    { "owner", refIdString(ref.mOwner) },
                    { "moved", reference.mMoved },
                    { "source", reference.mSource },
                };
                if (ref.mTeleport)
                {
                    value.insert("teleport", QJsonObject{ { "destination_cell", fromUtf8(ref.mDestCell) },
                                                   { "destination", positionJson(ref.mDoorDest) } });
                }
                if (const std::optional<float> height
                    = terrainHeightAt(cell, cellLandData, ref.mPos.pos[0], ref.mPos.pos[1]))
                {
                    value.insert("terrain_height", *height);
                    value.insert("height_above_terrain", ref.mPos.pos[2] - *height);
                }
                references.append(value);
                ++emittedReferences;

                const float x = ref.mPos.pos[0], y = ref.mPos.pos[1], z = ref.mPos.pos[2];
                if (!hasBounds)
                {
                    minX = maxX = x;
                    minY = maxY = y;
                    minZ = maxZ = z;
                    hasBounds = true;
                }
                else
                {
                    minX = std::min(minX, x);
                    minY = std::min(minY, y);
                    minZ = std::min(minZ, z);
                    maxX = std::max(maxX, x);
                    maxY = std::max(maxY, y);
                    maxZ = std::max(maxZ, z);
                }
                if (type == "NPC_")
                {
                    const auto npc = database.npcs().find(keyFor(ref.mRefID));
                    if (npc != database.npcs().end() && !npc->second.mDeleted)
                        localNpcs.append(npcJson(npc->second.mValue, npc->second.mSource));
                }
            }
            if (hasBounds)
            {
                bounds = { { "min", QJsonObject{ { "x", minX }, { "y", minY }, { "z", minZ } } },
                    { "max", QJsonObject{ { "x", maxX }, { "y", maxY }, { "z", maxZ } } } };
            }
            cellObject.insert("placement_bounds", bounds);
            cellObject.insert("references", references);
            cellObject.insert("npcs", localNpcs);
            if (cell.isExterior())
            {
                if (cellLand != nullptr)
                    cellObject.insert("land", landJson(*cellLand));
                else
                    cellObject.insert("land", QJsonObject{ { "present", false } });
            }
            cells.append(cellObject);
        }
        return {
            { "selector", QJsonObject{ { "area", area }, { "cell_id", cellId }, { "grid_x", gridX },
                              { "grid_y", gridY }, { "radius", radius } } },
            { "cells", cells },
            { "cell_count", cells.size() },
            { "references_returned", emittedReferences },
            { "references_truncated", emittedReferences >= maxReferences },
        };
    }

    QJsonObject dialogueJson(const DialogueRecord& dialogue)
    {
        QJsonArray infos;
        const QString topicId = refIdString(dialogue.mValue.mId);
        for (const InfoRecord* info : orderedInfos(dialogue))
            infos.append(infoJson(info->mValue, topicId, info->mSource));
        return {
            { "record_type", "DIAL" },
            { "dialogue_id", topicId },
            { "dialogue_type", dialogueTypeName(dialogue.mValue.mType) },
            { "source", dialogue.mSource },
            { "infos", infos },
        };
    }

    QJsonObject getRecord(const Database& database, const QJsonObject& arguments)
    {
        const QString type = arguments.value("record_type").toString().toUpper();
        const QString id = arguments.value("record_id").toString();
        const QString topicId = arguments.value("topic_id").toString();
        if (type.isEmpty() || id.isEmpty())
            throw std::runtime_error("record_type and record_id are required");

        if (type == "DIAL")
            return dialogueJson(database.dialogue(id));
        if (type == "SCPT")
        {
            const ScriptRecord& script = database.script(id);
            return {
                { "record_type", "SCPT" },
                { "script_id", refIdString(script.mValue.mId) },
                { "source_text", fromUtf8(script.mValue.mScriptText) },
                { "source", script.mSource },
            };
        }
        if (type == "NPC_")
        {
            const NpcRecord& npc = database.npc(id);
            return npcJson(npc.mValue, npc.mSource);
        }
        if (type == "MISC")
        {
            const MiscRecord& misc = database.misc(id);
            return {
                { "record_type", "MISC" },
                { "record_id", refIdString(misc.mValue.mId) },
                { "name", fromUtf8(misc.mValue.mName) },
                { "model", fromUtf8(misc.mValue.mModel.getOriginal()) },
                { "icon", fromUtf8(misc.mValue.mIcon.getOriginal()) },
                { "script", refIdString(misc.mValue.mScript) },
                { "weight", misc.mValue.mData.mWeight },
                { "value", misc.mValue.mData.mValue },
                { "is_key", (misc.mValue.mData.mFlags & ESM::Miscellaneous::Key) != 0 },
                { "source", misc.mSource },
            };
        }
        if (type == "CELL")
            return getEnvironment(database, QJsonObject{ { "cell_id", id } });
        if (type == "LAND")
        {
            QString coordinates = id;
            coordinates.remove('#');
            const QStringList parts = coordinates.simplified().split(' ');
            if (parts.size() != 2)
                throw std::runtime_error("LAND record_id must be formatted as #x y");
            bool xOk = false, yOk = false;
            const int x = parts[0].toInt(&xOk);
            const int y = parts[1].toInt(&yOk);
            const auto land = database.lands().find({ x, y });
            if (!xOk || !yOk || land == database.lands().end())
                throw std::runtime_error("LAND not found: " + id.toStdString());
            return landJson(land->second);
        }
        if (type == "INFO")
        {
            if (!topicId.isEmpty())
            {
                const DialogueRecord& dialogue = database.dialogue(topicId);
                const auto it = dialogue.mInfos.find(keyFor(id));
                if (it == dialogue.mInfos.end() || it->second.mDeleted)
                    throw std::runtime_error("INFO not found in topic");
                return infoJson(it->second.mValue, refIdString(dialogue.mValue.mId), it->second.mSource);
            }
            for (const auto& [key, dialogue] : database.dialogues())
            {
                const auto it = dialogue.mInfos.find(keyFor(id));
                if (it != dialogue.mInfos.end() && !it->second.mDeleted)
                    return infoJson(it->second.mValue, refIdString(dialogue.mValue.mId), it->second.mSource);
            }
            throw std::runtime_error("INFO not found: " + id.toStdString());
        }

        QJsonArray versions;
        for (const RawRecord& record : database.rawRecords())
        {
            if (record.mType.compare(type, Qt::CaseInsensitive) != 0
                || record.mId.compare(id, Qt::CaseInsensitive) != 0)
                continue;
            QJsonArray fields;
            for (const RawField& field : record.mFields)
                fields.append(QJsonObject{ { "subrecord", field.mTag }, { "text", field.mText } });
            versions.append(QJsonObject{
                { "record_type", record.mType },
                { "record_id", record.mId },
                { "source", record.mSource },
                { "text_fields", fields },
            });
        }
        if (versions.isEmpty())
            throw std::runtime_error("Record not found in the text index: " + type.toStdString() + " " + id.toStdString());
        return { { "versions_in_load_order", versions }, { "effective", versions.last() } };
    }

    QJsonObject searchRecords(const Database& database, const QJsonObject& arguments)
    {
        const QString query = arguments.value("query").toString().trimmed();
        if (query.size() < 2)
            throw std::runtime_error("query must contain at least two characters");
        const QString typeFilter = arguments.value("record_type").toString().toUpper();
        const int limit = std::clamp(arguments.value("limit").toInt(50), 1, 200);
        QJsonArray matches;

        const auto canAdd = [&]() { return matches.size() < limit; };
        for (const auto& [key, dialogue] : database.dialogues())
        {
            if (!canAdd())
                break;
            if (dialogue.mDeleted || (!typeFilter.isEmpty() && typeFilter != "DIAL" && typeFilter != "INFO"))
                continue;
            const QString topicId = refIdString(dialogue.mValue.mId);
            if ((typeFilter.isEmpty() || typeFilter == "DIAL") && containsInsensitive(topicId, query))
            {
                matches.append(QJsonObject{
                    { "record_type", "DIAL" },
                    { "record_id", topicId },
                    { "matched_text", topicId },
                    { "source", dialogue.mSource },
                });
            }
            for (const InfoRecord* info : orderedInfos(dialogue))
            {
                if (!canAdd() || (!typeFilter.isEmpty() && typeFilter != "INFO"))
                    break;
                QStringList matched;
                const auto addIfMatch = [&](const QString& value) {
                    if (!value.isEmpty() && containsInsensitive(value, query))
                        matched.append(value);
                };
                addIfMatch(refIdString(info->mValue.mId));
                addIfMatch(fromUtf8(info->mValue.mResponse));
                addIfMatch(fromUtf8(info->mValue.mResultScript));
                addIfMatch(refIdString(info->mValue.mActor));
                for (const auto& condition : info->mValue.mSelects)
                    addIfMatch(QString::fromUtf8(condition.mVariable));
                if (!matched.isEmpty())
                {
                    matches.append(QJsonObject{
                        { "record_type", "INFO" },
                        { "record_id", refIdString(info->mValue.mId) },
                        { "topic_id", topicId },
                        { "matched_text", matched.join(" | ") },
                        { "source", info->mSource },
                    });
                }
            }
        }

        for (const auto& [key, script] : database.scripts())
        {
            if (!canAdd())
                break;
            if (script.mDeleted || (!typeFilter.isEmpty() && typeFilter != "SCPT"))
                continue;
            const QString id = refIdString(script.mValue.mId);
            const QString text = fromUtf8(script.mValue.mScriptText);
            if (containsInsensitive(id, query) || containsInsensitive(text, query))
            {
                matches.append(QJsonObject{
                    { "record_type", "SCPT" },
                    { "record_id", id },
                    { "matched_text", containsInsensitive(id, query) ? id : matchingExcerpt(text, query) },
                    { "source", script.mSource },
                });
            }
        }

        for (const RawRecord& record : database.rawRecords())
        {
            if (!canAdd())
                break;
            if (!typeFilter.isEmpty() && record.mType.compare(typeFilter, Qt::CaseInsensitive) != 0)
                continue;
            QStringList matched;
            if (containsInsensitive(record.mId, query))
                matched.append(record.mId);
            for (const RawField& field : record.mFields)
            {
                if (containsInsensitive(field.mText, query))
                    matched.append(field.mTag + ": " + field.mText);
            }
            if (!matched.isEmpty())
            {
                matches.append(QJsonObject{
                    { "record_type", record.mType },
                    { "record_id", record.mId },
                    { "matched_text", matched.join(" | ").left(2000) },
                    { "source", record.mSource },
                });
            }
        }
        return { { "matches", matches }, { "count", matches.size() }, { "truncated", matches.size() >= limit } };
    }

    ESM::DialInfo::QuestStatus parseQuestStatus(const QString& status)
    {
        const QString lower = status.toLower();
        if (lower == "none" || lower.isEmpty())
            return ESM::DialInfo::QS_None;
        if (lower == "name")
            return ESM::DialInfo::QS_Name;
        if (lower == "finished")
            return ESM::DialInfo::QS_Finished;
        if (lower == "restart")
            return ESM::DialInfo::QS_Restart;
        throw std::runtime_error("Unknown quest_status: " + status.toStdString());
    }

    ESM::Dialogue::Type parseDialogueType(const QString& type)
    {
        const QString lower = type.toLower();
        if (lower == "topic")
            return ESM::Dialogue::Topic;
        if (lower == "voice")
            return ESM::Dialogue::Voice;
        if (lower == "greeting")
            return ESM::Dialogue::Greeting;
        if (lower == "persuasion")
            return ESM::Dialogue::Persuasion;
        if (lower == "journal")
            return ESM::Dialogue::Journal;
        throw std::runtime_error("Unknown dialogue_type: " + type.toStdString());
    }

    ESM::DialogueCondition::Comparison parseComparison(const QString& comparison)
    {
        using Comparison = ESM::DialogueCondition::Comparison;
        const QString lower = comparison.toLower();
        if (lower == "==" || lower == "eq")
            return Comparison::Comp_Eq;
        if (lower == "!=" || lower == "ne")
            return Comparison::Comp_Ne;
        if (lower == ">" || lower == "gt")
            return Comparison::Comp_Gt;
        if (lower == ">=" || lower == "ge")
            return Comparison::Comp_Ge;
        if (lower == "<" || lower == "lt")
            return Comparison::Comp_Ls;
        if (lower == "<=" || lower == "le")
            return Comparison::Comp_Le;
        throw std::runtime_error("Unknown condition comparison: " + comparison.toStdString());
    }

    ESM::DialogueCondition::Function parseConditionFunction(const QJsonValue& value)
    {
        using Function = ESM::DialogueCondition::Function;
        if (value.isDouble())
        {
            const int index = value.toInt(-1);
            if (index >= Function::Function_FacReactionLowest && index <= Function::Function_NotLocal)
                return static_cast<Function>(index);
        }
        const QString function = value.toString().toLower();
        const std::map<QString, Function> names{
            { "global", Function::Function_Global },
            { "local", Function::Function_Local },
            { "journal", Function::Function_Journal },
            { "item", Function::Function_Item },
            { "dead", Function::Function_Dead },
            { "not_id", Function::Function_NotId },
            { "not_faction", Function::Function_NotFaction },
            { "not_class", Function::Function_NotClass },
            { "not_race", Function::Function_NotRace },
            { "not_cell", Function::Function_NotCell },
            { "not_local", Function::Function_NotLocal },
        };
        const auto it = names.find(function);
        if (it != names.end())
            return it->second;
        if (function.startsWith("function_"))
        {
            bool ok = false;
            const int index = function.mid(9).toInt(&ok);
            if (ok && index >= Function::Function_FacReactionLowest && index <= Function::Function_PcWerewolfKills)
                return static_cast<Function>(index);
        }
        throw std::runtime_error("Unknown condition function: " + function.toStdString());
    }

    std::vector<ESM::DialogueCondition> parseConditions(const QJsonArray& values)
    {
        std::vector<ESM::DialogueCondition> result;
        std::uint8_t index = 0;
        for (const QJsonValue& value : values)
        {
            if (!value.isObject())
                throw std::runtime_error("Each condition must be an object");
            const QJsonObject object = value.toObject();
            ESM::DialogueCondition condition;
            condition.mIndex = index++;
            condition.mFunction = parseConditionFunction(object.value("function"));
            condition.mVariable = object.value("variable").toString().toUtf8().toStdString();
            condition.mComparison = parseComparison(object.value("comparison").toString());
            const double number = object.value("value").toDouble();
            if (std::floor(number) == number && number >= std::numeric_limits<int32_t>::min()
                && number <= std::numeric_limits<int32_t>::max())
                condition.mValue = static_cast<int32_t>(number);
            else
                condition.mValue = static_cast<float>(number);
            result.push_back(std::move(condition));
        }
        return result;
    }

    void setOptionalRef(ESM::RefId& field, const QJsonObject& edit, const char* name)
    {
        if (edit.contains(name))
        {
            const std::string value = edit.value(name).toString().toUtf8().toStdString();
            field = value.empty() ? ESM::RefId() : ESM::RefId::stringRefId(value);
        }
    }

    void applyInfoFields(ESM::DialInfo& info, const QJsonObject& edit)
    {
        if (edit.contains("response"))
            info.mResponse = edit.value("response").toString().toUtf8().toStdString();
        if (edit.contains("result_script"))
            info.mResultScript = edit.value("result_script").toString().toUtf8().toStdString();
        if (edit.contains("journal_index"))
            info.mData.mJournalIndex = edit.value("journal_index").toInt();
        if (edit.contains("disposition"))
            info.mData.mDisposition = edit.value("disposition").toInt();
        if (edit.contains("quest_status"))
            info.mQuestStatus = parseQuestStatus(edit.value("quest_status").toString());
        if (edit.contains("factionless"))
            info.mFactionLess = edit.value("factionless").toBool();
        setOptionalRef(info.mActor, edit, "actor");
        setOptionalRef(info.mRace, edit, "race");
        setOptionalRef(info.mClass, edit, "class");
        setOptionalRef(info.mFaction, edit, "faction");
        setOptionalRef(info.mPcFaction, edit, "player_faction");
        setOptionalRef(info.mCell, edit, "cell");
        if (edit.contains("conditions"))
            info.mSelects = parseConditions(edit.value("conditions").toArray());
    }

    struct TopicPatch
    {
        ESM::Dialogue mDialogue;
        std::map<std::string, ESM::DialInfo> mInfos;
        std::set<std::string> mEmit;
        std::set<std::string> mDeleted;
    };

    TopicPatch makeTopicPatch(const DialogueRecord& source)
    {
        TopicPatch result;
        result.mDialogue = source.mValue;
        for (const auto& [key, info] : source.mInfos)
        {
            if (!info.mDeleted)
                result.mInfos.emplace(key, info.mValue);
        }
        return result;
    }

    std::vector<std::string> orderedInfoKeys(const TopicPatch& topic)
    {
        std::vector<std::string> result;
        std::set<std::string> added;
        const ESM::DialInfo* current = nullptr;
        for (const auto& [key, info] : topic.mInfos)
        {
            if (info.mPrev.empty())
            {
                current = &info;
                break;
            }
        }
        while (current != nullptr)
        {
            const std::string key = keyFor(current->mId);
            if (!added.insert(key).second)
                break;
            result.push_back(key);
            if (current->mNext.empty())
                break;
            const auto next = topic.mInfos.find(keyFor(current->mNext));
            current = next == topic.mInfos.end() ? nullptr : &next->second;
        }
        for (const auto& [key, info] : topic.mInfos)
        {
            if (added.insert(key).second)
                result.push_back(key);
        }
        return result;
    }

    void insertInfo(
        TopicPatch& topic, ESM::DialInfo info, const QString& afterId, const QString& beforeId = QString())
    {
        const std::string newKey = keyFor(info.mId);
        if (topic.mInfos.contains(newKey))
            throw std::runtime_error("INFO already exists: " + info.mId.toString());

        std::vector<std::string> order = orderedInfoKeys(topic);
        std::size_t position = order.size();
        if (!afterId.isEmpty() && !beforeId.isEmpty())
            throw std::runtime_error("Specify only one of insert_after_info_id and insert_before_info_id");
        if (!beforeId.isEmpty())
        {
            const auto it = std::find(order.begin(), order.end(), keyFor(beforeId));
            if (it == order.end())
                throw std::runtime_error("insert_before_info_id was not found: " + beforeId.toStdString());
            position = static_cast<std::size_t>(std::distance(order.begin(), it));
        }
        else if (!afterId.isEmpty())
        {
            const auto it = std::find(order.begin(), order.end(), keyFor(afterId));
            if (it == order.end())
                throw std::runtime_error("insert_after_info_id was not found: " + afterId.toStdString());
            position = static_cast<std::size_t>(std::distance(order.begin(), it)) + 1;
        }
        else if (topic.mDialogue.mType == ESM::Dialogue::Journal)
        {
            position = 0;
            while (position < order.size()
                && topic.mInfos.at(order[position]).mData.mJournalIndex <= info.mData.mJournalIndex)
                ++position;
        }

        const std::optional<std::string> previous = position > 0 ? std::optional(order[position - 1]) : std::nullopt;
        const std::optional<std::string> next = position < order.size() ? std::optional(order[position]) : std::nullopt;
        info.mPrev = previous ? topic.mInfos.at(*previous).mId : ESM::RefId();
        info.mNext = next ? topic.mInfos.at(*next).mId : ESM::RefId();
        topic.mInfos.emplace(newKey, info);
        topic.mEmit.insert(newKey);
        if (previous)
        {
            topic.mInfos.at(*previous).mNext = info.mId;
            topic.mEmit.insert(*previous);
        }
        if (next)
        {
            topic.mInfos.at(*next).mPrev = info.mId;
            topic.mEmit.insert(*next);
        }
    }

    struct WorldCellPatch
    {
        ESM::Cell mCell;
        std::vector<std::pair<ESM::CellRef, bool>> mReferences; // second is persistent
    };

    void applyPosition(ESM::Position& position, const QJsonObject& value)
    {
        position.pos[0] = static_cast<float>(value.value("x").toDouble());
        position.pos[1] = static_cast<float>(value.value("y").toDouble());
        position.pos[2] = static_cast<float>(value.value("z").toDouble());
        const QJsonObject rotation = value.value("rotation").toObject();
        const bool degrees = value.value("rotation_degrees").toBool(true);
        constexpr double degreesToRadians = 0.017453292519943295;
        const double multiplier = degrees ? degreesToRadians : 1.0;
        position.rot[0] = static_cast<float>(rotation.value("x").toDouble() * multiplier);
        position.rot[1] = static_cast<float>(rotation.value("y").toDouble() * multiplier);
        position.rot[2] = static_cast<float>(rotation.value("z").toDouble() * multiplier);
    }

    ESM::Land makeLand(const QJsonObject& edit)
    {
        ESM::Land land;
        land.blank();
        land.mX = edit.value("grid_x").toInt();
        land.mY = edit.value("grid_y").toInt();
        land.mFlags = ESM::Land::Flag_HeightsNormals | ESM::Land::Flag_Colors;
        land.mDataTypes
            = ESM::Land::DATA_VNML | ESM::Land::DATA_VHGT | ESM::Land::DATA_WNAM | ESM::Land::DATA_VCLR;
        ESM::Land::LandData* data = land.getLandData();
        if (data == nullptr)
            throw std::runtime_error("Failed to allocate LAND data");
        data->mDataLoaded = land.mDataTypes;

        const QJsonArray explicitHeights = edit.value("heights").toArray();
        if (!explicitHeights.isEmpty() && explicitHeights.size() != ESM::Land::LAND_NUM_VERTS)
            throw std::runtime_error("add_land heights must contain exactly 4225 values");
        const QJsonObject terrain = edit.value("terrain").toObject();
        const QString style = terrain.value("style").toString("flat").toLower();
        const double baseHeight = terrain.value("base_height").toDouble(0.0);
        const double centerHeight = terrain.value("center_height").toDouble(384.0);
        const double edgeHeight = terrain.value("edge_height").toDouble(-256.0);
        const double centerX = terrain.value("center_x").toDouble(
            (static_cast<double>(land.mX) + 0.5) * ESM::Land::REAL_SIZE);
        const double centerY = terrain.value("center_y").toDouble(
            (static_cast<double>(land.mY) + 0.5) * ESM::Land::REAL_SIZE);
        const double radius = std::max(1.0, terrain.value("radius").toDouble(6000.0));
        const double roughness = terrain.value("roughness").toDouble(0.0);
        const double seed = terrain.value("seed").toDouble(1.0);
        const double vertexSpacing
            = static_cast<double>(ESM::Land::REAL_SIZE) / (ESM::Land::LAND_SIZE - 1);
        data->mMinHeight = std::numeric_limits<float>::max();
        data->mMaxHeight = -std::numeric_limits<float>::max();
        for (int y = 0; y < ESM::Land::LAND_SIZE; ++y)
        {
            for (int x = 0; x < ESM::Land::LAND_SIZE; ++x)
            {
                const int index = y * ESM::Land::LAND_SIZE + x;
                double height = baseHeight;
                if (!explicitHeights.isEmpty())
                    height = explicitHeights.at(index).toDouble();
                else if (style == "island")
                {
                    const double worldX = static_cast<double>(land.mX) * ESM::Land::REAL_SIZE + x * vertexSpacing;
                    const double worldY = static_cast<double>(land.mY) * ESM::Land::REAL_SIZE + y * vertexSpacing;
                    const double distance = std::hypot(worldX - centerX, worldY - centerY);
                    double t = std::clamp(1.0 - distance / radius, 0.0, 1.0);
                    t = t * t * (3.0 - 2.0 * t);
                    const double noise = std::sin(worldX * 0.0037 + seed * 1.71)
                            * std::cos(worldY * 0.0043 - seed * 2.13)
                        + 0.45 * std::sin((worldX + worldY) * 0.0071 + seed);
                    height = edgeHeight + (centerHeight - edgeHeight) * t + roughness * noise * t;
                }
                else if (style != "flat")
                    throw std::runtime_error("Unknown terrain style: " + style.toStdString());
                height = std::round(height / ESM::Land::sHeightScale) * ESM::Land::sHeightScale;
                data->mHeights[index] = static_cast<float>(height);
                data->mMinHeight = std::min(data->mMinHeight, static_cast<float>(height));
                data->mMaxHeight = std::max(data->mMaxHeight, static_cast<float>(height));
            }
        }

        for (int y = 0; y < ESM::Land::LAND_SIZE; ++y)
        {
            for (int x = 0; x < ESM::Land::LAND_SIZE; ++x)
            {
                const int left = y * ESM::Land::LAND_SIZE + std::max(0, x - 1);
                const int right = y * ESM::Land::LAND_SIZE + std::min(ESM::Land::LAND_SIZE - 1, x + 1);
                const int down = std::max(0, y - 1) * ESM::Land::LAND_SIZE + x;
                const int up = std::min(ESM::Land::LAND_SIZE - 1, y + 1) * ESM::Land::LAND_SIZE + x;
                double nx = data->mHeights[left] - data->mHeights[right];
                double ny = data->mHeights[down] - data->mHeights[up];
                double nz = vertexSpacing * 2.0;
                const double length = std::max(1.0, std::sqrt(nx * nx + ny * ny + nz * nz));
                const int index = 3 * (y * ESM::Land::LAND_SIZE + x);
                data->mNormals[index] = static_cast<std::int8_t>(std::clamp(nx / length * 127.0, -127.0, 127.0));
                data->mNormals[index + 1]
                    = static_cast<std::int8_t>(std::clamp(ny / length * 127.0, -127.0, 127.0));
                data->mNormals[index + 2]
                    = static_cast<std::int8_t>(std::clamp(nz / length * 127.0, -127.0, 127.0));
                const std::uint8_t color = static_cast<std::uint8_t>(std::clamp(
                    edit.value("vertex_color").toInt(210), 0, 255));
                data->mColours[index] = color;
                data->mColours[index + 1] = color;
                data->mColours[index + 2] = color;
            }
        }
        data->mTextures.fill(0);
        return land;
    }

    void validateWrittenPlugin(const std::filesystem::path& path, ToUTF8::Utf8Encoder& encoder)
    {
        ESM::ESMReader reader;
        reader.setEncoder(&encoder);
        reader.open(path);
        while (reader.hasMoreRecs())
        {
            reader.getRecName();
            std::uint32_t flags = 0;
            reader.getRecHeader(flags);
            reader.skipRecord();
        }
    }

    QJsonObject validatePlugin(Database& database, const QJsonObject& arguments)
    {
        const QString pathValue = arguments.value("path").toString();
        if (pathValue.isEmpty())
            throw std::runtime_error("path is required");
        const std::filesystem::path path = std::filesystem::absolute(pathValue.toStdString());
        if (!std::filesystem::is_regular_file(path))
            throw std::runtime_error("Plugin does not exist: " + path.string());

        ESM::ESMReader reader;
        reader.setEncoder(&database.encoder());
        reader.open(path);

        QJsonArray masters;
        for (const ESM::Header::MasterData& master : reader.getGameFiles())
        {
            masters.append(QJsonObject{
                { "name", fromUtf8(master.name) },
                { "size_bytes", static_cast<qint64>(master.size) },
            });
        }

        std::map<QString, int> counts;
        QJsonArray dialogues;
        QJsonArray scripts;
        QJsonArray worldCells;
        QJsonArray worldLands;
        QJsonArray worldNpcs;
        QJsonArray worldItems;
        std::map<std::pair<int, int>, ESM::Land> pluginLands;
        struct PluginCellData
        {
            ESM::Cell mCell;
            std::vector<ESM::CellRef> mReferences;
        };
        std::vector<PluginCellData> pluginCells;
        std::set<std::string> referencedBaseIds;
        struct SourceToValidate
        {
            QString mKind;
            QString mId;
            QString mTopicId;
            QString mSource;
            bool mFullScript = false;
        };
        std::vector<SourceToValidate> sourcesToValidate;
        std::set<std::string> pluginIds;
        QString currentTopic;
        ESM::Dialogue::Type currentType = ESM::Dialogue::Unknown;
        while (reader.hasMoreRecs())
        {
            const ESM::NAME recordName = reader.getRecName();
            std::uint32_t flags = 0;
            reader.getRecHeader(flags);
            const QString type = fromUtf8(recordName.toStringView());
            ++counts[type];
            if (recordName.toInt() == ESM::REC_DIAL)
            {
                ESM::Dialogue dialogue;
                bool deleted = false;
                dialogue.load(reader, deleted);
                currentTopic = refIdString(dialogue.mId);
                pluginIds.insert(keyFor(dialogue.mId));
                currentType = dialogue.mType;
                dialogues.append(QJsonObject{
                    { "record_type", "DIAL" },
                    { "dialogue_id", currentTopic },
                    { "dialogue_type", dialogueTypeName(currentType) },
                    { "deleted", deleted },
                });
            }
            else if (recordName.toInt() == ESM::REC_INFO)
            {
                ESM::DialInfo info;
                bool deleted = false;
                info.load(reader, deleted);
                QJsonObject value = infoJson(info, currentTopic, QString::fromStdString(path.filename().string()));
                value.insert("deleted", deleted);
                dialogues.append(value);
                if (!deleted && !info.mResultScript.empty())
                {
                    sourcesToValidate.push_back({
                        "dialogue_result", refIdString(info.mId), currentTopic, fromUtf8(info.mResultScript), false });
                }
            }
            else if (recordName.toInt() == ESM::REC_SCPT)
            {
                currentTopic.clear();
                currentType = ESM::Dialogue::Unknown;
                ESM::Script script;
                bool deleted = false;
                script.load(reader, deleted);
                pluginIds.insert(keyFor(script.mId));
                scripts.append(QJsonObject{
                    { "script_id", refIdString(script.mId) },
                    { "source_text", fromUtf8(script.mScriptText) },
                    { "deleted", deleted },
                });
                if (!deleted)
                    sourcesToValidate.push_back(
                        { "script", refIdString(script.mId), QString(), fromUtf8(script.mScriptText), true });
            }
            else if (recordName.toInt() == ESM::REC_NPC_)
            {
                currentTopic.clear();
                ESM::NPC npc;
                bool deleted = false;
                npc.load(reader, deleted);
                pluginIds.insert(keyFor(npc.mId));
                QJsonObject value = npcJson(npc, QString::fromStdString(path.filename().string()));
                value.insert("deleted", deleted);
                worldNpcs.append(value);
            }
            else if (recordName.toInt() == ESM::REC_MISC)
            {
                currentTopic.clear();
                ESM::Miscellaneous item;
                bool deleted = false;
                item.load(reader, deleted);
                pluginIds.insert(keyFor(item.mId));
                worldItems.append(QJsonObject{
                    { "item_id", refIdString(item.mId) }, { "name", fromUtf8(item.mName) },
                    { "model", fromUtf8(item.mModel.getOriginal()) }, { "icon", fromUtf8(item.mIcon.getOriginal()) },
                    { "weight", item.mData.mWeight }, { "value", item.mData.mValue }, { "deleted", deleted },
                });
            }
            else if (recordName.toInt() == ESM::REC_LAND)
            {
                currentTopic.clear();
                ESM::Land land;
                bool deleted = false;
                land.load(reader, deleted);
                pluginLands[{ land.mX, land.mY }] = land;
                worldLands.append(landJson({ land, QString::fromStdString(path.filename().string()), deleted }));
            }
            else if (recordName.toInt() == ESM::REC_CELL)
            {
                currentTopic.clear();
                ESM::Cell cell;
                bool deleted = false;
                cell.load(reader, deleted, false);
                PluginCellData cellData;
                cellData.mCell = cell;
                QJsonArray references;
                while (reader.hasMoreSubs())
                {
                    ESM::CellRef cellRef;
                    ESM::MovedCellRef movedRef;
                    bool refDeleted = false;
                    bool moved = false;
                    if (reader.peekNextSub("FRMR") || reader.peekNextSub("MVRF"))
                    {
                        if (ESM::Cell::getNextRef(reader, cellRef, refDeleted, movedRef, moved))
                        {
                            if (!refDeleted)
                            {
                                cellData.mReferences.push_back(cellRef);
                                referencedBaseIds.insert(keyFor(cellRef.mRefID));
                            }
                            references.append(QJsonObject{
                                { "base_id", refIdString(cellRef.mRefID) },
                                { "reference_number", QJsonObject{ { "index", static_cast<qint64>(cellRef.mRefNum.mIndex) },
                                                          { "content_file", cellRef.mRefNum.mContentFile } } },
                                { "position", positionJson(cellRef.mPos) }, { "scale", cellRef.mScale },
                                { "teleport", cellRef.mTeleport }, { "destination_cell", fromUtf8(cellRef.mDestCell) },
                                { "deleted", refDeleted }, { "moved", moved },
                            });
                            continue;
                        }
                    }
                    reader.getSubName();
                    reader.skipHSub();
                }
                QJsonObject value{
                    { "cell_id", refIdString(cell.mId) }, { "name", fromUtf8(cell.mName) },
                    { "interior", !cell.isExterior() }, { "region", refIdString(cell.mRegion) },
                    { "flags", cell.mData.mFlags }, { "reference_count", references.size() },
                    { "references", references }, { "deleted", deleted },
                };
                if (cell.isExterior())
                {
                    value.insert("grid_x", cell.mData.mX);
                    value.insert("grid_y", cell.mData.mY);
                }
                worldCells.append(value);
                pluginCells.push_back(std::move(cellData));
            }
            else
            {
                currentTopic.clear();
                currentType = ESM::Dialogue::Unknown;
                reader.skipRecord();
            }
        }

        QJsonObject recordCounts;
        for (const auto& [type, count] : counts)
            recordCounts.insert(type, count);

        Compiler::Extensions extensions;
        Compiler::registerExtensions(extensions);
        ValidationContext context(database, pluginIds);
        context.setExtensions(&extensions);
        QJsonArray compilerValidation;
        int compilerErrors = 0;
        int compilerWarnings = 0;
        for (const SourceToValidate& source : sourcesToValidate)
        {
            QJsonObject result = compileSource(source.mSource, source.mFullScript, context, extensions);
            result.insert("source_kind", source.mKind);
            result.insert("record_id", source.mId);
            if (!source.mTopicId.isEmpty())
                result.insert("topic_id", source.mTopicId);
            compilerErrors += result.value("error_count").toInt();
            compilerWarnings += result.value("warning_count").toInt();
            compilerValidation.append(result);
        }

        QJsonArray missingReferences;
        for (const std::string& id : referencedBaseIds)
        {
            const ESM::RefId refId = ESM::RefId::stringRefId(id);
            if (!pluginIds.contains(id) && !database.hasId(refId))
                missingReferences.append(fromUtf8(id));
        }
        QJsonArray outOfCellReferences;
        for (const PluginCellData& cell : pluginCells)
        {
            if (!cell.mCell.isExterior())
                continue;
            const float minX = static_cast<float>(cell.mCell.mData.mX * ESM::Cell::sSize);
            const float minY = static_cast<float>(cell.mCell.mData.mY * ESM::Cell::sSize);
            const float maxX = minX + ESM::Cell::sSize;
            const float maxY = minY + ESM::Cell::sSize;
            for (const ESM::CellRef& reference : cell.mReferences)
            {
                if (reference.mPos.pos[0] < minX || reference.mPos.pos[0] >= maxX || reference.mPos.pos[1] < minY
                    || reference.mPos.pos[1] >= maxY)
                {
                    outOfCellReferences.append(QJsonObject{ { "cell_id", refIdString(cell.mCell.mId) },
                        { "base_id", refIdString(reference.mRefID) }, { "position", positionJson(reference.mPos) } });
                }
            }
        }
        QJsonArray landSeams;
        int seamErrors = 0;
        for (const auto& [coordinates, land] : pluginLands)
        {
            const ESM::Land::LandData* data = land.getLandData(ESM::Land::DATA_VHGT);
            if (data == nullptr)
                continue;
            const auto compare = [&](const std::pair<int, int>& neighborCoordinates, bool horizontal) {
                const auto neighbor = pluginLands.find(neighborCoordinates);
                if (neighbor == pluginLands.end())
                    return;
                const ESM::Land::LandData* other = neighbor->second.getLandData(ESM::Land::DATA_VHGT);
                if (other == nullptr)
                    return;
                float maximumDifference = 0;
                for (int i = 0; i < ESM::Land::LAND_SIZE; ++i)
                {
                    const int firstIndex = horizontal ? i * ESM::Land::LAND_SIZE + ESM::Land::LAND_SIZE - 1
                                                      : (ESM::Land::LAND_SIZE - 1) * ESM::Land::LAND_SIZE + i;
                    const int secondIndex = horizontal ? i * ESM::Land::LAND_SIZE : i;
                    maximumDifference
                        = std::max(maximumDifference, std::abs(data->mHeights[firstIndex] - other->mHeights[secondIndex]));
                }
                const bool valid = maximumDifference <= ESM::Land::sHeightScale;
                if (!valid)
                    ++seamErrors;
                landSeams.append(QJsonObject{ { "from", QString("#%1 %2").arg(coordinates.first).arg(coordinates.second) },
                    { "to", QString("#%1 %2").arg(neighborCoordinates.first).arg(neighborCoordinates.second) },
                    { "maximum_height_difference", maximumDifference }, { "valid", valid } });
            };
            compare({ coordinates.first + 1, coordinates.second }, true);
            compare({ coordinates.first, coordinates.second + 1 }, false);
        }
        const bool worldValid = missingReferences.isEmpty() && outOfCellReferences.isEmpty() && seamErrors == 0;
        return {
            { "path", QString::fromStdString(path.string()) },
            { "file_size_bytes", static_cast<qint64>(std::filesystem::file_size(path)) },
            { "author", fromUtf8(reader.getAuthor()) },
            { "description", fromUtf8(reader.getDesc()) },
            { "format_version", reader.esmVersionF() },
            { "declared_record_count", reader.getRecordCount() },
            { "record_counts", recordCounts },
            { "masters", masters },
            { "dialogue_records", dialogues },
            { "scripts", scripts },
            { "world_cells", worldCells },
            { "world_lands", worldLands },
            { "world_npcs", worldNpcs },
            { "world_items", worldItems },
            { "world_validation", QJsonObject{ { "valid", worldValid },
                                      { "cell_count", worldCells.size() }, { "land_count", worldLands.size() },
                                      { "npc_count", worldNpcs.size() }, { "item_count", worldItems.size() },
                                      { "missing_base_references", missingReferences },
                                      { "out_of_cell_references", outOfCellReferences }, { "land_seams", landSeams },
                                      { "land_seam_error_count", seamErrors } } },
            { "compiler_validation", compilerValidation },
            { "compiler_summary",
                QJsonObject{ { "sources_checked", static_cast<int>(sourcesToValidate.size()) },
                    { "error_count", compilerErrors }, { "warning_count", compilerWarnings } } },
            { "valid", compilerErrors == 0 && worldValid },
        };
    }

    QJsonObject writeQuestPatch(Database& database, const QJsonObject& arguments)
    {
        const QString outputValue = arguments.value("output_path").toString();
        if (outputValue.isEmpty())
            throw std::runtime_error("output_path is required");
        const std::filesystem::path output = std::filesystem::absolute(outputValue.toStdString());
        const QString extension = QString::fromStdString(output.extension().string()).toLower();
        if (extension != ".esp")
            throw std::runtime_error("output_path must end in .esp");
        for (const ContentFile& content : database.contentFiles())
        {
            std::error_code ec;
            if (std::filesystem::equivalent(output, content.mPath, ec) && !ec)
                throw std::runtime_error("Refusing to overwrite a loaded content/master file");
        }
        const bool overwrite = arguments.value("overwrite").toBool(false);
        if (std::filesystem::exists(output) && !overwrite)
            throw std::runtime_error("Output already exists; pass overwrite=true to replace it");

        const QJsonArray edits = arguments.value("edits").toArray();
        if (edits.isEmpty())
            throw std::runtime_error("edits must contain at least one edit");
        std::map<std::string, TopicPatch> topics;
        std::map<std::string, ESM::Script> scripts;
        std::map<std::string, ESM::NPC> npcs;
        std::map<std::string, ESM::Miscellaneous> miscellaneous;
        std::map<std::pair<int, int>, ESM::Land> lands;
        std::map<std::string, WorldCellPatch> cells;
        std::uint32_t nextReferenceIndex = 1;
        QJsonArray applied;

        const auto getTopic = [&](const QString& topicId) -> TopicPatch& {
            const std::string key = keyFor(topicId);
            auto it = topics.find(key);
            if (it == topics.end())
                it = topics.emplace(key, makeTopicPatch(database.dialogue(topicId))).first;
            return it->second;
        };

        for (const QJsonValue& value : edits)
        {
            if (!value.isObject())
                throw std::runtime_error("Each edit must be an object");
            const QJsonObject edit = value.toObject();
            const QString op = edit.value("op").toString().toLower();
            if (op == "add_dialogue")
            {
                const QString topicId = edit.value("topic_id").toString();
                const QString type = edit.value("dialogue_type").toString();
                if (topicId.isEmpty() || type.isEmpty())
                    throw std::runtime_error("add_dialogue requires topic_id and dialogue_type");
                const std::string key = keyFor(topicId);
                if (topics.contains(key) || database.dialogues().contains(key))
                    throw std::runtime_error("Dialogue already exists: " + topicId.toStdString());
                TopicPatch topic;
                topic.mDialogue.mId = ESM::RefId::stringRefId(topicId.toUtf8().toStdString());
                topic.mDialogue.mStringId = topicId.toUtf8().toStdString();
                topic.mDialogue.mType = parseDialogueType(type);
                topics.emplace(key, std::move(topic));
                applied.append(QJsonObject{ { "op", op }, { "topic_id", topicId }, { "dialogue_type", type } });
            }
            else if (op == "update_info")
            {
                const QString topicId = edit.value("topic_id").toString();
                const QString infoId = edit.value("info_id").toString();
                TopicPatch& topic = getTopic(topicId);
                const auto it = topic.mInfos.find(keyFor(infoId));
                if (it == topic.mInfos.end())
                    throw std::runtime_error("INFO was not found in topic: " + infoId.toStdString());
                applyInfoFields(it->second, edit);
                topic.mEmit.insert(it->first);
                applied.append(QJsonObject{ { "op", op }, { "topic_id", topicId }, { "info_id", infoId } });
            }
            else if (op == "add_info")
            {
                const QString topicId = edit.value("topic_id").toString();
                const QString infoId = edit.value("info_id").toString();
                if (topicId.isEmpty() || infoId.isEmpty())
                    throw std::runtime_error("add_info requires topic_id and info_id");
                TopicPatch& topic = getTopic(topicId);
                ESM::DialInfo info;
                info.mId = ESM::RefId::stringRefId(infoId.toUtf8().toStdString());
                info.blank();
                info.mData.mType = topic.mDialogue.mType;
                info.mData.mRank = -1;
                info.mData.mGender = ESM::DialInfo::Gender::NA;
                info.mData.mPCrank = -1;
                applyInfoFields(info, edit);
                insertInfo(topic, std::move(info), edit.value("insert_after_info_id").toString(),
                    edit.value("insert_before_info_id").toString());
                applied.append(QJsonObject{ { "op", op }, { "topic_id", topicId }, { "info_id", infoId } });
            }
            else if (op == "delete_info")
            {
                const QString topicId = edit.value("topic_id").toString();
                const QString infoId = edit.value("info_id").toString();
                TopicPatch& topic = getTopic(topicId);
                const std::string key = keyFor(infoId);
                if (!topic.mInfos.contains(key))
                    throw std::runtime_error("INFO was not found in topic: " + infoId.toStdString());
                topic.mEmit.insert(key);
                topic.mDeleted.insert(key);
                applied.append(QJsonObject{ { "op", op }, { "topic_id", topicId }, { "info_id", infoId } });
            }
            else if (op == "update_script")
            {
                const QString scriptId = edit.value("script_id").toString();
                if (scriptId.isEmpty() || !edit.contains("source_text"))
                    throw std::runtime_error("update_script requires script_id and source_text");
                ESM::Script script = database.script(scriptId).mValue;
                script.mScriptText = edit.value("source_text").toString().toUtf8().toStdString();
                script.mScriptData.clear();
                scripts[keyFor(script.mId)] = std::move(script);
                applied.append(QJsonObject{ { "op", op }, { "script_id", scriptId } });
            }
            else if (op == "add_script")
            {
                const QString scriptId = edit.value("script_id").toString();
                if (scriptId.isEmpty() || !edit.contains("source_text"))
                    throw std::runtime_error("add_script requires script_id and source_text");
                const std::string key = keyFor(scriptId);
                if (scripts.contains(key) || database.scripts().contains(key))
                    throw std::runtime_error("Script already exists: " + scriptId.toStdString());
                ESM::Script script;
                script.mId = ESM::RefId::stringRefId(scriptId.toUtf8().toStdString());
                script.blank();
                script.mScriptText = edit.value("source_text").toString().toUtf8().toStdString();
                scripts.emplace(key, std::move(script));
                applied.append(QJsonObject{ { "op", op }, { "script_id", scriptId } });
            }
            else if (op == "add_npc")
            {
                const QString npcId = edit.value("npc_id").toString();
                const QString templateId = edit.value("template_id").toString();
                if (npcId.isEmpty() || templateId.isEmpty() || !edit.contains("name"))
                    throw std::runtime_error("add_npc requires npc_id, template_id, and name");
                const std::string key = keyFor(npcId);
                if (npcs.contains(key) || database.npcs().contains(key))
                    throw std::runtime_error("NPC already exists: " + npcId.toStdString());
                ESM::NPC npc = database.npc(templateId).mValue;
                npc.mId = ESM::RefId::stringRefId(npcId.toUtf8().toStdString());
                npc.mName = edit.value("name").toString().toUtf8().toStdString();
                npc.mRecordFlags = 0;
                setOptionalRef(npc.mRace, edit, "race");
                setOptionalRef(npc.mClass, edit, "class");
                setOptionalRef(npc.mFaction, edit, "faction");
                setOptionalRef(npc.mScript, edit, "script_id");
                setOptionalRef(npc.mHead, edit, "head");
                setOptionalRef(npc.mHair, edit, "hair");
                if (edit.contains("female"))
                    npc.setIsMale(!edit.value("female").toBool());
                const auto setFlag = [&](const char* name, int flag) {
                    if (!edit.contains(name))
                        return;
                    if (edit.value(name).toBool())
                        npc.mFlags |= flag;
                    else
                        npc.mFlags &= ~flag;
                };
                setFlag("essential", ESM::NPC::Essential);
                setFlag("respawn", ESM::NPC::Respawn);
                setFlag("autocalc", ESM::NPC::Autocalc);
                if (edit.contains("level"))
                    npc.mNpdt.mLevel = static_cast<int16_t>(edit.value("level").toInt());
                if (edit.contains("disposition"))
                    npc.mNpdt.mDisposition = static_cast<unsigned char>(
                        std::clamp(edit.value("disposition").toInt(), 0, 100));
                if (edit.contains("reputation"))
                    npc.mNpdt.mReputation = static_cast<unsigned char>(
                        std::clamp(edit.value("reputation").toInt(), 0, 255));
                if (edit.contains("gold"))
                    npc.mNpdt.mGold = edit.value("gold").toInt();
                if (edit.value("clear_inventory").toBool(true))
                    npc.mInventory.mList.clear();
                if (edit.contains("inventory"))
                {
                    npc.mInventory.mList.clear();
                    for (const QJsonValue& itemValue : edit.value("inventory").toArray())
                    {
                        const QJsonObject item = itemValue.toObject();
                        const QString itemId = item.value("item_id").toString();
                        if (itemId.isEmpty())
                            throw std::runtime_error("NPC inventory item_id cannot be empty");
                        npc.mInventory.mList.push_back(
                            { item.value("count").toInt(1), ESM::RefId::stringRefId(itemId.toUtf8().toStdString()) });
                    }
                }
                if (edit.contains("hello"))
                    npc.mAiData.mHello = static_cast<std::uint16_t>(
                        std::clamp(edit.value("hello").toInt(), 0, 65535));
                if (edit.contains("fight"))
                    npc.mAiData.mFight = static_cast<unsigned char>(std::clamp(edit.value("fight").toInt(), 0, 100));
                if (edit.contains("flee"))
                    npc.mAiData.mFlee = static_cast<unsigned char>(std::clamp(edit.value("flee").toInt(), 0, 100));
                if (edit.contains("alarm"))
                    npc.mAiData.mAlarm = static_cast<unsigned char>(std::clamp(edit.value("alarm").toInt(), 0, 100));
                if (edit.contains("services"))
                    npc.mAiData.mServices = edit.value("services").toInt();
                if (edit.contains("wander_distance"))
                {
                    npc.mAiPackage.mList.clear();
                    ESM::AIPackage package{};
                    package.mType = ESM::AI_Wander;
                    package.mWander.mDistance = static_cast<int16_t>(
                        std::clamp(edit.value("wander_distance").toInt(), 0, 32767));
                    package.mWander.mDuration = 5;
                    package.mWander.mTimeOfDay = 0;
                    std::fill(std::begin(package.mWander.mIdle), std::end(package.mWander.mIdle), 0);
                    package.mWander.mShouldRepeat = 1;
                    npc.mAiPackage.mList.push_back(package);
                }
                npcs.emplace(key, std::move(npc));
                applied.append(
                    QJsonObject{ { "op", op }, { "npc_id", npcId }, { "template_id", templateId } });
            }
            else if (op == "add_misc")
            {
                const QString itemId = edit.value("item_id").toString();
                const QString templateId = edit.value("template_id").toString();
                if (itemId.isEmpty() || templateId.isEmpty() || !edit.contains("name"))
                    throw std::runtime_error("add_misc requires item_id, template_id, and name");
                const std::string key = keyFor(itemId);
                if (miscellaneous.contains(key) || database.miscellaneous().contains(key))
                    throw std::runtime_error("MISC already exists: " + itemId.toStdString());
                ESM::Miscellaneous item = database.misc(templateId).mValue;
                item.mId = ESM::RefId::stringRefId(itemId.toUtf8().toStdString());
                item.mName = edit.value("name").toString().toUtf8().toStdString();
                item.mRecordFlags = 0;
                if (edit.contains("weight"))
                    item.mData.mWeight = static_cast<float>(edit.value("weight").toDouble());
                if (edit.contains("value"))
                    item.mData.mValue = edit.value("value").toInt();
                if (edit.contains("is_key"))
                    item.mData.mFlags = edit.value("is_key").toBool() ? ESM::Miscellaneous::Key : 0;
                setOptionalRef(item.mScript, edit, "script_id");
                miscellaneous.emplace(key, std::move(item));
                applied.append(QJsonObject{ { "op", op }, { "item_id", itemId }, { "template_id", templateId } });
            }
            else if (op == "add_land" || op == "update_land")
            {
                const bool update = op == "update_land";
                if (!edit.contains("grid_x") || !edit.contains("grid_y"))
                    throw std::runtime_error("add_land requires grid_x and grid_y");
                const std::pair<int, int> coordinates{ edit.value("grid_x").toInt(), edit.value("grid_y").toInt() };
                if (lands.contains(coordinates) || (!update && database.lands().contains(coordinates)))
                    throw std::runtime_error("LAND already exists at the requested coordinates");
                if (update && !database.lands().contains(coordinates))
                    throw std::runtime_error("LAND to update does not exist at the requested coordinates");
                lands.emplace(coordinates, makeLand(edit));
                applied.append(QJsonObject{ { "op", op }, { "grid_x", coordinates.first },
                    { "grid_y", coordinates.second } });
            }
            else if (op == "add_cell" || op == "update_cell")
            {
                const bool update = op == "update_cell";
                const bool interior = edit.value("interior").toBool(false);
                ESM::Cell cell;
                if (update)
                {
                    ESM::RefId sourceId;
                    if (interior)
                    {
                        const QString sourceCell = edit.value("cell_id").toString();
                        if (sourceCell.isEmpty())
                            throw std::runtime_error("Interior update_cell requires cell_id");
                        sourceId = ESM::RefId::stringRefId(sourceCell.toUtf8().toStdString());
                    }
                    else
                    {
                        if (!edit.contains("grid_x") || !edit.contains("grid_y"))
                            throw std::runtime_error("Exterior update_cell requires grid_x and grid_y");
                        sourceId = ESM::RefId::esm3ExteriorCell(
                            edit.value("grid_x").toInt(), edit.value("grid_y").toInt());
                    }
                    const auto source = database.cells().find(keyFor(sourceId));
                    if (source == database.cells().end() || source->second.mDeleted)
                        throw std::runtime_error("CELL to update was not found: " + sourceId.toString());
                    cell = source->second.mValue;
                }
                else
                    cell.blank();
                if (interior)
                {
                    const QString name = edit.value("name").toString(fromUtf8(cell.mName));
                    if (name.isEmpty())
                        throw std::runtime_error("Interior add_cell requires name");
                    cell.mName = name.toUtf8().toStdString();
                    cell.mData.mFlags |= ESM::Cell::Interior;
                    if (edit.value("has_water").toBool(false))
                        cell.mData.mFlags |= ESM::Cell::HasWater;
                    cell.mWater = static_cast<float>(edit.value("water_height").toDouble(0));
                    cell.mHasWaterHeightSub = edit.value("has_water").toBool(false);
                    const QJsonObject ambient = edit.value("ambient").toObject();
                    cell.mHasAmbi = true;
                    cell.mAmbi.mAmbient = static_cast<ESM::Color>(ambient.value("ambient_color").toDouble(0x404040));
                    cell.mAmbi.mSunlight = static_cast<ESM::Color>(ambient.value("sunlight_color").toDouble(0x707070));
                    cell.mAmbi.mFog = static_cast<ESM::Color>(ambient.value("fog_color").toDouble(0x303030));
                    cell.mAmbi.mFogDensity = static_cast<float>(ambient.value("fog_density").toDouble(0.01));
                }
                else
                {
                    if (!edit.contains("grid_x") || !edit.contains("grid_y"))
                        throw std::runtime_error("Exterior add_cell requires grid_x and grid_y");
                    cell.mData.mX = edit.value("grid_x").toInt();
                    cell.mData.mY = edit.value("grid_y").toInt();
                    if (edit.contains("name"))
                        cell.mName = edit.value("name").toString().toUtf8().toStdString();
                    const QString region = edit.value("region").toString("Bitter Coast Region");
                    cell.mRegion = ESM::RefId::stringRefId(region.toUtf8().toStdString());
                    cell.mMapColor = edit.value("map_color").toInt(0x456b55);
                }
                if (edit.value("no_sleep").toBool(false))
                    cell.mData.mFlags |= ESM::Cell::NoSleep;
                if (edit.value("quasi_exterior").toBool(false))
                    cell.mData.mFlags |= ESM::Cell::QuasiEx;
                cell.updateId();
                const std::string cellKey = keyFor(cell.mId);
                if (cells.contains(cellKey) || (!update && database.cells().contains(cellKey)))
                    throw std::runtime_error("CELL already exists: " + cell.mId.toString());

                WorldCellPatch cellPatch;
                cellPatch.mCell = cell;
                for (const QJsonValue& refValue : edit.value("references").toArray())
                {
                    const QJsonObject refObject = refValue.toObject();
                    const QString baseId = refObject.value("base_id").toString();
                    if (baseId.isEmpty())
                        throw std::runtime_error("Cell reference base_id cannot be empty");
                    ESM::CellRef reference;
                    reference.blank();
                    reference.mRefNum.mIndex = nextReferenceIndex++;
                    reference.mRefNum.mContentFile = 0;
                    reference.mRefID = ESM::RefId::stringRefId(baseId.toUtf8().toStdString());
                    reference.mScale = static_cast<float>(refObject.value("scale").toDouble(1.0));
                    applyPosition(reference.mPos, refObject.value("position").toObject());
                    setOptionalRef(reference.mOwner, refObject, "owner");
                    reference.mCount = refObject.value("count").toInt(1);
                    if (refObject.contains("lock_level"))
                    {
                        reference.mIsLocked = true;
                        reference.mLockLevel = refObject.value("lock_level").toInt();
                        setOptionalRef(reference.mKey, refObject, "key");
                    }
                    const QJsonObject teleport = refObject.value("teleport").toObject();
                    if (!teleport.isEmpty())
                    {
                        reference.mTeleport = true;
                        reference.mDestCell = teleport.value("destination_cell").toString().toUtf8().toStdString();
                        applyPosition(reference.mDoorDest, teleport.value("destination").toObject());
                    }
                    const QString type = npcs.contains(keyFor(baseId)) ? QString("NPC_") : database.recordType(reference.mRefID);
                    const bool persistent = refObject.value("persistent").toBool(
                        type == "NPC_" || type == "CREA" || reference.mTeleport);
                    cellPatch.mReferences.push_back({ reference, persistent });
                }
                cells.emplace(cellKey, std::move(cellPatch));
                applied.append(QJsonObject{ { "op", op }, { "cell_id", refIdString(cell.mId) },
                    { "reference_count", edit.value("references").toArray().size() } });
            }
            else
                throw std::runtime_error("Unknown edit op: " + op.toStdString());
        }

        int recordCount = static_cast<int>(
            scripts.size() + npcs.size() + miscellaneous.size() + lands.size() + cells.size());
        for (const auto& [key, topic] : topics)
            recordCount += 1 + static_cast<int>(topic.mEmit.size());

        std::filesystem::create_directories(output.parent_path());
        std::filesystem::path temporary = output;
        temporary += ".tmp";
        std::filesystem::path backup = output;
        backup += ".openmw-cs-mcp-backup";
        std::error_code cleanupError;
        std::filesystem::remove(temporary, cleanupError);
        if (std::filesystem::exists(backup))
            throw std::runtime_error("A previous overwrite backup exists; move or remove it first: " + backup.string());

        ESM::ESMWriter writer;
        writer.setEncoder(&database.encoder());
        writer.setVersion(ESM::VER_130);
        writer.setType(0);
        writer.setAuthor(arguments.value("author").toString("OpenMW-CS MCP").toUtf8().toStdString());
        writer.setDescription(arguments.value("description")
                                  .toString("Quest patch generated by openmw-cs-mcp")
                                  .toUtf8()
                                  .toStdString());
        writer.setFormatVersion(ESM::DefaultFormatVersion);
        writer.setRecordCount(recordCount);
        for (const ContentFile& content : database.contentFiles())
            writer.addMaster(content.mName.toUtf8().toStdString(), content.mSize);

        try
        {
            std::fstream stream(temporary, std::ios::out | std::ios::binary);
            if (!stream)
                throw std::runtime_error("Could not open temporary output: " + temporary.string());
            writer.save(stream);
            for (const auto& [key, item] : miscellaneous)
            {
                writer.startRecord(ESM::REC_MISC, item.mRecordFlags);
                item.save(writer);
                writer.endRecord(ESM::REC_MISC);
            }
            for (const auto& [key, npc] : npcs)
            {
                writer.startRecord(ESM::REC_NPC_, npc.mRecordFlags);
                npc.save(writer);
                writer.endRecord(ESM::REC_NPC_);
            }
            for (const auto& [coordinates, land] : lands)
            {
                writer.startRecord(ESM::REC_LAND);
                land.save(writer);
                writer.endRecord(ESM::REC_LAND);
            }
            for (const auto& [key, patch] : cells)
            {
                writer.startRecord(ESM::REC_CELL);
                patch.mCell.save(writer);
                for (const auto& [reference, persistent] : patch.mReferences)
                {
                    if (persistent)
                        reference.save(writer);
                }
                const int temporaryCount = static_cast<int>(std::count_if(patch.mReferences.begin(),
                    patch.mReferences.end(), [](const auto& value) { return !value.second; }));
                patch.mCell.saveTempMarker(writer, temporaryCount);
                for (const auto& [reference, persistent] : patch.mReferences)
                {
                    if (!persistent)
                        reference.save(writer);
                }
                writer.endRecord(ESM::REC_CELL);
            }
            for (const auto& [key, topic] : topics)
            {
                writer.startRecord(ESM::REC_DIAL);
                topic.mDialogue.save(writer);
                writer.endRecord(ESM::REC_DIAL);
                const auto order = orderedInfoKeys(topic);
                for (const std::string& infoKey : order)
                {
                    if (!topic.mEmit.contains(infoKey))
                        continue;
                    const ESM::DialInfo& info = topic.mInfos.at(infoKey);
                    writer.startRecord(ESM::REC_INFO);
                    info.save(writer, topic.mDeleted.contains(infoKey));
                    writer.endRecord(ESM::REC_INFO);
                }
            }
            for (const auto& [key, script] : scripts)
            {
                writer.startRecord(ESM::REC_SCPT, script.mRecordFlags);
                script.save(writer);
                writer.endRecord(ESM::REC_SCPT);
            }
            writer.close();
            stream.close();
            validateWrittenPlugin(temporary, database.encoder());
            if (std::filesystem::exists(output))
            {
                std::filesystem::rename(output, backup);
                try
                {
                    std::filesystem::rename(temporary, output);
                    std::filesystem::remove(backup);
                }
                catch (...)
                {
                    if (!std::filesystem::exists(output) && std::filesystem::exists(backup))
                        std::filesystem::rename(backup, output);
                    throw;
                }
            }
            else
                std::filesystem::rename(temporary, output);
        }
        catch (...)
        {
            std::filesystem::remove(temporary, cleanupError);
            throw;
        }

        return {
            { "output_path", QString::fromStdString(output.string()) },
            { "record_count", recordCount },
            { "masters", loadOrderJson(database).value("content_files") },
            { "applied_edits", applied },
            { "validated", true },
            { "validation_scope",
                "TES3 structure and record readability. Call validate_plugin for OpenMW compiler diagnostics; in-game behavior is not simulated." },
            { "note", "The base content files were not modified. Add this ESP after its masters in the OpenMW load order." },
        };
    }

    QJsonObject objectSchema(const QJsonObject& properties, const QJsonArray& required = {})
    {
        QJsonObject result{ { "type", "object" }, { "properties", properties }, { "additionalProperties", false } };
        if (!required.isEmpty())
            result.insert("required", required);
        return result;
    }

    QJsonObject stringProperty(const QString& description)
    {
        return { { "type", "string" }, { "description", description } };
    }

    QJsonArray toolDefinitions()
    {
        const QJsonObject readOnlyAnnotations{
            { "readOnlyHint", true },
            { "destructiveHint", false },
            { "idempotentHint", true },
            { "openWorldHint", false },
        };
        const QJsonObject writeAnnotations{
            { "readOnlyHint", false },
            { "destructiveHint", true },
            { "idempotentHint", false },
            { "openWorldHint", false },
        };
        const QJsonObject limitProperty{
            { "type", "integer" }, { "minimum", 1 }, { "maximum", 200 }, { "default", 50 }
        };
        const QJsonObject editSchema = objectSchema(
            {
                { "op", QJsonObject{ { "type", "string" },
                            { "enum", QJsonArray{ "add_dialogue", "update_info", "add_info", "delete_info",
                                          "add_script", "update_script", "add_npc", "add_misc", "add_land",
                                          "update_land", "add_cell", "update_cell" } } } },
                { "topic_id", stringProperty("Quest journal ID or dialogue topic ID") },
                { "dialogue_type", QJsonObject{ { "type", "string" },
                                       { "enum", QJsonArray{ "topic", "voice", "greeting", "persuasion", "journal" } } } },
                { "info_id", stringProperty("Existing INFO ID, or a unique ID for add_info") },
                { "insert_after_info_id", stringProperty("Optional insertion point for add_info") },
                { "insert_before_info_id", stringProperty("Optional insertion point that gives a new INFO precedence") },
                { "response", stringProperty("Journal or dialogue response text") },
                { "result_script", stringProperty("Dialogue result script source") },
                { "journal_index", QJsonObject{ { "type", "integer" } } },
                { "disposition", QJsonObject{ { "type", "integer" } } },
                { "quest_status", QJsonObject{ { "type", "string" },
                                      { "enum", QJsonArray{ "none", "name", "finished", "restart" } } } },
                { "actor", stringProperty("Actor filter; empty string clears it") },
                { "race", stringProperty("Race filter; empty string clears it") },
                { "class", stringProperty("Class filter; empty string clears it") },
                { "faction", stringProperty("Faction filter; empty string clears it") },
                { "player_faction", stringProperty("Player faction filter; empty string clears it") },
                { "cell", stringProperty("Cell filter; empty string clears it") },
                { "factionless", QJsonObject{ { "type", "boolean" } } },
                { "conditions", QJsonObject{ { "type", "array" }, { "items", QJsonObject{ { "type", "object" } } } } },
                { "script_id", stringProperty("Script ID for update_script") },
                { "source_text", stringProperty("Complete script source for update_script") },
                { "npc_id", stringProperty("Unique NPC ID for add_npc") },
                { "template_id", stringProperty("Existing NPC_ or MISC record to clone safely") },
                { "item_id", stringProperty("Unique MISC ID for add_misc, or an inventory item ID") },
                { "name", stringProperty("Display name for an NPC, item, or cell") },
                { "cell_id", stringProperty("Existing interior CELL ID for update_cell") },
                { "female", QJsonObject{ { "type", "boolean" } } },
                { "essential", QJsonObject{ { "type", "boolean" } } },
                { "respawn", QJsonObject{ { "type", "boolean" } } },
                { "autocalc", QJsonObject{ { "type", "boolean" } } },
                { "level", QJsonObject{ { "type", "integer" } } },
                { "reputation", QJsonObject{ { "type", "integer" } } },
                { "gold", QJsonObject{ { "type", "integer" } } },
                { "head", stringProperty("Optional NPC head override") },
                { "hair", stringProperty("Optional NPC hair override") },
                { "clear_inventory", QJsonObject{ { "type", "boolean" }, { "default", true } } },
                { "inventory", QJsonObject{ { "type", "array" }, { "items", QJsonObject{ { "type", "object" } } } } },
                { "hello", QJsonObject{ { "type", "integer" } } },
                { "fight", QJsonObject{ { "type", "integer" } } },
                { "flee", QJsonObject{ { "type", "integer" } } },
                { "alarm", QJsonObject{ { "type", "integer" } } },
                { "services", QJsonObject{ { "type", "integer" } } },
                { "wander_distance", QJsonObject{ { "type", "integer" }, { "minimum", 0 } } },
                { "weight", QJsonObject{ { "type", "number" } } },
                { "value", QJsonObject{ { "type", "integer" } } },
                { "is_key", QJsonObject{ { "type", "boolean" } } },
                { "grid_x", QJsonObject{ { "type", "integer" } } },
                { "grid_y", QJsonObject{ { "type", "integer" } } },
                { "interior", QJsonObject{ { "type", "boolean" }, { "default", false } } },
                { "region", stringProperty("Exterior region ID") },
                { "map_color", QJsonObject{ { "type", "integer" } } },
                { "has_water", QJsonObject{ { "type", "boolean" } } },
                { "water_height", QJsonObject{ { "type", "number" } } },
                { "no_sleep", QJsonObject{ { "type", "boolean" } } },
                { "quasi_exterior", QJsonObject{ { "type", "boolean" } } },
                { "ambient", QJsonObject{ { "type", "object" } } },
                { "references", QJsonObject{ { "type", "array" }, { "items", QJsonObject{ { "type", "object" } } } } },
                { "terrain", QJsonObject{ { "type", "object" } } },
                { "heights", QJsonObject{ { "type", "array" }, { "minItems", 4225 }, { "maxItems", 4225 } } },
                { "vertex_color", QJsonObject{ { "type", "integer" }, { "minimum", 0 }, { "maximum", 255 } } },
            },
            QJsonArray{ "op" });

        return {
            QJsonObject{
                { "name", "get_load_order" },
                { "title", "Get OpenMW load order" },
                { "description", "Show the OpenMW data files loaded by this server, in effective load order." },
                { "inputSchema", objectSchema({}) },
                { "annotations", readOnlyAnnotations },
            },
            QJsonObject{
                { "name", "find_quests" },
                { "title", "Find Morrowind quests" },
                { "description", "Find Morrowind quest journals by ID, title, or journal text." },
                { "inputSchema", objectSchema({ { "query", stringProperty("Case-insensitive search text; empty lists quests") },
                                      { "limit", limitProperty } }) },
                { "annotations", readOnlyAnnotations },
            },
            QJsonObject{
                { "name", "get_quest" },
                { "title", "Inspect a quest" },
                { "description", "Inspect a quest's journal stages plus dialogue and scripts that directly reference its journal ID." },
                { "inputSchema", objectSchema({ { "quest_id", stringProperty("Exact quest journal ID") } },
                                      QJsonArray{ "quest_id" }) },
                { "annotations", readOnlyAnnotations },
            },
            QJsonObject{
                { "name", "get_area_quests" },
                { "title", "Discover quests in an area" },
                { "description", "Find quests referenced by dialogue or attached scripts belonging to actors and objects placed in matching cells." },
                { "inputSchema", objectSchema({ { "area", stringProperty("Cell-name fragment such as Seyda Neen or Balmora") },
                                                   { "scope", QJsonObject{ { "type", "string" },
                                                                  { "enum", QJsonArray{ "starts", "all" } },
                                                                  { "default", "starts" } } } },
                                      QJsonArray{ "area" }) },
                { "annotations", readOnlyAnnotations },
            },
            QJsonObject{
                { "name", "find_environments" },
                { "title", "Find cells and environments" },
                { "description", "Find interior and exterior cells by name, region, or cell ID, with placement and NPC counts." },
                { "inputSchema", objectSchema({ { "query", stringProperty("Optional cell-name, region, or ID fragment") },
                                      { "limit", QJsonObject{ { "type", "integer" }, { "minimum", 1 },
                                                     { "maximum", 500 }, { "default", 100 } } } }) },
                { "annotations", readOnlyAnnotations },
            },
            QJsonObject{
                { "name", "get_environment" },
                { "title", "Inspect an environment or level" },
                { "description", "Inspect matching CELL and LAND records, placed references, terrain samples, bounds, and local NPC definitions." },
                { "inputSchema", objectSchema({ { "area", stringProperty("Cell-name or region fragment, including matching interiors") },
                                                   { "cell_id", stringProperty("Exact CELL ID or interior name") },
                                                   { "grid_x", QJsonObject{ { "type", "integer" } } },
                                                   { "grid_y", QJsonObject{ { "type", "integer" } } },
                                                   { "radius", QJsonObject{ { "type", "integer" }, { "minimum", 0 },
                                                                  { "maximum", 10 }, { "default", 0 } } },
                                                   { "max_references", QJsonObject{ { "type", "integer" },
                                                                          { "minimum", 1 }, { "maximum", 10000 },
                                                                          { "default", 2000 } } } }) },
                { "annotations", readOnlyAnnotations },
            },
            QJsonObject{
                { "name", "search_records" },
                { "title", "Search ESM records" },
                { "description", "Search IDs and textual subrecords across dialogue, scripts, objects, NPCs, containers, cells, and other ESM3 records." },
                { "inputSchema", objectSchema(
                      { { "query", stringProperty("Case-insensitive text or record ID fragment") },
                          { "record_type", stringProperty("Optional four-character type such as INFO, SCPT, CLOT, CONT, NPC_, or CELL") },
                          { "limit", limitProperty } },
                      QJsonArray{ "query" }) },
                { "annotations", readOnlyAnnotations },
            },
            QJsonObject{
                { "name", "get_record" },
                { "title", "Get an ESM record" },
                { "description", "Get structured DIAL/INFO/SCPT/NPC_/MISC/CELL/LAND data or indexed textual fields for another ESM3 record." },
                { "inputSchema", objectSchema(
                      { { "record_type", stringProperty("Four-character record type") },
                          { "record_id", stringProperty("Exact record ID") },
                          { "topic_id", stringProperty("Required only to disambiguate an INFO ID") } },
                      QJsonArray{ "record_type", "record_id" }) },
                { "annotations", readOnlyAnnotations },
            },
            QJsonObject{
                { "name", "write_quest_patch" },
                { "title", "Write a quest patch ESP" },
                { "description", "Safely write quest/dialogue/script edits to a separate ESP patch. Loaded masters are never overwritten." },
                { "inputSchema", objectSchema(
                      { { "output_path", stringProperty("Destination .esp path") },
                          { "author", stringProperty("TES3 header author") },
                          { "description", stringProperty("TES3 header description") },
                          { "overwrite", QJsonObject{ { "type", "boolean" }, { "default", false } } },
                          { "edits", QJsonObject{ { "type", "array" }, { "minItems", 1 }, { "items", editSchema } } } },
                      QJsonArray{ "output_path", "edits" }) },
                { "annotations", writeAnnotations },
            },
            QJsonObject{
                { "name", "write_world_plugin" },
                { "title", "Write a world and quest plugin" },
                { "description", "Safely create a separate ESP containing new terrain, exterior/interior cells, placed references, cloned NPCs/items, scripts, dialogue, and quests." },
                { "inputSchema", objectSchema(
                      { { "output_path", stringProperty("Destination .esp path") },
                          { "author", stringProperty("TES3 header author") },
                          { "description", stringProperty("TES3 header description") },
                          { "overwrite", QJsonObject{ { "type", "boolean" }, { "default", false } } },
                          { "edits", QJsonObject{ { "type", "array" }, { "minItems", 1 }, { "items", editSchema } } } },
                      QJsonArray{ "output_path", "edits" }) },
                { "annotations", writeAnnotations },
            },
            QJsonObject{
                { "name", "validate_plugin" },
                { "title", "Validate and inspect a plugin" },
                { "description", "Open an ESM3 plugin, inspect its structured quest records, and compile every dialogue result and full script with OpenMW's compiler." },
                { "inputSchema", objectSchema({ { "path", stringProperty("Path to an ESM3 ESM or ESP") } },
                                      QJsonArray{ "path" }) },
                { "annotations", readOnlyAnnotations },
            },
        };
    }

    QJsonObject callTool(Database& database, const QString& name, const QJsonObject& arguments)
    {
        if (name == "get_load_order")
            return loadOrderJson(database);
        if (name == "find_quests")
            return findQuests(database, arguments);
        if (name == "get_quest")
            return getQuest(database, arguments);
        if (name == "get_area_quests")
            return getAreaQuests(database, arguments);
        if (name == "find_environments")
            return findEnvironments(database, arguments);
        if (name == "get_environment")
            return getEnvironment(database, arguments);
        if (name == "search_records")
            return searchRecords(database, arguments);
        if (name == "get_record")
            return getRecord(database, arguments);
        if (name == "write_quest_patch" || name == "write_world_plugin")
            return writeQuestPatch(database, arguments);
        if (name == "validate_plugin")
            return validatePlugin(database, arguments);
        throw std::runtime_error("Unknown tool: " + name.toStdString());
    }

    void writeMessage(const QJsonObject& message)
    {
        std::cout << QJsonDocument(message).toJson(QJsonDocument::Compact).constData() << '\n';
        std::cout.flush();
    }

    QJsonObject jsonRpcError(const QJsonValue& id, int code, const QString& message)
    {
        return {
            { "jsonrpc", "2.0" },
            { "id", id },
            { "error", QJsonObject{ { "code", code }, { "message", message } } },
        };
    }

    void serve(Database& database)
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            if (line.empty())
                continue;
            QJsonParseError parseError;
            const QJsonDocument document = QJsonDocument::fromJson(QByteArray::fromStdString(line), &parseError);
            if (parseError.error != QJsonParseError::NoError || !document.isObject())
            {
                writeMessage(jsonRpcError(QJsonValue::Null, -32700, "Invalid JSON: " + parseError.errorString()));
                continue;
            }
            const QJsonObject request = document.object();
            const QJsonValue id = request.value("id");
            const QString method = request.value("method").toString();
            if (method.startsWith("notifications/"))
                continue;

            try
            {
                QJsonValue result;
                if (method == "initialize")
                {
                    const QString requested = request.value("params").toObject().value("protocolVersion").toString();
                    static const std::set<QString> supportedVersions{
                        "2024-11-05", "2025-03-26", "2025-06-18", "2025-11-25"
                    };
                    const QString negotiated
                        = supportedVersions.contains(requested) ? requested : QString("2025-11-25");
                    result = QJsonObject{
                        { "protocolVersion", negotiated },
                        { "capabilities", QJsonObject{ { "tools", QJsonObject{ { "listChanged", false } } } } },
                        { "serverInfo", QJsonObject{ { "name", fromUtf8(sServerName) },
                                            { "version", fromUtf8(sServerVersion) } } },
                        { "instructions", "Inspect quests with get_quest, environments with get_environment, and records with get_record. Write changes only to separate ESPs using write_quest_patch or write_world_plugin." },
                    };
                }
                else if (method == "ping")
                    result = QJsonObject{};
                else if (method == "tools/list")
                    result = QJsonObject{ { "tools", toolDefinitions() } };
                else if (method == "tools/call")
                {
                    const QJsonObject params = request.value("params").toObject();
                    const QJsonObject value = callTool(database, params.value("name").toString(),
                        params.value("arguments").toObject());
                    const QString text = QString::fromUtf8(QJsonDocument(value).toJson(QJsonDocument::Indented));
                    result = QJsonObject{
                        { "content", QJsonArray{ QJsonObject{ { "type", "text" }, { "text", text } } } },
                        { "structuredContent", value },
                        { "isError", false },
                    };
                }
                else
                {
                    writeMessage(jsonRpcError(id, -32601, "Method not found: " + method));
                    continue;
                }
                writeMessage(QJsonObject{ { "jsonrpc", "2.0" }, { "id", id }, { "result", result } });
            }
            catch (const std::exception& error)
            {
                if (method == "tools/call")
                {
                    writeMessage(QJsonObject{
                        { "jsonrpc", "2.0" },
                        { "id", id },
                        { "result", QJsonObject{
                                        { "content", QJsonArray{ QJsonObject{ { "type", "text" },
                                                                 { "text", QString::fromUtf8(error.what()) } } } },
                                        { "isError", true },
                                    } },
                    });
                }
                else
                    writeMessage(jsonRpcError(id, -32603, QString::fromUtf8(error.what())));
            }
        }
    }
}

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    std::filesystem::path config = QDir::homePath().toStdString();
    config /= ".config/openmw/openmw.cfg";

    for (int i = 1; i < argc; ++i)
    {
        const std::string argument = argv[i];
        if (argument == "--config" && i + 1 < argc)
            config = argv[++i];
        else if (argument == "--help" || argument == "-h")
        {
            std::cout << "Usage: openmw-cs-mcp [--config /path/to/openmw.cfg]\n"
                         "Runs an MCP server over stdin/stdout using the configured OpenMW load order.\n";
            return 0;
        }
        else
        {
            std::cerr << "Unknown argument: " << argument << '\n';
            return 2;
        }
    }

    try
    {
        const Configuration configuration = readConfiguration(config);
        Database database(configuration);
        std::cerr << "openmw-cs-mcp loaded " << database.contentFiles().size() << " content files, "
                  << database.dialogues().size() << " dialogues, and " << database.scripts().size() << " scripts\n";
        serve(database);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "openmw-cs-mcp: " << error.what() << '\n';
        return 1;
    }
}
