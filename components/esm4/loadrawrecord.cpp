#include "loadrawrecord.hpp"

#include <algorithm>
#include <stdexcept>

#include <components/esm/fourcc.hpp>

#include "reader.hpp"

namespace ESM4
{
    namespace
    {
        std::string readZeroTerminatedString(const std::vector<std::uint8_t>& data)
        {
            const auto end = std::find(data.begin(), data.end(), std::uint8_t{ 0 });
            return { data.begin(), end };
        }
    }

    void RawRecord::load(Reader& reader)
    {
        mId = reader.getFormIdFromHeader();
        mFormKey = reader.getFormKeyFromHeader();
        mFlags = reader.hdr().record.flags;
        mRecordType = reader.hdr().record.typeId;

        RawSubRecordHeader header;
        while (reader.getRawSubRecordHeader(header))
        {
            RawSubRecord subRecord;
            subRecord.mType = header.typeId;
            subRecord.mExtended = header.extended;
            subRecord.mData.resize(header.dataSize);
            if (!subRecord.mData.empty() && !reader.get(subRecord.mData.data(), subRecord.mData.size()))
                throw std::runtime_error("Failed to read raw ESM4 subrecord payload");

            reader.recordCurrentSubRecordFormIds(std::span<const std::uint8_t>(subRecord.mData));

            if (subRecord.mType == ESM::fourCC("EDID"))
                mEditorId = readZeroTerminatedString(subRecord.mData);
            else if (subRecord.mType == ESM::fourCC("FULL"))
                mFullName = readZeroTerminatedString(subRecord.mData);

            mSubRecords.push_back(std::move(subRecord));
        }
    }
}
