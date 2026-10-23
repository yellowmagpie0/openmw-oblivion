#ifndef OPENMW_COMPONENTS_ESM4_DIALOGUEVOICES_H
#define OPENMW_COMPONENTS_ESM4_DIALOGUEVOICES_H

#include <algorithm>
#include <functional>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>

#include "loadinfo.hpp"
#include "loadqust.hpp"
#include "loadrace.hpp"

namespace ESM4
{
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
        for (const DialogInfo* info : infos)
        {
            if (info == nullptr || !accept(*info))
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
        std::span<const std::string> files, const std::string& race, const std::string& sex)
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
        }
        return result;
    }
}

#endif
