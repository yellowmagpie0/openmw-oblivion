#include "obliviondropplacement.hpp"

#include "../mwclass/esm4npc.hpp"
#include "../mwworld/esmstore.hpp"
#include <components/esm4/npcplacement.hpp>
#include <components/nif/nativetransform.hpp>
#include <components/nifosg/matrixtransform.hpp>
#include <components/sceneutil/positionattitudetransform.hpp>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <span>

namespace MWRender
{
    std::optional<Nif::NiTransform> resolveOblivionNpcDropPlacement(
        const MWWorld::ConstPtr& actor, const MWWorld::ESMStore& store) noexcept
    {
        if (actor.isEmpty() || actor.getType() != ESM::REC_NPC_4) return std::nullopt;
        const auto* data = dynamic_cast<const MWClass::ESM4NpcCustomData*>(actor.getRefData().getCustomData());
        if (!data) return std::nullopt;
        const auto* placed = actor.getCellRef().getNativeActor();
        const auto* reference = actor.getCellRef().getNativeReference();
        if (!placed && !reference) return std::nullopt;
        const auto* base = store.search<ESM4::Npc>(placed ? placed->mBaseKey : reference->mBaseKey);
        if (!base || !base->mIsTES4 || base->mId != (placed ? placed->mBaseObj : reference->mBaseObj)
            || actor.get<ESM4::Npc>()->mBase != base
            || data->mTraits != base || data->mBaseData != base
            || data->mIsFemale != bool(base->mBaseConfig.tes4.flags & ESM4::Npc::TES4_Female))
            return std::nullopt;
        const auto* race = store.get<ESM4::Race>().search(base->mRace);
        if (race != data->mRace) return std::nullopt;
        std::optional<ESM4::NpcPlacementMorphology> morphology;
        if (race)
            morphology = ESM4::NpcPlacementMorphology{
                data->mIsFemale ? race->mWeightFemale : race->mWeightMale,
                data->mIsFemale ? race->mHeightFemale : race->mHeightMale};
        return ESM4::npcPlacementRoot(actor.getRefData().getPosition(), actor.getCellRef().getScale(), morphology);
    }

    namespace
    {
        bool same(float a, float b) noexcept
        {
            return std::bit_cast<std::uint32_t>(a) == std::bit_cast<std::uint32_t>(b);
        }

        bool same(const Nif::NiTransform& a, const Nif::NiTransform& b) noexcept
        {
            if (!same(a.mScale, b.mScale)) return false;
            for (unsigned row = 0; row < 3; ++row)
            {
                if (!same(a.mTranslation[row], b.mTranslation[row])) return false;
                for (unsigned column = 0; column < 3; ++column)
                    if (!same(a.mRotation.mValues[row][column], b.mRotation.mValues[row][column])) return false;
            }
            return true;
        }

        bool store(double value, float& result) noexcept
        {
            return Nif::tryStoreNativeTransformFloat(value, result);
        }

        bool readMatrix(const osg::Node& node, osg::Matrix& result) noexcept
        {
            result.makeIdentity();
            const auto* transform = dynamic_cast<const osg::Transform*>(&node);
            if (!transform) return true;
            if (transform->getReferenceFrame() != osg::Transform::RELATIVE_RF) return false;
            if (const auto* matrix = dynamic_cast<const osg::MatrixTransform*>(transform))
                result = matrix->getMatrix();
            else if (const auto* pat = dynamic_cast<const SceneUtil::PositionAttitudeTransform*>(transform))
            {
                result.preMultTranslate(pat->getPosition());
                result.preMultRotate(pat->getAttitude());
                result.preMultScale(pat->getScale());
            }
            else return false;
            for (unsigned row = 0; row < 4; ++row)
                for (unsigned column = 0; column < 4; ++column)
                    if (!std::isfinite(result(row, column))) return false;
            return result(0, 3) == 0 && result(1, 3) == 0 && result(2, 3) == 0 && result(3, 3) == 1;
        }

        bool readLocal(const osg::Node& node, Nif::NiTransform& result) noexcept
        {
            osg::Matrix matrix;
            if (!readMatrix(node, matrix)) return false;
            // Synthetic renderer wrappers have no authored separate NIF scale;
            // their full linear map is retained as rotationScale with scale 1.
            result.mScale = 1;
            if (const auto* nif = dynamic_cast<const NifOsg::MatrixTransform*>(&node))
            {
                // A zero separate scale erases rendered rotation information.
                // Cached rotation may be stale after procedural controllers.
                if (!std::isfinite(nif->mScale) || nif->mScale == 0) return false;
                result.mScale = nif->mScale;
            }
            for (unsigned row = 0; row < 3; ++row)
            {
                if (!store(matrix(3, row), result.mTranslation[row])) return false;
                for (unsigned column = 0; column < 3; ++column)
                    if (!store(matrix(column, row) / double(result.mScale),
                            result.mRotation.mValues[row][column])) return false;
            }
            return true;
        }

        std::optional<Nif::NiTransform> readWorld(
            std::span<const osg::Node* const> path, const Nif::NiTransform& placement) noexcept
        {
            if (path.empty() || !std::isfinite(placement.mScale)) return std::nullopt;
            osg::Matrix root;
            if (!readMatrix(*path.front(), root)) return std::nullopt;
            if (const auto* nif = dynamic_cast<const NifOsg::MatrixTransform*>(path.front());
                nif && !same(nif->mScale, placement.mScale)) return std::nullopt;
            for (unsigned row = 0; row < 3; ++row)
            {
                float coordinate;
                if (!store(root(3, row), coordinate) || !same(coordinate, placement.mTranslation[row]))
                    return std::nullopt;
                for (unsigned column = 0; column < 3; ++column)
                {
                    const double expected = double(placement.mRotation.mValues[row][column]) * placement.mScale;
                    const double actual = root(column, row);
                    // Explicit renderer/native root admission tolerance; later
                    // validation compares both scene and native float bits exactly.
                    if (!std::isfinite(expected) || !std::isfinite(placement.mRotation.mValues[row][column])
                        || std::abs(expected - actual) > 1e-6 * std::max(1.0, std::abs(actual)))
                        return std::nullopt;
                }
            }
            Nif::NiTransform world = placement;
            for (std::size_t index = 1; index < path.size(); ++index)
            {
                Nif::NiTransform local;
                if (!readLocal(*path[index], local)) return std::nullopt;
                const auto composed = Nif::tryComposeNativeTransform(world, local);
                if (!composed) return std::nullopt;
                world = *composed;
            }
            return world;
        }
    }

    std::optional<OblivionDroppedWeaponPose> OblivionDroppedWeaponPose::capture(
        const OblivionAttachedWeaponPose& anchor, const Nif::NiTransform& placement)
    {
        if (!anchor.matchesCurrentScene()) return std::nullopt;
        OblivionDroppedWeaponPose result;
        result.mScene = anchor;
        const osg::Node* node = anchor.getAttachedNode();
        while (node)
        {
            if (std::find(result.mPath.begin(), result.mPath.end(), node) != result.mPath.end())
                return std::nullopt;
            result.mPath.push_back(node);
            if (node == anchor.getPlacementNode()) break;
            if (node->getNumParents() != 1) return std::nullopt;
            node = node->getParent(0);
        }
        if (result.mPath.empty() || result.mPath.back() != anchor.getPlacementNode()) return std::nullopt;
        std::reverse(result.mPath.begin(), result.mPath.end());
        const auto world = readWorld(result.mPath, placement);
        if (!world) return std::nullopt;
        result.mPlacement = placement;
        result.mNativeWorld = *world;
        result.mDropPlacement = ESM4::droppedItemPlacement(*world);
        if (!result.matchesCurrentScene(placement)) return std::nullopt;
        return result;
    }

    bool OblivionDroppedWeaponPose::matchesCurrentScene(const Nif::NiTransform& placement) const noexcept
    {
        if (!same(mPlacement, placement) || !mScene.matchesCurrentScene()) return false;
        const auto current = readWorld(mPath, placement);
        return current && same(*current, mNativeWorld);
    }
}
