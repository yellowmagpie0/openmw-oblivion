#ifndef OPENMW_MWRENDER_OBLIVIONMELEEPROJECTION_H
#define OPENMW_MWRENDER_OBLIVIONMELEEPROJECTION_H

#include "animation.hpp"

namespace ESM4 { struct RuntimePassiveMeleeSequence; }

namespace MWRender
{
    // Project a resident slot3 without playing an event track or assigning
    // process/contact ownership. The pose binding remains stable across frames.
    std::vector<Animation::NativeSequenceLayer> oblivionPassiveMeleeLayers(Animation& animation,
        const ESM4::RuntimePassiveMeleeSequence* passive, float clock,
        std::shared_ptr<const ESM4::RuntimeMeleePoseSnapshot>& poseBinding);
}

#endif
