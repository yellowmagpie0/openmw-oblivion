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
                if (body.mUsesRigidBodyTransform || !records.insert(body.mRecord).second
                    || !nodes.insert(body.mNodeRecord).second)
                    throw std::invalid_argument("native physical snapshot requires unique admitted body/bone identities");
        }
    }

    ESM4::RuntimeActorRagdoll captureNativeActorRagdoll(const ESM::FormKey& base,
        std::string_view model, const NifBullet::ActorRagdollDefinition& definition,
        std::span<const NifBullet::RagdollBodyState> bodies)
    {
        validateGraph(definition);
        if (bodies.size() != definition.mBodies.size())
            throw std::invalid_argument("native physical snapshot body count does not match asset");
        ESM4::RuntimeActorRagdoll result;
        result.mBase = base;
        result.mModel = model;
        result.mAssetHash = assetHash(definition);
        std::unordered_map<std::uint32_t, std::uint32_t> nodes;
        for (const auto& body : definition.mBodies)
            nodes.emplace(body.mRecord, body.mNodeRecord);
        result.mBodies.reserve(bodies.size());
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
            result.mBodies.push_back(body);
        }
        std::sort(result.mBodies.begin(), result.mBodies.end(), [](const auto& a, const auto& b) {
            return a.mRecord < b.mRecord;
        });
        result.validate();
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
}
