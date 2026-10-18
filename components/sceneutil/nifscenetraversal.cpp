#include "nifscenetraversal.hpp"

#include "skeleton.hpp"

#include <components/nifosg/matrixtransform.hpp>

#include <osg/FrameStamp>
#include <osg/Geode>
#include <osg/observer_ptr>
#include <osgUtil/UpdateVisitor>

#include <cmath>
#include <bit>
#include <cstdint>
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
            std::optional<std::uint16_t> mSelectiveFlags;
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
        struct InitialLocal
        {
            osg::Matrixd mMatrix;
            Nif::Matrix3 mRotationScale;
            float mScale;
        };
        struct Preparation
        {
            Placement mPlacement;
            std::vector<InitialLocal> mLocals;
            std::vector<NifOsg::MatrixTransform::TraversalMatrix> mProjections;
        };
        osg::observer_ptr<NifOsg::MatrixTransform> mRoot;
        Nif::NativeTransformPrecision mPrecision;
        std::vector<Node> mNodes;
        std::vector<Group> mGroups;
        std::unordered_map<const osg::Node*, std::size_t> mIndices;
        std::optional<Nif::NiTransform> mNativeParentWorld;
        std::optional<Preparation> mPreparation;
        bool mPublished = false;
        bool mRetired = false;
        bool mUpdating = false;

        Impl(NifOsg::MatrixTransform& root, Nif::NativeTransformPrecision precision,
            const Nif::NiTransform* nativeParentWorld = nullptr, bool deferred = false)
            : mRoot(&root), mPrecision(precision)
            , mNativeParentWorld(nativeParentWorld ? std::optional(*nativeParentWorld) : std::nullopt)
        {
            require(precision == Nif::NativeTransformPrecision::Single
                    || precision == Nif::NativeTransformPrecision::Double, "unknown NIF scene precision");
            std::unordered_set<const osg::Node*> visited;
            std::vector<InitialLocal> locals;
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
                    locals.push_back({transform.getMatrix(), transform.mRotationScale, transform.mScale});
                    const auto* parentWorld = parent ? &mNodes[*parent].mWorld
                        : mNativeParentWorld ? &*mNativeParentWorld : nullptr;
                    auto world = parentWorld ? Nif::tryComposeNativeTransform(*parentWorld, local, precision)
                                            : std::optional<Nif::NiTransform>(local);
                    require(world.has_value(), "invalid initial NIF scene composition");
                    const auto index = mNodes.size();
                    mIndices.emplace(&node, index);
                    mNodes.push_back({&transform, parent, {}, *world, {}, {}});
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
            auto initialPlacement = placement();
            // Complete all validation/allocation before changing the graph.
            std::vector<NifOsg::MatrixTransform::TraversalMatrix> prepared;
            prepared.reserve(mNodes.size());
            for (const auto& entry : mNodes)
            {
                const osg::Matrixd parent = entry.mParent
                    ? osg::Matrixd(mNodes[*entry.mParent].mWorld.toMatrix()) : initialPlacement.mWorld;
                prepared.emplace_back(osg::Matrixd(entry.mWorld.toMatrix()) * inverse(parent));
            }
            mPreparation.emplace(Preparation{std::move(initialPlacement), std::move(locals), std::move(prepared)});
            if (!deferred) require(commitPrepared(), "changed initial NIF scene preparation");
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

        static void invalidateCurrentSkeletons(osg::Node* start) noexcept
        {
            const auto parent = [](osg::Node* node) {
                return node && node->getNumParents() == 1 ? node->getParent(0) : nullptr;
            };
            osg::Node* slow = start;
            osg::Node* fast = start;
            do
            {
                slow = parent(slow);
                fast = parent(parent(fast));
            } while (fast && fast != slow);
            osg::Node* cycle = fast ? slow : nullptr;
            bool seenCycle = false;
            for (osg::Node* node = start; node; node = parent(node))
            {
                if (node == cycle)
                {
                    if (seenCycle) break;
                    seenCycle = true;
                }
                if (auto* skeleton = dynamic_cast<Skeleton*>(node)) skeleton->invalidateBoneMatrices();
            }
        }

        bool valid() const noexcept
        {
            if (mRetired || !mRoot.valid()) return false;
            for (const auto& entry : mNodes)
                if (!entry.mTransform.valid() || entry.mTransform->getReferenceFrame() != osg::Transform::RELATIVE_RF
                    || entry.mTransform->getNumParents() > 1
                    || !entry.mTransform->getTraversalMatrix().sharesStorageWith(entry.mPublished)) return false;
            for (const auto& entry : mGroups)
            {
                if (!entry.mGroup.valid() || entry.mGroup->getNumChildren() != entry.mChildren.size()) return false;
                if (entry.mGroup.get() != mRoot.get() && entry.mGroup->getNumParents() != 1) return false;
                for (std::size_t i = 0; i < entry.mChildren.size(); ++i)
                    if (!entry.mChildren[i].valid() || entry.mGroup->getChild(i) != entry.mChildren[i].get()) return false;
            }
            if (mPreparation)
            {
                for (std::size_t i = 0; i < mNodes.size(); ++i)
                {
                    const auto& node = *mNodes[i].mTransform;
                    const auto& saved = mPreparation->mLocals[i];
                    for (unsigned j = 0; j < 16; ++j)
                        if (std::bit_cast<std::uint64_t>(node.getMatrix().ptr()[j])
                            != std::bit_cast<std::uint64_t>(saved.mMatrix.ptr()[j])) return false;
                    if (std::bit_cast<std::uint32_t>(node.mScale)
                        != std::bit_cast<std::uint32_t>(saved.mScale)) return false;
                    for (unsigned row = 0; row < 3; ++row)
                        for (unsigned column = 0; column < 3; ++column)
                            if (std::bit_cast<std::uint32_t>(node.mRotationScale.mValues[row][column])
                                != std::bit_cast<std::uint32_t>(saved.mRotationScale.mValues[row][column])) return false;
                }
                // The retained path leases every ancestor. Compare parent
                // links without allocating a new NodePath in admission.
                const auto& saved = mPreparation->mPlacement;
                const osg::Node* node = mRoot.get();
                for (auto i = saved.mPath.rbegin(); i != saved.mPath.rend(); ++i)
                {
                    if (node->getNumParents() != 1 || node->getParent(0) != *i) return false;
                    node = *i;
                }
                if (node->getNumParents()) return false;
                try
                {
                    if (osg::computeLocalToWorld(saved.mPath) != saved.mWorld) return false;
                }
                catch (...) { return false; }
            }
            return true;
        }

        bool commitPrepared() noexcept
        {
            if (mUpdating || mPublished || !mPreparation || !valid()) return false;
            for (std::size_t i = 0; i < mNodes.size(); ++i)
            {
                mNodes[i].mTransform->setTraversalMatrix(mPreparation->mProjections[i]);
                mNodes[i].mPublished = mPreparation->mProjections[i];
            }
            invalidateSkeletons(mPreparation->mPlacement.mPath);
            mPublished = true;
            mPreparation.reset();
            return true;
        }

        bool retire() noexcept
        {
            if (mUpdating || mRetired) return false;
            if (mPublished)
                for (auto& entry : mNodes)
                    if (entry.mTransform.valid() && entry.mTransform->getTraversalMatrix().sharesStorageWith(entry.mPublished))
                    {
                        entry.mTransform->clearTraversalMatrix();
                        // A detached child can now belong to another Skeleton.
                        // Avoid allocation and terminate even on damaged cycles.
                        invalidateCurrentSkeletons(entry.mTransform.get());
                    }
            mRetired = true;
            mPreparation.reset();
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
            const auto* parent = entry.mParent ? &mNodes[*entry.mParent].mWorld
                : mNativeParentWorld ? &*mNativeParentWorld : nullptr;
            const auto world = parent ? Nif::tryComposeNativeTransform(*parent, local, mPrecision)
                                      : std::optional<Nif::NiTransform>(local);
            require(world.has_value(), "invalid NIF scene composition");
            CollisionResult collision;
            if (callbacks.mCollision)
            {
                collision = callbacks.mCollision(node, *world);
                require(valid(), "NIF collision dispatch changed the bound hierarchy");
                require(!collision.mPhysicalTarget
                        || (collision.mFlags && (*collision.mFlags & 1) && !(*collision.mFlags & 0x40)),
                    "NIF collision target requires settled active flags");
            }
            else
            {
                if (callbacks.mCollisionUpdate)
                {
                    callbacks.mCollisionUpdate(node, *world);
                    require(valid(), "NIF collision update changed the bound hierarchy");
                }
                collision.mFlags = callbacks.mCollisionFlags ? callbacks.mCollisionFlags(node) : std::nullopt;
            }
            auto flags = collision.mFlags;
            require(valid(), "NIF collision provider changed the bound hierarchy");
            const bool pending = flags && (*flags & 0x40);
            require(!pending || bool(callbacks.mCollision) || bool(callbacks.mFirstSynchronization),
                "missing pending NIF first synchronization");
            NifBullet::RagdollBonePoseWriteback frame{local, *world, 0};
            bool physicalTarget = false;
            if (!pending && flags && (*flags & 1))
            {
                require(bool(callbacks.mCollision)
                        || bool(callbacks.mPhysicalWorld) != bool(callbacks.mPhysicalTarget),
                    "missing or ambiguous active NIF physical pose source");
                const auto desired = callbacks.mCollision ? collision.mPhysicalTarget
                    : callbacks.mPhysicalTarget ? callbacks.mPhysicalTarget(node)
                                               : std::optional(callbacks.mPhysicalWorld(node));
                require(valid(), "NIF physical provider changed the bound hierarchy");
                if (desired)
                {
                    frame = NifBullet::ragdollBonePoseWriteback(local, *world, *desired,
                        parent, *flags, false, mPrecision);
                    physicalTarget = true;
                }
            }
            const osg::Matrixd parentMatrix = entry.mParent ? osg::Matrixd(parent->toMatrix()) : external;
            NifOsg::MatrixTransform::TraversalMatrix own(
                osg::Matrixd(frame.mWorld.toMatrix()) * inverse(parentMatrix));
            std::vector<NifOsg::MatrixTransform::TraversalMatrix> children;
            children.reserve(entry.mChildren.size());
            const auto inverseWorld = inverse(osg::Matrixd(frame.mWorld.toMatrix()));
            for (const auto child : entry.mChildren)
                children.emplace_back(osg::Matrixd(mNodes[child].mWorld.toMatrix()) * inverseWorld);
            // Retain author/controller state unless actual active bit8 writes it.
            if (physicalTarget && (*flags & 8))
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
            if (pending && !callbacks.mCollision)
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
        Mode mMode;
        const bool mUpdateControllers;
        bool mRunProperties = true;
        bool mRunningControllers = false;
        Visitor(Impl& binding, const Callbacks& callbacks, Placement& placement, Mode mode, bool updateControllers)
            : mBinding(binding), mCallbacks(callbacks), mPlacement(placement), mMode(mode)
            , mUpdateControllers(updateControllers)
        {
            setTraversalMode(TRAVERSE_ALL_CHILDREN);
            setNodeMaskOverride(~0u);
        }
        std::uint16_t nodeFlags(const NifOsg::MatrixTransform& node)
        {
            const auto flags = mCallbacks.mNodeFlags ? mCallbacks.mNodeFlags(node)
                : *mBinding.mNodes[mBinding.mIndices.at(&node)].mSelectiveFlags;
            require(mBinding.valid(), "NIF update policy changed the bound hierarchy");
            mBinding.requirePlacement(mPlacement);
            return flags;
        }
        void visit(osg::Node& node)
        {
            if (mRunningControllers) return;
            osg::ref_ptr<osg::Node> lease(&node);
            osg::ref_ptr<osg::Callback> callbackLease(node.getUpdateCallback());
            struct RestorePolicy
            {
                Visitor& mVisitor;
                Mode mMode;
                bool mRunProperties;
                ~RestorePolicy() { mVisitor.mMode = mMode; mVisitor.mRunProperties = mRunProperties; }
            } restorePolicy{*this, mMode, mRunProperties};
            const auto found = mBinding.mIndices.find(&node);
            if (mMode != Mode::Full && found != mBinding.mIndices.end())
            {
                const auto flags = nodeFlags(static_cast<NifOsg::MatrixTransform&>(node));
                if (found->second != 0)
                {
                    if (!(flags & 2)) return;
                    if (flags & 0x10) mMode = Mode::Rigid;
                }
                mRunProperties = flags & 8;
            }
            if (mUpdateControllers)
            {
                struct Restore
                {
                    Visitor& mVisitor;
                    TraversalMode mMode;
                    ~Restore() { mVisitor.setTraversalMode(mMode); mVisitor.mRunningControllers = false; }
                } restore{*this, getTraversalMode()};
                mRunningControllers = true;
                setTraversalMode(TRAVERSE_NONE);
                if (mRunProperties) handle_callbacks(node.getStateSet());
                // Native47C930 reads the node's controller list after property
                // controllers; a property callback may replace that list.
                osg::ref_ptr<osg::Callback> currentCallback(node.getUpdateCallback());
                if (currentCallback) currentCallback->run(&node, this);
            }
            require(mBinding.valid(), "NIF controller changed the bound hierarchy");
            mBinding.requirePlacement(mPlacement);
            if (found != mBinding.mIndices.end()
                && (mMode == Mode::Full || (nodeFlags(static_cast<NifOsg::MatrixTransform&>(node)) & 4)))
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
        void apply(osg::Drawable& drawable) override
        {
            if (mRunningControllers || !mUpdateControllers) return;
            // Preserve OSG's drawable callback ordering and callback forms,
            // while applying the containing native node's property decision.
            osg::ref_ptr<osg::Drawable> lease(&drawable);
            osg::ref_ptr<osg::Callback> callback(drawable.getUpdateCallback());
            if (callback)
            {
                auto* update = callback->asDrawableUpdateCallback();
                auto* node = callback->asNodeCallback();
                if (update) update->update(this, &drawable);
                if (node) (*node)(&drawable, this);
                if (!update && !node) callback->run(&drawable, this);
            }
            if (mRunProperties) handle_callbacks(drawable.getStateSet());
            require(mBinding.valid(), "NIF drawable callback changed the bound hierarchy");
            mBinding.requirePlacement(mPlacement);
        }
    };

    NifSceneTraversal::NifSceneTraversal(NifOsg::MatrixTransform& root, Nif::NativeTransformPrecision precision)
        : mImpl(std::make_unique<Impl>(root, precision)) {}
    NifSceneTraversal::NifSceneTraversal(NifOsg::MatrixTransform& root, const Nif::NiTransform& nativeParentWorld,
        Nif::NativeTransformPrecision precision)
        : mImpl(std::make_unique<Impl>(root, precision, &nativeParentWorld)) {}
    NifSceneTraversal::NifSceneTraversal(NifOsg::MatrixTransform& root, const Nif::NiTransform& nativeParentWorld,
        Nif::NativeTransformPrecision precision, bool deferred)
        : mImpl(std::make_unique<Impl>(root, precision, &nativeParentWorld, deferred)) {}
    std::unique_ptr<NifSceneTraversal> NifSceneTraversal::prepareObjectLocal(NifOsg::MatrixTransform& root,
        const Nif::NiTransform& nativeParentWorld, Nif::NativeTransformPrecision precision)
    {
        return std::unique_ptr<NifSceneTraversal>(new NifSceneTraversal(root, nativeParentWorld, precision, true));
    }
    bool NifSceneTraversal::commitPrepared() noexcept { return mImpl->commitPrepared(); }
    bool NifSceneTraversal::retire() noexcept { return mImpl->retire(); }
    NifSceneTraversal::~NifSceneTraversal() = default;
    bool NifSceneTraversal::isValid() const noexcept { return mImpl->valid(); }
    Nif::NiTransform NifSceneTraversal::cachedWorld(const NifOsg::MatrixTransform& node) const
    {
        const auto found = mImpl->mIndices.find(&node);
        require(found != mImpl->mIndices.end(), "foreign NIF scene cache query");
        return mImpl->mNodes[found->second].mWorld;
    }
    NifSceneTraversal::SelectiveUpdateResult NifSceneTraversal::recomputeSelectiveUpdateFlags(
        const std::function<SelectiveUpdatePolicy(const NifOsg::MatrixTransform&)>& policy,
        bool inheritedTransform)
    {
        require(mImpl->valid(), "expired or changed NIF scene hierarchy");
        require(bool(policy), "missing NIF selective policy resolver");
        require(!mImpl->mUpdating, "reentrant NIF scene policy preparation");
        struct Reset
        {
            bool& mUpdating;
            ~Reset() { mUpdating = false; }
        } reset{mImpl->mUpdating};
        mImpl->mUpdating = true;
        const auto placement = mImpl->placement();
        std::vector<osg::ref_ptr<NifOsg::MatrixTransform>> leases;
        for (const auto& node : mImpl->mNodes)
        {
            osg::ref_ptr<NifOsg::MatrixTransform> lease;
            require(node.mTransform.lock(lease), "expired NIF selective policy node");
            leases.push_back(std::move(lease));
        }
        std::vector<SelectiveUpdatePolicy> policies;
        for (std::size_t i = 0; i < leases.size(); ++i)
        {
            const auto value = policy(*leases[i]);
            require(mImpl->valid(), "NIF policy resolver changed the hierarchy");
            mImpl->requirePlacement(placement);
            const auto kind = value.mKind;
            require(kind == SelectiveUpdatePolicy::Kind::Node || kind == SelectiveUpdatePolicy::Kind::AVObject
                    || kind == SelectiveUpdatePolicy::Kind::Geometry, "unknown NIF selective policy kind");
            require(!value.mHasTransformController || value.mHasController,
                "NIF transform controller requires controller presence");
            require(kind == SelectiveUpdatePolicy::Kind::Node || mImpl->mNodes[i].mChildren.empty(),
                "NIF leaf selective policy has native children");
            require(!value.mHasSkin || kind == SelectiveUpdatePolicy::Kind::Geometry,
                "NIF skin requires geometry selective policy");
            policies.push_back(value);
        }
        std::vector<std::uint16_t> flags(policies.size());
        const auto compute = [&](auto&& self, std::size_t index, bool inherited) -> SelectiveUpdateResult {
            const auto& value = policies[index];
            const bool transform = inherited || value.mHasTransformController;
            bool needs = transform || value.mHasController || value.mHasPropertyController;
            bool rigidChildren = true;
            for (const auto child : mImpl->mNodes[index].mChildren)
            {
                const auto result = self(self, child, transform);
                needs |= result.mNeedsUpdate;
                rigidChildren &= result.mRigid;
            }
            // Node bit4 describes its descendants. Own transform controllers
            // clear the rigidity returned to its parent after that flag store.
            flags[index] = (value.mFlags & ~std::uint16_t(0x1e))
                | (needs ? 2 : 0) | (transform ? 4 : 0)
                | (value.mHasPropertyController ? 8 : 0) | (rigidChildren ? 16 : 0);
            bool rigid = rigidChildren && !value.mHasTransformController;
            if (value.mHasSkin)
            {
                flags[index] |= 6;
                needs = true;
                rigid = false;
            }
            return {needs, rigid};
        };
        const auto result = compute(compute, 0, inheritedTransform);
        for (std::size_t i = 0; i < flags.size(); ++i) mImpl->mNodes[i].mSelectiveFlags = flags[i];
        return result;
    }
    std::uint16_t NifSceneTraversal::selectiveUpdateFlags(const NifOsg::MatrixTransform& node) const
    {
        require(mImpl->valid(), "expired or changed NIF scene hierarchy");
        const auto found = mImpl->mIndices.find(&node);
        require(found != mImpl->mIndices.end(), "foreign NIF selective flag query");
        const auto flags = mImpl->mNodes[found->second].mSelectiveFlags;
        require(flags.has_value(), "unprepared NIF selective flags");
        return *flags;
    }
    bool NifSceneTraversal::isReferenceRoot(const osg::NodeVisitor* visitor, const osg::Node* node) noexcept
    {
        const auto* native = dynamic_cast<const Impl::Visitor*>(visitor);
        return native && native->mBinding.mRoot.get() == node && !native->mBinding.mNativeParentWorld;
    }
    std::optional<Nif::NiTransform> NifSceneTraversal::nativeRootParentWorld(
        const osg::NodeVisitor* visitor, const osg::Node* node) noexcept
    {
        const auto* native = dynamic_cast<const Impl::Visitor*>(visitor);
        return native && native->mBinding.mRoot.get() == node ? native->mBinding.mNativeParentWorld : std::nullopt;
    }
    std::optional<NifSceneTraversal::Mode> NifSceneTraversal::updateMode(const osg::NodeVisitor* visitor) noexcept
    {
        const auto* native = dynamic_cast<const Impl::Visitor*>(visitor);
        return native ? std::optional(native->mMode) : std::nullopt;
    }
    void NifSceneTraversal::update(const osg::FrameStamp& stamp, unsigned traversalNumber,
        const Callbacks& callbacks, Mode mode)
    {
        updateImpl(stamp, traversalNumber, callbacks, false, mode, true);
    }
    void NifSceneTraversal::updateWithoutControllers(const osg::FrameStamp& stamp, unsigned traversalNumber,
        const Callbacks& callbacks)
    {
        updateImpl(stamp, traversalNumber, callbacks, false, Mode::Full, false);
    }
    void NifSceneTraversal::updateObjectLocal(const osg::FrameStamp& stamp, unsigned traversalNumber,
        const Nif::NiTransform& nativeParentWorld, const Callbacks& callbacks, Mode mode)
    {
        updateImpl(stamp, traversalNumber, callbacks, false, mode, true, &nativeParentWorld);
    }
    void NifSceneTraversal::updateObjectLocalWithoutControllers(const osg::FrameStamp& stamp, unsigned traversalNumber,
        const Nif::NiTransform& nativeParentWorld, const Callbacks& callbacks)
    {
        updateImpl(stamp, traversalNumber, callbacks, false, Mode::Full, false, &nativeParentWorld);
    }
    void NifSceneTraversal::refreshReferenceRootCache()
    {
        updateImpl(osg::FrameStamp{}, 0, {}, true, Mode::Full, false);
    }
    void NifSceneTraversal::updateImpl(const osg::FrameStamp& stamp, unsigned traversalNumber,
        const Callbacks& callbacks, bool rootOnly, Mode mode, bool updateControllers,
        const Nif::NiTransform* nativeParentWorld)
    {
        require(mImpl->valid(), "expired or changed NIF scene hierarchy");
        require(mImpl->mPublished, "unpublished NIF scene traversal");
        require(mode == Mode::Full || mode == Mode::Selected || mode == Mode::Rigid, "unknown NIF update mode");
        require(mode == Mode::Full || bool(callbacks.mNodeFlags) || mImpl->mNodes[0].mSelectiveFlags.has_value(),
            "missing NIF selected update policy");
        require(!mImpl->mUpdating, "reentrant NIF scene traversal");
        require(!callbacks.mCollision || (!callbacks.mCollisionFlags && !callbacks.mPhysicalWorld
                    && !callbacks.mFirstSynchronization && !callbacks.mCollisionUpdate && !callbacks.mPhysicalTarget),
            "ambiguous NIF collision dispatch ownership");
        require(bool(nativeParentWorld) == mImpl->mNativeParentWorld.has_value(),
            "NIF traversal native parent space mismatch");
        if (nativeParentWorld)
        {
            // Validate before changing the retained parent or drawing handles.
            const auto world = Nif::tryComposeNativeTransform(*nativeParentWorld,
                mImpl->mRoot->getControllerTransform(), mImpl->mPrecision);
            require(world.has_value(), "invalid NIF native parent composition");
            inverse(osg::Matrixd(nativeParentWorld->toMatrix()));
        }
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
        if (nativeParentWorld) mImpl->mNativeParentWorld = *nativeParentWorld;
        mImpl->reanchor(placement);
        if (rootOnly)
            mImpl->project(0, callbacks, placement.mWorld, placement.mPath);
        else
        {
            Impl::Visitor visitor(*mImpl, callbacks, placement, mode, updateControllers);
            visitor.setFrameStamp(new osg::FrameStamp(stamp));
            visitor.setTraversalNumber(traversalNumber);
            for (auto* node : placement.mPath) visitor.pushOntoNodePath(node);
            mImpl->mRoot->accept(visitor);
        }
        require(mImpl->valid(), "NIF publisher changed the bound hierarchy");
        mImpl->requirePlacement(placement);
    }
}
