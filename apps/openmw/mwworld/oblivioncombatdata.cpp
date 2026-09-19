#include "oblivioncombatdata.hpp"

#include "esmstore.hpp"
#include <components/esm/records.hpp>
#include <components/esm4/combatstylepolicy.hpp>

#include <set>
#include <stdexcept>
#include <vector>

namespace MWWorld
{
    ESM4::CombatStyleDefaults buildOblivionCombatDefaults(const ESMStore& store)
    {
        std::vector<const ESM4::GameSetting*> settings;
        const auto& native = store.get<ESM4::GameSetting>();
        std::set<ESM::FormId> seen;
        for (const auto& setting : native)
            if (seen.insert(setting.mId).second)
                settings.push_back(native.search(setting.mId));
        return ESM4::buildCombatStyleDefaults(settings);
    }

    OblivionCombatPolicy resolveOblivionCombatPolicy(const ESMStore& store,
        const ESM::FormKey& actorBase, const ESM4::CombatStyleDefaults& defaults)
    {
        const auto fail = [&](const std::string& reason) {
            throw std::runtime_error("Native combat policy for " + actorBase.serialize() + ": " + reason);
        };
        ESM::FormId styleId;
        if (actorBase.isNull())
            fail("null actor base");
        if (const auto* npc = store.search<ESM4::Npc>(actorBase))
        {
            if (!npc->mIsTES4)
                fail("actor is not a TES4 NPC");
            styleId = npc->mCombatStyle;
        }
        else if (const auto* creature = store.search<ESM4::Creature>(actorBase))
        {
            // RNAM is a native attack input. The later-game CREA path does not
            // decode it; do not manufacture reach for unsupported actor data.
            if (!creature->mAttackReach)
                fail("creature has no native attack reach");
            styleId = creature->mCombatStyle;
        }
        else
            fail("missing or unsupported actor base");

        OblivionCombatPolicy result;
        result.mActorBase = actorBase;
        const ESM4::CombatStyle* style = nullptr;
        if (!styleId.isZeroOrUnset())
        {
            const auto& styles = store.get<ESM4::CombatStyle>();
            style = styles.search(styleId);
            const auto key = styles.findFormKey(ESM::RefId(styleId));
            if (!style || !key || !style->mStandard)
                fail("missing, deleted or invalid combat style " + styleId.toString());
            result.mStyle = *key;
        }
        result.mStandard = ESM4::resolveCombatStyleStandard(
            style ? &*style->mStandard : nullptr, defaults.mStandard);
        result.mAdvanced = ESM4::resolveCombatStyleAdvanced(style, defaults.mAdvanced);
        return result;
    }
}
