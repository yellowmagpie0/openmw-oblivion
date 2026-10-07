#include "saveadmission.hpp"

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <sstream>

#include <components/esm3/esmreader.hpp>
#include <components/esm3/loadglob.hpp>
#include <components/esm3/loadnpc.hpp>
#include <components/esm3/loadclas.hpp>
#include <components/esm3/cellstate.hpp>
#include <components/esm3/fogstate.hpp>
#include <components/esm3/player.hpp>
#include <components/esm3/containerstate.hpp>
#include <components/esm3/creaturestate.hpp>
#include <components/esm3/controlsstate.hpp>
#include <components/esm3/custommarkerstate.hpp>
#include <components/esm3/dialoguestate.hpp>
#include <components/esm3/globalmap.hpp>
#include <components/esm3/globalscript.hpp>
#include <components/esm3/journalentry.hpp>
#include <components/esm3/projectilestate.hpp>
#include <components/esm3/queststate.hpp>
#include <components/esm3/quickkeys.hpp>
#include <components/esm3/stolenitems.hpp>
#include <components/esm3/weatherstate.hpp>
#include <components/lua/configuration.hpp>
#include <components/esm4/loadglob.hpp>
#include <components/esm4/runtimestate.hpp>
#include <components/esm4/actorvalues.hpp>
#include <components/esm4/crimerules.hpp>
#include <components/misc/strings/algorithm.hpp>
#include <components/misc/rng.hpp>

#include "../mwworld/esmstore.hpp"
#include "../mwworld/savedreference.hpp"
#include "../mwworld/manualref.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/oblivioninventoryidentity.hpp"
#include "../mwlua/userdataserializer.hpp"

namespace
{
    template <class T>
    T readSharedState(ESM::ESMReader& reader)
    {
        T state{};
        state.load(reader);
        return state;
    }

    bool validateAuxiliaryRecord(ESM::ESMReader& reader, std::uint32_t type,
        std::set<std::uint32_t>& singletons)
    {
        const auto singleton = [&] {
            if (!singletons.insert(type).second)
                throw std::runtime_error("Saved game contains duplicate auxiliary singleton records");
        };
        const auto finite = [](float value) {
            if (!std::isfinite(value))
                throw std::runtime_error("Saved game auxiliary record has a nonfinite value");
        };
        const auto projectile = [&](const ESM::BaseProjectileState& state) {
            for (const float value : state.mPosition.mValues) finite(value);
            for (const float value : state.mOrientation.mValues) finite(value);
        };
        switch (type)
        {
            case ESM::REC_INPU:
                singleton();
                readSharedState<ESM::ControlsState>(reader);
                break;
            case ESM::REC_CAM_:
            {
                singleton();
                bool firstPerson;
                reader.getHNT(firstPerson, "FIRS");
                break;
            }
            case ESM::REC_ENAB:
            {
                singleton();
                bool teleport, levitation;
                reader.getHNT(teleport, "TELE");
                reader.getHNT(levitation, "LEVT");
                break;
            }
            case ESM::REC_RAND:
            {
                singleton();
                std::istringstream stream(reader.getHNOString("RAND"));
                Misc::Rng::Generator random;
                if (!(stream >> random) || !(stream >> std::ws).eof())
                    throw std::runtime_error("Saved game shared random state is invalid");
                break;
            }
            case ESM::REC_DIAS:
                singleton();
                readSharedState<ESM::DialogueState>(reader);
                break;
            case ESM::REC_JOUR:
            {
                const auto state = readSharedState<ESM::JournalEntry>(reader);
                if (state.mType < ESM::JournalEntry::Type_Journal || state.mType > ESM::JournalEntry::Type_Quest)
                    throw std::runtime_error("Saved game journal entry has an invalid type");
                break;
            }
            case ESM::REC_QUES:
                readSharedState<ESM::QuestState>(reader);
                break;
            case ESM::REC_GSCR:
                readSharedState<ESM::GlobalScript>(reader);
                break;
            case ESM::REC_KEYS:
            {
                singleton();
                const auto state = readSharedState<ESM::QuickKeys>(reader);
                if (state.mKeys.size() > 10)
                    throw std::runtime_error("Saved game contains too many quickkeys");
                for (const auto& key : state.mKeys)
                    if (key.mType > ESM::QuickKeys::Type::HandToHand)
                        throw std::runtime_error("Saved game quickkey has an invalid type");
                break;
            }
            case ESM::REC_ASPL:
                singleton();
                reader.getHNRefId("ID__");
                break;
            case ESM::REC_MARK:
            {
                const auto state = readSharedState<ESM::CustomMarker>(reader);
                finite(state.mWorldX);
                finite(state.mWorldY);
                break;
            }
            case ESM::REC_GMAP:
            {
                singleton();
                const auto state = readSharedState<ESM::GlobalMap>(reader);
                // The renderer subtracts these in signed int arithmetic. Keep
                // its empty/inverted-map convention, but reject overflow.
                for (const auto& [low, high] : {std::pair{state.mBounds.mMinX, state.mBounds.mMaxX},
                         std::pair{state.mBounds.mMinY, state.mBounds.mMaxY}})
                {
                    const auto difference = static_cast<std::int64_t>(high) - low;
                    if (difference < std::numeric_limits<int>::min()
                        || difference >= std::numeric_limits<int>::max())
                        throw std::runtime_error("Saved game global map bounds exceed the renderer domain");
                }
                break;
            }
            case ESM::REC_STLN:
            {
                singleton();
                const auto state = readSharedState<ESM::StolenItems>(reader);
                for (const auto& [id, owners] : state.mStolenItems)
                    for (const auto& [owner, count] : owners)
                        if (count < 0)
                            throw std::runtime_error("Saved game stolen item count is negative");
                break;
            }
            case ESM::REC_DCOU:
            {
                singleton();
                std::set<ESM::RefId> actors;
                while (reader.isNextSub("ID__"))
                {
                    const auto id = reader.getRefId();
                    int count;
                    reader.getHNT(count, "COUN");
                    if (count < 0 || !actors.insert(id).second)
                        throw std::runtime_error("Saved game death count is negative or duplicated");
                }
                break;
            }
            case ESM::REC_WTHR:
            {
                singleton();
                const auto state = readSharedState<ESM::WeatherState>(reader);
                finite(state.mTimePassed);
                finite(state.mWeatherUpdateTime);
                finite(state.mTransitionFactor);
                break;
            }
            case ESM::REC_PROJ:
            {
                const auto state = readSharedState<ESM::ProjectileState>(reader);
                projectile(state);
                for (const float value : state.mVelocity.mValues) finite(value);
                finite(state.mAttackStrength);
                finite(state.mAttackWindUp);
                break;
            }
            case ESM::REC_MPRJ:
            {
                const auto state = readSharedState<ESM::MagicBoltState>(reader);
                projectile(state);
                finite(state.mSpeed);
                break;
            }
            default:
                return false;
        }
        return true;
    }

    struct LegacyGeneratedItem
    {
        ESM::CellRef mReference;
        std::optional<int> mSlot;
    };

    std::vector<LegacyGeneratedItem> generatedItems(const ESM::InventoryState& inventory)
    {
        std::vector<LegacyGeneratedItem> result;
        for (std::size_t i = 0; i != inventory.mItems.size(); ++i)
        {
            const auto& item = inventory.mItems[i];
            if (!item.mRef.mRefID.getIf<ESM::GeneratedRefId>())
                continue;
            const auto slot = inventory.mEquipmentSlots.find(static_cast<int>(i));
            result.push_back({item.mRef, slot == inventory.mEquipmentSlots.end()
                ? std::nullopt : std::optional<int>(slot->second)});
        }
        return result;
    }

    ESM::FormKey sharedOwnerKey(const ESM::RefId& id, ESM::ESMReader& reader,
        const ESM::SavedGame& profile)
    {
        if (id.empty()) return {};
        const auto* form = id.getIf<ESM::FormId>();
        if (!form)
            throw std::runtime_error("Legacy generated inventory owner is not a native FormId");
        if (!form->hasContentFile())
            return ESM::FormKeyResolver(profile.mContentFiles).toFormKey(*form);
        // getRefId remaps owners to the current order. Recover their canonical
        // plugin identity using the reader's saved-to-current mapping.
        for (std::size_t i = 0; i != profile.mContentFiles.size(); ++i)
        {
            ESM::FormId mapped{1, static_cast<std::int32_t>(i)};
            if (reader.applyContentFileMapping(mapped) && mapped.mContentFile == form->mContentFile)
                return ESM::FormKey::content(profile.mContentFiles[i], form->mIndex);
        }
        throw std::runtime_error("Legacy generated inventory owner has no saved content identity");
    }

    void recoverGeneratedItems(std::vector<ESM4::RuntimeInventoryItem>& target,
        const std::vector<LegacyGeneratedItem>& saved, const MWWorld::ESMStore& definitions,
        ESM::ESMReader& reader, const ESM::SavedGame& profile, std::uint32_t version)
    {
        std::set<ESM::FormKey> nativeBases;
        for (const auto& item : target) nativeBases.insert(item.mBase);
        for (const auto& item : saved)
        {
            const auto key = *MWWorld::OblivionInventory::sharedKey(item.mReference.mRefID);
            // A native base describes all its stacks. Never duplicate, replace
            // or conceal a bad explicit native entry using its shared copy.
            if (nativeBases.contains(key)) continue;
            const auto count = std::abs(static_cast<std::int64_t>(item.mReference.mCount));
            if (count == 0 || count > std::numeric_limits<std::int32_t>::max())
                throw std::runtime_error("Legacy generated inventory quantity cannot be represented");
            MWWorld::ManualRef source(definitions, item.mReference.mRefID, static_cast<int>(count));
            const auto ptr = source.getPtr();
            MWWorld::ContainerStore::getType(ptr);
            ptr.getCellRef() = MWWorld::CellRef(item.mReference);
            ESM4::RuntimeInventoryItem recovered;
            recovered.mBase = key;
            recovered.mCount = static_cast<std::int32_t>(count);
            if (version >= 4)
            {
                const auto& itemClass = ptr.getClass();
                recovered.mCondition = itemClass.hasItemHealth(ptr)
                    ? ptr.getCellRef().getItemCondition(static_cast<float>(itemClass.getItemMaxHealth(ptr))) : -1.f;
                recovered.mCharge = ptr.getCellRef().getEnchantmentCharge();
                recovered.mRemainingUsageTime = itemClass.getRemainingUsageTime(ptr);
                recovered.mOwner = sharedOwnerKey(item.mReference.mOwner, reader, profile);
                if (version >= 41)
                {
                    recovered.mOwnershipRank = item.mReference.mNativeOwnershipRank;
                    recovered.mOwnershipGlobal = sharedOwnerKey(item.mReference.mNativeOwnershipGlobal, reader, profile);
                }
                if (item.mSlot)
                {
                    const auto equipment = itemClass.getEquipmentSlots(ptr);
                    if (std::find(equipment.first.begin(), equipment.first.end(), *item.mSlot) == equipment.first.end()
                        || (count != 1 && !equipment.second))
                        throw std::runtime_error("Legacy generated inventory equipment disagrees with its definition");
                    recovered.mEquippedSlots = MWWorld::OblivionInventory::slotMask(
                        *item.mSlot, ptr.getType() == ESM::REC_LIGH);
                }
            }
            ESM4::addInventoryItem(target, std::move(recovered));
        }
    }
}

namespace MWState
{
    ESM::SavedGame admitSave(ESM::ESMReader& reader, ESM::GameProfile activeProfile,
        const std::function<void(const ESM4::RuntimeState&)>& validateNative,
        const MWWorld::ESMStore* content,
        const std::function<void(const ESM4::RuntimeState&, std::unique_ptr<MWWorld::ESMStore>)>& prepareNative)
    {
        const auto start = reader.getContext();
        try
        {
            std::optional<ESM::ESM_Context> profileRecord;
            std::optional<ESM::ESM_Context> nativeRecord;
            while (reader.hasMoreRecs())
            {
                if (reader.getContext().leftFile < 16)
                    throw std::runtime_error("Saved game contains a truncated record header");
                const auto type = reader.getRecName();
                reader.getRecHeader();
                const auto record = reader.getContext();
                if (type == ESM::REC_SAVE)
                {
                    if (profileRecord)
                        throw std::runtime_error("Saved game contains duplicate SAVE records");
                    profileRecord = record;
                }
                else if (type == ESM::REC_T4ST)
                {
                    if (nativeRecord)
                        throw std::runtime_error("Saved game contains duplicate T4ST records");
                    nativeRecord = record;
                }
                // ESMReader supports legacy readers crossing subrecord bounds.
                // Admission must instead reject every incomplete or oversized
                // subrecord, including records later ignored by restoration.
                while (reader.hasMoreSubs())
                {
                    if (reader.getContext().leftRec < 8)
                        throw std::runtime_error("Saved game contains a truncated subrecord header");
                    reader.getSubName();
                    reader.getSubHeader();
                    if (reader.getContext().leftRec < 0)
                        throw std::runtime_error("Saved game subrecord exceeds its record bounds");
                    if (type == ESM::REC_CSTA && reader.retSubName() == ESM::NAME("FTEX")
                        && reader.getSubSize() < sizeof(std::int32_t) * 2)
                        throw std::runtime_error("Saved game fog texture is missing its coordinates");
                    reader.skip(reader.getSubSize());
                }
            }
            if (!profileRecord)
                throw std::runtime_error("Saved game has no SAVE profile record");
            reader.restoreContext(*profileRecord);
            ESM::SavedGame profile;
            profile.load(reader);
            if (reader.hasMoreSubs())
                throw std::runtime_error("Saved game profile contains unexpected trailing data");
            if (profile.mGameProfile != activeProfile)
                throw std::runtime_error("Saved game profile '" + std::string(ESM::toString(profile.mGameProfile))
                    + "' cannot be loaded by active profile '" + std::string(ESM::toString(activeProfile)) + "'");
            if (profile.mGameProfile == ESM::GameProfile::Oblivion
                && profile.mRuntimeStateVersion > ESM4::CurrentRuntimeStateVersion)
                throw std::runtime_error("Saved game declares an unsupported TES4 runtime-state version");
            std::unique_ptr<MWWorld::ESMStore> shared;
            std::map<ESM::RefId, ESM::Global> globals;
            std::vector<std::pair<std::uint32_t, ESM::ESM_Context>> worldRecords;
            ESM4::LocalLuaScripts nativeScripts;
            std::vector<LegacyGeneratedItem> legacyPlayerItems;
            struct ActorInventory { ESM::FormKey mBase; std::vector<LegacyGeneratedItem> mItems; };
            std::map<ESM::FormKey, ActorInventory> legacyActorItems;
            struct SharedBounty { ESM::FormKey mBase; int mAmount; };
            std::map<ESM::FormKey, SharedBounty> sharedBounties;
            std::optional<int> sharedPlayerFame;
            if (activeProfile == ESM::GameProfile::Oblivion)
            {
                // Framing alone does not prove that shared dynamic records can
                // be decoded. Use the production store reader on a detached
                // store before any outgoing world is cleared. Do not set up
                // or publish this store: content-dependent reconciliation is
                // a separate preparation step.
                shared = std::make_unique<MWWorld::ESMStore>();
                std::set<std::uint32_t> auxiliarySingletons;
                reader.restoreContext(start);
                while (reader.hasMoreRecs())
                {
                    const auto type = reader.getRecName();
                    reader.getRecHeader();
                    if (type == ESM::REC_PLAY || type == ESM::REC_CSTA || type == ESM::REC_LUAM)
                        worldRecords.emplace_back(type.toInt(), reader.getContext());
                    bool decoded = false;
                    if (type == ESM::REC_GLOB)
                    {
                        ESM::Global global;
                        bool deleted = false;
                        global.load(reader, deleted);
                        if (deleted)
                            throw std::runtime_error("Saved game contains a deleted shared global");
                        globals.insert_or_assign(global.mId, std::move(global));
                        decoded = true;
                    }
                    else if (type == ESM::REC_NPC_)
                    {
                        ESM::NPC npc{};
                        bool deleted = false;
                        npc.load(reader, deleted);
                        if (npc.mId == "Player" && deleted)
                            throw std::runtime_error("Saved game deletes its shared Player record");
                        shared->getWritable<ESM::NPC>().insert(npc);
                        decoded = true;
                    }
                    else if (type == ESM::REC_CLAS)
                    {
                        ESM::Class characterClass{};
                        bool deleted = false;
                        characterClass.load(reader, deleted);
                        if (deleted)
                            throw std::runtime_error("Saved game contains a deleted shared class");
                        shared->getWritable<ESM::Class>().insert(characterClass);
                        decoded = true;
                    }
                    else if (validateAuxiliaryRecord(reader, type.toInt(), auxiliarySingletons))
                        decoded = true;
                    else
                        decoded = shared->readRecord(reader, type.toInt(), false);
                    if (!decoded)
                        reader.skipRecord();
                    else if (reader.hasMoreSubs())
                        throw std::runtime_error("Saved game shared record contains unexpected trailing data");
                }
                shared->rebuildIdsIndex();
                std::set<ESM::RefId> cells;
                bool hasPlayer = false;
                bool hasLua = false;
                LuaUtil::ScriptsConfiguration luaConfiguration;
                const auto scripts = content ? content->getLuaScriptsCfg() : ESM::LuaScriptsCfg{};
                luaConfiguration.init(scripts, false);
                struct RestoreConfiguration
                {
                    ESM::ESMReader& mReader;
                    const LuaUtil::ScriptsConfiguration* mPrevious;
                    ~RestoreConfiguration() { mReader.mScriptsConfiguration = mPrevious; }
                } restore{reader, reader.mScriptsConfiguration};
                // Local states may precede LUAM in the file. Resolve their IDs
                // against the saved configuration, never the outgoing game.
                for (const auto& [type, context] : worldRecords)
                {
                    if (type != ESM::REC_LUAM)
                        continue;
                    if (hasLua)
                        throw std::runtime_error("Saved game contains duplicate LUAM records");
                    hasLua = true;
                    reader.restoreContext(context);
                    nativeScripts = MWLua::validateSavedLuaRecord(reader, scripts, &luaConfiguration);
                    if (content)
                        ESM4::validateLocalLuaScriptContent(nativeScripts, content->getFormKeyIndex());
                }
                reader.mScriptsConfiguration = &luaConfiguration;
                std::vector<ESM::LuaScripts> localScripts;
                const auto collectScripts = [&](ESM::ObjectState& object) {
                    if (!object.mLuaScripts.mScripts.empty())
                        localScripts.push_back(std::move(object.mLuaScripts));
                    ESM::InventoryState* inventory = nullptr;
                    if (auto* npc = dynamic_cast<ESM::NpcState*>(&object)) inventory = &npc->mInventory;
                    else if (auto* creature = dynamic_cast<ESM::CreatureState*>(&object)) inventory = &creature->mInventory;
                    else if (auto* container = dynamic_cast<ESM::ContainerState*>(&object)) inventory = &container->mInventory;
                    if (inventory)
                        for (auto& item : inventory->mItems)
                            if (!item.mLuaScripts.mScripts.empty())
                                localScripts.push_back(std::move(item.mLuaScripts));
                };
                const auto validatePosition = [](const ESM::Position& position) {
                    for (int axis = 0; axis != 3; ++axis)
                        if (!std::isfinite(position.pos[axis]) || !std::isfinite(position.rot[axis]))
                            throw std::runtime_error("Saved game shared reference has a nonfinite position");
                };
                for (const auto& [type, context] : worldRecords)
                {
                    if (type == ESM::REC_LUAM)
                        continue;
                    reader.restoreContext(context);
                    if (type == ESM::REC_PLAY)
                    {
                        if (hasPlayer)
                            throw std::runtime_error("Saved game contains duplicate PLAY records");
                        hasPlayer = true;
                        ESM::Player player{};
                        player.load(reader);
                        validatePosition(player.mObject.mPosition);
                        if (player.mObject.mHasCustomState)
                        {
                            sharedBounties.emplace(ESM::FormKey::dynamic("player", 1),
                                SharedBounty{{}, player.mObject.mNpcStats.mBounty});
                            sharedPlayerFame = player.mObject.mNpcStats.mReputation;
                        }
                        legacyPlayerItems = generatedItems(player.mObject.mInventory);
                        collectScripts(player.mObject);
                    }
                    else
                    {
                        ESM::CellState cell{};
                        cell.mId = reader.getCellId();
                        if (!cells.insert(cell.mId).second)
                            throw std::runtime_error("Saved game contains duplicate CSTA records");
                        cell.load(reader);
                        if (!std::isfinite(cell.mWaterLevel) || !std::isfinite(cell.mLastRespawn.mHour))
                            throw std::runtime_error("Saved game shared cell has a nonfinite value");
                        if (cell.mHasFogOfWar)
                        {
                            ESM::FogState fog{};
                            fog.load(reader);
                        }
                        while (reader.isNextSub("OBJE"))
                        {
                            std::uint32_t unused;
                            reader.getHT(unused);
                            ESM::CellRef reference;
                            reference.loadId(reader, true);
                            auto referenceType = shared->find(reference.mRefID);
                            if (!referenceType && content)
                                referenceType = content->findStatic(reference.mRefID);
                            if (referenceType)
                            {
                                const auto state = MWWorld::readSavedReferenceState(reader, reference, referenceType);
                                validatePosition(state->mPosition);
                                // CSTA reference numbers retain their saved load-order
                                // index; base IDs have already been remapped by getRefId.
                                const ESM::InventoryState* inventory = nullptr;
                                if (const auto* npc = dynamic_cast<const ESM::NpcState*>(state.get()))
                                {
                                    inventory = &npc->mInventory;
                                    if (npc->mHasCustomState && npc->mRef.mRefNum.hasContentFile()
                                        && npc->mRef.mRefID.getIf<ESM::FormId>())
                                    {
                                        const auto key = ESM::FormKeyResolver(profile.mContentFiles).toFormKey(
                                            npc->mRef.mRefNum);
                                        if (!sharedBounties.emplace(key, SharedBounty{
                                                sharedOwnerKey(npc->mRef.mRefID, reader, profile),
                                                npc->mNpcStats.mBounty}).second)
                                            throw std::runtime_error("Duplicate shared actor bounty view");
                                    }
                                }
                                else if (const auto* creature = dynamic_cast<const ESM::CreatureState*>(state.get()))
                                    inventory = &creature->mInventory;
                                if (inventory && state->mRef.mRefNum.hasContentFile()
                                    && state->mRef.mRefID.getIf<ESM::FormId>())
                                {
                                    auto items = generatedItems(*inventory);
                                    if (!items.empty())
                                    {
                                        const auto key = ESM::FormKeyResolver(profile.mContentFiles).toFormKey(
                                            state->mRef.mRefNum);
                                        const bool inserted = legacyActorItems.emplace(key, ActorInventory{
                                            sharedOwnerKey(state->mRef.mRefID, reader, profile), std::move(items)}).second;
                                        if (!inserted)
                                            throw std::runtime_error("Duplicate shared actor generated inventory");
                                    }
                                }
                                collectScripts(*state);
                            }
                            else
                                // Match CellStore's deliberate missing-object
                                // compatibility path, without loading a cell.
                                while (reader.hasMoreSubs() && !reader.peekNextSub("OBJE")
                                    && !reader.peekNextSub("MVRF"))
                                {
                                    reader.getSubName();
                                    reader.skipHSub();
                                }
                        }
                        while (reader.isNextSub("MVRF"))
                        {
                            reader.cacheSubName();
                            static_cast<void>(reader.getFormId(true, "MVRF"));
                            static_cast<void>(reader.getCellId());
                        }
                    }
                    if (reader.hasMoreSubs())
                        throw std::runtime_error("Saved game shared world record contains unexpected trailing data");
                }
                MWLua::validateSavedLocalLuaScripts(localScripts, luaConfiguration, reader.getFormatVersion());
            }
            if (nativeRecord)
            {
                if (activeProfile != ESM::GameProfile::Oblivion)
                    throw std::runtime_error("TES4 runtime state encountered while the Morrowind profile is active");
                reader.restoreContext(*nativeRecord);
                ESM4::RuntimeState native;
                native.load(reader);
                if (reader.hasMoreSubs())
                    throw std::runtime_error("TES4 runtime-state record contains unexpected trailing data");
                if (profile.mRuntimeStateVersion != native.mVersion)
                    throw std::runtime_error("Saved game profile runtime-state version does not match T4ST");
                recoverGeneratedItems(native.mPlayer.mInventory, legacyPlayerItems, *shared, reader, profile,
                    native.mVersion);
                for (auto& reference : native.mReferences)
                    if (const auto found = legacyActorItems.find(reference.mKey); found != legacyActorItems.end())
                    {
                        if (found->second.mBase != reference.mBase)
                            throw std::runtime_error("Legacy generated inventory actor base disagrees with native state");
                        recoverGeneratedItems(reference.mInventory, found->second.mItems, *shared, reader, profile,
                            native.mVersion);
                    }
                native.validate();
                for (const auto& actor : native.mNativeActorValues)
                    if (actor.mReputation && sharedPlayerFame && actor.mReputation->mFame != *sharedPlayerFame)
                        throw std::runtime_error("Shared Player Fame view disagrees with native reputation");
                // Owned crime gold is authoritative. Reject a conflicting
                // persisted compatibility view before outgoing-world teardown.
                // Legacy absence remains unresolved rather than assuming zero.
                for (const auto& actor : native.mNativeActorValues)
                    if (native.mVersion >= 45 && actor.mBounty)
                        if (const auto sharedView = sharedBounties.find(actor.mActor);
                            sharedView != sharedBounties.end())
                        {
                            const bool player = actor.mOwner == ESM4::ActorValueOwner::Player;
                            if (!player && sharedView->second.mBase != actor.mBase)
                                throw std::runtime_error("Shared bounty actor base disagrees with native state");
                            const auto amount = ESM4::queryCrimeBounty(*actor.mBounty, player,
                                actor.mPlayerInShiveringIsles);
                            const auto projected = ESM4::convertActorBaseFloat(amount,
                                ESM4::ActorValueConversionMode::Sse);
                            if (projected != sharedView->second.mAmount)
                                throw std::runtime_error("Shared bounty view disagrees with native crime gold");
                        }
                ESM4::validateLocalLuaScriptOwners(nativeScripts, native);
                if (content)
                {
                    if (native.mVersion >= 3 && native.mPlayer.mClass.isDynamic())
                    {
                        const auto playerId = ESM::RefId::stringRefId("Player");
                        const auto* player = shared->get<ESM::NPC>().search(playerId);
                        if (!player)
                            player = content->get<ESM::NPC>().searchStatic(playerId);
                        if (!player || (!shared->get<ESM::Class>().search(player->mClass)
                            && !content->get<ESM::Class>().searchStatic(player->mClass)))
                            throw std::runtime_error("TES4 runtime-state shared Player class cannot be resolved");
                    }
                    for (const auto& [key, value] : native.mGlobals)
                    {
                        const auto* definition = content->get<ESM4::GlobalVariable>().searchStatic(key);
                        if (!definition || definition->mEditorId.empty())
                            throw std::runtime_error("TES4 runtime-state global is not present: " + key.serialize());
                        std::string name = definition->mEditorId;
                        if (Misc::StringUtils::ciEqual(name, "GameDaysPassed")) name = "dayspassed";
                        else if (Misc::StringUtils::ciEqual(name, "GameDay")) name = "day";
                        else if (Misc::StringUtils::ciEqual(name, "GameMonth")) name = "month";
                        else if (Misc::StringUtils::ciEqual(name, "GameYear")) name = "year";
                        const auto id = ESM::RefId::stringRefId(name);
                        const auto saved = globals.find(id);
                        const auto* target = saved != globals.end() ? &saved->second
                            : content->get<ESM::Global>().searchStatic(id);
                        if (!target)
                            throw std::runtime_error("TES4 runtime-state shared global cannot be resolved: " + name);
                        const auto* number = std::get_if<double>(&value);
                        if (target->mValue.getType() == ESM::VT_Float)
                        {
                            const double projected = std::visit([](const auto& item) -> double {
                                if constexpr (std::is_same_v<std::decay_t<decltype(item)>, std::string>)
                                    throw std::runtime_error("TES4 runtime-state numeric global has a string value");
                                else return static_cast<double>(item);
                            }, value);
                            if (!std::isfinite(projected) || std::abs(projected) > std::numeric_limits<float>::max())
                                throw std::runtime_error("TES4 runtime-state global exceeds the finite float domain");
                        }
                        else if (number && (std::trunc(*number) < -0x1p63 || std::trunc(*number) >= 0x1p63))
                            throw std::runtime_error("TES4 runtime-state global exceeds the integer conversion domain");
                    }
                }
                if (prepareNative)
                    prepareNative(native, std::move(shared));
                else
                    validateNative(native);
            }
            else if (!nativeScripts.empty())
                throw std::runtime_error("Native local Lua state requires T4ST");
            reader.restoreContext(start);
            return profile;
        }
        catch (...)
        {
            reader.restoreContext(start);
            throw;
        }
    }
}
