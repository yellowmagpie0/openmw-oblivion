#ifndef GAME_MWWORLD_OBLIVIONINTERACTION_H
#define GAME_MWWORLD_OBLIVIONINTERACTION_H

#include "action.hpp"

namespace MWWorld
{
    enum class OblivionInteractionKind
    {
        Activator,
        Book,
        Container,
        Door,
        Flora,
        Take,
    };

    // Executes the deliberately small, native-TES4 interaction vocabulary used
    // by the first prison slice. Later inventory and script milestones can
    // replace individual verbs without coupling them to TES3 ContainerStore.
    class OblivionInteractionAction final : public Action
    {
    public:
        OblivionInteractionAction(const Ptr& target, OblivionInteractionKind kind);

    private:
        void executeImp(const Ptr& actor) override;

        OblivionInteractionKind mKind;
    };
}

#endif
