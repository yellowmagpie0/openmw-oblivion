#include "oblivionscriptmanager.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <type_traits>

#include <components/debug/debuglog.hpp>
#include <components/esm4/loadacti.hpp>
#include <components/esm4/loadachr.hpp>
#include <components/esm4/loadalch.hpp>
#include <components/esm4/loadarmo.hpp>
#include <components/esm4/loadbook.hpp>
#include <components/esm4/loadclot.hpp>
#include <components/esm4/loadcont.hpp>
#include <components/esm4/loadcrea.hpp>
#include <components/esm4/loaddoor.hpp>
#include <components/esm4/loadflor.hpp>
#include <components/esm4/loadfurn.hpp>
#include <components/esm4/loadglob.hpp>
#include <components/esm4/loadingr.hpp>
#include <components/esm4/loadkeym.hpp>
#include <components/esm4/loadligh.hpp>
#include <components/esm4/loadmisc.hpp>
#include <components/esm4/loadnpc.hpp>
#include <components/esm4/loadrace.hpp>
#include <components/esm4/loadqust.hpp>
#include <components/esm4/loadrefr.hpp>
#include <components/esm4/loadscpt.hpp>
#include <components/esm4/loadsndr.hpp>
#include <components/esm4/loadsoun.hpp>
#include <components/esm4/loadweap.hpp>
#include <components/misc/strings/algorithm.hpp>
#include <components/misc/strings/lower.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/vfs/manager.hpp>
#include <components/vfs/recursivedirectoryiterator.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/soundmanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwsound/sound.hpp"
#include "action.hpp"
#include "class.hpp"
#include "esmstore.hpp"
#include "globalvariablename.hpp"
#include "worldimp.hpp"
#include "weather.hpp"

namespace MWWorld
{
    namespace
    {
        std::string lower(std::string_view value)
        {
            return Misc::StringUtils::lowerCase(value);
        }

        std::string jsonEscape(std::string_view value)
        {
            std::ostringstream out;
            for (const unsigned char c : value)
            {
                switch (c)
                {
                    case '\\': out << "\\\\"; break;
                    case '"': out << "\\\""; break;
                    case '\n': out << "\\n"; break;
                    case '\r': out << "\\r"; break;
                    case '\t': out << "\\t"; break;
                    default:
                        if (c < 0x20)
                            out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << unsigned(c);
                        else
                            out << c;
                }
            }
            return out.str();
        }

        std::vector<std::string> words(std::string_view line)
        {
            std::vector<std::string> result;
            std::string current;
            char quote = 0;
            for (std::size_t i = 0; i < line.size(); ++i)
            {
                const char c = line[i];
                if (quote != 0)
                {
                    if (c == quote)
                        quote = 0;
                    else if (c == '\\' && i + 1 < line.size())
                        current.push_back(line[++i]);
                    else
                        current.push_back(c);
                }
                else if (c == '"' || c == '\'')
                    quote = c;
                else if (std::isspace(static_cast<unsigned char>(c)))
                {
                    if (!current.empty())
                    {
                        result.push_back(std::move(current));
                        current.clear();
                    }
                }
                else if (c == '#')
                    break;
                else
                    current.push_back(c);
            }
            if (!current.empty())
                result.push_back(std::move(current));
            return result;
        }

        ESM::FormId scriptId(const Ptr& ptr)
        {
            switch (ptr.getClass().getType())
            {
                case ESM::REC_ACTI4: return ptr.get<ESM4::Activator>()->mBase->mScriptId;
                case ESM::REC_ALCH4: return ptr.get<ESM4::Potion>()->mBase->mScriptId;
                case ESM::REC_ARMO4: return ptr.get<ESM4::Armor>()->mBase->mScriptId;
                case ESM::REC_BOOK4: return ptr.get<ESM4::Book>()->mBase->mScriptId;
                case ESM::REC_CLOT4: return ptr.get<ESM4::Clothing>()->mBase->mScriptId;
                case ESM::REC_CONT4: return ptr.get<ESM4::Container>()->mBase->mScriptId;
                case ESM::REC_CREA4: return ptr.get<ESM4::Creature>()->mBase->mScriptId;
                case ESM::REC_DOOR4: return ptr.get<ESM4::Door>()->mBase->mScriptId;
                case ESM::REC_FLOR4: return ptr.get<ESM4::Flora>()->mBase->mScriptId;
                case ESM::REC_FURN4: return ptr.get<ESM4::Furniture>()->mBase->mScriptId;
                case ESM::REC_INGR4: return ptr.get<ESM4::Ingredient>()->mBase->mScriptId;
                case ESM::REC_KEYM4: return ptr.get<ESM4::Key>()->mBase->mScriptId;
                case ESM::REC_LIGH4: return ptr.get<ESM4::Light>()->mBase->mScriptId;
                case ESM::REC_MISC4: return ptr.get<ESM4::MiscItem>()->mBase->mScriptId;
                case ESM::REC_NPC_4: return ptr.get<ESM4::Npc>()->mBase->mScriptId;
                case ESM::REC_WEAP4: return ptr.get<ESM4::Weapon>()->mBase->mScriptId;
                default: return {};
            }
        }

        std::int32_t boundedCount(const ObScript::Value& value)
        {
            return static_cast<std::int32_t>(std::clamp<std::int64_t>(ObScript::asInteger(value),
                std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()));
        }

        void changeInventory(
            std::vector<ESM4::RuntimeInventoryItem>& inventory, const ESM::FormKey& item, std::int32_t delta)
        {
            if (item.isNull() || delta == 0)
                return;
            auto found = std::find_if(inventory.begin(), inventory.end(),
                [&](const ESM4::RuntimeInventoryItem& value) { return value.mBase == item; });
            if (found == inventory.end())
                inventory.push_back({ item, delta });
            else
                found->mCount = static_cast<std::int32_t>(std::clamp<std::int64_t>(
                    static_cast<std::int64_t>(found->mCount) + delta,
                    std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()));
            std::erase_if(inventory, [](const auto& value) { return value.mCount == 0; });
            std::sort(inventory.begin(), inventory.end(), [](const auto& left, const auto& right) {
                return left.mBase < right.mBase;
            });
        }

        std::int32_t inventoryCount(
            const std::vector<ESM4::RuntimeInventoryItem>& inventory, const ESM::FormKey& item)
        {
            const auto found = std::find_if(inventory.begin(), inventory.end(),
                [&](const ESM4::RuntimeInventoryItem& value) { return value.mBase == item; });
            return found == inventory.end() ? 0 : found->mCount;
        }

        ESM4::RuntimeScriptValue saveValue(const ObScript::Value& value)
        {
            return std::visit(
                [](const auto& item) -> ESM4::RuntimeScriptValue {
                    using T = std::decay_t<decltype(item)>;
                    if constexpr (std::is_same_v<T, ObScript::ReferenceValue>)
                    {
                        if (!item.mKey.isNull())
                            return item.mKey;
                        if (!item.mName.empty())
                            return item.mName;
                        return std::monostate{};
                    }
                    else
                        return item;
                },
                value);
        }

        ObScript::Value loadValue(const ESM4::RuntimeScriptValue& value)
        {
            return std::visit(
                [](const auto& item) -> ObScript::Value {
                    using T = std::decay_t<decltype(item)>;
                    if constexpr (std::is_same_v<T, ESM::FormKey>)
                        return ObScript::ReferenceValue{ item, {} };
                    else
                        return item;
                },
                value);
        }
    }

    OblivionScriptManager::OblivionScriptManager(
        World& world, ESMStore& store, const std::vector<std::string>& contentFiles)
        : mWorld(world)
        , mStore(store)
        , mResolver(contentFiles)
        , mCache(&mCoverage)
    {
        compileCorpus();
        indexNativeVoices();
        loadScheduledEvents();
    }

    void OblivionScriptManager::compileCorpus()
    {
        ObScript::Corpus corpus;
        const auto revision = [&](const ESM::FormKey& key) {
            const ESM::FormRecordMetadata* record = mStore.getFormKeyIndex().winner(key);
            return record == nullptr ? std::string{} : record->mWinningPlugin;
        };
        for (const ESM4::Script& script : mStore.get<ESM4::Script>())
            corpus.add(script, revision(script.mFormKey));
        for (const ESM4::Quest& quest : mStore.get<ESM4::Quest>())
        {
            corpus.add(quest, revision(quest.mFormKey));
            const ESM::FormKey script = mResolver.toFormKey(quest.mQuestScript);
            if (!script.isNull())
                mQuestScripts[quest.mFormKey] = script;
            ESM4::RuntimeQuestState state;
            state.mQuest = quest.mFormKey;
            state.mRunning = (quest.mData.flags & ESM4::Quest::Flag_StartGameEnabled) != 0;
            mQuests.emplace(state.mQuest, std::move(state));
        }
        for (const ESM4::DialogInfo& info : mStore.get<ESM4::DialogInfo>())
        {
            corpus.add(info, revision(info.mFormKey));
            const ESM::FormRecordMetadata* metadata = mStore.getFormKeyIndex().resolve(info.mFormKey);
            if (metadata != nullptr && metadata->mParent)
                mTopicInfos[*metadata->mParent].push_back(info.mFormKey);
        }
        corpus.finalize();

        for (const ObScript::ScriptUnit& unit : corpus.units())
        {
            const ObScript::CompilationResult result = mCache.compile(unit);
            if (!result.mProgram)
            {
                ++mCompilationFailures;
                for (const auto& diagnostic : result.mDiagnostics)
                    Log(Debug::Error) << "M7 ObScript compile: unit=" << unit.mId.serialize()
                                      << " code=" << diagnostic.mDiagnostic.mCode
                                      << " message=" << diagnostic.mDiagnostic.mMessage;
                continue;
            }
            ++mCompiledUnits;
            mProgramsByUnit.emplace(unit.mId.serialize(), result.mProgram);
            switch (unit.mId.mContext)
            {
                case ObScript::ExecutionContext::Object:
                case ObScript::ExecutionContext::Quest:
                case ObScript::ExecutionContext::Effect:
                case ObScript::ExecutionContext::Global:
                    mScripts[unit.mId.mOwner] = result.mProgram;
                    break;
                case ObScript::ExecutionContext::DialogueResult:
                    mDialogueResults[unit.mId.mOwner].push_back(result.mProgram);
                    break;
                case ObScript::ExecutionContext::QuestResult:
                    mQuestResults[unit.mId.mOwner].push_back(result.mProgram);
                    break;
            }
        }
        const auto indexBaseScripts = [&](const auto& store) {
            for (const auto& base : store)
            {
                const ESM::FormKey script = mResolver.toFormKey(base.mScriptId);
                const auto program = mScripts.find(script);
                const auto baseKey = store.findFormKey(ESM::RefId(base.mId));
                if (program != mScripts.end() && baseKey)
                    mBaseScripts[*baseKey] = program->second;
            }
        };
        indexBaseScripts(mStore.get<ESM4::Activator>());
        indexBaseScripts(mStore.get<ESM4::Potion>());
        indexBaseScripts(mStore.get<ESM4::Armor>());
        indexBaseScripts(mStore.get<ESM4::Book>());
        indexBaseScripts(mStore.get<ESM4::Clothing>());
        indexBaseScripts(mStore.get<ESM4::Container>());
        indexBaseScripts(mStore.get<ESM4::Creature>());
        indexBaseScripts(mStore.get<ESM4::Door>());
        indexBaseScripts(mStore.get<ESM4::Flora>());
        indexBaseScripts(mStore.get<ESM4::Furniture>());
        indexBaseScripts(mStore.get<ESM4::Ingredient>());
        indexBaseScripts(mStore.get<ESM4::Key>());
        indexBaseScripts(mStore.get<ESM4::Light>());
        indexBaseScripts(mStore.get<ESM4::MiscItem>());
        indexBaseScripts(mStore.get<ESM4::Npc>());
        indexBaseScripts(mStore.get<ESM4::Weapon>());
        const auto indexReferences = [&](const auto& store) {
            for (const auto& reference : store)
                if (!reference.mFormKey.isNull())
                    mReferenceBases[reference.mFormKey] = mResolver.toFormKey(reference.mBaseObj);
        };
        indexReferences(mStore.get<ESM4::Reference>());
        indexReferences(mStore.get<ESM4::ActorCharacter>());
        indexReferences(mStore.get<ESM4::ActorCreature>());
        Log(Debug::Info) << "M7 ObScript runtime: compiled=" << mCompiledUnits
                         << " failed=" << mCompilationFailures << " scripts=" << mScripts.size()
                         << " quest-results=" << mQuestResults.size()
                         << " dialogue-results=" << mDialogueResults.size();
    }

    void OblivionScriptManager::indexNativeVoices()
    {
        const VFS::Manager* vfs = MWBase::Environment::get().getResourceSystem()->getVFS();
        if (vfs == nullptr)
            return;
        for (const VFS::Path::Normalized& path
            : vfs->getRecursiveDirectoryIterator(VFS::Path::NormalizedView("sound/voice/")))
        {
            const std::string_view value = path.value();
            if (!value.ends_with(".mp3"))
                continue;
            constexpr std::string_view prefix = "sound/voice/";
            const std::size_t pluginEnd = value.find('/', prefix.size());
            if (pluginEnd == std::string_view::npos)
                continue;
            const std::size_t response = value.rfind('_', value.size() - 5);
            const std::size_t form = response == std::string_view::npos ? response : value.rfind('_', response - 1);
            if (form == std::string_view::npos || response - form != 9)
                continue;
            std::uint32_t localId = 0;
            const std::string_view token = value.substr(form + 1, 8);
            const auto parsed = std::from_chars(token.data(), token.data() + token.size(), localId, 16);
            if (parsed.ec == std::errc{} && parsed.ptr == token.data() + token.size())
                mNativeVoiceFiles[ESM::FormKey::content(value.substr(prefix.size(), pluginEnd - prefix.size()), localId)]
                    .push_back(path.value());
        }
        for (auto& [_, paths] : mNativeVoiceFiles)
            std::sort(paths.begin(), paths.end());
        Log(Debug::Info) << "M10 native voice index: infos=" << mNativeVoiceFiles.size();
    }

    std::optional<std::string> OblivionScriptManager::findNativeVoice(
        const ESM::FormKey& topic, const Ptr& actor, const ESM::FormKey& voiceType) const
    {
        std::string race;
        std::string sex;
        const ESM4::Npc* npc = nullptr;
        if (!voiceType.isNull())
            npc = mStore.search<ESM4::Npc>(voiceType);
        if (npc == nullptr && !actor.isEmpty() && actor.getClass().getType() == ESM::REC_NPC_4)
            npc = actor.get<ESM4::Npc>()->mBase;
        if (npc != nullptr)
        {
            if (const ESM4::Race* raceRecord = mStore.get<ESM4::Race>().search(ESM::RefId(npc->mRace)))
                race = lower(raceRecord->mFullName);
            sex = (npc->mBaseConfig.tes4.flags & ESM4::Npc::TES4_Female) != 0 ? "f" : "m";
        }

        const auto topicInfos = mTopicInfos.find(topic);
        if (topicInfos == mTopicInfos.end())
            return std::nullopt;
        std::optional<std::string> fallback;
        for (const ESM::FormKey& info : topicInfos->second)
        {
            const auto files = mNativeVoiceFiles.find(info);
            if (files == mNativeVoiceFiles.end())
                continue;
            for (const std::string& path : files->second)
            {
                if (!fallback)
                    fallback = path;
                if (!race.empty() && path.find("/" + race + "/" + sex + "/") != std::string::npos)
                    return path;
            }
        }
        return fallback;
    }

    void OblivionScriptManager::loadScheduledEvents()
    {
        if (const char* report = std::getenv("OPENMW_OBSCRIPT_REPORT"))
            mReportPath = report;
        const char* path = std::getenv("OPENMW_OBSCRIPT_EVENTS");
        if (path == nullptr || *path == '\0')
            return;
        std::ifstream input(path);
        if (!input)
            throw std::runtime_error("Cannot open OPENMW_OBSCRIPT_EVENTS file " + std::string(path));
        std::string line;
        std::size_t lineNumber = 0;
        while (std::getline(input, line))
        {
            ++lineNumber;
            std::vector<std::string> tokens = words(line);
            if (tokens.empty())
                continue;
            ScheduledEvent event;
            if (lower(tokens.front()) == "at")
            {
                if (tokens.size() < 3)
                    throw std::runtime_error("Invalid M7 event line " + std::to_string(lineNumber));
                event.mAt = std::stod(tokens[1]);
                tokens.erase(tokens.begin(), tokens.begin() + 2);
            }
            event.mWords = std::move(tokens);
            mScheduledEvents.push_back(std::move(event));
        }
        std::stable_sort(mScheduledEvents.begin(), mScheduledEvents.end(),
            [](const ScheduledEvent& left, const ScheduledEvent& right) { return left.mAt < right.mAt; });
        Log(Debug::Info) << "M7 ObScript acceptance events: count=" << mScheduledEvents.size();
    }

    void OblivionScriptManager::clear()
    {
        mInstances.clear();
        for (auto& [_, quest] : mQuests)
        {
            quest.mStage = 0;
            quest.mCompletedStages.clear();
            if (const ESM4::Quest* record = mStore.search<ESM4::Quest>(quest.mQuest))
                quest.mRunning = (record->mData.flags & ESM4::Quest::Flag_StartGameEnabled) != 0;
        }
        mDiagnostics.clear();
        mTrace.clear();
        mSequence = 0;
        mDepth = 0;
        mElapsed = 0;
        for (ScheduledEvent& event : mScheduledEvents)
            event.mExecuted = false;
    }

    ESM::FormKey OblivionScriptManager::keyFor(const Ptr& ptr) const
    {
        if (ptr.isEmpty())
            return {};
        if (ptr == mWorld.getPlayerConstPtr())
            return ESM::FormKey::dynamic("player", 1);
        return ptr.getCellRef().getFormKey();
    }

    Ptr OblivionScriptManager::ptrFor(const ESM::FormKey& key)
    {
        if (key == ESM::FormKey::dynamic("player", 1))
            return mWorld.getPlayerPtr();
        const std::optional<ESM::FormId> id = mResolver.toFormId(key);
        if (!id)
            return {};
        Ptr ptr = mWorld.mWorldModel.getPtr(*id);
        if (!ptr.isEmpty())
            return ptr;
        const ESM::FormRecordMetadata* metadata = mStore.getFormKeyIndex().resolve(key);
        if (metadata != nullptr && metadata->mParent)
            if (const std::optional<ESM::FormId> cell = mResolver.toFormId(*metadata->mParent))
                static_cast<void>(mWorld.mWorldModel.getCell(ESM::RefId(*cell)));
        return mWorld.mWorldModel.getPtr(*id);
    }

    std::shared_ptr<const ObScript::Program> OblivionScriptManager::scriptFor(const Ptr& ptr) const
    {
        if (ptr.isEmpty())
            return {};
        const ESM::FormKey script = mResolver.toFormKey(scriptId(ptr));
        const auto found = mScripts.find(script);
        return found == mScripts.end() ? std::shared_ptr<const ObScript::Program>{} : found->second;
    }

    std::shared_ptr<const ObScript::Program> OblivionScriptManager::scriptFor(const ESM::FormKey& key)
    {
        if (const auto direct = mBaseScripts.find(key); direct != mBaseScripts.end())
            return direct->second;
        if (const Ptr ptr = ptrFor(key); !ptr.isEmpty())
            if (const auto program = scriptFor(ptr))
                return program;
        if (const auto reference = mReferenceBases.find(key); reference != mReferenceBases.end())
            if (const auto base = mBaseScripts.find(reference->second); base != mBaseScripts.end())
                return base->second;
        if (const ESM4::RuntimeReferenceState* state = referenceState(key))
            if (const auto base = mBaseScripts.find(state->mBase); base != mBaseScripts.end())
                return base->second;
        return {};
    }

    OblivionScriptManager::Instance& OblivionScriptManager::instanceFor(
        const ObScript::Program& program, const ESM::FormKey& context)
    {
        Instance& instance = mInstances[{ program.mUnit.serialize(), context }];
        if (instance.mLocals.empty() && !program.mLocals.empty())
            instance.mLocals = ObScript::VirtualMachine::makeLocals(program);
        if (instance.mLocals.size() != program.mLocals.size())
            throw std::runtime_error("M7 persisted local layout does not match Program " + program.mUnit.serialize());
        return instance;
    }

    bool OblivionScriptManager::execute(const std::shared_ptr<const ObScript::Program>& program,
        const ESM::FormKey& instanceKey, const ESM::FormKey& self, std::string_view event,
        const ESM::FormKey& actionReference)
    {
        if (!program)
            return false;
        const std::string requested = lower(event);
        if (mDepth >= 64)
        {
            ObScript::RuntimeDiagnostic diagnostic;
            diagnostic.mCode = "OBSV103";
            diagnostic.mMessage = "ObScript reentrancy limit exceeded";
            diagnostic.mUnit = program->mUnit;
            diagnostic.mEvent = requested;
            diagnostic.mSequence = ++mSequence;
            recordDiagnostic(diagnostic);
            return false;
        }
        const bool hasEntry = std::any_of(program->mEntryPoints.begin(), program->mEntryPoints.end(),
            [&](const ObScript::EntryPoint& entry) {
                return lower(entry.mEvent) == requested || (entry.mEvent == "__stray" && requested == "__result");
            });
        if (!hasEntry)
            return false;
        Instance& instance = instanceFor(*program, instanceKey);
        bool handled = false;
        ++mDepth;
        for (const ObScript::EntryPoint& entry : program->mEntryPoints)
        {
            if (lower(entry.mEvent) != requested && !(entry.mEvent == "__stray" && requested == "__result"))
                continue;
            bool matches = true;
            if (!entry.mRuntimeArguments.empty())
            {
                const std::optional<ESM::FormKey> expected = mStore.findEsm4FormKey(entry.mRuntimeArguments.front());
                matches = expected && *expected == actionReference;
            }
            if (!matches)
                continue;
            handled = true;
            ObScript::RuntimeContext context;
            context.mUnit = program->mUnit;
            context.mInstance = instanceKey;
            context.mSelf = self;
            context.mActionReference = actionReference;
            context.mEvent = requested;
            context.mSecondsPassed = mWorld.mScriptsEnabled ? mWorld.mLastOblivionScriptSeconds : 0;
            context.mSequence = ++mSequence;
            context.mDepth = mDepth;
            if (requested != "gamemode")
                trace("dispatch sequence=" + std::to_string(context.mSequence) + " depth="
                    + std::to_string(context.mDepth) + " event=" + requested
                    + " unit=" + program->mUnit.serialize() + " instance=" + instanceKey.serialize());
            const ObScript::ExecutionReport report = mVm.execute(*program, entry, instance.mLocals, *this, context);
            for (const ObScript::RuntimeDiagnostic& diagnostic : report.mDiagnostics)
                recordDiagnostic(diagnostic);
        }
        --mDepth;
        return handled;
    }

    bool OblivionScriptManager::dispatchObjectEvent(
        const Ptr& self, std::string_view event, const Ptr& actionReference)
    {
        const auto program = scriptFor(self);
        if (!program)
            return false;
        const ESM::FormKey selfKey = keyFor(self);
        if (Misc::StringUtils::ciEqual(event, "onload"))
        {
            const bool hasOnLoad = std::any_of(program->mEntryPoints.begin(), program->mEntryPoints.end(),
                [](const ObScript::EntryPoint& entry) {
                    return Misc::StringUtils::ciEqual(entry.mEvent, "onload");
                });
            if (!hasOnLoad)
                return false;
            Instance& instance = instanceFor(*program, selfKey);
            if (instance.mOnLoadFired)
                return false;
            instance.mOnLoadFired = true;
        }
        return execute(program, selfKey, selfKey, event, keyFor(actionReference));
    }

    bool OblivionScriptManager::dispatchObjectEvent(
        const ESM::FormKey& self, std::string_view event, const ESM::FormKey& actionReference)
    {
        const auto program = scriptFor(self);
        if (!program)
            return false;
        if (Misc::StringUtils::ciEqual(event, "onload"))
        {
            const bool hasOnLoad = std::any_of(program->mEntryPoints.begin(), program->mEntryPoints.end(),
                [](const ObScript::EntryPoint& entry) {
                    return Misc::StringUtils::ciEqual(entry.mEvent, "onload");
                });
            if (!hasOnLoad)
                return false;
            Instance& instance = instanceFor(*program, self);
            if (instance.mOnLoadFired)
                return false;
            instance.mOnLoadFired = true;
        }
        return execute(program, self, self, event, actionReference);
    }

    bool OblivionScriptManager::dispatchBaseEvent(
        const ESM::FormKey& base, std::string_view event, const ESM::FormKey& actionReference)
    {
        const auto found = mBaseScripts.find(base);
        return found != mBaseScripts.end()
            && execute(found->second, base, base, event, actionReference);
    }

    bool OblivionScriptManager::dispatchActivation(const Ptr& self, const Ptr& actionReference)
    {
        const ESM::FormKey key = keyFor(self);
        if (mSuppressedActivations.contains(key))
            return false;
        return dispatchObjectEvent(self, "onactivate", actionReference);
    }

    void OblivionScriptManager::update(double secondsPassed)
    {
        if (!mWorld.mOblivionRuntimeState)
            mWorld.mOblivionRuntimeState
                = std::make_unique<ESM4::RuntimeState>(mWorld.captureOblivionRuntimeState());
        mWorld.mLastOblivionScriptSeconds = secondsPassed;
        mElapsed += secondsPassed;
        runScheduledEvents();

        for (const auto& [questKey, scriptKey] : mQuestScripts)
        {
            const auto quest = mQuests.find(questKey);
            const auto program = mScripts.find(scriptKey);
            if (quest != mQuests.end() && quest->second.mRunning && program != mScripts.end())
                execute(program->second, questKey, questKey, "gamemode");
        }

        std::vector<Ptr> objects;
        for (CellStore* cell : mWorld.mWorldScene->getActiveCells())
            cell->forEach([&](const Ptr& ptr) {
                if (!ptr.isEmpty() && ptr.getRefData().isEnabled() && scriptFor(ptr))
                    objects.push_back(ptr);
                return true;
            });
        std::sort(objects.begin(), objects.end(), [&](const Ptr& left, const Ptr& right) {
            return keyFor(left) < keyFor(right);
        });
        for (const Ptr& ptr : objects)
        {
            dispatchObjectEvent(ptr, "onload");
            dispatchObjectEvent(ptr, "gamemode");
        }
        writeRuntimeReport();
    }

    ESM4::RuntimeQuestState& OblivionScriptManager::questState(const ESM::FormKey& key)
    {
        ESM4::RuntimeQuestState& result = mQuests[key];
        result.mQuest = key;
        return result;
    }

    void OblivionScriptManager::setStage(const ESM::FormKey& quest, std::int32_t stage)
    {
        ESM4::RuntimeQuestState& state = questState(quest);
        state.mStage = stage;
        state.mRunning = true;
        const auto insert = std::lower_bound(state.mCompletedStages.begin(), state.mCompletedStages.end(), stage);
        if (insert == state.mCompletedStages.end() || *insert != stage)
            state.mCompletedStages.insert(insert, stage);
        trace("setstage quest=" + quest.serialize() + " stage=" + std::to_string(stage));
        if (const auto programs = mQuestResults.find(quest); programs != mQuestResults.end())
            for (const auto& program : programs->second)
                if (program->mUnit.mStage && *program->mUnit.mStage == stage)
                    execute(program, quest, quest, "__result");
    }

    void OblivionScriptManager::dispatchDialogueResult(
        const ESM::FormKey& info, std::uint32_t ordinal, const Ptr& actor)
    {
        const auto found = mDialogueResults.find(info);
        if (found == mDialogueResults.end())
            return;
        for (const auto& program : found->second)
            if (program->mUnit.mOrdinal == ordinal)
                execute(program, info, keyFor(actor), "__result", keyFor(actor));
    }

    void OblivionScriptManager::dispatchEffect(
        const ESM::FormKey& script, std::string_view event, const Ptr& target, const Ptr& caster)
    {
        const auto found = mScripts.find(script);
        if (found != mScripts.end())
        {
            ESM::FormKey targetKey = keyFor(target);
            if (targetKey.isNull())
                targetKey = script;
            execute(found->second, targetKey, targetKey, event, keyFor(caster));
        }
    }

    std::optional<ESM::FormKey> OblivionScriptManager::keyFromValue(const ObScript::Value& value) const
    {
        if (const auto* reference = std::get_if<ObScript::ReferenceValue>(&value))
        {
            if (!reference->mKey.isNull())
                return reference->mKey;
            if (const auto key = mStore.findEsm4FormKey(reference->mName))
                return key;
        }
        if (const auto* name = std::get_if<std::string>(&value))
            return mStore.findEsm4FormKey(*name);
        return std::nullopt;
    }

    ESM::FormKey OblivionScriptManager::keyArgument(const std::optional<ObScript::Value>& target,
        const std::vector<ObScript::Value>& arguments, std::size_t argument,
        const ObScript::RuntimeContext& context) const
    {
        if (target)
            if (const auto key = keyFromValue(*target))
                return *key;
        if (argument < arguments.size())
            if (const auto key = keyFromValue(arguments[argument]))
                return *key;
        return context.mSelf;
    }

    ESM4::RuntimeReferenceState* OblivionScriptManager::referenceState(const ESM::FormKey& key)
    {
        if (!mWorld.mOblivionRuntimeState)
            mWorld.mOblivionRuntimeState
                = std::make_unique<ESM4::RuntimeState>(mWorld.captureOblivionRuntimeState());
        auto& references = mWorld.mOblivionRuntimeState->mReferences;
        auto found = std::find_if(references.begin(), references.end(),
            [&](const ESM4::RuntimeReferenceState& value) { return value.mKey == key; });
        if (found != references.end())
            return &*found;

        const ESM::FormRecordMetadata* metadata = mStore.getFormKeyIndex().resolve(key);
        const auto base = mReferenceBases.find(key);
        if (metadata == nullptr || !metadata->mParent || base == mReferenceBases.end())
            return nullptr;

        ESM4::RuntimeReferenceState state;
        state.mKey = key;
        state.mBase = base->second;
        state.mCell = *metadata->mParent;
        if (const Ptr ptr = ptrFor(key); !ptr.isEmpty())
        {
            state.mEnabled = ptr.getRefData().isEnabled();
            state.mDeleted = ptr.mRef->isDeleted();
            state.mPosition = ptr.getRefData().getPosition();
            try
            {
                state.mLockLevel = ptr.getCellRef().getLockLevel();
            }
            catch (const std::logic_error&)
            {
            }
        }
        references.push_back(std::move(state));
        std::sort(references.begin(), references.end(), [](const auto& left, const auto& right) {
            return left.mKey < right.mKey;
        });
        found = std::lower_bound(references.begin(), references.end(), key,
            [](const ESM4::RuntimeReferenceState& value, const ESM::FormKey& wanted) {
                return value.mKey < wanted;
            });
        return found != references.end() && found->mKey == key ? &*found : nullptr;
    }

    const ESM4::RuntimeReferenceState* OblivionScriptManager::referenceState(const ESM::FormKey& key) const
    {
        if (!mWorld.mOblivionRuntimeState)
            return nullptr;
        const auto& references = mWorld.mOblivionRuntimeState->mReferences;
        const auto found = std::find_if(references.begin(), references.end(),
            [&](const ESM4::RuntimeReferenceState& value) { return value.mKey == key; });
        return found == references.end() ? nullptr : &*found;
    }

    ObScript::Value OblivionScriptManager::resolveName(
        std::string_view name, const ObScript::RuntimeContext& context)
    {
        if (Misc::StringUtils::ciEqual(name, "self"))
            return ObScript::ReferenceValue{ context.mSelf, "self" };
        if (Misc::StringUtils::ciEqual(name, "player"))
            return ObScript::ReferenceValue{ ESM::FormKey::dynamic("player", 1), "player" };
        if (Misc::StringUtils::ciEqual(name, "actionref") || Misc::StringUtils::ciEqual(name, "actionreference"))
            return ObScript::ReferenceValue{ context.mActionReference, "actionref" };
        if (const auto key = mStore.findEsm4FormKey(name))
        {
            if (mWorld.mOblivionRuntimeState)
            {
                const auto global = mWorld.mOblivionRuntimeState->mGlobals.find(*key);
                if (global != mWorld.mOblivionRuntimeState->mGlobals.end())
                    return std::visit([](const auto& value) -> ObScript::Value { return value; }, global->second);
            }
            return ObScript::ReferenceValue{ *key, std::string(name) };
        }
        return ObScript::ReferenceValue{ {}, std::string(name) };
    }

    ObScript::Value OblivionScriptManager::loadScriptLocal(const ESM::FormKey& target, std::string_view name)
    {
        std::shared_ptr<const ObScript::Program> program;
        ESM::FormKey instance = target;
        if (const auto quest = mQuestScripts.find(target); quest != mQuestScripts.end())
        {
            const auto found = mScripts.find(quest->second);
            if (found != mScripts.end())
                program = found->second;
        }
        else
        {
            program = scriptFor(target);
        }
        if (!program)
            return std::monostate{};
        Instance& state = instanceFor(*program, instance);
        for (std::size_t i = 0; i < program->mLocals.size(); ++i)
            if (Misc::StringUtils::ciEqual(program->mLocals[i].mName, name))
                return state.mLocals[i];
        return std::monostate{};
    }

    bool OblivionScriptManager::storeScriptLocal(
        const ESM::FormKey& target, std::string_view name, const ObScript::Value& value)
    {
        std::shared_ptr<const ObScript::Program> program;
        if (const auto quest = mQuestScripts.find(target); quest != mQuestScripts.end())
        {
            const auto found = mScripts.find(quest->second);
            if (found != mScripts.end())
                program = found->second;
        }
        else
        {
            program = scriptFor(target);
        }
        if (!program)
            return false;
        Instance& state = instanceFor(*program, target);
        for (std::size_t i = 0; i < program->mLocals.size(); ++i)
        {
            if (!Misc::StringUtils::ciEqual(program->mLocals[i].mName, name))
                continue;
            ObScript::ValueType type = ObScript::ValueType::Long;
            switch (program->mLocals[i].mType)
            {
                case ObScript::VariableType::Short: type = ObScript::ValueType::Short; break;
                case ObScript::VariableType::Integer: type = ObScript::ValueType::Integer; break;
                case ObScript::VariableType::Long: type = ObScript::ValueType::Long; break;
                case ObScript::VariableType::Float: type = ObScript::ValueType::Float; break;
                case ObScript::VariableType::Reference: type = ObScript::ValueType::Reference; break;
            }
            state.mLocals[i] = ObScript::convert(value, type);
            return true;
        }
        return false;
    }

    ObScript::Value OblivionScriptManager::loadMember(
        const ObScript::Value& target, std::string_view name, const ObScript::RuntimeContext&)
    {
        const auto key = keyFromValue(target);
        return key ? loadScriptLocal(*key, name) : ObScript::Value(std::monostate{});
    }

    void OblivionScriptManager::storeExternal(
        std::string_view name, const ObScript::Value& value, const ObScript::RuntimeContext&)
    {
        const auto key = mStore.findEsm4FormKey(name);
        if (!key)
            throw ObScript::RuntimeError("OBSV101", "Unknown external variable " + std::string(name));
        if (!mWorld.mOblivionRuntimeState)
            mWorld.mOblivionRuntimeState
                = std::make_unique<ESM4::RuntimeState>(mWorld.captureOblivionRuntimeState());
        ESM4::RuntimeValue saved;
        if (const auto* number = std::get_if<double>(&value))
            saved = *number;
        else if (const auto* text = std::get_if<std::string>(&value))
            saved = *text;
        else
            saved = ObScript::asInteger(value);
        mWorld.mOblivionRuntimeState->mGlobals[*key] = saved;
        ESM::Variant& global = mWorld.mGlobalVariables[GlobalVariableName(name)];
        if (global.getType() == ESM::VT_Float)
            global.setFloat(static_cast<float>(ObScript::asNumber(value)));
        else if (global.getType() == ESM::VT_String)
            global.setString(ObScript::valueString(value));
        else
            global.setInteger(static_cast<int>(ObScript::asInteger(value)));
    }

    void OblivionScriptManager::storeMember(const ObScript::Value& target, std::string_view name,
        const ObScript::Value& value, const ObScript::RuntimeContext&)
    {
        const auto key = keyFromValue(target);
        if (!key || !storeScriptLocal(*key, name, value))
            throw ObScript::RuntimeError("OBSV102", "Unknown member variable " + std::string(name));
    }

    ObScript::Value OblivionScriptManager::call(std::string_view command,
        const std::optional<ObScript::Value>& target, const std::vector<ObScript::Value>& arguments,
        const ObScript::RuntimeContext& context, const ObScript::SourceLocation&)
    {
        const std::string name = lower(command);
        ++mCommandCounts[name];
        const auto argument = [&](std::size_t index) -> ObScript::Value {
            return index < arguments.size() ? arguments[index] : ObScript::Value(std::monostate{});
        };
        const auto subjectKey = [&](std::size_t index = 0) { return keyArgument(target, arguments, index, context); };
        const auto objectKey = [&]() {
            if (target)
                if (const auto key = keyFromValue(*target))
                    return *key;
            return context.mSelf;
        };
        const auto objectPtr = [&]() { return ptrFor(objectKey()); };

        if (name == "getsecondspassed")
            return context.mSecondsPassed;
        if (name == "getself" || name == "getcontainer")
            return ObScript::ReferenceValue{ context.mSelf, "self" };
        if (name == "getactionref" || name == "getactionreference")
            return ObScript::ReferenceValue{ context.mActionReference, "actionref" };
        if (name == "isactionref")
        {
            const auto expected = keyFromValue(argument(0));
            return std::int64_t(expected && *expected == context.mActionReference);
        }
        if (name == "getstage")
            return std::int64_t(questState(subjectKey()).mStage);
        if (name == "getstagedone")
        {
            const auto& completed = questState(subjectKey()).mCompletedStages;
            const std::int32_t stage = boundedCount(argument(target ? 0 : 1));
            return std::int64_t(std::binary_search(completed.begin(), completed.end(), stage));
        }
        if (name == "setstage")
        {
            const std::size_t stageArg = target ? 0 : 1;
            setStage(subjectKey(), boundedCount(argument(stageArg)));
            return std::int64_t(0);
        }
        if (name == "startquest" || name == "stopquest")
        {
            questState(subjectKey()).mRunning = name == "startquest";
            return std::int64_t(0);
        }
        if (name == "getquestrunning")
            return std::int64_t(questState(subjectKey()).mRunning);

        if (name == "enable" || name == "disable")
        {
            const ESM::FormKey key = objectKey();
            const Ptr ptr = ptrFor(key);
            const bool enabled = name == "enable";
            bool changed = false;
            if (!ptr.isEmpty())
            {
                changed = ptr.getRefData().isEnabled() != enabled;
                if (changed)
                {
                    if (enabled)
                        mWorld.enable(ptr);
                    else
                        mWorld.disable(ptr);
                }
            }
            if (ESM4::RuntimeReferenceState* state = referenceState(key))
            {
                changed = changed || state->mEnabled != enabled;
                state->mEnabled = enabled;
            }
            if (changed)
                trace(name + " ref=" + key.serialize());
            return std::int64_t(0);
        }
        if (name == "getdisabled")
        {
            const Ptr ptr = objectPtr();
            if (!ptr.isEmpty())
                return std::int64_t(!ptr.getRefData().isEnabled());
            const auto* state = referenceState(objectKey());
            return std::int64_t(state != nullptr && !state->mEnabled);
        }
        if (name == "activate")
        {
            const ESM::FormKey key = objectKey();
            const Ptr ptr = ptrFor(key);
            const Ptr actor = ptrFor(context.mActionReference.isNull() ? context.mSelf : context.mActionReference);
            if (!ptr.isEmpty())
            {
                mSuppressedActivations.insert(key);
                std::unique_ptr<Action> action = ptr.getClass().activate(ptr, actor);
                if (action)
                    action->execute(actor, true);
                mSuppressedActivations.erase(key);
            }
            trace("activate-default ref=" + key.serialize());
            return std::int64_t(0);
        }
        if (name == "playgroup")
        {
            const Ptr ptr = objectPtr();
            const std::size_t groupArg = 0;
            const std::string group = ObScript::valueString(argument(groupArg));
            const int mode = static_cast<int>(ObScript::asInteger(argument(groupArg + 1)));
            bool played = false;
            if (!ptr.isEmpty())
                played = MWBase::Environment::get().getMechanicsManager()->playAnimationGroup(
                    ptr, group, mode, 1, true);
            if (ESM4::RuntimeReferenceState* state = referenceState(objectKey()))
            {
                state->mCustomState["obscript.animation_group"] = group;
                state->mCustomState["obscript.animation_mode"] = std::int64_t(mode);
                state->mCustomState["obscript.animation_playing"] = played;
                if (played)
                {
                    state->mCustomState["obscript.animation_progress"] = 0.0;
                    state->mCustomState["obscript.animation_loop_count"] = std::int64_t(0);
                    state->mCustomState["obscript.animation_absolute"] = false;
                }
            }
            trace("playgroup ref=" + objectKey().serialize() + " group=" + group
                + " played=" + (played ? "true" : "false"));
            return std::int64_t(played);
        }
        if (name == "isanimplaying")
        {
            const Ptr ptr = objectPtr();
            const std::size_t groupArg = 0;
            return std::int64_t(!ptr.isEmpty()
                && MWBase::Environment::get().getMechanicsManager()->checkAnimationPlaying(
                    ptr, ObScript::valueString(argument(groupArg))));
        }
        if (name == "message" || name == "messagebox")
        {
            const std::string text = ObScript::valueString(argument(0));
            MWBase::Environment::get().getWindowManager()->messageBox(text);
            trace(name + " text=" + text);
            return std::int64_t(0);
        }
        if (name == "getrandompercent")
            return std::int64_t(Misc::Rng::rollDice(100, mWorld.mPrng));

        if (name == "additem" || name == "removeitem" || name == "getitemcount")
        {
            const std::size_t itemArg = 0;
            const ESM::FormKey owner = objectKey();
            const auto item = keyFromValue(argument(itemArg));
            if (!item)
                return std::int64_t(0);
            std::vector<ESM4::RuntimeInventoryItem>* inventory = nullptr;
            if (owner == ESM::FormKey::dynamic("player", 1))
            {
                if (!mWorld.mOblivionRuntimeState)
                    mWorld.mOblivionRuntimeState
                        = std::make_unique<ESM4::RuntimeState>(mWorld.captureOblivionRuntimeState());
                inventory = &mWorld.mOblivionRuntimeState->mPlayer.mInventory;
            }
            else if (ESM4::RuntimeReferenceState* state = referenceState(owner))
                inventory = &state->mInventory;
            if (!inventory)
                return std::int64_t(0);
            if (name == "getitemcount")
                return std::int64_t(inventoryCount(*inventory, *item));
            const std::int32_t count = std::max<std::int32_t>(1, boundedCount(argument(itemArg + 1)));
            changeInventory(*inventory, *item, name == "additem" ? count : -count);
            trace(name + " owner=" + owner.serialize() + " item=" + item->serialize()
                + " count=" + std::to_string(count));
            if (name == "additem")
                dispatchBaseEvent(*item, "onadd", owner);
            return std::int64_t(0);
        }

        if (name == "moveto" || name == "movetomarker")
        {
            const Ptr ptr = objectPtr();
            const std::size_t markerArg = 0;
            const auto markerKey = keyFromValue(argument(markerArg));
            const Ptr marker = markerKey ? ptrFor(*markerKey) : Ptr{};
            if (!ptr.isEmpty() && !marker.isEmpty())
            {
                const ESM::Position& position = marker.getRefData().getPosition();
                mWorld.moveObject(ptr, marker.getCell(), position.asVec3(), true, true);
                mWorld.rotateObject(ptr, osg::Vec3f(position.rot[0], position.rot[1], position.rot[2]));
                trace("moveto ref=" + objectKey().serialize() + " marker=" + markerKey->serialize());
            }
            return std::int64_t(0);
        }
        if (name == "getdistance")
        {
            const Ptr left = objectPtr();
            const std::size_t rightArg = 0;
            const auto rightKey = keyFromValue(argument(rightArg));
            const Ptr right = rightKey ? ptrFor(*rightKey) : Ptr{};
            if (left.isEmpty() || right.isEmpty())
                return double(std::numeric_limits<float>::max());
            return double((left.getRefData().getPosition().asVec3()
                - right.getRefData().getPosition().asVec3()).length());
        }
        if (name == "getincell")
        {
            const Ptr left = objectPtr();
            const auto cellKey = keyFromValue(argument(0));
            if (left.isEmpty() || !left.isInCell() || !cellKey)
                return std::int64_t(0);
            const ESM::FormId* cellId = left.getCell()->getCell()->getId().getIf<ESM::FormId>();
            return std::int64_t(cellId != nullptr && mResolver.toFormKey(*cellId) == *cellKey);
        }
        if (name == "getinsamecell")
        {
            const Ptr left = objectPtr();
            const auto rightKey = keyFromValue(argument(0));
            const Ptr right = rightKey ? ptrFor(*rightKey) : Ptr{};
            return std::int64_t(!left.isEmpty() && !right.isEmpty() && left.getCell() == right.getCell());
        }

        if (name == "lock" || name == "unlock")
        {
            const Ptr ptr = objectPtr();
            if (!ptr.isEmpty())
            {
                if (name == "unlock") ptr.getCellRef().unlock();
                else ptr.getCellRef().lock(arguments.empty() ? 0 : boundedCount(arguments.back()));
            }
            return std::int64_t(0);
        }
        if (name == "getlocked" || name == "getlocklevel")
        {
            const Ptr ptr = objectPtr();
            if (ptr.isEmpty())
                return std::int64_t(0);
            return name == "getlocked" ? ObScript::Value(std::int64_t(ptr.getCellRef().isLocked()))
                                        : ObScript::Value(std::int64_t(ptr.getCellRef().getLockLevel()));
        }
        if (name == "getpos" || name == "getangle" || name == "getscale")
        {
            const Ptr ptr = objectPtr();
            if (ptr.isEmpty()) return double(0);
            if (name == "getscale") return double(ptr.getCellRef().getScale());
            const std::string axis = lower(ObScript::valueString(argument(target ? 0 : 1)));
            const int index = axis == "y" ? 1 : axis == "z" ? 2 : 0;
            return double(name == "getpos" ? ptr.getRefData().getPosition().pos[index]
                                            : ptr.getRefData().getPosition().rot[index] * 180.0 / osg::PI);
        }
        if (name == "setpos" || name == "setangle" || name == "setscale")
        {
            const Ptr ptr = objectPtr();
            if (ptr.isEmpty()) return std::int64_t(0);
            const std::size_t firstArg = target ? 0 : 1;
            if (name == "setscale")
                mWorld.scaleObject(ptr, static_cast<float>(ObScript::asNumber(argument(firstArg))), true);
            else
            {
                const std::string axis = lower(ObScript::valueString(argument(firstArg)));
                const int index = axis == "y" ? 1 : axis == "z" ? 2 : 0;
                if (name == "setpos")
                {
                    osg::Vec3f position = ptr.getRefData().getPosition().asVec3();
                    position[index] = static_cast<float>(ObScript::asNumber(argument(firstArg + 1)));
                    mWorld.moveObject(ptr, position);
                }
                else
                {
                    const float* currentRotation = ptr.getRefData().getPosition().rot;
                    osg::Vec3f rotation(currentRotation[0], currentRotation[1], currentRotation[2]);
                    rotation[index] = static_cast<float>(ObScript::asNumber(argument(firstArg + 1)) * osg::PI / 180.0);
                    mWorld.rotateObject(ptr, rotation);
                }
            }
            return std::int64_t(0);
        }

        if (name == "getisid" || name == "getisreference")
        {
            const auto expected = keyFromValue(argument(0));
            return std::int64_t(expected && objectKey() == *expected);
        }
        if (name == "getparentref")
        {
            const ESM::FormRecordMetadata* metadata = mStore.getFormKeyIndex().resolve(objectKey());
            return ObScript::ReferenceValue{ metadata && metadata->mParent ? *metadata->mParent : ESM::FormKey{}, {} };
        }
        if (name == "getdead")
        {
            const ESM4::RuntimeReferenceState* state = referenceState(objectKey());
            if (state)
                if (const auto dead = state->mCustomState.find("obscript.dead"); dead != state->mCustomState.end())
                    return std::int64_t(std::get<bool>(dead->second));
            return std::int64_t(0);
        }
        if (name == "getdeadcount")
        {
            const auto base = keyFromValue(argument(0));
            if (!base || !mWorld.mOblivionRuntimeState)
                return std::int64_t(0);
            std::int64_t count = 0;
            for (const ESM4::RuntimeReferenceState& state : mWorld.mOblivionRuntimeState->mReferences)
            {
                if (state.mBase != *base)
                    continue;
                const auto dead = state.mCustomState.find("obscript.dead");
                if (dead != state.mCustomState.end())
                    if (const bool* value = std::get_if<bool>(&dead->second); value != nullptr && *value)
                        ++count;
            }
            return count;
        }
        if (name == "kill" || name == "resurrect")
        {
            if (ESM4::RuntimeReferenceState* state = referenceState(objectKey()))
                state->mCustomState["obscript.dead"] = name == "kill";
            if (name == "kill")
                dispatchObjectEvent(objectKey(), "ondeath", context.mSelf);
            return std::int64_t(0);
        }

        if (name == "isincombat" || name == "isspelltarget" || name == "ispcamurderer")
            return std::int64_t(0);
        if (name == "isininterior")
        {
            const Ptr ptr = objectPtr();
            return std::int64_t(!ptr.isEmpty() && ptr.isInCell() && !ptr.getCell()->isExterior());
        }
        if (name == "isridinghorse")
            return std::int64_t(0);
        if (name == "isplayerinjail")
            return std::int64_t(mWorld.mPlayerInJail);
        if (name == "getpcinfamy" || name == "getpcfactionmurder" || name == "getpcfactionsteal")
        {
            if (!mWorld.mOblivionRuntimeState)
                return std::int64_t(0);
            const std::string valueName = name == "getpcinfamy" ? "infamy"
                : name == "getpcfactionmurder" ? "faction_murder" : "faction_steal";
            const auto value = mWorld.mOblivionRuntimeState->mPlayer.mActorValues.find(valueName);
            return std::int64_t(value == mWorld.mOblivionRuntimeState->mPlayer.mActorValues.end()
                    ? 0 : value->second);
        }
        if (name == "modpcinfamy")
        {
            if (!mWorld.mOblivionRuntimeState)
                mWorld.mOblivionRuntimeState
                    = std::make_unique<ESM4::RuntimeState>(mWorld.captureOblivionRuntimeState());
            mWorld.mOblivionRuntimeState->mPlayer.mActorValues["infamy"] += ObScript::asNumber(argument(0));
            return std::int64_t(0);
        }
        if (name == "getlevel")
        {
            if (objectKey() == ESM::FormKey::dynamic("player", 1) && mWorld.mOblivionRuntimeState)
            {
                const auto level = mWorld.mOblivionRuntimeState->mPlayer.mActorValues.find("level");
                if (level != mWorld.mOblivionRuntimeState->mPlayer.mActorValues.end())
                    return std::int64_t(level->second);
            }
            return std::int64_t(1);
        }
        if (name == "getvampire")
        {
            if (objectKey() == ESM::FormKey::dynamic("player", 1))
            {
                if (!mWorld.mOblivionRuntimeState)
                    return std::int64_t(0);
                const auto value = mWorld.mOblivionRuntimeState->mPlayer.mActorValues.find("vampire");
                return std::int64_t(value == mWorld.mOblivionRuntimeState->mPlayer.mActorValues.end()
                        ? 0 : value->second);
            }
            if (const ESM4::RuntimeReferenceState* state = referenceState(objectKey()))
            {
                const auto value = state->mCustomState.find("obscript.vampire");
                if (value != state->mCustomState.end())
                {
                    if (const auto* number = std::get_if<std::int64_t>(&value->second))
                        return *number;
                    if (const auto* enabled = std::get_if<bool>(&value->second))
                        return std::int64_t(*enabled);
                }
            }
            return std::int64_t(0);
        }
        if (name == "getcurrenttime")
            return double(mWorld.mGlobalVariables[Globals::sGameHour].getFloat());
        if (name == "getfactionrank")
            return std::int64_t(-1);
        if (name == "getcurrentaiprocedure")
            return std::int64_t(0);
        if (name == "getdestroyed")
        {
            const ESM4::RuntimeReferenceState* state = referenceState(objectKey());
            if (state != nullptr)
            {
                const auto destroyed = state->mCustomState.find("obscript.destroyed");
                if (destroyed != state->mCustomState.end())
                    if (const bool* value = std::get_if<bool>(&destroyed->second))
                        return std::int64_t(*value);
            }
            return std::int64_t(0);
        }
        if (name == "getplayerinseworld")
        {
            const auto world = keyFromValue(argument(0));
            const Ptr player = mWorld.getPlayerPtr();
            if (!world || !player.isInCell())
                return std::int64_t(0);
            const ESM::RefId worldId = player.getCell()->getCell()->getWorldSpace();
            const ESM::FormId* formId = worldId.getIf<ESM::FormId>();
            return std::int64_t(formId != nullptr && mResolver.toFormKey(*formId) == *world);
        }

        if (name == "getav" || name == "getbaseav")
        {
            const std::string attribute = lower(ObScript::valueString(argument(0)));
            if (objectKey() == ESM::FormKey::dynamic("player", 1))
            {
                if (!mWorld.mOblivionRuntimeState)
                    return double(0);
                const auto value = mWorld.mOblivionRuntimeState->mPlayer.mActorValues.find(attribute);
                return value == mWorld.mOblivionRuntimeState->mPlayer.mActorValues.end() ? double(0)
                                                                                         : value->second;
            }
            if (const ESM4::RuntimeReferenceState* state = referenceState(objectKey()))
            {
                const auto value = state->mCustomState.find("obscript.av." + attribute);
                if (value != state->mCustomState.end())
                    if (const double* number = std::get_if<double>(&value->second))
                        return *number;
            }
            return double(0);
        }
        if (name == "setav" || name == "forceav" || name == "modav")
        {
            const std::string attribute = lower(ObScript::valueString(argument(0)));
            const double value = ObScript::asNumber(argument(1));
            if (objectKey() == ESM::FormKey::dynamic("player", 1))
            {
                if (!mWorld.mOblivionRuntimeState)
                    mWorld.mOblivionRuntimeState
                        = std::make_unique<ESM4::RuntimeState>(mWorld.captureOblivionRuntimeState());
                double& current = mWorld.mOblivionRuntimeState->mPlayer.mActorValues[attribute];
                current = name == "modav" ? current + value : value;
            }
            else if (ESM4::RuntimeReferenceState* state = referenceState(objectKey()))
            {
                ESM4::RuntimeValue& saved = state->mCustomState["obscript.av." + attribute];
                double current = 0;
                if (const double* number = std::get_if<double>(&saved))
                    current = *number;
                saved = name == "modav" ? current + value : value;
            }
            return std::int64_t(0);
        }
        if (name == "setdestroyed" || name == "setghost")
        {
            if (ESM4::RuntimeReferenceState* state = referenceState(objectKey()))
                state->mCustomState[name == "setdestroyed" ? "obscript.destroyed" : "obscript.ghost"]
                    = ObScript::asInteger(argument(0)) != 0;
            return std::int64_t(0);
        }
        if (name == "disablelinkedpathpoints" || name == "enablelinkedpathpoints")
        {
            if (ESM4::RuntimeReferenceState* state = referenceState(objectKey()))
                state->mCustomState["obscript.linked_pathpoints_enabled"] = name == "enablelinkedpathpoints";
            return std::int64_t(0);
        }

        if (name == "forceweather" || name == "fw")
        {
            const auto weather = keyFromValue(argument(0));
            const auto id = weather ? mResolver.toFormId(*weather) : std::nullopt;
            const bool persistent = arguments.size() >= 2 && ObScript::asInteger(argument(1)) != 0;
            const bool changed = id && mWorld.mWeatherManager->forceWeatherOverride(ESM::RefId(*id), persistent);
            trace("forceweather weather=" + (weather ? weather->serialize() : std::string("null"))
                + " override=" + (persistent ? "true" : "false")
                + " applied=" + (changed ? "true" : "false"));
            return std::int64_t(changed);
        }
        if (name == "releaseweatheroverride")
        {
            mWorld.mWeatherManager->releaseWeatherOverride();
            trace("releaseweatheroverride");
            return std::int64_t(0);
        }

        if (name == "playsound" || name == "playsound3d")
        {
            const auto sound = keyFromValue(argument(0));
            const auto id = sound ? mResolver.toFormId(*sound) : std::nullopt;
            bool loop = false;
            if (id)
            {
                if (const ESM4::Sound* record = mStore.get<ESM4::Sound>().search(ESM::RefId(*id)))
                    loop = (record->mData.flags & ESM4::Sound::Flag_Loop) != 0;
                else if (const ESM4::SoundReference* soundReference
                    = mStore.get<ESM4::SoundReference>().search(ESM::RefId(*id)))
                    loop = (soundReference->mLoopInfo.flags & 0x1) != 0;
            }
            const MWSound::PlayMode mode = loop
                ? (name == "playsound3d" ? MWSound::PlayMode::LoopRemoveAtDistance
                                          : MWSound::PlayMode::LoopNoEnv)
                : MWSound::PlayMode::Normal;
            MWBase::Sound* played = nullptr;
            if (id)
            {
                MWBase::SoundManager* manager = MWBase::Environment::get().getSoundManager();
                const Ptr ptr = objectPtr();
                if (name == "playsound3d" && !ptr.isEmpty())
                    played = manager->playSound3D(ptr, ESM::RefId(*id), 1.f, 1.f, MWSound::Type::Sfx,
                        mode);
                else
                    played = manager->playSound(
                        ESM::RefId(*id), 1.f, 1.f, MWSound::Type::Sfx, mode);
            }
            trace(name + " sound=" + (sound ? sound->serialize() : std::string("null"))
                + " ref=" + objectKey().serialize() + " played=" + (played ? "true" : "false"));
            return std::int64_t(played != nullptr);
        }

        if (name == "say" || name == "sayto")
        {
            const Ptr speaker = objectPtr();
            const std::size_t topicArg = name == "sayto" ? 1 : 0;
            const std::size_t voiceArg = name == "sayto" ? 3 : 2;
            const auto topic = keyFromValue(argument(topicArg));
            const auto voiceType = keyFromValue(argument(voiceArg));
            const std::optional<std::string> voice
                = topic ? findNativeVoice(*topic, speaker, voiceType.value_or(ESM::FormKey{})) : std::nullopt;
            if (!voice)
            {
                trace(name + " topic=" + (topic ? topic->serialize() : std::string("null"))
                    + " voice=missing");
                return double(0);
            }
            MWBase::SoundManager* manager = MWBase::Environment::get().getSoundManager();
            const VFS::Path::Normalized path(*voice);
            const double duration = manager->getSoundFileDuration(path);
            if (!speaker.isEmpty()
                && (speaker.getClass().getType() == ESM::REC_NPC_4
                    || speaker.getClass().getType() == ESM::REC_CREA4))
                manager->say(speaker, path);
            else
                manager->say(path);
            trace(name + " topic=" + topic->serialize() + " voice=" + *voice
                + " duration=" + std::to_string(duration));
            return duration;
        }

        if (name == "playbink")
        {
            const std::string video = ObScript::valueString(argument(0));
            const bool allowSkipping = arguments.size() < 2 || ObScript::asInteger(argument(1)) != 0;
            MWBase::Environment::get().getWindowManager()->playVideo(video, allowSkipping);
            trace("playbink video=" + video + " skipping=" + (allowSkipping ? "true" : "false"));
            return std::int64_t(0);
        }

        // These commands acknowledge state owned by later AI/UI/audio/magic
        // milestones without pretending their subsystem effect occurred. They
        // retain deterministic control-flow compatibility for M7 scripts.
        static const std::set<std::string, std::less<>> deferred{
            "evaluatepackage", "evp", "addtopic", "showmap",
            "cast", "addspell", "removespell", "moddisposition", "setessential", "addscriptpackage",
            "setquestobject", "setownership", "setfactionrank", "modfactionrank", "setcrimegold",
            "pathpointenable", "pathpointdisable", "stopcombat", "startcombat", "equipitem", "unequipitem",
            "forceflee", "setrestrained", "setunconscious", "enableplayercontrols", "disableplayercontrols" };
        if (deferred.contains(name))
        {
            trace("deferred command=" + name + " unit=" + context.mUnit.serialize());
            return std::int64_t(0);
        }

        throw ObScript::RuntimeError("OBSV100", "Unsupported ObScript command " + name, name);
    }

    void OblivionScriptManager::capture(ESM4::RuntimeState& state) const
    {
        state.mVersion = ESM4::CurrentRuntimeStateVersion;
        state.mScriptEventSequence = mSequence;
        state.mScriptInstances.clear();
        for (const auto& [key, instance] : mInstances)
        {
            ESM4::RuntimeScriptInstance saved;
            saved.mUnit = key.mUnit;
            saved.mContext = key.mContext;
            saved.mOnLoadFired = instance.mOnLoadFired;
            for (const ObScript::Value& value : instance.mLocals)
                saved.mLocals.push_back(saveValue(value));
            state.mScriptInstances.push_back(std::move(saved));
        }
        state.mQuests.clear();
        for (const auto& [_, quest] : mQuests)
            state.mQuests.push_back(quest);
    }

    void OblivionScriptManager::restore(const ESM4::RuntimeState& state)
    {
        mSequence = state.mScriptEventSequence;
        mInstances.clear();
        for (const ESM4::RuntimeScriptInstance& saved : state.mScriptInstances)
        {
            Instance instance;
            instance.mOnLoadFired = saved.mOnLoadFired;
            const auto program = mProgramsByUnit.find(saved.mUnit);
            for (std::size_t i = 0; i < saved.mLocals.size(); ++i)
            {
                ObScript::Value value = loadValue(saved.mLocals[i]);
                // M7 development saves written before null references had a
                // distinct wire representation encoded them as empty strings.
                // Reapply the Program's declared local type both to migrate
                // those saves and to keep restored values type-stable.
                if (program != mProgramsByUnit.end() && i < program->second->mLocals.size()
                    && program->second->mLocals[i].mType == ObScript::VariableType::Reference)
                    value = ObScript::convert(std::move(value), ObScript::ValueType::Reference);
                instance.mLocals.push_back(std::move(value));
            }
            mInstances.emplace(InstanceKey{ saved.mUnit, saved.mContext }, std::move(instance));
        }
        for (const ESM4::RuntimeQuestState& quest : state.mQuests)
            mQuests[quest.mQuest] = quest;
        trace("restore sequence=" + std::to_string(mSequence) + " scripts=" + std::to_string(mInstances.size())
            + " quests=" + std::to_string(state.mQuests.size()));
    }

    void OblivionScriptManager::trace(std::string value)
    {
        Log(Debug::Info) << "M7 ObScript: " << value;
        mTrace.push_back(std::move(value));
        if (mTrace.size() > 10000)
            mTrace.erase(mTrace.begin(), mTrace.begin() + 1000);
    }

    void OblivionScriptManager::recordDiagnostic(const ObScript::RuntimeDiagnostic& diagnostic)
    {
        mDiagnostics.push_back(diagnostic);
        Log(Debug::Error) << "M7 ObScript diagnostic: code=" << diagnostic.mCode
                          << " sequence=" << diagnostic.mSequence << " event=" << diagnostic.mEvent
                          << " unit=" << diagnostic.mUnit.serialize() << " command=" << diagnostic.mCommand
                          << " message=" << diagnostic.mMessage;
    }

    void OblivionScriptManager::runScheduledEvents()
    {
        for (ScheduledEvent& event : mScheduledEvents)
        {
            if (event.mExecuted || event.mAt > mElapsed)
                continue;
            event.mExecuted = true;
            executeScheduledEvent(event);
            writeRuntimeReport();
        }
    }

    void OblivionScriptManager::executeScheduledEvent(const ScheduledEvent& event)
    {
        if (event.mWords.empty())
            return;
        const auto key = [&](std::size_t index) -> ESM::FormKey {
            if (index >= event.mWords.size())
                return {};
            if (Misc::StringUtils::ciEqual(event.mWords[index], "player"))
                return ESM::FormKey::dynamic("player", 1);
            if (event.mWords[index].starts_with("content:") || event.mWords[index].starts_with("dynamic:"))
                return ESM::FormKey::deserialize(event.mWords[index]);
            return mStore.findEsm4FormKey(event.mWords[index]).value_or(ESM::FormKey{});
        };
        const std::string kind = lower(event.mWords[0]);
        if (kind == "activate" && event.mWords.size() >= 2)
        {
            const ESM::FormKey targetKey = key(1);
            const ESM::FormKey actorKey
                = event.mWords.size() >= 3 ? key(2) : ESM::FormKey::dynamic("player", 1);
            const Ptr target = ptrFor(targetKey);
            const Ptr actor = ptrFor(actorKey);
            if (!dispatchObjectEvent(targetKey, "onactivate", actorKey) && !target.isEmpty())
            {
                mSuppressedActivations.insert(targetKey);
                std::unique_ptr<Action> action = target.getClass().activate(target, actor);
                if (action) action->execute(actor, true);
                mSuppressedActivations.erase(targetKey);
            }
        }
        else if (kind == "event" && event.mWords.size() >= 3)
            dispatchObjectEvent(key(1), event.mWords[2], event.mWords.size() >= 4 ? key(3) : ESM::FormKey{});
        else if (kind == "setstage" && event.mWords.size() >= 3)
            setStage(key(1), std::stoi(event.mWords[2]));
        else if (kind == "dialogue" && event.mWords.size() >= 2)
            dispatchDialogueResult(key(1), event.mWords.size() >= 3 ? std::stoul(event.mWords[2]) : 0,
                event.mWords.size() >= 4 ? ptrFor(key(3)) : Ptr{});
        else if (kind == "effect" && event.mWords.size() >= 4)
            dispatchEffect(key(1), event.mWords[2], ptrFor(key(3)), event.mWords.size() >= 5 ? ptrFor(key(4)) : Ptr{});
        else
            throw std::runtime_error("Invalid M7 scheduled event command " + kind);
        trace("acceptance-event command=" + kind);
    }

    void OblivionScriptManager::writeRuntimeReport() const
    {
        if (mReportPath.empty())
            return;
        std::ofstream out(mReportPath);
        if (!out)
            return;
        out << "{\"schema_version\":1,\"compiled_units\":" << mCompiledUnits
            << ",\"compilation_failures\":" << mCompilationFailures << ",\"event_sequence\":" << mSequence
            << ",\"diagnostics\":[";
        for (std::size_t i = 0; i < mDiagnostics.size(); ++i)
        {
            if (i) out << ',';
            out << "{\"code\":\"" << jsonEscape(mDiagnostics[i].mCode) << "\",\"message\":\""
                << jsonEscape(mDiagnostics[i].mMessage) << "\",\"command\":\""
                << jsonEscape(mDiagnostics[i].mCommand) << "\",\"unit\":\""
                << jsonEscape(mDiagnostics[i].mUnit.serialize()) << "\",\"event\":\""
                << jsonEscape(mDiagnostics[i].mEvent) << "\",\"sequence\":" << mDiagnostics[i].mSequence << '}';
        }
        out << "],\"command_counts\":{";
        std::size_t index = 0;
        for (const auto& [name, count] : mCommandCounts)
            out << (index++ ? "," : "") << '"' << jsonEscape(name) << "\":" << count;
        out << "},\"trace\":[";
        for (std::size_t i = 0; i < mTrace.size(); ++i)
            out << (i ? "," : "") << '"' << jsonEscape(mTrace[i]) << '"';
        out << "]}";
    }
}
