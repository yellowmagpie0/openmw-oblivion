#ifndef OPENMW_MWWORLD_OBLIVIONANIMATEDREFERENCE_H
#define OPENMW_MWWORLD_OBLIVIONANIMATEDREFERENCE_H

namespace Nif { class FileView; }
namespace MWWorld
{
    // Scenery with multiple positive-mass keyframed bodies, optionally anchored
    // by mass-zero fixed bodies. Ordinary mass-zero doors remain excluded.
    bool isOblivionAnimatedReferenceAsset(Nif::FileView file);
}
#endif
