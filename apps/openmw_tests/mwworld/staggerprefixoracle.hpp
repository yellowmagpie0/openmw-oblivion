#ifndef OPENMW_TESTS_STAGGERPREFIXORACLE_H
#define OPENMW_TESTS_STAGGERPREFIXORACLE_H

#include <array>
#include <cstdint>

// Fixed observations from the original executable, never production output.
// Source/scope: M15-STAGGER-NATIVE-DISPATCH-VERIFICATION.json, phase_prefix02.
// Group20 ordinary,22 power,28 BlockHit,30 Recoil; initial phase1/clock.5.
namespace StaggerPrefixOracle
{
    struct Sample
    {
        int mPhase;
        unsigned mState;
        std::uint32_t mOffset, mOutput;
    };
    struct Case
    {
        unsigned mGroup;
        float mSpeed;
        std::array<Sample, 8> mSamples;
    };
    constexpr std::array<Case, 7> sCases{{
        {20, 0.7f, {{
            {1, 1, 0xbcf5c290u, 0x3f11eb86u},
            {2, 1, 0xbd75c290u, 0x3f23d70bu},
            {2, 1, 0xbdb851ecu, 0x3f35c290u},
            {2, 1, 0xbdf5c290u, 0x3f47ae16u},
            {2, 1, 0xbe19999au, 0x3f59999cu},
            {2, 1, 0xbe3851ecu, 0x3f6b8521u},
            {2, 1, 0xbe570a3eu, 0x3f7d70a6u},
            {-1, 3, 0xbe75c290u, 0x3f800000u},
        }}},
        {20, 1.0f, {{
            {1, 1, 0x00000000u, 0x3f19999au},
            {2, 1, 0x00000000u, 0x3f333334u},
            {2, 1, 0x00000000u, 0x3f4cccceu},
            {2, 1, 0x00000000u, 0x3f666668u},
            {-1, 3, 0x00000000u, 0x3f800000u},
            {-1, 0, 0x00000000u, 0x3f800000u},
            {-1, 0, 0x00000000u, 0x3f800000u},
            {-1, 0, 0x00000000u, 0x3f800000u},
        }}},
        {20, 2.0f, {{
            {2, 1, 0x3dcccccdu, 0x3f333334u},
            {2, 1, 0x3e4ccccdu, 0x3f666667u},
            {-1, 3, 0x3e99999au, 0x3f800000u},
            {-1, 0, 0x3e99999au, 0x3f800000u},
            {-1, 0, 0x3e99999au, 0x3f800000u},
            {-1, 0, 0x3e99999au, 0x3f800000u},
            {-1, 0, 0x3e99999au, 0x3f800000u},
            {-1, 0, 0x3e99999au, 0x3f800000u},
        }}},
        {20, 20.0f, {{
            {-1, 3, 0x3ff33333u, 0x3f800000u},
            {-1, 0, 0x3ff33333u, 0x3f800000u},
            {-1, 0, 0x3ff33333u, 0x3f800000u},
            {-1, 0, 0x3ff33333u, 0x3f800000u},
            {-1, 0, 0x3ff33333u, 0x3f800000u},
            {-1, 0, 0x3ff33333u, 0x3f800000u},
            {-1, 0, 0x3ff33333u, 0x3f800000u},
            {-1, 0, 0x3ff33333u, 0x3f800000u},
        }}},
        {22, 2.0f, {{
            {1, 1, 0x3dcccccdu, 0x3f333334u},
            {1, 1, 0x3e4ccccdu, 0x3f666667u},
            {-1, 3, 0x3e99999au, 0x3f800000u},
            {-1, 0, 0x3e99999au, 0x3f800000u},
            {-1, 0, 0x3e99999au, 0x3f800000u},
            {-1, 0, 0x3e99999au, 0x3f800000u},
            {-1, 0, 0x3e99999au, 0x3f800000u},
            {-1, 0, 0x3e99999au, 0x3f800000u},
        }}},
        {28, 2.0f, {{
            {1, 1, 0x80000000u, 0x3f19999au},
            {1, 1, 0x80000000u, 0x3f333334u},
            {1, 1, 0x80000000u, 0x3f4cccceu},
            {1, 1, 0x80000000u, 0x3f666668u},
            {-1, 3, 0x80000000u, 0x3f800000u},
            {-1, 0, 0x00000000u, 0x3f800000u},
            {-1, 0, 0x00000000u, 0x3f800000u},
            {-1, 0, 0x00000000u, 0x3f800000u},
        }}},
        {30, 2.0f, {{
            {1, 1, 0x80000000u, 0x3f19999au},
            {1, 1, 0x80000000u, 0x3f333334u},
            {1, 1, 0x80000000u, 0x3f4cccceu},
            {1, 1, 0x80000000u, 0x3f666668u},
            {-1, 3, 0x80000000u, 0x3f800000u},
            {-1, 0, 0x00000000u, 0x3f800000u},
            {-1, 0, 0x00000000u, 0x3f800000u},
            {-1, 0, 0x00000000u, 0x3f800000u},
        }}},
    }};
}

#endif
