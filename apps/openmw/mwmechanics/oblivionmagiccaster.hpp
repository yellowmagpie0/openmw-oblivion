#ifndef OPENMW_MWMECHANICS_OBLIVIONMAGICCASTER_H
#define OPENMW_MWMECHANICS_OBLIVIONMAGICCASTER_H

namespace MWBase { class World; }
namespace MWWorld { class Ptr; }

namespace MWMechanics
{
    // Original self-Spell caster virtual+2C(false,0), using current authority
    // and actually worn native armor. NPC/Creature and Player god mode return
    // one. This query changes no actor state or RNG. Finite negative authored
    // results are preserved; effect admission owns its supported power domain.
    float oblivionCombatSpellPower(MWBase::World& world, const MWWorld::Ptr& actor);
}

#endif
