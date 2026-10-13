#ifndef OPENMW_MWWORLD_PREPAREDSAVERECORDS_H
#define OPENMW_MWWORLD_PREPAREDSAVERECORDS_H

#include <cstddef>
#include <cstdint>
#include <memory>

namespace ESM
{
    class ESMReader;
    class CellRef;
    struct ObjectState;
    struct Player;
}

namespace MWWorld
{
    // Own admission's decoded objects for the same immutable save reader.
    // Consumption advances over framing only, then rebinds legacy converters.
    class PreparedSaveRecords
    {
        struct Impl;
        std::unique_ptr<Impl> mImpl;

    public:
        explicit PreparedSaveRecords(ESM::ESMReader& reader);
        ~PreparedSaveRecords();
        void addReference(ESM::ESMReader& reader, std::size_t begin, std::uint32_t type,
            std::unique_ptr<ESM::ObjectState> state);
        void addPlayer(ESM::ESMReader& reader, std::size_t begin, std::unique_ptr<ESM::Player> state);
        std::unique_ptr<ESM::ObjectState> takeReference(
            ESM::ESMReader& reader, const ESM::CellRef& reference, std::uint32_t type);
        std::unique_ptr<ESM::Player> takePlayer(ESM::ESMReader& reader);
        std::size_t size() const;
    };
}

#endif
