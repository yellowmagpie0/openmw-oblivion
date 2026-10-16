#ifndef OPENMW_MWRENDER_OBLIVIONDROPPLACEMENT_HPP
#define OPENMW_MWRENDER_OBLIVIONDROPPLACEMENT_HPP

#include "util.hpp"
#include <components/esm4/dropplacement.hpp>
#include <components/nif/niftypes.hpp>
#include <optional>
#include <vector>

namespace MWWorld { class ConstPtr; class ESMStore; }

namespace MWRender
{
    // Reads an already initialized TES4 NPC cache and its winning records.
    // Never initializes custom data. Caller guards the World/reference lifetime.
    std::optional<Nif::NiTransform> resolveOblivionNpcDropPlacement(
        const MWWorld::ConstPtr& actor, const MWWorld::ESMStore& store) noexcept;

    // Captures an attachment anchor, independently of the carried model root.
    // The caller resolves the native actor placement, including its separate
    // scale; this class verifies its agreement with the rendered placement.
    // Authored NIF scales stay separate during native parent/local composition.
    class OblivionDroppedWeaponPose
    {
    public:
        static std::optional<OblivionDroppedWeaponPose> capture(
            const OblivionAttachedWeaponPose& anchor, const Nif::NiTransform& placement);
        const Nif::NiTransform& getNativeWorld() const { return mNativeWorld; }
        const ESM4::DroppedItemPlacement& getDropPlacement() const { return mDropPlacement; }
        const osg::Node* getPlacementNode() const { return mScene.getPlacementNode(); }
        const osg::Node* getAnchor() const { return mScene.getAttachedNode(); }
        bool matchesCurrentScene(const Nif::NiTransform& placement) const noexcept;

    private:
        OblivionAttachedWeaponPose mScene;
        // mScene owns every node; pointers here retain root-to-anchor order.
        std::vector<const osg::Node*> mPath;
        Nif::NiTransform mPlacement = Nif::NiTransform::getIdentity();
        Nif::NiTransform mNativeWorld = Nif::NiTransform::getIdentity();
        ESM4::DroppedItemPlacement mDropPlacement;
    };
}

#endif
