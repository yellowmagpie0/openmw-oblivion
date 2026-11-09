#include "oblivionlevelled.hpp"
#include "esmstore.hpp"
#include "oblivionprofileservices.hpp"
#include <components/esm4/levelledlist.hpp>
#include <components/esm4/loadlvlc.hpp>
#include <components/esm4/loadlvli.hpp>
#include <components/esm4/loadrefr.hpp>
#include <components/esm4/loadnpc.hpp>
#include <components/esm4/loadcrea.hpp>
#include <components/esm4/loadgmst.hpp>
#include <components/esm4/runtimestate.hpp>
#include <components/misc/strings/algorithm.hpp>
#include <functional>
#include <limits>
#include <map>
#include <set>

namespace MWWorld
{
    namespace
    {
        std::uint16_t checkedLevel(int level)
        {
            if (level < 1 || level > 32767) throw std::invalid_argument("native leveled selection level out of range");
            return level;
        }
        std::uint16_t difference(const ESMStore& store, bool actors)
        {
            const char* name = actors ? "iLevCreaLevelDifferenceMax" : "iLevItemLevelDifferenceMax";
            for (const auto& row : store.get<ESM4::GameSetting>())
                if (Misc::StringUtils::ciEqual(row.mEditorId, name))
                {
                    const auto* winning = store.get<ESM4::GameSetting>().search(row.mId);
                    const auto* value = winning ? std::get_if<std::int32_t>(&winning->mData) : nullptr;
                    if (!value || *value < 0 || *value > 32767)
                        throw std::invalid_argument("invalid native leveled maximum difference setting");
                    return *value;
                }
            return 8; // Original compiled defaults; installed winning GMST takes precedence.
        }
        template<class List>
        std::optional<std::size_t> draw(const List& list, int level, int maxDifference,
            bool container, Misc::Rng::Generator& random)
        {
            const int chance = list.chanceNone();
            if (chance < 0 || chance > 100) throw std::invalid_argument("invalid native leveled chance-none");
            if (chance && Misc::Rng::roll0to99(random) < chance) return {};
            const auto flags = list.calcAllLvlLessThanPlayer() ? 1 : 0;
            const auto pool = ESM4::levelledEntryPool(list.mLvlObject, checkedLevel(level), flags, maxDifference, container);
            if (pool.empty()) return {};
            return pool[Misc::Rng::rollDice(pool.size(), random)];
        }
        int multiply(int first, int second)
        {
            if (first < 0 || second < 0 || (second && first > std::numeric_limits<int>::max() / second))
                throw std::overflow_error("native leveled inventory count overflow");
            return first * second;
        }
        struct Recursion
        {
            std::set<ESM::FormId>& mPath;
            ESM::FormId mId;
            Recursion(std::set<ESM::FormId>& path, ESM::FormId id) : mPath(path), mId(id)
            {
                if (path.size() >= 32 || !path.insert(id).second)
                    throw std::invalid_argument("cyclic or excessive native leveled-list nesting");
            }
            ~Recursion() { mPath.erase(mId); }
        };
    }

    std::vector<ESM4::InventoryItem> expandOblivionLevelledItems(const ESMStore& store,
        const std::vector<ESM4::InventoryItem>& items, int level, Misc::Rng::Generator& random)
    {
        checkedLevel(level);
        const auto maxDifference = difference(store, false);
        std::set<ESM::FormId> path;
        using Counts = std::map<ESM::FormId, int>;
        std::function<Counts(ESM::FormId, int)> expand = [&](ESM::FormId id, int count) -> Counts {
            if (count == 0) return {};
            const auto* list = store.get<ESM4::LevelledItem>().search(id);
            if (!list)
            {
                if (!OblivionProfileServices::itemDefinition(store, ESM::RefId(id)))
                    throw std::invalid_argument("native leveled inventory leaf is not a supported item");
                return {{id, count}};
            }
            Recursion guard(path, id);
            Counts result;
            const bool each = list->calcEachItemInCount();
            // Without EachInCount, expand the selected subtree once and multiply
            // the result. Multiplying its input would incorrectly repeat child rolls.
            const int rolls = each ? count : 1;
            if (rolls > 100000) throw std::invalid_argument("excessive native leveled draw count");
            for (int i = 0; i < rolls; ++i)
                if (const auto selected = draw(*list, level, maxDifference, true, random))
                {
                    const auto& entry = list->mLvlObject[*selected];
                    for (const auto& [item, quantity] : expand(ESM::FormId::fromUint32(entry.item), entry.count))
                    {
                        const int added = multiply(quantity, each ? 1 : count);
                        if (result[item] > std::numeric_limits<int>::max() - added)
                            throw std::overflow_error("native leveled inventory sum overflow");
                        result[item] += added;
                    }
                }
            return result;
        };
        std::vector<ESM4::InventoryItem> result;
        for (const auto& item : items)
        {
            const auto id = ESM::FormId::fromUint32(item.item);
            // Preserve replenishing-stock intent on direct CNTOs. Generated
            // list contents are concrete quantities, persisted by inventory authority.
            if (!store.get<ESM4::LevelledItem>().search(id)) { result.push_back(item); continue; }
            for (const auto& [base, count] : expand(id, ESM4::inventoryItemCount(item)))
                result.push_back({base.toUint32(), count});
        }
        return result;
    }

    bool supportsOblivionLevelledActor(const ESMStore& store, const ESM::FormKey& key, int level)
    {
        checkedLevel(level);
        const auto* root = store.get<ESM4::LevelledCreature>().search(key);
        if (!root) return false;
        const auto maxDifference = difference(store, true);
        std::set<ESM::FormId> path;
        std::function<bool(const ESM4::LevelledCreature&)> supported = [&](const ESM4::LevelledCreature& list) {
            Recursion guard(path, list.mId);
            if (!list.mScriptId.isZeroOrUnset() || !list.mTemplate.isZeroOrUnset()) return false;
            if (list.chanceNone() == 100) return true;
            const auto pool = ESM4::levelledEntryPool(list.mLvlObject, checkedLevel(level),
                list.calcAllLvlLessThanPlayer() ? 1 : 0, maxDifference, true);
            for (const auto i : pool)
            {
                const auto& entry = list.mLvlObject[i];
                // Original004AFD80 consumes a container result and creates its
                // quantity of actors. Multiple child identities need a separate
                // saved spawn-group owner; do not partially roll such a list.
                if (entry.count != 1) return false;
                const auto id = ESM::FormId::fromUint32(entry.item);
                if (const auto* nested = store.get<ESM4::LevelledCreature>().search(id))
                { if (!supported(*nested)) return false; }
                else if (!store.get<ESM4::Npc>().search(id) && !store.get<ESM4::Creature>().search(id)) return false;
            }
            return true;
        };
        return supported(*root);
    }

    std::optional<ESM::FormId> selectOblivionLevelledActor(const ESMStore& store,
        const ESM::FormKey& key, int level, Misc::Rng::Generator& random)
    {
        checkedLevel(level);
        const auto maxDifference = difference(store, true);
        std::set<ESM::FormId> path;
        std::function<std::optional<ESM::FormId>(const ESM4::LevelledCreature&)> select =
            [&](const ESM4::LevelledCreature& list) -> std::optional<ESM::FormId> {
                Recursion guard(path, list.mId);
                if (const auto selected = draw(list, level, maxDifference, true, random))
                {
                    const auto& entry = list.mLvlObject[*selected];
                    if (entry.count != 1) throw std::invalid_argument("native actor placement requires one actor per entry");
                    const auto id = ESM::FormId::fromUint32(entry.item);
                    if (const auto* nested = store.get<ESM4::LevelledCreature>().search(id)) return select(*nested);
                    if (!store.get<ESM4::Npc>().search(id) && !store.get<ESM4::Creature>().search(id))
                        throw std::invalid_argument("native leveled actor leaf has no actor definition");
                    return id;
                }
                return {};
            };
        const auto* list = store.get<ESM4::LevelledCreature>().search(key);
        if (!list) throw std::invalid_argument("missing native leveled actor list");
        return select(*list);
    }

    bool oblivionLevelledActorContains(const ESMStore& store, const ESM::FormKey& reference,
        const ESM::FormKey& base, std::optional<int> level)
    {
        const auto* placed = store.get<ESM4::Reference>().search(reference);
        if (!placed) return false;
        const auto* root = store.get<ESM4::LevelledCreature>().search(placed->mBaseObj);
        if (!root) return false;
        if (!store.get<ESM4::Npc>().search(base) && !store.get<ESM4::Creature>().search(base)) return false;
        const auto maxDifference = level ? difference(store, true) : 0;
        std::set<ESM::FormId> path;
        std::function<bool(const ESM4::LevelledCreature&)> contains = [&](const ESM4::LevelledCreature& list) {
            Recursion guard(path, list.mId);
            if (list.chanceNone() == 100) return false;
            std::vector<std::size_t> pool;
            if (level) pool = ESM4::levelledEntryPool(list.mLvlObject, checkedLevel(*level),
                list.calcAllLvlLessThanPlayer() ? 1 : 0, maxDifference, true);
            else for (std::size_t i = 0; i < list.mLvlObject.size(); ++i) pool.push_back(i);
            for (const auto i : pool)
            {
                const auto& entry = list.mLvlObject[i];
                if (entry.count != 1) continue;
                const auto id = ESM::FormId::fromUint32(entry.item);
                if (const auto* nested = store.get<ESM4::LevelledCreature>().search(id))
                { if (contains(*nested)) return true; }
                else if (const auto* npc = store.get<ESM4::Npc>().search(id); npc && npc->mFormKey == base) return true;
                else if (const auto* creature = store.get<ESM4::Creature>().search(id); creature && creature->mFormKey == base) return true;
            }
            return false;
        };
        return contains(*root);
    }

    bool validateOblivionLevelledActor(const ESMStore& store, const ESM4::RuntimeReferenceState& reference)
    {
        if (reference.mCustomState.contains("native.levelled_actor.inventory_pending"))
            throw std::invalid_argument("native leveled actor inventory has not finished construction");
        const auto marker = reference.mCustomState.find("native.levelled_actor.level");
        const auto empty = reference.mCustomState.find("native.levelled_actor.empty");
        if (marker == reference.mCustomState.end() && empty == reference.mCustomState.end()) return false;
        const auto* level = marker == reference.mCustomState.end() ? nullptr : std::get_if<std::int64_t>(&marker->second);
        const auto* isEmpty = empty == reference.mCustomState.end() ? nullptr : std::get_if<bool>(&empty->second);
        const auto* placed = store.get<ESM4::Reference>().search(reference.mKey);
        const auto* list = placed ? store.get<ESM4::LevelledCreature>().search(placed->mBaseObj) : nullptr;
        if (!level || !isEmpty || *level < 1 || *level > 32767 || !list)
            throw std::invalid_argument("invalid saved native leveled actor selection");
        if (*isEmpty)
        {
            // Empty draws keep the original list base and have no actor authority.
            if (reference.mBase != placed->mBaseKey || !reference.mInventory.empty() || reference.mActorDrawState)
                throw std::invalid_argument("saved empty leveled actor has live actor state");
            std::set<ESM::FormId> path;
            const auto maxDifference = difference(store, true);
            std::function<bool(const ESM4::LevelledCreature&)> canBeEmpty = [&](const ESM4::LevelledCreature& current) {
                Recursion guard(path, current.mId);
                if (current.chanceNone() > 0) return true;
                const auto pool = ESM4::levelledEntryPool(current.mLvlObject, checkedLevel(*level),
                    current.calcAllLvlLessThanPlayer() ? 1 : 0, maxDifference, true);
                if (pool.empty()) return true;
                for (const auto index : pool)
                    if (const auto* nested = store.get<ESM4::LevelledCreature>().search(
                            ESM::FormId::fromUint32(current.mLvlObject[index].item)); nested && canBeEmpty(*nested)) return true;
                return false;
            };
            if (!canBeEmpty(*list))
                throw std::invalid_argument("saved empty leveled draw is impossible for its original list");
        }
        else if (!oblivionLevelledActorContains(store, reference.mKey, reference.mBase, *level))
            throw std::invalid_argument("saved leveled actor is not eligible in its winning original list");
        return true;
    }
}
