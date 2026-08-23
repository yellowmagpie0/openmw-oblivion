#ifndef GAME_RENDER_ESM4NPCANIMATION_H
#define GAME_RENDER_ESM4NPCANIMATION_H

#include "animation.hpp"
#include "tes4facegen.hpp"

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

        osg::Vec3f runAnimation(float timepassed) override;

    private:
        osg::ref_ptr<osg::Node> insertPart(
            std::string_view model, std::string_view attachBone = {}, std::string_view texture = {});
        std::size_t applyRagdollPose();

        // Works for FO3/FONV/TES5
        void insertHeadParts(const std::vector<ESM::FormId>& partIds, std::set<uint32_t>& usedHeadPartTypes);

        void updateParts();
        void updatePartsTES4(const ESM4::Npc& traits);
        void updatePartsTES5(const ESM4::Npc& traits);

        std::vector<PartHolderPtr> mParts;
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
