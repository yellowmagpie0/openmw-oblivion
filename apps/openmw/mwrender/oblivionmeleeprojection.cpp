#include "oblivionmeleeprojection.hpp"

#include <components/esm4/runtimestate.hpp>
#include <components/esm4/animationblend.hpp>
#include <components/sceneutil/keyframe.hpp>

#include <cmath>
#include <stdexcept>
#include <type_traits>

namespace MWRender
{
    std::vector<Animation::NativeSequenceLayer> oblivionPassiveMeleeLayers(Animation& animation,
        const ESM4::RuntimePassiveMeleeSequence* passive, float clock,
        std::shared_ptr<const ESM4::RuntimeMeleePoseSnapshot>& poseBinding)
    {
        if (!std::isfinite(clock) || clock < 0)
            throw std::invalid_argument("invalid passive melee projection clock");
        if (!passive)
        {
            poseBinding.reset();
            return {};
        }
        passive->validate();
        return std::visit([&](const auto& sequence) -> std::vector<Animation::NativeSequenceLayer> {
            const ESM4::MeleeSequenceTiming* timing;
            const ESM4::RuntimeReactionRetirement* retirement = nullptr;
            if constexpr (std::is_same_v<std::decay_t<decltype(sequence)>, ESM4::RuntimeMeleeReaction>)
            {
                if (sequence.mInactive)
                {
                    poseBinding.reset();
                    return {};
                }
                timing = &sequence.mSequenceTiming;
                if (sequence.mRetirement) retirement = &*sequence.mRetirement;
            }
            else timing = &*sequence.mSequenceTiming;
            const auto* source = animation.getControllerSequenceMetadata(sequence.mAnimationGroup);
            if (!source) throw std::invalid_argument("passive melee projection lost its native sequence");
            if ((timing->mEasing && !timing->mEaseStart) || (retirement && !retirement->mStartClock))
                throw std::invalid_argument("passive melee projection requires an anchored manager frame");
            const float ease = retirement
                ? ESM4::sequenceBlendEase(clock, *retirement->mStartClock,
                    static_cast<float>(double(*retirement->mStartClock) + retirement->mDuration), true)
                : timing->mEasing ? ESM4::sequenceBlendEase(clock, *timing->mEaseStart, timing->mEaseEnd, false) : 1.f;
            std::vector<Animation::NativeSequenceLayer> layers;
            if (sequence.mPoseSnapshot)
            {
                if (!poseBinding || *poseBinding != *sequence.mPoseSnapshot)
                    poseBinding = std::make_shared<const ESM4::RuntimeMeleePoseSnapshot>(*sequence.mPoseSnapshot);
                layers.push_back({sequence.mAnimationGroup, BlendMask_All, 0, 1,
                    ESM4::sequenceBlendEase(clock, *timing->mEaseStart, timing->mEaseEnd, true), false, poseBinding});
            }
            else poseBinding.reset();
            layers.push_back({sequence.mAnimationGroup, BlendMask_All, sequence.mAnimationTime, source->mWeight, ease});
            return layers;
        }, passive->mSequence);
    }
}
