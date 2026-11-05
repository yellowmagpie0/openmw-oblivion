#include "oblivioncombatmagic.hpp"

#include "esmstore.hpp"
#include "worldimp.hpp"
#include "class.hpp"
#include "player.hpp"
#include "inventorystore.hpp"
#include "scene.hpp"
#include "projectilemanager.hpp"
#include "oblivionprofileservices.hpp"
#include "oblivionactorstats.hpp"
#include "../mwbase/world.hpp"
#include "../mwmechanics/oblivioncombat.hpp"
#include "../mwmechanics/oblivionai.hpp"
#include "../mwmechanics/oblivionmelee.hpp"
#include "../mwmechanics/creaturestats.hpp"
#include "../mwphysics/physicssystem.hpp"
#include "../mwphysics/collisiontype.hpp"
#include <components/debug/debuglog.hpp>
#include <components/settings/values.hpp>

#include <components/esm4/combatmagicprofile.hpp>
#include <components/esm4/common.hpp>
#include <components/esm4/loadmgef.hpp>
#include <components/esm4/loadspel.hpp>
#include <components/esm4/combatsettings.hpp>
#include <components/esm4/loadgmst.hpp>
#include <components/esm4/loadench.hpp>
#include <components/esm4/loadweap.hpp>

#include <bit>
#include <set>
#include <cmath>
#include <sstream>
#include <limits>
#include <algorithm>
#include <components/esm4/loadalch.hpp>

namespace MWWorld
{
    OblivionSpellPresentation describeOblivionPlayerSpell(MWBase::World& world, const ESM4::Spell& spell)
    {
        const auto* native = dynamic_cast<const World*>(&world);
        const auto* combat = native ? native->getOblivionCombatService() : nullptr;
        if (!combat) throw std::logic_error("native spell display requires actor authority");
        std::array<std::int32_t, 6> skills;
        for (std::size_t i = 0; i < skills.size(); ++i)
            skills[i] = combat->getPlayerIntegerValue(static_cast<std::uint8_t>(20 + i));
        return describeOblivionSpell(world.getStore(), spell, skills, combat->getPlayerIntegerValue(7));
    }

    OblivionSpellPresentation describeOblivionSpell(const ESMStore& store, const ESM4::Spell& spell,
        const std::array<std::int32_t, 6>& schoolSkills, std::int32_t luck)
    {
        OblivionSpellPresentation result;
        if (!spell.mData || (spell.mFlags & ESM4::Rec_Deleted)) return result;
        std::vector<const ESM4::GameSetting*> settings;
        std::set<ESM::FormId> seenSettings;
        for (const auto& record : store.get<ESM4::GameSetting>())
            if (seenSettings.insert(record.mId).second)
                if (const auto* winner = store.get<ESM4::GameSetting>().search(record.mId);
                    winner && !(winner->mFlags & ESM4::Rec_Deleted)) settings.push_back(winner);
        const auto costs = ESM4::buildSpellCostSettings(settings);
        bool knownCost = spell.mData->mType == 0 && !spell.mEffects.empty();
        float total = 0;
        std::ostringstream description;
        for (const auto& effect : spell.mEffects)
        {
            const ESM4::EffectSetting* definition = nullptr;
            std::set<ESM::FormId> seen;
            for (const auto& record : store.get<ESM4::EffectSetting>())
            {
                if (!seen.insert(record.mId).second) continue;
                const auto* winner = store.get<ESM4::EffectSetting>().search(record.mId);
                if (!winner || winner->mEffectCode != effect.mId || (winner->mFlags & ESM4::Rec_Deleted)) continue;
                if (definition) throw std::invalid_argument("ambiguous native spell effect definition");
                definition = winner;
            }
            if (!definition || !definition->mData || effect.mScriptEffect || effect.mRange > 2)
            {
                knownCost = false;
                continue;
            }
            const auto& data = *definition->mData;
            if (description.tellp() > 0) description << '\n';
            description << definition->mFullName;
            if (!(data.mFlags & 0x100)) description << " " << effect.mMagnitude << " pts";
            if (!(data.mFlags & 0x80) && effect.mDuration) description << " for " << effect.mDuration << " sec";
            if (!(data.mFlags & 0x200) && effect.mRange && effect.mArea)
                description << " in " << effect.mArea << " ft";
            description << (effect.mRange == 0 ? " on Self" : effect.mRange == 1 ? " on Touch" : " on Target");
            if (result.mIcon.empty())
                for (const auto& sub : definition->mSubRecords)
                    if (sub.mType == ESM::fourCC("ICON") && !sub.mData.empty())
                    {
                        result.mIcon.assign(reinterpret_cast<const char*>(sub.mData.data()), sub.mData.size());
                        if (const auto end = result.mIcon.find('\0'); end != std::string::npos) result.mIcon.resize(end);
                    }
            if (data.mSchool >= schoolSkills.size()) { knownCost = false; continue; }
            if (spell.mData->mFlags & 1)
            {
                // Multi-effect manual spells need highest-cost school selection.
                if (spell.mEffects.size() != 1) knownCost = false;
                else total = ESM4::manualSpellMagickaCost(spell.mData->mCost, schoolSkills[data.mSchool], luck, costs);
            }
            else if (!(data.mFlags & 0x400000))
                total += ESM4::spellEffectMagickaCost({data.mBaseCost,
                    data.mFlags & 0x100 ? 0u : effect.mMagnitude,
                    data.mFlags & 0x80 ? 0u : effect.mDuration,
                    (data.mFlags & 0x200) || !effect.mRange ? 0u : effect.mArea,
                    effect.mRange == 2}, schoolSkills[data.mSchool], luck, costs);
        }
        result.mDescription = description.str();
        if (knownCost && std::isfinite(total)) result.mCost = total;
        return result;
    }

    static std::optional<std::array<float, 3>> resolveInstantRestoration(
        const ESMStore& store, const std::vector<ESM4::SpellEffect>& effects)
    {
        if (effects.empty()) return std::nullopt;
        std::array<float, 3> amounts{};
        for (const auto& effect : effects)
        {
            const int resource = effect.mId == ESM::fourCC("REHE") ? 0
                : effect.mId == ESM::fourCC("RESP") ? 1 : effect.mId == ESM::fourCC("REFA") ? 2 : -1;
            if (resource < 0 || effect.mDuration || effect.mArea || effect.mRange || effect.mScriptEffect)
                return std::nullopt;
            const auto& definitions = store.get<ESM4::EffectSetting>();
            const ESM4::EffectSetting* definition = nullptr;
            std::set<ESM::FormId> seen;
            for (const auto& record : definitions)
            {
                if (!seen.insert(record.mId).second) continue;
                const auto* candidate = definitions.search(record.mId);
                if (!candidate || candidate->mEffectCode != effect.mId) continue;
                if (const auto key = definitions.findFormKey(ESM::RefId(candidate->mId)))
                    if (const auto* winner = store.getFormKeyIndex().winner(*key);
                        winner && (winner->mDeleted || winner->mRecordType != ESM4::REC_MGEF)) continue;
                if (definition) return std::nullopt;
                definition = candidate;
            }
            // Admit the original restoration definition. Altered effect classes
            // need their own execution semantics, not a guessed resource write.
            if (!definition || !definition->mData || (definition->mFlags & ESM4::Rec_Deleted))
                return std::nullopt;
            const auto& data = *definition->mData;
            if (data.mFlags != 0x01000870 || data.mAssociatedForm
                || data.mAssociatedData != static_cast<unsigned>(resource + 8)
                || data.mSchool != 5 || data.mResistanceActorValue != -1)
                return std::nullopt;
            amounts[resource] += static_cast<float>(effect.mMagnitude);
        }
        return amounts;
    }

    std::optional<std::array<float, 3>> resolveOblivionPotionRestoration(
        const ESMStore& store, const ESM4::Potion& potion)
    {
        if ((potion.mFlags & ESM4::Rec_Deleted) || !potion.mScriptId.isZeroOrUnset()) return {};
        return resolveInstantRestoration(store, potion.mEffects);
    }

    std::optional<std::array<float, 3>> resolveOblivionSpellRestoration(
        const ESMStore& store, const ESM4::Spell& spell)
    {
        if ((spell.mFlags & ESM4::Rec_Deleted) || !spell.mData || spell.mData->mType != 0) return {};
        return resolveInstantRestoration(store, spell.mEffects);
    }

    static std::optional<OblivionTargetDamage> resolveTargetDamage(
        const ESMStore& store, const std::vector<ESM4::SpellEffect>& effects)
    {
        if (effects.size() != 1) return {};
        const auto& effect = effects.front();
        const int resistance = effect.mId == ESM::fourCC("SHDG") ? 68
            : effect.mId == ESM::fourCC("FIDG") ? 61 : -1;
        if (resistance < 0 || effect.mRange != 2 || effect.mDuration || effect.mArea
            || effect.mActorValue != 8 || effect.mScriptEffect) return {};
        const ESM4::EffectSetting* definition = nullptr;
        std::set<ESM::FormId> seen;
        for (const auto& row : store.get<ESM4::EffectSetting>())
        {
            if (!seen.insert(row.mId).second) continue;
            const auto* winner = store.get<ESM4::EffectSetting>().search(row.mId);
            if (!winner || (winner->mFlags & ESM4::Rec_Deleted) || winner->mEffectCode != effect.mId) continue;
            if (definition) return {}; // Ambiguous codes cannot select an arbitrary asset or rule.
            definition = winner;
        }
        if (!definition || !definition->mData) return {};
        const auto& data = *definition->mData;
        if (data.mAssociatedData != 8 || data.mSchool != 2 || data.mResistanceActorValue != resistance
            || (data.mFlags & 0x41) != 0x41 || (data.mFlags & 0x100)) return {};
        std::optional<float> speed;
        std::optional<std::string> model;
        for (const auto& sub : definition->mSubRecords)
        {
            if (sub.mType == ESM::fourCC("DATA"))
            {
                if (speed || sub.mData.size() < 32) return {};
                const auto& bytes = sub.mData;
                const std::uint32_t bits = std::uint32_t(bytes[28]) | (std::uint32_t(bytes[29]) << 8)
                    | (std::uint32_t(bytes[30]) << 16) | (std::uint32_t(bytes[31]) << 24);
                speed = std::bit_cast<float>(bits);
            }
            else if (sub.mType == ESM::fourCC("MODL"))
            {
                if (model || sub.mData.size() < 2 || sub.mData.back() != 0) return {};
                model.emplace(reinterpret_cast<const char*>(sub.mData.data()), sub.mData.size() - 1);
                if (model->find('\0') != std::string::npos) return {};
            }
        }
        if (!speed || !std::isfinite(*speed) || *speed <= 0 || !model || model->empty()) return {};
        return OblivionTargetDamage{effect.mId, static_cast<float>(effect.mMagnitude),
            static_cast<std::uint8_t>(resistance), *speed, std::move(*model)};
    }

    std::optional<OblivionTargetDamage> resolveOblivionTargetSpellDamage(
        const ESMStore& store, const ESM4::Spell& spell)
    {
        if ((spell.mFlags & ESM4::Rec_Deleted) || !spell.mData || spell.mData->mType != 0) return {};
        return resolveTargetDamage(store, spell.mEffects);
    }

    std::optional<OblivionStaffCastInputs> resolveOblivionStaffCastInputs(
        const ESMStore& store, const ESM4::Weapon& weapon)
    {
        if ((weapon.mFlags & ESM4::Rec_Deleted) || weapon.mData.type != 4
            || !weapon.mScriptId.isZeroOrUnset()) return {};
        const auto* enchantment = store.get<ESM4::Enchantment>().search(weapon.mEnchantment);
        if (!enchantment || (enchantment->mFlags & ESM4::Rec_Deleted) || !enchantment->mData
            || enchantment->mData->mType != 1) return {};
        const auto damage = resolveTargetDamage(store, enchantment->mEffects);
        if (!damage) return {};
        return OblivionStaffCastInputs{enchantment->mFormKey,
            static_cast<float>(enchantment->mData->mCost), static_cast<float>(weapon.mEnchantmentPoints), *damage};
    }

    namespace
    {
        std::vector<const ESM4::GameSetting*> magicSettings(const ESMStore& store)
        {
            std::vector<const ESM4::GameSetting*> result;
            std::set<ESM::FormId> seen;
            for (const auto& row : store.get<ESM4::GameSetting>())
                if (seen.insert(row.mId).second)
                    if (const auto* winner = store.get<ESM4::GameSetting>().search(row.mId);
                        winner && !(winner->mFlags & ESM4::Rec_Deleted)) result.push_back(winner);
            return result;
        }

        float projectileSetting(const std::vector<const ESM4::GameSetting*>& settings,
            std::string_view name, float fallback)
        {
            bool found = false;
            for (const auto* setting : settings)
                if (setting->mEditorId == name)
                {
                    const auto* value = std::get_if<float>(&setting->mData);
                    if (found || !value || !std::isfinite(*value) || *value <= 0)
                        throw std::invalid_argument("invalid or ambiguous native projectile setting");
                    found = true; fallback = *value;
                }
            return fallback;
        }
    }

    bool World::releaseOblivionStaffProjectile(const Ptr& actor, const ESM::FormKey& enchantment, float chargeCost)
    {
        if (mGameProfile != ESM::GameProfile::Oblivion || !mOblivionCombat || !mPhysics
            || actor.isEmpty() || !actor.isInCell() || !actor.getCell()->getCell()->isEsm4()) return false;
        auto& inventory = actor.getClass().getInventoryStore(actor);
        const auto item = inventory.getSlot(InventoryStore::Slot_CarriedRight);
        if (item == inventory.end()) return false;
        const auto id = OblivionProfileServices::nativeItemId(mStore, item->getCellRef().getRefId());
        const auto* weapon = mStore.get<ESM4::Weapon>().search(id);
        const auto inputs = weapon ? resolveOblivionStaffCastInputs(mStore, *weapon) : std::nullopt;
        if (!inputs || inputs->mEnchantment != enchantment || !std::isfinite(chargeCost) || chargeCost < 0
            || inputs->mAuthoredChargeCost != chargeCost || !item->getCellRef().supportsNativeItemCondition()) return false;
        const float rawCharge = item->getCellRef().getEnchantmentCharge();
        const float charge = rawCharge == -1 ? inputs->mMaximumCharge : rawCharge;
        if (!std::isfinite(charge) || charge < chargeCost) return false;
        const bool player = actor == getPlayerPtr();
        const auto casterKey = player ? ESM::FormKey::dynamic("player", 1) : actor.getCellRef().getFormKey();
        osg::Vec3f position = actor.getRefData().getPosition().asVec3();
        position.z() += mPhysics->getRenderingHalfExtents(actor).z() * 1.5f;
        osg::Vec3f direction;
        if (player)
        {
            const auto& rotation = actor.getRefData().getPosition().rot;
            direction = (osg::Quat(rotation[0], osg::Vec3f(-1, 0, 0))
                * osg::Quat(rotation[2], osg::Vec3f(0, 0, -1))) * osg::Vec3f(0, 1, 0);
        }
        else
        {
            const auto targetKey = getOblivionCombatTarget(casterKey);
            if (!targetKey || targetKey->isNull() || !mOblivionAi) return false;
            const auto target = *targetKey == ESM::FormKey::dynamic("player", 1)
                ? getPlayerPtr() : mOblivionAi->resolveReference(*targetKey);
            if (target.isEmpty() || !target.isInCell() || !mPhysics->getActor(target)
                || target.getClass().getCreatureStats(target).isDead()) return false;
            auto aim = target.getRefData().getPosition().asVec3();
            aim.z() += mPhysics->getRenderingHalfExtents(target).z() * 1.5f;
            direction = aim - position;
        }
        if (direction.normalize() == 0) return false;
        const auto settings = magicSettings(mStore);
        // Verified original compiled defaults; the installed master has no overrides.
        const float speed = projectileSetting(settings, "fMagicProjectileBaseSpeed", 1000.f)
            * inputs->mDamage.mProjectileSpeedFactor;
        ESM4::MagicProjectileState projectile;
        projectile.mCaster = casterKey; projectile.mSource = enchantment;
        projectile.mCell = actor.getCell()->getCell()->getEsm4().mFormKey;
        projectile.mEffectCode = inputs->mDamage.mEffectCode; projectile.mMagnitude = inputs->mDamage.mMagnitude;
        for (std::size_t i = 0; i < 3; ++i) { projectile.mPosition[i] = position[i]; projectile.mVelocity[i] = direction[i] * speed; }
        projectile.mMaximumDistance = projectileSetting(settings, "fMagicProjectileMaxDistance", 10000.f);
        projectile.mModel = inputs->mDamage.mModel;
        projectile.validate();
        if (!mOblivionRuntimeState) mOblivionRuntimeState = std::make_unique<ESM4::RuntimeState>(captureOblivionRuntimeState());
        const auto serial = mOblivionRuntimeState->mNextMagicProjectile;
        if (!serial || serial == std::numeric_limits<std::uint64_t>::max())
            throw std::overflow_error("native magic projectile serial exhausted");
        auto projectiles = mOblivionRuntimeState->mNativeMagicProjectiles;
        if (!projectiles.emplace(serial, std::move(projectile)).second)
            throw std::logic_error("native magic projectile serial reused");
        auto nextCharge = item->getCellRef();
        nextCharge.setEnchantmentCharge(charge - chargeCost);
        // All allocation/validation precedes this no-throw publication. The
        // controller publishes its released flag immediately after return.
        item->getCellRef().swapNativeEnchantmentCharge(nextCharge);
        mOblivionRuntimeState->mNativeMagicProjectiles.swap(projectiles);
        mOblivionRuntimeState->mNextMagicProjectile = serial + 1;
        return true;
    }

    void World::updateOblivionMagicProjectiles(float duration)
    {
        if (mGameProfile != ESM::GameProfile::Oblivion || !mOblivionRuntimeState || !mOblivionCombat
            || !mPhysics || !mWorldScene || !mProjectileManager) return;
        auto& projectiles = mOblivionRuntimeState->mNativeMagicProjectiles;
        if (projectiles.empty() || duration == 0)
        {
            mProjectileManager->syncOblivionMagicProjectiles(projectiles);
            return;
        }
        const auto player = getPlayerPtr();
        const auto settings = magicSettings(mStore);
        for (auto it = projectiles.begin(); it != projectiles.end();)
        {
            const auto& projectile = it->second;
            const bool active = std::any_of(mWorldScene->getActiveCells().begin(), mWorldScene->getActiveCells().end(),
                [&](const auto* cell) { return cell->getCell()->isEsm4()
                    && cell->getCell()->getEsm4().mFormKey == projectile.mCell; });
            if (!active) { it = projectiles.erase(it); continue; }
            auto next = ESM4::advanceMagicProjectile(projectile, duration);
            const osg::Vec3f from(projectile.mPosition[0], projectile.mPosition[1], projectile.mPosition[2]);
            const osg::Vec3f to(next.mPosition[0], next.mPosition[1], next.mPosition[2]);
            const auto caster = projectile.mCaster == ESM::FormKey::dynamic("player", 1) ? player
                : mOblivionAi ? mOblivionAi->resolveReference(projectile.mCaster) : Ptr();
            const auto hit = mPhysics->castRay(from, to, caster.isEmpty() ? std::vector<ConstPtr>{} : std::vector<ConstPtr>{caster},
                {}, 0xff, MWPhysics::CollisionType_Projectile);
            if (hit.mHit)
            {
                const auto serial = it->first;
                float appliedDamage = 0;
                const auto& target = hit.mHitObject;
                if (!target.isEmpty() && target.getClass().isActor()
                    && !target.getClass().getCreatureStats(target).isDead())
                {
                    const bool targetPlayer = target == player;
                    const auto value = [&](std::uint8_t av) { return targetPlayer ? mOblivionCombat->getPlayerValue(av)
                        : mOblivionCombat->getNonPlayerValue(target, av); };
                    if (value(52) != 0 || value(53) != 0)
                        throw std::runtime_error("native projectile absorption and reflection are not implemented");
                    const auto resistance = projectile.mEffectCode == ESM::fourCC("SHDG") ? 68 : 61;
                    float damage = projectile.mMagnitude * std::max(0.f, 1.f - value(resistance) / 100.f)
                        * std::max(0.f, 1.f - value(64) / 100.f);
                    const auto role = targetPlayer ? ESM4::PlayerDamageRole::Victim
                        : caster == player ? ESM4::PlayerDamageRole::Attacker : ESM4::PlayerDamageRole::Unaffected;
                    damage = ESM4::difficultyDamage(damage,
                        MWMechanics::oblivionNormalizedDifficulty(Settings::game().mDifficulty),
                        ESM4::buildDifficultyDamageMultiplier(settings), role);
                    auto adoption = mOblivionCombat->guardLifeAdoption(target, targetPlayer ? mPlayer.get() : nullptr);
                    adoptOblivionActorLife(target);
                    const auto recovery = resolveOblivionEssentialRecoverySettings(mStore);
                    if (targetPlayer) mOblivionCombat->changePlayerHealth(*mPlayer, -damage, projectile.mCaster,
                        target.getClass().isEssential(target), recovery, resolveOblivionPlayerDynamicBaseSettings(mStore), getGodModeState());
                    else mOblivionCombat->changeNonPlayerHealth(target, -damage, projectile.mCaster,
                        target.getClass().isEssential(target), recovery);
                    adoption.commit();
                    appliedDamage = damage;
                }
                // Removal cannot throw after Health/death publication, so the
                // same contact is never retried even if a later frame fails.
                it = projectiles.erase(it);
                Log(Debug::Info) << "M16 magic projectile impact: id=" << serial
                    << " actor=" << (!target.isEmpty() && target.getClass().isActor()) << " damage=" << appliedDamage;
            }
            else if (next.mDistance >= next.mMaximumDistance) it = projectiles.erase(it);
            else { std::swap(it->second, next); ++it; }
        }
        mProjectileManager->syncOblivionMagicProjectiles(projectiles);
    }

    void World::validateOblivionMagicProjectiles(const ESM4::RuntimeState& state) const
    {
        for (const auto& [id, projectile] : state.mNativeMagicProjectiles)
        {
            std::optional<OblivionTargetDamage> damage;
            if (const auto* spell = mStore.get<ESM4::Spell>().search(projectile.mSource))
                damage = resolveOblivionTargetSpellDamage(mStore, *spell);
            else if (const auto* enchantment = mStore.get<ESM4::Enchantment>().search(projectile.mSource);
                enchantment && !(enchantment->mFlags & ESM4::Rec_Deleted)
                && enchantment->mData && enchantment->mData->mType == 1)
                damage = resolveTargetDamage(mStore, enchantment->mEffects);
            const auto* cell = mStore.get<ESM4::Cell>().search(projectile.mCell);
            if (!damage || !cell || (cell->mFlags & ESM4::Rec_Deleted)
                || damage->mEffectCode != projectile.mEffectCode || damage->mModel != projectile.mModel)
                throw std::invalid_argument("saved native magic projectile content no longer resolves");
        }
    }

    bool supportsOblivionDefaultCombatParalysis(const ESMStore& store)
    {
        const auto key = ESM::FormKey::content("Oblivion.esm", 0x137);
        const auto* metadata = store.getFormKeyIndex().winner(key);
        const ESM4::Spell* spell = nullptr;
        // Original41C81B dynamically casts the lookup result to SpellItem;
        // absent, deleted or other-type forms select the fallback constructor.
        if (!metadata || (!metadata->mDeleted && metadata->mRecordType == ESM4::REC_SPEL))
        {
            spell = store.get<ESM4::Spell>().search(key);
            if (metadata && !spell)
                return false; // Indexed winning Spell must have a typed definition.
        }
        const auto& effects = store.get<ESM4::EffectSetting>();
        const ESM4::EffectSetting* paralysis = nullptr;
        std::set<ESM::FormId> seen;
        for (const auto& record : effects)
        {
            if (!seen.insert(record.mId).second)
                continue;
            const auto* effect = effects.search(record.mId);
            if (!effect || effect->mEffectCode != ESM::fourCC("PARA"))
                continue;
            if (const auto identity = effects.findFormKey(ESM::RefId(effect->mId)))
                if (const auto* winner = store.getFormKeyIndex().winner(*identity);
                    winner && (winner->mDeleted || winner->mRecordType != ESM4::REC_MGEF))
                    continue;
            if (paralysis)
                return false; // Do not guess an order between different keys with the same code.
            paralysis = effect;
        }
        return ESM4::supportsDefaultCombatParalysisProfile(spell, paralysis);
    }
}
