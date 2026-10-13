#include "creaturestate.hpp"

namespace ESM
{

    void CreatureState::load(ESMReader& esm)
    {
        ObjectState::load(esm);

        if (mHasCustomState)
        {
            mInventory.load(esm);

            mCreatureStats.load(esm);
        }
        else
        {
            mInventory = InventoryState{};
            mCreatureStats = CreatureStats{};
            mCreatureStats.blank();
        }
    }

    void CreatureState::save(ESMWriter& esm, bool inInventory) const
    {
        ObjectState::save(esm, inInventory);

        if (mHasCustomState)
        {
            mInventory.save(esm);

            mCreatureStats.save(esm);
        }
    }

    void CreatureState::blank()
    {
        ObjectState::blank();
        mCreatureStats.blank();
    }

}
