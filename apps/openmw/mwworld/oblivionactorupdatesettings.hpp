#ifndef OPENMW_MWWORLD_OBLIVIONACTORUPDATESETTINGS_H
#define OPENMW_MWWORLD_OBLIVIONACTORUPDATESETTINGS_H

#include "../mwmechanics/oblivioncombat.hpp"

#include <optional>

namespace MWWorld
{
    struct OblivionArmorWeightSettings
    {
        ESM4::CombatMasterySettings mMastery;
        ESM4::ArmorMasterySettings mArmor;
    };

    OblivionArmorWeightSettings resolveOblivionArmorWeightSettings(const ESMStore& store);

    // Winning GMSTs are read-only during the synchronous actor pass. Discard
    // the derived settings at its end, including on an exception or early exit.
    class ScopedOblivionActorSettings
    {
        const ESMStore& mStore;
        ScopedOblivionActorSettings* mPrevious;
        static thread_local ScopedOblivionActorSettings* sActive;
        std::optional<MWMechanics::OblivionFrameSettings> mFrame;
        std::optional<OblivionArmorWeightSettings> mArmorWeight;
        std::optional<ESM4::EssentialRecoverySettings> mRecovery;

        template <class T, class Builder>
        static T resolve(const ESMStore& store, std::optional<T> ScopedOblivionActorSettings::* member, Builder build);
        friend MWMechanics::OblivionFrameSettings resolveOblivionFrameSettings(const ESMStore&);
        friend OblivionArmorWeightSettings resolveOblivionArmorWeightSettings(const ESMStore&);
        friend ESM4::EssentialRecoverySettings resolveOblivionEssentialRecoverySettings(const ESMStore&);

    public:
        explicit ScopedOblivionActorSettings(const ESMStore& store);
        ~ScopedOblivionActorSettings();
        ScopedOblivionActorSettings(const ScopedOblivionActorSettings&) = delete;
        ScopedOblivionActorSettings& operator=(const ScopedOblivionActorSettings&) = delete;
    };
}

#endif
