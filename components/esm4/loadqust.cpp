/*
  Copyright (C) 2020-2021 cc9cii

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
#include "loadqust.hpp"

#include <cstring>
#include <limits>
#include <stdexcept>

#include "reader.hpp"
//#include "writer.hpp"

void ESM4::Quest::load(ESM4::Reader& reader)
{
    mId = reader.getFormIdFromHeader();
    mFormKey = reader.getFormKeyFromHeader();
    mFlags = reader.hdr().record.flags;

    ScriptDefinition currentScript;
    bool scriptStarted = false;
    std::optional<std::uint16_t> currentStage;
    std::optional<std::uint32_t> currentEntry;
    std::uint32_t nextEntry = 0;
    const auto finishScript = [&]() {
        if (scriptStarted)
            mResultScripts.push_back(std::move(currentScript));
        currentScript = {};
        scriptStarted = false;
    };
    const auto startScript = [&]() {
        if (!scriptStarted)
        {
            currentScript.stage = currentStage;
            currentScript.stageEntry = currentEntry;
            scriptStarted = true;
        }
    };

    while (reader.getSubRecordHeader())
    {
        const ESM4::SubRecordHeader& subHdr = reader.subRecordHeader();
        switch (subHdr.typeId)
        {
            case ESM::fourCC("EDID"):
                reader.getZString(mEditorId);
                break;
            case ESM::fourCC("FULL"):
                reader.getZString(mQuestName);
                break;
            case ESM::fourCC("ICON"):
                reader.getZString(mFileName);
                break; // TES4 (none in FO3/FONV)
            case ESM::fourCC("DATA"):
            {
                if (subHdr.dataSize == 2) // TES4
                {
                    reader.get(&mData, 2);
                    mData.questDelay = 0.f; // unused in TES4 but keep it clean

                    // if ((mData.flags & Flag_StartGameEnabled) != 0)
                    // std::cout << "start quest " << mEditorId << std::endl;
                }
                else
                    reader.get(mData); // FO3

                break;
            }
            case ESM::fourCC("SCRI"):
                reader.getFormId(mQuestScript);
                break;
            case ESM::fourCC("CTDA"): // FIXME: how to detect if 1st/2nd param is a formid?
            {
                if (subHdr.dataSize == 24) // TES4
                {
                    TargetCondition cond;
                    reader.get(&cond, 24);
                    reader.recordCurrentSubRecordFormIds(cond, 24);
                    cond.reference = 0; // unused in TES4 but keep it clean
                    mTargetConditions.push_back(cond);
                }
                else if (subHdr.dataSize == 28)
                {
                    TargetCondition cond;
                    reader.get(cond); // FO3/FONV
                    reader.recordCurrentSubRecordFormIds(cond);
                    if (cond.reference)
                        reader.adjustFormId(cond.reference);
                    mTargetConditions.push_back(cond);
                }
                else
                {
                    // one record with size 20: EDID GenericSupMutBehemoth
                    reader.skipSubRecordData(); // FIXME
                }
                // FIXME: support TES5

                break;
            }
            case ESM::fourCC("SCHR"):
                if (scriptStarted)
                    finishScript();
                startScript();
                reader.get(currentScript.scriptHeader);
                currentScript.hasHeader = true;
                break;
            case ESM::fourCC("SCDA"):
            {
                if (currentScript.compiledData.has_value())
                    finishScript();
                startScript();
                std::vector<std::uint8_t> data(subHdr.dataSize);
                if (!data.empty() && !reader.get(data.data(), data.size()))
                    throw std::runtime_error("ESM4::QUST::load - Truncated SCDA payload");
                currentScript.compiledData = std::move(data);
                break;
            }
            case ESM::fourCC("SCTX"):
            {
                if (currentScript.sourceData.has_value())
                    finishScript();
                startScript();
                std::vector<std::uint8_t> raw;
                if (!reader.getRawString(raw, currentScript.scriptSource))
                    throw std::runtime_error("ESM4::QUST::load - Truncated SCTX payload");
                currentScript.sourceData = std::move(raw);
                break;
            }
            case ESM::fourCC("SCRO"):
            {
                startScript();
                ESM::FormKey key;
                reader.getFormKey(key);
                currentScript.globalReferences.push_back(std::move(key));
                break;
            }
            case ESM::fourCC("INDX"):
            {
                finishScript();
                std::uint32_t value = 0;
                if (subHdr.dataSize == sizeof(std::uint16_t))
                {
                    std::uint16_t shortValue = 0;
                    reader.get(shortValue);
                    value = shortValue;
                }
                else if (subHdr.dataSize == sizeof(std::uint32_t))
                    reader.get(value);
                else
                    throw std::runtime_error("ESM4::QUST::load - Invalid INDX payload size");
                if (value > std::numeric_limits<std::uint16_t>::max())
                    throw std::runtime_error("ESM4::QUST::load - Stage index exceeds TES4 range");
                currentStage = static_cast<std::uint16_t>(value);
                currentEntry.reset();
                nextEntry = 0;
                break;
            }
            case ESM::fourCC("QSDT"):
                finishScript();
                currentEntry = nextEntry++;
                reader.skipSubRecordData();
                break;
            case ESM::fourCC("SLSD"):
            {
                startScript();
                ScriptLocalVariableData localVar{};
                reader.get(localVar.index);
                reader.get(localVar.unknown1);
                reader.get(localVar.unknown2);
                reader.get(localVar.unknown3);
                reader.get(localVar.type);
                reader.get(localVar.unknown4);
                currentScript.localVarData.push_back(std::move(localVar));
                break;
            }
            case ESM::fourCC("SCVR"):
                if (!currentScript.localVarData.empty())
                    reader.getZString(currentScript.localVarData.back().variableName);
                else
                    reader.skipSubRecordData();
                break;
            case ESM::fourCC("SCRV"):
            {
                startScript();
                std::uint32_t index = 0;
                reader.get(index);
                currentScript.localRefVarIndex.push_back(index);
                break;
            }
            case ESM::fourCC("CNAM"):
            case ESM::fourCC("QSTA"):
            case ESM::fourCC("NNAM"): // FO3
            case ESM::fourCC("QOBJ"): // FO3
            case ESM::fourCC("NAM0"): // FO3
            case ESM::fourCC("ANAM"): // TES5
            case ESM::fourCC("DNAM"): // TES5
            case ESM::fourCC("ENAM"): // TES5
            case ESM::fourCC("FNAM"): // TES5
            case ESM::fourCC("NEXT"): // TES5
            case ESM::fourCC("ALCA"): // TES5
            case ESM::fourCC("ALCL"): // TES5
            case ESM::fourCC("ALCO"): // TES5
            case ESM::fourCC("ALDN"): // TES5
            case ESM::fourCC("ALEA"): // TES5
            case ESM::fourCC("ALED"): // TES5
            case ESM::fourCC("ALEQ"): // TES5
            case ESM::fourCC("ALFA"): // TES5
            case ESM::fourCC("ALFC"): // TES5
            case ESM::fourCC("ALFD"): // TES5
            case ESM::fourCC("ALFE"): // TES5
            case ESM::fourCC("ALFI"): // TES5
            case ESM::fourCC("ALFL"): // TES5
            case ESM::fourCC("ALFR"): // TES5
            case ESM::fourCC("ALID"): // TES5
            case ESM::fourCC("ALLS"): // TES5
            case ESM::fourCC("ALNA"): // TES5
            case ESM::fourCC("ALNT"): // TES5
            case ESM::fourCC("ALPC"): // TES5
            case ESM::fourCC("ALRT"): // TES5
            case ESM::fourCC("ALSP"): // TES5
            case ESM::fourCC("ALST"): // TES5
            case ESM::fourCC("ALUA"): // TES5
            case ESM::fourCC("CIS1"): // TES5
            case ESM::fourCC("CIS2"): // TES5
            case ESM::fourCC("CNTO"): // TES5
            case ESM::fourCC("COCT"): // TES5
            case ESM::fourCC("ECOR"): // TES5
            case ESM::fourCC("FLTR"): // TES5
            case ESM::fourCC("KNAM"): // TES5
            case ESM::fourCC("KSIZ"): // TES5
            case ESM::fourCC("KWDA"): // TES5
            case ESM::fourCC("QNAM"): // TES5
            case ESM::fourCC("QTGL"): // TES5
            case ESM::fourCC("SPOR"): // TES5
            case ESM::fourCC("VMAD"): // TES5
            case ESM::fourCC("VTCK"): // TES5
            case ESM::fourCC("ALCC"): // FO4
            case ESM::fourCC("ALCS"): // FO4
            case ESM::fourCC("ALDI"): // FO4
            case ESM::fourCC("ALFV"): // FO4
            case ESM::fourCC("ALLA"): // FO4
            case ESM::fourCC("ALMI"): // FO4
            case ESM::fourCC("GNAM"): // FO4
            case ESM::fourCC("GWOR"): // FO4
            case ESM::fourCC("LNAM"): // FO4
            case ESM::fourCC("NAM2"): // FO4
            case ESM::fourCC("OCOR"): // FO4
            case ESM::fourCC("SNAM"): // FO4
            case ESM::fourCC("XNAM"): // FO4
                reader.skipSubRecordData();
                break;
            default:
                throw std::runtime_error("ESM4::QUST::load - Unknown subrecord " + ESM::printName(subHdr.typeId));
        }
    }
    finishScript();
    // if (mEditorId == "DAConversations")
    // std::cout << mEditorId << std::endl;
}

// void ESM4::Quest::save(ESM4::Writer& writer) const
//{
// }

// void ESM4::Quest::blank()
//{
// }
