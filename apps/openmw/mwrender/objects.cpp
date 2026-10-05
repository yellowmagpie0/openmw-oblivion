#include "objects.hpp"

#include <osg/Group>
#include <bit>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <osg/UserDataContainer>

#include <components/esm/gameprofile.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/resource/scenemanager.hpp>
#include <components/misc/resourcehelpers.hpp>
#include <components/misc/strings/algorithm.hpp>
#include <components/sceneutil/positionattitudetransform.hpp>
#include <components/sceneutil/unrefqueue.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/ptr.hpp"

#include "animation.hpp"
#include "creatureanimation.hpp"
#include "esm4npcanimation.hpp"
#include "npcanimation.hpp"
#include "vismask.hpp"

namespace MWRender
{

    Objects::Objects(Resource::ResourceSystem* resourceSystem, const osg::ref_ptr<osg::Group>& rootNode,
        SceneUtil::UnrefQueue& unrefQueue)
        : mRootNode(rootNode)
        , mResourceSystem(resourceSystem)
        , mUnrefQueue(unrefQueue)
    {
    }

    Objects::~Objects()
    {
        mObjects.clear();

        for (CellMap::iterator iter = mCellSceneNodes.begin(); iter != mCellSceneNodes.end(); ++iter)
            iter->second->getParent(0)->removeChild(iter->second);
        mCellSceneNodes.clear();
    }

    struct Objects::PreparedModel::Data
    {
        Objects* mOwner = nullptr;
        std::weak_ptr<const char> mIdentity;
        MWWorld::Ptr mPtr;
        MWWorld::CellStore* mCell = nullptr;
        ESM::Position mPosition;
        float mScale = 1;
        int mCount = 0;
        osg::ref_ptr<osg::Group> mCellNode;
        osg::ref_ptr<SceneUtil::PositionAttitudeTransform> mInsert;
        osg::ref_ptr<Animation> mAnimation;
        CellMap::node_type mCellEntry;
        PtrAnimationMap::node_type mObjectEntry;
        bool mNewCell = false;
        bool mPublished = false;
        bool mConsumed = false;
    };

    Objects::PreparedModel::PreparedModel(std::unique_ptr<Data> data) : mData(std::move(data)) {}
    Objects::PreparedModel::~PreparedModel() = default;
    bool Objects::PreparedModel::hasLiveOwner() const noexcept
    { return mData && !mData->mIdentity.expired(); }

    std::unique_ptr<Objects::PreparedModel> Objects::prepareModel(const MWWorld::Ptr& ptr,
        const std::string& model, const osg::Quat& rotation, unsigned nodeMask)
    {
        if (ptr.isEmpty() || !ptr.isInCell() || ptr.getClass().isActor() || ptr.getClass().useAnim()
            || ptr.getContainerStore()
            || ptr.getCellRef().getCount() <= 0 || ptr.getRefData().getBaseNode() || mObjects.contains(ptr.mRef))
            throw std::invalid_argument("prepared model requires an unregistered non-animated world object");
        auto data = std::make_unique<PreparedModel::Data>();
        data->mOwner = this;
        data->mIdentity = mModelPreparationIdentity;
        data->mPtr = ptr;
        data->mCell = ptr.getCell();
        data->mPosition = ptr.getRefData().getPosition();
        data->mScale = ptr.getCellRef().getScale();
        data->mCount = ptr.getCellRef().getCount();
        for (unsigned axis = 0; axis < 3; ++axis)
            if (!std::isfinite(data->mPosition.pos[axis]) || !std::isfinite(data->mPosition.rot[axis]))
                throw std::invalid_argument("nonfinite prepared model placement");
        if (!std::isfinite(data->mScale) || data->mScale <= 0)
            throw std::invalid_argument("invalid prepared model scale");
        double norm = 0;
        for (unsigned axis = 0; axis < 4; ++axis)
        {
            if (!std::isfinite(rotation[axis]))
                throw std::invalid_argument("nonfinite prepared model rotation");
            norm += rotation[axis] * rotation[axis];
        }
        if (std::abs(norm - 1.) > 1e-5)
            throw std::invalid_argument("prepared model rotation is not a unit quaternion");
        data->mInsert = new SceneUtil::PositionAttitudeTransform;
        data->mInsert->setPosition(data->mPosition.asVec3());
        data->mInsert->setAttitude(rotation);
        osg::Vec3f scale(data->mScale, data->mScale, data->mScale);
        ptr.getClass().adjustScale(ptr, scale, true);
        for (unsigned axis = 0; axis < 3; ++axis)
            if (!std::isfinite(scale[axis]) || scale[axis] <= 0)
                throw std::invalid_argument("invalid adjusted prepared model scale");
        data->mInsert->setScale(scale);
        data->mInsert->setNodeMask(nodeMask);
        data->mInsert->getOrCreateUserDataContainer()->addUserObject(new PtrHolder(ptr));
        // Require a real source template rather than the ordinary error marker.
        // ObjectAnimation subsequently clones this same cached valid template.
        if (!model.empty())
            mResourceSystem->getSceneManager()->getTemplate(VFS::Path::toNormalized(model), true, true);
        // Build the actual animation/model on its detached parent, without
        // changing the live reference's base node.
        data->mAnimation = new ObjectAnimation(ptr, osg::ref_ptr<osg::Group>(data->mInsert),
            model, mResourceSystem, false, true);
        PtrAnimationMap stagedObjects;
        stagedObjects.emplace(ptr.mRef, data->mAnimation);
        data->mObjectEntry = stagedObjects.extract(stagedObjects.begin());
        if (const auto found = mCellSceneNodes.find(data->mCell); found != mCellSceneNodes.end())
            data->mCellNode = found->second;
        else
        {
            data->mNewCell = true;
            data->mCellNode = new osg::Group;
            data->mCellNode->setName("Cell Root");
            if (!data->mCellNode->addChild(data->mInsert))
                throw std::runtime_error("private model parent admission rejected");
            CellMap stagedCells;
            stagedCells.emplace(data->mCell, data->mCellNode);
            data->mCellEntry = stagedCells.extract(stagedCells.begin());
        }
        return std::unique_ptr<PreparedModel>(new PreparedModel(std::move(data)));
    }

    bool Objects::validatePreparedModel(const PreparedModel& prepared) const
    {
        const auto* data = prepared.mData.get();
        if (!data || data->mOwner != this || data->mIdentity.lock() != mModelPreparationIdentity
            || data->mPublished || data->mConsumed || data->mObjectEntry.empty())
            return false;
        const auto& ptr = data->mPtr;
        if (ptr.getCell() != data->mCell || ptr.getCellRef().getCount() != data->mCount
            || ptr.getRefData().getBaseNode() || mObjects.contains(ptr.mRef)
            || std::bit_cast<std::uint32_t>(ptr.getCellRef().getScale()) != std::bit_cast<std::uint32_t>(data->mScale))
            return false;
        const auto& pos = ptr.getRefData().getPosition();
        for (unsigned axis = 0; axis < 3; ++axis)
            if (std::bit_cast<std::uint32_t>(pos.pos[axis]) != std::bit_cast<std::uint32_t>(data->mPosition.pos[axis])
                || std::bit_cast<std::uint32_t>(pos.rot[axis]) != std::bit_cast<std::uint32_t>(data->mPosition.rot[axis]))
                return false;
        const auto cell = mCellSceneNodes.find(data->mCell);
        return data->mNewCell ? cell == mCellSceneNodes.end() && !data->mCellEntry.empty()
            : cell != mCellSceneNodes.end() && cell->second == data->mCellNode
                && mRootNode->containsNode(data->mCellNode);
    }

    bool Objects::commitModel(PreparedModel& prepared)
    {
        if (!validatePreparedModel(prepared)) return false;
        auto& data = *prepared.mData;
        osg::Group* parent = data.mNewCell ? mRootNode.get() : data.mCellNode.get();
        osg::Node* child = data.mNewCell ? static_cast<osg::Node*>(data.mCellNode.get()) : data.mInsert.get();
        try
        {
            if (!parent->addChild(child))
                throw std::runtime_error("model scene admission rejected");
        }
        catch (...)
        {
            // Include the attempted child if a parent inserts and then throws.
            if (parent->containsNode(child)) parent->removeChild(child);
            throw;
        }
        if (data.mNewCell) mCellSceneNodes.insert(std::move(data.mCellEntry));
        mObjects.insert(std::move(data.mObjectEntry));
        data.mPtr.getRefData().setBaseNode(data.mInsert);
        data.mPublished = true;
        return true;
    }

    bool Objects::rollbackModelAdmission(PreparedModel& prepared)
    {
        auto* data = prepared.mData.get();
        if (!data || data->mOwner != this || data->mIdentity.lock() != mModelPreparationIdentity
            || !data->mPublished || data->mConsumed)
            return false;
        const auto object = mObjects.find(data->mPtr.mRef);
        const auto cell = mCellSceneNodes.find(data->mCell);
        if (object == mObjects.end() || object->second != data->mAnimation
            || data->mPtr.getRefData().getBaseNode() != data->mInsert
            || cell == mCellSceneNodes.end() || cell->second != data->mCellNode
            || !data->mCellNode->containsNode(data->mInsert))
            return false;
        data->mCellNode->removeChild(data->mInsert);
        data->mPtr.getRefData().setBaseNode(nullptr);
        mObjects.erase(object);
        if (data->mNewCell && data->mCellNode->getNumChildren() == 0)
        {
            mRootNode->removeChild(data->mCellNode);
            mCellSceneNodes.erase(cell);
        }
        data->mPublished = false;
        data->mConsumed = true;
        return true;
    }

    void Objects::insertBegin(const MWWorld::Ptr& ptr)
    {
        assert(mObjects.find(ptr.mRef) == mObjects.end());

        osg::ref_ptr<osg::Group> cellnode;

        CellMap::iterator found = mCellSceneNodes.find(ptr.getCell());
        if (found == mCellSceneNodes.end())
        {
            cellnode = new osg::Group;
            cellnode->setName("Cell Root");
            mRootNode->addChild(cellnode);
            mCellSceneNodes[ptr.getCell()] = cellnode;
        }
        else
            cellnode = found->second;

        osg::ref_ptr<SceneUtil::PositionAttitudeTransform> insert(new SceneUtil::PositionAttitudeTransform);
        cellnode->addChild(insert);

        insert->getOrCreateUserDataContainer()->addUserObject(new PtrHolder(ptr));

        const float* f = ptr.getRefData().getPosition().pos;

        insert->setPosition(osg::Vec3(f[0], f[1], f[2]));

        const float scale = ptr.getCellRef().getScale();
        osg::Vec3f scaleVec(scale, scale, scale);
        ptr.getClass().adjustScale(ptr, scaleVec, true);
        insert->setScale(scaleVec);

        ptr.getRefData().setBaseNode(std::move(insert));
    }

    void Objects::insertModel(const MWWorld::Ptr& ptr, const std::string& mesh, bool allowLight)
    {
        insertBegin(ptr);
        ptr.getRefData().getBaseNode()->setNodeMask(Mask_Object);
        bool animated = ptr.getClass().useAnim();
        std::string animationMesh = mesh;
        if (animated && !mesh.empty())
        {
            animationMesh = Misc::ResourceHelpers::correctActorModelPath(
                VFS::Path::toNormalized(mesh), mResourceSystem->getVFS());
            if (animationMesh == mesh && Misc::StringUtils::ciEndsWith(animationMesh, ".nif")
                && MWBase::Environment::get().getWorld()->getGameProfile() != ESM::GameProfile::Oblivion)
                animated = false;
        }

        osg::ref_ptr<ObjectAnimation> anim(
            new ObjectAnimation(ptr, animationMesh, mResourceSystem, animated, allowLight));

        mObjects.emplace(ptr.mRef, std::move(anim));
    }

    void Objects::insertCreature(const MWWorld::Ptr& ptr, const std::string& mesh, bool weaponsShields)
    {
        insertBegin(ptr);
        ptr.getRefData().getBaseNode()->setNodeMask(Mask_Actor);

        bool animated = true;
        std::string animationMesh
            = Misc::ResourceHelpers::correctActorModelPath(VFS::Path::toNormalized(mesh), mResourceSystem->getVFS());
        if (animationMesh == mesh && Misc::StringUtils::ciEndsWith(animationMesh, ".nif"))
            animated = false;

        // CreatureAnimation
        osg::ref_ptr<Animation> anim;

        if (weaponsShields)
            anim = new CreatureWeaponAnimation(ptr, animationMesh, mResourceSystem, animated);
        else
            anim = new CreatureAnimation(ptr, animationMesh, mResourceSystem, animated);

        if (mObjects.emplace(ptr.mRef, anim).second)
            ptr.getClass().getContainerStore(ptr).setContListener(static_cast<ActorAnimation*>(anim.get()));
    }

    void Objects::insertCreature4(const MWWorld::Ptr& ptr, const std::string& model)
    {
        insertBegin(ptr);
        ptr.getRefData().getBaseNode()->setNodeMask(Mask_Actor);
        osg::ref_ptr<ESM4CreatureAnimation> animation(new ESM4CreatureAnimation(
            ptr, model, osg::ref_ptr<osg::Group>(ptr.getRefData().getBaseNode()), mResourceSystem));
        mObjects.emplace(ptr.mRef, std::move(animation));
    }

    void Objects::insertNPC(const MWWorld::Ptr& ptr)
    {
        insertBegin(ptr);
        ptr.getRefData().getBaseNode()->setNodeMask(Mask_Actor);

        if (ptr.getType() == ESM::REC_NPC_4)
        {
            osg::ref_ptr<ESM4NpcAnimation> anim(
                new ESM4NpcAnimation(ptr, osg::ref_ptr<osg::Group>(ptr.getRefData().getBaseNode()), mResourceSystem));
            mObjects.emplace(ptr.mRef, anim);
        }
        else
        {
            osg::ref_ptr<NpcAnimation> anim(
                new NpcAnimation(ptr, osg::ref_ptr<osg::Group>(ptr.getRefData().getBaseNode()), mResourceSystem));

            if (mObjects.emplace(ptr.mRef, anim).second)
            {
                ptr.getClass().getInventoryStore(ptr).setInvListener(anim.get());
                ptr.getClass().getInventoryStore(ptr).setContListener(anim.get());
            }
        }
    }

    struct Objects::PreparedModelRemoval::Data
    {
        Objects* mOwner = nullptr;
        std::weak_ptr<const char> mIdentity;
        MWWorld::Ptr mPtr;
        MWWorld::CellStore* mCell = nullptr;
        int mCount = 0;
        osg::ref_ptr<osg::Group> mCellNode;
        osg::ref_ptr<osg::Node> mInsert;
        osg::ref_ptr<Animation> mAnimation;
        std::size_t mQueueSize = 0;
        bool mCommitted = false;
    };

    Objects::PreparedModelRemoval::PreparedModelRemoval(std::unique_ptr<Data> data)
        : mData(std::move(data)) {}
    Objects::PreparedModelRemoval::~PreparedModelRemoval() = default;

    bool Objects::PreparedModelRemoval::isValid() const
    {
        const auto& data = *mData;
        if (data.mCommitted || data.mIdentity.expired())
            return false;
        const auto& owner = *data.mOwner;
        const auto object = owner.mObjects.find(data.mPtr.mRef);
        if (!data.mAnimation)
            return object == owner.mObjects.end() && !data.mPtr.getRefData().getBaseNode()
                && data.mPtr.getCellRef().getCount() == data.mCount;
        const auto cell = owner.mCellSceneNodes.find(data.mCell);
        return object != owner.mObjects.end() && object->second == data.mAnimation
            && cell != owner.mCellSceneNodes.end() && cell->second == data.mCellNode
            && data.mPtr.getRefData().getBaseNode() == data.mInsert
            && data.mInsert->getNumParents() == 1 && data.mInsert->getParent(0) == data.mCellNode
            && data.mPtr.getCellRef().getCount() == data.mCount
            && owner.mUnrefQueue.canPushPrepared(data.mQueueSize);
    }

    bool Objects::PreparedModelRemoval::commit()
    {
        if (!isValid())
            return false;
        auto& data = *mData;
        if (data.mAnimation)
        {
            auto& owner = *data.mOwner;
            const auto index = data.mCellNode->getChildIndex(data.mInsert);
            // The engine owns this exact ordinary Group. Detach the public
            // placement node while retaining its private animation subtree.
            data.mCellNode->osg::Group::removeChildren(index, 1);
            owner.mObjects.erase(data.mPtr.mRef);
            owner.mUnrefQueue.push(osg::ref_ptr<osg::Referenced>(data.mAnimation));
            data.mPtr.getRefData().setBaseNode(nullptr);
        }
        data.mCommitted = true;
        return true;
    }

    std::unique_ptr<Objects::PreparedModelRemoval> Objects::prepareModelRemoval(const MWWorld::Ptr& ptr)
    {
        if (ptr.isEmpty() || !ptr.isInCell() || ptr.getClass().isActor() || ptr.getClass().useAnim()
            || ptr.getContainerStore() || ptr.getCellRef().getCount() <= 0)
            throw std::invalid_argument("prepared model removal requires a live non-animated world item");
        auto data = std::make_unique<PreparedModelRemoval::Data>();
        data->mOwner = this;
        data->mIdentity = mModelPreparationIdentity;
        data->mPtr = ptr;
        data->mCell = ptr.getCell();
        data->mCount = ptr.getCellRef().getCount();
        data->mInsert = ptr.getRefData().getBaseNode();
        const auto object = mObjects.find(ptr.mRef);
        if (data->mInsert)
        {
            const auto cell = mCellSceneNodes.find(data->mCell);
            if (object == mObjects.end() || cell == mCellSceneNodes.end()
                || typeid(*object->second) != typeid(ObjectAnimation)
                || typeid(*cell->second) != typeid(osg::Group)
                || data->mInsert->getNumParents() != 1 || data->mInsert->getParent(0) != cell->second)
                throw std::invalid_argument("unexpected prepared item model binding");
            data->mCellNode = cell->second;
            data->mAnimation = object->second;
            data->mQueueSize = mUnrefQueue.getSize();
            mUnrefQueue.reserveAdditional(1);
        }
        else if (object != mObjects.end())
            throw std::invalid_argument("item animation has no placement node");
        return std::unique_ptr<PreparedModelRemoval>(new PreparedModelRemoval(std::move(data)));
    }

    bool Objects::removeObject(const MWWorld::Ptr& ptr)
    {
        if (!ptr.getRefData().getBaseNode())
            return true;

        const auto iter = mObjects.find(ptr.mRef);
        if (iter != mObjects.end())
        {
            iter->second->removeFromScene();
            mUnrefQueue.push(std::move(iter->second));
            mObjects.erase(iter);

            if (ptr.getClass().isActor())
            {
                if (ptr.getClass().hasInventoryStore(ptr))
                    ptr.getClass().getInventoryStore(ptr).setInvListener(nullptr);

                ptr.getClass().getContainerStore(ptr).setContListener(nullptr);
            }

            ptr.getRefData().getBaseNode()->getParent(0)->removeChild(ptr.getRefData().getBaseNode());

            ptr.getRefData().setBaseNode(nullptr);
            return true;
        }
        return false;
    }

    void Objects::removeCell(const MWWorld::CellStore* store)
    {
        for (PtrAnimationMap::iterator iter = mObjects.begin(); iter != mObjects.end();)
        {
            MWWorld::Ptr ptr = iter->second->getPtr();
            if (ptr.getCell() == store)
            {
                if (ptr.getClass().isActor() && ptr.getRefData().getCustomData())
                {
                    if (ptr.getClass().hasInventoryStore(ptr))
                        ptr.getClass().getInventoryStore(ptr).setInvListener(nullptr);
                    ptr.getClass().getContainerStore(ptr).setContListener(nullptr);
                }

                iter->second->removeFromScene();
                mUnrefQueue.push(std::move(iter->second));
                iter = mObjects.erase(iter);
            }
            else
                ++iter;
        }

        CellMap::iterator cell = mCellSceneNodes.find(store);
        if (cell != mCellSceneNodes.end())
        {
            cell->second->getParent(0)->removeChild(cell->second);
            mCellSceneNodes.erase(cell);
        }
    }

    void Objects::updatePtr(const MWWorld::Ptr& old, const MWWorld::Ptr& cur)
    {
        osg::ref_ptr<osg::Node> objectNode = cur.getRefData().getBaseNode();
        if (!objectNode)
            return;

        MWWorld::CellStore* newCell = cur.getCell();

        osg::Group* cellnode;
        if (mCellSceneNodes.find(newCell) == mCellSceneNodes.end())
        {
            cellnode = new osg::Group;
            mRootNode->addChild(cellnode);
            mCellSceneNodes[newCell] = cellnode;
        }
        else
        {
            cellnode = mCellSceneNodes[newCell];
        }

        osg::UserDataContainer* userDataContainer = objectNode->getUserDataContainer();
        if (userDataContainer)
            for (unsigned int i = 0; i < userDataContainer->getNumUserObjects(); ++i)
            {
                if (dynamic_cast<PtrHolder*>(userDataContainer->getUserObject(i)))
                    userDataContainer->setUserObject(i, new PtrHolder(cur));
            }

        if (objectNode->getNumParents())
            objectNode->getParent(0)->removeChild(objectNode);
        cellnode->addChild(objectNode);

        PtrAnimationMap::iterator iter = mObjects.find(old.mRef);
        if (iter != mObjects.end())
            iter->second->updatePtr(cur);
    }

    Animation* Objects::getAnimation(const MWWorld::Ptr& ptr)
    {
        PtrAnimationMap::const_iterator iter = mObjects.find(ptr.mRef);
        if (iter != mObjects.end())
            return iter->second;

        return nullptr;
    }

    const Animation* Objects::getAnimation(const MWWorld::ConstPtr& ptr) const
    {
        PtrAnimationMap::const_iterator iter = mObjects.find(ptr.mRef);
        if (iter != mObjects.end())
            return iter->second;

        return nullptr;
    }

}
