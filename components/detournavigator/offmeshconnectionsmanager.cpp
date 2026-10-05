#include "offmeshconnectionsmanager.hpp"
#include "objectid.hpp"
#include "offmeshconnection.hpp"
#include "settings.hpp"
#include "settingsutils.hpp"
#include "tileposition.hpp"

#include <algorithm>
#include <set>
#include <vector>

namespace DetourNavigator
{
    OffMeshConnectionsManager::OffMeshConnectionsManager(const RecastSettings& settings)
        : mSettings(settings)
    {
    }

    void OffMeshConnectionsManager::add(const ObjectId id, const OffMeshConnection& value)
    {
        const auto values = mValues.lock();

        values->mById.insert(std::make_pair(id, value));

        const auto startTilePosition = getTilePosition(mSettings, value.mStart);
        const auto endTilePosition = getTilePosition(mSettings, value.mEnd);

        values->mByTilePosition[startTilePosition].insert(id);

        if (startTilePosition != endTilePosition)
            values->mByTilePosition[endTilePosition].insert(id);
    }

    struct OffMeshConnectionsManager::PreparedRemoval::Data
    {
        Misc::Locked<Values> mValues;
        ObjectId mId;
        std::set<TilePosition> mTiles;
        bool mCommitted = false;

        Data(Misc::Locked<Values> values, ObjectId id)
            : mValues(std::move(values)), mId(id) {}
    };

    OffMeshConnectionsManager::PreparedRemoval::PreparedRemoval(std::unique_ptr<Data> data)
        : mData(std::move(data)) {}
    OffMeshConnectionsManager::PreparedRemoval::~PreparedRemoval() = default;

    const std::set<TilePosition>& OffMeshConnectionsManager::PreparedRemoval::changedTiles() const
    {
        return mData->mTiles;
    }

    bool OffMeshConnectionsManager::PreparedRemoval::isValid() const
    {
        return !mData->mCommitted;
    }

    bool OffMeshConnectionsManager::PreparedRemoval::commit()
    {
        if (!isValid())
            return false;
        auto& values = *mData->mValues;
        for (const TilePosition& tile : mData->mTiles)
        {
            const auto it = values.mByTilePosition.find(tile);
            if (it == values.mByTilePosition.end())
                continue;
            it->second.erase(mData->mId);
            if (it->second.empty())
                values.mByTilePosition.erase(it);
        }
        const auto range = values.mById.equal_range(mData->mId);
        values.mById.erase(range.first, range.second);
        mData->mCommitted = true;
        return true;
    }

    std::unique_ptr<OffMeshConnectionsManager::PreparedRemoval>
    OffMeshConnectionsManager::prepareRemoval(ObjectId id)
    {
        auto data = std::make_unique<PreparedRemoval::Data>(mValues.lock(), id);
        const auto range = data->mValues->mById.equal_range(id);
        for (auto it = range.first; it != range.second; ++it)
        {
            data->mTiles.emplace(getTilePosition(mSettings, it->second.mStart));
            data->mTiles.emplace(getTilePosition(mSettings, it->second.mEnd));
        }
        return std::unique_ptr<PreparedRemoval>(new PreparedRemoval(std::move(data)));
    }

    std::set<TilePosition> OffMeshConnectionsManager::remove(const ObjectId id)
    {
        auto plan = prepareRemoval(id);
        plan->commit();
        return std::move(plan->mData->mTiles);
    }

    std::vector<OffMeshConnection> OffMeshConnectionsManager::get(const TilePosition& tilePosition) const
    {
        std::vector<OffMeshConnection> result;

        const auto values = mValues.lockConst();

        const auto itByTilePosition = values->mByTilePosition.find(tilePosition);

        if (itByTilePosition == values->mByTilePosition.end())
            return result;

        std::for_each(itByTilePosition->second.begin(), itByTilePosition->second.end(), [&](const ObjectId id) {
            const auto byId = values->mById.equal_range(id);
            std::for_each(byId.first, byId.second, [&](const auto& v) {
                if (getTilePosition(mSettings, v.second.mStart) == tilePosition
                    || getTilePosition(mSettings, v.second.mEnd) == tilePosition)
                    result.push_back(v.second);
            });
        });

        std::sort(result.begin(), result.end());

        return result;
    }
}
