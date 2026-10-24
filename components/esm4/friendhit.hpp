#ifndef OPENMW_ESM4_FRIENDHIT_H
#define OPENMW_ESM4_FRIENDHIT_H

#include <components/esm/formkey.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace ESM4
{
    struct FriendHitEntry
    {
        ESM::FormKey mTarget;
        std::uint16_t mCount = 0;
        float mElapsed = 0;

        friend bool operator==(const FriendHitEntry&, const FriendHitEntry&) = default;
    };

    // One actor's native extra 0x4E. List order and allocated-empty presence
    // are observable state. This owns storage only: callers must establish hit
    // classification and actor-update admission before mutating it.
    class FriendHitCounters
    {
    public:
        FriendHitCounters() = default;
        FriendHitCounters(bool present, std::vector<FriendHitEntry> entries);

        bool present() const { return mPresent; }
        std::span<const FriendHitEntry> entries() const { return mEntries; }
        std::uint16_t count(const ESM::FormKey& target) const;
        void increment(const ESM::FormKey& target);
        void remove(const ESM::FormKey& target);

        // Use the global frame delta, only on admitted native actor updates.
        // Advances every timer before the native ordered removal pass.
        // Invalid input/overflow leaves the owner unchanged.
        void advance(float duration, float threshold);

    private:
        bool mPresent = false;
        std::vector<FriendHitEntry> mEntries;
    };
}

#endif
