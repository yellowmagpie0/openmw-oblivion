#ifndef OPENMW_MECHANICS_AITIMER_H
#define OPENMW_MECHANICS_AITIMER_H

#include <stdexcept>

#include <components/misc/rng.hpp>
#include <components/misc/timer.hpp>

namespace MWMechanics
{
    constexpr float AI_REACTION_TIME = 0.25f;

    class AiReactionTimer
    {
    public:
        static constexpr float sDeviation = 0.1f;

        // Detached restoration must not draw from or borrow the outgoing PRNG.
        AiReactionTimer() : mImpl{ AI_REACTION_TIME, sDeviation, 0.f } {}

        AiReactionTimer(Misc::Rng::Generator& prng) : AiReactionTimer() { initialize(prng); }

        void initialize(Misc::Rng::Generator& prng)
        {
            if (mPrng)
                throw std::logic_error("AI reaction timer already initialized");
            mImpl.reset(Misc::Rng::deviate(0, sDeviation, prng));
            mPrng = &prng;
        }

        Misc::TimerStatus update(float duration) { return mImpl.update(duration, generator()); }

        void reset() { mImpl.reset(Misc::Rng::deviate(0, sDeviation, generator())); }

    private:
        Misc::Rng::Generator& generator() const
        {
            if (!mPrng)
                throw std::logic_error("Detached AI reaction timer used before installation");
            return *mPrng;
        }
        Misc::Rng::Generator* mPrng = nullptr;
        Misc::DeviatingPeriodicTimer mImpl;
    };
}

#endif
