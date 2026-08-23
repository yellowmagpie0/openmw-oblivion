#ifndef OPENMW_ESM4_BSGN_H
#define OPENMW_ESM4_BSGN_H

#include <cstdint>
#include <string>
#include <vector>

#include <components/esm/defs.hpp>
#include <components/esm/formid.hpp>

namespace ESM4
{
    class Reader;

    struct BirthSign
    {
        ESM::FormId mId;
        std::uint32_t mFlags = 0;
        std::string mEditorId;
        std::string mFullName;
        std::string mDescription;
        std::string mIcon;
        std::vector<ESM::FormId> mSpells;

        void load(Reader& reader);

        static constexpr ESM::RecNameInts sRecordId = ESM::REC_BSGN4;
    };
}

#endif
