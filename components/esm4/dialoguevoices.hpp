#ifndef OPENMW_COMPONENTS_ESM4_DIALOGUEVOICES_H
#define OPENMW_COMPONENTS_ESM4_DIALOGUEVOICES_H

#include <algorithm>
#include <functional>
#include <optional>
#include <set>
#include <span>

#include "loadinfo.hpp"
#include "loadrace.hpp"

namespace ESM4
{
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
