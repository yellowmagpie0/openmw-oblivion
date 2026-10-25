#ifndef OPENMW_COMPONENTS_ESM4_NATIVEIDLE_H
#define OPENMW_COMPONENTS_ESM4_NATIVEIDLE_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <components/esm/formid.hpp>

namespace ESM4
{
    struct IdleAnimation;

    // Original 521370 and 520220 consume C strings. These rules preserve
    // separators and operate on the supported ASCII paths shorter than 260
    // bytes, before the first NUL. They do not normalize a VFS path.
    std::string nativeIdleModelKey(std::string_view model);
    bool nativeIdleHasFile(std::string_view model);

    struct NativeIdleNode
    {
        const IdleAnimation* mRecord;
        std::vector<std::size_t> mChildren;
        // Compiled native links, separate from the authored DATA fields.
        std::optional<std::size_t> mParent = {};
        std::optional<std::size_t> mPrevious = {};
    };

    struct NativeIdleModelBank
    {
        std::string mKey;
        std::vector<std::size_t> mRoots;
    };

    struct NativeIdleRegistrationIndex
    {
        std::vector<NativeIdleNode> mNodes;
        std::vector<NativeIdleModelBank> mBanks;
    };

    // Authored registration inputs at one successful content read. These
    // values must survive later overrides of the borrowed winning record.
    struct NativeIdleRegistrationEvent
    {
        ESM::FormId mId;
        ESM::FormId mParent;
        ESM::FormId mPrevious;
        std::string mModel;
        std::uint32_t mFlags = 0;
    };

    NativeIdleRegistrationEvent nativeIdleRegistrationEvent(const IdleAnimation& record);

    // Ordinary same-type overrides remove the previous registration before
    // initializing links and inserting the new definition. Records are final
    // winners; events are actual load order. Parent/predecessor identities
    // must already have been defined. Deleted/0x4000 overrides are outside
    // this verified profile. The result borrows records and owns its links.
    NativeIdleRegistrationIndex replayNativeIdleRegistration(std::span<const IdleAnimation* const> records,
        std::span<const NativeIdleRegistrationEvent> events);

    // Replay one definition per typed identity in registration order. This
    // supported profile requires parent/predecessor identities to be earlier
    // definitions and nonempty root model keys. It does not infer native
    // override/deletion-loader semantics from a final winning snapshot.
    // Insertion itself retains Deleted flags and their append rule.
    NativeIdleRegistrationIndex registerNativeIdleRecords(std::span<const IdleAnimation* const> records);

    // An already-built native bank/forest, not a reconstruction of DATA links
    // from winning records. Registration order and link repair belong to the
    // caller. Structure is owned; records are borrowed and must remain current
    // and alive. A validated finite forest avoids recursive stack growth.
    class NativeIdleSelector
    {
        std::vector<NativeIdleNode> mNodes;
        std::map<std::string, std::vector<std::size_t>> mBanks;

    public:
        NativeIdleSelector(std::vector<NativeIdleNode> nodes, std::span<const NativeIdleModelBank> banks);

        // Original 521450/5206B0: match the actor's raw model bank, prune
        // Deleted/false parents, prefer matching descendants, then consider
        // the parent's optional/file fallback. An optional no-file result
        // stops later roots but the outer file postcheck returns null.
        // The condition adapter supplies native boolean results; this class
        // does not interpret CTDA, select a playback slot or start animation.
        const IdleAnimation* select(std::string_view actorModel,
            const std::function<bool(const IdleAnimation&)>& matches) const;
    };
}

#endif
