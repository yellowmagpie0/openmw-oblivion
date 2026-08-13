#include "bulletnifloader.hpp"

#include <cassert>
#include <cmath>
#include <limits>
#include <sstream>
#include <tuple>
#include <variant>
#include <vector>

#include <components/debug/debuglog.hpp>
#include <components/files/conversion.hpp>
#include <components/misc/convert.hpp>
#include <components/misc/strings/algorithm.hpp>
#include <components/misc/strings/lower.hpp>
#include <components/nif/controller.hpp>
#include <components/nif/data.hpp>
#include <components/nif/extra.hpp>
#include <components/nif/nifstream.hpp>
#include <components/nif/node.hpp>
#include <components/nif/parent.hpp>
#include <components/nif/physics.hpp>

#include <BulletCollision/CollisionShapes/btBoxShape.h>
#include <BulletCollision/CollisionShapes/btConvexHullShape.h>
#include <BulletCollision/CollisionShapes/btMultiSphereShape.h>
#include <BulletCollision/CollisionShapes/btSphereShape.h>
#include <BulletCollision/CollisionShapes/btTriangleMesh.h>

namespace
{

    // NetImmerse units per Havok unit. Oblivion uses Havok 6.6.0, while
    // Fallout 3 and later Bethesda streams use the 2010 scale.
    constexpr float sHavok660Scale = 10.f / 1.42875f;
    constexpr float sHavok2010Scale = 100.f / 1.42875f;

    void addTriangle(btTriangleMesh& mesh, const std::vector<osg::Vec3f>& vertices, unsigned short a,
        unsigned short b, unsigned short c, float scale)
    {
        if (a >= vertices.size() || b >= vertices.size() || c >= vertices.size() || a == b || b == c || a == c)
            return;
        mesh.addTriangle(Misc::Convert::toBullet(vertices[a]) * scale, Misc::Convert::toBullet(vertices[b]) * scale,
            Misc::Convert::toBullet(vertices[c]) * scale);
    }

    void addStrips(btTriangleMesh& mesh, const Nif::NiTriStripsData& data, float scale)
    {
        for (const auto& strip : data.mStrips)
        {
            for (std::size_t i = 2; i < strip.size(); ++i)
            {
                if ((i & 1) == 0)
                    addTriangle(mesh, data.mVertices, strip[i - 2], strip[i - 1], strip[i], scale);
                else
                    addTriangle(mesh, data.mVertices, strip[i - 2], strip[i], strip[i - 1], scale);
            }
        }
    }

    btTransform matrixToBullet(osg::Matrixf matrix, float translationScale = 1.f)
    {
        const osg::Vec3f translation = matrix.getTrans() * translationScale;
        matrix.orthoNormalize(matrix);
        btTransform result;
        result.setOrigin(Misc::Convert::toBullet(translation));
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                result.getBasis()[i][j] = matrix(j, i);
        return result;
    }

    bool pathFileNameStartsWithX(const std::string& path)
    {
        const std::size_t slashpos = path.find_last_of("/\\");
        const std::size_t letterPos = slashpos == std::string::npos ? 0 : slashpos + 1;
        return letterPos < path.size() && (path[letterPos] == 'x' || path[letterPos] == 'X');
    }

}

namespace NifBullet
{

    osg::ref_ptr<Resource::BulletShape> BulletNifLoader::load(Nif::FileView nif)
    {
        mShape = new Resource::BulletShape;

        mCompoundShape.reset();
        mAvoidCompoundShape.reset();
        mEmbeddedAnimationNodes.clear();
        mBethesdaCollisionStats = {};
        mHavokScale = nif.getBethVersion() < Nif::NIFFile::BETHVER_FO3 ? sHavok660Scale : sHavok2010Scale;

        for (std::size_t i = 0; i < nif.numRecords(); ++i)
        {
            const Nif::Record* record = nif.getRecord(i);
            if (record == nullptr || record->mRecordType != Nif::RC_NiControllerSequence)
                continue;
            const auto* sequence = static_cast<const Nif::NiControllerSequence*>(record);
            for (const Nif::ControlledBlock& block : sequence->mControlledBlocks)
            {
                const std::string& node = block.mNodeName.empty() ? block.mTargetName : block.mNodeName;
                if (!node.empty())
                    mEmbeddedAnimationNodes.insert(Misc::StringUtils::lowerCase(node));
            }
        }

        mShape->mFileHash = nif.getHash();

        const size_t numRoots = nif.numRoots();
        std::vector<const Nif::NiAVObject*> roots;
        for (size_t i = 0; i < numRoots; ++i)
        {
            const Nif::Record* r = nif.getRoot(i);
            if (!r)
                continue;
            const Nif::NiAVObject* node = dynamic_cast<const Nif::NiAVObject*>(r);
            if (node)
                roots.emplace_back(node);
        }
        mShape->mFileName = nif.getFilename();
        if (roots.empty())
        {
            warn("Found no root nodes in NIF file " + mShape->mFileName.value());
            return mShape;
        }

        for (const Nif::NiAVObject* node : roots)
            if (findBoundingBox(*node))
                break;

        HandleNodeArgs args;

        // files with the name convention xmodel.nif usually have keyframes stored in a separate file xmodel.kf (see
        // Animation::addAnimSource). assume all nodes in the file will be animated
        // TODO: investigate whether this should and could be optimized.
        args.mAnimated = pathFileNameStartsWithX(mShape->mFileName);

        for (const Nif::NiAVObject* node : roots)
            handleRoot(nif, *node, args);

        if (mCompoundShape)
            mShape->mCollisionShape = std::move(mCompoundShape);

        if (mAvoidCompoundShape)
            mShape->mAvoidCollisionShape = std::move(mAvoidCompoundShape);

        return mShape;
    }

    // Find a bounding box in the node hierarchy to use for actor collision
    bool BulletNifLoader::findBoundingBox(const Nif::NiAVObject& node)
    {
        if (Misc::StringUtils::ciEqual(node.mName, "Bounding Box"))
        {
            if (node.mBounds.mType == Nif::BoundingVolume::Type::BOX_BV
                && std::ranges::all_of(node.mBounds.mBox.mExtents._v, [](float extent) { return extent > 0.f; }))
            {
                mShape->mCollisionBox.mExtents = node.mBounds.mBox.mExtents;
                mShape->mCollisionBox.mCenter = node.mBounds.mBox.mCenter;
            }
            else
            {
                warn("Invalid Bounding Box node bounds in file " + mShape->mFileName.value());
            }
            return true;
        }

        if (auto ninode = dynamic_cast<const Nif::NiNode*>(&node))
            for (const auto& child : ninode->mChildren)
                if (!child.empty() && findBoundingBox(child.get()))
                    return true;

        return false;
    }

    void BulletNifLoader::handleRoot(Nif::FileView nif, const Nif::NiAVObject& node, HandleNodeArgs args)
    {
        // Gamebryo/Bethbryo meshes
        if (nif.getVersion() >= Nif::NIFStream::generateVersion(10, 0, 1, 0))
        {
            // Handle BSXFlags
            const Nif::NiIntegerExtraData* bsxFlags = nullptr;
            for (const auto& e : node.getExtraList())
            {
                if (!e.empty() && e->mRecordType == Nif::RC_BSXFlags)
                {
                    bsxFlags = static_cast<const Nif::NiIntegerExtraData*>(e.getPtr());
                    break;
                }
            }

            // Collision flag
            if (!bsxFlags || !(bsxFlags->mData & 2))
                return;

            // Editor marker flag
            if (bsxFlags->mData & 32)
                args.mHasMarkers = true;

            // Bethesda meshes carry collision independently from rendered
            // geometry. handleNode follows the NiAVObject collision links.
        }
        // Pre-Gamebryo meshes
        else
        {
            bool recursiveRcn = false;
            // Check for extra data
            for (const auto& e : node.getExtraList())
            {
                if (!e.empty() && e->mRecordType == Nif::RC_NiStringExtraData)
                {
                    // String markers may contain important information
                    // affecting the entire subtree of this node
                    auto sd = static_cast<const Nif::NiStringExtraData*>(e.getPtr());

                    // Editor marker flag
                    if (sd->mData == "MRK")
                        args.mHasTriMarkers = true;
                    else if (Misc::StringUtils::ciStartsWith(sd->mData, "NC"))
                    {
                        // NC prefix is case-insensitive but the second C in NCC flag needs be uppercase.

                        // Collide only with camera.
                        if (sd->mData.length() > 2 && sd->mData[2] == 'C')
                            mShape->mVisualCollisionType = Resource::VisualCollisionType::Camera;
                        // No collision.
                        else
                            mShape->mVisualCollisionType = Resource::VisualCollisionType::Default;
                    }
                    else if (sd->mData == "RCN")
                        recursiveRcn = true;
                }
            }

            const Nif::NiNode* ninode = dynamic_cast<const Nif::NiNode*>(&node);
            if (ninode)
                args.mCollisionNode = ninode->findRootCollisionNode(recursiveRcn);
            if (!args.mCollisionNode)
                args.mGenerateCollision = true;
            else if (args.mCollisionNode->mChildren.empty())
            {
                // FIXME: BulletNifLoader should never have to provide rendered geometry for camera collision
                args.mGenerateCollision = true;
                mShape->mVisualCollisionType = Resource::VisualCollisionType::Camera;
            }
        }

        handleNode(node, nullptr, args);
    }

    void BulletNifLoader::handleNode(const Nif::NiAVObject& node, const Nif::Parent* parent, HandleNodeArgs args)
    {
        if (mEmbeddedAnimationNodes.contains(Misc::StringUtils::lowerCase(node.mName)))
            args.mAnimated = true;

        // TODO: allow on-the fly collision switching via toggling this flag
        if (node.mRecordType == Nif::RC_NiCollisionSwitch && !node.collisionActive())
            return;

        for (Nif::NiTimeControllerPtr ctrl = node.mController; !ctrl.empty(); ctrl = ctrl->mNext)
        {
            if (args.mAnimated)
                break;
            if (!ctrl->isActive())
                continue;
            switch (ctrl->mRecordType)
            {
                case Nif::RC_NiKeyframeController:
                case Nif::RC_NiPathController:
                case Nif::RC_NiRollController:
                    args.mAnimated = true;
                    break;
                default:
                    continue;
            }
        }

        if (node.mRecordType == Nif::RC_RootCollisionNode)
        {
            // Encountered our RootCollisionNode inside an autogenerated mesh.
            // We treat empty RootCollisionNodes as NCC flag (set collisionType to `Camera`)
            // and generate the camera collision shape based on rendered geometry.
            if (args.mCollisionNode == &node && args.mGenerateCollision
                && mShape->mVisualCollisionType == Resource::VisualCollisionType::Camera)
                return;

            // Standard handling
            if (!args.mCollisionNode)
            {
                Log(Debug::Info) << "BulletNifLoader: Unexpected RootCollisionNode in " << mShape->mFileName
                                 << ". Treating as visible geometry.";
            }
            else if (args.mCollisionNode != &node)
            {
                Log(Debug::Info) << "BulletNifLoader: Extra RootCollisionNode in " << mShape->mFileName
                                 << ". Treating as visible geometry.";
            }
            else
            {
                args.mGenerateCollision = true;
            }
        }

        // Don't collide with AvoidNode shapes
        if (node.mRecordType == Nif::RC_AvoidNode)
            args.mAvoid = true;

        if (args.mGenerateCollision)
        {
            auto geometry = dynamic_cast<const Nif::NiGeometry*>(&node);
            if (geometry)
                handleGeometry(*geometry, parent, args);
        }

        if (!node.mCollision.empty())
            handleBethesdaCollision(node, parent, args);

        // For NiNodes, loop through children
        if (const Nif::NiNode* ninode = dynamic_cast<const Nif::NiNode*>(&node))
        {
            const Nif::Parent currentParent{ *ninode, parent };
            for (const auto& child : ninode->mChildren)
            {
                if (!child.empty())
                {
                    assert(std::find(child->mParents.begin(), child->mParents.end(), ninode) != child->mParents.end());
                    handleNode(child.get(), &currentParent, args);
                }
                // For NiSwitchNodes and NiFltAnimationNodes, only use the first child
                // TODO: must synchronize with the rendering scene graph somehow
                // Doing this for NiLODNodes is unsafe (the first level might not be the closest)
                if (node.mRecordType == Nif::RC_NiSwitchNode || node.mRecordType == Nif::RC_NiFltAnimationNode)
                    break;
            }
        }
    }

    void BulletNifLoader::handleGeometry(
        const Nif::NiGeometry& niGeometry, const Nif::Parent* nodeParent, HandleNodeArgs args)
    {
        // This flag comes from BSXFlags
        if (args.mHasMarkers && Misc::StringUtils::ciStartsWith(niGeometry.mName, "EditorMarker"))
            return;

        // This flag comes from Morrowind
        if (args.mHasTriMarkers && Misc::StringUtils::ciStartsWith(niGeometry.mName, "Tri EditorMarker"))
            return;

        if (!niGeometry.mSkin.empty())
            args.mAnimated = false;

        std::unique_ptr<btCollisionShape> childShape = niGeometry.getCollisionShape();
        if (childShape == nullptr)
            return;

        osg::Matrixf transform = niGeometry.mTransform.toMatrix();
        for (const Nif::Parent* parent = nodeParent; parent != nullptr; parent = parent->mParent)
            transform *= parent->mNiNode.mTransform.toMatrix();

        if (childShape->getShapeType() == TRIANGLE_MESH_SHAPE_PROXYTYPE)
        {
            auto scaledShape = std::make_unique<Resource::ScaledTriangleMeshShape>(
                static_cast<btBvhTriangleMeshShape*>(childShape.get()), Misc::Convert::toBullet(transform.getScale()));
            std::ignore = childShape.release();

            childShape = std::move(scaledShape);
        }
        else
        {
            childShape->setLocalScaling(Misc::Convert::toBullet(transform.getScale()));
        }

        transform.orthoNormalize(transform);

        btTransform trans;
        trans.setOrigin(Misc::Convert::toBullet(transform.getTrans()));
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                trans.getBasis()[i][j] = transform(j, i);

        if (args.mAvoid)
        {
            if (!mAvoidCompoundShape)
                mAvoidCompoundShape.reset(new btCompoundShape);
            mAvoidCompoundShape->addChildShape(trans, childShape.release());
            return;
        }

        if (!mCompoundShape)
            mCompoundShape.reset(new btCompoundShape);
        if (args.mAnimated)
            mShape->mAnimatedShapes.emplace(niGeometry.mRecordIndex, mCompoundShape->getNumChildShapes());
        mCompoundShape->addChildShape(trans, childShape.release());
    }

    void BulletNifLoader::addCollisionShape(
        std::unique_ptr<btCollisionShape> shape, const osg::Matrixf& transform, int animatedRecordIndex)
    {
        if (!shape)
            return;

        shape->setLocalScaling(shape->getLocalScaling() * Misc::Convert::toBullet(transform.getScale()));
        if (!mCompoundShape)
            mCompoundShape.reset(new btCompoundShape);
        if (animatedRecordIndex >= 0)
            mShape->mAnimatedShapes.emplace(animatedRecordIndex, mCompoundShape->getNumChildShapes());
        mCompoundShape->addChildShape(matrixToBullet(transform), shape.release());
    }

    void BulletNifLoader::handleBethesdaCollision(
        const Nif::NiAVObject& node, const Nif::Parent* parent, HandleNodeArgs args)
    {
        ++mBethesdaCollisionStats.mObjects;
        const auto* collision = dynamic_cast<const Nif::bhkCollisionObject*>(node.mCollision.getPtr());
        if (!collision || collision->mBody.empty() || collision->mBody->mShape.empty())
        {
            ++mBethesdaCollisionStats.mUnsupportedObjects;
            warn("Unsupported Bethesda collision object on node " + node.mName + " in " + mShape->mFileName.value());
            return;
        }

        std::unique_ptr<btCollisionShape> shape = makeBethesdaShape(collision->mBody->mShape.get());
        if (!shape)
        {
            ++mBethesdaCollisionStats.mUnsupportedShapes;
            warn("Unsupported Bethesda collision shape on node " + node.mName + " in " + mShape->mFileName.value());
            return;
        }

        osg::Matrixf transform = node.mTransform.toMatrix();
        for (const Nif::Parent* current = parent; current != nullptr; current = current->mParent)
            transform *= current->mNiNode.mTransform.toMatrix();

        if (collision->mBody->mRecordType == Nif::RC_bhkRigidBodyT)
        {
            const auto& rigidBody = static_cast<const Nif::bhkRigidBody&>(collision->mBody.get());
            osg::Matrixf bodyTransform;
            bodyTransform.makeRotate(rigidBody.mInfo.mRotation);
            bodyTransform.setTrans(osg::Vec3f(rigidBody.mInfo.mTranslation.x(), rigidBody.mInfo.mTranslation.y(),
                                       rigidBody.mInfo.mTranslation.z())
                * mHavokScale);
            transform = bodyTransform * transform;
        }
        else if (const auto* phantom = dynamic_cast<const Nif::bhkSimpleShapePhantom*>(&collision->mBody.get()))
        {
            osg::Matrixf phantomTransform = phantom->mTransform;
            phantomTransform.setTrans(phantomTransform.getTrans() * mHavokScale);
            transform = phantomTransform * transform;
        }

        addCollisionShape(std::move(shape), transform, args.mAnimated ? node.mRecordIndex : -1);
    }

    std::unique_ptr<btCollisionShape> BulletNifLoader::makeBethesdaShape(const Nif::bhkShape& shape)
    {
        if (const auto* tree = dynamic_cast<const Nif::bhkBvTreeShape*>(&shape))
            return tree->mShape.empty() ? nullptr : makeBethesdaShape(tree->mShape.get());

        if (const auto* strips = dynamic_cast<const Nif::bhkNiTriStripsShape*>(&shape))
        {
            auto mesh = std::make_unique<btTriangleMesh>();
            for (const auto& data : strips->mData)
                if (!data.empty())
                    addStrips(*mesh, data.get(), 1.f);
            if (mesh->getNumTriangles() == 0)
                return nullptr;
            return std::make_unique<Resource::TriangleMeshShape>(mesh.release(), true);
        }

        if (const auto* packed = dynamic_cast<const Nif::bhkPackedNiTriStripsShape*>(&shape))
        {
            if (packed->mData.empty())
                return nullptr;
            const auto& data = packed->mData.get();
            auto mesh = std::make_unique<btTriangleMesh>();
            for (const auto& triangle : data.mTriangles)
                addTriangle(*mesh, data.mVertices, triangle.mTriangle[0], triangle.mTriangle[1],
                    triangle.mTriangle[2], mHavokScale);
            if (mesh->getNumTriangles() == 0)
                return nullptr;
            return std::make_unique<Resource::TriangleMeshShape>(mesh.release(), true);
        }

        if (const auto* meshShape = dynamic_cast<const Nif::bhkMeshShape*>(&shape))
        {
            auto mesh = std::make_unique<btTriangleMesh>();
            for (const auto& data : meshShape->mDataList)
                if (!data.empty())
                    addStrips(*mesh, data.get(), 1.f);
            if (mesh->getNumTriangles() == 0)
                return nullptr;
            return std::make_unique<Resource::TriangleMeshShape>(mesh.release(), true);
        }

        if (const auto* vertices = dynamic_cast<const Nif::bhkConvexVerticesShape*>(&shape))
        {
            auto result = std::make_unique<btConvexHullShape>();
            for (const osg::Vec4f& vertex : vertices->mVertices)
                result->addPoint(btVector3(vertex.x(), vertex.y(), vertex.z()) * mHavokScale, false);
            if (result->getNumPoints() == 0)
                return nullptr;
            result->recalcLocalAabb();
            return result;
        }

        if (const auto* box = dynamic_cast<const Nif::bhkBoxShape*>(&shape))
            return std::make_unique<btBoxShape>(Misc::Convert::toBullet(box->mExtents) * mHavokScale);

        if (const auto* capsule = dynamic_cast<const Nif::bhkCapsuleShape*>(&shape))
        {
            const btVector3 positions[]{ Misc::Convert::toBullet(capsule->mPoint1) * mHavokScale,
                Misc::Convert::toBullet(capsule->mPoint2) * mHavokScale };
            const btScalar radii[]{ capsule->mRadius1 * mHavokScale, capsule->mRadius2 * mHavokScale };
            return std::make_unique<btMultiSphereShape>(positions, radii, 2);
        }

        if (const auto* multiSphere = dynamic_cast<const Nif::bhkMultiSphereShape*>(&shape))
        {
            std::vector<btVector3> positions;
            std::vector<btScalar> radii;
            positions.reserve(multiSphere->mSpheres.size());
            radii.reserve(multiSphere->mSpheres.size());
            for (const osg::BoundingSpheref& sphere : multiSphere->mSpheres)
            {
                positions.push_back(Misc::Convert::toBullet(sphere.center()) * mHavokScale);
                radii.push_back(sphere.radius() * mHavokScale);
            }
            return positions.empty()
                ? nullptr
                : std::make_unique<btMultiSphereShape>(positions.data(), radii.data(), positions.size());
        }

        if (shape.mRecordType == Nif::RC_bhkSphereShape)
        {
            const auto& sphere = static_cast<const Nif::bhkConvexShape&>(shape);
            return std::make_unique<btSphereShape>(sphere.mRadius * mHavokScale);
        }

        if (const auto* list = dynamic_cast<const Nif::bhkListShape*>(&shape))
        {
            auto result = std::make_unique<btCompoundShape>();
            for (const auto& child : list->mSubshapes)
                if (!child.empty())
                    if (auto childShape = makeBethesdaShape(child.get()))
                        result->addChildShape(btTransform::getIdentity(), childShape.release());
            return result->getNumChildShapes() == 0 ? nullptr : std::move(result);
        }

        if (const auto* list = dynamic_cast<const Nif::bhkConvexListShape*>(&shape))
        {
            auto result = std::make_unique<btCompoundShape>();
            for (const auto& child : list->mSubShapes)
                if (!child.empty())
                    if (auto childShape = makeBethesdaShape(child.get()))
                        result->addChildShape(btTransform::getIdentity(), childShape.release());
            return result->getNumChildShapes() == 0 ? nullptr : std::move(result);
        }

        if (const auto* transformed = dynamic_cast<const Nif::bhkConvexTransformShape*>(&shape))
        {
            if (transformed->mShape.empty())
                return nullptr;
            auto child = makeBethesdaShape(transformed->mShape.get());
            if (!child)
                return nullptr;
            osg::Matrixf transform = transformed->mTransform;
            transform.setTrans(transform.getTrans() * mHavokScale);
            child->setLocalScaling(child->getLocalScaling() * Misc::Convert::toBullet(transform.getScale()));
            auto result = std::make_unique<btCompoundShape>();
            result->addChildShape(matrixToBullet(transform), child.release());
            return result;
        }

        if (const auto* sweep = dynamic_cast<const Nif::bhkConvexSweepShape*>(&shape))
            return sweep->mShape.empty() ? nullptr : makeBethesdaShape(sweep->mShape.get());

        return nullptr;
    }

} // namespace NifBullet
