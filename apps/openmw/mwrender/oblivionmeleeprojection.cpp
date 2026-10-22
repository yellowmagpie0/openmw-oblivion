#include "oblivionmeleeprojection.hpp"

#include <components/esm4/runtimestate.hpp>
#include <components/esm4/animationblend.hpp>
#include <components/sceneutil/keyframe.hpp>

#include <cmath>
#include <stdexcept>
#include <type_traits>

namespace MWRender
{
    std::vector<Animation::NativeSequenceLayer> oblivionHeldBlockLayers(Animation& animation,
        std::span<const ESM4::RuntimeHeldBlockSequence> held, float clock, OblivionHeldPoseBindings& bindings)
    {
        if (!std::isfinite(clock) || clock < 0)
            throw std::invalid_argument("invalid held-block projection clock");
        auto preparedBindings = bindings;
        preparedBindings.resize(held.size());
        std::vector<Animation::NativeSequenceLayer> layers;
        for (std::size_t i = 0; i < held.size(); ++i)
        {
            const auto& sequence = held[i];
            sequence.validate();
            auto& pose = preparedBindings[i];
            if (sequence.mInactive) { pose.reset(); continue; }
            const auto* source = animation.getControllerSequenceMetadata(sequence.mAnimationGroup);
            if (!source || source->mCycleType != 0 || source->mWeight != 1)
                throw std::invalid_argument("held-block projection requires its cyclic native source");
            const auto& timing = sequence.mSequenceTiming;
            if ((timing.mEasing && !timing.mEaseStart)
                || (sequence.mRetirement && !sequence.mRetirement->mStartClock))
                throw std::invalid_argument("held-block projection requires an anchored manager frame");
            const float start = sequence.mRetirement ? *sequence.mRetirement->mStartClock
                : timing.mEaseStart.value_or(clock);
            const float end = sequence.mRetirement
                ? static_cast<float>(double(start) + sequence.mRetirement->mDuration) : timing.mEaseEnd;
            const float ease = sequence.mRetirement ? ESM4::sequenceBlendEase(clock, start, end, true)
                : timing.mEasing ? ESM4::sequenceBlendEase(clock, start, end, false) : 1.f;
            if (sequence.mPoseSnapshot)
            {
                if (!pose || *pose != *sequence.mPoseSnapshot)
                    pose = std::make_shared<const ESM4::RuntimeMeleePoseSnapshot>(*sequence.mPoseSnapshot);
                // Full peer reset changes both NI4 source and NI5 destination
                // to NI3: both use the same new retirement ease.
                const float sourceEase = ESM4::sequenceBlendEase(clock, start, end, true);
                layers.push_back({sequence.mAnimationGroup, BlendMask_All, 0, 1, sourceEase, false, pose});
            }
            else pose.reset();
            layers.push_back({sequence.mAnimationGroup, BlendMask_All, sequence.mAnimationTime,
                source->mWeight, ease, false, nullptr, true});
        }
        bindings.swap(preparedBindings);
        return layers;
    }

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
