#ifndef GAME_RENDER_OBJECTS_H
#define GAME_RENDER_OBJECTS_H

#include <map>
#include <memory>
#include <osg/Quat>
#include <string>

#include <osg/Object>
#include <osg/ref_ptr>

#include "../mwworld/ptr.hpp"

namespace osg
{
    class Group;
}

namespace Resource
{
    class ResourceSystem;
}

namespace MWWorld
{
    class CellStore;
}

namespace SceneUtil
{
    class UnrefQueue;
}

namespace MWRender
{

    class Animation;

    class PtrHolder : public osg::Object
    {
    public:
        PtrHolder(const MWWorld::Ptr& ptr)
            : mPtr(ptr)
        {
        }

        PtrHolder() {}

        PtrHolder(const PtrHolder& copy, const osg::CopyOp& copyop)
            : mPtr(copy.mPtr)
        {
        }

        META_Object(MWRender, PtrHolder)

        MWWorld::Ptr mPtr;
    };

    class Objects
    {
        using PtrAnimationMap = std::map<const MWWorld::LiveCellRefBase*, osg::ref_ptr<Animation>>;

        typedef std::map<const MWWorld::CellStore*, osg::ref_ptr<osg::Group>> CellMap;
        CellMap mCellSceneNodes;
        PtrAnimationMap mObjects;
        osg::ref_ptr<osg::Group> mRootNode;
        Resource::ResourceSystem* mResourceSystem;
        SceneUtil::UnrefQueue& mUnrefQueue;

        void insertBegin(const MWWorld::Ptr& ptr);
        std::shared_ptr<const char> mModelPreparationIdentity = std::make_shared<const char>(0);

    public:
        Objects(Resource::ResourceSystem* resourceSystem, const osg::ref_ptr<osg::Group>& rootNode,
            SceneUtil::UnrefQueue& unrefQueue);
        ~Objects();

        class PreparedModel
        {
            struct Data;
            std::unique_ptr<Data> mData;
            explicit PreparedModel(std::unique_ptr<Data> data);
            friend class Objects;
        public:
            ~PreparedModel();
            bool hasLiveOwner() const noexcept;
            PreparedModel(const PreparedModel&) = delete;
            PreparedModel& operator=(const PreparedModel&) = delete;
        };

        // Borrowed reference and cell must outlive preparation and registration.
        // Main-thread non-actor admission; no physics or game observers.
        std::unique_ptr<PreparedModel> prepareModel(const MWWorld::Ptr& ptr, const std::string& model,
            const osg::Quat& rotation, unsigned nodeMask);
        bool validatePreparedModel(const PreparedModel& prepared) const;
        bool commitModel(PreparedModel& prepared);
        // Synchronous rollback before observers or another scene mutation.
        // Returns false if ownership no longer matches; never removes a
        // replacement model. The original preparation is consumed on rollback.
        bool rollbackModelAdmission(PreparedModel& prepared);

        /// @param allowLight If false, no lights will be created, and particles systems will be removed.
        void insertModel(const MWWorld::Ptr& ptr, const std::string& model, bool allowLight = true);

        void insertNPC(const MWWorld::Ptr& ptr);
        void insertCreature(const MWWorld::Ptr& ptr, const std::string& model, bool weaponsShields);
        void insertCreature4(const MWWorld::Ptr& ptr, const std::string& model);

        Animation* getAnimation(const MWWorld::Ptr& ptr);
        const Animation* getAnimation(const MWWorld::ConstPtr& ptr) const;

        class PreparedModelRemoval
        {
            struct Data;
            std::unique_ptr<Data> mData;
            explicit PreparedModelRemoval(std::unique_ptr<Data> data);
            friend class Objects;
        public:
            ~PreparedModelRemoval();
            PreparedModelRemoval(const PreparedModelRemoval&) = delete;
            PreparedModelRemoval& operator=(const PreparedModelRemoval&) = delete;
            bool isValid() const;
            bool commit();
        };

        // Synchronous non-actor removal. Reference and cell must outlive the
        // scoped plan; destroy it before callbacks or another scene mutation.
        std::unique_ptr<PreparedModelRemoval> prepareModelRemoval(const MWWorld::Ptr& ptr);
        bool removeObject(const MWWorld::Ptr& ptr);
        ///< \return found?

        void removeCell(const MWWorld::CellStore* store);

        /// Updates containing cell for object rendering data
        void updatePtr(const MWWorld::Ptr& old, const MWWorld::Ptr& cur);

    private:
        void operator=(const Objects&);
        Objects(const Objects&);
    };
}
#endif
