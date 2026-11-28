#include "suspendedtrap.hpp"

#include "controller.hpp"
#include "data.hpp"
#include "node.hpp"
#include "physics.hpp"

#include <algorithm>
#include <cmath>
#include <set>

namespace Nif
{
    static const bhkWorldObject* collisionBody(const NiAVObject& node)
    {
        if (node.mCollision.empty())
            return nullptr;
        const auto* collision = dynamic_cast<const bhkCollisionObject*>(node.mCollision.getPtr());
        return collision && !collision->mBody.empty() ? collision->mBody.getPtr() : nullptr;
    }

    static void appendRotationTrack(std::vector<std::unique_ptr<Record>>& records, NiControllerSequence& sequence,
        const NiAVObject& node, QuaternionKeyMapPtr rotations)
    {
        auto data = std::make_unique<NiKeyframeData>();
        data->mRecordIndex = static_cast<unsigned>(records.size());
        data->mRecordType = RC_NiKeyframeData;
        data->mRecordName = "NiTransformData";
        data->mRotations = std::move(rotations);
        auto interp = std::make_unique<NiTransformInterpolator>();
        interp->mRecordIndex = data->mRecordIndex + 1;
        interp->mRecordType = RC_NiTransformInterpolator;
        interp->mRecordName = "NiTransformInterpolator";
        interp->mData = data.get();
        const auto& bind = node.mTransform;
        interp->mDefaultValue = { bind.mTranslation, bind.mRotation.toOsgMatrix().getRotate(), bind.mScale };
        ControlledBlock block{};
        block.mController = nullptr;
        block.mBlendInterpolator = nullptr;
        block.mStringPalette = nullptr;
        block.mControllerType = "NiTransformController";
        block.mNodeName = node.mName;
        block.mInterpolator = interp.get();
        sequence.mControlledBlocks.push_back(std::move(block));
        records.push_back(std::move(data));
        records.push_back(std::move(interp));
    }

    // Sample the chain as one taut pendulum, with the damage head free to
    // rotate about the last link. Both rotations are relative to the bind pose.
    static void appendSwing(std::vector<std::unique_ptr<Record>>& records, NiControllerSequence& release,
        const NiAVObject& pivot, const NiAVObject& head, osg::Vec3d chain, osg::Vec3d tip,
        const osg::Quat& pivotParentRotation, const osg::Quat& headParentRotation)
    {
        const osg::Vec3d down(0, 0, -1);
        osg::Vec3d horizontal(chain.x(), chain.y(), 0);
        if (horizontal.normalize() < 1e-4)
            return;
        const osg::Vec3d axis = down ^ horizontal;
        const double initial = std::atan2(chain * horizontal, chain * down);
        const double headInitial = std::atan2(tip * horizontal, tip * down);
        const double length = chain.length();
        const double bladeLength = std::hypot(tip * horizontal, tip * down);
        if (!(length >= 1 && bladeLength >= 1))
            return;
        const auto chainKeys = std::make_shared<QuaternionKeyMap>();
        const auto bladeKeys = std::make_shared<QuaternionKeyMap>();
        chainKeys->mInterpolationType = bladeKeys->mInterpolationType = InterpolationType_Linear;
        const auto chainBind = pivot.mTransform.mRotation.toOsgMatrix().getRotate();
        const auto bladeBind = head.mTransform.mRotation.toOsgMatrix().getRotate();
        constexpr double gravity = 686.;
        constexpr int rate = 60, seconds = 16;
        double chainAngle = initial, chainSpeed = 0.;
        double bladeAngle = headInitial, bladeSpeed = 0.;
        const auto pivotAxis = pivotParentRotation.inverse() * axis;
        const auto headAxis = headParentRotation.inverse() * axis;
        for (int i = 0; i <= rate * seconds; ++i)
        {
            const float time = release.mStopTime + static_cast<float>(i) / rate;
            if (i == rate * seconds)
                chainAngle = bladeAngle = 0.;
            const osg::Quat chainRotation = chainBind * osg::Quat(chainAngle - initial, pivotAxis);
            const osg::Quat bladeRotation
                = bladeBind * osg::Quat((bladeAngle - headInitial) - (chainAngle - initial), headAxis);
            chainKeys->mKeys.emplace_back(time, KeyT<osg::Quat>{ chainRotation, {}, {} });
            bladeKeys->mKeys.emplace_back(time, KeyT<osg::Quat>{ bladeRotation, {}, {} });

            const double sinChain = std::sin(chainAngle), cosChain = std::cos(chainAngle);
            const double accel = -(gravity / length) * sinChain - 0.8 * chainSpeed;
            const double pinY = length * (cosChain * accel - sinChain * chainSpeed * chainSpeed);
            const double pinZ = length * (sinChain * accel + cosChain * chainSpeed * chainSpeed);
            bladeSpeed -= (((gravity + pinZ) * std::sin(bladeAngle) + pinY * std::cos(bladeAngle)) / bladeLength
                              + 1.1 * bladeSpeed)
                / rate;
            chainSpeed += accel / rate;
            bladeAngle += bladeSpeed / rate;
            chainAngle += chainSpeed / rate;
        }
        appendRotationTrack(records, release, pivot, chainKeys);
        appendRotationTrack(records, release, head, bladeKeys);
        release.mStopTime += seconds;
    }

    void animateSuspendedTrap(std::vector<std::unique_ptr<Record>>& records)
    {
        NiControllerSequence* release = nullptr;
        const bhkSimpleShapePhantom* damage = nullptr;
        const NiAVObject* node = nullptr;
        for (const auto& record : records)
        {
            if (auto* seq = dynamic_cast<NiControllerSequence*>(record.get()); seq && seq->mName == "Unequip")
            {
                if (release)
                    return;
                release = seq;
            }
            const auto* candidate = dynamic_cast<const NiAVObject*>(record.get());
            if (!candidate)
                continue;
            if (const auto* phantom = dynamic_cast<const bhkSimpleShapePhantom*>(collisionBody(*candidate));
                phantom && (phantom->mHavokFilter.mLayer & 0x3f) == 14)
            {
                if (damage)
                    return;
                damage = phantom;
                node = candidate;
            }
        }
        if (!release || release->mControlledBlocks.empty() || !damage || damage->mShape.empty())
            return;
        const auto* hull = dynamic_cast<const bhkConvexVerticesShape*>(damage->mShape.getPtr());
        if (!hull || hull->mVertices.empty())
            return;

        // Follow the render hierarchy from the damage volume to the fixed
        // anchor. Each moving body must also be constrained to that parent.
        std::vector<const NiAVObject*> path;
        std::set<const NiAVObject*> seen;
        while (node)
        {
            if (!seen.insert(node).second || node->mParents.size() > 1)
                return;
            path.push_back(node);
            node = node->mParents.empty() ? nullptr : node->mParents.front();
        }
        std::vector<osg::Matrixf> world(path.size());
        osg::Matrixf transform;
        for (std::size_t i = path.size(); i-- > 0;)
            world[i] = transform = path[i]->mTransform.toMatrix() * transform;

        std::size_t anchor = 1;
        for (; anchor + 1 < path.size(); ++anchor)
        {
            const auto* body = dynamic_cast<const bhkRigidBody*>(collisionBody(*path[anchor]));
            if (!body)
                return;
            if (body->mInfo.mMotionType == HkMotionType::Motion_Fixed)
                break;
            if (!(body->mInfo.mMass > 0) || body->mInfo.mMotionType == HkMotionType::Motion_Keyframed
                || path[anchor]->mName.empty() || body->mConstraints.size() != 1 || body->mConstraints.front().empty())
                return;
            const auto* joint = dynamic_cast<const bhkConstraint*>(body->mConstraints.front().getPtr());
            const auto* parent = collisionBody(*path[anchor + 1]);
            if (!parent || !joint || joint->mInfo.mEntityA.empty() || joint->mInfo.mEntityB.empty()
                || !((joint->mInfo.mEntityA.getPtr() == body && joint->mInfo.mEntityB.getPtr() == parent)
                    || (joint->mInfo.mEntityB.getPtr() == body && joint->mInfo.mEntityA.getPtr() == parent)))
                return;
            if (std::ranges::any_of(release->mControlledBlocks, [&](const auto& block) {
                    return (block.mNodeName.empty() ? block.mTargetName : block.mNodeName) == path[anchor]->mName;
                }))
                return;
        }
        if (anchor < 3 || anchor >= path.size())
            return;
        const auto* fixed = dynamic_cast<const bhkRigidBody*>(collisionBody(*path[anchor]));
        if (!fixed || fixed->mInfo.mMotionType != HkMotionType::Motion_Fixed)
            return;

        osg::Vec3f center;
        for (const auto& v : hull->mVertices)
            center += osg::Vec3f(v.x(), v.y(), v.z());
        center /= static_cast<float>(hull->mVertices.size());
        constexpr float havokToModel = 10.f / 1.42875f;
        // Havok stores an affine basis and translation, with padding in the fourth column.
        const auto localCenter = osg::Matrixf::transform3x3(center, damage->mTransform) + damage->mTransform.getTrans();
        const auto tip = (localCenter * havokToModel) * world.front();
        appendSwing(records, *release, *path[anchor - 1], *path[1], world[1].getTrans() - world[anchor - 1].getTrans(),
            tip - world[1].getTrans(), world[anchor].getRotate(), world[2].getRotate());
    }
}
