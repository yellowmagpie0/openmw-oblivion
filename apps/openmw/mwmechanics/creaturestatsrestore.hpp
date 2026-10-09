#ifndef OPENMW_MWMECHANICS_CREATURESTATSRESTORE_H
#define OPENMW_MWMECHANICS_CREATURESTATSRESTORE_H

#include "creaturestats.hpp"
#include <components/esm3/creaturestats.hpp>

namespace MWMechanics
{
    // Detached shared stat, spell payload, active-effect and AI-package plans.
    // Actor construction and shared SpellList attachment remain separate.
    class PreparedCreatureStats
    {
        friend class CreatureStats;
        ESM::CreatureStats mFields{};
        std::map<ESM::RefId, AttributeValue> mAttributes;
        MagicEffects mMagicEffects;
        std::unique_ptr<Spells::PreparedState> mSpells;
        std::unique_ptr<ActiveSpells::PreparedState> mActiveSpells;
        std::unique_ptr<AiSequence::PreparedState> mAiSequence;
        std::unique_ptr<AiSequence::PreparedFill> mBaseAiSequence;
        bool mConsumed = false;

        PreparedCreatureStats() = default;

    public:
        PreparedCreatureStats(const PreparedCreatureStats&) = delete;
        PreparedCreatureStats& operator=(const PreparedCreatureStats&) = delete;
        bool attachSpells(Spells& target, const ESM::RefId& actorId)
        {
            return mSpells ? mSpells->attach(target, actorId) : target.setSpells(actorId);
        }
        void setBaseAiSequence(std::unique_ptr<AiSequence::PreparedFill> plan) { mBaseAiSequence = std::move(plan); }
        bool installBaseAiSequence(AiSequence& target)
        {
            if (!mBaseAiSequence) return false;
            mBaseAiSequence->install(target);
            return true;
        }
        void install(CreatureStats& target);
    };
}

#endif
