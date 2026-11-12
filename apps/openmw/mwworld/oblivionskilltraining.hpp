#ifndef OPENMW_MWWORLD_OBLIVIONSKILLTRAINING_H
#define OPENMW_MWWORLD_OBLIVIONSKILLTRAINING_H
namespace MWWorld
{
    class ESMStore;
    class Ptr;
    float oblivionPlayerSkillProgressRequirement(const ESMStore& store, const Ptr& player, unsigned skill);
}
#endif
