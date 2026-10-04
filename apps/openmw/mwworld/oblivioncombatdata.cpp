#include "oblivioncombatdata.hpp"

#include "esmstore.hpp"
#include <components/esm/records.hpp>
#include <components/esm4/combatstylepolicy.hpp>

#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>
#include <vector>

namespace MWWorld
{
    OblivionArrowLaunch resolveOblivionArrowLaunch(const ESMStore& store,
        ESM::GameProfile profile, const OblivionArrowLaunchInput& input)
    {
        if (profile != ESM::GameProfile::Oblivion)
            throw std::invalid_argument("native arrow launch requires the Oblivion profile");
        if (input.mBow.isNull() || input.mAmmunition.isNull())
            throw std::invalid_argument("native arrow launch requires stable item identities");
        const auto* bow = store.search<ESM4::Weapon>(input.mBow);
        const auto* ammunition = store.search<ESM4::Ammunition>(input.mAmmunition);
        if (!bow || bow->mData.type != 5 || !ammunition)
            throw std::invalid_argument("native arrow launch requires a winning TES4 bow and ammunition");
        const float ammoDamage = ammunition->mData.mDamage;
        // TES4 AMMO DATA stores damage as uint16; later-game layouts use
        // float. Reject values incompatible with TES4 damage before narrowing.
        if (!std::isfinite(ammoDamage) || ammoDamage < 0
            || ammoDamage > std::numeric_limits<std::uint16_t>::max()
            || std::trunc(ammoDamage) != ammoDamage)
            throw std::invalid_argument("native ammunition damage is not uint16");

        std::vector<const ESM4::GameSetting*> settings;
        const auto& native = store.get<ESM4::GameSetting>();
        std::set<ESM::FormId> seen;
        for (const auto& setting : native)
            if (seen.insert(setting.mId).second)
                settings.push_back(native.search(setting.mId));

        const auto projectile = ESM4::buildProjectileSettings(settings);
        const auto physical = ESM4::buildPhysicalCombatSettings(settings);
        const auto fatigue = ESM4::buildBowFatigueSettings(settings);
        const auto mastery = ESM4::buildCombatMasterySettings(settings);
        const float draw = input.mPlayer ? ESM4::bowDrawFraction(input.mBowTimer, projectile) : 1.f;
        const ESM4::ArrowDamageInput damage{input.mMarksman, input.mLuck, input.mAgility,
            bow->mData.damage, static_cast<std::uint16_t>(ammoDamage),
            input.mBowConditionRatio, input.mFatigueRatio, draw, input.mAttackBonus};
        return {input.mBow, input.mAmmunition, draw,
            ESM4::arrowLaunchDamage(damage, physical),
            ESM4::arrowLaunchSpeed(ammunition->mData.mSpeed, draw, projectile),
            ESM4::arrowGravityFactor(input.mMarksman, input.mLuck, draw, projectile, physical),
            ESM4::bowShotFatigue(input.mMasteryMarksman, fatigue, mastery)};
    }

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
