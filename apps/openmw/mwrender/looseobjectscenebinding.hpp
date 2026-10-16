#ifndef OPENMW_MWRENDER_LOOSEOBJECTSCENEBINDING_H
#define OPENMW_MWRENDER_LOOSEOBJECTSCENEBINDING_H

#include <osg/observer_ptr>
#include <osg/ref_ptr>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace osg { class Group; }
namespace NifBullet { struct ActorRagdollDefinition; }
namespace MWWorld { class Ptr; }

namespace MWRender
{
    class ObjectAnimation;
    class Objects;

    // Main-thread link to one prepared/published private loose-item model.
    // Retaining this handle or an animation lease does not undo retirement.
    // Physics stores a weak handle; this does not retain a renderer or REFR.
    class LooseObjectSceneBinding
    {
    public:
        LooseObjectSceneBinding(const LooseObjectSceneBinding&) = delete;
        LooseObjectSceneBinding& operator=(const LooseObjectSceneBinding&) = delete;
        // Source/record/registration identity; physical properties may already
        // have been scaled independently for admission.
        bool matches(const NifBullet::ActorRagdollDefinition& definition) const;
        // The caller must keep the borrowed reference alive for this query.
        bool isValidFor(const MWWorld::Ptr& ptr, bool requirePublished = false) const;
        osg::ref_ptr<ObjectAnimation> lockAnimation(const MWWorld::Ptr& ptr) const;

    private:
        LooseObjectSceneBinding(ObjectAnimation& animation, osg::Group& placement,
            Objects& owner, const NifBullet::ActorRagdollDefinition& definition);
        void publish() noexcept { mPublished = true; }
        void retire() noexcept { mRetired = true; }

        osg::observer_ptr<ObjectAnimation> mAnimation;
        osg::observer_ptr<osg::Group> mPlacement;
        std::weak_ptr<const char> mOwner;
        const Objects* mObjects;
        std::string mSourceHash;
        std::uint32_t mBodyRecord, mNodeRecord;
        std::uint16_t mCollisionFlags;
        std::optional<std::uint32_t> mReferenceRoot;
        bool mPublished = false;
        bool mRetired = false;
        friend class Objects;
    };
}

#endif
