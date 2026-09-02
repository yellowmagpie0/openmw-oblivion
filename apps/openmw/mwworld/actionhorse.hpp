/*
  Copyright (C) 2026 OpenMW contributors

  This file is part of OpenMW.

  OpenMW is free software: you can redistribute it and/or modify it under the
  terms of the GNU General Public License version 3, as published by the
  Free Software Foundation.
*/
#ifndef GAME_MWWORLD_ACTIONHORSE_H
#define GAME_MWWORLD_ACTIONHORSE_H

#include "action.hpp"

namespace MWWorld
{
    class ActionHorse final : public Action
    {
        void executeImp(const Ptr& actor) override;

    public:
        explicit ActionHorse(const Ptr& horse);
    };
}

#endif
