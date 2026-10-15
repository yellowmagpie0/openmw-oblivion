#ifndef OPENMW_COMPONENTS_NIF_NATIVETRANSFORM_HPP
#define OPENMW_COMPONENTS_NIF_NATIVETRANSFORM_HPP

#include "niftypes.hpp"
#include <optional>

namespace Nif
{
    // Nearest-even binary32 store. Values just above FLT_MAX can still round
    // to finite FLT_MAX; the overflow midpoint and nonfinite values reject.
    // Failure leaves destination unchanged.
    bool tryStoreNativeTransformFloat(double value, float& destination) noexcept;

    // Original NiTransform composition, including binary32 matrix/vector,
    // scaled-vector and translation stores. Separate scale may be zero or
    // negative. Unsupported nonfinite inputs/results return empty without
    // allocating, throwing, changing inputs, or invoking observers.
    std::optional<NiTransform> tryComposeNativeTransform(
        const NiTransform& parent, const NiTransform& local) noexcept;
}

#endif
