#include "loadwatr.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>

#include "reader.hpp"

void ESM4::Water::load(Reader& reader)
{
    mId = reader.getFormIdFromHeader();
    mFlags = reader.hdr().record.flags;

    while (reader.getSubRecordHeader())
    {
        const auto& sub = reader.subRecordHeader();
        switch (sub.typeId)
        {
            case ESM::fourCC("EDID"):
                reader.getZString(mEditorId);
                break;
            case ESM::fourCC("TNAM"):
                reader.getZString(mTexture);
                break;
            case ESM::fourCC("ANAM"):
                reader.get(mOpacity);
                break;
            case ESM::fourCC("FNAM"):
                reader.get(mWaterFlags);
                break;
            case ESM::fourCC("MNAM"):
                reader.getZString(mMaterial);
                break;
            case ESM::fourCC("SNAM"):
                reader.getFormId(mSound);
                break;
            case ESM::fourCC("DATA"):
            {
                // Oblivion permits progressively extended DATA layouts (2, 42, 62, 86 or 102 bytes).
                const auto size = std::min<std::size_t>(sub.dataSize, sizeof(mData));
                if (size != 2 && size != 42 && size != 62 && size != 86 && size != 102)
                    throw std::runtime_error("ESM4::WATR DATA has an invalid size");
                reader.get(&mData, size);
                if (sub.dataSize > size)
                    reader.skipSubRecordData(sub.dataSize - size);
                break;
            }
            case ESM::fourCC("GNAM"):
                if (sub.dataSize != 12)
                    throw std::runtime_error("ESM4::WATR GNAM has an invalid size");
                reader.getFormId(mRelatedWater.mDaytime);
                reader.getFormId(mRelatedWater.mNighttime);
                reader.getFormId(mRelatedWater.mUnderwater);
                break;
            case ESM::fourCC("NAM0"):
            case ESM::fourCC("NAM1"):
                reader.skipSubRecordData();
                break;
            default:
                throw std::runtime_error("ESM4::WATR unknown subrecord " + ESM::printName(sub.typeId));
        }
    }
}
