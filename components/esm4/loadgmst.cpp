#include "loadgmst.hpp"

#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>

#include "common.hpp"
#include "reader.hpp"

namespace ESM4
{
    namespace
    {
        GameSetting::Data readData(ESM::FormId formId, std::string_view editorId, Reader& reader, bool native)
        {
            if (editorId.empty())
            {
                reader.skipSubRecordData();
                return std::monostate{};
            }
            const char type = editorId[0];
            if (native)
            {
                if (type != 'f' && type != 'i' && type != 's')
                    reader.fail("TES4 GMST has unsupported value type");
                if (type != 's' && reader.subRecordHeader().dataSize != 4)
                    reader.fail("TES4 numeric GMST DATA must be exactly four bytes");
            }
            switch (type)
            {
                case 'b':
                {
                    std::uint32_t value = 0;
                    reader.get(value);
                    return value != 0;
                }
                case 'i':
                {
                    std::int32_t value = 0;
                    reader.get(value);
                    return value;
                }
                case 'f':
                {
                    float value = 0;
                    reader.get(value);
                    if (native && !std::isfinite(value))
                        reader.fail("TES4 GMST has nonfinite value");
                    return value;
                }
                case 's':
                {
                    std::string value;
                    reader.getLocalizedString(value);
                    return value;
                }
                case 'u':
                {
                    std::uint32_t value = 0;
                    reader.get(value);
                    return value;
                }
                default:
                    throw std::runtime_error(
                        "Unsupported ESM4 GMST (" + formId.toString() + ") data type: " + std::string(editorId));
            }
        }
    }

    void GameSetting::load(Reader& reader)
    {
        *this = {};
        const bool native = !reader.hasFormVersion()
            && (reader.esmVersionF() == 0.8f || reader.esmVersionF() == 1.f);
        bool hasName = false;
        bool hasData = false;
        mId = reader.getFormIdFromHeader();
        mFlags = reader.hdr().record.flags;

        while (reader.getSubRecordHeader())
        {
            const ESM4::SubRecordHeader& subHdr = reader.subRecordHeader();
            switch (subHdr.typeId)
            {
                case ESM::fourCC("EDID"):
                    if (native && hasName)
                        reader.fail("TES4 GMST has duplicate EDID");
                    reader.getZString(mEditorId);
                    hasName = true;
                    if (native && mEditorId.empty())
                        reader.fail("TES4 GMST has empty EDID");
                    break;
                case ESM::fourCC("DATA"):
                    if (native && (!hasName || hasData))
                        reader.fail("TES4 GMST has unordered or duplicate DATA");
                    mData = readData(mId, mEditorId, reader, native);
                    hasData = true;
                    break;
                default:
                    throw std::runtime_error(
                        "Unknown ESM4 GMST (" + mId.toString() + ") subrecord " + ESM::printName(subHdr.typeId));
            }
        }
        if (native && !(mFlags & Rec_Deleted) && (!hasName || !hasData))
            reader.fail("TES4 GMST is missing EDID or DATA");
    }

}
