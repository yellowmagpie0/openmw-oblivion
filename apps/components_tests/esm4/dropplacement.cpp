#include <components/esm4/dropplacement.hpp>
#include <components/nif/niftypes.hpp>
#include <gtest/gtest.h>
#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <cmath>
#include <stdexcept>

namespace
{
    struct NativePlacementCase
    {
        const char* mName;
        std::array<float, 9> mMatrix;
        std::array<unsigned, 3> mRotationBits;
        bool mUnique;
    };
    // Original executable a8f31384..., 005F3A93 caller and 00711440.
    // Independent outputs from M15-ORIGINAL-DROP-PLACEMENT-EXPECTED.json.
    // Both original x87 control words agree for these 80 matrices.
    const std::array<NativePlacementCase, 80> cases{{
        {"identity", {0x1.0000000000000p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f}, {0x80000000u, 0x80000000u, 0x80000000u}, true},
        {"x+90", {0x1.0000000000000p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, -0x1.0000000000000p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f, 0x0.0p+0f}, {0xbfc90fdbu, 0x0u, 0x0u}, false},
        {"x-90", {0x1.0000000000000p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f, 0x0.0p+0f, -0x1.0000000000000p+0f, 0x0.0p+0f}, {0x3fc90fdbu, 0x0u, 0x0u}, false},
        {"y+90", {0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f, 0x0.0p+0f, -0x1.0000000000000p+0f, 0x0.0p+0f, 0x0.0p+0f}, {0x80000000u, 0xbfc90fdbu, 0x80000000u}, true},
        {"y-90", {0x0.0p+0f, 0x0.0p+0f, -0x1.0000000000000p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f, 0x0.0p+0f, 0x0.0p+0f}, {0x80000000u, 0x3fc90fdbu, 0x80000000u}, true},
        {"z+90", {0x0.0p+0f, -0x1.0000000000000p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f}, {0x80000000u, 0x80000000u, 0xbfc90fdbu}, true},
        {"z-90", {0x0.0p+0f, 0x1.0000000000000p+0f, 0x0.0p+0f, -0x1.0000000000000p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f}, {0x80000000u, 0x80000000u, 0x3fc90fdbu}, true},
        {"x0.3", {0x1.0000000000000p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.e921de0000000p-1f, -0x1.2e9cda0000000p-2f, 0x0.0p+0f, 0x1.2e9cda0000000p-2f, 0x1.e921de0000000p-1f}, {0xbe99999au, 0x80000000u, 0x80000000u}, true},
        {"y0.3", {0x1.e921de0000000p-1f, 0x0.0p+0f, 0x1.2e9cda0000000p-2f, 0x0.0p+0f, 0x1.0000000000000p+0f, 0x0.0p+0f, -0x1.2e9cda0000000p-2f, 0x0.0p+0f, 0x1.e921de0000000p-1f}, {0x80000000u, 0xbe999a56u, 0x80000000u}, true},
        {"z0.3", {0x1.e921de0000000p-1f, -0x1.2e9cda0000000p-2f, 0x0.0p+0f, 0x1.2e9cda0000000p-2f, 0x1.e921de0000000p-1f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f}, {0x80000000u, 0x80000000u, 0xbe999a56u}, true},
        {"x-0.8", {0x1.0000000000000p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.64b6be0000000p-1f, 0x1.6f494c0000000p-1f, 0x0.0p+0f, -0x1.6f494c0000000p-1f, 0x1.64b6be0000000p-1f}, {0x3f4ccccdu, 0x80000000u, 0x80000000u}, true},
        {"y-0.8", {0x1.64b6be0000000p-1f, 0x0.0p+0f, -0x1.6f494c0000000p-1f, 0x0.0p+0f, 0x1.0000000000000p+0f, 0x0.0p+0f, 0x1.6f494c0000000p-1f, 0x0.0p+0f, 0x1.64b6be0000000p-1f}, {0x80000000u, 0x3f4ccd6au, 0x80000000u}, true},
        {"z-0.8", {0x1.64b6be0000000p-1f, 0x1.6f494c0000000p-1f, 0x0.0p+0f, -0x1.6f494c0000000p-1f, 0x1.64b6be0000000p-1f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f}, {0x80000000u, 0x80000000u, 0x3f4ccd6au}, true},
        {"x1.2", {0x1.0000000000000p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.730dea0000000p-2f, -0x1.dd343a0000000p-1f, 0x0.0p+0f, 0x1.dd343a0000000p-1f, 0x1.730dea0000000p-2f}, {0xbf99999au, 0x80000000u, 0x80000000u}, true},
        {"y1.2", {0x1.730dea0000000p-2f, 0x0.0p+0f, 0x1.dd343a0000000p-1f, 0x0.0p+0f, 0x1.0000000000000p+0f, 0x0.0p+0f, -0x1.dd343a0000000p-1f, 0x0.0p+0f, 0x1.730dea0000000p-2f}, {0x80000000u, 0xbf99993au, 0x80000000u}, true},
        {"z1.2", {0x1.730dea0000000p-2f, -0x1.dd343a0000000p-1f, 0x0.0p+0f, 0x1.dd343a0000000p-1f, 0x1.730dea0000000p-2f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f}, {0x80000000u, 0x80000000u, 0xbf99993au}, true},
        {"combined--0.8,0.0,-0.2", {0x1.f5cb4a0000000p-1f, 0x1.1b78ee0000000p-3f, 0x1.23dfc60000000p-3f, -0x1.96dff20000000p-3f, 0x1.5d9a720000000p-1f, 0x1.67f70e0000000p-1f, 0x0.0p+0f, -0x1.6f494c0000000p-1f, 0x1.64b6be0000000p-1f}, {0x3f4ccccdu, 0x80000000u, 0x3e4ccab9u}, true},
        {"combined--0.8,0.0,0.4", {0x1.d7954e0000000p-1f, -0x1.15d2620000000p-2f, -0x1.1e0e5a0000000p-2f, 0x1.8ec3ae0000000p-2f, 0x1.488e220000000p-1f, 0x1.524b080000000p-1f, 0x0.0p+0f, -0x1.6f494c0000000p-1f, 0x1.64b6be0000000p-1f}, {0x3f4ccccdu, 0x80000000u, 0xbeccce39u}, true},
        {"combined--0.8,-0.7,-0.2", {0x1.7fcb140000000p-1f, 0x1.2ec3920000000p-1f, -0x1.3081140000000p-2f, -0x1.3731ec0000000p-3f, 0x1.2e98800000000p-1f, 0x1.959e9a0000000p-1f, 0x1.49d6e60000000p-1f, -0x1.18ea840000000p-1f, 0x1.10d46e0000000p-1f}, {0x3f14ad0fu, 0x3f61341bu, 0x3f49215eu}, true},
        {"combined--0.8,-0.7,0.4", {0x1.68afce0000000p-1f, 0x1.3c180e0000000p-3f, -0x1.62b0680000000p-1f, 0x1.30fde80000000p-2f, 0x1.a4b2460000000p-1f, 0x1.f19bc40000000p-2f, 0x1.49d6e60000000p-1f, -0x1.18ea840000000p-1f, 0x1.10d46e0000000p-1f}, {0x3f14ad0fu, 0x3f61341bu, 0x3e3e1f74u}, true},
        {"combined--0.8,0.5,-0.2", {0x1.b85da20000000p-1f, -0x1.96d5560000000p-3f, 0x1.e127920000000p-2f, -0x1.6510f40000000p-3f, 0x1.8096160000000p-1f, 0x1.45fd340000000p-1f, -0x1.eaee880000000p-2f, -0x1.4252f60000000p-1f, 0x1.390bbc0000000p-1f}, {0x3f2e5365u, 0xbf2a3ce0u, 0xbe845faau}, true},
        {"combined--0.8,0.5,0.4", {0x1.9dda6c0000000p-1f, -0x1.2d18e60000000p-1f, 0x1.cfabd20000000p-6f, 0x1.5df2dc0000000p-2f, 0x1.03fbe20000000p-1f, 0x1.94e3fa0000000p-1f, -0x1.eaee880000000p-2f, -0x1.4252f60000000p-1f, 0x1.390bbc0000000p-1f}, {0x3f2e5365u, 0xbf2a3ce0u, 0xbf5bc8e2u}, true},
        {"combined-0.3,0.0,-0.2", {0x1.f5cb4a0000000p-1f, 0x1.84b3cc0000000p-3f, -0x1.e0f57a0000000p-5f, -0x1.96dff20000000p-3f, 0x1.df61d80000000p-1f, -0x1.2894a20000000p-2f, 0x0.0p+0f, 0x1.2e9cda0000000p-2f, 0x1.e921de0000000p-1f}, {0xbe99999au, 0x80000000u, 0x3e4ccab9u}, true},
        {"combined-0.3,0.0,0.4", {0x1.d7954e0000000p-1f, -0x1.7cf4440000000p-2f, 0x1.d75f2a0000000p-4f, 0x1.8ec3ae0000000p-2f, 0x1.c2854a0000000p-1f, -0x1.16b9880000000p-2f, 0x0.0p+0f, 0x1.2e9cda0000000p-2f, 0x1.e921de0000000p-1f}, {0xbe99999au, 0x80000000u, 0xbeccce39u}, true},
        {"combined-0.3,-0.7,-0.2", {0x1.7fcb140000000p-1f, 0x1.a4f4600000000p-9f, -0x1.52e2ec0000000p-1f, -0x1.3731ec0000000p-3f, 0x1.f2bf520000000p-1f, -0x1.56c0940000000p-3f, 0x1.49d6e60000000p-1f, 0x1.cee6e40000000p-3f, 0x1.761be80000000p-1f}, {0xbe6977eau, 0x3f38fc39u, 0x3b580a54u}, true},
        {"combined-0.3,-0.7,0.4", {0x1.68afce0000000p-1f, -0x1.1841bc0000000p-1f, -0x1.ce9fb40000000p-2f, 0x1.30fde80000000p-2f, 0x1.9c8ffa0000000p-1f, -0x1.0612320000000p-1f, 0x1.49d6e60000000p-1f, 0x1.cee6e40000000p-3f, 0x1.761be80000000p-1f}, {0xbe6977eau, 0x3f38fc39u, 0xbf18c0efu}, true},
        {"combined-0.3,0.5,-0.2", {0x1.b85da20000000p-1f, 0x1.508a1a0000000p-2f, 0x1.8f894a0000000p-2f, -0x1.6510f40000000p-3f, 0x1.d0f8800000000p-1f, -0x1.85c1ec0000000p-2f, -0x1.eaee880000000p-2f, 0x1.0991500000000p-2f, 0x1.ad41040000000p-1f}, {0xbe8651bau, 0xbf04fac2u, 0x3eb1c993u}, true},
        {"combined-0.3,0.5,0.4", {0x1.9dda6c0000000p-1f, -0x1.eea71e0000000p-3f, 0x1.12e99e0000000p-1f, 0x1.5df2dc0000000p-2f, 0x1.dec4e60000000p-1f, -0x1.8057a60000000p-4f, -0x1.eaee880000000p-2f, 0x1.0991500000000p-2f, 0x1.ad41040000000p-1f}, {0xbe8651bau, 0xbf04fac2u, 0xbe816ad8u}, true},
        {"combined-1.5707963267948966,0.0,-0.2", {0x1.f5cb4a0000000p-1f, 0x1.c0cf100000000p-57f, -0x1.96dff20000000p-3f, -0x1.96dff20000000p-3f, 0x1.14c1640000000p-54f, -0x1.f5cb4a0000000p-1f, 0x0.0p+0f, 0x1.0000000000000p+0f, 0x1.1a62640000000p-54f}, {0xbfc90fdbu, 0x0u, 0xbe4ccab9u}, false},
        {"combined-1.5707963267948966,0.0,0.4", {0x1.d7954e0000000p-1f, -0x1.b7dcd00000000p-56f, 0x1.8ec3ae0000000p-2f, 0x1.8ec3ae0000000p-2f, 0x1.0417dc0000000p-54f, -0x1.d7954e0000000p-1f, 0x0.0p+0f, 0x1.0000000000000p+0f, 0x1.1a62640000000p-54f}, {0xbfc90fdbu, 0x0u, 0x3eccce39u}, false},
        {"combined-1.5707963267948966,-0.7,-0.2", {0x1.7fcb140000000p-1f, -0x1.4343be0000000p-1f, -0x1.96dff20000000p-3f, -0x1.3731ec0000000p-3f, 0x1.061dae0000000p-3f, -0x1.f5cb4a0000000p-1f, 0x1.49d6e60000000p-1f, 0x1.8799660000000p-1f, 0x1.aff57a0000000p-55f}, {0xbf5eec83u, 0x3fc90fdbu, 0xbfaf7684u}, true},
        {"combined-1.5707963267948966,-0.7,0.4", {0x1.68afce0000000p-1f, -0x1.2fcd600000000p-1f, 0x1.8ec3ae0000000p-2f, 0x1.30fde80000000p-2f, -0x1.00e41e0000000p-2f, -0x1.d7954e0000000p-1f, 0x1.49d6e60000000p-1f, 0x1.8799660000000p-1f, 0x1.aff57a0000000p-55f}, {0xbf5eec83u, 0x3fc90fdbu, 0xbffc436au}, true},
        {"combined-1.5707963267948966,0.5,-0.2", {0x1.b85da20000000p-1f, 0x1.e125540000000p-2f, -0x1.96dff20000000p-3f, -0x1.6510f40000000p-3f, -0x1.8621e00000000p-4f, -0x1.f5cb4a0000000p-1f, -0x1.eaee880000000p-2f, 0x1.c152800000000p-1f, 0x1.efa1920000000p-55f}, {0xbf890fdau, 0xbfc90fdbu, 0x3fe2a932u}, true},
        {"combined-1.5707963267948966,0.5,0.4", {0x1.9dda6c0000000p-1f, 0x1.c42d980000000p-2f, 0x1.8ec3ae0000000p-2f, 0x1.5df2dc0000000p-2f, 0x1.7e5b0c0000000p-3f, -0x1.d7954e0000000p-1f, -0x1.eaee880000000p-2f, 0x1.c152800000000p-1f, 0x1.efa1920000000p-55f}, {0xbf890fdau, 0xbfc90fdbu, 0x3f95dc4du}, true},
        {"signed-zero-0", {0x1.0000000000000p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f}, {0x80000000u, 0x80000000u, 0x80000000u}, true},
        {"signed-zero-1", {0x1.0000000000000p+0f, -0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f}, {0x80000000u, 0x80000000u, 0x80000000u}, true},
        {"signed-zero-2", {0x1.0000000000000p+0f, 0x0.0p+0f, -0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f}, {0x80000000u, 0x80000000u, 0x80000000u}, true},
        {"signed-zero-3", {0x1.0000000000000p+0f, -0x0.0p+0f, -0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f}, {0x80000000u, 0x80000000u, 0x80000000u}, true},
        {"signed-zero-4", {0x1.0000000000000p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f, 0x0.0p+0f, -0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f}, {0x80000000u, 0x80000000u, 0x80000000u}, true},
        {"signed-zero-5", {0x1.0000000000000p+0f, -0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f, 0x0.0p+0f, -0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f}, {0x80000000u, 0x80000000u, 0x80000000u}, true},
        {"signed-zero-6", {0x1.0000000000000p+0f, 0x0.0p+0f, -0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f, 0x0.0p+0f, -0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f}, {0x80000000u, 0x80000000u, 0x80000000u}, true},
        {"signed-zero-7", {0x1.0000000000000p+0f, -0x0.0p+0f, -0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f, 0x0.0p+0f, -0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f}, {0x80000000u, 0x80000000u, 0x80000000u}, true},
        {"signed-zero-8", {0x1.0000000000000p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f, 0x0.0p+0f, 0x0.0p+0f, -0x0.0p+0f, 0x1.0000000000000p+0f}, {0x0u, 0x80000000u, 0x80000000u}, true},
        {"signed-zero-9", {0x1.0000000000000p+0f, -0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f, 0x0.0p+0f, 0x0.0p+0f, -0x0.0p+0f, 0x1.0000000000000p+0f}, {0x0u, 0x80000000u, 0x80000000u}, true},
        {"signed-zero-10", {0x1.0000000000000p+0f, 0x0.0p+0f, -0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f, 0x0.0p+0f, 0x0.0p+0f, -0x0.0p+0f, 0x1.0000000000000p+0f}, {0x0u, 0x80000000u, 0x80000000u}, true},
        {"signed-zero-11", {0x1.0000000000000p+0f, -0x0.0p+0f, -0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f, 0x0.0p+0f, 0x0.0p+0f, -0x0.0p+0f, 0x1.0000000000000p+0f}, {0x0u, 0x80000000u, 0x80000000u}, true},
        {"signed-zero-12", {0x1.0000000000000p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f, 0x0.0p+0f, -0x0.0p+0f, -0x0.0p+0f, 0x1.0000000000000p+0f}, {0x0u, 0x80000000u, 0x80000000u}, true},
        {"signed-zero-13", {0x1.0000000000000p+0f, -0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f, 0x0.0p+0f, -0x0.0p+0f, -0x0.0p+0f, 0x1.0000000000000p+0f}, {0x0u, 0x80000000u, 0x80000000u}, true},
        {"signed-zero-14", {0x1.0000000000000p+0f, 0x0.0p+0f, -0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f, 0x0.0p+0f, -0x0.0p+0f, -0x0.0p+0f, 0x1.0000000000000p+0f}, {0x0u, 0x80000000u, 0x80000000u}, true},
        {"signed-zero-15", {0x1.0000000000000p+0f, -0x0.0p+0f, -0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f, 0x0.0p+0f, -0x0.0p+0f, -0x0.0p+0f, 0x1.0000000000000p+0f}, {0x0u, 0x80000000u, 0x80000000u}, true},
        {"gimbal-0x3f7ffffe--1", {0x1.0000000000000p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.fffffe0000000p-12f, 0x1.fffffc0000000p-1f, 0x0.0p+0f, -0x1.fffffc0000000p-1f, 0x1.fffffe0000000p-12f}, {0x3fc8ffdbu, 0x80000000u, 0x80000000u}, true},
        {"gimbal-0x3f7ffffe-1", {0x1.0000000000000p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.fffffe0000000p-12f, -0x1.fffffc0000000p-1f, 0x0.0p+0f, 0x1.fffffc0000000p-1f, 0x1.fffffe0000000p-12f}, {0xbfc8ffdbu, 0x80000000u, 0x80000000u}, true},
        {"gimbal-0x3f7fffff--1", {0x1.0000000000000p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.6a09e60000000p-12f, 0x1.fffffe0000000p-1f, 0x0.0p+0f, -0x1.fffffe0000000p-1f, 0x1.6a09e60000000p-12f}, {0x3fc9048au, 0x80000000u, 0x80000000u}, true},
        {"gimbal-0x3f7fffff-1", {0x1.0000000000000p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.6a09e60000000p-12f, -0x1.fffffe0000000p-1f, 0x0.0p+0f, 0x1.fffffe0000000p-1f, 0x1.6a09e60000000p-12f}, {0xbfc9048au, 0x80000000u, 0x80000000u}, true},
        {"gimbal-0x3f800000--1", {0x1.0000000000000p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f, 0x0.0p+0f, -0x1.0000000000000p+0f, 0x0.0p+0f}, {0x3fc90fdbu, 0x0u, 0x0u}, false},
        {"gimbal-0x3f800000-1", {0x1.0000000000000p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, -0x1.0000000000000p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f, 0x0.0p+0f}, {0xbfc90fdbu, 0x0u, 0x0u}, false},
        {"gimbal-0x3f800001--1", {0x1.0000000000000p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.0000020000000p+0f, 0x0.0p+0f, -0x1.0000020000000p+0f, 0x0.0p+0f}, {0x3fc90fdbu, 0x0u, 0x0u}, false},
        {"gimbal-0x3f800001-1", {0x1.0000000000000p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, -0x1.0000020000000p+0f, 0x0.0p+0f, 0x1.0000020000000p+0f, 0x0.0p+0f}, {0xbfc90fdbu, 0x0u, 0x0u}, false},
        {"embedded-scale-identity--2.0", {-0x1.0000000000000p+1f, -0x0.0p+0f, -0x0.0p+0f, -0x0.0p+0f, -0x1.0000000000000p+1f, -0x0.0p+0f, -0x0.0p+0f, -0x0.0p+0f, -0x1.0000000000000p+1f}, {0x0u, 0xc0490fdbu, 0xc0490fdbu}, true},
        {"embedded-scale-identity--0.5", {-0x1.0000000000000p-1f, -0x0.0p+0f, -0x0.0p+0f, -0x0.0p+0f, -0x1.0000000000000p-1f, -0x0.0p+0f, -0x0.0p+0f, -0x0.0p+0f, -0x1.0000000000000p-1f}, {0x0u, 0xc0490fdbu, 0xc0490fdbu}, true},
        {"embedded-scale-identity-0.5", {0x1.0000000000000p-1f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p-1f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p-1f}, {0x80000000u, 0x80000000u, 0x80000000u}, true},
        {"embedded-scale-identity-2.0", {0x1.0000000000000p+1f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p+1f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p+1f}, {0x80000000u, 0x80000000u, 0x80000000u}, true},
        {"embedded-scale-x0.3--2.0", {-0x1.0000000000000p+1f, -0x0.0p+0f, -0x0.0p+0f, -0x0.0p+0f, -0x1.e921de0000000p+0f, 0x1.2e9cda0000000p-1f, -0x0.0p+0f, -0x1.2e9cda0000000p-1f, -0x1.e921de0000000p+0f}, {0x3f21e190u, 0xc0490fdbu, 0xc0490fdbu}, true},
        {"embedded-scale-x0.3--0.5", {-0x1.0000000000000p-1f, -0x0.0p+0f, -0x0.0p+0f, -0x0.0p+0f, -0x1.e921de0000000p-2f, 0x1.2e9cda0000000p-3f, -0x0.0p+0f, -0x1.2e9cda0000000p-3f, -0x1.e921de0000000p-2f}, {0x3e17dcc7u, 0xc0490fdbu, 0xc0490fdbu}, true},
        {"embedded-scale-x0.3-0.5", {0x1.0000000000000p-1f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.e921de0000000p-2f, -0x1.2e9cda0000000p-3f, 0x0.0p+0f, 0x1.2e9cda0000000p-3f, 0x1.e921de0000000p-2f}, {0xbe17dcc7u, 0x80000000u, 0x80000000u}, true},
        {"embedded-scale-x0.3-2.0", {0x1.0000000000000p+1f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.e921de0000000p+0f, -0x1.2e9cda0000000p-1f, 0x0.0p+0f, 0x1.2e9cda0000000p-1f, 0x1.e921de0000000p+0f}, {0xbf21e190u, 0x80000000u, 0x80000000u}, true},
        {"embedded-scale-x1.2--2.0", {-0x1.0000000000000p+1f, -0x0.0p+0f, -0x0.0p+0f, -0x0.0p+0f, -0x1.730dea0000000p-1f, 0x1.dd343a0000000p+0f, -0x0.0p+0f, -0x1.dd343a0000000p+0f, -0x1.730dea0000000p-1f}, {0x3fc90fdbu, 0x0u, 0xc0490fdbu}, false},
        {"embedded-scale-x1.2--0.5", {-0x1.0000000000000p-1f, -0x0.0p+0f, -0x0.0p+0f, -0x0.0p+0f, -0x1.730dea0000000p-3f, 0x1.dd343a0000000p-2f, -0x0.0p+0f, -0x1.dd343a0000000p-2f, -0x1.730dea0000000p-3f}, {0x3ef835f2u, 0xc0490fdbu, 0xc0490fdbu}, true},
        {"embedded-scale-x1.2-0.5", {0x1.0000000000000p-1f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.730dea0000000p-3f, -0x1.dd343a0000000p-2f, 0x0.0p+0f, 0x1.dd343a0000000p-2f, 0x1.730dea0000000p-3f}, {0xbef835f2u, 0x80000000u, 0x80000000u}, true},
        {"embedded-scale-x1.2-2.0", {0x1.0000000000000p+1f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.730dea0000000p-1f, -0x1.dd343a0000000p+0f, 0x0.0p+0f, 0x1.dd343a0000000p+0f, 0x1.730dea0000000p-1f}, {0xbfc90fdbu, 0x0u, 0x0u}, false},
        {"embedded-scale-y0.3--2.0", {-0x1.e921de0000000p+0f, -0x0.0p+0f, -0x1.2e9cda0000000p-1f, -0x0.0p+0f, -0x1.0000000000000p+1f, -0x0.0p+0f, 0x1.2e9cda0000000p-1f, -0x0.0p+0f, -0x1.e921de0000000p+0f}, {0x0u, 0x4035dc90u, 0xc0490fdbu}, true},
        {"embedded-scale-y0.3--0.5", {-0x1.e921de0000000p-2f, -0x0.0p+0f, -0x1.2e9cda0000000p-3f, -0x0.0p+0f, -0x1.0000000000000p-1f, -0x0.0p+0f, 0x1.2e9cda0000000p-3f, -0x0.0p+0f, -0x1.e921de0000000p-2f}, {0x0u, 0x4035dc90u, 0xc0490fdbu}, true},
        {"embedded-scale-y0.3-0.5", {0x1.e921de0000000p-2f, 0x0.0p+0f, 0x1.2e9cda0000000p-3f, 0x0.0p+0f, 0x1.0000000000000p-1f, 0x0.0p+0f, -0x1.2e9cda0000000p-3f, 0x0.0p+0f, 0x1.e921de0000000p-2f}, {0x80000000u, 0xbe999a56u, 0x80000000u}, true},
        {"embedded-scale-y0.3-2.0", {0x1.e921de0000000p+0f, 0x0.0p+0f, 0x1.2e9cda0000000p-1f, 0x0.0p+0f, 0x1.0000000000000p+1f, 0x0.0p+0f, -0x1.2e9cda0000000p-1f, 0x0.0p+0f, 0x1.e921de0000000p+0f}, {0x80000000u, 0xbe999a56u, 0x80000000u}, true},
        {"embedded-scale-z-0.8--2.0", {-0x1.64b6be0000000p+0f, -0x1.6f494c0000000p+0f, -0x0.0p+0f, 0x1.6f494c0000000p+0f, -0x1.64b6be0000000p+0f, -0x0.0p+0f, -0x0.0p+0f, -0x0.0p+0f, -0x1.0000000000000p+1f}, {0x0u, 0xc0490fdbu, 0xc015dc80u}, true},
        {"embedded-scale-z-0.8--0.5", {-0x1.64b6be0000000p-2f, -0x1.6f494c0000000p-2f, -0x0.0p+0f, 0x1.6f494c0000000p-2f, -0x1.64b6be0000000p-2f, -0x0.0p+0f, -0x0.0p+0f, -0x0.0p+0f, -0x1.0000000000000p-1f}, {0x0u, 0xc0490fdbu, 0xc015dc80u}, true},
        {"embedded-scale-z-0.8-0.5", {0x1.64b6be0000000p-2f, 0x1.6f494c0000000p-2f, 0x0.0p+0f, -0x1.6f494c0000000p-2f, 0x1.64b6be0000000p-2f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p-1f}, {0x80000000u, 0x80000000u, 0x3f4ccd6au}, true},
        {"embedded-scale-z-0.8-2.0", {0x1.64b6be0000000p+0f, 0x1.6f494c0000000p+0f, 0x0.0p+0f, -0x1.6f494c0000000p+0f, 0x1.64b6be0000000p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p+1f}, {0x80000000u, 0x80000000u, 0x3f4ccd6au}, true},
        {"degenerate-zero", {0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f}, {0x80000000u, 0x80000000u, 0x80000000u}, true},
        {"shear", {0x1.0000000000000p+0f, 0x1.99999a0000000p-3f, 0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f, 0x1.3333340000000p-2f, 0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f}, {0x80000000u, 0x80000000u, 0x3e4a1febu}, true},
    }};

    TEST(ESM4DropPlacement, OriginalEquippedNodeArgumentsAndPolynomialRounding)
    {
        for (const auto& sample : cases)
        for (const auto& translation : {std::array<float, 3>{0, 0, 0}, {12.25f, -9.5f, 1001.125f}})
        {
            SCOPED_TRACE(sample.mName);
            const auto result = ESM4::droppedItemPlacement(sample.mMatrix, translation);
            for (unsigned axis = 0; axis < 3; ++axis)
            {
                EXPECT_EQ(std::bit_cast<unsigned>(result.mPosition.rot[axis]), sample.mRotationBits[axis]);
                EXPECT_EQ(std::bit_cast<unsigned>(result.mPosition.pos[axis]), std::bit_cast<unsigned>(translation[axis]));
            }
            EXPECT_EQ(result.mUniqueRotation, sample.mUnique);
        }
    }


    TEST(ESM4DropPlacement, NativeTransformPreservesEmbeddedScaleAndIgnoresSeparateScale)
    {
        for (const auto& sample : cases)
        for (float scale : {0.f, .25f, 1.f, 7.f, -2.f})
        {
            SCOPED_TRACE(sample.mName);
            Nif::NiTransform transform;
            for (unsigned row = 0; row < 3; ++row)
                for (unsigned column = 0; column < 3; ++column)
                    transform.mRotation.mValues[row][column] = sample.mMatrix[row * 3 + column];
            transform.mTranslation = {12.25f, -9.5f, 1001.125f};
            transform.mScale = scale;
            const auto result = ESM4::droppedItemPlacement(transform);
            for (unsigned axis = 0; axis < 3; ++axis)
                EXPECT_EQ(std::bit_cast<unsigned>(result.mPosition.rot[axis]), sample.mRotationBits[axis]);
            EXPECT_EQ(result.mPosition.asVec3(), transform.mTranslation);
            EXPECT_EQ(result.mUniqueRotation, sample.mUnique);
        }
    }

    TEST(ESM4DropPlacement, RejectNonfiniteInputs)
    {
        const std::array<float, 9> identity{1,0,0,0,1,0,0,0,1};
        for (unsigned index = 0; index < 9; ++index)
        for (float bad : {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
        {
            auto rotation = identity; rotation[index] = bad;
            EXPECT_THROW(ESM4::droppedItemPlacement(rotation, {}), std::invalid_argument);
        }
        for (unsigned index = 0; index < 3; ++index)
        {
            std::array<float, 3> translation{}; translation[index] = std::numeric_limits<float>::infinity();
            EXPECT_THROW(ESM4::droppedItemPlacement(identity, translation), std::invalid_argument);
        }

    }
    TEST(ESM4DropPlacement, FreshReferenceScaleMatchesOriginalDecimalRoundingAndClamps)
    {
        // Literal native i386 outputs: complete original 004DB520 with original
        // sprintf/atof; both x87 control words agree. See the editable
        // M15-ORIGINAL-DROP-SCALE-PROBES.json contract and native-02 corpus.
        const std::array<std::array<unsigned, 2>, 32> samples{{
            {0x3e000000u, 0x3e051eb8u},
            {0x3ec00000u, 0x3ec28f5cu},
            {0x3f200000u, 0x3f2147aeu},
            {0x3f600000u, 0x3f6147aeu},
            {0x3b83126fu, 0x3c23d70au},
            {0x3ba3d70au, 0x3c23d70au},
            {0x3bc49ba6u, 0x3c23d70au},
            {0x3c656042u, 0x3c23d70au},
            {0x3c75c28fu, 0x3c23d70au},
            {0x3c83126fu, 0x3ca3d70au},
            {0x3e800000u, 0x3e800000u},
            {0x3f000000u, 0x3f000000u},
            {0x3f800000u, 0x3f800000u},
            {0x40000000u, 0x40000000u},
            {0x40e00000u, 0x40e00000u},
            {0x41200000u, 0x41200000u},
            {0x41400000u, 0x41200000u},
            {0x00000000u, 0x3c23d70au},
            {0x80000000u, 0x3c23d70au},
            {0xc0000000u, 0x3c23d70au},
            {0x00000001u, 0x3c23d70au},
            {0x80000001u, 0x3c23d70au},
            {0x7f7fffffu, 0x41200000u},
            {0xff7fffffu, 0x3c23d70au},
            {0x3e000001u, 0x3e051eb8u},
            {0x3dffffffu, 0x3df5c28fu},
            {0x3f1fffffu, 0x3f1eb852u},
            {0x3f200001u, 0x3f2147aeu},
            {0x3c23d709u, 0x3c23d70au},
            {0x3c23d70bu, 0x3c23d70au},
            {0x41200001u, 0x41200000u},
            {0x411fffffu, 0x41200000u},
        }};
        EXPECT_EQ(ESM4::droppedReferenceScale(std::nullopt), 1.f);
        for (const auto& sample : samples)
        {
            SCOPED_TRACE(sample[0]);
            const auto result = ESM4::droppedReferenceScale(std::bit_cast<float>(sample[0]));
            EXPECT_EQ(std::bit_cast<unsigned>(result), sample[1]);
            EXPECT_EQ(ESM4::droppedReferenceScale(result), result);
        }
        for (const float invalid : {std::numeric_limits<float>::infinity(),
                -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
            EXPECT_THROW(ESM4::droppedReferenceScale(invalid), std::invalid_argument);
    }

    TEST(ESM4DropPlacement, PhysicalReferencePoseUsesOriginalFrameExtractor)
    {
        struct Case
        {
            const char* mName;
            std::array<unsigned,9> mMatrix;
            std::array<unsigned,3> mAngles;
            bool mUnique;
        };
        // Full original 4E2160 callback / 711300, both x87 words agree.
        // Unlike fresh drop arguments, the frame path has a Y pole.
        constexpr Case samples[]{
            {"identity", {0x3f800000u,0x0u,0x0u,0x0u,0x3f800000u,0x0u,0x0u,0x0u,0x3f800000u}, {0x80000000u,0x80000000u,0x80000000u}, true},
            {"x+90", {0x3f800000u,0x0u,0x0u,0x0u,0x0u,0xbf800000u,0x0u,0x3f800000u,0x0u}, {0xbfc90fdbu,0x80000000u,0x80000000u}, true},
            {"x-90", {0x3f800000u,0x0u,0x0u,0x0u,0x0u,0x3f800000u,0x0u,0xbf800000u,0x0u}, {0x3fc90fdbu,0x80000000u,0x80000000u}, true},
            {"y+90", {0x0u,0x0u,0x3f800000u,0x0u,0x3f800000u,0x0u,0xbf800000u,0x0u,0x0u}, {0x0u,0xbfc90fdbu,0x0u}, false},
            {"y-90", {0x0u,0x0u,0xbf800000u,0x0u,0x3f800000u,0x0u,0x3f800000u,0x0u,0x0u}, {0x0u,0x3fc90fdbu,0x0u}, false},
            {"z+90", {0x0u,0xbf800000u,0x0u,0x3f800000u,0x0u,0x0u,0x0u,0x0u,0x3f800000u}, {0x80000000u,0x80000000u,0xbfc90fdbu}, true},
            {"z-90", {0x0u,0x3f800000u,0x0u,0xbf800000u,0x0u,0x0u,0x0u,0x0u,0x3f800000u}, {0x80000000u,0x80000000u,0x3fc90fdbu}, true},
            {"x0.3", {0x3f800000u,0x0u,0x0u,0x0u,0x3f7490efu,0xbe974e6du,0x0u,0x3e974e6du,0x3f7490efu}, {0xbe999a56u,0x80000000u,0x80000000u}, true},
            {"y0.3", {0x3f7490efu,0x0u,0x3e974e6du,0x0u,0x3f800000u,0x0u,0xbe974e6du,0x0u,0x3f7490efu}, {0x80000000u,0xbe99999au,0x80000000u}, true},
            {"z-0.8", {0x3f325b5fu,0x3f37a4a6u,0x0u,0xbf37a4a6u,0x3f325b5fu,0x0u,0x0u,0x0u,0x3f800000u}, {0x80000000u,0x80000000u,0x3f4ccd6au}, true},
            {"random-0", {0x3eb9fa41u,0xbf24cfd0u,0xbf2c69acu,0xbefc261fu,0xbf3f0d35u,0x3ee543a4u,0xbf4a7875u,0x3e2d1563u,0xbf169035u}, {0x401f6a40u,0x3f3d29a3u,0xbf874f80u}, true},
            {"random-127", {0x3ecd60feu,0x3ef6f488u,0x3f475c00u,0x3e8e7250u,0xbf5fc72cu,0x3ecbd470u,0x3f5f6c74u,0x3d69570eu,0xbef83c3bu}, {0x401d0fedu,0xbf6485d1u,0x3f6085fau}, true},
            {"frame-pole-0x3f7ffffe--1", {0x39ffffffu,0x0u,0xbf7ffffeu,0x0u,0x3f800000u,0x0u,0x3f7ffffeu,0x0u,0x39ffffffu}, {0x80000000u,0x3fc8ffdbu,0x80000000u}, true},
            {"frame-pole-0x3f7ffffe-1", {0x39ffffffu,0x0u,0x3f7ffffeu,0x0u,0x3f800000u,0x0u,0xbf7ffffeu,0x0u,0x39ffffffu}, {0x80000000u,0xbfc8ffdbu,0x80000000u}, true},
            {"frame-pole-0x3f7fffff--1", {0x39b504f3u,0x0u,0xbf7fffffu,0x0u,0x3f800000u,0x0u,0x3f7fffffu,0x0u,0x39b504f3u}, {0x80000000u,0x3fc9048au,0x80000000u}, true},
            {"frame-pole-0x3f7fffff-1", {0x39b504f3u,0x0u,0x3f7fffffu,0x0u,0x3f800000u,0x0u,0xbf7fffffu,0x0u,0x39b504f3u}, {0x80000000u,0xbfc9048au,0x80000000u}, true},
            {"frame-pole-0x3f800000--1", {0x0u,0x0u,0xbf800000u,0x0u,0x3f800000u,0x0u,0x3f800000u,0x0u,0x0u}, {0x0u,0x3fc90fdbu,0x0u}, false},
            {"frame-pole-0x3f800000-1", {0x0u,0x0u,0x3f800000u,0x0u,0x3f800000u,0x0u,0xbf800000u,0x0u,0x0u}, {0x0u,0xbfc90fdbu,0x0u}, false},
            {"frame-pole-0x3f800001--1", {0x0u,0x0u,0xbf800001u,0x0u,0x3f800000u,0x0u,0x3f800001u,0x0u,0x0u}, {0x0u,0x3fc90fdbu,0x0u}, false},
            {"frame-pole-0x3f800001-1", {0x0u,0x0u,0x3f800001u,0x0u,0x3f800000u,0x0u,0xbf800001u,0x0u,0x0u}, {0x0u,0xbfc90fdbu,0x0u}, false},
        };
        for (const auto& sample : samples)
        {
            SCOPED_TRACE(sample.mName);
            std::array<float,9> matrix;
            for (unsigned i=0;i<9;++i) matrix[i]=std::bit_cast<float>(sample.mMatrix[i]);
            const std::array<float,3> position{12.25f,-0.f,1001.125f};
            const auto result=ESM4::physicalDroppedItemPlacement(matrix,position);
            for (unsigned i=0;i<3;++i)
            {
                EXPECT_EQ(std::bit_cast<unsigned>(result.mPosition.rot[i]),sample.mAngles[i]);
                EXPECT_EQ(std::bit_cast<unsigned>(result.mPosition.pos[i]),std::bit_cast<unsigned>(position[i]));
            }
            EXPECT_EQ(result.mUniqueRotation,sample.mUnique);
        }
    }

    TEST(ESM4DropPlacement, PhysicalReferencePoseRejectsNonfiniteInputs)
    {
        constexpr std::array<float,9> identity{1,0,0,0,1,0,0,0,1};
        for (float invalid : {std::numeric_limits<float>::infinity(),
                -std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()})
        {
            for (unsigned i=0;i<9;++i)
            {
                auto matrix=identity; matrix[i]=invalid;
                EXPECT_THROW(ESM4::physicalDroppedItemPlacement(matrix,{}),std::invalid_argument);
            }
            for (unsigned i=0;i<3;++i)
            {
                std::array<float,3> position{}; position[i]=invalid;
                EXPECT_THROW(ESM4::physicalDroppedItemPlacement(identity,position),std::invalid_argument);
            }
        }
    }

    TEST(ESM4DropPlacement, ScriptScaleMatchesOriginalCommandLimitsAndDecimalStores)
    {
        // Original5093B0 ->4DB520, live-scale-command-oracle-04. Both x87
        // words and ordinary/bodyT fixtures agree on these stored results.
        const std::array<std::array<unsigned, 2>, 32> commandCases{{
            {0x3c23d70au, 0x3f000000u},
            {0x3e000000u, 0x3f000000u},
            {0x3e051eb8u, 0x3f000000u},
            {0x3ec00000u, 0x3f000000u},
            {0x3eff7ceeu, 0x3f000000u},
            {0x3f000000u, 0x3f000000u},
            {0x3f004189u, 0x3f000000u},
            {0x3f0147aeu, 0x3f000000u},
            {0x3f028f5cu, 0x3f028f5cu},
            {0x3f200000u, 0x3f2147aeu},
            {0x3f600000u, 0x3f6147aeu},
            {0x3f800000u, 0x3f800000u},
            {0x3f900000u, 0x3f90a3d7u},
            {0x3fb00000u, 0x3fb0a3d7u},
            {0x3fff5c29u, 0x40000000u},
            {0x40000000u, 0x40000000u},
            {0x40e00000u, 0x40000000u},
            {0x41200000u, 0x40000000u},
            {0x00000000u, 0x3f000000u},
            {0x80000000u, 0x3f000000u},
            {0xc0000000u, 0x3f000000u},
            {0x41400000u, 0x40000000u},
            {0x3effffffu, 0x3f000000u},
            {0x3f000001u, 0x3f000000u},
            {0x3f1fffffu, 0x3f1eb852u},
            {0x3f200001u, 0x3f2147aeu},
            {0x3fffffffu, 0x40000000u},
            {0x40000001u, 0x40000000u},
            {0x00000001u, 0x3f000000u},
            {0x80000001u, 0x3f000000u},
            {0x7f7fffffu, 0x40000000u},
            {0xff7fffffu, 0x3f000000u},
        }};
        for (const auto& sample : commandCases)
        {
            const auto result = ESM4::scriptReferenceScale(std::bit_cast<float>(sample[0]));
            EXPECT_EQ(std::bit_cast<unsigned>(result), sample[1]);
        }
        // Source ExtraScale takes the separate wider reference-setter path.
        EXPECT_EQ(ESM4::droppedReferenceScale(.125f), .13f);
        EXPECT_EQ(ESM4::scriptReferenceScale(.125f), .5f);
        EXPECT_EQ(ESM4::droppedReferenceScale(7.f), 7.f);
        EXPECT_EQ(ESM4::scriptReferenceScale(7.f), 2.f);
    }

    TEST(ESM4DropPlacement, ScriptScaleRejectsNonfiniteInputs)
    {
        for (float invalid : {std::numeric_limits<float>::infinity(),
                -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
            EXPECT_THROW(ESM4::scriptReferenceScale(invalid), std::invalid_argument);
    }

}

TEST(ESM4DropPlacement, DefaultDropUsesNativeEulerOffsetAndPreservesActorRotation)
{
    // Original48B080/711580/7101F0, independent CW007F/027F observations.
    struct Golden
    {
        unsigned mControl;
        std::array<std::uint32_t, 3> mAngles, mPosition, mExpected;
    };
    const Golden goldens[]{
        {127, {0x0u, 0x0u, 0x0u}, {0x42c80000u, 0x41a00000u, 0x41f00000u}, {0x42c80000u, 0x428c0000u, 0x42700000u}},
        {639, {0x0u, 0x0u, 0x0u}, {0x42c80000u, 0x41a00000u, 0x41f00000u}, {0x42c80000u, 0x428c0000u, 0x42700000u}},
        {127, {0x0u, 0x0u, 0x3fc90fdbu}, {0x42c80000u, 0x41a00000u, 0x41f00000u}, {0x43160000u, 0x419fffffu, 0x42700000u}},
        {639, {0x0u, 0x0u, 0x3fc90fdbu}, {0x42c80000u, 0x41a00000u, 0x41f00000u}, {0x43160000u, 0x419fffffu, 0x42700000u}},
        {127, {0x3fc90fdbu, 0x0u, 0x0u}, {0x42c80000u, 0x41a00000u, 0x41f00000u}, {0x42c80000u, 0x42480000u, 0xc1a00000u}},
        {639, {0x3fc90fdbu, 0x0u, 0x0u}, {0x42c80000u, 0x41a00000u, 0x41f00000u}, {0x42c80000u, 0x42480000u, 0xc1a00000u}},
        {127, {0x0u, 0x3fc90fdbu, 0x0u}, {0x42c80000u, 0x41a00000u, 0x41f00000u}, {0x428c0000u, 0x428c0000u, 0x41efffffu}},
        {639, {0x0u, 0x3fc90fdbu, 0x0u}, {0x42c80000u, 0x41a00000u, 0x41f00000u}, {0x428c0000u, 0x428c0000u, 0x41efffffu}},
        {127, {0x3e800000u, 0xbe800000u, 0x3fc90fdbu}, {0x42c80000u, 0x41a00000u, 0x41f00000u}, {0x431bde24u, 0x41c10c2du, 0x4238b660u}},
        {639, {0x3e800000u, 0xbe800000u, 0x3fc90fdbu}, {0x42c80000u, 0x41a00000u, 0x41f00000u}, {0x431bde24u, 0x41c10c2du, 0x4238b660u}},
        {127, {0xbe800000u, 0x3e800000u, 0xbe800000u}, {0x42c80000u, 0x41a00000u, 0x41f00000u}, {0x42a12f3au, 0x42720579u, 0x42865e40u}},
        {639, {0xbe800000u, 0x3e800000u, 0xbe800000u}, {0x42c80000u, 0x41a00000u, 0x41f00000u}, {0x42a12f3au, 0x42720579u, 0x42865e40u}},
        {127, {0xc0490fdbu, 0xc0490fdbu, 0xbfc90fdbu}, {0x0u, 0x0u, 0x0u}, {0x4247ffffu, 0xb4eaac88u, 0x41f00002u}},
        {639, {0xc0490fdbu, 0xc0490fdbu, 0xbfc90fdbu}, {0x0u, 0x0u, 0x0u}, {0x4247ffffu, 0xb4eaac86u, 0x41f00002u}},
        {127, {0xc0490fdbu, 0xc0490fdbu, 0xbfc90fdbu}, {0x80000000u, 0x1u, 0x80000001u}, {0x4247ffffu, 0xb4eaac88u, 0x41f00002u}},
        {639, {0xc0490fdbu, 0xc0490fdbu, 0xbfc90fdbu}, {0x80000000u, 0x1u, 0x80000001u}, {0x4247ffffu, 0xb4eaac86u, 0x41f00002u}},
        {127, {0xc0490fdbu, 0xc0490fdbu, 0x3fc90fdbu}, {0x0u, 0x0u, 0x0u}, {0xc2480001u, 0xb4eaac68u, 0x41effffeu}},
        {639, {0xc0490fdbu, 0xc0490fdbu, 0x3fc90fdbu}, {0x0u, 0x0u, 0x0u}, {0xc2480001u, 0xb4eaac6du, 0x41effffeu}},
    };
    for (const auto& golden : goldens)
    {
        ESM::Position actor{};
        for (unsigned i = 0; i < 3; ++i)
        {
            actor.rot[i] = std::bit_cast<float>(golden.mAngles[i]);
            actor.pos[i] = std::bit_cast<float>(golden.mPosition[i]);
        }
        const auto result = ESM4::defaultDroppedItemPlacement(actor, golden.mControl == 127
            ? Nif::NativeTransformPrecision::Single : Nif::NativeTransformPrecision::Double);
        ASSERT_TRUE(result);
        for (unsigned i = 0; i < 3; ++i)
        {
            EXPECT_EQ(std::bit_cast<std::uint32_t>(result->pos[i]), golden.mExpected[i]);
            EXPECT_EQ(std::bit_cast<std::uint32_t>(result->rot[i]), golden.mAngles[i]);
        }
    }
}

TEST(ESM4DropPlacement, DefaultDropRejectsNonfiniteInputsAndUnsupportedTrigonometry)
{
    for (unsigned axis = 0; axis < 3; ++axis)
    {
        ESM::Position actor{};
        actor.pos[axis] = std::numeric_limits<float>::infinity();
        EXPECT_FALSE(ESM4::defaultDroppedItemPlacement(actor));
        actor = {}; actor.rot[axis] = std::numeric_limits<float>::quiet_NaN();
        EXPECT_FALSE(ESM4::defaultDroppedItemPlacement(actor));
        actor.rot[axis] = 0x1p63f;
        EXPECT_FALSE(ESM4::defaultDroppedItemPlacement(actor));
    }
    EXPECT_FALSE(ESM4::defaultDroppedItemPlacement({}, static_cast<Nif::NativeTransformPrecision>(255)));
}
