#ifndef OPENMW_MWWORLD_OBLIVIONSKILLTRAINING_H
#define OPENMW_MWWORLD_OBLIVIONSKILLTRAINING_H
namespace ESM4 { struct RuntimeState; }
namespace MWWorld
{
    class ESMStore;
    class Ptr;
    void validateOblivionAwardedSkillBooks(const ESMStore& store, const ESM4::RuntimeState& state);
    float oblivionPlayerSkillProgressRequirement(const ESMStore& store, const Ptr& player, unsigned skill);
}
#endif
