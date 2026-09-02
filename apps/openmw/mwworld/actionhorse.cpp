/*
  Copyright (C) 2026 OpenMW contributors

  This file is part of OpenMW.

  OpenMW is free software: you can redistribute it and/or modify it under the
  terms of the GNU General Public License version 3, as published by the
  Free Software Foundation.
*/
#include "actionhorse.hpp"

#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"
#include "../mwmechanics/oblivionai.hpp"
#include "worldimp.hpp"

namespace MWWorld
{
    ActionHorse::ActionHorse(const Ptr& horse)
        : Action(false, horse)
    {
    }

    void ActionHorse::executeImp(const Ptr& actor)
    {
        auto* world = dynamic_cast<World*>(static_cast<MWBase::World*>(MWBase::Environment::get().getWorld()));
        if (world == nullptr)
            return;
        MWMechanics::OblivionAiService* const ai = world->getOblivionAiService();
        if (ai == nullptr || !ai->isHorse(getTarget()))
            return;
        const ESM::FormKey horse = getTarget().getCellRef().getFormKey();
        const ESM4::RuntimeActorAiState* state = ai->state(actor);
        if (state != nullptr && state->mMount == horse)
            ai->dismountHorse(actor);
        else
            ai->mountHorse(actor, getTarget());
    }
}
