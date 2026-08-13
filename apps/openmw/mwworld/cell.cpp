#include "cell.hpp"

#include "esmstore.hpp"
#include "worldspaceutils.hpp"

#include "../mwbase/environment.hpp"

#include <components/esm3/loadcell.hpp>
#include <components/esm4/loadcell.hpp>
#include <components/esm4/loadwrld.hpp>
#include <components/misc/algorithm.hpp>

#include <stdexcept>
#include <string>

namespace MWWorld
{
    namespace
    {
        std::string getDescription(const ESM4::World& value)
        {
            if (!value.mEditorId.empty())
                return value.mEditorId;

            return ESM::RefId(value.mId).serializeText();
        }

        std::string getCellDescription(const ESM4::Cell& cell, const ESM4::World* world)
        {
            std::string result;

            if (!cell.mEditorId.empty())
                result = cell.mEditorId;
            else if (world != nullptr && cell.isExterior())
                result = getDescription(*world);
            else
                result = cell.mId.serializeText();

            if (cell.isExterior())
                result += " (" + std::to_string(cell.mX) + ", " + std::to_string(cell.mY) + ")";

            return result;
        }
    }

    Cell::Cell(const ESM4::Cell& cell)
        : ESM::CellVariant(cell)
        , mIsExterior(!(cell.mCellFlags & ESM4::CELL_Interior))
        , mIsQuasiExterior(cell.mCellFlags & ESM4::CELL_QuasiExt)
        , mHasWater(cell.mCellFlags & ESM4::CELL_HasWater)
        , mNoSleep(false) // No such notion in ESM4
        , mGridPos(cell.mX, cell.mY)
        , mDisplayname(cell.mFullName)
        , mNameID(cell.mEditorId)
        , mRegion(cell.mRegions.empty() ? ESM::RefId() : ESM::RefId(cell.mRegions.front()))
        , mId(cell.mId)
        , mParent(cell.mParent)
        , mWaterHeight(cell.mWaterHeight)
        , mMood{
            .mAmbiantColor = cell.mLighting.ambient,
            .mDirectionalColor = cell.mLighting.directional,
            .mFogColor = cell.mLighting.fogColor,
            .mFogDensity = cell.mLighting.fogFar > 0.f
                ? std::clamp(1.f - cell.mLighting.fogNear / cell.mLighting.fogFar, 0.f, 1.f)
                : 1.f,
            .mFogNear = cell.mLighting.fogNear,
            .mFogFar = cell.mLighting.fogFar,
        }
    {
        const ESM4::World* world = MWBase::Environment::get().getESMStore()->get<ESM4::World>().search(mParent);
        if (isExterior())
        {
            if (world == nullptr)
                throw std::runtime_error(
                    "Cell " + cell.mId.toDebugString() + " parent world " + mParent.toDebugString() + " is not found");
            const auto& worlds = MWBase::Environment::get().getESMStore()->get<ESM4::World>();
            if (const ESM4::World* waterWorld
                = getWorldspaceFeature(worlds, mParent, ESM4::World::UseFlag_Water))
                mWaterHeight = waterWorld->mWaterLevel;
        }
        mDescription = getCellDescription(cell, world);
    }

    Cell::Cell(const ESM::Cell& cell)
        : ESM::CellVariant(cell)
        , mIsExterior(!(cell.mData.mFlags & ESM::Cell::Interior))
        , mIsQuasiExterior(cell.mData.mFlags & ESM::Cell::QuasiEx)
        , mHasWater(cell.mData.mFlags & ESM::Cell::HasWater)
        , mNoSleep(cell.mData.mFlags & ESM::Cell::NoSleep)
        , mGridPos(cell.getGridX(), cell.getGridY())
        , mDisplayname(cell.mName)
        , mNameID(cell.mName)
        , mRegion(cell.mRegion)
        , mId(cell.mId)
        , mParent(ESM::Cell::sDefaultWorldspaceId)
        , mWaterHeight(cell.mWater)
        , mDescription(cell.getDescription())
        , mMood{
            .mAmbiantColor = cell.mAmbi.mAmbient,
            .mDirectionalColor = cell.mAmbi.mSunlight,
            .mFogColor = cell.mAmbi.mFog,
            .mFogDensity = cell.mAmbi.mFogDensity,
            .mFogNear = 0.f,
            .mFogFar = 0.f,
        }
    {
        if (isExterior())
        {
            mWaterHeight = -1.f;
            mHasWater = true;
        }
        else
            mGridPos = {};
    }

    ESM::RefId Cell::getClimate() const
    {
        if (!isEsm4())
            return {};
        const ESM4::Cell& cell = getEsm4();
        if (!cell.mClimate.isZeroOrUnset())
            return ESM::RefId(cell.mClimate);
        if (!isExterior())
            return {};
        const auto& worlds = MWBase::Environment::get().getESMStore()->get<ESM4::World>();
        const ESM4::World* world = getWorldspaceFeature(worlds, mParent, ESM4::World::UseFlag_Climate);
        return world == nullptr || world->mClimate.isZeroOrUnset() ? ESM::RefId() : ESM::RefId(world->mClimate);
    }

    ESM::RefId Cell::getWaterType() const
    {
        if (!isEsm4())
            return {};
        const ESM4::Cell& cell = getEsm4();
        if (!cell.mWater.isZeroOrUnset())
            return ESM::RefId(cell.mWater);
        if (!isExterior())
            return {};
        const auto& worlds = MWBase::Environment::get().getESMStore()->get<ESM4::World>();
        const ESM4::World* world = getWorldspaceFeature(worlds, mParent, ESM4::World::UseFlag_Water);
        return world == nullptr || world->mWater.isZeroOrUnset() ? ESM::RefId() : ESM::RefId(world->mWater);
    }
}
