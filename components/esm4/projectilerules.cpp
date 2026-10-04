#include "projectilerules.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace ESM4
{
    namespace
    {
        void nonnegative(float value)
        {
            if (!std::isfinite(value) || value < 0)
                throw std::invalid_argument("invalid native projectile input");
        }
        void fraction(float value)
        {
            nonnegative(value);
            if (value > 1)
                throw std::invalid_argument("invalid native projectile draw fraction");
        }
        float rounded(double value)
        {
            if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max())
                throw std::invalid_argument("native projectile arithmetic overflow");
            return static_cast<float>(value);
        }
    }

    ProjectileVector projectileWorldToHavok(const ProjectileVector& value)
    {
        ProjectileVector result;
        for (std::size_t i = 0; i < result.size(); ++i)
            result[i] = rounded(double(value[i]) * 0.1428767293691635);
        return result;
    }

    ProjectileVector projectileHavokToWorld(const ProjectileVector& value)
    {
        ProjectileVector result;
        for (std::size_t i = 0; i < result.size(); ++i)
            result[i] = rounded(double(value[i]) * 6.999040126800537);
        return result;
    }

    float arrowCollisionWorldRadius()
    {
        const auto radius = projectileWorldToHavok({0.1f, 0, 0});
        return projectileHavokToWorld(radius)[0];
    }

    ProjectileVector projectileVelocityAfterGravity(const ProjectileVector& velocity,
        const ProjectileVector& gravity, float gravityFactor, float duration)
    {
        nonnegative(gravityFactor);
        nonnegative(duration);
        ProjectileVector result;
        for (std::size_t i = 0; i < result.size(); ++i)
        {
            if (!std::isfinite(velocity[i]) || !std::isfinite(gravity[i]))
                throw std::invalid_argument("nonfinite native projectile vector");
            const float acceleration = rounded(double(gravityFactor) * gravity[i]);
            const float increment = rounded(double(acceleration) * duration);
            result[i] = rounded(double(increment) + velocity[i]);
        }
        return result;
    }

    BowAnimationKeys bowAnimationKeyTimes(std::span<const MeleeTextKey> textKeys)
    {
        for (const auto& key : textKeys)
            if (!std::isfinite(key.mTime))
                throw std::invalid_argument("nonfinite native bow animation time");
        constexpr std::array<std::string_view, 5> names{"start", "attach", "hold", "release", "end"};
        const auto prefix = [](std::string_view text, std::string_view name) {
            if (text.size() < name.size())
                return false;
            for (std::size_t i = 0; i < name.size(); ++i)
            {
                const auto c = text[i];
                const auto lower = c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
                if (lower != name[i])
                    return false;
            }
            return true;
        };
        BowAnimationKeys result;
        for (const auto& key : textKeys)
        {
            auto remaining = key.mText.substr(0, key.mText.find('\0'));
            while (!remaining.empty())
            {
                // Original skips initial CR text to its first LF. Bare CR
                // is not a line separator, and spaces are never trimmed.
                if (remaining.front() == '\r')
                {
                    const auto lf = remaining.find('\n');
                    if (lf == std::string_view::npos)
                        break;
                    remaining.remove_prefix(lf);
                    while (!remaining.empty() && (remaining.front() == '\r' || remaining.front() == '\n'))
                        remaining.remove_prefix(1);
                    if (remaining.empty())
                        break;
                }
                // Sound dispatch precedes phase lookup in the native loop.
                const bool sound = prefix(remaining, "sound:");
                if (!sound && result.mMatchedCount == names.size())
                    throw std::invalid_argument("unsupported bow text after End");
                if (!sound && result.mMatchedCount < names.size()
                    && prefix(remaining, names[result.mMatchedCount]))
                {
                    // Matching advances the slot even when time <= -1 is
                    // not stored by original51B92E. Preserve signed zero.
                    if (key.mTime > -1)
                        result.mTimes[result.mMatchedCount] = key.mTime;
                    ++result.mMatchedCount;
                }
                const auto lf = remaining.find('\n');
                if (lf == std::string_view::npos)
                    break;
                remaining.remove_prefix(lf);
                while (!remaining.empty() && (remaining.front() == '\r' || remaining.front() == '\n'))
                    remaining.remove_prefix(1);
            }
        }
        return result;
    }

    PlayerBowHoldResult playerBowHold(const PlayerBowHoldInput& input)
    {
        if (input.mPhase > BowAnimationPhase::End)
            throw std::invalid_argument("invalid native player bow hold phase");
        if (!(input.mHeld || input.mPressed) || !input.mReady || input.mCrossbow || input.mBlocked)
            return {false, false};
        const bool attack = input.mAnimationCategory >= 4 && input.mAnimationCategory <= 7
            && input.mAnimationCategory != 5;
        return {attack && input.mInputGate == 0 && input.mPhase == BowAnimationPhase::Hold
                    && input.mLatched,
            input.mLatched};
    }

    BowAnimationProgress advanceBowAnimation(const BowAnimationProgress& progress,
        float animationClock, const std::array<float, 5>& keyTimes, bool upperBody)
    {
        if (progress.mPhase > BowAnimationPhase::End
            || !std::isfinite(progress.mSequenceOffset) || !std::isfinite(animationClock))
            throw std::invalid_argument("invalid native bow animation progress");
        for (float time : keyTimes)
            if (!std::isfinite(time))
                throw std::invalid_argument("nonfinite native bow phase coordinate");
        auto result = progress;
        const auto phase = static_cast<std::size_t>(progress.mPhase);
        if (phase < keyTimes.size() - 1
            && rounded(double(progress.mSequenceOffset) + animationClock) > keyTimes[phase + 1])
        {
            result.mPhase = static_cast<BowAnimationPhase>(phase + 1);
            if (upperBody && result.mPhase == BowAnimationPhase::Hold)
                result.mSequenceOffset = rounded(double(keyTimes[2]) - animationClock);
        }
        return result;
    }

    BowAnimationProgress advanceBowPlayback(const BowAnimationProgress& progress,
        float animationClock, float duration, float sequenceStart,
        const std::array<float, 5>& keyTimes, bool upperBodyPaused, float playbackRate)
    {
        nonnegative(duration);
        if (!std::isfinite(sequenceStart))
            throw std::invalid_argument("nonfinite native bow sequence start");
        if (progress.mPhase > BowAnimationPhase::End
            || !std::isfinite(progress.mSequenceOffset) || !std::isfinite(animationClock))
            throw std::invalid_argument("invalid native paused bow progress");
        for (float time : keyTimes)
            if (!std::isfinite(time))
                throw std::invalid_argument("nonfinite native bow phase coordinate");
        auto result = progress;
        const float relative = rounded(double(progress.mSequenceOffset) - sequenceStart);
        float adjusted;
        if (upperBodyPaused)
            adjusted = rounded(double(relative) - duration);
        else
        {
            if (!std::isfinite(playbackRate))
                throw std::invalid_argument("nonfinite native bow playback rate");
            adjusted = rounded(double(relative) + (double(playbackRate) * duration - duration));
        }
        result.mSequenceOffset = rounded(double(adjusted) + sequenceStart);
        return upperBodyPaused ? result : advanceBowAnimation(result, animationClock, keyTimes);
    }

    float advancePlayerBowTimer(float current, float duration,
        std::int32_t processAction, BowAnimationPhase phase)
    {
        if (phase > BowAnimationPhase::End)
            throw std::invalid_argument("invalid native player bow phase");
        if ((processAction != 4 && processAction != 5) || phase == BowAnimationPhase::End)
            return 0;
        nonnegative(duration);
        if (!std::isfinite(current))
            throw std::invalid_argument("nonfinite native player bow timer");
        return rounded(double(current) + duration);
    }

    float playerBowTimerAfterInput(float current, float duration, std::int32_t action,
        BowAnimationPhase phase, bool held, bool pressed, bool ready, bool blocked)
    {
        if (phase > BowAnimationPhase::End)
            throw std::invalid_argument("invalid native player bow input phase");
        if (!(held || pressed) || !ready || blocked)
            return current;
        if ((action != 4 && action != 5) || phase == BowAnimationPhase::End)
            return 0;
        return pressed ? current : advancePlayerBowTimer(current, duration, action, phase);
    }

    BowActionEvent bowActionEvent(std::int32_t action, BowAnimationPhase phase,
        bool present, bool running)
    {
        if (phase > BowAnimationPhase::End)
            throw std::invalid_argument("invalid native bow event phase");
        if (!present || !running)
            return BowActionEvent::None;
        if (action == 4 && phase == BowAnimationPhase::Attach)
            return BowActionEvent::Attach;
        if (action == 5 && phase == BowAnimationPhase::Release)
            return BowActionEvent::Release;
        return BowActionEvent::None;
    }

    void validateArrowCleanupSettings(const ArrowCleanupSettings& settings)
    {
        if (settings.mMaximumReferences < 0)
            throw std::invalid_argument("invalid native arrow reference limit");
    }

    std::optional<std::size_t> selectArrowForCleanup(std::int32_t referenceCount,
        std::span<const ArrowCleanupCandidate> candidates, const ArrowCleanupSettings& settings)
    {
        validateArrowCleanupSettings(settings);
        if (referenceCount < 0)
            throw std::invalid_argument("invalid native arrow reference count");
        for (const auto& candidate : candidates)
            nonnegative(candidate.mAge);
        if (referenceCount <= settings.mMaximumReferences)
            return std::nullopt;
        for (bool preferred : {true, false})
        {
            std::optional<std::size_t> selected;
            float oldest = 0;
            for (std::size_t i = 0; i < candidates.size(); ++i)
            {
                const auto& candidate = candidates[i];
                if (candidate.mPreferredPool == preferred && candidate.mSettled && candidate.mAge > oldest)
                {
                    selected = i;
                    oldest = candidate.mAge;
                }
            }
            if (selected)
                return selected;
        }
        return std::nullopt;
    }

    void validateArrowRecoverySettings(const ArrowRecoverySettings& settings)
    {
        if (settings.mInventoryChance < 0 || settings.mInventoryChance > 100)
            throw std::invalid_argument("invalid native arrow recovery percentage");
    }

    ArrowInventoryRecoveryResult arrowInventoryRecovery(bool arrowEnchanted, unsigned draw,
        const ArrowRecoverySettings& settings)
    {
        validateArrowRecoverySettings(settings);
        if (draw >= 100)
            throw std::invalid_argument("invalid native arrow recovery draw");
        if (arrowEnchanted)
            return {};
        return {true, draw < static_cast<unsigned>(settings.mInventoryChance)};
    }

    void validateArrowLifetimeSettings(const ArrowLifetimeSettings& settings)
    {
        nonnegative(settings.mMaximumAge);
    }

    ArrowLifetimeChange advanceArrowLifetime(const ArrowLifetimeState& state, float duration,
        const ArrowLifetimeSettings& settings)
    {
        validateArrowLifetimeSettings(settings);
        nonnegative(state.mAge);
        fraction(state.mOpacity);
        nonnegative(duration);
        ArrowLifetimeChange result{state, false};
        result.mState.mAge = rounded(double(state.mAge) + duration);
        result.mState.mFading = state.mFading || result.mState.mAge > settings.mMaximumAge;
        if (result.mState.mFading)
        {
            // The original fade divisor is a double constant, not a GMST.
            result.mState.mOpacity = std::max(0.f, rounded(state.mOpacity - double(duration) / 3.0));
            result.mRemove = result.mState.mOpacity == 0;
        }
        return result;
    }

    void validateProjectileSettings(const ProjectileSettings& settings)
    {
        for (float value : {settings.mBowTimerBase, settings.mBowTimerMultiplier, settings.mSpeedMultiplier,
                 settings.mWeakSpeed, settings.mGravityBase, settings.mGravityMultiplier, settings.mWeakGravity})
            nonnegative(value);
    }

    void validateBowFatigueSettings(const BowFatigueSettings& settings)
    {
        nonnegative(settings.mHoldPerSecond);
        nonnegative(settings.mPerShot);
    }

    float bowHoldFatigue(std::int32_t marksman, bool player, bool holding, float duration,
        const BowFatigueSettings& settings, const CombatMasterySettings& mastery)
    {
        validateBowFatigueSettings(settings);
        nonnegative(duration);
        const auto rank = combatMastery(marksman, mastery);
        return player && holding && rank == CombatMastery::Novice
            ? rounded(double(settings.mHoldPerSecond) * duration) : 0.f;
    }

    float bowShotFatigue(std::int32_t marksman, const BowFatigueSettings& settings,
        const CombatMasterySettings& mastery)
    {
        validateBowFatigueSettings(settings);
        return combatMastery(marksman, mastery) == CombatMastery::Novice ? settings.mPerShot : 0.f;
    }

    float bowDrawFraction(float timer, const ProjectileSettings& settings)
    {
        validateProjectileSettings(settings);
        nonnegative(timer);
        return std::min(1.f, rounded(settings.mBowTimerBase + double(timer) * settings.mBowTimerMultiplier));
    }

    float arrowLaunchDamage(const ArrowDamageInput& input, const PhysicalCombatSettings& settings)
    {
        fraction(input.mDrawFraction);
        const WeaponDamageInput bow{input.mMarksman, input.mLuck, input.mAgility, input.mBowDamage,
            input.mBowConditionRatio, input.mFatigueRatio};
        const WeaponDamageInput ammo{input.mMarksman, input.mLuck, input.mAgility, input.mAmmoDamage,
            1.f, input.mFatigueRatio};
        const float bowDamage = rounded(double(weaponDamage(bow, settings)) + input.mAttackBonus);
        const float ammoDamage = rounded(double(weaponDamage(ammo, settings)) + input.mAttackBonus);
        const float combined = rounded(double(bowDamage) + ammoDamage);
        return rounded(double(combined) * input.mDrawFraction);
    }

    float arrowLaunchSpeed(float ammoSpeed, float drawFraction, const ProjectileSettings& settings)
    {
        validateProjectileSettings(settings);
        nonnegative(ammoSpeed);
        fraction(drawFraction);
        const float fullSpeed = rounded(double(ammoSpeed) * settings.mSpeedMultiplier);
        return rounded(double(fullSpeed) * (drawFraction + (1.0 - drawFraction) * settings.mWeakSpeed));
    }

    float arrowGravityFactor(std::int32_t marksman, std::int32_t luck, float drawFraction,
        const ProjectileSettings& settings, const PhysicalCombatSettings& physical)
    {
        validateProjectileSettings(settings);
        fraction(drawFraction);
        const float skill = effectiveCombatSkill(marksman, luck, physical);
        const float fullGravity = rounded(settings.mGravityBase - double(skill) * settings.mGravityMultiplier);
        return std::max(0.f, rounded(double(fullGravity) * drawFraction + (1.0 - drawFraction) * settings.mWeakGravity));
    }
}
