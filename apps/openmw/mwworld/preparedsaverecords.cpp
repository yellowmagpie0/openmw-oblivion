#include "preparedsaverecords.hpp"

#include <map>
#include <optional>
#include <stdexcept>

#include <components/esm3/containerstate.hpp>
#include <components/esm3/creaturestate.hpp>
#include <components/esm3/esmreader.hpp>
#include <components/esm3/player.hpp>
#include <components/lua/configuration.hpp>

namespace
{
    struct End
    {
        std::size_t mPayloadEnd;
        std::optional<ESM::NAME> mLookahead;

        explicit End(ESM::ESMReader& reader)
        {
            const auto context = reader.getContext();
            mPayloadEnd = context.filePos;
            if (context.subCached)
            {
                mPayloadEnd -= ESM::NAME::sCapacity;
                mLookahead = context.subName;
            }
        }

        void advance(ESM::ESMReader& reader) const
        {
            // Do not restore a copied ESM_Context here: its filename and parent
            // index vector can allocate. Subrecord framing needs no DTO storage.
            while (reader.getFileOffset() < mPayloadEnd && reader.hasMoreSubs())
            {
                reader.getSubName();
                reader.skipHSub();
            }
            if (reader.getFileOffset() != mPayloadEnd
                || (mLookahead && !reader.peekNextSub(*mLookahead)))
                throw std::logic_error("Prepared save record boundary changed");
        }
    };

    void bindLegacyScripts(ESM::LuaScripts& scripts, const ESM::ESMReader& reader)
    {
        for (auto& script : scripts.mScripts)
            if (script.mLegacyPath)
                script.mScriptId = reader.mScriptsConfiguration
                    ? reader.mScriptsConfiguration->findId(*script.mLegacyPath).value_or(-1) : -1;
    }

    void bindRuntimeState(ESM::ObjectState& state, ESM::ESMReader& reader)
    {
        auto* converter = reader.mActorIdConverter;
        state.mActorIdConverter = converter;
        bindLegacyScripts(state.mLuaScripts, reader);
        ESM::InventoryState* inventory = nullptr;
        ESM::CreatureStats* stats = nullptr;
        if (auto* npc = dynamic_cast<ESM::NpcState*>(&state); npc && state.mHasCustomState)
        {
            inventory = &npc->mInventory;
            stats = &npc->mCreatureStats;
        }
        else if (auto* creature = dynamic_cast<ESM::CreatureState*>(&state); creature && state.mHasCustomState)
        {
            inventory = &creature->mInventory;
            stats = &creature->mCreatureStats;
        }
        else if (auto* container = dynamic_cast<ESM::ContainerState*>(&state))
            inventory = &container->mInventory;
        if (inventory)
            for (auto& item : inventory->mItems)
            {
                item.mActorIdConverter = converter;
                bindLegacyScripts(item.mLuaScripts, reader);
            }
        if (stats)
        {
            stats->mActiveSpells.mActorIdConverter = converter;
            stats->mAiSequence.mActorIdConverter = converter;
        }
    }
}

struct MWWorld::PreparedSaveRecords::Impl
{
    struct Reference
    {
        End mEnd;
        std::uint32_t mType;
        ESM::RefNum mSavedRefNum;
        ESM::RefId mBase;
        std::unique_ptr<ESM::ObjectState> mState;
    };
    struct Player
    {
        End mEnd;
        ESM::RefNum mSavedRefNum;
        std::unique_ptr<ESM::Player> mState;
    };
    ESM::ESMReader* mReader;
    std::filesystem::path mFilename;
    ESM::FormatVersion mVersion;
    std::size_t mFileSize;
    std::map<std::size_t, Reference> mReferences;
    std::map<std::size_t, Player> mPlayers;
    std::size_t mRemaining = 0;

    explicit Impl(ESM::ESMReader& reader)
        : mReader(&reader)
        , mFilename(reader.getName())
        , mVersion(reader.getFormatVersion())
        , mFileSize(reader.getFileSize())
    {
    }
    void checkReader(ESM::ESMReader& reader) const
    {
        if (&reader != mReader || reader.getName() != mFilename || reader.getFormatVersion() != mVersion
            || reader.getFileSize() != mFileSize)
            throw std::logic_error("Prepared save records belong to another reader");
    }
};

MWWorld::PreparedSaveRecords::PreparedSaveRecords(ESM::ESMReader& reader)
    : mImpl(std::make_unique<Impl>(reader))
{
}

MWWorld::PreparedSaveRecords::~PreparedSaveRecords() = default;

void MWWorld::PreparedSaveRecords::addReference(ESM::ESMReader& reader, std::size_t begin, std::uint32_t type,
    std::unique_ptr<ESM::ObjectState> state)
{
    mImpl->checkReader(reader);
    if (!state) throw std::logic_error("Cannot prepare an empty saved reference");
    Impl::Reference entry{End(reader), type, state->mRef.mRefNum, state->mRef.mRefID, std::move(state)};
    if (!mImpl->mReferences.emplace(begin, std::move(entry)).second)
        throw std::logic_error("Duplicate prepared save reference offset");
    ++mImpl->mRemaining;
}

void MWWorld::PreparedSaveRecords::addPlayer(
    ESM::ESMReader& reader, std::size_t begin, std::unique_ptr<ESM::Player> state)
{
    mImpl->checkReader(reader);
    if (!state) throw std::logic_error("Cannot prepare an empty saved Player");
    Impl::Player entry{End(reader), state->mObject.mRef.mRefNum, std::move(state)};
    if (!mImpl->mPlayers.emplace(begin, std::move(entry)).second)
        throw std::logic_error("Duplicate prepared save Player offset");
    ++mImpl->mRemaining;
}

std::unique_ptr<ESM::ObjectState> MWWorld::PreparedSaveRecords::takeReference(
    ESM::ESMReader& reader, const ESM::CellRef& reference, std::uint32_t type)
{
    mImpl->checkReader(reader);
    const auto found = mImpl->mReferences.find(reader.getFileOffset());
    if (found == mImpl->mReferences.end()) return {};
    auto& entry = found->second;
    if (!entry.mState) throw std::logic_error("Prepared saved reference was already consumed");
    if (entry.mType != type || entry.mBase != reference.mRefID || entry.mSavedRefNum != reference.mRefNum
        || entry.mState->mRef.mRefID != entry.mBase)
        throw std::logic_error("Prepared saved reference identity changed");
    entry.mEnd.advance(reader);
    // Inventory preparation may have mapped the owner in place. The ordinary
    // cell reader must still apply its saved-to-active mapping exactly once.
    entry.mState->mRef.mRefNum = entry.mSavedRefNum;
    bindRuntimeState(*entry.mState, reader);
    auto result = std::move(entry.mState);
    --mImpl->mRemaining;
    return result;
}

std::unique_ptr<ESM::Player> MWWorld::PreparedSaveRecords::takePlayer(ESM::ESMReader& reader)
{
    mImpl->checkReader(reader);
    const auto found = mImpl->mPlayers.find(reader.getFileOffset());
    if (found == mImpl->mPlayers.end()) return {};
    auto& entry = found->second;
    if (!entry.mState) throw std::logic_error("Prepared saved Player was already consumed");
    entry.mEnd.advance(reader);
    entry.mState->mObject.mRef.mRefNum = entry.mSavedRefNum;
    bindRuntimeState(entry.mState->mObject, reader);
    auto result = std::move(entry.mState);
    --mImpl->mRemaining;
    return result;
}

std::size_t MWWorld::PreparedSaveRecords::size() const
{
    return mImpl->mRemaining;
}
