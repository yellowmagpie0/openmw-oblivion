#ifndef OPENMW_COMPONENTS_ESM4_CHARACTERGEN_HPP
#define OPENMW_COMPONENTS_ESM4_CHARACTERGEN_HPP

#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace ESM4
{
    struct RecommendedClassDefault
    {
        std::string_view mSetting;
        std::uint32_t mFormId;
    };

    inline constexpr RecommendedClassDefault DefaultCharacterGenerationClass{"iClassAcrobat", 145320};

    // Original662750 chooses a class setting from the proportions of tutorial
    // skill use in combat/magic/stealth. These are compiled fallback FormIDs;
    // the runtime must resolve winning GMSTs and actual CLAS definitions.
    // A null proposal preserves the previous cached recommendation. A concrete
    // proposal replaces it even when subsequent content lookup fails. Only an
    // empty resulting cache tries DefaultCharacterGenerationClass.
    // This pure policy does not own skill history or publish a recommendation.
    inline std::optional<RecommendedClassDefault> proposeCharacterClass(const std::array<float, 3>& uses)
    {
        for (const float value : uses)
            if (!std::isfinite(value) || value < 0)
                throw std::invalid_argument("invalid native character-generation skill use");
        const float total = (uses[0] + uses[1]) + uses[2];
        if (!std::isfinite(total))
            throw std::invalid_argument("native character-generation skill use overflow");
        constexpr auto acrobat = DefaultCharacterGenerationClass;
        // No proposal retains the caller's cached class. Only an empty cache
        // uses Acrobat. Avoid native zero-use NaNs/conversion sentinels.
        if (total == 0)
            return std::nullopt;
        std::array<int, 3> shares{};
        for (std::size_t i = 0; i < shares.size(); ++i)
        {
            const float scaled = static_cast<float>(double(uses[i]) / double(total) * 7.0);
            shares[i] = static_cast<int>(std::floor(double(scaled) + .5));
        }
        const auto [combat, magic, stealth] = shares;
        if (combat >= 7) return RecommendedClassDefault{"iClassWarrior", 145287};
        if (magic >= 7) return RecommendedClassDefault{"iClassMage", 67030};
        if (stealth >= 7) return RecommendedClassDefault{"iClassThief", 145295};
        std::optional<RecommendedClassDefault> result;
        // Keep original branch order. Independent rounding can produce eight
        // points (e.g.4/4/0); a later specialization can replace an earlier pick.
        switch (combat)
        {
            case 6: result = RecommendedClassDefault{"iClassBarbarian", 145288}; break;
            case 5: result = magic == 2 ? RecommendedClassDefault{"iClassCrusader", 145289}
                                      : RecommendedClassDefault{"iClassKnight", 2102}; break;
            case 4: result = stealth == 2 ? RecommendedClassDefault{"iClassScout", 145290}
                                         : RecommendedClassDefault{"iClassArcher", 145291}; break;
            case 3: if (magic == 2 && stealth == 2) result = RecommendedClassDefault{"iClassRogue", 145292}; break;
        }
        switch (magic)
        {
            case 6: result = RecommendedClassDefault{"iClassSorcerer", 145324}; break;
            case 5: result = stealth == 2 ? RecommendedClassDefault{"iClassHealer", 2320}
                                         : RecommendedClassDefault{"iClassBattlemage", 145815}; break;
            case 4: result = stealth == 2 ? RecommendedClassDefault{"iClassWitchhunter", 145816}
                                         : RecommendedClassDefault{"iClassSpellsword", 146439}; break;
            case 3: if (combat == 2 && stealth == 2) result = RecommendedClassDefault{"iClassNightblade", 146440}; break;
        }
        switch (stealth)
        {
            case 6: result = RecommendedClassDefault{"iClassAgent", 145298}; break;
            case 5: result = combat == 1 ? RecommendedClassDefault{"iClassAssassin", 145299} : acrobat; break;
            case 4: result = combat == 2 ? RecommendedClassDefault{"iClassMonk", 145321}
                                        : RecommendedClassDefault{"iClassPilgrim", 2098}; break;
            case 3: if (combat <= 3 && magic <= 3) result = RecommendedClassDefault{"iClassBard", 116772}; break;
        }
        return result;
    }

    // Original4F5880 compares actual class identity first, then specialization.
    // The caller owns/resolves the cached recommendation; absence means no
    // recommendation, never an inferred match with the current class.
    inline unsigned characterClassDefaultMatch(bool sameClass,
        std::optional<std::uint32_t> recommendedSpecialization, std::uint32_t currentSpecialization)
    {
        if (currentSpecialization > 2 || (recommendedSpecialization && *recommendedSpecialization > 2)
            || (sameClass && !recommendedSpecialization))
            throw std::invalid_argument("invalid native class match input");
        if (sameClass) return 2;
        return recommendedSpecialization && *recommendedSpecialization == currentSpecialization ? 1 : 0;
    }
}
#endif
