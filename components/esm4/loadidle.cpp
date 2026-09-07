/*
  Copyright (C) 2016, 2018 cc9cii

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  cc9cii cc9c@iinet.net.au

  Much of the information on the data structures are based on the information
  from Tes4Mod:Mod_File_Format and Tes5Mod:File_Formats but also refined by
  trial & error.  See http://en.uesp.net/wiki for details.

*/
#include "loadidle.hpp"

// Extended for native TES4 idle conditions, section flags and hierarchy links.

#include <stdexcept>

#include "reader.hpp"
//#include "writer.hpp"

void ESM4::IdleAnimation::load(ESM4::Reader& reader)
{
    mId = reader.getFormIdFromHeader();
    mFlags = reader.hdr().record.flags;
    mConditions.clear();

    while (reader.getSubRecordHeader())
    {
        const ESM4::SubRecordHeader& subHdr = reader.subRecordHeader();
        switch (subHdr.typeId)
        {
            case ESM::fourCC("EDID"):
                reader.getZString(mEditorId);
                break;
            case ESM::fourCC("DNAM"):
                reader.getZString(mCollision);
                break;
            case ESM::fourCC("ENAM"):
                reader.getZString(mEvent);
                break;
            case ESM::fourCC("ANAM"):
            {
                if (!reader.hasFormVersion() && subHdr.dataSize != 1)
                    reader.fail("TES4 IDLE ANAM must have 1 byte");
                switch (subHdr.dataSize)
                {
                    case 1: // TES4
                    {
                        // Animation group section
                        reader.get(mAnimationGroup);
                        break;
                    }
                    case 8: // Everything else
                    {
                        // These IDs go into DATA for TES4
                        reader.getFormId(mParent);
                        reader.getFormId(mPrevious);
                        break;
                    }
                    default:
                        reader.skipSubRecordData();
                        break;
                }
                break;
            }
            case ESM::fourCC("MODL"):
                reader.getZString(mModel);
                break;
            case ESM::fourCC("MODB"):
                reader.get(mBoundRadius);
                break;
            case ESM::fourCC("CTDA"):
            case ESM::fourCC("CTDT"):
                if (!reader.hasFormVersion())
                {
                    const std::size_t expected = subHdr.typeId == ESM::fourCC("CTDT") ? 20 : 24;
                    if (subHdr.dataSize != expected)
                        reader.fail("TES4 IDLE condition has an invalid size");
                    std::vector<std::uint8_t> data(expected);
                    if (!reader.get(data.data(), data.size()))
                        reader.fail("TES4 IDLE condition is truncated");
                    reader.recordCurrentSubRecordFormIds(data);
                    mConditions.push_back(decodePackageCondition(data,
                        [&reader](ESM::FormId raw) { return reader.resolveRawFormId(raw); }));
                }
                else
                    reader.skipSubRecordData();
                break;
            case ESM::fourCC("DATA"):
                if (!reader.hasFormVersion())
                {
                    if (subHdr.dataSize != 8)
                        reader.fail("TES4 IDLE DATA must have 8 bytes");
                    reader.getFormId(mParent);
                    reader.getFormId(mPrevious);
                }
                else
                    reader.skipSubRecordData();
                break;
            case ESM::fourCC("CIS1"):
            case ESM::fourCC("CIS2"):
            case ESM::fourCC("MODD"):
            case ESM::fourCC("MODS"):
            case ESM::fourCC("MODT"):
            case ESM::fourCC("GNAM"): // FO4
                reader.skipSubRecordData();
                break;
            default:
                throw std::runtime_error("ESM4::IDLE::load - Unknown subrecord " + std::to_string(subHdr.typeId) + " "
                    + ESM::printName(subHdr.typeId));
        }
    }
}

// void ESM4::IdleAnimation::save(ESM4::Writer& writer) const
//{
// }

// void ESM4::IdleAnimation::blank()
//{
// }
