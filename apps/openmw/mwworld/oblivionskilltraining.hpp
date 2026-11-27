#ifndef OPENMW_MWWORLD_OBLIVIONSKILLTRAINING_H
#define OPENMW_MWWORLD_OBLIVIONSKILLTRAINING_H
#include <components/esm/attr.hpp>
#include <span>
namespace ESM4 { struct RuntimeState; }
namespace MWWorld
{
    class World;
    class ESMStore;
    class Ptr;
    void validateOblivionAwardedSkillBooks(const ESMStore& store, const ESM4::RuntimeState& state);
    float oblivionPlayerSkillProgressRequirement(const ESMStore& store, const Ptr& player, unsigned skill);
    bool advanceOblivionPlayerLevel(World& world, std::span<const ESM::Attribute::AttributeID> choices);
}
#endif
