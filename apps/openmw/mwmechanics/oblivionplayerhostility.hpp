#ifndef OPENMW_MWMECHANICS_OBLIVIONPLAYERHOSTILITY_H
#define OPENMW_MWMECHANICS_OBLIVIONPLAYERHOSTILITY_H

#include <cstdint>
#include <optional>

namespace MWWorld { class World; class Ptr; }

namespace MWMechanics
{
    // Shared read-only native fight policy for Player encounters and authored
    // force greetings. A missing disposition/process is not a friendly result.
    std::optional<std::int32_t> oblivionPlayerFightScore(
        MWWorld::World& world, const MWWorld::Ptr& actor);
}

#endif
