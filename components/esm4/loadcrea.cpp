/*
  Copyright (C) 2016, 2018, 2020-2021 cc9cii

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
#include "loadcrea.hpp"

#include <cstring>
#include <stdexcept>
#include <string>

#include <components/debug/debuglog.hpp>

#include "reader.hpp"
//#include "writer.hpp"

void ESM4::Creature::load(ESM4::Reader& reader)
{
    *this = {};
    const bool tes4 = !reader.hasFormVersion() && (reader.esmVersionF() == 0.8f || reader.esmVersionF() == 1.f);
    std::optional<SoundType> soundType;
    std::optional<Sound> pendingSound;
    bool hasData = false;
    mId = reader.getFormIdFromHeader();
    mFormKey = reader.getFormKeyFromHeader();
    mFlags = reader.hdr().record.flags;
    mAIPackages.clear();
    mAIPackageKeys.clear();

    while (reader.getSubRecordHeader())
    {
        const ESM4::SubRecordHeader& subHdr = reader.subRecordHeader();
        switch (subHdr.typeId)
        {
            case ESM::fourCC("EDID"):
                reader.getZString(mEditorId);
                break;
            case ESM::fourCC("FULL"):
                reader.getZString(mFullName);
                break;
            case ESM::fourCC("MODL"):
                reader.getZString(mModel);
                break;
            case ESM::fourCC("CNTO"):
            {
                InventoryItem inv; // FIXME: use unique_ptr here?
                reader.get(inv);
                reader.adjustFormId(inv.item);
                mInventory.push_back(inv);
                break;
            }
            case ESM::fourCC("SPLO"):
                reader.getFormId(mSpell.emplace_back());
                break;
            case ESM::fourCC("PKID"):
            {
                ESM::FormId32 rawValue = 0;
                if (!reader.getExact(rawValue))
                    reader.fail("CREA PKID is truncated");
                const ESM::FormId raw = ESM::FormId::fromUint32(rawValue);
                reader.recordRawFormId(raw);
                ESM::FormId adjusted = raw;
                reader.adjustFormId(adjusted);
                mAIPackages.push_back(adjusted);
                mAIPackageKeys.push_back(reader.resolveRawFormId(raw));
                break;
            }
            case ESM::fourCC("SNAM"):
            {
                ActorFaction faction{};
                reader.get(faction);
                reader.adjustFormId(faction.faction);
                mFactions.push_back(faction);
                mFaction = faction;
                break;
            }
            case ESM::fourCC("INAM"):
                reader.getFormId(mDeathItem);
                break;
            case ESM::fourCC("SCRI"):
                reader.getFormId(mScriptId);
                break;
            case ESM::fourCC("AIDT"):
                if (subHdr.dataSize == 20) // FO3
                    reader.skipSubRecordData();
                else
                    reader.get(mAIData); // 12 bytes
                break;
            case ESM::fourCC("ACBS"):
                // if (esmVer == ESM::VER_094 || esmVer == ESM::VER_170 || mIsFONV)
                if (subHdr.dataSize == 24)
                    reader.get(mBaseConfig);
                else
                    reader.get(&mBaseConfig, 16); // TES4
                break;
            case ESM::fourCC("DATA"):
                if (!tes4 && subHdr.dataSize == 17) // FO3
                    reader.skipSubRecordData();
                else
                {
                    if (tes4 && (hasData || subHdr.dataSize != sizeof(Data)))
                        reader.fail("CREA DATA has duplicate or invalid native layout");
                    if (!reader.getExact(mData))
                        reader.fail("CREA DATA is truncated");
                    if (tes4 && (mData.creatureType > 5 || mData.soul > 5))
                        reader.fail("CREA DATA type or soul outside native domain");
                    hasData = true;
                }
                break;
            case ESM::fourCC("RNAM"):
                if (tes4)
                {
                    std::uint8_t reach = 0;
                    if (mAttackReach || subHdr.dataSize != 1 || !reader.getExact(reach))
                        reader.fail("CREA RNAM has duplicate or invalid native layout");
                    mAttackReach = reach;
                }
                else
                    reader.skipSubRecordData();
                break;
            case ESM::fourCC("ZNAM"):
                reader.getFormId(mCombatStyle);
                break;
            case ESM::fourCC("CSCR"):
            {
                ESM::FormId32 raw = 0;
                if (subHdr.dataSize != 4 || !reader.getExact(raw))
                    reader.fail("CREA CSCR has invalid layout");
                mSoundBase = ESM::FormId::fromUint32(raw);
                reader.recordRawFormId(mSoundBase);
                mSoundBaseKey = reader.resolveRawFormId(mSoundBase);
                reader.adjustFormId(mSoundBase);
                break;
            }
            case ESM::fourCC("CSDT"):
                if (tes4)
                {
                    std::uint32_t value = 0;
                    if (pendingSound || subHdr.dataSize != 4 || !reader.getExact(value) || value > 9)
                        reader.fail("CREA CSDT has invalid type, layout or order");
                    soundType = static_cast<SoundType>(value);
                }
                else
                    reader.skipSubRecordData();
                break;
            case ESM::fourCC("CSDI"):
                if (tes4)
                {
                    ESM::FormId32 raw = 0;
                    if (!soundType || pendingSound || subHdr.dataSize != 4 || !reader.getExact(raw))
                        reader.fail("CREA CSDI has invalid layout or order");
                    mSound = ESM::FormId::fromUint32(raw);
                    reader.recordRawFormId(mSound);
                    const ESM::FormKey key = reader.resolveRawFormId(mSound);
                    reader.adjustFormId(mSound);
                    pendingSound = Sound{ *soundType, mSound, key, 0 };
                }
                else
                    reader.getFormId(mSound);
                break;
            case ESM::fourCC("CSDC"):
                if (tes4)
                {
                    if (!pendingSound || subHdr.dataSize != 1 || !reader.getExact(mSoundChance) || mSoundChance > 100)
                        reader.fail("CREA CSDC has invalid chance, layout or order");
                    pendingSound->mChance = mSoundChance;
                    mSounds.push_back(*pendingSound);
                    pendingSound.reset();
                }
                else
                    reader.get(mSoundChance);
                break;
            case ESM::fourCC("BNAM"):
                reader.get(mBaseScale);
                break;
            case ESM::fourCC("TNAM"):
                reader.get(mTurningSpeed);
                break;
            case ESM::fourCC("WNAM"):
                reader.get(mFootWeight);
                break;
            case ESM::fourCC("MODB"):
                reader.get(mBoundRadius);
                break;
            case ESM::fourCC("NAM0"):
                reader.getZString(mBloodSpray);
                break;
            case ESM::fourCC("NAM1"):
                reader.getZString(mBloodDecal);
                break;
            case ESM::fourCC("NIFZ"):
                if (!reader.getZeroTerminatedStringArray(mNif))
                    throw std::runtime_error("CREA NIFZ data read error");
                break;
            case ESM::fourCC("NIFT"):
            {
                if (subHdr.dataSize != 4) // FIXME: FO3
                {
                    reader.skipSubRecordData();
                    break;
                }

                if (subHdr.dataSize != 4)
                    throw std::runtime_error("CREA NIFT datasize error");
                std::uint32_t nift;
                reader.get(nift);
                if (nift)
                    Log(Debug::Verbose) << "CREA NIFT " << mId << ", non-zero " << nift;
                break;
            }
            case ESM::fourCC("KFFZ"):
                if (!reader.getZeroTerminatedStringArray(mKf))
                    throw std::runtime_error("CREA KFFZ data read error");
                break;
            case ESM::fourCC("TPLT"):
                reader.getFormId(mBaseTemplate);
                break; // FO3
            case ESM::fourCC("PNAM"): // FO3/FONV/TES5
                reader.getFormId(mBodyParts.emplace_back());
                break;
            case ESM::fourCC("MODT"):
            case ESM::fourCC("OBND"): // FO3
            case ESM::fourCC("EAMT"): // FO3
            case ESM::fourCC("VTCK"): // FO3
            case ESM::fourCC("NAM4"): // FO3
            case ESM::fourCC("NAM5"): // FO3
            case ESM::fourCC("CNAM"): // FO3
            case ESM::fourCC("LNAM"): // FO3
            case ESM::fourCC("EITM"): // FO3
            case ESM::fourCC("DEST"): // FO3
            case ESM::fourCC("DSTD"): // FO3
            case ESM::fourCC("DSTF"): // FO3
            case ESM::fourCC("DMDL"): // FO3
            case ESM::fourCC("DMDT"): // FO3
            case ESM::fourCC("COED"): // FO3
                reader.skipSubRecordData();
                break;
            default:
                throw std::runtime_error("ESM4::CREA::load - Unknown subrecord " + ESM::printName(subHdr.typeId));
        }
    }
    if (pendingSound)
        reader.fail("CREA sound is missing its CSDC chance");
}

// void ESM4::Creature::save(ESM4::Writer& writer) const
//{
// }

// void ESM4::Creature::blank()
//{
// }
