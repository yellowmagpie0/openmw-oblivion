#ifndef OPENMW_COMPONENTS_SCENEUTIL_NIFSCENETRAVERSAL_HPP
#define OPENMW_COMPONENTS_SCENEUTIL_NIFSCENETRAVERSAL_HPP

#include <components/nifbullet/ragdollbonepose.hpp>

#include <functional>
#include <memory>
#include <optional>

namespace osg
{
    class FrameStamp;
    class Node;
    class NodeVisitor;
}
namespace NifOsg { class MatrixTransform; }

namespace SceneUtil
{
    // Explicit settled traversal of a private ordinary NIF reference hierarchy.
    // The reference root's author local is already in native world space; its
    // OSG placement ancestors are rendering scaffolding, not native parents.
    // Retains native global caches separately from controller locals and OSG
    // relative drawing matrices. Requires invertible transforms and an unchanged
    // hierarchy/placement during a pass. Admission, pending first synchronization,
    // controller clocks, physics stepping and reference callbacks have owners.
    // Publishing a pass leaves its projection handles on the model. Retire or
    // explicitly reinitialize the model when changing update ownership. Exceptions
    // do not roll back controller state or already completed nodes.
    class NifSceneTraversal
    {
    public:
        struct Callbacks
        {
            std::function<std::optional<std::uint16_t>(const NifOsg::MatrixTransform&)> mCollisionFlags;
            // Read only for an active collision. Input/output uses Ni world units.
            std::function<Nif::NiTransform(const NifOsg::MatrixTransform&)> mPhysicalWorld;
            // Runs after projection and child-cache rebasing, before children.
            // A child change mask is not authorization to publish a REFR.
            std::function<void(NifOsg::MatrixTransform&, const NifBullet::RagdollBonePoseWriteback&,
                std::optional<std::uint16_t>)> mProjected;
        };

        explicit NifSceneTraversal(NifOsg::MatrixTransform& referenceRoot,
            Nif::NativeTransformPrecision precision = Nif::NativeTransformPrecision::Single);
        ~NifSceneTraversal();
        NifSceneTraversal(const NifSceneTraversal&) = delete;
        NifSceneTraversal& operator=(const NifSceneTraversal&) = delete;

        bool isValid() const noexcept;
        Nif::NiTransform cachedWorld(const NifOsg::MatrixTransform& node) const;
        // Invokes existing property/node callbacks once, including callbacks
        // with work after traverse(), then projects and visits children. The
        // scheduling owner must prevent an additional ordinary update pass.
        void update(const osg::FrameStamp& stamp, unsigned traversalNumber, const Callbacks& callbacks = {});

        // Controller context query: the synthetic OSG ancestors of this root
        // must not be treated as native parents by a world-space controller.
        static bool isReferenceRoot(const osg::NodeVisitor* visitor, const osg::Node* node) noexcept;

    private:
        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };
}
#endif
