#include "looseobjectscenebinding.hpp"

#include "animation.hpp"
#include "objects.hpp"

#include <components/nifbullet/actorragdollphysics.hpp>
#include <components/sceneutil/actorragdollpose.hpp>
#include <components/sceneutil/positionattitudetransform.hpp>

#include <stdexcept>

namespace MWRender
{
    LooseObjectSceneBinding::LooseObjectSceneBinding(ObjectAnimation& animation, osg::Group& placement,
        Objects& owner, const NifBullet::ActorRagdollDefinition& definition)
        : mAnimation(&animation), mPlacement(&placement), mOwner(owner.mModelPreparationIdentity), mObjects(&owner)
        , mSourceHash(definition.mSourceHash)
        , mBodyRecord(0), mNodeRecord(0), mCollisionFlags(0), mReferenceRoot(definition.mReferenceRoot)
        , mReferenceAssetRoot(definition.mReferenceAssetRoot)
        , mReferenceScabbard(definition.mReferenceScabbard)
    {
        if (definition.mBodies.size() != 1 || mSourceHash.empty() || mOwner.expired())
            throw std::invalid_argument("loose scene link requires an owned identified single-body model");
        const auto& body = definition.mBodies.front();
        mBodyRecord = body.mRecord;
        mNodeRecord = body.mNodeRecord;
        mCollisionFlags = body.mCollisionFlags;
        if (!isValidFor(animation.getPtr()))
            throw std::invalid_argument("invalid prepared loose scene link");
    }

    bool LooseObjectSceneBinding::matches(const NifBullet::ActorRagdollDefinition& definition) const
    {
        return definition.mBodies.size() == 1 && definition.mSourceHash == mSourceHash
            && definition.mReferenceRoot == mReferenceRoot && definition.mBodies.front().mRecord == mBodyRecord
            && definition.mReferenceAssetRoot == mReferenceAssetRoot
            && definition.mReferenceScabbard == mReferenceScabbard
            && definition.mBodies.front().mNodeRecord == mNodeRecord
            && definition.mBodies.front().mCollisionFlags == mCollisionFlags;
    }

    bool LooseObjectSceneBinding::isValidFor(const MWWorld::Ptr& ptr, bool requirePublished) const
    {
        if (mRetired || mOwner.expired() || (requirePublished && !mPublished) || ptr.isEmpty())
            return false;
        osg::ref_ptr<ObjectAnimation> animation;
        osg::ref_ptr<osg::Group> placement;
        if (!mAnimation.lock(animation) || !mPlacement.lock(placement) || animation->getPtr() != ptr)
            return false;
        if (mPublished ? ptr.getRefData().getBaseNode() != placement : ptr.getRefData().getBaseNode() != nullptr)
            return false;
        const auto model = mObjects->mObjects.find(ptr.mRef);
        const auto binding = mObjects->mLooseSceneBindings.find(ptr.mRef);
        if (mPublished)
        {
            if (model == mObjects->mObjects.end() || model->second != animation
                || binding == mObjects->mLooseSceneBindings.end() || binding->second.get() != this)
                return false;
        }
        else if (model != mObjects->mObjects.end() || binding != mObjects->mLooseSceneBindings.end())
            return false;
        const auto* root = animation->getObjectRoot();
        const auto* pose = animation->getLoosePhysicalPose();
        return root && root->getNumParents() == 1 && root->getParent(0) == placement
            && pose && pose->isValid();
    }

    osg::ref_ptr<ObjectAnimation> LooseObjectSceneBinding::lockAnimation(const MWWorld::Ptr& ptr) const
    {
        osg::ref_ptr<ObjectAnimation> result;
        if (!isValidFor(ptr, true) || !mAnimation.lock(result))
            return {};
        return result;
    }
}
