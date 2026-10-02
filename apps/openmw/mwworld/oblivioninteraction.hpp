#ifndef GAME_MWWORLD_OBLIVIONINTERACTION_H
#define GAME_MWWORLD_OBLIVIONINTERACTION_H

#include "action.hpp"

#include <cstdint>

namespace MWWorld
{
    class ContainerStore;

    // Native script count over physical stacks, including signed int32 wrap.
    std::int32_t oblivionInventoryItemCount(const ContainerStore& inventory, const ESM::RefId& item);

    enum class OblivionInteractionKind
    {
        Activator,
        Actor,
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
