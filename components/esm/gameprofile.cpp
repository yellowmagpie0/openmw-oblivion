#include "gameprofile.hpp"

#include <algorithm>
#include <stdexcept>

namespace ESM
{
    GameProfile parseGameProfile(std::string_view value)
    {
        std::string normalized(value);
        std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c) {
            return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : static_cast<char>(c);
        });
        if (normalized == "auto")
            return GameProfile::Auto;
        if (normalized == "morrowind" || normalized == "tes3")
            return GameProfile::Morrowind;
        if (normalized == "oblivion" || normalized == "tes4")
            return GameProfile::Oblivion;
        throw std::invalid_argument("Unknown game profile '" + std::string(value)
            + "'; expected auto, morrowind, or oblivion");
    }

    std::string_view toString(GameProfile value)
    {
        switch (value)
        {
            case GameProfile::Auto:
                return "auto";
            case GameProfile::Morrowind:
                return "morrowind";
            case GameProfile::Oblivion:
                return "oblivion";
        }
        throw std::logic_error("Invalid game profile");
    }

    GameProfile gameProfileForFormat(Format format)
    {
        switch (format)
        {
            case Format::Tes3:
                return GameProfile::Morrowind;
            case Format::Tes4:
                return GameProfile::Oblivion;
        }
        throw std::logic_error("Unsupported content format");
    }

    GameProfile GameProfileSelector::observe(Format format, std::string_view contentFile)
    {
        const GameProfile observed = gameProfileForFormat(format);
        if (mRequested != GameProfile::Auto && mRequested != observed)
            throw std::runtime_error("Game profile '" + std::string(toString(mRequested)) + "' rejects content file '"
                + std::string(contentFile) + "' because it uses the '" + std::string(toString(observed)) + "' format");
        if (mSelected != GameProfile::Auto && mSelected != observed)
            throw std::runtime_error("Content file '" + std::string(contentFile) + "' mixes the '"
                + std::string(toString(observed)) + "' format with the already selected '"
                + std::string(toString(mSelected)) + "' game profile");
        mSelected = observed;
        return mSelected;
    }
}
