#ifndef OPENMW_COMPONENTS_ESM4_NATIVEIDLE_H
#define OPENMW_COMPONENTS_ESM4_NATIVEIDLE_H

#include <cstddef>
#include <functional>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

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
    };

    struct NativeIdleModelBank
    {
        std::string mKey;
        std::vector<std::size_t> mRoots;
    };

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
