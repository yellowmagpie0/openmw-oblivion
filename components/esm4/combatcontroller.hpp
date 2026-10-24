#ifndef OPENMW_ESM4_COMBATCONTROLLER_H
#define OPENMW_ESM4_COMBATCONTROLLER_H

#include <components/esm/formkey.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace ESM4
{
    struct CombatControllerTarget
    {
        ESM::FormKey mTarget;
        std::int32_t mPriority = 0;
        std::uint8_t mMarker = 0;
        float mValue0C = 0;
        float mValue10 = 0;

        friend bool operator==(const CombatControllerTarget&, const CombatControllerTarget&) = default;
    };

    // Original 616190 admission. Identities are canonical actor references;
    // targetCommandSource is the target process's CommandEffect caster, not
    // its AI target. Geometry is read only when integer Speed is zero.
    bool admitsCombatControllerTarget(const ESM::FormKey& owner, const ESM::FormKey& target,
        const ESM::FormKey& targetCommandSource, std::int32_t speed, float naturalReach, float distance);

    // Logical contents of one native CombatController's directed target list.
    // Package installation, constructor RNG and decision updates belong to the
    // caller. Symmetric combat engagement does not establish this authority.
    class CombatControllerTargets
    {
    public:
        CombatControllerTargets() = default;
        explicit CombatControllerTargets(std::vector<CombatControllerTarget> entries);

        std::span<const CombatControllerTarget> entries() const { return mEntries; }
        bool contains(const ESM::FormKey& target) const;
        ESM::FormKey first() const;
        // Call admission first. Duplicates are complete no-ops; new equal
        // priorities precede old ones. Raw marker and finite float bits survive.
        bool add(const CombatControllerTarget& entry);
        bool remove(const ESM::FormKey& target);

    private:
        std::vector<CombatControllerTarget> mEntries;
    };
}

#endif
