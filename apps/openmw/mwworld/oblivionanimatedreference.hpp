#ifndef OPENMW_MWWORLD_OBLIVIONANIMATEDREFERENCE_H
#define OPENMW_MWWORLD_OBLIVIONANIMATEDREFERENCE_H

namespace Nif { class FileView; }
namespace MWWorld
{
    // Positive-mass, keyframed multi-body scenery eligible for release after
    // its non-looping Unequip animation. Mass-zero doors are excluded.
    bool isOblivionAnimatedReferenceAsset(Nif::FileView file);
}
#endif
