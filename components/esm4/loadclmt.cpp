#include "loadclmt.hpp"

#include <stdexcept>

#include "reader.hpp"

void ESM4::Climate::load(Reader& reader)
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
            case ESM::fourCC("WLST"):
            {
                if (sub.dataSize % 8 != 0)
                    throw std::runtime_error("ESM4::CLMT WLST has an invalid size");
                for (std::uint32_t read = 0; read < sub.dataSize; read += 8)
                {
                    WeatherChance value;
                    reader.getFormId(value.mWeather);
                    reader.get(value.mChance);
                    mWeather.push_back(value);
                }
                break;
            }
            case ESM::fourCC("FNAM"):
                reader.getZString(mSunTexture);
                break;
            case ESM::fourCC("GNAM"):
                reader.getZString(mSunGlareTexture);
                break;
            case ESM::fourCC("MODL"):
                reader.getZString(mModel);
                break;
            case ESM::fourCC("MODB"):
                reader.get(mBoundRadius);
                break;
            case ESM::fourCC("TNAM"):
                if (sub.dataSize != sizeof(Timing))
                    throw std::runtime_error("ESM4::CLMT TNAM has an invalid size");
                reader.get(mTiming);
                break;
            case ESM::fourCC("MODT"):
            case ESM::fourCC("MODC"):
            case ESM::fourCC("MODS"):
            case ESM::fourCC("MODF"):
            case ESM::fourCC("OBND"):
                reader.skipSubRecordData();
                break;
            default:
                throw std::runtime_error("ESM4::CLMT unknown subrecord " + ESM::printName(sub.typeId));
        }
    }
}
