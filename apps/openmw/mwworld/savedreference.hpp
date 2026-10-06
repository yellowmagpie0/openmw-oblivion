#ifndef OPENMW_MWWORLD_SAVEDREFERENCE_H
#define OPENMW_MWWORLD_SAVEDREFERENCE_H

#include <memory>
#include <cstdint>

namespace ESM
{
    class ESMReader;
    struct CellRef;
    struct ObjectState;
    class RefId;
}
namespace ESM4 { struct Reference; }

namespace MWWorld
{
    class ESMStore;
    class CellStore;
    class Ptr;

    class PreparedSavedNativeReference
    {
    public:
        virtual ~PreparedSavedNativeReference() = default;
        virtual Ptr get() const = 0;
        virtual bool isValid() const noexcept = 0;
        virtual void commit() noexcept = 0;
    };

    // Only winning native placeable bases can reconstruct a World-owned key.
    std::uint32_t savedNativeReferenceType(const ESMStore& store, const ESM::RefId& base);
    std::unique_ptr<PreparedSavedNativeReference> prepareSavedNativeReference(
        const ESMStore& store, CellStore& cell, const ESM4::Reference& reference);

    // Decode with CellStore's record/state mapping into detached data. Does
    // not load cells, construct live classes, register references or publish.
    std::unique_ptr<ESM::ObjectState> readSavedReferenceState(
        ESM::ESMReader& reader, const ESM::CellRef& reference, std::uint32_t type);
}

#endif
