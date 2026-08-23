#include <gtest/gtest.h>

#include "../../openmw/mwrender/tes4facegen.hpp"

namespace MWRender
{
    TEST(Tes4FaceGen, SelectsExactSpeechChannels)
    {
        EXPECT_FLOAT_EQ(getTes4FaceMorphWeight("i", "i", "r", 0.25f, 0.8f, 0.f), 0.6f);
        EXPECT_FLOAT_EQ(getTes4FaceMorphWeight("r", "i", "r", 0.25f, 0.8f, 0.f), 0.2f);
        EXPECT_FLOAT_EQ(getTes4FaceMorphWeight("dispanger1", "i", "r", 0.25f, 0.8f, 0.f), 0.f);
        EXPECT_FLOAT_EQ(getTes4FaceMorphWeight("surprise", "i", "r", 0.25f, 0.8f, 0.f), 0.f);
        EXPECT_FLOAT_EQ(getTes4FaceMorphWeight("browinright", "i", "r", 0.25f, 0.8f, 0.f), 0.f);
    }

    TEST(Tes4FaceGen, KeepsBlinkIndependentFromSpeech)
    {
        EXPECT_FLOAT_EQ(getTes4FaceMorphWeight("blink", "aah", "bigaah", 0.5f, 1.f, 0.75f), 0.75f);
        EXPECT_FLOAT_EQ(
            getTes4FaceMorphWeight("eyes closed left", "aah", "bigaah", 0.5f, 1.f, 0.5f), 0.5f);
        EXPECT_FLOAT_EQ(getTes4FaceMorphWeight("happy", "aah", "bigaah", 0.5f, 0.f, 0.5f), 0.f);
    }
}
