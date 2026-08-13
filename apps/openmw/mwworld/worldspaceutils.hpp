#ifndef OPENMW_MWWORLD_WORLDSPACEUTILS_H
#define OPENMW_MWWORLD_WORLDSPACEUTILS_H

#include <cstdint>

#include <components/esm/refid.hpp>

namespace ESM4
{
    struct World;
}

namespace MWWorld
{
    template <class T>
    class Store;

    /// Resolve the worldspace which supplies an inherited WRLD feature.
    ///
    /// TES4 permits child worldspaces to inherit individual features from a
    /// parent. Parents may themselves be children, so consumers must follow
    /// the complete chain rather than applying a single PNAM hop.
    ESM::RefId resolveWorldspaceInheritance(
        const Store<ESM4::World>& worlds, ESM::RefId worldspace, std::uint16_t useFlag);

    /// Find a world record after applying feature inheritance.
    const ESM4::World* getWorldspaceFeature(
        const Store<ESM4::World>& worlds, ESM::RefId worldspace, std::uint16_t useFlag);
}

#endif
