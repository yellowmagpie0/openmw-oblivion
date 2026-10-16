#ifndef GAME_RENDER_ESM4NPCANIMATION_H
#define GAME_RENDER_ESM4NPCANIMATION_H

#include "animation.hpp"
#include "tes4facegen.hpp"
#include "util.hpp"
#include "obliviondropplacement.hpp"

#include <cstdint>
#include <vector>

#include <osg/Geometry>

namespace ESM4
{
    struct Npc;
    struct Race;
}

namespace MWRender
{
    class ESM4NpcAnimation : public Animation
    {
    public:
        ESM4NpcAnimation(
            const MWWorld::Ptr& ptr, osg::ref_ptr<osg::Group> parentNode, Resource::ResourceSystem* resourceSystem);

        struct CarriedWeaponPose
        {
            MWWorld::ConstPtr mItem;
            VFS::Path::Normalized mModel;
            // Binary file identity from the template actually instantiated.
            std::string mSourceHash;
            OblivionAttachedWeaponPose mScene;
            std::weak_ptr<const char> mBindingIdentity;
        };
        std::optional<CarriedWeaponPose> captureCarriedWeaponPose(const MWWorld::ConstPtr& item) const;
        bool isCurrentCarriedWeaponPose(const CarriedWeaponPose& pose) const noexcept;

        struct CarriedWeaponBody
        {
            CarriedWeaponPose mPose;
            std::shared_ptr<const NifBullet::ActorRagdollDefinition> mDefinition;
        };
        // Fallible, read-only preparation. Geometry must come from the same
        // bytes as the rendered attachment, even across independent caches.
        // This does not compute a dropped reference pose or publish a body.
        std::optional<CarriedWeaponBody> captureCarriedWeaponBody(const MWWorld::ConstPtr& item) const;
        bool isCurrentCarriedWeaponBody(const CarriedWeaponBody& body) const noexcept;

        struct CarriedWeaponDrop
        {
            CarriedWeaponBody mBody;
            OblivionDroppedWeaponPose mAnchor;
        };
        // The original NPC break path uses skeleton Weapon, not the model's
        // authored root. Caller resolves and guards the native actor placement.
        std::optional<CarriedWeaponDrop> captureCarriedWeaponDrop(
            const MWWorld::ConstPtr& item, const Nif::NiTransform& placement) const;
        bool isCurrentCarriedWeaponDrop(
            const CarriedWeaponDrop& drop, const Nif::NiTransform& placement) const noexcept;
        std::optional<CarriedWeaponDrop> captureCarriedWeaponDrop(
            const MWWorld::ConstPtr& item, const MWWorld::ESMStore& store) const;
        bool isCurrentCarriedWeaponDrop(
            const CarriedWeaponDrop& drop, const MWWorld::ESMStore& store) const noexcept;


        osg::Vec3f runAnimation(float timepassed) override;
        void refreshEquipment();
        void showWeapons(bool showWeapon) override;
        void attachArrow() override;
        void detachArrow() override;

    private:
        osg::ref_ptr<osg::Node> insertPart(
            std::string_view model, std::string_view attachBone = {}, std::string_view texture = {},
            bool correctHeadPartOrientation = true, std::vector<PartHolderPtr>* parts = nullptr,
            std::string* sourceHash = nullptr);
        void updateWeapon();
        void updateWeaponParts();
        std::size_t applyRagdollPose();

        // Works for FO3/FONV/TES5
        void insertHeadParts(const std::vector<ESM::FormId>& partIds, std::set<uint32_t>& usedHeadPartTypes);

        void updateParts();
        void updatePartsTES4(const ESM4::Npc& traits);
        void updatePartsTES5(const ESM4::Npc& traits);

        std::vector<PartHolderPtr> mParts;
        // Carried objects have their own lifetime: sheathing must not detach
        // the actor's body or invalidate the face morph geometry.
        std::vector<PartHolderPtr> mWeaponParts;
        MWWorld::ConstPtr mCarriedWeapon;
        VFS::Path::Normalized mCarriedWeaponModel;
        std::string mCarriedWeaponRecordModel;
        std::string mCarriedWeaponSourceHash;
        std::shared_ptr<const char> mWeaponBindingIdentity;
        bool mShowWeapon = false;
        std::vector<Tes4FaceMorph> mFaceMorphs;
        float mFaceAnimationTime = 0.f;
        VFS::Path::Normalized mVoiceFile;
        bool mHasLipCompanion = false;
    };

    /// Animation adapter for native ESM4 creatures. Oblivion stores one group
    /// per KF alongside the creature skeleton rather than in skeleton.kf.
    class ESM4CreatureAnimation : public Animation
    {
    public:
        ESM4CreatureAnimation(const MWWorld::Ptr& ptr, std::string_view model,
            osg::ref_ptr<osg::Group> parentNode, Resource::ResourceSystem* resourceSystem);
        osg::Vec3f runAnimation(float timepassed) override;

    private:
        std::size_t applyRagdollPose();
        std::vector<PartHolderPtr> mParts;
    };
}

#endif // GAME_RENDER_ESM4NPCANIMATION_H
