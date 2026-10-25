#ifndef OPENMW_ESM4_COMBATMAGICPROFILE_H
#define OPENMW_ESM4_COMBATMAGICPROFILE_H

namespace ESM4
{
    struct Spell;
    struct EffectSetting;

    // Narrow supported profile for compiled default FormID000137 self-PARA.
    // A null spell selects the original fallback constructor. The caller must
    // supply the winning PARA definition; missing/ambiguous data is unsupported.
    // Authored quantity fields ignored by NoMagnitude/static AV48 stay ignored.
    bool supportsDefaultCombatParalysisProfile(const Spell* spell, const EffectSetting* paralysis);
}

#endif
