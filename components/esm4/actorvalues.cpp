#include "actorvalues.hpp"
#include "actorstats.hpp"

#include <cmath>
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
}
