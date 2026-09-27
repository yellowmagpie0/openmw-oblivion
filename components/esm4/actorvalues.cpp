#include "actorvalues.hpp"
#include "actorstats.hpp"

#include <cmath>
#include <algorithm>
#include <bit>
#include <limits>
#include <stdexcept>

namespace ESM4
{
    namespace
    {
        void validateOwner(ActorValueOwner owner)
        {
            if (owner != ActorValueOwner::Player && owner != ActorValueOwner::NonPlayer)
                throw std::invalid_argument("invalid native actor-value owner");
        }

        float stored(double value)
        {
            const float result = static_cast<float>(value);
            if (!std::isfinite(result))
                throw std::invalid_argument("native actor-value composition overflow");
            return result;
        }

        std::int32_t truncated(double value)
        {
            value = std::trunc(value);
            if (!std::isfinite(value) || value < std::numeric_limits<std::int32_t>::min()
                || value > std::numeric_limits<std::int32_t>::max())
                throw std::invalid_argument("native actor-value integer overflow");
            return static_cast<std::int32_t>(value);
        }
    }

    std::optional<std::uint8_t> actorValueIndex(std::string_view name)
    {
        static constexpr std::array<std::string_view, 72> names{
            "strength", "intelligence", "willpower", "agility", "speed", "endurance", "personality", "luck",
            "health", "magicka", "fatigue", "encumbrance", "armorer", "athletics", "blade", "block", "blunt",
            "handtohand", "heavyarmor", "alchemy", "alteration", "conjuration", "destruction", "illusion",
            "mysticism", "restoration", "acrobatics", "lightarmor", "marksman", "mercantile", "security", "sneak",
            "speechcraft", "aggression", "confidence", "energy", "responsibility", "bounty", "fame", "infamy",
            "magickamultiplier", "nighteyebonus", "attackbonus", "defendbonus", "castingpenalty", "blindness",
            "chameleon", "invisibility", "paralysis", "silence", "confusion", "detectitemrange", "spellabsorbchance",
            "spellreflectchance", "swimspeedmultiplier", "waterbreathing", "waterwalking", "stuntedmagicka",
            "detectliferange", "reflectdamage", "telekinesis", "resistfire", "resistfrost", "resistdisease",
            "resistmagic", "resistnormalweapons", "resistparalysis", "resistpoison", "resistshock", "vampirism",
            "darkness", "resistwaterdamage"};
        for (std::uint8_t i = 0; i < names.size(); ++i)
            if (std::equal(name.begin(), name.end(), names[i].begin(), names[i].end(), [](char a, char b) {
                    return (a >= 'A' && a <= 'Z' ? static_cast<char>(a - 'A' + 'a') : a) == b;
                }))
                return i;
        return std::nullopt;
    }

    void validateActorValueState(const ActorValueState& state)
    {
        if (!std::isfinite(state.mBase))
            throw std::invalid_argument("nonfinite native actor-value base");
        for (const auto& value : state.mModifiers)
            if (value && !std::isfinite(*value))
                throw std::invalid_argument("nonfinite native actor-value modifier");
    }

    float composeActorValue(const ActorValueState& state, ActorValueOwner owner, ActorValueProcess process)
    {
        validateActorValueState(state);
        validateOwner(owner);
        if (process != ActorValueProcess::Low && process != ActorValueProcess::Active)
            throw std::invalid_argument("invalid native actor-value process");
        const double maximum = state.mModifiers[0].value_or(0.f);
        const double script = state.mModifiers[1].value_or(0.f);
        const double damage = state.mModifiers[2].value_or(0.f);
        if (owner == ActorValueOwner::Player)
            return stored(double(state.mBase) + maximum + script + damage);
        const float low = stored(double(state.mBase) + script + damage);
        return process == ActorValueProcess::Low ? low : stored(double(low) + maximum);
    }

    float composeNonPlayerActorValue(std::int32_t base, const ActorValueModifiers& modifiers,
        ActorValueProcess process)
    {
        validateActorValueState({0, modifiers});
        if (process != ActorValueProcess::Low && process != ActorValueProcess::Active)
            throw std::invalid_argument("invalid native actor-value process");
        const float low = stored(double(base) + modifiers[1].value_or(0.f) + modifiers[2].value_or(0.f));
        return process == ActorValueProcess::Low ? low : stored(double(low) + modifiers[0].value_or(0.f));
    }

    ActorValueState changeActorValueModifier(
        const ActorValueState& state, ActorValueOwner owner, ActorValueModifier modifier, float delta)
    {
        validateActorValueState(state);
        validateOwner(owner);
        const auto index = static_cast<unsigned>(modifier);
        if (index >= state.mModifiers.size())
            throw std::invalid_argument("invalid native actor-value modifier category");
        ActorValueState result = state;
        const bool allowPositive = modifier != ActorValueModifier::Damage;
        if (owner == ActorValueOwner::Player)
            result.mModifiers[index] = addActorValueModifier(state.mModifiers[index].value_or(0.f), delta, allowPositive);
        else
            result.mModifiers[index] = addSparseActorValueModifier(state.mModifiers[index], delta, allowPositive);
        return result;
    }

    std::int32_t composeIntegerActorValue(std::int32_t base, const ActorValueModifiers& modifiers,
        ActorValueOwner owner, ActorValueProcess process)
    {
        validateActorValueState({0, modifiers});
        validateOwner(owner);
        if (process != ActorValueProcess::Low && process != ActorValueProcess::Active)
            throw std::invalid_argument("invalid native actor-value process");
        const double maximum = modifiers[0].value_or(0.f);
        const double script = modifiers[1].value_or(0.f);
        const double damage = modifiers[2].value_or(0.f);
        if (owner == ActorValueOwner::Player)
            return truncated(double(base) + maximum + script + damage);
        const auto low = truncated(double(base) + script + damage);
        return process == ActorValueProcess::Low ? low : truncated(double(low) + maximum);
    }

    std::int32_t actorBaseValueInteger(const ActorBaseValueSet& value)
    {
        return std::visit([](auto storedValue) { return truncated(double(storedValue)); }, value.mValue);
    }

    std::optional<ActorBaseValueSet> prepareActorBaseValueSet(
        ActorBaseKind kind, std::uint8_t actorValue, std::int32_t requested)
    {
        if ((kind != ActorBaseKind::Npc && kind != ActorBaseKind::Creature) || actorValue >= 72)
            throw std::invalid_argument("invalid native base actor-value setter");
        if (actorValue == 11 || (actorValue >= 37 && actorValue <= 39))
            return std::nullopt;
        if (kind == ActorBaseKind::Creature && actorValue >= 12 && actorValue <= 32)
            actorValue = actorValue <= 18 || actorValue == 28 ? 12 : actorValue <= 25 ? 19 : 26;
        if (actorValue < 8 || (actorValue >= 12 && actorValue <= 36))
            return ActorBaseValueSet{actorValue, std::int32_t(static_cast<std::uint8_t>(requested))};
        if (actorValue == 9 || actorValue == 10)
            return ActorBaseValueSet{actorValue, std::int32_t(static_cast<std::uint16_t>(requested))};
        if (actorValue == 8)
            return ActorBaseValueSet{actorValue, requested};
        return ActorBaseValueSet{actorValue, static_cast<float>(requested)};
    }

    float forceActorValueDelta(std::int32_t requested, float current)
    {
        if (!std::isfinite(current))
            throw std::invalid_argument("nonfinite native ForceAV current value");
        return stored(double(requested) - current);
    }

    std::optional<ActorValueCommandChange> prepareActorValueModifierCommand(ActorValueOwner owner,
        std::uint8_t value, ActorValueCommand command, ActorValueCommandSource source,
        std::int32_t requested, float current, const ActorValueCommandPolicy& policy)
    {
        if ((owner != ActorValueOwner::Player && owner != ActorValueOwner::NonPlayer) || value >= 72
            || (command != ActorValueCommand::Mod && command != ActorValueCommand::Force)
            || (source != ActorValueCommandSource::Script && source != ActorValueCommandSource::Console))
            throw std::invalid_argument("invalid native modifier command");
        const float raw = command == ActorValueCommand::Force
            ? forceActorValueDelta(requested, current) : static_cast<float>(requested);
        // Eligibility precedes the integer wrapper's conversion. A positive
        // request rounded beyond int32 can become negative after this gate.
        const bool negative = command == ActorValueCommand::Mod ? requested < 0 : raw < 0;
        if (negative && ((owner == ActorValueOwner::Player && policy.mGodMode && value >= 8 && value <= 10)
                || (owner == ActorValueOwner::NonPlayer && value == 10 && !policy.mCanSpendFatigue)))
            return std::nullopt;
        // Both original CPU conversion paths return INT_MIN for rounded 2^31.
        // Other int32 inputs round to an exactly integral, in-range float.
        const float delta = command == ActorValueCommand::Mod && raw == 0x1p31f ? -0x1p31f : raw;
        return ActorValueCommandChange{source == ActorValueCommandSource::Console
                ? ActorValueModifier::Damage : ActorValueModifier::Script,
            delta, value == 8 && delta < 0};
    }

    float actorMagickaScale(float multiplier)
    {
        if (!std::isfinite(multiplier))
            throw std::invalid_argument("nonfinite native magicka multiplier");
        const float scale = stored(double(multiplier) / 10.0);
        return scale == 0.f ? 1.f : scale;
    }

    float scaleNpcMagicka(float processValue, float multiplier)
    {
        if (!std::isfinite(processValue))
            throw std::invalid_argument("nonfinite native process magicka");
        return stored(double(processValue) * actorMagickaScale(multiplier));
    }

    std::int32_t scaleNpcIntegerMagicka(std::int32_t processValue, float multiplier)
    {
        return truncated(double(processValue) * actorMagickaScale(multiplier));
    }

    float dynamicActorValueMaximum(std::int32_t base, float maximumModifier,
        ActorValueOwner owner, ActorValueProcess process)
    {
        validateOwner(owner);
        if (process != ActorValueProcess::Low && process != ActorValueProcess::Active)
            throw std::invalid_argument("invalid native actor-value process");
        if (!std::isfinite(maximumModifier))
            throw std::invalid_argument("nonfinite native maximum modifier");
        const bool useMaximum = owner == ActorValueOwner::Player || process == ActorValueProcess::Active;
        return stored(double(base) + (useMaximum ? maximumModifier : 0.f));
    }

    float calculatePlayerDynamicBaseValue(
        const PlayerDynamicBaseInput& input, const PlayerDynamicBaseSettings& settings)
    {
        for (const float value : {settings.mHealthMultiplier, settings.mMagickaMultiplier,
                 settings.mStrengthEncumbranceMultiplier, input.mMagickaMultiplier})
            if (!std::isfinite(value))
                throw std::invalid_argument("nonfinite native player dynamic base input");
        const auto integerAdjustment = [](double value) { return static_cast<float>(truncated(value)); };
        const auto& attributes = input.mCurrentAttributes;
        float adjustment;
        float scale = 1.f;
        switch (input.mValue)
        {
            case DynamicActorValue::Health:
                adjustment = integerAdjustment(double(attributes[5]) * settings.mHealthMultiplier);
                break;
            case DynamicActorValue::Magicka:
                adjustment = integerAdjustment(double(attributes[1]) * settings.mMagickaMultiplier + attributes[1]);
                scale = actorMagickaScale(input.mMagickaMultiplier);
                break;
            case DynamicActorValue::Fatigue:
            {
                // Original ADD instructions wrap before FILD and the float store.
                const std::uint32_t sum = std::uint32_t(attributes[0]) + std::uint32_t(attributes[2])
                    + std::uint32_t(attributes[3]) + std::uint32_t(attributes[5]);
                adjustment = static_cast<float>(std::bit_cast<std::int32_t>(sum));
                break;
            }
            case DynamicActorValue::Encumbrance:
                adjustment = std::max(0.f, stored(double(static_cast<float>(attributes[0]))
                    * settings.mStrengthEncumbranceMultiplier));
                break;
            default:
                throw std::invalid_argument("unsupported native player dynamic base actor value");
        }
        return stored((double(input.mFormValue) + adjustment) * scale);
    }
}
