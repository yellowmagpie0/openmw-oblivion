#ifndef OPENMW_COMPONENTS_ESM_GAMEPROFILE_H
#define OPENMW_COMPONENTS_ESM_GAMEPROFILE_H

#include <span>
#include <string>
#include <string_view>

#include "format.hpp"

namespace ESM
{
    enum class GameProfile
    {
        Auto,
        Morrowind,
        Oblivion,
    };

    GameProfile parseGameProfile(std::string_view value);
    std::string_view toString(GameProfile value);
    GameProfile gameProfileForFormat(Format format);
    std::span<const std::string_view> getDefaultArchives(GameProfile profile);

    class GameProfileSelector
    {
    public:
        explicit GameProfileSelector(GameProfile requested = GameProfile::Auto)
            : mRequested(requested)
        {
        }

        GameProfile observe(Format format, std::string_view contentFile);
        GameProfile selected() const { return mSelected; }
        GameProfile requested() const { return mRequested; }

    private:
        GameProfile mRequested = GameProfile::Auto;
        GameProfile mSelected = GameProfile::Auto;
    };
}

#endif
