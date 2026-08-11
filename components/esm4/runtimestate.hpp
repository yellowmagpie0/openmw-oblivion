#ifndef OPENMW_COMPONENTS_ESM4_RUNTIMESTATE_H
#define OPENMW_COMPONENTS_ESM4_RUNTIMESTATE_H

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <components/esm/defs.hpp>
#include <components/esm/formkey.hpp>
#include <components/esm/gameprofile.hpp>
#include <components/esm/position.hpp>

namespace ESM
{
    class ESMReader;
    class ESMWriter;
}

namespace ESM4
{
    inline constexpr std::uint32_t CurrentRuntimeStateVersion = 2;

    struct RuntimeContentIdentity
    {
        std::string mPlugin;
        std::string mFingerprint;

        friend bool operator==(const RuntimeContentIdentity&, const RuntimeContentIdentity&) = default;
    };

    using RuntimeValue = std::variant<bool, std::int64_t, double, std::string>;

    struct RuntimeInventoryItem
    {
        ESM::FormKey mBase;
        std::int32_t mCount = 0;

        friend bool operator==(const RuntimeInventoryItem&, const RuntimeInventoryItem&) = default;
    };

    struct RuntimeReferenceState
    {
        ESM::FormKey mKey;
        ESM::FormKey mBase;
        ESM::FormKey mCell;
        bool mEnabled = true;
        bool mDeleted = false;
        ESM::Position mPosition{};
        std::optional<ESM::FormKey> mOwner;
        std::int32_t mLockLevel = 0;
        std::vector<RuntimeInventoryItem> mInventory;
        std::map<std::string, RuntimeValue, std::less<>> mCustomState;

        friend bool operator==(const RuntimeReferenceState&, const RuntimeReferenceState&) = default;
    };

    struct RuntimeClockState
    {
        std::int32_t mYear = 1;
        std::int32_t mMonth = 0;
        std::int32_t mDay = 1;
        double mHour = 0;
        double mTimeScale = 30;

        friend bool operator==(const RuntimeClockState&, const RuntimeClockState&) = default;
    };

    struct RuntimePlayerState
    {
        ESM::FormKey mReference;
        ESM::FormKey mCell;
        ESM::Position mPosition{};
        std::map<std::string, double, std::less<>> mActorValues;
        std::vector<RuntimeInventoryItem> mInventory;

        friend bool operator==(const RuntimePlayerState&, const RuntimePlayerState&) = default;
    };

    // Script locals use a distinct value type so reference variables remain
    // distinguishable from strings across a save/reload boundary.
    using RuntimeScriptValue = std::variant<std::monostate, std::int64_t, double, std::string, ESM::FormKey>;

    struct RuntimeScriptInstance
    {
        std::string mUnit;
        ESM::FormKey mContext;
        std::vector<RuntimeScriptValue> mLocals;
        bool mOnLoadFired = false;

        friend bool operator==(const RuntimeScriptInstance&, const RuntimeScriptInstance&) = default;
    };

    struct RuntimeQuestState
    {
        ESM::FormKey mQuest;
        std::int32_t mStage = 0;
        bool mRunning = false;
        std::vector<std::int32_t> mCompletedStages;

        friend bool operator==(const RuntimeQuestState&, const RuntimeQuestState&) = default;
    };

    // Versioned, load-order-independent state owned by the Oblivion profile.
    // The binary representation is private to OpenMW saves and deliberately
    // does not reuse raw load-order indices from Bethesda plugins.
    struct RuntimeState
    {
        static constexpr ESM::RecNameInts sRecordId = ESM::REC_T4ST;

        std::uint32_t mVersion = CurrentRuntimeStateVersion;
        ESM::GameProfile mProfile = ESM::GameProfile::Oblivion;
        std::uint64_t mNextDynamicSerial = 1;
        std::vector<RuntimeContentIdentity> mContent;
        RuntimeClockState mClock;
        RuntimePlayerState mPlayer;
        std::map<ESM::FormKey, RuntimeValue> mGlobals;
        std::vector<RuntimeReferenceState> mReferences;
        std::uint64_t mScriptEventSequence = 0;
        std::vector<RuntimeScriptInstance> mScriptInstances;
        std::vector<RuntimeQuestState> mQuests;

        void validate() const;
        std::vector<std::uint8_t> serializeBinary() const;
        static RuntimeState deserializeBinary(const std::vector<std::uint8_t>& data);

        void save(ESM::ESMWriter& writer) const;
        void load(ESM::ESMReader& reader);

        std::vector<std::string> getMissingContentFiles(const std::vector<std::string>& currentContent) const;
        void validateContent(const std::vector<RuntimeContentIdentity>& currentContent) const;
        std::string canonicalJson() const;

        friend bool operator==(const RuntimeState&, const RuntimeState&) = default;
    };
}

#endif
