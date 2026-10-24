#ifndef OPENMW_ESM4_ABILITY_H
#define OPENMW_ESM4_ABILITY_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <components/esm/formkey.hpp>

namespace ESM4
{
    struct EffectSettingData;
    struct SpellEffect;

    struct LoadedEffectSetting
    {
        std::uint32_t mFlags;
        std::uint32_t mData;
    };

    // Verified compiled inputs for the fourteen racial/birthsign VMOD codes.
    // Other codes remain unadmitted; this is not a complete magic registry.
    std::optional<LoadedEffectSetting> compiledPassiveValueModifierDefinition(std::uint32_t code);

    // Original TES4 DATA loading preserves compiled flags and static AV data.
    // The caller supplies the prior ready definition, including on overrides.
    LoadedEffectSetting mergeLoadedEffectSetting(
        LoadedEffectSetting previous, const EffectSettingData& authored);

    // Original548AD0 scalar result under the game's single-precision x87
    // control. Caller owns hostility/class/range admission and selects the
    // general and effect-specific resistance AVs. Weakness can exceed one;
    // either resistance >=100 returns zero. Preserve the original intermediate
    // rounding, including cancellation to zero below100. No RNG is consumed.
    float nativeEffectResistanceMultiplier(float generalResistance, float effectResistance);

    // Narrow owned lifecycle for the verified recoverable, NoMagnitude PARA
    // effect. Actor preparation, modifier writes and collection ownership stay
    // with the caller; restoring this state must not replay application.
    struct CombatParalysisState
    {
        float mDuration = 0;
        float mElapsed = 0;
        bool mApplied = false;
        bool mTerminated = false;
        bool mRemoved = false;
        void validate() const;
        friend bool operator==(const CombatParalysisState&, const CombatParalysisState&) = default;
    };
    struct CombatParalysisUpdate
    {
        CombatParalysisState mState;
        // Apply: actor preparation then temporary AV48 +1.
        // Remove: Damage AV48 +0 then temporary AV48 -1, in that order.
        // Both can occur in one update of a zero-duration effect.
        bool mApply = false;
        bool mRemove = false;
    };
    CombatParalysisUpdate updateCombatParalysis(CombatParalysisState state, float duration);
    CombatParalysisUpdate cancelCombatParalysis(CombatParalysisState state, bool immediate);

    struct DefaultCombatParalysisAdmission
    {
        CombatParalysisState mIncoming;
        std::optional<std::size_t> mReplaced;
        CombatParalysisUpdate mReplacement;
        std::size_t mInsertion = 0;
    };
    // Compiled default self-PARA: ten-second EFIT, Recover/NoMagnitude, AV48.
    // Every prior entry belongs to that same spell/prototype and self caster.
    // Resistance rejection precedes replacement. Cancel only the first live
    // match, retain its node, then insert before the first equal/greater native
    // comparison key (including terminated nodes). No modifier is applied to
    // the incoming effect until its positive owner update. Caster power scales
    // its duration before target resistance; zero power can admit duration0.
    // Player self-casts bypass target resistance (caller supplies zero here).
    // General spell
    // identities and authored default-spell overrides require another adapter.
    std::optional<DefaultCombatParalysisAdmission> resolveDefaultCombatParalysisAdmission(
        std::span<const CombatParalysisState> prior, float generalResistance, float paralysisResistance,
        float casterPower = 1.f);

    // Native ASCII comparison key, not a display label. Only the first thirty
    // name bytes are read; ignored quantities use the original 1000 sentinel.
    std::string passiveEffectComparisonKey(std::uint32_t flags, std::uint32_t school,
        std::string_view name, float magnitude, float duration);

    struct ValueModifierEffectInputs
    {
        std::uint32_t mActorValue;
        float mMagnitude;
        float mDuration;
    };

    struct PassiveValueModifierInput
    {
        std::uint32_t mEffectIndex;
        std::uint32_t mCode;
        std::uint32_t mFlags;
        ValueModifierEffectInputs mValues;
    };

    // Resolved self Ability4 constructor inputs. These are not saved applied
    // magnitudes. The winning-record adapter owns class/range admission.
    struct PassiveAbilityInput
    {
        ESM::FormKey mSpell;
        std::vector<PassiveValueModifierInput> mEffects;
    };

    // Constructor inputs only. Caller verifies native effect class/admission;
    // detrimental sign, resistance, application and removal happen later.
    ValueModifierEffectInputs resolveValueModifierEffectInputs(
        const SpellEffect& effect, LoadedEffectSetting setting);
    // Query only when the original clamp reaches actor current-value dispatch.
    bool valueModifierRequiresCurrent(std::uint32_t actorValue, std::uint32_t code, float delta);
    float clampValueModifierDelta(std::uint32_t actorValue, std::uint32_t code,
        float delta, std::optional<float> current);

    // Initial Damage write during recoverable removal, before the separate
    // Health-specific correction and essential-health checks. Null omits the
    // write; an engaged zero still dispatches it. Base removal always uses the
    // negated stored magnitude, not the clamped value returned here.
    std::optional<float> initialValueModifierRemovalDamage(std::uint32_t actorValue,
        std::uint32_t code, float storedMagnitude, std::optional<float> current);

}
#endif
