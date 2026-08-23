#include "loadbsgn.hpp"

#include <stdexcept>

#include "reader.hpp"

void ESM4::BirthSign::load(Reader& reader)
{
    mId = reader.getFormIdFromHeader();
    mFlags = reader.hdr().record.flags;
    while (reader.getSubRecordHeader())
    {
        const SubRecordHeader& sub = reader.subRecordHeader();
        switch (sub.typeId)
        {
            case ESM::fourCC("EDID"):
                reader.getZString(mEditorId);
                break;
            case ESM::fourCC("FULL"):
                reader.getLocalizedString(mFullName);
                break;
            case ESM::fourCC("DESC"):
                reader.getLocalizedString(mDescription);
                break;
            case ESM::fourCC("ICON"):
                reader.getZString(mIcon);
                break;
            case ESM::fourCC("SPLO"):
                reader.getFormId(mSpells.emplace_back());
                break;
            default:
                throw std::runtime_error("ESM4::BSGN::load - Unknown subrecord " + ESM::printName(sub.typeId));
        }
    }
}
