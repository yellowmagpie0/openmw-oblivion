/*
  Copyright (C) 2020-2021 cc9cii
  Copyright (C) 2026 OpenMW contributors

  This file is part of OpenMW.

  OpenMW is free software: you can redistribute it and/or modify it under the
  terms of the GNU General Public License version 3, or (at your option) any
  later version.
*/
#include "loadpgrd.hpp"

#include <cstring>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "reader.hpp"

namespace
{
    template <class T>
    T readValue(ESM4::Reader& reader, const char* name)
    {
        T value{};
        if (!reader.getExact(value))
            reader.fail(std::string("PGRD ") + name + " is truncated");
        return value;
    }

    void readBytes(ESM4::Reader& reader, std::vector<std::uint8_t>& result, const char* name)
    {
        result.resize(reader.subRecordHeader().dataSize);
        if (!result.empty() && !reader.get(result.data(), result.size()))
            reader.fail(std::string("PGRD ") + name + " is truncated");
    }

    ESM::FormKey readObjectKey(ESM4::Reader& reader, ESM::FormId& adjusted)
    {
        const ESM::FormId32 rawValue = readValue<ESM::FormId32>(reader, "PGRL object FormID");
        const ESM::FormId raw = ESM::FormId::fromUint32(rawValue);
        reader.recordRawFormId(raw);
        adjusted = raw;
        reader.adjustFormId(adjusted);
        return reader.resolveRawFormId(raw);
    }
}

void ESM4::Pathgrid::load(ESM4::Reader& reader)
{
    mId = reader.getFormIdFromHeader();
    mFormKey = reader.getFormKeyFromHeader();
    mFlags = reader.hdr().record.flags;
    mOwningCell = {};
    mData = 0;
    mNodes.clear();
    mLinks.clear();
    mForeign.clear();
    mObjects.clear();
    mGraphAttributes.clear();
    mHasPgrr = false;

    std::vector<std::int16_t> linkEnds;
    bool hasData = false;
    bool hasNodes = false;
    bool hasForeign = false;

    while (reader.getSubRecordHeader())
    {
        const ESM4::SubRecordHeader& subHdr = reader.subRecordHeader();
        switch (subHdr.typeId)
        {
            case ESM::fourCC("DATA"):
                if (hasData)
                    reader.fail("PGRD has duplicate DATA subrecords");
                if (subHdr.dataSize != sizeof(mData))
                    reader.fail("PGRD DATA has size " + std::to_string(subHdr.dataSize) + ", expected 2");
                mData = readValue<std::int16_t>(reader, "DATA");
                if (mData < 0)
                    reader.fail("PGRD DATA contains a negative node count");
                hasData = true;
                break;

            case ESM::fourCC("PGRP"):
            {
                if (subHdr.dataSize % sizeof(PGRP) != 0)
                    reader.fail("PGRD PGRP size is not a multiple of 16");
                const std::size_t count = subHdr.dataSize / sizeof(PGRP);
                mNodes.reserve(mNodes.size() + count);
                for (std::size_t index = 0; index < count; ++index)
                    mNodes.push_back(readValue<PGRP>(reader, "PGRP"));
                hasNodes = true;
                break;
            }

            case ESM::fourCC("PGRR"):
            {
                if (subHdr.dataSize % sizeof(std::int16_t) != 0)
                    reader.fail("PGRD PGRR size is not a multiple of 2");
                const std::size_t count = subHdr.dataSize / sizeof(std::int16_t);
                linkEnds.reserve(linkEnds.size() + count);
                for (std::size_t index = 0; index < count; ++index)
                    linkEnds.push_back(readValue<std::int16_t>(reader, "PGRR"));
                mHasPgrr = true;
                break;
            }

            case ESM::fourCC("PGRI"):
            {
                if (subHdr.dataSize % sizeof(PGRI) != 0)
                    reader.fail("PGRD PGRI size is not a multiple of 16");
                const std::size_t count = subHdr.dataSize / sizeof(PGRI);
                mForeign.reserve(mForeign.size() + count);
                for (std::size_t index = 0; index < count; ++index)
                    mForeign.push_back(readValue<PGRI>(reader, "PGRI"));
                hasForeign = true;
                break;
            }

            case ESM::fourCC("PGRL"):
            {
                if (subHdr.dataSize < sizeof(ESM::FormId32)
                    || (subHdr.dataSize - sizeof(ESM::FormId32)) % sizeof(std::int32_t) != 0)
                    reader.fail("PGRD PGRL has an invalid size");

                PGRL object;
                object.objectKey = readObjectKey(reader, object.object);
                const std::size_t count = (subHdr.dataSize - sizeof(ESM::FormId32)) / sizeof(std::int32_t);
                object.linkedNodes.resize(count);
                for (std::int32_t& node : object.linkedNodes)
                    node = readValue<std::int32_t>(reader, "PGRL linked node");
                mObjects.push_back(std::move(object));
                break;
            }

            case ESM::fourCC("PGAG"):
            {
                std::vector<std::uint8_t> payload;
                readBytes(reader, payload, "PGAG");
                mGraphAttributes.insert(mGraphAttributes.end(), payload.begin(), payload.end());
                break;
            }

            default:
                reader.fail("PGRD has unknown subrecord " + ESM::printName(subHdr.typeId));
        }
    }

    if (!hasData)
        reader.fail("PGRD is missing DATA");
    if (!hasNodes)
        reader.fail("PGRD is missing PGRP");
    if (mNodes.size() != static_cast<std::size_t>(mData))
        reader.fail("PGRD DATA/PGRP node count mismatch");

    std::size_t expectedLinks = 0;
    for (const PGRP& node : mNodes)
    {
        if (expectedLinks > std::numeric_limits<std::size_t>::max() - node.numLinks)
            reader.fail("PGRD link count overflows the host size type");
        expectedLinks += node.numLinks;
    }

    if (mHasPgrr)
    {
        if (linkEnds.size() != expectedLinks)
            reader.fail("PGRD PGRR link count does not match PGRP");
        mLinks.reserve(expectedLinks);
        std::size_t offset = 0;
        for (std::size_t start = 0; start < mNodes.size(); ++start)
        {
            for (std::size_t link = 0; link < mNodes[start].numLinks; ++link)
            {
                const std::int16_t end = linkEnds[offset++];
                if (end != -1 && (end < 0 || end >= mData))
                    reader.fail("PGRD PGRR points outside the local node array");
                mLinks.push_back({ static_cast<std::int16_t>(start), end });
            }
        }
    }
    else if (expectedLinks != 0)
        reader.fail("PGRD has linked PGRP nodes but no PGRR subrecord");

    if (hasForeign)
    {
        for (const PGRI& point : mForeign)
        {
            if (point.localNode >= mData)
                reader.fail("PGRD PGRI local node is outside the node array");
        }
    }

    for (const PGRL& object : mObjects)
    {
        for (const std::int32_t node : object.linkedNodes)
        {
            if (node != -1 && (node < 0 || node >= mData))
                reader.fail("PGRD PGRL linked node is outside the node array");
        }
    }
}
