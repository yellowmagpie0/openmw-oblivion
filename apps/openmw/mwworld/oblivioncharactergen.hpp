#ifndef OPENMW_MWWORLD_OBLIVIONCHARACTERGEN_HPP
#define OPENMW_MWWORLD_OBLIVIONCHARACTERGEN_HPP
#include <components/esm/refid.hpp>
#include <components/esm4/charactergenstate.hpp>
#include <optional>
namespace MWWorld
{
    class World;
    class Ptr;
    void setOblivionCharacterGenerationTracking(World& world, bool enabled);
    unsigned oblivionClassDefaultMatch(World& world);
    std::optional<ESM4::CharacterGenerationState> prepareOblivionCharacterGenerationSkillUse(
        World& world, unsigned nativeSkill, int usage, float factor);
    void recordOblivionCharacterGenerationSkillUse(const Ptr& actor, ESM::RefId skill, int usage, float factor);
}
#endif
