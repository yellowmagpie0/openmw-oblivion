#include "landmanager.hpp"

#include <components/esm4/loadwrld.hpp>
#include <components/resource/objectcache.hpp>
#include <components/settings/values.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/worldspaceutils.hpp"

namespace MWRender
{

    LandManager::LandManager(int loadFlags)
        : GenericResourceManager<ESM::ExteriorCellLocation>(nullptr, Settings::cells().mCacheExpiryDelay)
        , mLoadFlags(loadFlags)
    {
    }

    osg::ref_ptr<ESMTerrain::LandObject> LandManager::getLand(ESM::ExteriorCellLocation cellIndex)
    {
        const MWBase::World& world = *MWBase::Environment::get().getWorld();
        if (ESM::isEsm4Ext(cellIndex.mWorldspace))
        {
            const auto& worlds = world.getStore().get<ESM4::World>();
            const ESM4::World* worldspace = worlds.find(cellIndex.mWorldspace);
            if (worldspace->mWorldFlags & ESM4::World::WLD_NoLandscpe)
                return nullptr;
            cellIndex.mWorldspace
                = MWWorld::resolveWorldspaceInheritance(worlds, cellIndex.mWorldspace, ESM4::World::UseFlag_Land);
        }

        if (const std::optional<osg::ref_ptr<osg::Object>> obj = mCache->getRefFromObjectCacheOrNone(cellIndex))
            return static_cast<ESMTerrain::LandObject*>(obj->get());

        osg::ref_ptr<ESMTerrain::LandObject> landObj = nullptr;

        if (ESM::isEsm4Ext(cellIndex.mWorldspace))
        {
            const ESM4::Land* land = world.getStore().get<ESM4::Land>().search(cellIndex);
            if (land != nullptr)
                landObj = new ESMTerrain::LandObject(*land, mLoadFlags);
        }
        else
        {
            const ESM::Land* land = world.getStore().get<ESM::Land>().search(cellIndex.mX, cellIndex.mY);
            if (land != nullptr)
                landObj = new ESMTerrain::LandObject(*land, mLoadFlags);
        }

        mCache->addEntryToObjectCache(cellIndex, landObj.get());
        return landObj;
    }

    void LandManager::reportStats(unsigned int frameNumber, osg::Stats* stats) const
    {
        Resource::reportStats("Land", frameNumber, mCache->getStats(), *stats);
    }

}
