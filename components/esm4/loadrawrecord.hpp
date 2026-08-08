#ifndef OPENMW_COMPONENTS_ESM4_LOADRAWRECORD_H
#define OPENMW_COMPONENTS_ESM4_LOADRAWRECORD_H

#include <cstdint>
#include <string>
#include <vector>

#include <components/esm/formid.hpp>
#include <components/esm/formkey.hpp>

namespace ESM4
{
    class Reader;

    struct RawSubRecord
    {
        std::uint32_t mType = 0;
        bool mExtended = false;
        std::vector<std::uint8_t> mData;
    };

    // Lossless fallback for official record families that do not yet have a
    // semantic runtime type. It keeps them visible to audits and preserves all
    // logical subrecord payloads, including XXXX-extended payloads.
    struct RawRecord
    {
        ESM::FormId mId;
        ESM::FormKey mFormKey;
        std::uint32_t mFlags = 0;
        std::uint32_t mRecordType = 0;
        std::string mEditorId;
        std::string mFullName;
        std::vector<RawSubRecord> mSubRecords;

        void load(Reader& reader);
    };
}

#endif
