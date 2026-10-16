#include <components/nifosg/autotransform.hpp>
#include <components/nifosg/matrixtransform.hpp>
#include <components/sceneutil/riggeometry.hpp>
#include <components/sceneutil/skeleton.hpp>

#include <gtest/gtest.h>

#include <osg/Geode>
#include <osgUtil/CullVisitor>

namespace
{
    void expectMatrixNear(const osg::Matrixf& actual, const osg::Matrixf& expected)
    {
        for (unsigned row = 0; row < 4; ++row)
            for (unsigned column = 0; column < 4; ++column)
                EXPECT_NEAR(actual(row, column), expected(row, column), 2e-5);
    }

    TEST(TraversalSkinningTest, BonePaletteUsesProjectedParentsAndConnectorsWithoutChangingControllerLocals)
    {
        osg::ref_ptr<SceneUtil::Skeleton> skeleton = new SceneUtil::Skeleton;
        osg::ref_ptr<NifOsg::MatrixTransform> parent = new NifOsg::MatrixTransform;
        osg::ref_ptr<NifOsg::MatrixTransform> connector = new NifOsg::MatrixTransform;
        osg::ref_ptr<NifOsg::MatrixTransform> child = new NifOsg::MatrixTransform;
        skeleton->addChild(parent);
        parent->addChild(connector);
        connector->addChild(child);
        child->setName("bone");
        parent->setTranslation({1, 2, 3});
        connector->setTranslation({0, 4, 0});
        child->setTranslation({5, 0, 0});
        const auto parentLocal = parent->getMatrix();
        const auto connectorLocal = connector->getMatrix();
        const auto childLocal = child->getMatrix();
        auto* bone = skeleton->getBone("bone");
        ASSERT_NE(bone, nullptr);
        skeleton->updateBoneMatrices(7);
        expectMatrixNear(bone->mMatrixInSkeletonSpace, childLocal * connectorLocal * parentLocal);

        parent->setTraversalMatrix(osg::Matrixd::rotate(.7, osg::Vec3d(0, 0, 1))
            * osg::Matrixd::translate(30, 40, 50));
        connector->setTraversalMatrix(osg::Matrixd::rotate(-.3, osg::Vec3d(1, 0, 0))
            * osg::Matrixd::translate(2, 8, 0));
        child->setTraversalMatrix(osg::Matrixd::translate(-3, 1, 2));
        skeleton->invalidateBoneMatrices();
        skeleton->updateBoneMatrices(7); // Same-frame publication must be explicitly invalidated.
        expectMatrixNear(bone->mMatrixInSkeletonSpace, child->getWorldMatrices()[0]);
        EXPECT_EQ(parent->getMatrix(), parentLocal);
        EXPECT_EQ(connector->getMatrix(), connectorLocal);
        EXPECT_EQ(child->getMatrix(), childLocal);
        parent->clearTraversalMatrix();
        connector->clearTraversalMatrix();
        child->clearTraversalMatrix();
        skeleton->invalidateBoneMatrices();
        skeleton->updateBoneMatrices(7);
        EXPECT_EQ(skeleton->getBone("bone"), bone);
        expectMatrixNear(bone->mMatrixInSkeletonSpace, childLocal * connectorLocal * parentLocal);
    }

    TEST(TraversalSkinningTest, ExplicitAbsoluteProjectionResetsParentButClearRetainsLegacyBoneBehavior)
    {
        osg::ref_ptr<SceneUtil::Skeleton> skeleton = new SceneUtil::Skeleton;
        osg::ref_ptr<osg::MatrixTransform> parent
            = new osg::MatrixTransform(osg::Matrixd::translate(20, 0, 0));
        osg::ref_ptr<NifOsg::AutoTransform> child = new NifOsg::AutoTransform;
        skeleton->addChild(parent);
        parent->addChild(child);
        child->setName("billboard bone");
        child->setTranslation({2, 3, 4});
        child->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
        auto* bone = skeleton->getBone("billboard bone");
        ASSERT_NE(bone, nullptr);
        skeleton->updateBoneMatrices(1);
        // Legacy palette behavior reads locals even for absolute/billboard nodes.
        expectMatrixNear(bone->mMatrixInSkeletonSpace, child->getMatrix() * parent->getMatrix());
        const auto projection = osg::Matrixd::rotate(.4, osg::Vec3d(0, 1, 0))
            * osg::Matrixd::translate(-4, 5, 6);
        child->setTraversalMatrix(projection);
        skeleton->updateBoneMatrices(2);
        expectMatrixNear(bone->mMatrixInSkeletonSpace, projection);
        child->clearTraversalMatrix();
        skeleton->updateBoneMatrices(3);
        expectMatrixNear(bone->mMatrixInSkeletonSpace, child->getMatrix() * parent->getMatrix());
    }

    class CaptureSkin : public osgUtil::CullVisitor
    {
    public:
        std::vector<osg::Vec3f> mPositions, mNormals;
        std::vector<osg::Vec4f> mTangents;
        unsigned mCalls = 0;
        void apply(osg::Geometry& geometry) override
        {
            ++mCalls;
            const auto& positions = *static_cast<osg::Vec3Array*>(geometry.getVertexArray());
            const auto& normals = *static_cast<osg::Vec3Array*>(geometry.getNormalArray());
            const auto& tangents = *static_cast<osg::Vec4Array*>(geometry.getTexCoordArray(7));
            mPositions.assign(positions.begin(), positions.end());
            mNormals.assign(normals.begin(), normals.end());
            mTangents.assign(tangents.begin(), tangents.end());
        }
    };

    class TraversalRigGeometryTest : public ::testing::TestWithParam<bool>
    {
    };

    TEST_P(TraversalRigGeometryTest, SkinVerticesDirectionsAndBoundsCancelProjectedIdentityAuthorRoot)
    {
        osg::ref_ptr<SceneUtil::Skeleton> skeleton = new SceneUtil::Skeleton;
        osg::ref_ptr<NifOsg::MatrixTransform> boneNode = new NifOsg::MatrixTransform;
        osg::ref_ptr<NifOsg::MatrixTransform> skinRoot = new NifOsg::MatrixTransform;
        osg::ref_ptr<osg::Geode> geode = new osg::Geode;
        osg::ref_ptr<SceneUtil::RigGeometry> rig = new SceneUtil::RigGeometry;
        osg::ref_ptr<osg::Geometry> source = new osg::Geometry;
        osg::ref_ptr<osg::Vec3Array> positions = new osg::Vec3Array;
        positions->push_back({1, 2, 3});
        positions->push_back({-2, 1, 0});
        osg::ref_ptr<osg::Vec3Array> normals = new osg::Vec3Array;
        normals->push_back({0, 1, 0});
        normals->push_back({0, 0, 1});
        osg::ref_ptr<osg::Vec4Array> tangents = new osg::Vec4Array;
        tangents->push_back({1, 0, 0, -1});
        tangents->push_back({0, 1, 0, 1});
        source->setVertexArray(positions);
        source->setNormalArray(normals, osg::Array::BIND_PER_VERTEX);
        source->setTexCoordArray(7, tangents, osg::Array::BIND_PER_VERTEX);
        rig->setSourceGeometry(source);
        boneNode->setName("bone");
        skinRoot->setName("skin root");
        // Exercise both explicit root lookup and the missing-root attachment fallback.
        rig->setRootBone(GetParam() ? "SKIN ROOT" : "missing root");
        rig->setName("mesh");
        const osg::Vec3f boundCenter(2, 3, 4);
        rig->setBoneInfo({{"bone", osg::BoundingSpheref(boundCenter, 2.f), osg::Matrixf::identity()}});
        rig->setInfluences({{{0, 1.f}}, {{0, 1.f}}});
        skeleton->addChild(boneNode);
        skeleton->addChild(skinRoot);
        skinRoot->addChild(geode);
        geode->addDrawable(rig);

        const auto boneProjection = osg::Matrixd::rotate(.6, osg::Vec3d(0, 0, 1))
            * osg::Matrixd::translate(9, 8, 7);
        const auto rootProjection = osg::Matrixd::rotate(-.8, osg::Vec3d(1, 0, 0))
            * osg::Matrixd::translate(-5, 4, 2);
        boneNode->setTraversalMatrix(boneProjection);
        skinRoot->setTraversalMatrix(rootProjection);

        for (unsigned frame = 1; frame <= 3; ++frame)
        {
            SCOPED_TRACE(frame);
            if (frame == 2)
            {
                // Controller writes are independent of the still-active drawing pose.
                boneNode->setTranslation({70, 80, 90});
                skinRoot->setTranslation({-20, 30, 40});
            }
            if (frame == 3)
            {
                boneNode->clearTraversalMatrix();
                skinRoot->clearTraversalMatrix();
            }
            osg::NodeVisitor update(osg::NodeVisitor::UPDATE_VISITOR, osg::NodeVisitor::TRAVERSE_ALL_CHILDREN);
            update.setTraversalNumber(frame);
            skeleton->accept(update);
            const osg::Matrixf boneWorld = frame < 3 ? boneProjection : boneNode->getMatrix();
            const osg::Matrixf rootWorld = frame < 3 ? rootProjection : skinRoot->getMatrix();
            const osg::Matrixf skinMatrix = boneWorld * osg::Matrixf::inverse(rootWorld);
            const auto expectedCenter = boundCenter * skinMatrix;
            EXPECT_LT((rig->getBoundingBox().center() - expectedCenter).length(), 2e-5);
            for (unsigned axis = 0; axis < 3; ++axis)
            {
                EXPECT_NEAR(rig->getBoundingBox()._min[axis], expectedCenter[axis] - 2.f, 2e-5);
                EXPECT_NEAR(rig->getBoundingBox()._max[axis], expectedCenter[axis] + 2.f, 2e-5);
            }

            CaptureSkin cull;
            cull.setTraversalNumber(frame);
            cull.pushOntoNodePath(skeleton);
            cull.pushOntoNodePath(skinRoot);
            cull.pushOntoNodePath(geode);
            rig->accept(cull); // Actual CPU skinning; the capture stops before GL submission.
            ASSERT_EQ(cull.mCalls, 1u);
            ASSERT_EQ(cull.mPositions.size(), positions->size());
            for (unsigned i = 0; i < positions->size(); ++i)
            {
                EXPECT_LT((cull.mPositions[i] - (*positions)[i] * skinMatrix).length(), 2e-5);
                EXPECT_LT((cull.mPositions[i] * rootWorld - (*positions)[i] * boneWorld).length(), 2e-5);
                EXPECT_LT((cull.mNormals[i] - osg::Matrixf::transform3x3((*normals)[i], skinMatrix)).length(),
                    2e-5);
                const auto& tangent = (*tangents)[i];
                const auto expectedTangent = osg::Matrixf::transform3x3(
                    osg::Vec3f(tangent.x(), tangent.y(), tangent.z()), skinMatrix);
                const auto& actualTangent = cull.mTangents[i];
                EXPECT_LT((osg::Vec3f(actualTangent.x(), actualTangent.y(), actualTangent.z()) - expectedTangent).length(),
                    2e-5);
                EXPECT_EQ(cull.mTangents[i].w(), tangent.w());
            }
            EXPECT_EQ((*positions)[0], osg::Vec3f(1, 2, 3));
            EXPECT_EQ((*normals)[0], osg::Vec3f(0, 1, 0));
            EXPECT_EQ((*tangents)[0], osg::Vec4f(1, 0, 0, -1));
            EXPECT_EQ(skinRoot->getMatrix(),
                frame == 1 ? osg::Matrixd::identity() : osg::Matrixd::translate(-20, 30, 40));
            EXPECT_EQ(boneNode->getMatrix(),
                frame == 1 ? osg::Matrixd::identity() : osg::Matrixd::translate(70, 80, 90));
        }
    }

    INSTANTIATE_TEST_SUITE_P(RootSelection, TraversalRigGeometryTest, ::testing::Bool());
}
