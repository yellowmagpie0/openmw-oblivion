#include "actioneat.hpp"

#include <components/esm3/loadskil.hpp>

#include "../mwmechanics/actorutil.hpp"

#include "class.hpp"
#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"

namespace MWWorld
{
    void ActionEat::executeImp(const Ptr& actor)
    {
        if (actor.getClass().consume(getTarget(), actor) && actor == MWMechanics::getPlayer()
            && MWBase::Environment::get().getWorld()->getGameProfile() != ESM::GameProfile::Oblivion)
            actor.getClass().skillUsageSucceeded(actor, ESM::Skill::Alchemy, ESM::Skill::Alchemy_UseIngredient);
    }

    ActionEat::ActionEat(const MWWorld::Ptr& object)
        : Action(false, object)
    {
    }
}
