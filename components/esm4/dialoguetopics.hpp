#ifndef OPENMW_COMPONENTS_ESM4_DIALOGUETOPICS_H
#define OPENMW_COMPONENTS_ESM4_DIALOGUETOPICS_H

#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>
#include <vector>

#include <components/esm/formkey.hpp>

namespace ESM4
{
    struct DialogueQuestGroup
    {
        ESM::FormKey mQuest;
        std::vector<ESM::FormKey> mInfos;
        friend bool operator==(const DialogueQuestGroup&, const DialogueQuestGroup&) = default;
    };

    // Content-load authority corresponding to original52FD20/52FC40. Replay
    // revisions in file order, including overrides. Deletion leaves the old
    // membership: lookup rejects the current deleted definition separately.
    class DialogueTopicIndex
    {
        std::map<ESM::FormKey, std::vector<DialogueQuestGroup>> mTopics;
        std::map<ESM::FormKey, std::uint16_t> mOrdinals;

        std::uint16_t ordinal(const ESM::FormKey& info) const
        {
            const auto it = mOrdinals.find(info);
            return it == mOrdinals.end() ? 0xffff : it->second;
        }

    public:
        const std::vector<DialogueQuestGroup>* find(const ESM::FormKey& topic) const
        {
            const auto it = mTopics.find(topic);
            return it == mTopics.end() ? nullptr : &it->second;
        }

        // A missing field is append-if-new; a present null key is front
        // insertion. Resolve a nonzero PNAM before calling this method.
        bool insert(const ESM::FormKey& topic, const ESM::FormKey& quest,
            const ESM::FormKey& info, const std::optional<ESM::FormKey>& previous)
        {
            if (topic.isNull() || quest.isNull() || info.isNull())
                return false;
            auto& groups = mTopics[topic];
            auto group = std::find_if(groups.begin(), groups.end(),
                [&](const auto& entry) { return entry.mQuest == quest; });
            if (group == groups.end())
                group = groups.insert(groups.end(), DialogueQuestGroup{quest, {}});
            auto& infos = group->mInfos;
            const auto old = ordinal(info);
            std::optional<std::size_t> position;
            if (!previous)
            {
                if (old == 0xffff)
                    position = infos.size();
            }
            else if (previous->isNull())
                position = 0;
            else
            {
                // The original clears the stored slot before finding PNAM,
                // but compacts only after calculating the insertion position.
                if (old != 0xffff)
                {
                    if (infos.size() <= old)
                        infos.resize(std::size_t(old) + 1);
                    infos[old] = {};
                }
                bool found = false;
                for (const auto& entry : groups)
                {
                    if (std::find(entry.mInfos.rbegin(), entry.mInfos.rend(), *previous) != entry.mInfos.rend())
                    {
                        found = true;
                        break;
                    }
                }
                if (!found)
                    position = 0; // Native diagnostic, then front insertion.
                else if (const auto index = ordinal(*previous); index != 0xffff)
                    position = std::size_t(index) + 1;
                else
                {
                    const auto it = std::find(infos.begin(), infos.end(), *previous);
                    if (it == infos.end())
                        throw std::runtime_error("Unindexed dialogue predecessor belongs to another quest");
                    position = std::size_t(it - infos.begin()) + 1;
                }
            }
            std::erase_if(infos, [](const auto& key) { return key.isNull(); });
            if (position)
                infos.insert(infos.begin() + std::min(*position, infos.size()), info);
            for (std::size_t i = 0; i < infos.size(); ++i)
                mOrdinals[infos[i]] = static_cast<std::uint16_t>(i);
            return true;
        }

        // Original52F330: descending unsigned quest priority, stable on ties;
        // an unresolved quest compares as -1. Preserve the source group order.
        template <class Priority>
        std::vector<DialogueQuestGroup> ordered(const ESM::FormKey& topic, Priority&& priority) const
        {
            const auto* groups = find(topic);
            if (!groups)
                return {};
            auto result = *groups;
            std::stable_sort(result.begin(), result.end(), [&](const auto& a, const auto& b) {
                return priority(a.mQuest) > priority(b.mQuest);
            });
            return result;
        }
    };
}

#endif
