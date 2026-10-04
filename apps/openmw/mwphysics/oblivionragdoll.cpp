#include "oblivionragdoll.hpp"

#include <algorithm>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace MWPhysics
{
    namespace
    {
        std::string assetHash(const NifBullet::ActorRagdollDefinition& definition)
        {
            if (definition.mSourceHash.size() != 16)
                throw std::invalid_argument("native physical snapshot requires a 16-byte NIF asset identity");
            constexpr char hex[] = "0123456789abcdef";
            std::string result;
            result.reserve(32);
            for (unsigned char byte : definition.mSourceHash)
            {
                result.push_back(hex[byte >> 4]);
                result.push_back(hex[byte & 15]);
            }
            return result;
        }

        void validateGraph(const NifBullet::ActorRagdollDefinition& definition)
        {
            std::unordered_set<std::uint32_t> records, nodes;
            for (const auto& body : definition.mBodies)
                if (!records.insert(body.mRecord).second || !nodes.insert(body.mNodeRecord).second)
                    throw std::invalid_argument("native physical snapshot requires unique body/bone identities");
        }
    }

    ESM4::RuntimeActorRagdoll captureNativeActorRagdoll(const ESM::FormKey& base,
        std::string_view model, const NifBullet::ActorRagdollDefinition& definition,
        std::span<const NifBullet::RagdollBodyState> bodies,
        std::span<const NifBullet::RagdollNativePackedVelocityState> packedVelocities,
        std::span<const NifBullet::RagdollNativeMotionRequest> motions,
        std::optional<std::span<const NifBullet::RagdollNativeBlendState>> blends,
        std::optional<NativeRagdollControllerSnapshot> controllers)
    {
        validateGraph(definition);
        if (bodies.size() != definition.mBodies.size())
            throw std::invalid_argument("native physical snapshot body count does not match asset");
        if (!packedVelocities.empty() && packedVelocities.size() != bodies.size())
            throw std::invalid_argument("native packed snapshot body count does not match asset");
        if (!motions.empty() && (packedVelocities.empty() || motions.size() != bodies.size()))
            throw std::invalid_argument("native motion snapshot requires complete packed velocity state");
        if (blends && (packedVelocities.empty() || motions.empty()))
            throw std::invalid_argument("native blend snapshot requires complete packed velocity and motion state");
        if (controllers && (!blends || packedVelocities.empty() || motions.empty()))
            throw std::invalid_argument("native controller snapshot requires complete physical and blend state");
        ESM4::RuntimeActorRagdoll result;
        result.mBase = base;
        result.mModel = model;
        result.mAssetHash = assetHash(definition);
        std::unordered_map<std::uint32_t, std::uint32_t> nodes;
        for (const auto& body : definition.mBodies)
            nodes.emplace(body.mRecord, body.mNodeRecord);
        result.mBodies.reserve(bodies.size());
        std::size_t index = 0;
        for (const auto& input : bodies)
        {
            const auto node = nodes.find(input.mRecord);
            if (node == nodes.end())
                throw std::invalid_argument("native physical snapshot body is absent from asset");
            ESM4::RuntimeRagdollBody body;
            body.mRecord = input.mRecord;
            body.mNodeRecord = node->second;
            for (unsigned row = 0; row < 3; ++row)
            {
                for (unsigned col = 0; col < 3; ++col)
                    body.mRotation[row * 3 + col] = static_cast<float>(input.mPose.getBasis()[row][col]);
                body.mPosition[row] = static_cast<float>(input.mPose.getOrigin()[row]);
                body.mLinearVelocity[row] = static_cast<float>(input.mLinearVelocity[row]);
                body.mAngularVelocity[row] = static_cast<float>(input.mAngularVelocity[row]);
            }
            if (!packedVelocities.empty())
            {
                if (packedVelocities[index].mRecord != input.mRecord)
                    throw std::invalid_argument("native packed snapshot body identity mismatch");
                body.mNativePackedVelocity = packedVelocities[index].mVelocities;
            }
            if (!motions.empty())
            {
                if (motions[index].mRecord != input.mRecord)
                    throw std::invalid_argument("native motion snapshot body identity mismatch");
                switch (motions[index].mMotion)
                {
                    case NifBullet::RagdollNativeMotion::Dynamic:
                        body.mNativeMotion = ESM4::RuntimeRagdollMotion::Dynamic; break;
                    case NifBullet::RagdollNativeMotion::Keyframed:
                        body.mNativeMotion = ESM4::RuntimeRagdollMotion::Keyframed; break;
                    default: throw std::invalid_argument("unsupported native motion snapshot mode");
                }
            }
            ++index;
            result.mBodies.push_back(body);
        }
        std::sort(result.mBodies.begin(), result.mBodies.end(), [](const auto& a, const auto& b) {
            return a.mRecord < b.mRecord;
        });
        if (blends)
        {
            result.mNativeBlends.emplace();
            result.mNativeBlends->reserve(blends->size());
            for (const auto& blend : *blends)
                result.mNativeBlends->push_back({blend.mBodyRecord, blend.mCollisionFlags,
                    blend.mRequestedMotion, blend.mGains.mHierarchy, blend.mGains.mVelocity});
            std::sort(result.mNativeBlends->begin(), result.mNativeBlends->end(), [](const auto& a, const auto& b) {
                return a.mBodyRecord < b.mBodyRecord;
            });
        }
        if (controllers)
        {
            result.mNativeControllers.emplace();
            auto& saved = *result.mNativeControllers;
            saved.mBlends.reserve(controllers->mBlends.size());
            for (const auto& controller : controllers->mBlends)
                saved.mBlends.push_back({controller.mRecord, controller.mAttachedNode, controller.mTargetNode, controller.mState});
            saved.mVelocities.reserve(controllers->mVelocities.size());
            for (const auto& controller : controllers->mVelocities)
                saved.mVelocities.push_back({controller.mAttachedNode, controller.mTargetNode, controller.mPrecedesBlend, controller.mState});
            std::sort(saved.mBlends.begin(), saved.mBlends.end(), [](const auto& a, const auto& b) { return a.mRecord < b.mRecord; });
            std::sort(saved.mVelocities.begin(), saved.mVelocities.end(), [](const auto& a, const auto& b) { return a.mAttachedNode < b.mAttachedNode; });
        }
        result.validate();
        // Resolve against the complete winning target set before returning a
        // current snapshot. Empty is complete only for an asset with no targets.
        (void)restoreNativeActorRagdollBlendStates(result, base, model, definition);
        (void)restoreNativeActorRagdollControllers(result, base, model, definition);
        return result;
    }

    std::vector<NifBullet::RagdollBodyState> restoreNativeActorRagdoll(
        const ESM4::RuntimeActorRagdoll& snapshot, const ESM::FormKey& base,
        std::string_view model, const NifBullet::ActorRagdollDefinition& definition)
    {
        snapshot.validate();
        validateGraph(definition);
        if (snapshot.mBase != base || snapshot.mModel != model || snapshot.mAssetHash != assetHash(definition)
            || snapshot.mBodies.size() != definition.mBodies.size())
            throw std::invalid_argument("native physical snapshot does not match the winning actor asset");
        std::unordered_map<std::uint32_t, const ESM4::RuntimeRagdollBody*> bodies;
        for (const auto& body : snapshot.mBodies)
            bodies.emplace(body.mRecord, &body);
        std::vector<NifBullet::RagdollBodyState> result;
        result.reserve(definition.mBodies.size());
        for (const auto& target : definition.mBodies)
        {
            const auto found = bodies.find(target.mRecord);
            if (found == bodies.end() || found->second->mNodeRecord != target.mNodeRecord)
                throw std::invalid_argument("native physical snapshot bone does not match the winning asset");
            const auto& body = *found->second;
            const auto& r = body.mRotation;
            result.push_back({body.mRecord,
                btTransform(btMatrix3x3(r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7], r[8]),
                    btVector3(body.mPosition[0], body.mPosition[1], body.mPosition[2])),
                btVector3(body.mLinearVelocity[0], body.mLinearVelocity[1], body.mLinearVelocity[2]),
                btVector3(body.mAngularVelocity[0], body.mAngularVelocity[1], body.mAngularVelocity[2])});
        }
        return result;
    }

    std::optional<std::vector<NifBullet::RagdollNativePackedVelocityState>> restoreNativeActorRagdollPackedVelocities(
        const ESM4::RuntimeActorRagdoll& snapshot, const ESM::FormKey& base,
        std::string_view model, const NifBullet::ActorRagdollDefinition& definition)
    {
        // Reuse complete winning-asset and geometry validation before producing
        // either projection. No live owner is touched by this adapter.
        (void)restoreNativeActorRagdoll(snapshot, base, model, definition);
        if (!snapshot.mBodies.front().mNativePackedVelocity)
            return std::nullopt;
        std::unordered_map<std::uint32_t, const ESM4::RuntimeRagdollBody*> bodies;
        for (const auto& body : snapshot.mBodies)
            bodies.emplace(body.mRecord, &body);
        std::vector<NifBullet::RagdollNativePackedVelocityState> result;
        result.reserve(definition.mBodies.size());
        for (const auto& target : definition.mBodies)
            result.push_back({target.mRecord, *bodies.at(target.mRecord)->mNativePackedVelocity});
        return result;
    }

    std::optional<std::vector<NifBullet::RagdollNativeMotionRequest>> restoreNativeActorRagdollMotionModes(
        const ESM4::RuntimeActorRagdoll& snapshot, const ESM::FormKey& base,
        std::string_view model, const NifBullet::ActorRagdollDefinition& definition)
    {
        (void)restoreNativeActorRagdoll(snapshot, base, model, definition);
        if (!snapshot.mBodies.front().mNativeMotion)
            return std::nullopt;
        std::unordered_map<std::uint32_t, const ESM4::RuntimeRagdollBody*> bodies;
        for (const auto& body : snapshot.mBodies)
            bodies.emplace(body.mRecord, &body);
        std::vector<NifBullet::RagdollNativeMotionRequest> result;
        result.reserve(definition.mBodies.size());
        for (const auto& target : definition.mBodies)
        {
            const auto motion = *bodies.at(target.mRecord)->mNativeMotion;
            result.push_back({target.mRecord, motion == ESM4::RuntimeRagdollMotion::Dynamic
                ? NifBullet::RagdollNativeMotion::Dynamic : NifBullet::RagdollNativeMotion::Keyframed});
        }
        return result;
    }

    std::optional<std::vector<NifBullet::RagdollNativeBlendState>> restoreNativeActorRagdollBlendStates(
        const ESM4::RuntimeActorRagdoll& snapshot, const ESM::FormKey& base,
        std::string_view model, const NifBullet::ActorRagdollDefinition& definition)
    {
        (void)restoreNativeActorRagdoll(snapshot, base, model, definition);
        if (!snapshot.mNativeBlends)
            return std::nullopt;
        const auto expected = std::count_if(definition.mBodies.begin(), definition.mBodies.end(),
            [](const auto& body) { return body.mBlend.has_value(); });
        if (snapshot.mNativeBlends->size() != static_cast<std::size_t>(expected))
            throw std::invalid_argument("native blend snapshot does not match the winning target count");
        std::unordered_map<std::uint32_t, const ESM4::RuntimeRagdollBlendState*> blends;
        for (const auto& blend : *snapshot.mNativeBlends)
            blends.emplace(blend.mBodyRecord, &blend);
        std::vector<NifBullet::RagdollNativeBlendState> result;
        result.reserve(snapshot.mNativeBlends->size());
        for (const auto& body : definition.mBodies)
        {
            if (!body.mBlend)
                continue;
            const auto found = blends.find(body.mRecord);
            if (found == blends.end())
                throw std::invalid_argument("native blend target does not match the winning body");
            const auto& blend = *found->second;
            result.push_back({blend.mBodyRecord, blend.mCollisionFlags,
                {blend.mHierarchyGain, blend.mVelocityGain}, blend.mRequestedMotion});
        }
        return result;
    }

    std::optional<RestoredNativeRagdollControllers> restoreNativeActorRagdollControllers(
        const ESM4::RuntimeActorRagdoll& snapshot, const ESM::FormKey& base,
        std::string_view model, const NifBullet::ActorRagdollDefinition& definition)
    {
        (void)restoreNativeActorRagdoll(snapshot, base, model, definition);
        if (!snapshot.mNativeControllers)
            return std::nullopt;
        const auto& saved = *snapshot.mNativeControllers;
        const auto expected = std::count_if(definition.mBodies.begin(), definition.mBodies.end(),
            [](const auto& body) { return body.mBlendController.has_value(); });
        if (saved.mBlends.size() != static_cast<std::size_t>(expected))
            throw std::invalid_argument("native controller snapshot does not match the winning authored count");
        std::unordered_map<std::uint32_t, const ESM4::RuntimeRagdollBlendController*> controllers;
        for (const auto& controller : saved.mBlends)
            controllers.emplace(controller.mRecord, &controller);
        RestoredNativeRagdollControllers result;
        result.mBlends.reserve(saved.mBlends.size());
        for (const auto& body : definition.mBodies)
        {
            if (!body.mBlendController)
                continue;
            const auto found = controllers.find(body.mBlendController->mRecord);
            if (found == controllers.end() || found->second->mAttachedNode != body.mNodeRecord)
                throw std::invalid_argument("native controller snapshot does not match the winning record or attachment");
            const auto& controller = *found->second;
            result.mBlends.push_back({controller.mRecord, controller.mTargetNode, controller.mState, controller.mAttachedNode});
        }
        result.mVelocities.reserve(saved.mVelocities.size());
        for (const auto& controller : saved.mVelocities)
            result.mVelocities.push_back({controller.mAttachedNode, controller.mTargetNode, controller.mState, controller.mPrecedesBlend});
        return result;
    }
}
