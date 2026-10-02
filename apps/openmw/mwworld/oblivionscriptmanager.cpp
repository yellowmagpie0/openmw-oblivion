#include "oblivionscriptmanager.hpp"
#include <components/esm4/observation.hpp>

#include <algorithm>
#include <array>
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
#include <components/esm4/dialoguevoices.hpp>
#include <components/esm4/idletree.hpp>
#include <components/esm4/runtimereferences.hpp>
#include <components/esm4/loadacti.hpp>
#include <components/esm4/loadachr.hpp>
#include <components/esm4/loadalch.hpp>
#include <components/esm4/loadappa.hpp>
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
#include <components/esm4/loadinfo.hpp>
#include <components/obscript/nativevariables.hpp>
#include <components/obscript/builtinreferences.hpp>
#include <components/esm4/loadkeym.hpp>
#include <components/esm4/loadligh.hpp>
#include <components/esm4/loadmisc.hpp>
#include <components/esm4/loadnpc.hpp>
#include <components/esm4/loadpgrd.hpp>
#include <components/esm4/loadrace.hpp>
#include <components/esm4/loadqust.hpp>
#include <components/esm4/loadrefr.hpp>
#include <components/esm4/loadscpt.hpp>
#include <components/esm4/loadsgst.hpp>
#include <components/esm4/loadslgm.hpp>
#include <components/esm4/loadsndr.hpp>
#include <components/esm4/loadsoun.hpp>
#include <components/esm4/loadweap.hpp>
#include <components/esm4/inventorymechanics.hpp>
#include <components/misc/strings/algorithm.hpp>
#include <components/misc/strings/lower.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/vfs/manager.hpp>
#include <components/vfs/recursivedirectoryiterator.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/inputmanager.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/soundmanager.hpp"
#include "../mwbase/statemanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwmechanics/oblivionai.hpp"
#include "../mwmechanics/oblivioncombat.hpp"
#include "../mwmechanics/oblivionidle.hpp"
#include "../mwgui/mode.hpp"
#include "../mwmechanics/npcstats.hpp"
#include "../mwphysics/physicssystem.hpp"
#include "../mwrender/esm4npcanimation.hpp"
#include "../mwrender/renderingmanager.hpp"
#include "../mwsound/sound.hpp"
#include "action.hpp"
#include "class.hpp"
#include "esmstore.hpp"
#include "globalvariablename.hpp"
#include "oblivionprofileservices.hpp"
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
                case ESM::REC_APPA4: return ptr.get<ESM4::Apparatus>()->mBase->mScriptId;
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
                case ESM::REC_SGST4: return ptr.get<ESM4::SigilStone>()->mBase->mScriptId;
                case ESM::REC_SLGM4: return ptr.get<ESM4::SoulGem>()->mBase->mScriptId;
                case ESM::REC_WEAP4: return ptr.get<ESM4::Weapon>()->mBase->mScriptId;
                default: return {};
            }
        }

        std::int32_t boundedCount(const ObScript::Value& value)
        {
            return static_cast<std::int32_t>(std::clamp<std::int64_t>(ObScript::asInteger(value),
                std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()));
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

        std::optional<double> numericScriptValue(const ObScript::Value& value)
        {
            return std::visit(
                [](const auto& item) -> std::optional<double> {
                    using T = std::decay_t<decltype(item)>;
                    if constexpr (std::is_same_v<T, std::int64_t> || std::is_same_v<T, double>)
                        return static_cast<double>(item);
                    else
                        return std::nullopt;
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
                {
                    Log(Debug::Error) << "M7 ObScript compile: unit=" << unit.mId.serialize()
                                      << " code=" << diagnostic.mDiagnostic.mCode
                                      << " message=" << diagnostic.mDiagnostic.mMessage;
                    if (auto* observation = mWorld.getOblivionObservation())
                        observation->diagnostic(diagnostic.mDiagnostic.mMessage);
                }
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
        indexBaseScripts(mStore.get<ESM4::Apparatus>());
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
        indexBaseScripts(mStore.get<ESM4::SigilStone>());
        indexBaseScripts(mStore.get<ESM4::SoulGem>());
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

    std::optional<OblivionScriptManager::NativeVoice> OblivionScriptManager::findNativeVoice(
        const ESM::FormKey& topic, const Ptr& actor, const Ptr& target, const ESM::FormKey& voiceType) const
    {
        std::string race;
        std::string sex;
        const bool diagnose = mDiagnosedVoiceTopics.emplace(topic, keyFor(actor)).second;
        const ESM4::Npc* npc = nullptr;
        if (!voiceType.isNull())
            npc = mStore.search<ESM4::Npc>(voiceType);
        if (npc == nullptr && !actor.isEmpty() && actor.getClass().getType() == ESM::REC_NPC_4)
            npc = actor.get<ESM4::Npc>()->mBase;
        if (npc != nullptr)
        {
            const bool female = (npc->mBaseConfig.tes4.flags & ESM4::Npc::TES4_Female) != 0;
            const auto resolveRace = [&](ESM::FormId id) { return mStore.get<ESM4::Race>().search(ESM::RefId(id)); };
            const ESM4::Race* voiceRace = ESM4::dialogueVoiceRace(resolveRace(npc->mRace), female, resolveRace);
            if (voiceRace == nullptr)
                throw std::runtime_error("Native dialogue voice race is unresolved or cyclic");
            race = lower(voiceRace->mFullName);
            sex = female ? "f" : "m";
        }

        const auto topicInfos = mTopicInfos.find(topic);
        if (diagnose)
            Log(Debug::Info) << "Native dialogue lookup: topic=" << topic.serialize()
                << " actor=" << keyFor(actor).serialize() << " race=" << race << " sex=" << sex
                << " infos=" << (topicInfos == mTopicInfos.end() ? 0 : topicInfos->second.size());
        if (topicInfos == mTopicInfos.end())
            return std::nullopt;
        for (const ESM::FormKey& info : topicInfos->second)
        {
            const ESM4::DialogInfo* definition = mStore.search<ESM4::DialogInfo>(info);
            const MWMechanics::OblivionAiService* ai = mWorld.getOblivionAiService();
            if (definition == nullptr || ai == nullptr)
                continue;
            const ESM::FormKey quest = mResolver.toFormKey(definition->mQuest);
            const auto questState = mQuests.find(quest);
            if (!quest.isNull() && (questState == mQuests.end() || !questState->second.mRunning))
                continue;
            const auto conditions = ai->evaluateDialogueConditions(actor, target, definition->mCanonicalConditions, diagnose);
            if (diagnose)
                Log(Debug::Info) << "Native dialogue candidate: info=" << info.serialize()
                    << " result=" << static_cast<int>(conditions);
            if (conditions == ESM4::ConditionResult::Unsupported || conditions == ESM4::ConditionResult::MissingContext)
                throw std::runtime_error("Native dialogue condition cannot be evaluated: " + info.serialize());
            if (conditions != ESM4::ConditionResult::True)
                continue;
            const auto files = mNativeVoiceFiles.find(info);
            if (files == mNativeVoiceFiles.end())
                return std::nullopt;
            const auto selected = ESM4::dialogueVoiceFiles(*definition, files->second, race, sex);
            if (diagnose)
                Log(Debug::Info) << "Native dialogue voices: info=" << info.serialize()
                    << " files=" << files->second.size() << " responses=" << definition->mResponses.size()
                    << " matched=" << selected.has_value();
            if (!selected)
                return std::nullopt;
            return NativeVoice{ info, *selected };
        }
        return std::nullopt;
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
        mNativeSpeech.clear();
        mDiagnosedVoiceTopics.clear();
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
        mIgnoreFriendlyHits = false;
        // OPENMW_OBSCRIPT_EVENTS is a process-local acceptance driver, not game content. Its commands must execute
        // once per launched scenario: replaying them after an in-process quickload can mutate actor values twice and
        // hide persistence regressions. A fresh process constructs a fresh manager with every event unexecuted.
    }

    void OblivionScriptManager::startNewGame()
    {
        clear();
        // CharacterGen is a native engine bootstrap quest rather than a
        // Start Game Enabled record. Starting it here lets its ordinary
        // GameMode script drive the tutorial state and package conditions.
        if (const auto characterGen = mStore.findEsm4FormKey("Charactergen"))
        {
            questState(*characterGen).mRunning = true;
            trace("startquest quest=" + characterGen->serialize() + " reason=new-game");
        }
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
        if (ESM4::runtimeReferenceKey(key) == ESM::FormKey::dynamic("player", 1))
            return mWorld.getPlayerPtr();
        if (const MWMechanics::OblivionAiService* oblivionAi = mWorld.getOblivionAiService())
            if (Ptr ptr = oblivionAi->resolveReference(key); !ptr.isEmpty())
                return ptr;
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

    const ESM4::RuntimeQuestState* OblivionScriptManager::findQuestState(const ESM::FormKey& quest) const
    {
        const auto found = mQuests.find(quest);
        return found == mQuests.end() ? nullptr : &found->second;
    }

    std::optional<double> OblivionScriptManager::scriptVariable(
        const ESM::FormKey& target, std::int32_t index) const
    {
        if (index < 0)
            return std::nullopt;

        std::shared_ptr<const ObScript::Program> program;
        if (const auto direct = mBaseScripts.find(target); direct != mBaseScripts.end())
            program = direct->second;
        else if (const auto script = mScripts.find(target); script != mScripts.end())
            program = script->second;
        else if (const auto reference = mReferenceBases.find(target); reference != mReferenceBases.end())
        {
            if (const auto base = mBaseScripts.find(reference->second); base != mBaseScripts.end())
                program = base->second;
        }
        else if (const ESM4::RuntimeReferenceState* state = referenceState(target))
        {
            if (const auto base = mBaseScripts.find(state->mBase); base != mBaseScripts.end())
                program = base->second;
        }
        if (!program)
            return std::nullopt;
        return nativeScriptVariable(*program, target, index);
    }

    std::optional<double> OblivionScriptManager::questVariable(
        const ESM::FormKey& quest, std::int32_t index) const
    {
        if (index < 0)
            return std::nullopt;
        const auto script = mQuestScripts.find(quest);
        if (script == mQuestScripts.end())
            return std::nullopt;
        const auto program = mScripts.find(script->second);
        if (program == mScripts.end())
            return std::nullopt;
        return nativeScriptVariable(*program->second, quest, index);
    }

    std::optional<double> OblivionScriptManager::nativeScriptVariable(
        const ObScript::Program& program, const ESM::FormKey& context, std::int32_t index) const
    {
        const ESM4::Script* definition = mStore.search<ESM4::Script>(program.mUnit.mOwner);
        if (definition == nullptr)
            return std::nullopt;
        const auto local = ObScript::nativeLocalIndex(definition->mScript, program, index);
        if (!local)
            return std::nullopt;
        const InstanceKey key{ program.mUnit.serialize(), context };
        const auto instance = mInstances.find(key);
        if (instance == mInstances.end())
            return 0.0;
        if (*local >= instance->second.mLocals.size())
            return std::nullopt;
        return numericScriptValue(instance->second.mLocals[*local]);
    }

    std::optional<Ptr> OblivionScriptManager::scriptedLookTarget(const Ptr& actor) const
    {
        const ESM4::RuntimeReferenceState* state = referenceState(keyFor(actor));
        if (state == nullptr)
            return std::nullopt;
        const auto found = state->mCustomState.find("obscript.look_target");
        if (found == state->mCustomState.end())
            return std::nullopt;
        const auto* saved = std::get_if<std::string>(&found->second);
        if (saved == nullptr)
            return Ptr{};
        const ESM::FormKey key = ESM::FormKey::deserialize(*saved);
        Ptr target;
        if (key == ESM::FormKey::dynamic("player", 1))
            target = mWorld.getPlayerPtr();
        else if (const auto id = mResolver.toFormId(key))
            target = mWorld.mWorldModel.getPtr(*id);
        // Looking never loads a remote cell or resurrects a disabled target.
        if (target.isEmpty() || !target.getRefData().isEnabled() || target.mRef->isDeleted()
            || !target.isInCell() || !actor.isInCell()
            || !mWorld.mWorldScene->getActiveCells().contains(target.getCell())
            || (target.getCell() != actor.getCell()
                && (!target.getCell()->isExterior() || !actor.getCell()->isExterior())))
            return Ptr{};
        return target;
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
                auto expected = ObScript::builtinReferenceKey(entry.mRuntimeArguments.front(), self, actionReference);
                if (!expected)
                    expected = mStore.findEsm4FormKey(entry.mRuntimeArguments.front());
                matches = expected && !expected->isNull() && *expected == actionReference;
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
        {
            try
            {
                mWorld.mOblivionRuntimeState
                    = std::make_unique<ESM4::RuntimeState>(mWorld.captureOblivionRuntimeState());
            }
            catch (const std::exception& error)
            {
                Log(Debug::Error) << "TES4 runtime-state capture failed during script update: " << error.what();
                throw;
            }
        }
        mWorld.mLastOblivionScriptSeconds = secondsPassed;
        mElapsed += secondsPassed;
        for (auto speech = mNativeSpeech.begin(); speech != mNativeSpeech.end();)
        {
            NativeSpeech& current = speech->second;
            current.mRemaining -= std::max(0.0, secondsPassed);
            if (current.mRemaining > 0)
            {
                ++speech;
                continue;
            }
            const Ptr speaker = ptrFor(speech->first);
            if (++current.mIndex >= current.mResponses.size() || speaker.isEmpty())
            {
                speech = mNativeSpeech.erase(speech);
                continue;
            }
            const auto& [file, duration] = current.mResponses[current.mIndex];
            current.mRemaining = duration;
            MWBase::Environment::get().getSoundManager()->say(speaker, VFS::Path::Normalized(file));
            ++speech;
        }
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
            if (!ptr.getRefData().isEnabled() || ptr.mRef->isDeleted())
                continue;
            dispatchObjectEvent(ptr, "onload");
            if (!ptr.getRefData().isEnabled() || ptr.mRef->isDeleted())
                continue;
            dispatchObjectEvent(ptr, "gamemode");
            if (!ptr.getRefData().isEnabled() || ptr.mRef->isDeleted())
                continue;
            const auto program = scriptFor(ptr);
            if (!program || std::none_of(program->mEntryPoints.begin(), program->mEntryPoints.end(),
                    [](const auto& entry) { return Misc::StringUtils::ciEqual(entry.mEvent, "ontrigger"); }))
                continue;
            // Snapshot and order actual narrow-phase overlaps before invoking
            // scripts: a callback may disable/delete a volume or move actors.
            auto overlapping = mWorld.mPhysics->getTriggerActors(ptr);
            std::sort(overlapping.begin(), overlapping.end(), [&](const Ptr& left, const Ptr& right) {
                return keyFor(left) < keyFor(right);
            });
            for (const Ptr& actor : overlapping)
            {
                if (!ptr.getRefData().isEnabled() || ptr.mRef->isDeleted())
                    break;
                if (actor.isEmpty() || !actor.isInCell() || !actor.getRefData().isEnabled() || actor.mRef->isDeleted())
                    continue;
                dispatchObjectEvent(ptr, "ontrigger", actor);
            }
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
        {
            if (name->starts_with("content:") || name->starts_with("dynamic:"))
                return ESM::FormKey::deserialize(*name);
            return mStore.findEsm4FormKey(*name);
        }
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

    void OblivionScriptManager::persistAnimationState(const ESM::FormKey& reference,
        std::string_view group, int mode, bool playing, bool scripted)
    {
        if (ESM4::RuntimeReferenceState* state = referenceState(reference))
        {
            state->mCustomState["obscript.animation_group"] = std::string(group);
            state->mCustomState["obscript.animation_mode"] = std::int64_t(mode);
            state->mCustomState["obscript.animation_playing"] = playing;
            state->mCustomState["obscript.animation_scripted"] = scripted;
            if (playing)
            {
                state->mCustomState["obscript.animation_progress"] = 0.0;
                state->mCustomState["obscript.animation_loop_count"] = std::int64_t(0);
                state->mCustomState["obscript.animation_absolute"] = false;
            }
        }
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
        if (const auto key = ObScript::builtinReferenceKey(name, context.mSelf, context.mActionReference))
            return ObScript::ReferenceValue{ *key, lower(name) };
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
        const ESM::Variant& global = mWorld.mGlobalVariables[GlobalVariableName(name)];
        if (global.getType() == ESM::VT_Float)
            mWorld.setGlobalFloat(GlobalVariableName(name), static_cast<float>(ObScript::asNumber(value)));
        else if (global.getType() == ESM::VT_String)
            mWorld.mGlobalVariables[GlobalVariableName(name)].setString(ObScript::valueString(value));
        else
            mWorld.setGlobalInt(GlobalVariableName(name), static_cast<int>(ObScript::asInteger(value)));
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
        const auto oblivionAi = [&]() { return mWorld.getOblivionAiService(); };
        const auto playAnimation = [&](const Ptr& ptr, const std::string& group, int mode, bool scripted = true) {
            bool played = false;
            if (!ptr.isEmpty())
                played = MWBase::Environment::get().getMechanicsManager()->playAnimationGroup(
                    ptr, group, mode, 1, scripted);
            persistAnimationState(objectKey(), group, mode, played, scripted);
            return played;
        };

        if (name == "look" || name == "stoplook")
        {
            const Ptr actor = objectPtr();
            if (actor.isEmpty() || !actor.getClass().isActor())
                throw std::runtime_error("Native Look requires an actor reference");
            ESM4::RuntimeReferenceState* state = referenceState(objectKey());
            if (state == nullptr)
                throw std::runtime_error("Native Look cannot persist its actor reference");
            if (name == "stoplook")
                state->mCustomState.erase("obscript.look_target");
            else
            {
                const auto key = keyFromValue(argument(0));
                if (!key || key->isNull())
                    throw std::runtime_error("Native Look requires a target reference");
                state->mCustomState["obscript.look_target"] = key->serialize();
            }
            trace(name + " actor=" + objectKey().serialize());
            return std::int64_t(0);
        }

        if (name == "getsecondspassed")
            return context.mSecondsPassed;
        if (name == "getbuttonpressed")
            return std::int64_t(MWBase::Environment::get().getWindowManager()->readPressedButton());
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
        if (name == "setinchargen")
        {
            const bool enabled = ObScript::asInteger(argument(0)) != 0;
            mWorld.mGlobalVariables[Globals::sCharGenState].setInteger(enabled ? 1 : -1);
            trace("setinchargen enabled=" + std::string(enabled ? "true" : "false"));
            return std::int64_t(0);
        }
        if (name == "setignorefriendlyhits")
        {
            mIgnoreFriendlyHits = ObScript::asInteger(argument(0)) != 0;
            trace("setignorefriendlyhits enabled=" + std::string(mIgnoreFriendlyHits ? "true" : "false"));
            return std::int64_t(0);
        }
        if (name == "getignorefriendlyhits")
            return std::int64_t(mIgnoreFriendlyHits);
        if (name == "autosave")
        {
            MWBase::Environment::get().getStateManager()->quickSave("Autosave");
            trace("autosave");
            return std::int64_t(0);
        }

        if (name == "enable" || name == "disable")
        {
            const ESM::FormKey key = objectKey();
            Ptr ptr = ptrFor(key);
            // Disabled references in a loaded cell need not have entered the
            // Ptr registry. Update their actual RefData and scene membership,
            // not just the saved native flag that capture would overwrite.
            if (ptr.isEmpty())
                if (const auto id = mResolver.toFormId(key))
                    ptr = mWorld.mWorldModel.getResidentPtr(*id);
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
            const bool played = playAnimation(ptr, group, mode);
            trace("playgroup ref=" + objectKey().serialize() + " group=" + group
                + " played=" + (played ? "true" : "false"));
            return std::int64_t(played);
        }
        if (name == "pickidle")
        {
            const Ptr actor = objectPtr();
            MWMechanics::OblivionAiService* ai = oblivionAi();
            MWRender::Animation* animation
                = actor.isEmpty() ? nullptr : mWorld.mRendering->getAnimation(actor);
            if (actor.isEmpty() || !actor.getClass().isActor() || ai == nullptr || animation == nullptr)
                throw ObScript::RuntimeError("OBSV110", "PickIdle requires a loaded native actor", name);

            const std::string skeleton = actor.getClass().getCorrectedModel(actor);
            const auto& store = mWorld.mStore.get<ESM4::IdleAnimation>();
            std::vector<const ESM4::IdleAnimation*> records;
            records.reserve(store.getSize());
            for (const ESM4::IdleAnimation& idle : store)
                records.push_back(&idle);
            const auto rooted = ESM4::rootedIdleAnimations(records);
            const ESM4::IdleTree tree(rooted);
            const Ptr player = mWorld.getPlayerPtr();
            const ESM4::IdleAnimation* selected = tree.select(
                [&](const ESM4::IdleAnimation& idle) {
                    return ai->evaluateDialogueConditions(actor, player, idle.mConditions)
                        == ESM4::ConditionResult::True;
                },
                [&](const ESM4::IdleAnimation& idle) {
                    return MWMechanics::oblivionPickIdleAnimationGroup(idle, skeleton).has_value();
                });

            bool played = false;
            std::string group;
            std::string idleKey = "null";
            if (selected != nullptr)
            {
                if (const auto key = store.findFormKey(ESM::RefId(selected->mId)))
                    idleKey = key->serialize();
                if (const auto selectedGroup = MWMechanics::oblivionPickIdleAnimationGroup(*selected, skeleton))
                {
                    group = *selectedGroup;
                    played = playAnimation(actor, group, 0, false);
                }
            }
            trace("pickidle actor=" + objectKey().serialize() + " idle=" + idleKey + " group=" + group
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
            if (owner == ESM::FormKey::dynamic("player", 1))
            {
                if (name == "getitemcount")
                    return std::int64_t(mWorld.oblivionPlayerItemCount(*item));
                const std::int32_t count = std::max<std::int32_t>(1, boundedCount(argument(itemArg + 1)));
                mWorld.oblivionChangePlayerInventory(*item, name == "additem" ? count : -count);
                trace(name + " owner=" + owner.serialize() + " item=" + item->serialize()
                    + " count=" + std::to_string(count));
                if (name == "additem")
                    dispatchBaseEvent(*item, "onadd", owner);
                return std::int64_t(0);
            }
            ESM4::RuntimeReferenceState* state = referenceState(owner);
            if (!state)
                return std::int64_t(0);
            if (name == "getitemcount")
                return std::int64_t(ESM4::inventoryCount(state->mInventory, *item));
            const std::int32_t count = std::max<std::int32_t>(1, boundedCount(argument(itemArg + 1)));
            if (name == "additem")
            {
                ESM4::RuntimeInventoryItem added;
                added.mBase = *item;
                added.mCount = count;
                if (const std::optional<ESM::FormId> id = mResolver.toFormId(*item))
                    if (auto definition = OblivionProfileServices::itemDefinition(mWorld.mStore, ESM::RefId(*id)))
                    {
                        added.mCondition = definition->mMaxCondition;
                        added.mCharge = definition->mMaxCharge;
                    }
                ESM4::addInventoryItem(state->mInventory, std::move(added));
            }
            else
                ESM4::removeInventoryItem(state->mInventory, *item, count);
            trace(name + " owner=" + owner.serialize() + " item=" + item->serialize()
                + " count=" + std::to_string(count));
            if (name == "additem")
                dispatchBaseEvent(*item, "onadd", owner);
            return std::int64_t(0);
        }

        if (name == "getequipped")
        {
            const auto item = keyFromValue(argument(0));
            if (!item)
                return std::int64_t(0);
            const ESM::FormKey owner = objectKey();
            if (owner != ESM::FormKey::dynamic("player", 1))
            {
                const Ptr actor = ptrFor(owner);
                if (!actor.isEmpty() && (actor.getType() == ESM::REC_NPC_4 || actor.getType() == ESM::REC_CREA4))
                    return std::int64_t(mWorld.oblivionActorItemEquipped(actor, *item));
            }
            if (!mWorld.mOblivionRuntimeState)
                return std::int64_t(0);
            const std::vector<ESM4::RuntimeInventoryItem>* inventory = nullptr;
            if (owner == ESM::FormKey::dynamic("player", 1))
                inventory = &mWorld.mOblivionRuntimeState->mPlayer.mInventory;
            else if (const ESM4::RuntimeReferenceState* state = referenceState(owner))
                inventory = &state->mInventory;
            return std::int64_t(inventory != nullptr
                && std::ranges::any_of(*inventory, [&](const ESM4::RuntimeInventoryItem& entry) {
                       return entry.mBase == *item && entry.mCount > 0 && entry.mEquippedSlots != 0;
                   }));
        }

        if (name == "equipitem" || name == "unequipitem")
        {
            const auto item = keyFromValue(argument(0));
            if (!item)
                return std::int64_t(0);
            const bool equip = name == "equipitem";
            const ESM::FormKey owner = objectKey();
            bool changed = false;
            if (owner == ESM::FormKey::dynamic("player", 1))
                changed = mWorld.oblivionEquipPlayerItem(*item, equip);
            else if (ESM4::RuntimeReferenceState* state = referenceState(owner))
            {
                const Ptr actor = ptrFor(owner);
                const bool liveNativeActor = !actor.isEmpty()
                    && (actor.getType() == ESM::REC_NPC_4 || actor.getType() == ESM::REC_CREA4);
                if (liveNativeActor)
                {
                    changed = mWorld.oblivionEquipActorItem(actor, *item, equip);
                    // Controller/equipment callbacks may reenter native state.
                    state = referenceState(owner);
                    if (!state)
                        throw ObScript::RuntimeError("OBSV118", "Native equipment owner disappeared", name);
                }
                if (equip && (!liveNativeActor || changed))
                {
                    if (const std::optional<ESM::FormId> id = mResolver.toFormId(*item))
                        if (auto definition
                            = OblivionProfileServices::itemDefinition(mWorld.mStore, ESM::RefId(*id)))
                        {
                            definition->mBase = *item;
                            const bool projected = ESM4::equipInventoryItem(state->mInventory, *definition);
                            if (!liveNativeActor)
                                changed = projected;
                        }
                }
                else if (!equip && (!liveNativeActor || changed))
                {
                    const bool projected = ESM4::unequipInventoryItem(state->mInventory, *item);
                    if (!liveNativeActor)
                        changed = projected;
                }
                if (changed)
                {
                    if (!actor.isEmpty() && mWorld.mRendering != nullptr)
                        if (auto* animation
                            = dynamic_cast<MWRender::ESM4NpcAnimation*>(mWorld.mRendering->getAnimation(actor)))
                            animation->refreshEquipment();
                }
            }
            trace(name + " owner=" + owner.serialize() + " item=" + item->serialize()
                + " changed=" + (changed ? "true" : "false"));
            return std::int64_t(changed);
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
        if (name == "getopenstate")
        {
            const Ptr ptr = objectPtr();
            if (ptr.isEmpty() || ptr.getClass().getType() != ESM::REC_DOOR4)
                return std::int64_t(0);
            return std::int64_t(MWMechanics::oblivionDoorOpenState(ptr.getClass().getDoorState(ptr),
                ptr.getRefData().getPosition().rot[2], ptr.getCellRef().getPosition().rot[2]));
        }
        if (name == "setopenstate")
        {
            const Ptr ptr = objectPtr();
            if (ptr.isEmpty() || ptr.getClass().getType() != ESM::REC_DOOR4)
                throw ObScript::RuntimeError("OBSV110", "SetOpenState requires a native TES4 door", name);
            const int requested = static_cast<int>(ObScript::asInteger(argument(0)));
            const std::optional<MWWorld::DoorState> transition
                = MWMechanics::oblivionDoorTransition(requested);
            if (!transition)
                throw ObScript::RuntimeError("OBSV110", "SetOpenState requires 0 or 1", name);
            const int current = MWMechanics::oblivionDoorOpenState(ptr.getClass().getDoorState(ptr),
                ptr.getRefData().getPosition().rot[2], ptr.getCellRef().getPosition().rot[2]);
            if ((requested == 0 && current != 3 && current != 4)
                || (requested == 1 && current != 1 && current != 2))
                mWorld.activateDoor(ptr, *transition);
            trace(name + " ref=" + objectKey().serialize() + " state=" + std::to_string(requested));
            return std::int64_t(0);
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
            if (const auto* combat = mWorld.getOblivionCombatService())
                if (const auto* life = combat->findActorLife(ESM4::runtimeReferenceKey(objectKey())))
                    return std::int64_t(life->mPhase == ESM4::ActorLifePhase::Dead);
            const ESM4::RuntimeReferenceState* state = referenceState(objectKey());
            if (state)
                if (const auto dead = state->mCustomState.find("obscript.dead"); dead != state->mCustomState.end())
                    return std::int64_t(std::get<bool>(dead->second));
            return std::int64_t(0);
        }
        if (name == "getdeadcount")
        {
            const auto base = keyFromValue(argument(0));
            if (!base)
                return std::int64_t(0);
            if (const auto* combat = mWorld.getOblivionCombatService())
                return std::int64_t(combat->getDeadCount(*base));
            return std::int64_t(0);
        }
        if (name == "kill" || name == "resurrect")
        {
            const auto nativeKey = ESM4::runtimeReferenceKey(objectKey());
            if (const auto* combat = mWorld.getOblivionCombatService(); combat
                && (combat->findActorValues(nativeKey) || combat->findActorLife(nativeKey)))
            {
                try
                {
                    if (name == "resurrect")
                        throw std::invalid_argument("native resurrection reset is not implemented");
                    ESM::FormKey killer;
                    if (!arguments.empty())
                    {
                        const auto source = keyFromValue(argument(0));
                        if (!source)
                            throw std::invalid_argument("native Kill requires an optional actor reference");
                        killer = ESM4::runtimeReferenceKey(*source);
                        if (!killer.isNull())
                        {
                            const Ptr sourceActor = ptrFor(killer);
                            if (sourceActor.isEmpty() || !sourceActor.getClass().isActor())
                                throw std::invalid_argument("native Kill source must be an actor");
                        }
                    }
                    if (!mWorld.killOblivionActor(objectPtr(), killer))
                        throw std::invalid_argument("native Kill requires a registered resident actor");
                    return std::int64_t(0);
                }
                catch (const std::exception& error)
                {
                    throw ObScript::RuntimeError("OBSV116", error.what(), name);
                }
            }
            if (ESM4::RuntimeReferenceState* state = referenceState(objectKey()))
                state->mCustomState["obscript.dead"] = name == "kill";
            if (name == "kill")
                dispatchObjectEvent(objectKey(), "ondeath", context.mSelf);
            return std::int64_t(0);
        }

        if (name == "evaluatepackage" || name == "evp")
        {
            const Ptr actor = objectPtr();
            if (oblivionAi() == nullptr || actor.isEmpty() || !oblivionAi()->handles(actor))
                throw ObScript::RuntimeError("OBSV102", "EvaluatePackage requires a native TES4 actor", name);
            const bool evaluated = oblivionAi()->evaluatePackage(actor);
            trace(name + " actor=" + objectKey().serialize() + " evaluated="
                + (evaluated ? "true" : "false"));
            return std::int64_t(evaluated);
        }
        if (name == "addscriptpackage")
        {
            const Ptr actor = objectPtr();
            const auto package = keyFromValue(argument(0));
            if (!package)
                throw ObScript::RuntimeError("OBSV103", "AddScriptPackage requires a native PACK reference", name);
            if (oblivionAi() == nullptr || actor.isEmpty() || !oblivionAi()->handles(actor))
                throw ObScript::RuntimeError("OBSV104", "AddScriptPackage requires a native TES4 actor", name);
            try
            {
                const bool added = oblivionAi()->addScriptPackage(actor, *package);
                trace(name + " actor=" + objectKey().serialize() + " package=" + package->serialize());
                return std::int64_t(added);
            }
            catch (const std::exception& error)
            {
                throw ObScript::RuntimeError("OBSV104", error.what(), name);
            }
        }
        if (name == "forceflee")
        {
            const Ptr actor = objectPtr();
            const auto threat = keyFromValue(argument(0));
            if (!threat)
                throw ObScript::RuntimeError("OBSV105", "ForceFlee requires a threat reference", name);
            const float duration = arguments.size() > 1
                ? static_cast<float>(std::max(0.0, ObScript::asNumber(argument(1)))) : 1.f;
            if (oblivionAi() == nullptr || actor.isEmpty() || !oblivionAi()->handles(actor))
                throw ObScript::RuntimeError("OBSV105", "ForceFlee requires a native TES4 actor", name);
            const bool applied = oblivionAi()->forceFlee(actor, *threat, duration);
            trace(name + " actor=" + objectKey().serialize() + " threat=" + threat->serialize()
                + " applied=" + (applied ? "true" : "false"));
            return std::int64_t(applied);
        }
        if (name == "setrestrained")
        {
            const Ptr actor = objectPtr();
            const bool restrained = ObScript::asInteger(argument(0)) != 0;
            if (oblivionAi() == nullptr || actor.isEmpty() || !oblivionAi()->handles(actor))
                throw ObScript::RuntimeError("OBSV108", "SetRestrained requires a native TES4 actor", name);
            const bool applied = oblivionAi()->setRestrained(actor, restrained);
            trace(name + " actor=" + objectKey().serialize() + " value=" + (restrained ? "true" : "false")
                + " applied=" + (applied ? "true" : "false"));
            return std::int64_t(applied);
        }
        if (name == "pathpointenable" || name == "pathpointdisable")
        {
            const bool enabled = name == "pathpointenable";
            ESM::FormKey pathgrid;
            std::size_t nodeArgument = 0;
            const auto firstKey = keyFromValue(argument(0));
            if (firstKey && mStore.search<ESM4::Pathgrid>(*firstKey) != nullptr)
            {
                pathgrid = *firstKey;
                nodeArgument = 1;
            }
            else if (target)
            {
                const ESM::FormKey explicitKey = objectKey();
                if (mStore.search<ESM4::Pathgrid>(explicitKey) != nullptr)
                    pathgrid = explicitKey;
            }
            if (pathgrid.isNull())
            {
                const Ptr actor = objectPtr();
                if (!actor.isEmpty() && actor.isInCell())
                    if (const ESM::FormId* id = actor.getCell()->getCell()->getId().getIf<ESM::FormId>())
                        if (const ESM4::PathgridGraph* graph
                            = mStore.getOblivionPathgridService().graphForCell(mResolver.toFormKey(*id)))
                            pathgrid = graph->pathgridKey();
            }
            if (pathgrid.isNull() || nodeArgument >= arguments.size() || !ObScript::isNumeric(argument(nodeArgument)))
                throw ObScript::RuntimeError("OBSV106", "PathPoint command requires a valid graph and node", name);
            const std::int64_t node = ObScript::asInteger(argument(nodeArgument));
            if (node < 0 || static_cast<std::uint64_t>(node) > std::numeric_limits<std::uint32_t>::max())
                throw ObScript::RuntimeError("OBSV106", "PathPoint node is outside the native range", name);
            if (oblivionAi() == nullptr)
                throw ObScript::RuntimeError("OBSV107", "PathPoint command has no native TES4 AI service", name);
            try
            {
                const bool changed = oblivionAi()->setPathPoint(pathgrid, static_cast<std::uint32_t>(node), enabled);
                trace(name + " pathgrid=" + pathgrid.serialize() + " node=" + std::to_string(node)
                    + " changed=" + (changed ? "true" : "false"));
                return std::int64_t(changed);
            }
            catch (const std::exception& error)
            {
                throw ObScript::RuntimeError("OBSV107", error.what(), name);
            }
        }

        if (name == "getcurrentaipackage")
        {
            const Ptr actor = objectPtr();
            if (oblivionAi() != nullptr && !actor.isEmpty() && oblivionAi()->handles(actor))
                if (const ESM4::RuntimeActorAiState* state = oblivionAi()->state(actor))
                    return std::int64_t(state->mPackageType == ESM4::AIPackageType::Unknown
                            ? -1 : static_cast<std::uint8_t>(state->mPackageType));
            return std::int64_t(-1);
        }
        if (name == "getiscurrentpackage")
        {
            const Ptr actor = objectPtr();
            const auto package = keyFromValue(argument(0));
            return std::int64_t(package && oblivionAi() != nullptr && !actor.isEmpty()
                && oblivionAi()->handles(actor) && oblivionAi()->isCurrentPackage(actor, *package));
        }
        if (name == "getcurrentaiprocedure")
        {
            const Ptr actor = objectPtr();
            if (oblivionAi() != nullptr && !actor.isEmpty() && oblivionAi()->handles(actor))
                return std::int64_t(static_cast<std::uint16_t>(oblivionAi()->currentProcedure(actor)));
            return std::int64_t(0);
        }
        if (name == "getlos" || name == "getlineofsight" || name == "getdetected"
            || name == "getdetectionlevel")
        {
            const Ptr observer = objectPtr();
            const auto targetKey = keyFromValue(argument(0));
            const Ptr detected = targetKey ? ptrFor(*targetKey) : Ptr{};
            if (observer.isEmpty() || detected.isEmpty())
                return name == "getdetectionlevel" ? ObScript::Value(double(0)) : ObScript::Value(std::int64_t(0));
            if (oblivionAi() != nullptr && oblivionAi()->handles(observer))
            {
                if (name == "getlos" || name == "getlineofsight")
                    return std::int64_t(oblivionAi()->getLOS(observer, detected));
                const ESM4::DetectionResult result = oblivionAi()->detection(observer, detected);
                return name == "getdetectionlevel" ? ObScript::Value(result.mLevel)
                                                      : ObScript::Value(std::int64_t(result.mDetected));
            }
            if (name == "getlos" || name == "getlineofsight")
                return std::int64_t(mWorld.getLOS(observer, detected));
            const bool visible = mWorld.getLOS(observer, detected)
                && MWBase::Environment::get().getMechanicsManager()->awarenessCheck(detected, observer);
            return name == "getdetectionlevel" ? ObScript::Value(visible ? 100.0 : 0.0)
                                                  : ObScript::Value(std::int64_t(visible));
        }

        if (name == "startcombat" || name == "stopcombat")
        {
            try
            {
                const bool start = name == "startcombat";
                if (arguments.size() != (start ? 1u : 0u))
                    throw std::invalid_argument("native combat command has invalid argument count");
                auto* combat = mWorld.getOblivionCombatService();
                const auto requireActor = [&](const ESM::FormKey& reference) {
                    const Ptr actor = ptrFor(reference);
                    const auto key = ESM4::runtimeReferenceKey(reference);
                    if (!combat || actor.isEmpty() || !actor.getClass().isActor()
                        || (actor != mWorld.getPlayerPtr()
                            && actor.getType() != ESM::REC_NPC_4 && actor.getType() != ESM::REC_CREA4)
                        || !combat->findActorValues(key) || !combat->findActorLife(key))
                        throw std::invalid_argument("native combat command requires an initialized resident actor");
                    return key;
                };
                const auto actor = requireActor(objectKey());
                if (start)
                {
                    const auto targetKey = keyFromValue(argument(0));
                    if (!targetKey)
                        throw std::invalid_argument("StartCombat requires an actor reference");
                    const auto opponent = requireActor(*targetKey);
                    combat->engage(actor, opponent);
                }
                else if (actor != ESM::FormKey::dynamic("player", 1) && combat->isInCombat(actor))
                {
                    // The native command uses actor +334(true), whose Player
                    // implementation is false. It does not use IsInCombat's
                    // special Player-list query and is a no-op out of combat.
                    combat->stopCombat(actor);
                    combat->cancelActorActions(actor);
                }
                trace(name + " actor=" + actor.serialize());
                return std::int64_t(0);
            }
            catch (const std::exception& error)
            {
                throw ObScript::RuntimeError("OBSV117", error.what(), name);
            }
        }
        if (name == "isincombat")
        {
            const auto* combat = mWorld.getOblivionCombatService();
            return std::int64_t(combat && combat->isInCombat(ESM4::runtimeReferenceKey(objectKey())));
        }
        if (name == "isspelltarget" || name == "ispcamurderer")
            return std::int64_t(0);
        if (name == "issneaking")
        {
            const Ptr actor = objectPtr();
            return std::int64_t(!actor.isEmpty()
                && MWBase::Environment::get().getMechanicsManager()->isSneaking(actor));
        }
        if (name == "isininterior")
        {
            const Ptr ptr = objectPtr();
            return std::int64_t(!ptr.isEmpty() && ptr.isInCell() && !ptr.getCell()->isExterior());
        }
        if (name == "isridinghorse")
        {
            const Ptr actor = objectPtr();
            return std::int64_t(oblivionAi() != nullptr && !actor.isEmpty() && oblivionAi()->handles(actor)
                && oblivionAi()->isRidingHorse(actor));
        }
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

        if (name == "getav" || name == "getactorvalue" || name == "getbaseav" || name == "getbaseactorvalue")
        {
            const std::string attribute = lower(ObScript::valueString(argument(0)));
            const bool base = name == "getbaseav" || name == "getbaseactorvalue";
            try
            {
                const auto index = ESM4::actorValueIndex(attribute);
                if (const auto native = mWorld.getOblivionScriptActorValue(objectKey(), index.value_or(255), base))
                    return *native;
            }
            catch (const std::invalid_argument& error)
            {
                throw ObScript::RuntimeError("OBSV115", error.what(), name);
            }
            if (objectKey() == ESM::FormKey::dynamic("player", 1))
            {
                const Ptr player = mWorld.getPlayerPtr();
                const MWMechanics::CreatureStats& stats = player.getClass().getCreatureStats(player);
                const MWMechanics::NpcStats& npcStats = player.getClass().getNpcStats(player);
                static constexpr std::array attributeNames{ "strength", "intelligence", "willpower", "agility",
                    "speed", "endurance", "personality", "luck" };
                for (std::size_t i = 0; i < attributeNames.size(); ++i)
                    if (attribute == attributeNames[i])
                    {
                        const MWMechanics::AttributeValue& value
                            = stats.getAttribute(ESM::Attribute::indexToRefId(i));
                        return base ? double(value.getBase()) : double(value.getModified());
                    }
                static constexpr std::array skillNames{ "armorer", "athletics", "blade", "block", "blunt",
                    "handtohand", "heavyarmor", "alchemy", "alteration", "conjuration", "destruction",
                    "illusion", "mysticism", "restoration", "acrobatics", "lightarmor", "marksman",
                    "mercantile", "security", "sneak", "speechcraft" };
                static const std::array skillIds{ ESM::Skill::Armorer, ESM::Skill::Athletics,
                    ESM::Skill::LongBlade, ESM::Skill::Block, ESM::Skill::BluntWeapon, ESM::Skill::HandToHand,
                    ESM::Skill::HeavyArmor, ESM::Skill::Alchemy, ESM::Skill::Alteration, ESM::Skill::Conjuration,
                    ESM::Skill::Destruction, ESM::Skill::Illusion, ESM::Skill::Mysticism, ESM::Skill::Restoration,
                    ESM::Skill::Acrobatics, ESM::Skill::LightArmor, ESM::Skill::Marksman, ESM::Skill::Mercantile,
                    ESM::Skill::Security, ESM::Skill::Sneak, ESM::Skill::Speechcraft };
                for (std::size_t i = 0; i < skillNames.size(); ++i)
                    if (attribute == skillNames[i])
                    {
                        const MWMechanics::SkillValue& value = npcStats.getSkill(ESM::RefId(skillIds[i]));
                        return base ? double(value.getBase()) : double(value.getModified());
                    }
                const auto dynamic = [&](const MWMechanics::DynamicStat<float>& value) {
                    return base ? double(value.getBase()) : double(value.getCurrent());
                };
                if (attribute == "health") return dynamic(stats.getHealth());
                if (attribute == "magicka") return dynamic(stats.getMagicka());
                if (attribute == "fatigue") return dynamic(stats.getFatigue());
                if (attribute == "encumbrance") return double(player.getClass().getEncumbrance(player));
                if (attribute == "carryweight") return double(player.getClass().getCapacity(player));
                if (attribute == "level") return double(stats.getLevel());
                return double(0);
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
        if (name == "setav" || name == "setactorvalue" || name == "forceav" || name == "forceactorvalue"
            || name == "modav" || name == "modactorvalue")
        {
            const std::string attribute = lower(ObScript::valueString(argument(0)));
            const double value = ObScript::asNumber(argument(1));
            const bool mod = name == "modav" || name == "modactorvalue";
            const auto key = ESM4::runtimeReferenceKey(objectKey());
            if (const auto* combat = mWorld.getOblivionCombatService(); combat && combat->findActorValues(key))
            {
                try
                {
                    const auto index = ESM4::actorValueIndex(attribute);
                    const auto requested = ObScript::asInteger(argument(1));
                    if (!index || !std::isfinite(value) || requested < std::numeric_limits<std::int32_t>::min()
                        || requested > std::numeric_limits<std::int32_t>::max())
                        throw std::invalid_argument("native actor-value command requires a known value and int32 argument");
                    const auto nativeCommand = mod ? ESM4::ActorValueCommand::Mod
                        : name == "setav" || name == "setactorvalue" ? ESM4::ActorValueCommand::Set
                        : ESM4::ActorValueCommand::Force;
                    if (!mWorld.executeOblivionActorValueCommand(key, *index, nativeCommand,
                            ESM4::ActorValueCommandSource::Script, static_cast<std::int32_t>(requested)))
                        throw std::invalid_argument("native actor-value command requires a registered actor");
                    return std::int64_t(0);
                }
                catch (const std::exception& error)
                {
                    throw ObScript::RuntimeError("OBSV115", error.what(), name);
                }
            }
            if (objectKey() == ESM::FormKey::dynamic("player", 1))
            {
                const Ptr player = mWorld.getPlayerPtr();
                MWMechanics::CreatureStats& stats = player.getClass().getCreatureStats(player);
                MWMechanics::NpcStats& npcStats = player.getClass().getNpcStats(player);
                static constexpr std::array attributeNames{ "strength", "intelligence", "willpower", "agility",
                    "speed", "endurance", "personality", "luck" };
                bool applied = false;
                for (std::size_t i = 0; i < attributeNames.size(); ++i)
                    if (attribute == attributeNames[i])
                    {
                        MWMechanics::AttributeValue actorValue = stats.getAttribute(ESM::Attribute::indexToRefId(i));
                        actorValue.setBase(static_cast<float>(mod ? actorValue.getBase() + value : value),
                            !mod);
                        stats.setAttribute(ESM::Attribute::indexToRefId(i), actorValue);
                        applied = true;
                    }
                static constexpr std::array skillNames{ "armorer", "athletics", "blade", "block", "blunt",
                    "handtohand", "heavyarmor", "alchemy", "alteration", "conjuration", "destruction",
                    "illusion", "mysticism", "restoration", "acrobatics", "lightarmor", "marksman",
                    "mercantile", "security", "sneak", "speechcraft" };
                static const std::array skillIds{ ESM::Skill::Armorer, ESM::Skill::Athletics,
                    ESM::Skill::LongBlade, ESM::Skill::Block, ESM::Skill::BluntWeapon, ESM::Skill::HandToHand,
                    ESM::Skill::HeavyArmor, ESM::Skill::Alchemy, ESM::Skill::Alteration, ESM::Skill::Conjuration,
                    ESM::Skill::Destruction, ESM::Skill::Illusion, ESM::Skill::Mysticism, ESM::Skill::Restoration,
                    ESM::Skill::Acrobatics, ESM::Skill::LightArmor, ESM::Skill::Marksman, ESM::Skill::Mercantile,
                    ESM::Skill::Security, ESM::Skill::Sneak, ESM::Skill::Speechcraft };
                for (std::size_t i = 0; i < skillNames.size(); ++i)
                    if (attribute == skillNames[i])
                    {
                        MWMechanics::SkillValue& actorValue = npcStats.getSkill(ESM::RefId(skillIds[i]));
                        actorValue.setBase(static_cast<float>(mod ? actorValue.getBase() + value : value),
                            !mod);
                        applied = true;
                    }
                const auto setDynamic = [&](MWMechanics::DynamicStat<float> actorValue, auto setter) {
                    if (mod)
                        actorValue.setCurrent(actorValue.getCurrent() + static_cast<float>(value), true, true);
                    else
                    {
                        actorValue.setBase(static_cast<float>(value));
                        actorValue.setCurrent(static_cast<float>(value), true, true);
                    }
                    (stats.*setter)(actorValue);
                    applied = true;
                };
                if (attribute == "health") setDynamic(stats.getHealth(), &MWMechanics::CreatureStats::setHealth);
                else if (attribute == "magicka")
                    setDynamic(stats.getMagicka(), &MWMechanics::CreatureStats::setMagicka);
                else if (attribute == "fatigue")
                    setDynamic(stats.getFatigue(), &MWMechanics::CreatureStats::setFatigue);
                else if (attribute == "level")
                {
                    stats.setLevel(static_cast<int>(mod ? stats.getLevel() + value : value));
                    applied = true;
                }
                if (applied)
                    mWorld.mOblivionRuntimeState
                        = std::make_unique<ESM4::RuntimeState>(mWorld.captureOblivionRuntimeState());
            }
            else if (ESM4::RuntimeReferenceState* state = referenceState(objectKey()))
            {
                ESM4::RuntimeValue& saved = state->mCustomState["obscript.av." + attribute];
                double current = 0;
                if (const double* number = std::get_if<double>(&saved))
                    current = *number;
                saved = mod ? current + value : value;
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
            const bool enabled = name == "enablelinkedpathpoints";
            const ESM::FormKey object = objectKey();
            if (oblivionAi() == nullptr || !oblivionAi()->hasLinkedPathPoints(object))
                throw ObScript::RuntimeError("OBSV109", "Linked path-point command requires a linked PGRL object", name);
            const bool changed = oblivionAi()->setLinkedPathPoints(object, enabled);
            trace(name + " object=" + object.serialize() + " changed=" + (changed ? "true" : "false"));
            return std::int64_t(changed);
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
            const auto targetKey = name == "sayto" ? keyFromValue(argument(0)) : std::nullopt;
            const Ptr listener = targetKey ? ptrFor(*targetKey) : mWorld.getPlayerPtr();
            const auto voice = topic ? findNativeVoice(*topic, speaker, listener, voiceType.value_or(ESM::FormKey{}))
                                     : std::nullopt;
            if (!voice)
            {
                trace(name + " topic=" + (topic ? topic->serialize() : std::string("null"))
                    + " voice=missing");
                return double(0);
            }
            MWBase::SoundManager* manager = MWBase::Environment::get().getSoundManager();
            NativeSpeech speech;
            double duration = 0;
            for (const auto& file : voice->mFiles)
            {
                const double responseDuration = manager->getSoundFileDuration(VFS::Path::Normalized(file));
                if (!std::isfinite(responseDuration) || responseDuration <= 0)
                    throw std::runtime_error("Native dialogue response has no valid duration: " + file);
                speech.mResponses.emplace_back(file, responseDuration);
                duration += responseDuration;
            }
            speech.mRemaining = speech.mResponses.front().second;
            const VFS::Path::Normalized path(voice->mFiles.front());
            if (!speaker.isEmpty()
                && (speaker.getClass().getType() == ESM::REC_NPC_4
                    || speaker.getClass().getType() == ESM::REC_CREA4))
                manager->say(speaker, path);
            else
                manager->say(path);
            mNativeSpeech.insert_or_assign(keyFor(speaker), std::move(speech));
            trace(name + " topic=" + topic->serialize() + " info=" + voice->mInfo.serialize()
                + " responses=" + std::to_string(voice->mFiles.size()) + " voice=" + voice->mFiles.front()
                + " duration=" + std::to_string(duration));
            // A scripted Say speaks the selected INFO now; its result must
            // run against that speaker, not an arbitrary topic voice entry.
            dispatchDialogueResult(voice->mInfo, 0, speaker);
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

        if (name == "showracemenu" || name == "showclassmenu" || name == "showbirthsignmenu")
        {
            MWBase::WindowManager* window = MWBase::Environment::get().getWindowManager();
            // Native scripts can open these independently. Do not stack stale character-generation dialogs, and
            // expose the real playable CLAS list directly instead of the Morrowind questionnaire choice screen.
            for (const MWGui::GuiMode characterMode : { MWGui::GM_Name, MWGui::GM_Race, MWGui::GM_Class,
                     MWGui::GM_ClassGenerate, MWGui::GM_ClassPick, MWGui::GM_ClassCreate, MWGui::GM_Birth,
                     MWGui::GM_Review })
                window->removeGuiMode(characterMode);
            const MWGui::GuiMode mode = name == "showracemenu" ? MWGui::GM_Race
                : name == "showclassmenu" ? MWGui::GM_ClassPick : MWGui::GM_Birth;
            window->pushGuiMode(mode);
            trace("character_menu command=" + name);
            return std::int64_t(0);
        }

        if (name == "enableplayercontrols" || name == "disableplayercontrols")
        {
            const bool enable = name == "enableplayercontrols";
            MWBase::InputManager* input = MWBase::Environment::get().getInputManager();
            const auto selected = [&](std::size_t index) {
                return arguments.empty() || index >= arguments.size() || ObScript::asInteger(argument(index)) != 0;
            };
            if (selected(0))
            {
                input->toggleControlSwitch("playercontrols", enable);
                input->toggleControlSwitch("playerjumping", enable);
            }
            if (selected(1))
            {
                input->toggleControlSwitch("playerfighting", enable);
                input->toggleControlSwitch("playermagic", enable);
            }
            if (selected(2)) input->toggleControlSwitch("playerviewswitch", enable);
            if (selected(3)) input->toggleControlSwitch("playerlooking", enable);
            trace(name + " movement=" + (selected(0) ? "true" : "false")
                + " fighting=" + (selected(1) ? "true" : "false")
                + " view=" + (selected(2) ? "true" : "false")
                + " looking=" + (selected(3) ? "true" : "false"));
            return std::int64_t(0);
        }

        // These commands acknowledge state owned by later AI/UI/audio/magic
        // milestones without pretending their subsystem effect occurred. They
        // retain deterministic control-flow compatibility for M7 scripts.
        static const std::set<std::string, std::less<>> deferred{
            "addtopic", "showmap",
            "cast", "addspell", "removespell", "moddisposition", "setessential",
            "setquestobject", "setownership", "setfactionrank", "modfactionrank", "setcrimegold",
            "setunconscious" };
        if (deferred.contains(name))
        {
            trace("deferred command=" + name + " unit=" + context.mUnit.serialize());
            if (auto* observation = mWorld.getOblivionObservation())
                observation->diagnostic("deferred command=" + name + " unit=" + context.mUnit.serialize(), true);
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
        mNativeSpeech.clear();
        mDiagnosedVoiceTopics.clear();
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
        if (auto* observation = mWorld.getOblivionObservation())
            observation->diagnostic(diagnostic.mMessage, diagnostic.mCode == "OBSV100");
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
        else if (kind == "command" && event.mWords.size() >= 3)
        {
            const ESM::FormKey targetKey = key(1);
            std::vector<ObScript::Value> arguments;
            for (std::size_t i = 3; i < event.mWords.size(); ++i)
            {
                char* end = nullptr;
                const double number = std::strtod(event.mWords[i].c_str(), &end);
                if (end != event.mWords[i].c_str() && *end == '\0')
                    arguments.emplace_back(number);
                else
                    arguments.emplace_back(event.mWords[i]);
            }
            ObScript::RuntimeContext context;
            context.mUnit.mOwner = targetKey;
            context.mUnit.mRevisionPlugin = "acceptance";
            context.mSelf = targetKey;
            context.mInstance = targetKey;
            context.mEvent = "acceptance";
            context.mSequence = ++mSequence;
            const auto result = call(event.mWords[2],
                ObScript::Value(ObScript::ReferenceValue{ targetKey, event.mWords[1] }), arguments, context, {});
            trace("acceptance-command actor=" + targetKey.serialize() + " name=" + event.mWords[2]
                + " result=" + ObScript::valueString(result));
        }
        else
            throw std::runtime_error("Invalid M7 scheduled event command " + kind);
        trace("acceptance-event command=" + kind);
    }

    void OblivionScriptManager::writeRuntimeReport() const
    {
        if (mReportPath.empty())
            return;
        // Assemble the report before replacing the prior snapshot. Acceptance tooling may read this path while a
        // scheduled event is capturing live world state, and must never observe a transient zero-byte document.
        std::ostringstream report;
        report << "{\"schema_version\":1,\"compiled_units\":" << mCompiledUnits
            << ",\"compilation_failures\":" << mCompilationFailures << ",\"event_sequence\":" << mSequence
            << ",\"diagnostics\":[";
        for (std::size_t i = 0; i < mDiagnostics.size(); ++i)
        {
            if (i) report << ',';
            report << "{\"code\":\"" << jsonEscape(mDiagnostics[i].mCode) << "\",\"message\":\""
                << jsonEscape(mDiagnostics[i].mMessage) << "\",\"command\":\""
                << jsonEscape(mDiagnostics[i].mCommand) << "\",\"unit\":\""
                << jsonEscape(mDiagnostics[i].mUnit.serialize()) << "\",\"event\":\""
                << jsonEscape(mDiagnostics[i].mEvent) << "\",\"sequence\":" << mDiagnostics[i].mSequence << '}';
        }
        report << "],\"command_counts\":{";
        std::size_t index = 0;
        for (const auto& [name, count] : mCommandCounts)
            report << (index++ ? "," : "") << '"' << jsonEscape(name) << "\":" << count;
        report << "},\"trace\":[";
        for (std::size_t i = 0; i < mTrace.size(); ++i)
            report << (i ? "," : "") << '"' << jsonEscape(mTrace[i]) << '"';
        report << "],\"runtime_state\":" << mWorld.captureOblivionRuntimeState().canonicalJson() << '}';

        const std::filesystem::path temporary = mReportPath.string() + ".tmp";
        {
            std::ofstream out(temporary, std::ios::trunc);
            if (!out)
                return;
            out << report.str();
        }
        std::error_code error;
        std::filesystem::rename(temporary, mReportPath, error);
        if (error)
            Log(Debug::Error) << "Failed to replace Oblivion runtime report " << mReportPath << ": "
                              << error.message();
    }
}
