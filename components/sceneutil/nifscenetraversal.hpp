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
        struct SelectiveUpdatePolicy
        {
            enum class Kind { Node, AVObject, Geometry };
            Kind mKind = Kind::Node;
            std::uint16_t mFlags = 0;
            bool mHasController = false;
            bool mHasTransformController = false;
            bool mHasPropertyController = false;
            bool mHasSkin = false;
        };
        struct SelectiveUpdateResult
        {
            bool mNeedsUpdate;
            bool mRigid;
        };
        struct CollisionResult
        {
            std::optional<std::uint16_t> mFlags;
            std::optional<Nif::NiTransform> mPhysicalTarget;
        };
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
            // Overrides prepared flags; required for Selected/Rigid when no
            // policy has been prepared. Live computed NiAVObject flags,
            // not the original file flags: child admission bit1, projection
            // bit2, property controllers bit3, rigid child dispatch bit4.
            // Own node controllers run whenever a selected node is entered.
            // Reinspect projection flags after controllers, as native does.
            std::function<std::uint16_t(const NifOsg::MatrixTransform&)> mNodeFlags;
            // After own controllers and native composition, before collision
            // flags/physical reads. Supplies this node's animated world using
            // its parent's already projected cache. The physical owner may
            // dispatch its body here; query the supplied world rather than the
            // node's previous drawing/cache handle. Do not change scene locals.
            std::function<void(const NifOsg::MatrixTransform&, const Nif::NiTransform&)> mCollisionUpdate;
            // Alternative to mPhysicalWorld for owners whose collision update
            // can produce no scene target (for example SceneToPhysics). A null
            // target keeps animated world/locals even with active bit0/bit8.
            // Supply exactly one physical provider when active; pending40 does
            // not read either. mProjected still receives the actual flags.
            std::function<std::optional<Nif::NiTransform>(const NifOsg::MatrixTransform&)> mPhysicalTarget;
            // Complete collision dispatch owned by the caller, after own
            // controllers and native composition. Return actual live flags
            // and an optional renderer target. No target retains animated
            // world/locals, including pending synchronization deferred by the
            // raw update phase. The traversal does not dispatch/reset/clear it
            // again. Targets require settled active flags. Mutually exclusive
            // with the five legacy collision callbacks above; mProjected and
            // mReferenceUpdate still run. Do not change scene locals/hierarchy.
            // Body effects may already be published if later projection fails.
            std::function<CollisionResult(const NifOsg::MatrixTransform&, const Nif::NiTransform&)> mCollision;
        };

        explicit NifSceneTraversal(NifOsg::MatrixTransform& referenceRoot,
            Nif::NativeTransformPrecision precision = Nif::NativeTransformPrecision::Single);
        // Object-local root with an explicit native parent. Rendering ancestors
        // remain independent scaffolding. Supply current native placement on
        // every update, including controllerless passes; reference-only entry
        // points reject this binding. Ordinary-node policies still apply.
        NifSceneTraversal(NifOsg::MatrixTransform& objectRoot, const Nif::NiTransform& nativeParentWorld,
            Nif::NativeTransformPrecision precision = Nif::NativeTransformPrecision::Single);
        // Prepare all caches/projection storage without changing the scene.
        // Admission guards every native local/cache component, hierarchy,
        // source projection and external drawing placement. The source must be
        // owned exclusively through commit; callbacks have their own owner.
        static std::unique_ptr<NifSceneTraversal> prepareObjectLocal(NifOsg::MatrixTransform& objectRoot,
            const Nif::NiTransform& nativeParentWorld,
            Nif::NativeTransformPrecision precision = Nif::NativeTransformPrecision::Single);
        bool commitPrepared() noexcept;
        // Release only still-owned drawing handles, including after hierarchy
        // damage. Preserve current author locals and foreign replacements.
        // Reject reentry while traversing. Retirement is final for this owner.
        bool retire() noexcept;
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
        void updateObjectLocal(const osg::FrameStamp& stamp, unsigned traversalNumber,
            const Nif::NiTransform& nativeParentWorld, const Callbacks& callbacks = {}, Mode mode = Mode::Full);
        // Native Full(time, false): compose/project every node and perform
        // physical/reference callbacks without evaluating any own/property or
        // drawing callbacks. Generation supplies time zero. Unlike Selected/
        // Rigid, Full has an independent controller-evaluation argument.
        void updateWithoutControllers(const osg::FrameStamp& stamp, unsigned traversalNumber,
            const Callbacks& callbacks = {});
        void updateObjectLocalWithoutControllers(const osg::FrameStamp& stamp, unsigned traversalNumber,
            const Nif::NiTransform& nativeParentWorld, const Callbacks& callbacks = {});

        // Prepare ordinary NiNode/NiAVObject/NiGeometry selective flags from
        // live instance policy inputs. Controller presence includes inactive
        // controllers; transform classification is independent of activation.
        // Preserve all input bits except computed bits1..4. The resolver must
        // only inspect state. Commit all flags together after hierarchy/placement
        // validation; no controllers, caches or physics are advanced. The owner
        // chooses recomputation events and the inherited transform input.
        SelectiveUpdateResult recomputeSelectiveUpdateFlags(
            const std::function<SelectiveUpdatePolicy(const NifOsg::MatrixTransform&)>& policy,
            bool inheritedTransform);
        std::uint16_t selectiveUpdateFlags(const NifOsg::MatrixTransform& node) const;

        // Controller context query: the synthetic OSG ancestors of this root
        // must not be treated as native parents by a world-space controller.
        static bool isReferenceRoot(const osg::NodeVisitor* visitor, const osg::Node* node) noexcept;
        // Only an object-local root has this explicit parent. World-space root
        // controllers must use it instead of synthetic OSG ancestors.
        static std::optional<Nif::NiTransform> nativeRootParentWorld(
            const osg::NodeVisitor* visitor, const osg::Node* node) noexcept;
        static std::optional<Mode> updateMode(const osg::NodeVisitor* visitor) noexcept;

    private:
        NifSceneTraversal(NifOsg::MatrixTransform& objectRoot, const Nif::NiTransform& nativeParentWorld,
            Nif::NativeTransformPrecision precision, bool deferred);
        void updateImpl(const osg::FrameStamp& stamp, unsigned traversalNumber,
            const Callbacks& callbacks, bool rootOnly, Mode mode, bool updateControllers,
            const Nif::NiTransform* nativeParentWorld = nullptr);
        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };
}
#endif
