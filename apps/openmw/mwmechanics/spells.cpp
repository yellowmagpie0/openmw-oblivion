#include "spells.hpp"

#include <stdexcept>

#include <components/debug/debuglog.hpp>
#include <components/esm3/loadspel.hpp>
#include <components/esm3/spellstate.hpp>

#include <components/esm3/loadmgef.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"

#include "../mwworld/class.hpp"
#include "../mwworld/esmstore.hpp"

#include "actorutil.hpp"
#include "creaturestats.hpp"
#include "stat.hpp"

namespace MWMechanics
{
    Spells::Spells() {}

    Spells::Spells(const Spells& spells)
        : mSpellList(spells.mSpellList)
        , mSpells(spells.mSpells)
        , mSelectedSpell(spells.mSelectedSpell)
        , mUsedPowers(spells.mUsedPowers)
    {
        if (mSpellList)
            mSpellList->addListener(this);
    }

    Spells::Spells(Spells&& spells)
        : mSpellList(std::move(spells.mSpellList))
        , mSpells(std::move(spells.mSpells))
        , mSelectedSpell(std::move(spells.mSelectedSpell))
        , mUsedPowers(std::move(spells.mUsedPowers))
    {
        if (mSpellList)
            mSpellList->updateListener(&spells, this);
    }

    std::vector<const ESM::Spell*>::const_iterator Spells::begin() const
    {
        return mSpells.begin();
    }

    std::vector<const ESM::Spell*>::const_iterator Spells::end() const
    {
        return mSpells.end();
    }

    bool Spells::hasSpell(const ESM::RefId& spell) const
    {
        return hasSpell(SpellList::getSpell(spell));
    }

    bool Spells::hasSpell(const ESM::Spell* spell) const
    {
        return std::find(mSpells.begin(), mSpells.end(), spell) != mSpells.end();
    }

    void Spells::add(const ESM::Spell* spell, bool modifyBase)
    {
        if (modifyBase)
            mSpellList->add(spell);
        else
            addSpell(spell);
    }

    void Spells::add(const ESM::RefId& spellId, bool modifyBase)
    {
        add(SpellList::getSpell(spellId), modifyBase);
    }

    void Spells::addSpell(const ESM::Spell* spell)
    {
        if (!hasSpell(spell))
            mSpells.emplace_back(spell);
    }

    void Spells::remove(const ESM::RefId& spellId, bool modifyBase)
    {
        remove(SpellList::getSpell(spellId), modifyBase);
    }

    void Spells::remove(const ESM::Spell* spell, bool modifyBase)
    {
        removeSpell(spell);
        if (modifyBase)
            mSpellList->remove(spell);
        if (spell->mId == mSelectedSpell)
            mSelectedSpell = ESM::RefId();
    }

    void Spells::removeSpell(const ESM::Spell* spell)
    {
        const auto it = std::find(mSpells.begin(), mSpells.end(), spell);
        if (it != mSpells.end())
            mSpells.erase(it);
    }

    void Spells::removeAllSpells()
    {
        mSpells.clear();
    }

    void Spells::clear(bool modifyBase)
    {
        removeAllSpells();
        if (modifyBase)
            mSpellList->clear();
    }

    void Spells::setSelectedSpell(const ESM::RefId& spellId)
    {
        mSelectedSpell = spellId;
    }

    const ESM::RefId& Spells::getSelectedSpell() const
    {
        return mSelectedSpell;
    }

    bool Spells::hasSpellType(const ESM::Spell::SpellType type) const
    {
        auto it = std::find_if(std::begin(mSpells), std::end(mSpells),
            [=](const ESM::Spell* spell) { return spell->mData.mType == type; });
        return it != std::end(mSpells);
    }

    bool Spells::hasCommonDisease() const
    {
        return hasSpellType(ESM::Spell::ST_Disease);
    }

    bool Spells::hasBlightDisease() const
    {
        return hasSpellType(ESM::Spell::ST_Blight);
    }

    void Spells::purge(const SpellFilter& filter)
    {
        std::vector<ESM::RefId> purged;
        for (auto iter = mSpells.begin(); iter != mSpells.end();)
        {
            const ESM::Spell* spell = *iter;
            if (filter(spell))
            {
                iter = mSpells.erase(iter);
                purged.push_back(spell->mId);
            }
            else
                ++iter;
        }
        if (!purged.empty())
            mSpellList->removeAll(purged);
    }

    void Spells::purgeCommonDisease()
    {
        purge([](auto spell) { return spell->mData.mType == ESM::Spell::ST_Disease; });
    }

    void Spells::purgeBlightDisease()
    {
        purge([](auto spell) { return spell->mData.mType == ESM::Spell::ST_Blight && !hasCorprusEffect(spell); });
    }

    void Spells::purgeCorprusDisease()
    {
        purge(&hasCorprusEffect);
    }

    void Spells::purgeCurses()
    {
        purge([](auto spell) { return spell->mData.mType == ESM::Spell::ST_Curse; });
    }

    bool Spells::hasCorprusEffect(const ESM::Spell* spell)
    {
        for (const auto& effectIt : spell->mEffects.mList)
        {
            if (effectIt.mData.mEffectID == ESM::MagicEffect::Corprus)
            {
                return true;
            }
        }
        return false;
    }

    bool Spells::canUsePower(const ESM::Spell* spell) const
    {
        const auto it = std::find_if(
            std::begin(mUsedPowers), std::end(mUsedPowers), [&](auto& pair) { return pair.first == spell; });
        return it == mUsedPowers.end() || it->second + 24 <= MWBase::Environment::get().getWorld()->getTimeStamp();
    }

    void Spells::usePower(const ESM::Spell* spell)
    {
        // Updates or inserts a new entry with the current timestamp.
        const auto it = std::find_if(
            std::begin(mUsedPowers), std::end(mUsedPowers), [&](auto& pair) { return pair.first == spell; });
        const auto timestamp = MWBase::Environment::get().getWorld()->getTimeStamp();
        if (it == mUsedPowers.end())
            mUsedPowers.emplace_back(spell, timestamp);
        else
            it->second = timestamp;
    }

    Spells::PreparedInstance::PreparedInstance(const PreparedInstance& other)
        : mAdditions(other.mAdditions)
        , mMerged(other.mMerged)
        , mMissing(other.mMissing)
        , mConsumed(other.mConsumed)
    {
        mMerged.reserve(other.mMerged.capacity());
    }

    Spells::PreparedInstance& Spells::PreparedInstance::operator=(const PreparedInstance& other)
    {
        if (this != &other)
            *this = PreparedInstance(other);
        return *this;
    }

    Spells::PreparedInstance Spells::prepareInstance(const std::vector<ESM::RefId>& ids,
        const MWWorld::ESMStore& store, const MWWorld::ESMStore& incoming, std::size_t priorCapacity)
    {
        PreparedInstance result;
        result.mMerged.reserve(priorCapacity + ids.size());
        for (const auto& id : ids)
        {
            const auto* spell = store.searchForRestore<ESM::Spell>(id, incoming);
            if (!spell)
                result.mMissing.push_back(id);
            else if (std::find(result.mAdditions.begin(), result.mAdditions.end(), spell) == result.mAdditions.end())
                result.mAdditions.push_back(spell);
        }
        return result;
    }

    void Spells::PreparedInstance::warnMissing() const
    {
        for (const auto& id : mMissing)
            Log(Debug::Warning) << "Warning: ignoring nonexistent spell " << id;
    }

    void Spells::PreparedInstance::install(Spells& target)
    {
        if (mConsumed)
            throw std::logic_error("Prepared instance spells already consumed");
        // Validate the bound before changing the target; no allocation during merge.
        if (target.mSpells.size() + mAdditions.size() > mMerged.capacity())
            throw std::logic_error("Prepared instance spell capacity exceeded");
        mMerged.insert(mMerged.end(), target.mSpells.begin(), target.mSpells.end());
        for (const auto* spell : mAdditions)
            if (std::find(mMerged.begin(), mMerged.end(), spell) == mMerged.end())
                mMerged.push_back(spell);
        target.mSpells.swap(mMerged);
        mConsumed = true;
        warnMissing();
    }

    bool Spells::PreparedInstance::bind(Spells& target, const ESM::RefId& actorId)
    {
        if (mConsumed || !target.mSpells.empty() || target.mSpellList)
            throw std::logic_error("Prepared initial spell binding has unexpected target");
        auto [list, initialized] = MWBase::Environment::get().getESMStore()->getSpellList(actorId);
        target.mSpellList = std::move(list);
        target.mSpellList->addListener(&target);
        install(target); // Ordinary setSpells adds base spells even for cached lists.
        return initialized;
    }

    std::unique_ptr<Spells::PreparedState> Spells::prepareReadState(const ESM::SpellState& state,
        const MWWorld::ESMStore& store, const std::vector<ESM::RefId>& baseSpells,
        const MWWorld::ESMStore* incoming, const Spells* existing)
    {
        auto prepared = std::unique_ptr<PreparedState>(new PreparedState);
        const auto resolve = [&](const ESM::RefId& id) {
            return incoming ? store.searchForRestore<ESM::Spell>(id, *incoming) : store.get<ESM::Spell>().search(id);
        };
        const auto append = [](Collection& spells, const ESM::Spell* spell) {
            if (spell && std::find(spells.begin(), spells.end(), spell) == spells.end())
                spells.push_back(spell);
        };
        for (const auto& id : baseSpells)
            append(prepared->mExpectedBase, resolve(id));
        prepared->mBindingBase = prepared->mExpectedBase;
        prepared->mBaseFirst = existing ? existing->mSpells : prepared->mExpectedBase;
        prepared->mExisting = existing != nullptr;
        if (existing)
            prepared->mUsedPowers = existing->mUsedPowers;
        for (const auto& id : state.mSpells)
        {
            const auto* spell = resolve(id);
            append(prepared->mBaseFirst, spell);
            append(prepared->mSavedFirst, spell);
            if (spell && id == state.mSelectedSpell)
            {
                prepared->mHasSelection = true;
                prepared->mSelectedSpell = id;
            }
        }
        for (const auto* spell : prepared->mExpectedBase)
        {
            append(prepared->mBaseFirst, spell);
            append(prepared->mSavedFirst, spell);
        }
        for (const auto& [id, timestamp] : state.mUsedPowers)
            if (const auto* spell = resolve(id))
                prepared->mUsedPowers.emplace_back(spell, MWWorld::TimeStamp(timestamp));
        for (const auto& [id, effects] : state.mPermanentSpellEffects)
        {
            if (!resolve(id)) continue;
            prepared->mHasLegacyEffects = true;
            for (const auto& info : effects)
                prepared->mLegacyEffects.push_back({info.mId, info.mArg, info.mMagnitude});
        }
        return prepared;
    }

    bool Spells::PreparedState::attach(Spells& target, const ESM::RefId& actorId)
    {
        if (mConsumed || mAttached || mExisting)
            throw std::logic_error("Spell restore attachment already consumed or not a class plan");
        auto [list, initialized] = MWBase::Environment::get().getESMStore()->getSpellList(actorId);
        if (!initialized && !target.mSpells.empty() && target.mSpells != mExpectedBase)
            throw std::logic_error("First spell-list binding has unexpected instance spells");
        if (target.mSpellList && target.mSpellList != list)
            target.mSpellList->removeListener(&target);
        target.mSpellList = std::move(list);
        target.mSpellList->addListener(&target);
        // Cached class readers clear immediately. First readers can take the
        // prepared base vector without querying/copying the base record again.
        if (!initialized)
            target.mSpells.swap(mBindingBase);
        mAttached = true;
        return initialized;
    }

    void Spells::PreparedState::install(Spells& target, CreatureStats* creatureStats)
    {
        if (mConsumed)
            throw std::logic_error("Spell restore plan already consumed");
        if (!mExisting && ((!target.mSpells.empty() && target.mSpells != mExpectedBase) || !target.mUsedPowers.empty()))
            throw std::logic_error("Spell restore target differs from prepared base state");
        const bool baseFirst = mExisting || !target.mSpells.empty();
        target.mSpells.swap(baseFirst ? mBaseFirst : mSavedFirst);
        target.mUsedPowers.swap(mUsedPowers);
        if (mHasSelection)
            target.mSelectedSpell = mSelectedSpell;
        mConsumed = true;
        if (!mHasLegacyEffects)
            return;
        const MWWorld::Ptr player = getPlayer();
        if (creatureStats != &player.getClass().getCreatureStats(player))
            return;
        // Preserve the old Player-only corprus conversion and its ordering.
        for (const auto& info : mLegacyEffects)
        {
            if (info.mId == ESM::MagicEffect::refIdToIndex(ESM::MagicEffect::FortifyAttribute))
            {
                auto id = ESM::Attribute::indexToRefId(info.mArg);
                AttributeValue attr = creatureStats->getAttribute(id);
                attr.setModifier(attr.getModifier() - info.mMagnitude);
                attr.damage(-info.mMagnitude);
                creatureStats->setAttribute(id, attr);
            }
            else if (info.mId == ESM::MagicEffect::refIdToIndex(ESM::MagicEffect::DrainAttribute))
            {
                auto id = ESM::Attribute::indexToRefId(info.mArg);
                AttributeValue attr = creatureStats->getAttribute(id);
                attr.setModifier(attr.getModifier() + info.mMagnitude);
                attr.damage(info.mMagnitude);
                creatureStats->setAttribute(id, attr);
            }
        }
    }

    void Spells::readState(const ESM::SpellState& state, CreatureStats* creatureStats)
    {
        prepareReadState(state, *MWBase::Environment::get().getESMStore(), mSpellList->getSpells(), nullptr, this)
            ->install(*this, creatureStats);
    }

    void Spells::writeState(ESM::SpellState& state) const
    {
        const auto& baseSpells = mSpellList->getSpells();
        for (const auto spell : mSpells)
        {
            // Don't save spells and powers stored in the base record
            if ((spell->mData.mType != ESM::Spell::ST_Spell && spell->mData.mType != ESM::Spell::ST_Power)
                || std::find(baseSpells.begin(), baseSpells.end(), spell->mId) == baseSpells.end())
            {
                state.mSpells.emplace_back(spell->mId);
            }
        }

        state.mSelectedSpell = mSelectedSpell;

        for (const auto& it : mUsedPowers)
            state.mUsedPowers[it.first->mId] = it.second.toEsm();
    }

    bool Spells::setSpells(const ESM::RefId& actorId)
    {
        bool result;
        std::tie(mSpellList, result) = MWBase::Environment::get().getESMStore()->getSpellList(actorId);
        mSpellList->addListener(this);
        addAllToInstance(mSpellList->getSpells());
        return result;
    }

    void Spells::addAllToInstance(const std::vector<ESM::RefId>& spells)
    {
        for (const ESM::RefId& id : spells)
        {
            const ESM::Spell* spell = MWBase::Environment::get().getESMStore()->get<ESM::Spell>().search(id);
            if (spell)
                addSpell(spell);
            else
                Log(Debug::Warning) << "Warning: ignoring nonexistent spell " << id;
        }
    }

    Spells::~Spells()
    {
        if (mSpellList)
            mSpellList->removeListener(this);
    }
}
