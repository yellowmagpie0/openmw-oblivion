#include "loadwthr.hpp"

#include <stdexcept>

#include "reader.hpp"

void ESM4::Weather::load(Reader& reader)
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
            case ESM::fourCC("CNAM"):
                reader.getZString(mLowerCloudTexture);
                break;
            case ESM::fourCC("DNAM"):
                reader.getZString(mUpperCloudTexture);
                break;
            case ESM::fourCC("MODL"):
                reader.getZString(mModel);
                break;
            case ESM::fourCC("MODB"):
                reader.get(mBoundRadius);
                break;
            case ESM::fourCC("NAM0"):
                if (sub.dataSize != sizeof(mColors))
                    throw std::runtime_error("ESM4::WTHR NAM0 has an invalid size");
                reader.get(mColors);
                break;
            case ESM::fourCC("FNAM"):
                if (sub.dataSize != sizeof(mFog))
                    throw std::runtime_error("ESM4::WTHR FNAM has an invalid size");
                reader.get(mFog);
                break;
            case ESM::fourCC("HNAM"):
                if (sub.dataSize != sizeof(mHdr))
                    throw std::runtime_error("ESM4::WTHR HNAM has an invalid size");
                reader.get(mHdr);
                break;
            case ESM::fourCC("DATA"):
                if (sub.dataSize != sizeof(mData))
                    throw std::runtime_error("ESM4::WTHR DATA has an invalid size");
                reader.get(mData);
                break;
            case ESM::fourCC("SNAM"):
            {
                if (sub.dataSize % 8 != 0)
                    throw std::runtime_error("ESM4::WTHR SNAM has an invalid size");
                for (std::uint32_t read = 0; read < sub.dataSize; read += 8)
                {
                    Sound value;
                    reader.getFormId(value.mSound);
                    reader.get(value.mType);
                    mSounds.push_back(value);
                }
                break;
            }
            case ESM::fourCC("MODT"):
            case ESM::fourCC("MODC"):
            case ESM::fourCC("MODS"):
            case ESM::fourCC("MODF"):
            case ESM::fourCC("OBND"):
                reader.skipSubRecordData();
                break;
            default:
                throw std::runtime_error("ESM4::WTHR unknown subrecord " + ESM::printName(sub.typeId));
        }
    }
}
