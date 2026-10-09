#ifndef OPENMW_AUTOCALCSPELL_H
#define OPENMW_AUTOCALCSPELL_H

#include "creaturestats.hpp"
#include <components/esm/refid.hpp>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace ESM
{
    struct Spell;
    struct Race;
}

namespace MWMechanics
{

    /// Contains algorithm for calculating an NPC's spells based on stats
    /// @note We might want to move this code to a component later, so the editor can use it for preview purposes

    std::vector<ESM::RefId> autoCalcNpcSpells(const std::map<ESM::RefId, SkillValue>& actorSkills,
        const std::map<ESM::RefId, AttributeValue>& actorAttributes, const ESM::Race* race);

    std::vector<ESM::RefId> autoCalcPlayerSpells(const std::map<ESM::RefId, SkillValue>& actorSkills,
        const std::map<ESM::RefId, AttributeValue>& actorAttributes, const ESM::Race* race);

    class SpellCalculationContext;
    class PreparedNpcSpells
    {
        friend PreparedNpcSpells prepareNpcSpells(const std::map<ESM::RefId, SkillValue>&,
            const std::map<ESM::RefId, AttributeValue>&, const ESM::Race*,
            const MWWorld::ESMStore&, const MWWorld::ESMStore&, std::size_t);
        Spells::PreparedInstance mSpells;
        std::vector<std::function<void()>> mCacheCommits;
        bool mConsumed = false;
    public:
        void install(Spells& target);
    };
    PreparedNpcSpells prepareNpcSpells(const std::map<ESM::RefId, SkillValue>& actorSkills,
        const std::map<ESM::RefId, AttributeValue>& actorAttributes, const ESM::Race* race,
        const MWWorld::ESMStore& store, const MWWorld::ESMStore& incoming, std::size_t priorCapacity = 0);

    // Helpers

    bool attrSkillCheck(const ESM::Spell* spell, const std::map<ESM::RefId, SkillValue>& actorSkills,
        const std::map<ESM::RefId, AttributeValue>& actorAttributes, SpellCalculationContext* context = nullptr);

    void calcWeakestSchool(const ESM::Spell* spell, const std::map<ESM::RefId, SkillValue>& actorSkills,
        ESM::RefId& effectiveSchool, float& skillTerm, SpellCalculationContext* context = nullptr);

    float calcAutoCastChance(const ESM::Spell* spell, const std::map<ESM::RefId, SkillValue>& actorSkills,
        const std::map<ESM::RefId, AttributeValue>& actorAttributes, ESM::RefId effectiveSchool,
        SpellCalculationContext* context = nullptr);

}

#endif
