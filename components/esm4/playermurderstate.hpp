#ifndef OPENMW_COMPONENTS_ESM4_PLAYERMURDERSTATE_H
#define OPENMW_COMPONENTS_ESM4_PLAYERMURDERSTATE_H

#include <cstdint>
#include <stdexcept>
#include <string_view>

namespace ESM4
{
    inline constexpr std::string_view PlayerMurdererKey = "crime.murderer";

    inline bool playerMurdererValue(double value)
    {
        if (value != 0.0 && value != 1.0)
            throw std::invalid_argument("Native Player murderer flag requires zero or one");
        return value != 0.0;
    }

    struct PlayerMurderState
    {
        bool mMurderer = false;
        std::uint32_t mMurders = 0;
    };

    inline PlayerMurderState recordPlayerMurder(PlayerMurderState state) noexcept
    {
        // Original6608F0 sets Player+611 and increments misc-stat32 at+6D8.
        state.mMurderer = true;
        ++state.mMurders;
        return state;
    }
}

#endif
