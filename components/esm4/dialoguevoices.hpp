#ifndef OPENMW_COMPONENTS_ESM4_DIALOGUEVOICES_H
#define OPENMW_COMPONENTS_ESM4_DIALOGUEVOICES_H

#include <algorithm>
#include <functional>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <type_traits>

#include "loadinfo.hpp"
#include "loadqust.hpp"
#include "loadrace.hpp"

namespace ESM4
{
    // Original530830 rejects these entries before visiting any condition.
    inline bool dialogueInfoHistoryEligible(const DialogInfo& info, bool hasQuest, bool hasSpoken)
    {
        return !(info.mFlags & 0x20) && hasQuest && (!(info.mInfoFlags & INFO_SayOnce) || !hasSpoken);
    }

    // Response6B7C30 suppresses the result callback for RunImmediately. The
    // direct531470 core has no such flag gate; keep these phases distinct.
    inline bool dialogueResponseRunsResult(std::uint16_t flags, bool actorPresent)
    {
        return !(flags & INFO_RunImmediately) && actorPresent;
    }

    // Read this AFTER the result VM. Neither a missing script nor a failed VM
    // suppresses history, but InfoRefusal does (original531470).
    inline bool dialogueResultMarksSpoken(std::uint16_t flags)
    {
        return !(flags & INFO_InfoRefusal);
    }

    // Original530830 starts with quest eligibility and appends INFO predicates
    // to the same list traversal. A final quest OR flag joins the first INFO.
    inline std::vector<PackageCondition> dialogueInfoConditions(const Quest& quest, const DialogInfo& info)
    {
        auto conditions = quest.mDialogueConditions;
        conditions.insert(conditions.end(), info.mCanonicalConditions.begin(), info.mCanonicalConditions.end());
        return conditions;
    }

    // Original52F010, after topic/quest/link/history admission. Conditions are
    // lazy: a matching RandomEnd or nonrandom entry ends traversal. Candidate
    // insertion446CB0 prepends; modulo selection therefore reverses acceptance
    // order. The owner supplies either a staged or an immediately consumed raw
    // CRT draw. Missing voice files do not change the selected INFO or draw.
    template <class Accept, class Draw>
    const DialogInfo* selectDialogueInfo(std::span<const DialogInfo* const> infos,
        Accept&& accept, Draw&& draw)
    {
        std::vector<const DialogInfo*> candidates;
        for (std::size_t index = 0; index < infos.size(); ++index)
        {
            const DialogInfo* info = infos[index];
            if (info == nullptr)
                continue;
            // The same INFO can occur in multiple native quest arrays after
            // content overrides. Its eligibility uses this visit's quest.
            const bool accepted = [&] {
                if constexpr (std::is_invocable_v<Accept, const DialogInfo&, std::size_t>)
                    return accept(*info, index);
                else
                    return accept(*info);
            }();
            if (!accepted)
                continue;
            if ((info->mInfoFlags & INFO_Random) == 0)
            {
                if (candidates.empty())
                    return info;
                break;
            }
            candidates.push_back(info);
            if (info->mInfoFlags & INFO_RandomEnd)
                break;
        }
        if (candidates.empty())
            return nullptr;
        const unsigned raw = draw();
        if (raw > 32767)
            throw std::invalid_argument("invalid native dialogue random draw");
        return candidates[candidates.size() - 1 - raw % candidates.size()];
    }

    inline const Race* dialogueVoiceRace(const Race* race, bool female,
        const std::function<const Race*(ESM::FormId)>& resolve)
    {
        std::set<ESM::FormId> visited;
        while (race != nullptr)
        {
            if (!visited.insert(race->mId).second)
                return nullptr;
            const auto next = race->mVNAM[female ? 1 : 0];
            if (next.isZeroOrUnset())
                return race;
            race = resolve(next);
        }
        return nullptr;
    }

    // Resolve the entire selected INFO before playing or committing results.
    // Never substitute a different INFO, race, sex, or response number.
    inline std::optional<std::vector<std::string>> dialogueVoiceFiles(const DialogInfo& info,
        std::span<const std::string> files, const std::string& race, const std::string& sex, bool firstResponseOnly = false)
    {
        if (race.empty() || sex.empty() || info.mResponses.empty())
            return std::nullopt;
        std::vector<std::string> result;
        for (const auto& response : info.mResponses)
        {
            const std::string suffix = "_" + std::to_string(response.mData.responseNo & 0xff) + ".mp3";
            const auto path = std::find_if(files.begin(), files.end(), [&](const auto& file) {
                return file.ends_with(suffix) && file.find("/" + race + "/" + sex + "/") != std::string::npos;
            });
            if (path == files.end())
                return std::nullopt;
            result.push_back(*path);
            if (firstResponseOnly) break;
        }
        return result;
    }
}

#endif
