#include <components/esm4/referenceflags.hpp>
#include <gtest/gtest.h>
#include <array>
#include <limits>

TEST(ESM4ReferenceFlags, FriendlyHitCommandPreservesOtherFlagsAndHasAnExplicitNoOp)
{
    for (const std::uint32_t flags : {0u, 1u, 0x100000u, 0x80000000u, 0xffefffffu, 0xffffffffu})
        for (const std::int32_t value : {std::numeric_limits<std::int32_t>::min(), -2, -1, 0, 1, 2,
                 std::numeric_limits<std::int32_t>::max()})
        {
            const auto result = ESM4::ignoreFriendlyHitsFlags(flags, value);
            if (value == -1)
                EXPECT_FALSE(result);
            else
            {
                ASSERT_TRUE(result);
                EXPECT_EQ(*result & ~0x100000u, flags & ~0x100000u);
                EXPECT_EQ(ESM4::ignoresFriendlyHits(*result), value != 0);
            }
        }
}
