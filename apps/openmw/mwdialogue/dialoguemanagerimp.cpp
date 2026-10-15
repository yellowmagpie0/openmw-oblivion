#include "dialoguemanagerimp.hpp"
#include <stdexcept>
#include <type_traits>

#include <algorithm>
#include <list>
#include <limits>
#include <optional>
#include <sstream>

#include <components/debug/debuglog.hpp>

#include <components/esm3/dialoguestate.hpp>
#include <components/esm3/esmwriter.hpp>
#include <components/esm3/loaddial.hpp>
#include <components/esm3/loadfact.hpp>
#include <components/esm3/loadinfo.hpp>
#include <components/esm3/loadmgef.hpp>

#include <components/compiler/errorhandler.hpp>
#include <components/compiler/exception.hpp>
#include <components/compiler/locals.hpp>
#include <components/compiler/output.hpp>
#include <components/compiler/scanner.hpp>
#include <components/compiler/scriptparser.hpp>

#include <components/interpreter/defines.hpp>
#include <components/interpreter/interpreter.hpp>

#include <components/misc/resourcehelpers.hpp>

#include <components/settings/values.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/journal.hpp"
#include "../mwbase/luamanager.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/scriptmanager.hpp"
#include "../mwbase/soundmanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwworld/class.hpp"
#include "../mwworld/containerstore.hpp"
#include "../mwworld/esmstore.hpp"

#include "../mwscript/compilercontext.hpp"
#include "../mwscript/extensions.hpp"
#include "../mwscript/interpretercontext.hpp"

#include "../mwmechanics/actorutil.hpp"
#include "../mwmechanics/creaturestats.hpp"
#include "../mwmechanics/npcstats.hpp"

#include "filter.hpp"
#include "keywordsearch.hpp"

namespace MWDialogue
{
    namespace
    {
        struct RuntimeCall
        {
            unsigned& mDepth;
            explicit RuntimeCall(unsigned& depth) : mDepth(depth) { ++mDepth; }
            ~RuntimeCall() { --mDepth; }
        };
    }

    struct DialogueManager::RetainedRuntime::Data
    {
        DialogueManager* mOwner;
        decltype(DialogueManager::mKnownTopics) mKnownTopics{};
        decltype(DialogueManager::mChangedFactionReaction) mChangedFactionReaction{};
        decltype(DialogueManager::mRestoreLifetime) mRestoreLifetime{};
        decltype(DialogueManager::mRestoreEpoch) mRestoreEpoch{};
        decltype(DialogueManager::mActorKnownTopics) mActorKnownTopics{};
        decltype(DialogueManager::mKeywordSearchInitialized) mKeywordSearchInitialized{};
        decltype(DialogueManager::mKeywordSearch) mKeywordSearch{};
        decltype(DialogueManager::mActor) mActor{};
        decltype(DialogueManager::mTalkedTo) mTalkedTo{};
        decltype(DialogueManager::mChoice) mChoice{};
        decltype(DialogueManager::mLastTopic) mLastTopic{};
        decltype(DialogueManager::mIsInChoice) mIsInChoice{};
        decltype(DialogueManager::mGoodbye) mGoodbye{};
        decltype(DialogueManager::mChoices) mChoices{};
        decltype(DialogueManager::mOriginalDisposition) mOriginalDisposition{};
        decltype(DialogueManager::mCurrentDisposition) mCurrentDisposition{};
        decltype(DialogueManager::mPermanentDispositionChange) mPermanentDispositionChange{};
        bool mDetached = false;
        bool mOriginalSelected = false;
        explicit Data(DialogueManager& owner) : mOwner(&owner) { mRestoreLifetime = std::make_shared<const char>(); mRestoreEpoch = owner.mRestoreEpoch; mChoice = -1; }
        void swap() noexcept
        {
            using std::swap;
            static_assert(std::is_nothrow_swappable_v<decltype(mKnownTopics)>);
            swap(mOwner->mKnownTopics, mKnownTopics);
            static_assert(std::is_nothrow_swappable_v<decltype(mChangedFactionReaction)>);
            swap(mOwner->mChangedFactionReaction, mChangedFactionReaction);
            static_assert(std::is_nothrow_swappable_v<decltype(mRestoreLifetime)>);
            swap(mOwner->mRestoreLifetime, mRestoreLifetime);
            static_assert(std::is_nothrow_swappable_v<decltype(mRestoreEpoch)>);
            swap(mOwner->mRestoreEpoch, mRestoreEpoch);
            static_assert(std::is_nothrow_swappable_v<decltype(mActorKnownTopics)>);
            swap(mOwner->mActorKnownTopics, mActorKnownTopics);
            static_assert(std::is_nothrow_swappable_v<decltype(mKeywordSearchInitialized)>);
            swap(mOwner->mKeywordSearchInitialized, mKeywordSearchInitialized);
            static_assert(std::is_nothrow_swappable_v<decltype(mKeywordSearch)>);
            swap(mOwner->mKeywordSearch, mKeywordSearch);
            static_assert(std::is_nothrow_swappable_v<decltype(mActor)>);
            swap(mOwner->mActor, mActor);
            static_assert(std::is_nothrow_swappable_v<decltype(mTalkedTo)>);
            swap(mOwner->mTalkedTo, mTalkedTo);
            static_assert(std::is_nothrow_swappable_v<decltype(mChoice)>);
            swap(mOwner->mChoice, mChoice);
            static_assert(std::is_nothrow_swappable_v<decltype(mLastTopic)>);
            swap(mOwner->mLastTopic, mLastTopic);
            static_assert(std::is_nothrow_swappable_v<decltype(mIsInChoice)>);
            swap(mOwner->mIsInChoice, mIsInChoice);
            static_assert(std::is_nothrow_swappable_v<decltype(mGoodbye)>);
            swap(mOwner->mGoodbye, mGoodbye);
            static_assert(std::is_nothrow_swappable_v<decltype(mChoices)>);
            swap(mOwner->mChoices, mChoices);
            static_assert(std::is_nothrow_swappable_v<decltype(mOriginalDisposition)>);
            swap(mOwner->mOriginalDisposition, mOriginalDisposition);
            static_assert(std::is_nothrow_swappable_v<decltype(mCurrentDisposition)>);
            swap(mOwner->mCurrentDisposition, mCurrentDisposition);
            static_assert(std::is_nothrow_swappable_v<decltype(mPermanentDispositionChange)>);
            swap(mOwner->mPermanentDispositionChange, mPermanentDispositionChange);
        }
    };
    void DialogueManager::requireMutableRuntime() const
    {
        if (mRetainedRuntime && !mRetainedRuntime->mDetached)
            throw std::logic_error("Outgoing DialogueManager runtime is frozen before detachment");
    }
    DialogueManager::RetainedRuntime::RetainedRuntime(std::unique_ptr<Data> data) : mData(std::move(data)) {}
    DialogueManager::RetainedRuntime::~RetainedRuntime()
    {
        if (mData && mData->mOwner && !restore()) std::terminate();
    }
    DialogueManager::RetainedRuntime::RetainedRuntime(RetainedRuntime&&) noexcept = default;
    DialogueManager::RetainedRuntime& DialogueManager::RetainedRuntime::operator=(RetainedRuntime&& other) noexcept
    {
        if (this != &other)
        {
            if (mData && mData->mOwner && !restore()) std::terminate();
            mData = std::move(other.mData);
        }
        return *this;
    }
    DialogueManager::RetainedRuntime DialogueManager::retainRuntime()
    {
        if (mRetainedRuntime || mActiveCalls) throw std::logic_error("DialogueManager retention requires an idle runtime");
        auto data = std::make_unique<RetainedRuntime::Data>(*this);
        mRetainedRuntime = data.get();
        return RetainedRuntime(std::move(data));
    }
    bool DialogueManager::RetainedRuntime::detach() noexcept
    {
        if (!mData || !mData->mOwner || mData->mDetached || mData->mOwner->mActiveCalls) return false;

        mData->swap(); mData->mDetached = true;
        return true;
    }
    bool DialogueManager::RetainedRuntime::exchange() noexcept
    {
        if (!mData || !mData->mOwner || !mData->mDetached || mData->mOwner->mActiveCalls) return false;

        mData->swap(); mData->mOriginalSelected = !mData->mOriginalSelected;
        return true;
    }
    bool DialogueManager::RetainedRuntime::restore() noexcept
    {
        if (!mData || !mData->mOwner || mData->mOwner->mActiveCalls) return false;

        if (mData->mDetached && !mData->mOriginalSelected) mData->swap();
        mData->mOwner->mRetainedRuntime = nullptr;
        mData->mOwner = nullptr;
        mData.reset();
        return true;
    }
    void DialogueManager::RetainedRuntime::discard() noexcept
    {
        if (!mData || !mData->mOwner) return;
        if (!mData->mDetached) { restore(); return; }
        if (mData->mOriginalSelected && !exchange()) std::terminate();

        mData->mOwner->mRetainedRuntime = nullptr;
        mData->mOwner = nullptr;
        mData.reset();
    }
    DialogueManager::~DialogueManager()
    {
        if (mRetainedRuntime)
        {
            mRetainedRuntime->mRestoreLifetime.reset();
            mRetainedRuntime->mOwner = nullptr;
        }
    }


    DialogueManager::DialogueManager(
        const Compiler::Extensions& extensions, Translation::Storage& translationDataStorage)
        : mTranslationDataStorage(translationDataStorage)
        , mCompilerContext(MWScript::CompilerContext::Type_Dialogue)
        , mErrorHandler()
        , mTalkedTo(false)
        , mOriginalDisposition(0)
        , mCurrentDisposition(0)
        , mPermanentDispositionChange(0)
    {
        mChoice = -1;
        mIsInChoice = false;
        mGoodbye = false;
        mCompilerContext.setExtensions(&extensions);
    }

    void DialogueManager::clear()
    {
        requireMutableRuntime();
        ++mRestoreEpoch;
        mKnownTopics.clear();
        mTalkedTo = false;
        mOriginalDisposition = 0;
        mCurrentDisposition = 0;
        mPermanentDispositionChange = 0;
        mKeywordSearch.clear();
        mKeywordSearchInitialized = false;
    }

    void DialogueManager::addTopic(const ESM::RefId& topic)
    {
        requireMutableRuntime();
        mKnownTopics.insert(topic);
    }

    const MWDialogue::KeywordSearch& DialogueManager::getKeywordSearch() const
    {
        const auto& dialogue = MWBase::Environment::get().getESMStore()->get<ESM::Dialogue>();
        if (dialogue.getKeywordSearchModFlag() || !mKeywordSearchInitialized)
        {
            requireMutableRuntime();
            mKeywordSearch.clear();

            for (const ESM::Dialogue& topic : dialogue)
                mKeywordSearch.seed(mTranslationDataStorage.topicKeyword(topic.mStringId), topic.mStringId);

            mKeywordSearchInitialized = true;
        }

        return mKeywordSearch;
    }

    std::vector<ESM::RefId> DialogueManager::parseTopicIdsFromText(const std::string& text) const
    {
        std::vector<ESM::RefId> topicIdList;

        std::vector<KeywordSearch::Match> matches = getKeywordSearch().parseHyperText(text, mTranslationDataStorage);

        for (const auto& match : matches)
            topicIdList.push_back(ESM::RefId::stringRefId(match.mTopicId));

        return topicIdList;
    }

    void DialogueManager::addTopicsFromText(const std::string& text)
    {
        updateActorKnownTopics();

        for (const auto& topicId : parseTopicIdsFromText(text))
        {
            if (mActorKnownTopics.count(topicId))
                mKnownTopics.insert(topicId);
        }
    }

    void DialogueManager::updateOriginalDisposition()
    {
        requireMutableRuntime();
        if (mActor.getClass().isNpc())
        {
            const auto& stats = mActor.getClass().getNpcStats(mActor);
            // Disposition changed by script; discard our preconceived notions
            if (stats.getBaseDisposition() != mCurrentDisposition)
            {
                mCurrentDisposition = stats.getBaseDisposition();
                mOriginalDisposition = mCurrentDisposition;
            }
        }
    }

    bool DialogueManager::startDialogue(const MWWorld::Ptr& actor, ResponseCallback* callback)
    {
        requireMutableRuntime();
        const RuntimeCall call(mActiveCalls);
        updateGlobals();

        // Dialogue with dead actor (e.g. through script) should not be allowed.
        if (actor.getClass().getCreatureStats(actor).isDead())
            return false;

        mLastTopic = ESM::RefId();
        // Note that we intentionally don't reset mPermanentDispositionChange

        mChoice = -1;
        mIsInChoice = false;
        mGoodbye = false;
        mChoices.clear();

        mActor = actor;

        MWMechanics::CreatureStats& creatureStats = actor.getClass().getCreatureStats(actor);
        mTalkedTo = creatureStats.hasTalkedToPlayer();

        mActorKnownTopics.clear();

        // greeting
        const MWWorld::Store<ESM::Dialogue>& dialogs = MWBase::Environment::get().getESMStore()->get<ESM::Dialogue>();

        Filter filter(actor, mChoice, mTalkedTo);

        for (const ESM::Dialogue& dialogue : dialogs)
        {
            if (dialogue.mType == ESM::Dialogue::Greeting)
            {
                // Search a response (we do not accept a fallback to "Info refusal" here)
                if (const ESM::DialInfo* info = filter.search(dialogue, false).second)
                {
                    creatureStats.talkedToPlayer();

                    if (!info->mSound.empty())
                    {
                        // TODO play sound
                    }

                    MWScript::InterpreterContext interpreterContext(&mActor.getRefData().getLocals(), mActor);
                    callback->addResponse({}, Interpreter::fixDefinesDialog(info->mResponse, interpreterContext));
                    MWBase::Environment::get().getLuaManager()->onDialogueResponse(mActor, *info, dialogue);
                    executeScript(info->mResultScript, mActor);
                    mLastTopic = dialogue.mId;

                    addTopicsFromText(info->mResponse);

                    return true;
                }
            }
        }
        return false;
    }

    std::optional<Interpreter::Program> DialogueManager::compile(const std::string& cmd, const MWWorld::Ptr& actor)
    {
        requireMutableRuntime();
        const RuntimeCall call(mActiveCalls);
        bool success = true;
        std::optional<Interpreter::Program> program;

        try
        {
            mErrorHandler.reset();

            mErrorHandler.setContext("[dialogue script]");

            std::istringstream input(cmd + "\n");

            Compiler::Scanner scanner(mErrorHandler, input, mCompilerContext.getExtensions());

            Compiler::Locals locals;

            const ESM::RefId& actorScript = actor.getClass().getScript(actor);

            if (!actorScript.empty())
            {
                // grab local variables from actor's script, if available.
                locals = MWBase::Environment::get().getScriptManager()->getLocals(actorScript);
            }

            Compiler::ScriptParser parser(mErrorHandler, mCompilerContext, locals, false);

            scanner.scan(parser);

            if (!mErrorHandler.isGood())
                success = false;

            if (success)
                program = parser.getProgram();
        }
        catch (const Compiler::SourceException& /* error */)
        {
            // error has already been reported via error handler
            success = false;
        }
        catch (const std::exception& error)
        {
            Log(Debug::Error) << std::string("Dialogue error: An exception has been thrown: ") + error.what();
            success = false;
        }

        if (!success)
        {
            Log(Debug::Error) << "Error: compiling failed (dialogue script): \n" << cmd << "\n";
        }

        return program;
    }

    void DialogueManager::executeScript(const std::string& script, const MWWorld::Ptr& actor)
    {
        requireMutableRuntime();
        const RuntimeCall call(mActiveCalls);
        if (const std::optional<Interpreter::Program> program = compile(script, actor))
        {
            try
            {
                MWScript::InterpreterContext interpreterContext(&actor.getRefData().getLocals(), actor);
                Interpreter::Interpreter interpreter;
                MWScript::installOpcodes(interpreter);
                interpreter.run(*program, interpreterContext);
            }
            catch (const std::exception& error)
            {
                Log(Debug::Error) << std::string("Dialogue error: An exception has been thrown: ") + error.what();
            }
        }
    }

    bool DialogueManager::inJournal(const ESM::RefId& topicId, const ESM::RefId& infoId) const
    {
        MWBase::Journal* journal = MWBase::Environment::get().getJournal();
        const auto topic = journal->getTopics().find(topicId);
        if (topic != journal->getTopics().end())
        {
            return std::ranges::find_if(topic->second, [&](const MWDialogue::Entry& entry) {
                return entry.mInfoId == infoId;
            }) != topic->second.end();
        }
        return false;
    }

    void DialogueManager::executeTopic(const ESM::RefId& topic, ResponseCallback* callback)
    {
        requireMutableRuntime();
        const RuntimeCall call(mActiveCalls);
        Filter filter(mActor, mChoice, mTalkedTo);

        const MWWorld::Store<ESM::Dialogue>& dialogues = MWBase::Environment::get().getESMStore()->get<ESM::Dialogue>();

        const ESM::Dialogue& dialogue = *dialogues.find(topic);

        const auto [responseTopic, info] = filter.search(dialogue, true);

        if (info)
        {
            std::string_view title;
            if (dialogue.mType == ESM::Dialogue::Persuasion)
            {
                // Determine GMST from dialogue topic. GMSTs are:
                // sAdmireSuccess, sAdmireFail, sIntimidateSuccess, sIntimidateFail,
                // sTauntSuccess, sTauntFail, sBribeSuccess, sBribeFail
                std::string modifiedTopic = "s" + topic.getRefIdString();

                modifiedTopic.erase(std::remove(modifiedTopic.begin(), modifiedTopic.end(), ' '), modifiedTopic.end());

                const MWWorld::Store<ESM::GameSetting>& gmsts
                    = MWBase::Environment::get().getESMStore()->get<ESM::GameSetting>();

                title = gmsts.find(modifiedTopic)->mValue.getString();
            }
            else
                title = dialogue.mStringId;

            MWScript::InterpreterContext interpreterContext(&mActor.getRefData().getLocals(), mActor);
            callback->addResponse(title, Interpreter::fixDefinesDialog(info->mResponse, interpreterContext));
            MWBase::Environment::get().getLuaManager()->onDialogueResponse(mActor, *info, dialogue);

            if (dialogue.mType == ESM::Dialogue::Topic)
            {
                // Make sure the returned DialInfo is from the Dialogue we supplied. If could also be from the Info
                // refusal group, in which case it should not be added to the journal.
                if (responseTopic == &dialogue)
                    MWBase::Environment::get().getJournal()->addTopic(topic, info->mId, mActor);
            }

            mLastTopic = topic;

            executeScript(info->mResultScript, mActor);

            addTopicsFromText(info->mResponse);
        }
    }

    const ESM::Dialogue* DialogueManager::searchDialogue(const ESM::RefId& id)
    {
        return MWBase::Environment::get().getESMStore()->get<ESM::Dialogue>().search(id);
    }

    void DialogueManager::updateGlobals()
    {
        MWBase::Environment::get().getWorld()->updateDialogueGlobals();
    }

    void DialogueManager::updateActorKnownTopics()
    {
        requireMutableRuntime();
        updateGlobals();

        mActorKnownTopics.clear();

        const auto& dialogs = MWBase::Environment::get().getESMStore()->get<ESM::Dialogue>();

        Filter filter(mActor, -1, mTalkedTo);

        for (const auto& dialog : dialogs)
        {
            if (dialog.mType == ESM::Dialogue::Topic)
            {
                const auto* answer = filter.search(dialog, true).second;
                const auto& topicId = dialog.mId;

                if (answer != nullptr)
                {
                    int topicFlags = 0;
                    if (!inJournal(topicId, answer->mId))
                    {
                        // Does this dialogue contains some actor-specific answer?
                        if (answer->mActor == mActor.getCellRef().getRefId())
                            topicFlags |= MWBase::DialogueManager::TopicType::Specific;
                    }
                    else
                        topicFlags |= MWBase::DialogueManager::TopicType::Exhausted;
                    mActorKnownTopics.insert(std::make_pair(dialog.mId, ActorKnownTopicInfo{ topicFlags, answer }));
                }
            }
        }

        // If response to a topic leads to a new topic, the original topic is not exhausted.

        for (auto& [dialogId, topicInfo] : mActorKnownTopics)
        {
            // If the topic is not marked as exhausted, we don't need to do anything about it.
            // If the topic will not be shown to the player, the flag actually does not matter.

            if (!(topicInfo.mFlags & MWBase::DialogueManager::TopicType::Exhausted) || !mKnownTopics.count(dialogId))
                continue;

            for (const auto& topicId : parseTopicIdsFromText(topicInfo.mInfo->mResponse))
            {
                if (mActorKnownTopics.count(topicId) && !mKnownTopics.count(topicId))
                {
                    topicInfo.mFlags &= ~MWBase::DialogueManager::TopicType::Exhausted;
                    break;
                }
            }
        }
    }

    std::list<std::string> DialogueManager::getAvailableTopics()
    {
        requireMutableRuntime();
        updateActorKnownTopics();

        std::list<std::string> keywordList;
        const auto& store = MWBase::Environment::get().getESMStore()->get<ESM::Dialogue>();
        for (const auto& [topic, topicInfo] : mActorKnownTopics)
        {
            // does the player know the topic?
            if (mKnownTopics.contains(topic))
                keywordList.push_back(store.find(topic)->mStringId);
        }

        // sort again, because the previous sort was case-sensitive
        keywordList.sort(Misc::StringUtils::ciLess);
        return keywordList;
    }

    int DialogueManager::getTopicFlag(const ESM::RefId& topicId) const
    {
        auto known = mActorKnownTopics.find(topicId);
        if (known != mActorKnownTopics.end())
            return known->second.mFlags;
        return 0;
    }

    void DialogueManager::keywordSelected(std::string_view keyword, ResponseCallback* callback)
    {
        requireMutableRuntime();
        const RuntimeCall call(mActiveCalls);
        if (!mIsInChoice)
        {
            const ESM::Dialogue* dialogue = searchDialogue(ESM::RefId::stringRefId(keyword));
            if (dialogue && dialogue->mType == ESM::Dialogue::Topic)
            {
                executeTopic(dialogue->mId, callback);
            }
        }
    }

    bool DialogueManager::isInChoice() const
    {
        return mIsInChoice;
    }

    void DialogueManager::goodbyeSelected()
    {
        requireMutableRuntime();
        const RuntimeCall call(mActiveCalls);
        // Apply disposition change to NPC's base disposition if we **think** we need to change something
        if ((mPermanentDispositionChange || mOriginalDisposition != mCurrentDisposition) && mActor.getClass().isNpc())
        {
            updateOriginalDisposition();
            MWMechanics::NpcStats& npcStats = mActor.getClass().getNpcStats(mActor);

            // Get the sum of disposition effects minus charm (shouldn't be made permanent)
            npcStats.setBaseDisposition(0);
            int zero = static_cast<int>(
                MWBase::Environment::get().getMechanicsManager()->getDerivedDisposition(mActor, false)
                - npcStats.getMagicEffects().getOrDefault(ESM::MagicEffect::Charm).getMagnitude());

            // Clamp new permanent disposition to avoid negative derived disposition (can be caused by intimidate)
            int disposition = std::clamp(mOriginalDisposition + mPermanentDispositionChange, -zero, 100 - zero);
            npcStats.setBaseDisposition(disposition);
        }
        mPermanentDispositionChange = 0;
        mOriginalDisposition = 0;
        mCurrentDisposition = 0;
    }

    void DialogueManager::questionAnswered(int answer, ResponseCallback* callback)
    {
        requireMutableRuntime();
        const RuntimeCall call(mActiveCalls);
        mChoice = answer;

        const ESM::Dialogue* dialogue = searchDialogue(mLastTopic);
        if (dialogue)
        {
            Filter filter(mActor, mChoice, mTalkedTo);

            if (dialogue->mType == ESM::Dialogue::Topic || dialogue->mType == ESM::Dialogue::Greeting)
            {
                const auto [responseTopic, info] = filter.search(*dialogue, true);
                if (info)
                {
                    const std::string& text = info->mResponse;
                    addTopicsFromText(text);

                    mChoice = -1;
                    mIsInChoice = false;
                    mChoices.clear();

                    MWScript::InterpreterContext interpreterContext(&mActor.getRefData().getLocals(), mActor);
                    callback->addResponse({}, Interpreter::fixDefinesDialog(text, interpreterContext));
                    MWBase::Environment::get().getLuaManager()->onDialogueResponse(mActor, *info, *dialogue);

                    if (dialogue->mType == ESM::Dialogue::Topic)
                    {
                        // Make sure the returned DialInfo is from the Dialogue we supplied. If could also be from the
                        // Info refusal group, in which case it should not be added to the journal
                        if (responseTopic == dialogue)
                            MWBase::Environment::get().getJournal()->addTopic(mLastTopic, info->mId, mActor);
                    }

                    executeScript(info->mResultScript, mActor);
                }
                else
                {
                    mChoice = -1;
                    mIsInChoice = false;
                    mChoices.clear();
                }
            }
        }

        updateActorKnownTopics();
    }

    void DialogueManager::addChoice(std::string_view text, int choice)
    {
        requireMutableRuntime();
        mIsInChoice = true;
        mChoices.emplace_back(text, choice);
    }

    const std::vector<std::pair<std::string, int>>& DialogueManager::getChoices() const
    {
        return mChoices;
    }

    bool DialogueManager::isGoodbye() const
    {
        return mGoodbye;
    }

    void DialogueManager::goodbye()
    {
        requireMutableRuntime();
        mIsInChoice = false;
        mGoodbye = true;
    }

    void DialogueManager::persuade(int type, ResponseCallback* callback)
    {
        bool success;
        int temp, perm;
        MWBase::Environment::get().getMechanicsManager()->getPersuasionDispositionChange(
            mActor, MWBase::MechanicsManager::PersuasionType(type), success, temp, perm);
        updateOriginalDisposition();
        if (temp > 0 && perm > 0 && mOriginalDisposition + perm + mPermanentDispositionChange < 0)
            perm = -(mOriginalDisposition + mPermanentDispositionChange);
        mCurrentDisposition += temp;
        mActor.getClass().getNpcStats(mActor).setBaseDisposition(mCurrentDisposition);
        mPermanentDispositionChange += perm;

        MWWorld::Ptr player = MWMechanics::getPlayer();
        player.getClass().skillUsageSucceeded(
            player, ESM::Skill::Speechcraft, success ? ESM::Skill::Speechcraft_Success : ESM::Skill::Speechcraft_Fail);

        if (success)
        {
            int gold = 0;
            if (type == MWBase::MechanicsManager::PT_Bribe10)
                gold = 10;
            else if (type == MWBase::MechanicsManager::PT_Bribe100)
                gold = 100;
            else if (type == MWBase::MechanicsManager::PT_Bribe1000)
                gold = 1000;

            if (gold)
            {
                player.getClass().getContainerStore(player).remove(MWWorld::ContainerStore::sGoldId, gold);
                mActor.getClass().getContainerStore(mActor).add(MWWorld::ContainerStore::sGoldId, gold);
            }
        }

        std::string text;

        if (type == MWBase::MechanicsManager::PT_Admire)
            text = "Admire";
        else if (type == MWBase::MechanicsManager::PT_Taunt)
            text = "Taunt";
        else if (type == MWBase::MechanicsManager::PT_Intimidate)
            text = "Intimidate";
        else
        {
            text = "Bribe";
        }

        executeTopic(ESM::RefId::stringRefId(text + (success ? " Success" : " Fail")), callback);
    }

    void DialogueManager::applyBarterDispositionChange(int delta)
    {
        if (!mActor.isEmpty() && mActor.getClass().isNpc())
        {
            updateOriginalDisposition();
            mCurrentDisposition += delta;
            mActor.getClass().getNpcStats(mActor).setBaseDisposition(mCurrentDisposition);
            if (Settings::game().mBarterDispositionChangeIsPermanent)
                mPermanentDispositionChange += delta;
        }
    }

    bool DialogueManager::checkServiceRefused(ResponseCallback* callback, ServiceType service)
    {
        Filter filter(mActor, service, mTalkedTo);

        const MWWorld::Store<ESM::Dialogue>& dialogues = MWBase::Environment::get().getESMStore()->get<ESM::Dialogue>();

        const ESM::Dialogue& dialogue = *dialogues.find(ESM::RefId::stringRefId("Service Refusal"));

        std::vector<Filter::Response> infos = filter.list(dialogue, false, false, true);
        if (!infos.empty())
        {
            const ESM::DialInfo* info = infos[0].second;

            addTopicsFromText(info->mResponse);

            const MWWorld::Store<ESM::GameSetting>& gmsts
                = MWBase::Environment::get().getESMStore()->get<ESM::GameSetting>();

            MWScript::InterpreterContext interpreterContext(&mActor.getRefData().getLocals(), mActor);

            callback->addResponse(gmsts.find("sServiceRefusal")->mValue.getString(),
                Interpreter::fixDefinesDialog(info->mResponse, interpreterContext));
            MWBase::Environment::get().getLuaManager()->onDialogueResponse(mActor, *info, dialogue);

            executeScript(info->mResultScript, mActor);
            return true;
        }
        return false;
    }

    bool DialogueManager::say(const MWWorld::Ptr& actor, const ESM::RefId& topic)
    {
        MWBase::SoundManager* sndMgr = MWBase::Environment::get().getSoundManager();
        if (sndMgr->sayActive(actor))
        {
            // Actor is already saying something.
            return false;
        }

        if (actor.getClass().isNpc() && MWBase::Environment::get().getWorld()->isSwimming(actor))
        {
            // NPCs don't talk while submerged
            return false;
        }

        if (actor.getClass().getCreatureStats(actor).getKnockedDown())
        {
            // Unconscious actors can not speak
            return false;
        }

        const MWWorld::ESMStore& store = *MWBase::Environment::get().getESMStore();
        const ESM::Dialogue* dial = store.get<ESM::Dialogue>().find(topic);

        const MWMechanics::CreatureStats& creatureStats = actor.getClass().getCreatureStats(actor);
        Filter filter(actor, 0, creatureStats.hasTalkedToPlayer());
        const ESM::DialInfo* info = filter.search(*dial, false).second;
        if (info != nullptr)
        {
            MWBase::WindowManager* winMgr = MWBase::Environment::get().getWindowManager();
            if (Settings::gui().mSubtitles)
                winMgr->messageBox(info->mResponse);
            if (!info->mSound.empty())
                sndMgr->say(actor, Misc::ResourceHelpers::correctSoundPath(VFS::Path::Normalized(info->mSound)));
            if (!info->mResultScript.empty())
                executeScript(info->mResultScript, actor);
            MWBase::Environment::get().getLuaManager()->onDialogueResponse(actor, *info, *dial);
        }
        return info != nullptr;
    }

    size_t DialogueManager::countSavedGameRecords() const
    {
        return 1; // known topics
    }

    void DialogueManager::write(ESM::ESMWriter& writer, Loading::Listener& progress) const
    {
        ESM::DialogueState state;

        state.mKnownTopics.reserve(mKnownTopics.size());
        std::copy(mKnownTopics.begin(), mKnownTopics.end(), std::back_inserter(state.mKnownTopics));

        state.mChangedFactionReaction = mChangedFactionReaction;

        writer.startRecord(ESM::REC_DIAS);
        state.save(writer);
        writer.endRecord(ESM::REC_DIAS);
    }

    void DialogueManager::readRecord(ESM::ESMReader& reader, uint32_t type)
    {
        requireMutableRuntime();
        if (type == ESM::REC_DIAS)
        {
            const MWWorld::ESMStore& store = *MWBase::Environment::get().getESMStore();

            ESM::DialogueState state;
            state.load(reader);

            for (const auto& knownTopic : state.mKnownTopics)
                if (store.get<ESM::Dialogue>().search(knownTopic))
                    mKnownTopics.insert(knownTopic);

            mChangedFactionReaction = state.mChangedFactionReaction;
        }
    }

    std::function<bool()> DialogueManager::prepareDialogueState(
        ESM::DialogueState&& state, const MWWorld::ESMStore& incoming)
    {
        if (mRestoreEpoch == std::numeric_limits<std::uint64_t>::max())
            throw std::overflow_error("Dialogue restore epoch exhausted");
        struct Plan
        {
            std::set<ESM::RefId> mTopics;
            ModFactionReactionMap mReactions;
            std::weak_ptr<const char> mLifetime;
            std::uint64_t mEpoch;
            bool mUsed = false;
        };
        auto plan = std::make_shared<Plan>();
        const auto& store = *MWBase::Environment::get().getESMStore();
        // This specialized store contains content dialogues only; saved
        // dynamic definitions do not include DIAL records.
        for (const auto& topic : state.mKnownTopics)
            if (store.get<ESM::Dialogue>().search(topic)) plan->mTopics.insert(topic);
        plan->mReactions = std::move(state.mChangedFactionReaction);
        plan->mLifetime = mRestoreLifetime;
        plan->mEpoch = mRestoreEpoch + 1;
        return [this, plan] {
            if (plan->mUsed || plan->mLifetime.expired() || mRestoreEpoch != plan->mEpoch) return false;
            plan->mUsed = true;
            mKnownTopics.merge(plan->mTopics);
            mChangedFactionReaction.swap(plan->mReactions);
            return true;
        };
    }

    void DialogueManager::modFactionReaction(const ESM::RefId& faction1, const ESM::RefId& faction2, int diff)
    {
        requireMutableRuntime();
        // Make sure the factions exist
        MWBase::Environment::get().getESMStore()->get<ESM::Faction>().find(faction1);
        MWBase::Environment::get().getESMStore()->get<ESM::Faction>().find(faction2);

        int newValue = getFactionReaction(faction1, faction2) + diff;

        auto& map = mChangedFactionReaction[faction1];
        map[faction2] = newValue;
    }

    void DialogueManager::setFactionReaction(const ESM::RefId& faction1, const ESM::RefId& faction2, int absolute)
    {
        requireMutableRuntime();
        // Make sure the factions exist
        MWBase::Environment::get().getESMStore()->get<ESM::Faction>().find(faction1);
        MWBase::Environment::get().getESMStore()->get<ESM::Faction>().find(faction2);

        auto& map = mChangedFactionReaction[faction1];
        map[faction2] = absolute;
    }

    int DialogueManager::getFactionReaction(const ESM::RefId& faction1, const ESM::RefId& faction2) const
    {
        ModFactionReactionMap::const_iterator map = mChangedFactionReaction.find(faction1);
        if (map != mChangedFactionReaction.end())
        {
            auto it = map->second.find(faction2);
            if (it != map->second.end())
                return it->second;
        }

        const ESM::Faction* faction = MWBase::Environment::get().getESMStore()->get<ESM::Faction>().find(faction1);

        auto it = faction->mReactions.begin();
        for (; it != faction->mReactions.end(); ++it)
        {
            if (it->first == faction2)
                return it->second;
        }
        return 0;
    }

    const std::map<ESM::RefId, int>* DialogueManager::getFactionReactionOverrides(const ESM::RefId& faction) const
    {
        // Make sure the faction exists
        MWBase::Environment::get().getESMStore()->get<ESM::Faction>().find(faction);

        const auto found = mChangedFactionReaction.find(faction);
        if (found != mChangedFactionReaction.end())
            return &found->second;
        return nullptr;
    }

    void DialogueManager::clearInfoActor(const MWWorld::Ptr& actor) const
    {
        if (actor == mActor && !mLastTopic.empty())
        {
            MWBase::Environment::get().getJournal()->removeLastAddedTopicResponse(
                mLastTopic, actor.getClass().getName(actor));
        }
    }
}
