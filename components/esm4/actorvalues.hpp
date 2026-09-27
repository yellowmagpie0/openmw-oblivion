#ifndef OPENMW_ESM4_ACTORVALUES_H
#define OPENMW_ESM4_ACTORVALUES_H

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <variant>

namespace ESM4
{
    // Canonical vanilla script names, ASCII case-insensitive. UI aliases such
    // as CarryWeight and TES3-only skills are not native actor-value names.
    std::optional<std::uint8_t> actorValueIndex(std::string_view name);

    enum class ActorValueOwner : std::uint8_t { Player, NonPlayer };
    enum class ActorValueProcess : std::uint8_t { Low, Active };
    enum class ActorValueModifier : std::uint8_t { Maximum, Script, Damage };
    using ActorValueModifiers = std::array<std::optional<float>, 3>;

    // Native scalar storage. Sparse-entry presence must survive serialization:
    // an absent NPC modifier does not behave like a stored zero when mutated.
    struct ActorValueState
    {
        float mBase = 0;
        ActorValueModifiers mModifiers{};

        friend bool operator==(const ActorValueState&, const ActorValueState&) = default;
    };

    void validateActorValueState(const ActorValueState& state);

    // Ordinary scalar composition only. Base lookup, creature AV aliases,
    // magicka/encumbrance special handling and integer queries are separate.
    float composeActorValue(const ActorValueState& state, ActorValueOwner owner, ActorValueProcess process);
    // Native NPC/Creature form-float getters return the exact int32 in x87.
    // Add Script and Damage before the first float store, then active Maximum.
    float composeNonPlayerActorValue(std::int32_t base, const ActorValueModifiers& modifiers,
        ActorValueProcess process);
    // Integer AV queries have their own truncation boundaries. The caller
    // supplies the resolved integer base (player base floor or NPC form query).
    std::int32_t composeIntegerActorValue(std::int32_t base, const ActorValueModifiers& modifiers,
        ActorValueOwner owner, ActorValueProcess process);

    // Immutable storage update after the caller's eligibility/delta adjustment.
    // Does not trigger death, derived-stat updates, notifications or effects.
    ActorValueState changeActorValueModifier(
        const ActorValueState& state, ActorValueOwner owner, ActorValueModifier modifier, float delta);

    // Original ForceAV command: exact integer request minus the current float
    // query, then one float store. The caller selects Script or console Damage
    // and applies eligibility; this does not promise a final current value.
    float forceActorValueDelta(std::int32_t requested, float current);

    enum class ActorValueCommand : std::uint8_t { Set, Mod, Force };
    enum class ActorValueCommandSource : std::uint8_t { Script, Console };
    struct ActorValueCommandPolicy
    {
        bool mGodMode = false;
        bool mCanSpendFatigue = true;
    };
    struct ActorValueCommandChange
    {
        ActorValueModifier mModifier;
        float mDelta;
        // Dispatch only after the authority and every shared view commit.
        // This signals the native negative-Health callback, not death itself.
        bool mHealthReaction;
    };
    // Prepare ModAV/ForceAV. SetAV has separate shared base storage semantics.
    // Current is queried only by ForceAV, independently of reference enablement.
    // Null means the original actor wrapper suppressed the write and callbacks.
    std::optional<ActorValueCommandChange> prepareActorValueModifierCommand(ActorValueOwner owner,
        std::uint8_t value, ActorValueCommand command, ActorValueCommandSource source,
        std::int32_t requested, float current, const ActorValueCommandPolicy& policy);

    enum class ActorBaseKind : std::uint8_t { Npc, Creature };
    struct ActorBaseValueSet
    {
        std::uint8_t mActorValue;
        // Attributes, skills and AI bytes; unsigned Magicka/Fatigue words;
        // signed Health int32; or a stored float for extra AVs 40..71.
        std::variant<std::int32_t, float> mValue;

        friend bool operator==(const ActorBaseValueSet&, const ActorBaseValueSet&) = default;
    };
    // Base-record integer query: extra stored floats truncate before either
    // integer or float actor composition. Out-of-int32 conversions depend on
    // the original CPU path and are outside this explicitly supported domain.
    std::int32_t actorBaseValueInteger(const ActorBaseValueSet& value);
    // Prepare the base-record part of an actor SetAV command, including the
    // Creature runtime skill aliases. The player uses the NPC base kind.
    // Null means no base write (AV11 and 37..39), not no process-cache updates
    // or notifications. Shared base ownership/publication belongs to the caller.
    std::optional<ActorBaseValueSet> prepareActorBaseValueSet(
        ActorBaseKind kind, std::uint8_t actorValue, std::int32_t requested);

    enum class DynamicActorValue : std::uint8_t { Health = 8, Magicka = 9, Fatigue = 10, Encumbrance = 11 };

    struct PlayerDynamicBaseSettings
    {
        float mHealthMultiplier;
        float mMagickaMultiplier;
        float mStrengthEncumbranceMultiplier;
    };

    struct PlayerDynamicBaseInput
    {
        DynamicActorValue mValue;
        std::int32_t mFormValue;
        // These are current integer AV queries, not base/mastery attributes.
        std::array<std::int32_t, 8> mCurrentAttributes;
        float mMagickaMultiplier;
    };

    // Native AV40 is divided by TEN, stored as float, with zero selecting one.
    float actorMagickaScale(float multiplier);
    float scaleNpcMagicka(float processValue, float multiplier);
    std::int32_t scaleNpcIntegerMagicka(std::int32_t processValue, float multiplier);
    // Native dynamic maximum uses the integer base query, then one float
    // store after adding the eligible maximum modifier. It is not clamped.
    float dynamicActorValueMaximum(std::int32_t base, float maximumModifier,
        ActorValueOwner owner, ActorValueProcess process);
    // Base-form contribution plus player-specific dynamic adjustment. Base
    // Encumbrance is capacity; the current Encumbrance query uses inventory.
    float calculatePlayerDynamicBaseValue(
        const PlayerDynamicBaseInput& input, const PlayerDynamicBaseSettings& settings);
}

#endif
