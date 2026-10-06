#include "saveadmission.hpp"

#include <optional>
#include <stdexcept>

#include <components/esm3/esmreader.hpp>
#include <components/esm4/runtimestate.hpp>

namespace MWState
{
    ESM::SavedGame admitSave(ESM::ESMReader& reader, ESM::GameProfile activeProfile,
        const std::function<void(const ESM4::RuntimeState&)>& validateNative)
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
                validateNative(native);
            }
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
