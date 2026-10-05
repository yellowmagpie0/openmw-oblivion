#ifndef OPENMW_COMPONENTS_DETOURNAVIGATOR_OFFMESHCONNECTIONSMANAGER_H
#define OPENMW_COMPONENTS_DETOURNAVIGATOR_OFFMESHCONNECTIONSMANAGER_H

#include "objectid.hpp"
#include "offmeshconnection.hpp"
#include "settings.hpp"
#include "tileposition.hpp"

#include <components/misc/guarded.hpp>

#include <map>
#include <set>
#include <unordered_set>
#include <vector>

namespace DetourNavigator
{
    class OffMeshConnectionsManager
    {
    public:
        explicit OffMeshConnectionsManager(const RecastSettings& settings);

        void add(const ObjectId id, const OffMeshConnection& value);

        class PreparedRemoval
        {
            struct Data;
            std::unique_ptr<Data> mData;
            explicit PreparedRemoval(std::unique_ptr<Data> data);
            friend class OffMeshConnectionsManager;
        public:
            ~PreparedRemoval();
            PreparedRemoval(const PreparedRemoval&) = delete;
            PreparedRemoval& operator=(const PreparedRemoval&) = delete;
            const std::set<TilePosition>& changedTiles() const;
            bool isValid() const;
            bool commit();
        };
        // Holds the connection lock until destruction; manager must outlive it.
        std::unique_ptr<PreparedRemoval> prepareRemoval(ObjectId id);
        std::set<TilePosition> remove(const ObjectId id);

        std::vector<OffMeshConnection> get(const TilePosition& tilePosition) const;

    private:
        struct Values
        {
            std::multimap<ObjectId, OffMeshConnection> mById;
            std::map<TilePosition, std::unordered_set<ObjectId>> mByTilePosition;
        };

        const RecastSettings& mSettings;
        Misc::ScopeGuarded<Values> mValues;
    };
}

#endif
