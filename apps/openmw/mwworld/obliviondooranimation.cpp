#include "obliviondooranimation.hpp"

#include "worldimp.hpp"
#include "class.hpp"
#include "oblivionscriptmanager.hpp"
#include "../mwbase/environment.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwrender/animation.hpp"

#include <components/misc/strings/algorithm.hpp>
#include <cmath>

namespace MWWorld
{
    namespace
    {
        std::optional<int> poseState(std::string_view group, float progress)
        {
            if (!std::isfinite(progress)) return std::nullopt;
            if (Misc::StringUtils::ciEqual(group, "open")) return progress >= 1.f ? 1 : 2;
            if (Misc::StringUtils::ciEqual(group, "close")) return progress >= 1.f ? 3 : 4;
            return std::nullopt;
        }

        bool nativeDoor(const World& world, const ConstPtr& door)
        {
            return world.getGameProfile() == ESM::GameProfile::Oblivion && !door.isEmpty()
                && door.getType() == ESM::REC_DOOR4 && !door.getCellRef().getTeleport();
        }
    }

    std::optional<int> oblivionAnimatedDoorState(const World& world, const ConstPtr& door)
    {
        if (!nativeDoor(world, door)) return std::nullopt;
        const auto* animation = world.getAnimation(door);
        const bool embedded = animation && animation->hasAnimation("open") && animation->hasAnimation("close");
        if (!embedded && door.getClass().getDoorState(door) != DoorState::Idle)
            return std::nullopt; // A hinge transition owns this reference.
        if (embedded)
        {
            const auto group = animation->getActiveGroup(MWRender::BoneGroup_LowerBody);
            float progress = 0.f;
            if (animation->getInfo(group, &progress))
                if (const auto state = poseState(group, progress)) return state;
        }
        // This is also the owner restored from native animation metadata.
        // Do not interpret absolute seconds as a normalized fraction.
        const auto& saved = door.getRefData().getAnimationState().mScriptedAnims;
        if (!saved.empty())
        {
            const auto& pose = saved.front();
            float progress = pose.mTime;
            if (pose.mAbsolute)
            {
                if (!animation) return std::nullopt;
                const float start = animation->getStartTime(pose.mGroup);
                const float stop = animation->getTextKeyTime(pose.mGroup + ": stop");
                if (!std::isfinite(start) || !std::isfinite(stop) || stop <= start) return std::nullopt;
                progress = (progress - start) / (stop - start);
            }
            if (const auto state = poseState(pose.mGroup, progress)) return state;
        }
        return embedded ? std::optional(3) : std::nullopt;
    }

    bool playOblivionDoorAnimation(World& world, const Ptr& door, DoorState transition)
    {
        if (!nativeDoor(world, door) || transition == DoorState::Idle) return false;
        const auto* animation = world.getAnimation(ConstPtr(door));
        if (!animation || !animation->hasAnimation("open") || !animation->hasAnimation("close")) return false;
        MWBase::MechanicsManager* mechanics = MWBase::Environment::get().getMechanicsManager();
        const std::string group = transition == DoorState::Opening ? "open" : "close";
        if (!mechanics || !mechanics->playAnimationGroup(door, group, 1, 0, true)) return false;
        if (auto* scripts = world.getOblivionScriptManager())
            scripts->persistAnimationState(door.getCellRef().getFormKey(), group, 1, true, true);
        return true;
    }
}
