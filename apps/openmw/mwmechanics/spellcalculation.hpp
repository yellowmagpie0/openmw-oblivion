#ifndef OPENMW_MWMECHANICS_SPELLCALCULATION_H
#define OPENMW_MWMECHANICS_SPELLCALCULATION_H

#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include <components/esm3/loadmgef.hpp>
#include <components/esm3/loadspel.hpp>
#include "../mwworld/esmstore.hpp"

namespace MWMechanics
{
    template <class T> struct SpellCalculationSetting
    {
        const char* mName;
        std::optional<T> mValue;
    };

    struct SpellCalculationCaches
    {
        SpellCalculationSetting<float> mNpcMagicka{"fNPCbaseMagickaMult", {}};
        SpellCalculationSetting<int> mTimesCanCast{"iAutoSpellTimesCanCast", {}};
        SpellCalculationSetting<float> mNpcChance{"fAutoSpellChance", {}};
        SpellCalculationSetting<int> mAttributeSkillMinimum{"iAutoSpellAttSkillMin", {}};
        // These were independent function-local statics, despite sharing a name.
        SpellCalculationSetting<float> mWeakestSchoolCost{"fEffectCostMult", {}};
        SpellCalculationSetting<float> mEffectCost{"fEffectCostMult", {}};
        SpellCalculationSetting<float> mAlchemyCost{"iAlchemyMod", {}};
    };

    inline SpellCalculationCaches& spellCalculationCaches()
    {
        static SpellCalculationCaches result;
        return result;
    }

    // Calculation against a future store never initializes live first-use caches.
    // Commit callbacks capture only persistent cache addresses and numeric values.
    class SpellCalculationContext
    {
        struct Captured
        {
            std::variant<std::optional<int>*, std::optional<float>*> mTarget;
            std::variant<int, float> mValue;
        };
        const MWWorld::ESMStore& mStore;
        const MWWorld::ESMStore* mIncoming;
        std::vector<Captured> mCaptured;

    public:
        explicit SpellCalculationContext(const MWWorld::ESMStore& store, const MWWorld::ESMStore* incoming = nullptr)
            : mStore(store), mIncoming(incoming)
        {
        }
        const MWWorld::ESMStore& store() const { return mStore; }
        bool isPreparing() const { return mIncoming != nullptr; }

        template <class T> T setting(SpellCalculationSetting<T>& slot)
        {
            if (slot.mValue)
                return *slot.mValue;
            if (isPreparing())
                for (const auto& captured : mCaptured)
                    if (const auto* target = std::get_if<std::optional<T>*>(&captured.mTarget);
                        target && *target == &slot.mValue)
                        return std::get<T>(captured.mValue);
            const auto& settings = mStore.get<ESM::GameSetting>();
            const auto* record = isPreparing()
                ? settings.searchStatic(ESM::RefId::stringRefId(slot.mName)) : settings.find(slot.mName);
            if (!record)
                throw std::runtime_error(std::string("Prepared spell setting is unavailable: ") + slot.mName);
            const T value = [&] {
                if constexpr (std::is_same_v<T, int>) return record->mValue.getInteger();
                else return record->mValue.getFloat();
            }();
            if (isPreparing())
                mCaptured.push_back({&slot.mValue, value});
            else
                slot.mValue = value;
            return value;
        }

        const ESM::MagicEffect* effect(const ESM::RefId& id) const
        {
            const auto& effects = mStore.get<ESM::MagicEffect>();
            const auto* record = isPreparing() ? effects.searchStatic(id) : effects.find(id);
            if (!record)
                throw std::runtime_error("Prepared spell magic effect is unavailable");
            return record;
        }

        const ESM::Spell* spell(const ESM::RefId& id) const
        {
            const auto* record = isPreparing() ? mStore.searchForRestore<ESM::Spell>(id, *mIncoming)
                                              : mStore.get<ESM::Spell>().find(id);
            if (!record)
                throw std::runtime_error("Prepared autocalculated spell is unavailable");
            return record;
        }

        std::vector<const ESM::Spell*> spells() const
        {
            std::vector<const ESM::Spell*> result;
            result.reserve(mStore.get<ESM::Spell>().getSize()
                + (mIncoming ? mIncoming->get<ESM::Spell>().getSize() : 0));
            for (const auto& record : mStore.get<ESM::Spell>())
                if (!isPreparing() || mStore.get<ESM::Spell>().searchStatic(record.mId) == &record)
                    result.push_back(&record);
            if (mIncoming)
                for (const auto& record : mIncoming->get<ESM::Spell>())
                    result.push_back(&record);
            return result;
        }

        std::vector<std::function<void()>> cacheCommits() const
        {
            std::vector<std::function<void()>> result;
            result.reserve(mCaptured.size());
            for (const auto& captured : mCaptured)
                std::visit([&](auto* target) {
                    using T = typename std::decay_t<decltype(*target)>::value_type;
                    const auto value = std::get<T>(captured.mValue);
                    result.emplace_back([target, value] { if (!*target) *target = value; });
                }, captured.mTarget);
            return result;
        }
    };
}
#endif
