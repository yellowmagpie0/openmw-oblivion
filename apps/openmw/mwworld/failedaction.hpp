#ifndef GAME_MWWORLD_FAILEDACTION_H
#define GAME_MWWORLD_FAILEDACTION_H

#include "action.hpp"
#include "ptr.hpp"

#include <string>
#include <string_view>

namespace MWWorld
{
    class FailedAction : public Action
    {
        // Activation can return an action built from a temporary error message.
        std::string mMessage;

        void executeImp(const Ptr& actor) override;

    public:
        FailedAction(std::string_view message = {}, const Ptr& target = Ptr());
    };
}

#endif
