#include "journalimp.hpp"
#include <stdexcept>
#include <type_traits>

#include <iterator>

#include <components/esm3/esmreader.hpp>
#include <components/esm3/esmwriter.hpp>
#include <components/esm3/journalentry.hpp>
#include <components/esm3/queststate.hpp>

#include <components/misc/strings/algorithm.hpp>

#include "../mwworld/class.hpp"
#include "../mwworld/esmstore.hpp"

#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

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

    struct Journal::RetainedRuntime::Data
    {
        Journal* mOwner;
        decltype(Journal::mJournal) mJournal{};
        decltype(Journal::mQuests) mQuests{};
        decltype(Journal::mTopics) mTopics{};
        bool mDetached = false;
        bool mOriginalSelected = false;
        explicit Data(Journal& owner) : mOwner(&owner) {}
        void swap() noexcept
        {
            using std::swap;
            static_assert(std::is_nothrow_swappable_v<decltype(mJournal)>);
            swap(mOwner->mJournal, mJournal);
            static_assert(std::is_nothrow_swappable_v<decltype(mQuests)>);
            swap(mOwner->mQuests, mQuests);
            static_assert(std::is_nothrow_swappable_v<decltype(mTopics)>);
            swap(mOwner->mTopics, mTopics);
        }
    };
    void Journal::requireMutableRuntime() const
    {
        if (mRetainedRuntime && !mRetainedRuntime->mDetached)
            throw std::logic_error("Outgoing Journal runtime is frozen before detachment");
    }
    Journal::RetainedRuntime::RetainedRuntime(std::unique_ptr<Data> data) : mData(std::move(data)) {}
    Journal::RetainedRuntime::~RetainedRuntime()
    {
        if (mData && mData->mOwner && !restore()) std::terminate();
    }
    Journal::RetainedRuntime::RetainedRuntime(RetainedRuntime&&) noexcept = default;
    Journal::RetainedRuntime& Journal::RetainedRuntime::operator=(RetainedRuntime&& other) noexcept
    {
        if (this != &other)
        {
            if (mData && mData->mOwner && !restore()) std::terminate();
            mData = std::move(other.mData);
        }
        return *this;
    }
    Journal::RetainedRuntime Journal::retainRuntime()
    {
        if (mRetainedRuntime || mActiveCalls) throw std::logic_error("Journal retention requires an idle runtime");
        auto data = std::make_unique<RetainedRuntime::Data>(*this);
        mRetainedRuntime = data.get();
        return RetainedRuntime(std::move(data));
    }
    bool Journal::RetainedRuntime::detach() noexcept
    {
        if (!mData || !mData->mOwner || mData->mDetached || mData->mOwner->mActiveCalls) return false;

        mData->swap(); mData->mDetached = true;
        return true;
    }
    bool Journal::RetainedRuntime::exchange() noexcept
    {
        if (!mData || !mData->mOwner || !mData->mDetached || mData->mOwner->mActiveCalls) return false;

        mData->swap(); mData->mOriginalSelected = !mData->mOriginalSelected;
        return true;
    }
    bool Journal::RetainedRuntime::restore() noexcept
    {
        if (!mData || !mData->mOwner || mData->mOwner->mActiveCalls) return false;

        if (mData->mDetached && !mData->mOriginalSelected) mData->swap();
        mData->mOwner->mRetainedRuntime = nullptr;
        mData->mOwner = nullptr;
        mData.reset();
        return true;
    }
    void Journal::RetainedRuntime::discard() noexcept
    {
        if (!mData || !mData->mOwner) return;
        if (!mData->mDetached) { restore(); return; }
        if (mData->mOriginalSelected && !exchange()) std::terminate();

        mData->mOwner->mRetainedRuntime = nullptr;
        mData->mOwner = nullptr;
        mData.reset();
    }
    Journal::~Journal()
    {
        if (mRetainedRuntime) {  mRetainedRuntime->mOwner = nullptr; }
    }


    Quest& Journal::getOrStartQuest(const ESM::RefId& id)
    {
        requireMutableRuntime();
        TQuestContainer::iterator iter = mQuests.find(id);

        if (iter == mQuests.end())
            iter = mQuests.emplace(id, Quest(id)).first;

        return iter->second;
    }

    Quest* Journal::getQuestOrNull(const ESM::RefId& id)
    {
        requireMutableRuntime();
        TQuestContainer::iterator iter = mQuests.find(id);
        if (iter == mQuests.end())
        {
            return nullptr;
        }

        return &(iter->second);
    }

    Topic& Journal::getTopic(const ESM::RefId& id)
    {
        requireMutableRuntime();
        TTopicContainer::iterator iter = mTopics.find(id);

        if (iter == mTopics.end())
        {
            std::pair<TTopicContainer::iterator, bool> result = mTopics.insert(std::make_pair(id, Topic(id)));

            iter = result.first;
        }

        return iter->second;
    }

    bool Journal::isThere(const ESM::RefId& topicId, const ESM::RefId& infoId) const
    {
        if (const ESM::Dialogue* dialogue
            = MWBase::Environment::get().getESMStore()->get<ESM::Dialogue>().search(topicId))
        {
            if (infoId.empty())
                return true;

            for (const ESM::DialInfo& info : dialogue->mInfo)
                if (info.mId == infoId)
                    return true;
        }

        return false;
    }

    Journal::Journal() {}

    void Journal::clear()
    {
        requireMutableRuntime();
        mJournal.clear();
        mQuests.clear();
        mTopics.clear();
    }

    void Journal::addEntry(const ESM::RefId& id, int index, const MWWorld::Ptr& actor)
    {
        requireMutableRuntime();
        const RuntimeCall call(mActiveCalls);
        // bail out if we already have heard this...
        const ESM::RefId& infoId = JournalEntry::idFromIndex(id, index);
        for (const JournalEntry& entry : mJournal)
            if (entry.mTopic == id && entry.mInfoId == infoId)
            {
                if (getJournalIndex(id) < index)
                {
                    setJournalIndex(id, index);
                    MWBase::Environment::get().getWindowManager()->messageBox("#{sJournalEntry}");
                }
                return;
            }

        StampedJournalEntry entry = StampedJournalEntry::makeFromQuest(id, index, actor);

        Quest& quest = getOrStartQuest(id);
        if (quest.addEntry(entry)) // we are doing slicing on purpose here
        {
            // Restart all "other" quests with the same name as well
            std::string_view name = quest.getName();
            for (auto& it : mQuests)
            {
                if (it.second.isFinished() && Misc::StringUtils::ciEqual(it.second.getName(), name))
                    it.second.setFinished(false);
            }
        }

        // there is no need to show empty entries in journal
        if (!entry.getText().empty())
        {
            mJournal.push_back(std::move(entry));
            MWBase::Environment::get().getWindowManager()->messageBox("#{sJournalEntry}");
        }
    }

    void Journal::setJournalIndex(const ESM::RefId& id, int index)
    {
        requireMutableRuntime();
        Quest& quest = getOrStartQuest(id);

        quest.setIndex(index);
    }

    void Journal::addTopic(const ESM::RefId& topicId, const ESM::RefId& infoId, const MWWorld::Ptr& actor)
    {
        requireMutableRuntime();
        const RuntimeCall call(mActiveCalls);
        Topic& topic = getTopic(topicId);

        JournalEntry entry(topicId, infoId, actor);
        entry.mActorName = actor.getClass().getName(actor);
        topic.addEntry(entry);
    }

    void Journal::removeLastAddedTopicResponse(const ESM::RefId& topicId, std::string_view actorName)
    {
        requireMutableRuntime();
        Topic& topic = getTopic(topicId);

        topic.removeLastAddedResponse(actorName);

        if (topic.begin() == topic.end())
            mTopics.erase(mTopics.find(topicId)); // All responses removed -> remove topic
    }

    int Journal::getJournalIndex(const ESM::RefId& id) const
    {
        TQuestContainer::const_iterator iter = mQuests.find(id);

        if (iter == mQuests.end())
            return 0;

        return iter->second.getIndex();
    }

    size_t Journal::countSavedGameRecords() const
    {
        std::size_t count = mQuests.size();

        for (const auto& [_, quest] : mQuests)
            count += quest.size();

        count += mJournal.size();

        for (const auto& [_, topic] : mTopics)
            count += topic.size();

        return count;
    }

    void Journal::write(ESM::ESMWriter& writer, Loading::Listener& progress) const
    {
        for (const auto& [_, quest] : mQuests)
        {
            ESM::QuestState state;
            quest.write(state);
            writer.startRecord(ESM::REC_QUES);
            state.save(writer);
            writer.endRecord(ESM::REC_QUES);

            for (const Entry& questEntry : quest)
            {
                ESM::JournalEntry entry;
                entry.mType = ESM::JournalEntry::Type_Quest;
                entry.mTopic = quest.getTopic();
                questEntry.write(entry);
                writer.startRecord(ESM::REC_JOUR);
                entry.save(writer);
                writer.endRecord(ESM::REC_JOUR);
            }
        }

        for (const StampedJournalEntry& journalEntry : mJournal)
        {
            ESM::JournalEntry entry;
            entry.mType = ESM::JournalEntry::Type_Journal;
            journalEntry.write(entry);
            writer.startRecord(ESM::REC_JOUR);
            entry.save(writer);
            writer.endRecord(ESM::REC_JOUR);
        }

        for (const auto& [_, topic] : mTopics)
        {
            for (const Entry& topicEntry : topic)
            {
                ESM::JournalEntry entry;
                entry.mType = ESM::JournalEntry::Type_Topic;
                entry.mTopic = topic.getTopic();
                topicEntry.write(entry);
                writer.startRecord(ESM::REC_JOUR);
                entry.save(writer);
                writer.endRecord(ESM::REC_JOUR);
            }
        }
    }

    void Journal::readRecord(ESM::ESMReader& reader, uint32_t type)
    {
        requireMutableRuntime();
        const RuntimeCall call(mActiveCalls);
        if (type == ESM::REC_JOUR)
        {
            ESM::JournalEntry record;
            record.load(reader);

            if (isThere(record.mTopic, record.mInfo))
                switch (record.mType)
                {
                    case ESM::JournalEntry::Type_Quest:

                        getOrStartQuest(record.mTopic).insertEntry(record);
                        break;

                    case ESM::JournalEntry::Type_Journal:

                        mJournal.push_back(record);
                        break;

                    case ESM::JournalEntry::Type_Topic:

                        getTopic(record.mTopic).insertEntry(record);
                        break;
                }
        }
        else if (type == ESM::REC_QUES)
        {
            ESM::QuestState record;
            record.load(reader);

            if (isThere(record.mTopic))
            {
                std::pair<TQuestContainer::iterator, bool> result
                    = mQuests.insert(std::make_pair(record.mTopic, record));
                // reapply quest index, this is to handle users upgrading from only
                // Morrowind.esm (no quest states) to Morrowind.esm + Tribunal.esm
                result.first->second.setIndex(record.mState);
            }
        }
    }
}
