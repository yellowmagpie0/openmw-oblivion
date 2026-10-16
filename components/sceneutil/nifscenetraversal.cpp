#include "nifscenetraversal.hpp"

#include "skeleton.hpp"

#include <components/nifosg/matrixtransform.hpp>

#include <osg/FrameStamp>
#include <osg/Geode>
#include <osg/observer_ptr>
#include <osgUtil/UpdateVisitor>

#include <cmath>
#include <stdexcept>
#include <typeinfo>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace SceneUtil
{
    namespace
    {
        void require(bool condition, const char* message)
        {
            if (!condition) throw std::invalid_argument(message);
        }

        osg::Matrixd inverse(const osg::Matrixd& matrix)
        {
            for (unsigned i = 0; i < 16; ++i)
                require(std::isfinite(matrix.ptr()[i]), "nonfinite NIF scene parent");
            osg::Matrixd result;
            require(result.invert(matrix), "singular NIF scene parent");
            for (unsigned i = 0; i < 16; ++i)
                require(std::isfinite(result.ptr()[i]), "nonfinite NIF scene parent inverse");
            return result;
        }
    }

    struct NifSceneTraversal::Impl
    {
        struct Node
        {
            osg::observer_ptr<NifOsg::MatrixTransform> mTransform;
            std::optional<std::size_t> mParent;
            std::vector<std::size_t> mChildren;
            Nif::NiTransform mWorld;
            NifOsg::MatrixTransform::TraversalMatrix mPublished;
        };
        struct Group
        {
            osg::observer_ptr<osg::Group> mGroup;
            std::vector<osg::observer_ptr<osg::Node>> mChildren;
        };
        struct Visitor;
        struct Placement
        {
            osg::NodePath mPath;
            std::vector<osg::ref_ptr<osg::Node>> mLeases;
            osg::Matrixd mWorld;
        };
        struct Projection
        {
            NifBullet::RagdollBonePoseWriteback mFrame;
            std::optional<std::uint16_t> mFlags;
        };
        osg::observer_ptr<NifOsg::MatrixTransform> mRoot;
        Nif::NativeTransformPrecision mPrecision;
        std::vector<Node> mNodes;
        std::vector<Group> mGroups;
        std::unordered_map<const osg::Node*, std::size_t> mIndices;
        bool mUpdating = false;

        Impl(NifOsg::MatrixTransform& root, Nif::NativeTransformPrecision precision)
            : mRoot(&root), mPrecision(precision)
        {
            require(precision == Nif::NativeTransformPrecision::Single
                    || precision == Nif::NativeTransformPrecision::Double, "unknown NIF scene precision");
            std::unordered_set<const osg::Node*> visited;
            const auto walk = [&](auto&& self, osg::Node& node, std::optional<std::size_t> parent) -> void {
                if (!visited.insert(&node).second)
                {
                    require(node.asDrawable() && !node.getUpdateCallback(), "shared NIF update hierarchy");
                    return;
                }
                if (node.asTransform())
                {
                    // Billboard/absolute/switch/LOD semantics require their own
                    // native policies; do not silently use ordinary-node rules.
                    require(typeid(node) == typeid(NifOsg::MatrixTransform), "unsupported NIF scene transform");
                    auto& transform = static_cast<NifOsg::MatrixTransform&>(node);
                    require(transform.getReferenceFrame() == osg::Transform::RELATIVE_RF
                            && transform.getNumParents() <= 1, "ambiguous NIF scene transform");
                    require(!transform.getTraversalMatrix().getMatrix(), "NIF scene transform is already projected");
                    const auto local = transform.getControllerTransform();
                    auto world = parent ? Nif::tryComposeNativeTransform(mNodes[*parent].mWorld, local, precision)
                                        : std::optional<Nif::NiTransform>(local);
                    require(world.has_value(), "invalid initial NIF scene composition");
                    const auto index = mNodes.size();
                    mIndices.emplace(&node, index);
                    mNodes.push_back({&transform, parent, {}, *world, {}});
                    if (parent) mNodes[*parent].mChildren.push_back(index);
                    parent = index;
                }
                if (auto* group = node.asGroup())
                {
                    require(group == &root || group->getNumParents() == 1, "shared NIF scene group");
                    require(typeid(node) == typeid(NifOsg::MatrixTransform)
                            || typeid(node) == typeid(osg::Group) || typeid(node) == typeid(osg::Geode),
                        "unsupported NIF scene group policy");
                    Group saved{group, {}};
                    for (unsigned i = 0; i < group->getNumChildren(); ++i)
                        saved.mChildren.emplace_back(group->getChild(i));
                    mGroups.push_back(std::move(saved));
                    for (unsigned i = 0; i < group->getNumChildren(); ++i)
                        self(self, *group->getChild(i), parent);
                }
            };
            walk(walk, root, {});
            const auto path = externalPath();
            const auto external = osg::computeLocalToWorld(path);
            // Complete all validation/allocation before changing the graph.
            std::vector<NifOsg::MatrixTransform::TraversalMatrix> prepared;
            prepared.reserve(mNodes.size());
            for (const auto& entry : mNodes)
            {
                const osg::Matrixd parent = entry.mParent
                    ? osg::Matrixd(mNodes[*entry.mParent].mWorld.toMatrix()) : external;
                prepared.emplace_back(osg::Matrixd(entry.mWorld.toMatrix()) * inverse(parent));
            }
            for (std::size_t i = 0; i < prepared.size(); ++i)
            {
                mNodes[i].mTransform->setTraversalMatrix(prepared[i]);
                mNodes[i].mPublished = prepared[i];
            }
            invalidateSkeletons(path);
        }

        osg::NodePath externalPath() const
        {
            require(mRoot.valid(), "expired NIF scene root");
            osg::NodePath path;
            std::unordered_set<const osg::Node*> visited{mRoot.get()};
            const osg::Node* node = mRoot.get();
            while (node->getNumParents())
            {
                require(node->getNumParents() == 1, "ambiguous NIF scene placement");
                node = node->getParent(0);
                require(visited.insert(node).second, "cyclic NIF scene placement");
                path.insert(path.begin(), const_cast<osg::Node*>(node));
            }
            return path;
        }

        static void invalidateSkeletons(const osg::NodePath& path)
        {
            for (auto* node : path)
                if (auto* skeleton = dynamic_cast<Skeleton*>(node)) skeleton->invalidateBoneMatrices();
        }

        bool valid() const noexcept
        {
            if (!mRoot.valid()) return false;
            for (const auto& entry : mNodes)
                if (!entry.mTransform.valid() || entry.mTransform->getReferenceFrame() != osg::Transform::RELATIVE_RF
                    || entry.mTransform->getNumParents() > 1
                    || entry.mTransform->getTraversalMatrix() != entry.mPublished) return false;
            for (const auto& entry : mGroups)
            {
                if (!entry.mGroup.valid() || entry.mGroup->getNumChildren() != entry.mChildren.size()) return false;
                if (entry.mGroup.get() != mRoot.get() && entry.mGroup->getNumParents() != 1) return false;
                for (std::size_t i = 0; i < entry.mChildren.size(); ++i)
                    if (!entry.mChildren[i].valid() || entry.mGroup->getChild(i) != entry.mChildren[i].get()) return false;
            }
            return true;
        }

        Placement placement() const
        {
            auto path = externalPath();
            std::vector<osg::ref_ptr<osg::Node>> leases(path.begin(), path.end());
            const auto world = osg::computeLocalToWorld(path);
            inverse(world);
            return {std::move(path), std::move(leases), world};
        }

        void requirePlacement(const Placement& placement) const
        {
            require(externalPath() == placement.mPath
                    && osg::computeLocalToWorld(placement.mPath) == placement.mWorld,
                "NIF scene placement changed outside reference update");
        }

        void reanchor(const Placement& placement)
        {
            mRoot->setTraversalMatrix(osg::Matrixd(mNodes[0].mWorld.toMatrix()) * inverse(placement.mWorld));
            mNodes[0].mPublished = mRoot->getTraversalMatrix();
            invalidateSkeletons(placement.mPath);
        }

        Projection project(std::size_t index, const Callbacks& callbacks, const osg::Matrixd& external,
            const osg::NodePath& path)
        {
            auto& entry = mNodes[index];
            auto& node = *entry.mTransform;
            const auto local = node.getControllerTransform();
            const auto* parent = entry.mParent ? &mNodes[*entry.mParent].mWorld : nullptr;
            const auto world = parent ? Nif::tryComposeNativeTransform(*parent, local, mPrecision)
                                      : std::optional<Nif::NiTransform>(local);
            require(world.has_value(), "invalid NIF scene composition");
            auto flags = callbacks.mCollisionFlags ? callbacks.mCollisionFlags(node) : std::nullopt;
            require(valid(), "NIF collision provider changed the bound hierarchy");
            const bool pending = flags && (*flags & 0x40);
            require(!pending || bool(callbacks.mFirstSynchronization), "missing pending NIF first synchronization");
            NifBullet::RagdollBonePoseWriteback frame{local, *world, 0};
            if (!pending && flags && (*flags & 1))
            {
                require(bool(callbacks.mPhysicalWorld), "missing active NIF physical pose source");
                const auto desired = callbacks.mPhysicalWorld(node);
                require(valid(), "NIF physical provider changed the bound hierarchy");
                frame = NifBullet::ragdollBonePoseWriteback(local, *world, desired,
                    parent, *flags, false, mPrecision);
            }
            const osg::Matrixd parentMatrix = parent ? osg::Matrixd(parent->toMatrix()) : external;
            NifOsg::MatrixTransform::TraversalMatrix own(
                osg::Matrixd(frame.mWorld.toMatrix()) * inverse(parentMatrix));
            std::vector<NifOsg::MatrixTransform::TraversalMatrix> children;
            children.reserve(entry.mChildren.size());
            const auto inverseWorld = inverse(osg::Matrixd(frame.mWorld.toMatrix()));
            for (const auto child : entry.mChildren)
                children.emplace_back(osg::Matrixd(mNodes[child].mWorld.toMatrix()) * inverseWorld);
            // Retain author/controller state unless actual active bit8 writes it.
            if (!pending && flags && ((*flags & 9) == 9))
            {
                node.setRotation(frame.mLocal.mRotation);
                node.setTranslation(frame.mLocal.mTranslation);
            }
            entry.mWorld = frame.mWorld;
            node.setTraversalMatrix(own);
            entry.mPublished = own;
            for (std::size_t i = 0; i < children.size(); ++i)
            {
                mNodes[entry.mChildren[i]].mTransform->setTraversalMatrix(children[i]);
                mNodes[entry.mChildren[i]].mPublished = children[i];
            }
            invalidateSkeletons(path);
            if (pending)
            {
                // Original897510 first publishes the animated node world,
                // invokes reset/scene-to-body and clears40. It returns without
                // an ordinary physical-to-scene read, even for active bit0.
                const auto expected = std::uint16_t(*flags & ~0x40u);
                callbacks.mFirstSynchronization(node, frame.mWorld, *flags);
                require(valid(), "NIF first synchronization changed the bound hierarchy");
                flags = callbacks.mCollisionFlags(node);
                require(valid(), "NIF settled collision provider changed the bound hierarchy");
                require(flags && *flags == expected, "NIF first synchronization did not settle collision flags");
            }
            if (callbacks.mProjected) callbacks.mProjected(node, frame, flags);
            return {frame, flags};
        }
    };

    struct NifSceneTraversal::Impl::Visitor : osgUtil::UpdateVisitor
    {
        Impl& mBinding;
        const Callbacks& mCallbacks;
        Placement& mPlacement;
        bool mRunningControllers = false;
        Visitor(Impl& binding, const Callbacks& callbacks, Placement& placement)
            : mBinding(binding), mCallbacks(callbacks), mPlacement(placement)
        {
            setTraversalMode(TRAVERSE_ALL_CHILDREN);
            setNodeMaskOverride(~0u);
        }
        void visit(osg::Node& node)
        {
            if (mRunningControllers) return;
            osg::ref_ptr<osg::Node> lease(&node);
            osg::ref_ptr<osg::Callback> callbackLease(node.getUpdateCallback());
            {
                struct Restore
                {
                    Visitor& mVisitor;
                    TraversalMode mMode;
                    ~Restore() { mVisitor.setTraversalMode(mMode); mVisitor.mRunningControllers = false; }
                } restore{*this, getTraversalMode()};
                mRunningControllers = true;
                setTraversalMode(TRAVERSE_NONE);
                handle_callbacks_and_traverse(node);
            }
            require(mBinding.valid(), "NIF controller changed the bound hierarchy");
            mBinding.requirePlacement(mPlacement);
            const auto found = mBinding.mIndices.find(&node);
            if (found != mBinding.mIndices.end())
            {
                const auto projection = mBinding.project(found->second, mCallbacks, mPlacement.mWorld, getNodePath());
                require(mBinding.valid(), "NIF publisher changed the bound hierarchy");
                mBinding.requirePlacement(mPlacement);
                if (found->second == 0 && mCallbacks.mReferenceUpdate)
                {
                    mCallbacks.mReferenceUpdate(static_cast<NifOsg::MatrixTransform&>(node),
                        projection.mFrame, projection.mFlags);
                    require(mBinding.valid(), "NIF reference update changed the bound hierarchy");
                    auto placement = mBinding.placement();
                    mBinding.reanchor(placement);
                    // NodeVisitor paths borrow their nodes. Switch paths before
                    // releasing the previous ancestors, including old Skeletons.
                    while (!getNodePath().empty()) popFromNodePath();
                    for (auto* ancestor : placement.mPath) pushOntoNodePath(ancestor);
                    pushOntoNodePath(&node);
                    Impl::invalidateSkeletons(mPlacement.mPath);
                    mPlacement = std::move(placement);
                }
            }
            require(mBinding.valid(), "NIF publisher changed the bound hierarchy");
            mBinding.requirePlacement(mPlacement);
            traverse(node);
        }
        void apply(osg::Node& node) override { visit(node); }
        void apply(osg::Group& node) override { visit(node); }
        void apply(osg::Transform& node) override { visit(node); }
        void apply(osg::Geode& node) override { visit(node); }
    };

    NifSceneTraversal::NifSceneTraversal(NifOsg::MatrixTransform& root, Nif::NativeTransformPrecision precision)
        : mImpl(std::make_unique<Impl>(root, precision)) {}
    NifSceneTraversal::~NifSceneTraversal() = default;
    bool NifSceneTraversal::isValid() const noexcept { return mImpl->valid(); }
    Nif::NiTransform NifSceneTraversal::cachedWorld(const NifOsg::MatrixTransform& node) const
    {
        const auto found = mImpl->mIndices.find(&node);
        require(found != mImpl->mIndices.end(), "foreign NIF scene cache query");
        return mImpl->mNodes[found->second].mWorld;
    }
    bool NifSceneTraversal::isReferenceRoot(const osg::NodeVisitor* visitor, const osg::Node* node) noexcept
    {
        const auto* native = dynamic_cast<const Impl::Visitor*>(visitor);
        return native && native->mBinding.mRoot.get() == node;
    }
    void NifSceneTraversal::update(const osg::FrameStamp& stamp, unsigned traversalNumber, const Callbacks& callbacks)
    {
        updateImpl(stamp, traversalNumber, callbacks, false);
    }
    void NifSceneTraversal::refreshReferenceRootCache()
    {
        updateImpl(osg::FrameStamp{}, 0, {}, true);
    }
    void NifSceneTraversal::updateImpl(const osg::FrameStamp& stamp, unsigned traversalNumber,
        const Callbacks& callbacks, bool rootOnly)
    {
        require(mImpl->valid(), "expired or changed NIF scene hierarchy");
        require(!mImpl->mUpdating, "reentrant NIF scene traversal");
        struct Reset
        {
            bool& mUpdating;
            ~Reset() { mUpdating = false; }
        } reset{mImpl->mUpdating};
        mImpl->mUpdating = true;
        osg::ref_ptr<NifOsg::MatrixTransform> root;
        require(mImpl->mRoot.lock(root), "expired NIF scene root");
        // NodeVisitor's NodePath is borrowed. A callback can detach this root
        // and release its ancestors before the final placement guard rejects
        // the pass, including Skeletons used during projection invalidation.
        auto placement = mImpl->placement();
        mImpl->reanchor(placement);
        if (rootOnly)
            mImpl->project(0, callbacks, placement.mWorld, placement.mPath);
        else
        {
            Impl::Visitor visitor(*mImpl, callbacks, placement);
            visitor.setFrameStamp(new osg::FrameStamp(stamp));
            visitor.setTraversalNumber(traversalNumber);
            for (auto* node : placement.mPath) visitor.pushOntoNodePath(node);
            mImpl->mRoot->accept(visitor);
        }
        require(mImpl->valid(), "NIF publisher changed the bound hierarchy");
        mImpl->requirePlacement(placement);
    }
}
