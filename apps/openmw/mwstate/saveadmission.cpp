#include "saveadmission.hpp"

#include <optional>
#include <stdexcept>
#include <cmath>
#include <limits>
#include <map>
#include <set>

#include <components/esm3/esmreader.hpp>
#include <components/esm3/loadglob.hpp>
#include <components/esm3/loadnpc.hpp>
#include <components/esm3/loadclas.hpp>
#include <components/esm3/cellstate.hpp>
#include <components/esm3/fogstate.hpp>
#include <components/esm3/player.hpp>
#include <components/esm4/loadglob.hpp>
#include <components/esm4/runtimestate.hpp>
#include <components/misc/strings/algorithm.hpp>

#include "../mwworld/esmstore.hpp"
#include "../mwworld/savedreference.hpp"
#include "../mwlua/userdataserializer.hpp"

namespace MWState
{
    ESM::SavedGame admitSave(ESM::ESMReader& reader, ESM::GameProfile activeProfile,
        const std::function<void(const ESM4::RuntimeState&)>& validateNative,
        const MWWorld::ESMStore* content)
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
            if (activeProfile == ESM::GameProfile::Oblivion)
            {
                // Framing alone does not prove that shared dynamic records can
                // be decoded. Use the production store reader on a detached
                // store before any outgoing world is cleared. Do not set up
                // or publish this store: content-dependent reconciliation is
                // a separate preparation step.
                shared = std::make_unique<MWWorld::ESMStore>();
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
                        shared->getWritable<ESM::NPC>().insertStatic(npc);
                        decoded = true;
                    }
                    else if (type == ESM::REC_CLAS)
                    {
                        ESM::Class characterClass{};
                        bool deleted = false;
                        characterClass.load(reader, deleted);
                        if (deleted)
                            throw std::runtime_error("Saved game contains a deleted shared class");
                        shared->getWritable<ESM::Class>().insertStatic(characterClass);
                        decoded = true;
                    }
                    else
                        decoded = shared->readRecord(reader, type.toInt());
                    if (!decoded)
                        reader.skipRecord();
                    else if (reader.hasMoreSubs())
                        throw std::runtime_error("Saved game shared record contains unexpected trailing data");
                }
                shared->rebuildIdsIndex();
                std::set<ESM::RefId> cells;
                bool hasPlayer = false;
                bool hasLua = false;
                const auto validatePosition = [](const ESM::Position& position) {
                    for (int axis = 0; axis != 3; ++axis)
                        if (!std::isfinite(position.pos[axis]) || !std::isfinite(position.rot[axis]))
                            throw std::runtime_error("Saved game shared reference has a nonfinite position");
                };
                for (const auto& [type, context] : worldRecords)
                {
                    reader.restoreContext(context);
                    if (type == ESM::REC_LUAM)
                    {
                        if (hasLua)
                            throw std::runtime_error("Saved game contains duplicate LUAM records");
                        hasLua = true;
                        nativeScripts = MWLua::validateSavedLuaRecord(
                            reader, content ? content->getLuaScriptsCfg() : ESM::LuaScriptsCfg{});
                        if (content)
                            ESM4::validateLocalLuaScriptContent(nativeScripts, content->getFormKeyIndex());
                    }
                    else if (type == ESM::REC_PLAY)
                    {
                        if (hasPlayer)
                            throw std::runtime_error("Saved game contains duplicate PLAY records");
                        hasPlayer = true;
                        ESM::Player player{};
                        player.load(reader);
                        validatePosition(player.mObject.mPosition);
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
