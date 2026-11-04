#ifndef OPENMW_COMPONENTS_ESM4_BOUNDITEMSOURCE_H
#define OPENMW_COMPONENTS_ESM4_BOUNDITEMSOURCE_H
#include <components/esm/formkey.hpp>
#include <cstdint>
namespace ESM4
{
    struct BoundItemSource
    {
        ESM::FormKey mSpell;
        std::uint32_t mEffectIndex = 0;
        friend bool operator==(const BoundItemSource&, const BoundItemSource&) = default;
        friend bool operator<(const BoundItemSource& a, const BoundItemSource& b)
        {
            return a.mSpell < b.mSpell || (a.mSpell == b.mSpell && a.mEffectIndex < b.mEffectIndex);
        }
    };
}
#endif
