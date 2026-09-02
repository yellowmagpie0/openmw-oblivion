/*
  Copyright (C) 2026 OpenMW contributors

  This file is part of OpenMW.

  OpenMW is free software: you can redistribute it and/or modify it under the
  terms of the GNU General Public License version 3, or (at your option) any
  later version.
*/
#include "loadpack.hpp"

#include <cstring>
#include <type_traits>
#include <vector>

#include "reader.hpp"

namespace
{
    std::vector<std::uint8_t> readPayload(ESM4::Reader& reader, std::size_t expected, const char* name)
    {
        if (reader.subRecordHeader().dataSize != expected)
            reader.fail(std::string("PACK ") + name + " has size "
                + std::to_string(reader.subRecordHeader().dataSize) + ", expected " + std::to_string(expected));

        std::vector<std::uint8_t> result(expected);
        if (!result.empty() && !reader.get(result.data(), result.size()))
            reader.fail(std::string("PACK ") + name + " is truncated");
        return result;
    }

    template <class T>
    T unpack(const std::vector<std::uint8_t>& data)
    {
        static_assert(std::is_trivially_copyable_v<T>);
        T value{};
        std::memcpy(&value, data.data(), sizeof(T));
        return value;
    }

    ESM4::PackageLocationKind locationKind(std::int32_t value)
    {
        switch (value)
        {
            case -1:
                return ESM4::PackageLocationKind::None;
            case 0:
                return ESM4::PackageLocationKind::NearReference;
            case 1:
                return ESM4::PackageLocationKind::InCell;
            case 2:
                return ESM4::PackageLocationKind::CurrentLocation;
            case 3:
                return ESM4::PackageLocationKind::EditorLocation;
            case 4:
                return ESM4::PackageLocationKind::ObjectId;
            case 5:
                return ESM4::PackageLocationKind::ObjectType;
            default:
                return ESM4::PackageLocationKind::Unknown;
        }
    }

    ESM4::PackageTargetKind targetKind(std::int32_t value)
    {
        switch (value)
        {
            case -1:
                return ESM4::PackageTargetKind::None;
            case 0:
                return ESM4::PackageTargetKind::SpecificReference;
            case 1:
                return ESM4::PackageTargetKind::ObjectId;
            case 2:
                return ESM4::PackageTargetKind::ObjectType;
            case 3:
                return ESM4::PackageTargetKind::LinkedReference;
            default:
                return ESM4::PackageTargetKind::Unknown;
        }
    }

    bool isLocationReference(ESM4::PackageLocationKind kind)
    {
        return kind == ESM4::PackageLocationKind::NearReference || kind == ESM4::PackageLocationKind::InCell
            || kind == ESM4::PackageLocationKind::EditorLocation || kind == ESM4::PackageLocationKind::ObjectId;
    }

    bool isTargetReference(ESM4::PackageTargetKind kind)
    {
        return kind == ESM4::PackageTargetKind::SpecificReference
            || kind == ESM4::PackageTargetKind::ObjectId || kind == ESM4::PackageTargetKind::LinkedReference;
    }

    void adjustReference(ESM4::Reader& reader, ESM::FormId32& value, ESM::FormId& adjusted, ESM::FormKey& key)
    {
        const ESM::FormId raw = ESM::FormId::fromUint32(value);
        reader.recordRawFormId(raw);
        adjusted = raw;
        reader.adjustFormId(adjusted);
        key = reader.resolveRawFormId(raw);
    }
}

void ESM4::AIPackage::load(ESM4::Reader& reader)
{
    mId = reader.getFormIdFromHeader();
    mFormKey = reader.getFormKeyFromHeader();
    mFlags = reader.hdr().record.flags;
    mEditorId.clear();

    mData = {};
    mSchedule = {};
    mLocation = {};
    mTarget = {};
    mPackageFlags = {};
    mPackageType = AIPackageType::Unknown;
    mScheduleData = {};
    mLocationData = {};
    mTargetData = {};
    mConditions.clear();
    mCanonicalConditions.clear();
    mRawPKDT.clear();
    mSkippedSubrecords.clear();
    mUsedShortPKDT = false;

    bool hasPackageData = false;
    bool hasSchedule = false;

    while (reader.getSubRecordHeader())
    {
        const ESM4::SubRecordHeader& subHdr = reader.subRecordHeader();
        switch (subHdr.typeId)
        {
            case ESM::fourCC("EDID"):
                if (!reader.getZString(mEditorId))
                    reader.fail("PACK EDID is truncated");
                break;
            case ESM::fourCC("PKDT"):
            {
                if (hasPackageData)
                    reader.fail("PACK contains duplicate PKDT subrecords");
                hasPackageData = true;
                if (subHdr.dataSize != sizeof(PKDT) && subHdr.dataSize != sizeof(PKDTShort))
                    reader.fail("PACK PKDT uses an unsupported layout size " + std::to_string(subHdr.dataSize));

                mRawPKDT.resize(subHdr.dataSize);
                if (!reader.get(mRawPKDT.data(), mRawPKDT.size()))
                    reader.fail("PACK PKDT is truncated");

                if (subHdr.dataSize == sizeof(PKDTShort))
                {
                    const PKDTShort value = unpack<PKDTShort>(mRawPKDT);
                    mData.flags = value.flags;
                    mData.type = value.type;
                    mData.unknown[0] = value.unknown;
                    mData.unknown[1] = 0;
                    mData.unknown[2] = 0;
                    mUsedShortPKDT = true;
                }
                else
                    mData = unpack<PKDT>(mRawPKDT);

                mPackageFlags = decodePackageFlags(mData.flags);
                const auto packageType = packageTypeFromRaw(mData.type);
                if (!packageType)
                    reader.fail("PACK PKDT has an unsupported TES4 package type " + std::to_string(mData.type));
                mPackageType = *packageType;
                break;
            }
            case ESM::fourCC("PSDT"):
            {
                if (hasSchedule)
                    reader.fail("PACK contains duplicate PSDT subrecords");
                hasSchedule = true;
                const std::vector<std::uint8_t> data = readPayload(reader, sizeof(PSDT), "PSDT");
                mSchedule = unpack<PSDT>(data);
                mScheduleData = { mSchedule.month, mSchedule.dayOfWeek, mSchedule.date, mSchedule.time,
                    mSchedule.duration };
                std::string reason;
                if (!mScheduleData.isValid(&reason))
                    reader.fail("PACK PSDT is invalid: " + reason);
                break;
            }
            case ESM::fourCC("PLDT"):
            {
                const std::vector<std::uint8_t> data = readPayload(reader, sizeof(PLDT), "PLDT");
                mLocation = unpack<PLDT>(data);
                mLocationData.mRawKind = mLocation.type;
                mLocationData.mKind = locationKind(mLocation.type);
                mLocationData.mRadius = mLocation.radius;
                if (mLocationData.mKind == PackageLocationKind::Unknown)
                    reader.fail("PACK PLDT has an unsupported location discriminant "
                        + std::to_string(mLocation.type));
                if (isLocationReference(mLocationData.mKind))
                {
                    adjustReference(reader, mLocation.location, mLocationData.mReference,
                        mLocationData.mReferenceKey);
                }
                else if (mLocationData.mKind == PackageLocationKind::ObjectType)
                    mLocationData.mObjectType = mLocation.location;
                break;
            }
            case ESM::fourCC("PTDT"):
            {
                const std::vector<std::uint8_t> data = readPayload(reader, sizeof(PTDT), "PTDT");
                mTarget = unpack<PTDT>(data);
                mTargetData.mRawKind = mTarget.type;
                mTargetData.mKind = targetKind(mTarget.type);
                mTargetData.mDistance = mTarget.distance;
                if (mTargetData.mKind == PackageTargetKind::Unknown)
                    reader.fail("PACK PTDT has an unsupported target discriminant "
                        + std::to_string(mTarget.type));
                if (isTargetReference(mTargetData.mKind))
                    adjustReference(reader, mTarget.target, mTargetData.mReference, mTargetData.mReferenceKey);
                else if (mTargetData.mKind == PackageTargetKind::ObjectType)
                    mTargetData.mObjectType = mTarget.target;
                break;
            }
            case ESM::fourCC("CTDA"):
            case ESM::fourCC("CTDT"):
            {
                const std::size_t expectedSize = subHdr.typeId == ESM::fourCC("CTDA")
                    ? sizeof(CTDA)
                    : sizeof(CTDA) - sizeof(std::uint32_t);
                const char* name = subHdr.typeId == ESM::fourCC("CTDA") ? "CTDA" : "CTDT";
                const std::vector<std::uint8_t> data = readPayload(reader, expectedSize, name);
                CTDA condition{};
                std::memcpy(&condition, data.data(), data.size());
                reader.recordCurrentSubRecordFormIds(data);
                mConditions.push_back(condition);
                mCanonicalConditions.push_back(
                    decodePackageCondition(data, [&reader](ESM::FormId raw) { return reader.resolveRawFormId(raw); }));
                break;
            }
            // These are later-game/extension layouts.  They are deliberately
            // counted and skipped rather than being partially interpreted as
            // TES4 package data.
            case ESM::fourCC("TNAM"):
            case ESM::fourCC("INAM"):
            case ESM::fourCC("CNAM"):
            case ESM::fourCC("SCHR"):
            case ESM::fourCC("POBA"):
            case ESM::fourCC("POCA"):
            case ESM::fourCC("POEA"):
            case ESM::fourCC("SCTX"):
            case ESM::fourCC("SCDA"):
            case ESM::fourCC("SCRO"):
            case ESM::fourCC("IDLA"):
            case ESM::fourCC("IDLC"):
            case ESM::fourCC("IDLF"):
            case ESM::fourCC("IDLT"):
            case ESM::fourCC("PKDD"):
            case ESM::fourCC("PKD2"):
            case ESM::fourCC("PKPT"):
            case ESM::fourCC("PKED"):
            case ESM::fourCC("PKE2"):
            case ESM::fourCC("PKAM"):
            case ESM::fourCC("PUID"):
            case ESM::fourCC("PKW3"):
            case ESM::fourCC("PTD2"):
            case ESM::fourCC("PLD2"):
            case ESM::fourCC("PKFD"):
            case ESM::fourCC("SLSD"):
            case ESM::fourCC("SCVR"):
            case ESM::fourCC("SCRV"):
            case ESM::fourCC("IDLB"):
            case ESM::fourCC("ANAM"):
            case ESM::fourCC("BNAM"):
            case ESM::fourCC("FNAM"):
            case ESM::fourCC("PNAM"):
            case ESM::fourCC("QNAM"):
            case ESM::fourCC("UNAM"):
            case ESM::fourCC("XNAM"):
            case ESM::fourCC("PDTO"):
            case ESM::fourCC("PTDA"):
            case ESM::fourCC("PFOR"):
            case ESM::fourCC("PFO2"):
            case ESM::fourCC("PRCB"):
            case ESM::fourCC("PKCU"):
            case ESM::fourCC("PKC2"):
            case ESM::fourCC("CITC"):
            case ESM::fourCC("CIS1"):
            case ESM::fourCC("CIS2"):
            case ESM::fourCC("VMAD"):
            case ESM::fourCC("TPIC"):
                reader.skipSubRecordData();
                mSkippedSubrecords.push_back(subHdr.typeId);
                break;
            default:
                reader.fail("PACK has unknown subrecord " + ESM::printName(subHdr.typeId));
        }
    }

    if (!hasPackageData)
        reader.fail("PACK is missing PKDT");
    if (!hasSchedule)
        reader.fail("PACK is missing PSDT");
}
