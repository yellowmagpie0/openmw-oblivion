#ifndef OPENMW_ESM4_ALCHEMY_H
#define OPENMW_ESM4_ALCHEMY_H

namespace ESM4
{
    // Original-game tutorial-kit observation; not a general potency formula.
    // Higher skills and additional apparatus need their own native profiles.
    struct NoviceAlchemyPoison
    {
        static constexpr int Magnitude = 1;
        static constexpr int Duration = 5;
        static constexpr int Value = 3;

        static bool supports(float skill, float intelligence, float luck, float mortarQuality)
        {
            return skill == 5.f && intelligence == 40.f && luck == 50.f && mortarQuality == 10.f;
        }
    };
}
#endif
