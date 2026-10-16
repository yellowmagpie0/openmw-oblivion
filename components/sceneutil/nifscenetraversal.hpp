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
    // Explicit traversal of a private ordinary NIF reference hierarchy.
    // The reference root's author local is already in native world space; its
    // OSG placement ancestors are rendering scaffolding, not native parents.
    // Retains native global caches separately from controller locals and OSG
    // relative drawing matrices. Requires invertible transforms and an unchanged
    // hierarchy. Placement may change only inside mReferenceUpdate. Admission, physical first synchronization,
    // controller clocks, physics stepping and reference callbacks have owners.
    // Publishing a pass leaves its projection handles on the model. Retire or
    // explicitly reinitialize the model when changing update ownership. Exceptions
    // do not roll back controller state or already completed nodes.
    class NifSceneTraversal
    {
    public:
        enum class Mode { Full, Selected, Rigid };
        struct Callbacks
        {
            std::function<std::optional<std::uint16_t>(const NifOsg::MatrixTransform&)> mCollisionFlags;
            // Read only for an active settled collision. Input/output uses Ni world units.
            std::function<Nif::NiTransform(const NifOsg::MatrixTransform&)> mPhysicalWorld;
            // Runs after projection and child-cache rebasing, before children.
            // A child change mask is not authorization to publish a REFR.
            std::function<void(NifOsg::MatrixTransform&, const NifBullet::RagdollBonePoseWriteback&,
                std::optional<std::uint16_t>)> mProjected;
            // Pending0x40 dispatch, even if bit0 is inactive. Runs after own
            // controllers and animated-world cache publication, before mProjected
            // and children. The physical owner must reset/synchronize motion and
            // clear only0x40 atomically on success; mCollisionFlags is reinspected.
            // No mPhysicalWorld read or bit8 author write occurs on this pass.
            // Missing handler rejects; exceptions do not roll back published caches.
            std::function<void(const NifOsg::MatrixTransform&, const Nif::NiTransform&, std::uint16_t)>
                mFirstSynchronization;
            // Root only, after mProjected and before children. The owner may
            // publish the REFR and change rendering ancestors here (including
            // cell migration). Reanchors drawing/visitor paths without changing
            // native caches or controller locals. The owner retains the REFR
            // and new placement; this traversal leases old/new ancestors.
            // Exceptions leave completed projection/writes in place.
            std::function<void(NifOsg::MatrixTransform&, const NifBullet::RagdollBonePoseWriteback&,
                std::optional<std::uint16_t>)> mReferenceUpdate;
            // Required for Selected/Rigid. Live computed NiAVObject flags,
            // not the original file flags: child admission bit1, projection
            // bit2, property controllers bit3, rigid child dispatch bit4.
            // Own node controllers run whenever a selected node is entered.
            // Reinspect projection flags after controllers, as native does.
            std::function<std::uint16_t(const NifOsg::MatrixTransform&)> mNodeFlags;
        };

        explicit NifSceneTraversal(NifOsg::MatrixTransform& referenceRoot,
            Nif::NativeTransformPrecision precision = Nif::NativeTransformPrecision::Single);
        ~NifSceneTraversal();
        NifSceneTraversal(const NifSceneTraversal&) = delete;
        NifSceneTraversal& operator=(const NifSceneTraversal&) = delete;

        bool isValid() const noexcept;
        Nif::NiTransform cachedWorld(const NifOsg::MatrixTransform& node) const;
        // Native selected-root registration refreshes this cache after assigning
        // final reference scale, with collision dispatch suppressed. Update only
        // the root world/drawing pose and rebase immediate child drawing handles
        // to retain their previous caches. No controllers, physics or REFR writes.
        // The preparation owner decides whether registration selected this root.
        void refreshReferenceRootCache();
        // Invokes existing property/node callbacks once, including callbacks
        // with work after traverse(), then projects and visits children. The
        // scheduling owner must prevent an additional ordinary update pass.
        // Selected/Rigid execute ordinary NiNode base policies. The scheduling
        // owner must resolve model-specific outer dispatch/culling first. Root
        // entry is explicit; bit1 only gates children. Rigid dispatch propagates
        // to entered descendants; Selected uses each child's bit4. Rendering
        // scaffolding inherits the containing native node's property policy.
        void update(const osg::FrameStamp& stamp, unsigned traversalNumber, const Callbacks& callbacks = {},
            Mode mode = Mode::Full);

        // Controller context query: the synthetic OSG ancestors of this root
        // must not be treated as native parents by a world-space controller.
        static bool isReferenceRoot(const osg::NodeVisitor* visitor, const osg::Node* node) noexcept;
        static std::optional<Mode> updateMode(const osg::NodeVisitor* visitor) noexcept;

    private:
        void updateImpl(const osg::FrameStamp& stamp, unsigned traversalNumber,
            const Callbacks& callbacks, bool rootOnly, Mode mode);
        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };
}
#endif
