#include "tes4.hpp"
#include "arguments.hpp"
#include "labels.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>

#include <components/debug/writeflags.hpp>
#include <components/esm/esmcommon.hpp>
#include <components/esm/formkey.hpp>
#include <components/esm/path.hpp>
#include <components/esm/refid.hpp>
#include <components/esm/typetraits.hpp>
#include <components/esm4/reader.hpp>
#include <components/esm4/readerutils.hpp>
#include <components/esm4/records.hpp>
#include <components/esm4/typetraits.hpp>
#include <components/files/conversion.hpp>
#include <components/files/openfile.hpp>
#include <components/misc/strings/lower.hpp>
#include <components/toutf8/toutf8.hpp>

namespace EsmTool
{
    namespace
    {
        struct Params
        {
            const bool mQuite;
            const std::vector<std::string>* mTypes = nullptr;
            ESM::FormKeyIndex* mGraph = nullptr;

            explicit Params(const Arguments& info)
                : mQuite(info.quiet_given || info.mode == "clone")
                , mTypes(&info.types)
            {
            }

            explicit Params(ESM::FormKeyIndex& graph)
                : mQuite(true)
                , mGraph(&graph)
            {
            }
        };

        std::string toString(ESM4::GroupType type)
        {
            switch (type)
            {
                case ESM4::Grp_RecordType:
                    return "RecordType";
                case ESM4::Grp_WorldChild:
                    return "WorldChild";
                case ESM4::Grp_InteriorCell:
                    return "InteriorCell";
                case ESM4::Grp_InteriorSubCell:
                    return "InteriorSubCell";
                case ESM4::Grp_ExteriorCell:
                    return "ExteriorCell";
                case ESM4::Grp_ExteriorSubCell:
                    return "ExteriorSubCell";
                case ESM4::Grp_CellChild:
                    return "CellChild";
                case ESM4::Grp_TopicChild:
                    return "TopicChild";
                case ESM4::Grp_CellPersistentChild:
                    return "CellPersistentChild";
                case ESM4::Grp_CellTemporaryChild:
                    return "CellTemporaryChild";
                case ESM4::Grp_CellVisibleDistChild:
                    return "CellVisibleDistChild";
            }

            return "Unknown (" + std::to_string(type) + ")";
        }

        template <class T>
        struct WriteArray
        {
            std::string_view mPrefix;
            const T& mValue;

            explicit WriteArray(std::string_view prefix, const T& value)
                : mPrefix(prefix)
                , mValue(value)
            {
            }
        };

        template <class T>
        struct WriteData
        {
            const T& mValue;

            explicit WriteData(const T& value)
                : mValue(value)
            {
            }
        };

        template <class T>
        std::ostream& operator<<(std::ostream& stream, const WriteArray<T>& write)
        {
            for (const auto& value : write.mValue)
                stream << write.mPrefix << value;
            return stream;
        }

        template <class T>
        std::ostream& operator<<(std::ostream& stream, const WriteData<T>& /*write*/)
        {
            return stream << " ?";
        }

        std::ostream& operator<<(std::ostream& stream, const std::monostate&)
        {
            return stream << "[none]";
        }

        std::ostream& operator<<(std::ostream& stream, const WriteData<ESM4::GameSetting::Data>& write)
        {
            std::visit([&](const auto& v) { stream << v; }, write.mValue);
            return stream;
        }

        std::ostream& operator<<(std::ostream& stream, const WriteData<ESM4::Class::Data>& write)
        {
            stream << " favored-attributes=" << write.mValue.mFavoredAttributes[0] << ','
                   << write.mValue.mFavoredAttributes[1] << " specialization=" << write.mValue.mSpecialization
                   << " major-skills=";
            for (std::size_t i = 0; i < write.mValue.mMajorSkills.size(); ++i)
                stream << (i ? "," : "") << write.mValue.mMajorSkills[i];
            stream << " flags=" << write.mValue.mFlags << " services=" << write.mValue.mServices;
            return stream;
        }

        struct WriteCellFlags
        {
            std::uint16_t mValue;
        };

        using CellFlagString = Debug::FlagString<std::uint16_t>;

        constexpr std::array cellFlags{
            CellFlagString{ ESM4::CELL_Interior, "Interior" },
            CellFlagString{ ESM4::CELL_HasWater, "HasWater" },
            CellFlagString{ ESM4::CELL_NoTravel, "NoTravel" },
            CellFlagString{ ESM4::CELL_HideLand, "HideLand" },
            CellFlagString{ ESM4::CELL_Public, "Public" },
            CellFlagString{ ESM4::CELL_HandChgd, "HandChgd" },
            CellFlagString{ ESM4::CELL_QuasiExt, "QuasiExt" },
            CellFlagString{ ESM4::CELL_SkyLight, "SkyLight" },
        };

        std::ostream& operator<<(std::ostream& stream, const WriteCellFlags& write)
        {
            return Debug::writeFlags(stream, write.mValue, cellFlags);
        }

        template <class T>
        void readTypedRecord(const Params& params, ESM4::Reader& reader)
        {
            reader.getRecordData();

            T value;
            value.load(reader);

            if (params.mGraph != nullptr && !reader.getFormKeyFromHeader().isNull())
            {
                bool enableParentInverted = false;
                if constexpr (requires { value.mEsp.flags; })
                    enableParentInverted = (value.mEsp.flags & 1) != 0;
                ESM::FormRecordMetadata metadata = reader.takeFormRecordMetadata(enableParentInverted);
                const ESM::FormKey sourceCandidate = ESM::FormKey::content(
                    Files::pathToUnicodeString(reader.getFileName().filename()), reader.hdr().record.getFormId().mIndex);
                metadata.mKey
                    = params.mGraph->resolveRecordHeader(metadata.mKey, sourceCandidate, metadata.mRecordType);
                params.mGraph->apply(std::move(metadata));
            }

            const std::string_view recordType = ESM::NAME(reader.hdr().record.typeId).toStringView();
            if (params.mQuite
                || (params.mTypes != nullptr && !params.mTypes->empty()
                    && std::find(params.mTypes->begin(), params.mTypes->end(), recordType) == params.mTypes->end()))
                return;

            std::cout << "\n  Record: " << recordType;
            if constexpr (ESM::HasId<T>)
                std::cout << "\n  Id: " << value.mId;
            if constexpr (ESM4::HasFlags<T>)
                std::cout << "\n  Record flags: " << recordFlags(value.mFlags);
            if constexpr (ESM4::HasParent<T>)
                std::cout << "\n  Parent: " << value.mParent;
            if constexpr (requires { value.mBaseObj; })
                std::cout << "\n  BaseObj: " << value.mBaseObj;
            if constexpr (ESM4::HasEditorId<T>)
                std::cout << "\n  EditorId: " << value.mEditorId;
            if constexpr (ESM4::HasFullName<T>)
                std::cout << "\n  FullName: " << value.mFullName;
            if constexpr (ESM4::HasCellFlags<T>)
                std::cout << "\n  CellFlags: " << WriteCellFlags{ value.mCellFlags };
            if constexpr (requires { value.mMusicType; })
                std::cout << "\n  MusicType: " << static_cast<unsigned>(value.mMusicType);
            if constexpr (requires { value.mRegions; })
                std::cout << "\n  Regions:" << WriteArray("\n  - ", value.mRegions);
            if constexpr (requires { value.mSounds.front().sound; value.mSounds.front().flags; })
            {
                std::cout << "\n  RegionSounds:";
                for (const auto& sound : value.mSounds)
                    std::cout << "\n  - " << sound.sound << " flags=" << sound.flags << " chance=" << sound.chance;
            }
            if constexpr (ESM4::HasX<T>)
                std::cout << "\n  X: " << value.mX;
            if constexpr (ESM4::HasY<T>)
                std::cout << "\n  Y: " << value.mY;
            if constexpr (ESM::HasModel<T>)
                std::cout << "\n  Model: " << value.mModel.getOriginal();
            if constexpr (requires { value.mSunTexture; })
                std::cout << "\n  SunTexture: " << value.mSunTexture;
            if constexpr (requires { value.mSunGlareTexture; })
                std::cout << "\n  SunGlareTexture: " << value.mSunGlareTexture;
            if constexpr (requires { value.mLowerCloudTexture; })
                std::cout << "\n  LowerCloudTexture: " << value.mLowerCloudTexture;
            if constexpr (requires { value.mUpperCloudTexture; })
                std::cout << "\n  UpperCloudTexture: " << value.mUpperCloudTexture;
            if constexpr (requires { value.mTexture; })
                std::cout << "\n  Texture: " << value.mTexture;
            if constexpr (requires { value.mSoundFile; })
                std::cout << "\n  SoundFile: " << value.mSoundFile;
            if constexpr (requires { value.mSoundId; })
                if (!value.mSoundId.isZeroOrUnset())
                    std::cout << "\n  SoundId: " << value.mSoundId;
            if constexpr (ESM4::HasModelMale<T>)
                std::cout << "\n  ModelMale: " << value.mModelMale.getOriginal();
            if constexpr (ESM4::HasModelMaleWorld<T>)
                std::cout << "\n  ModelMaleWorld: " << value.mModelMaleWorld.getOriginal();
            if constexpr (ESM4::HasModelFemale<T>)
                std::cout << "\n  ModelFemale: " << value.mModelFemale.getOriginal();
            if constexpr (ESM4::HasModelFemaleWorld<T>)
                std::cout << "\n  ModelFemaleWorld: " << value.mModelFemaleWorld.getOriginal();
            if constexpr (requires { value.mRace; })
                std::cout << "\n  RaceId: " << value.mRace;
            if constexpr (requires { value.mHair; })
                std::cout << "\n  HairId: " << value.mHair;
            if constexpr (requires { value.mEyes; })
                std::cout << "\n  EyesId: " << value.mEyes;
            if constexpr (requires { value.mRagdoll; value.mRagdollBiped; })
            {
                if (!value.mRagdoll.mBones.empty())
                {
                    std::cout << "\n  RagdollRootMetadata: " << value.mRagdoll.mRootMetadata;
                    std::cout << "\n  RagdollRootPosition: " << value.mRagdoll.mRootPosition[0] << " "
                              << value.mRagdoll.mRootPosition[1] << " " << value.mRagdoll.mRootPosition[2];
                    std::cout << "\n  RagdollRootRotation: " << value.mRagdoll.mRootRotation[0] << " "
                              << value.mRagdoll.mRootRotation[1] << " " << value.mRagdoll.mRootRotation[2];
                    std::cout << "\n  RagdollBones: " << value.mRagdoll.mBones.size();
                    for (const auto& bone : value.mRagdoll.mBones)
                    {
                        std::cout << "\n  RagdollBone: " << bone.mBone << " position=" << bone.mPosition[0] << " "
                                  << bone.mPosition[1] << " " << bone.mPosition[2] << " rotation="
                                  << bone.mRotation[0] << " " << bone.mRotation[1] << " " << bone.mRotation[2];
                    }
                }
                if (!value.mRagdollBiped.empty())
                    std::cout << "\n  RagdollBipedBytes: " << value.mRagdollBiped.size();
            }
            if constexpr (requires { value.mInventory; })
            {
                std::cout << "\n  Inventory: " << value.mInventory.size();
                for (const auto& item : value.mInventory)
                {
                    if constexpr (requires { item.item; item.count; })
                        std::cout << " " << ESM::FormId::fromUint32(item.item) << "x" << item.count;
                    else if constexpr (requires { std::cout << item; })
                        std::cout << " " << item;
                }
            }
            if constexpr (requires { value.mSpells; })
                std::cout << "\n  Spells:" << WriteArray(" ", value.mSpells);
            if constexpr (requires { value.mIcon; })
                std::cout << "\n  Icon: " << value.mIcon;
            if constexpr (requires { value.mArmorFlags; })
                std::cout << "\n  BipedSlots: " << (value.mArmorFlags & 0xffffu);
            if constexpr (requires { value.mClothingFlags; })
                std::cout << "\n  BipedSlots: " << (value.mClothingFlags & 0xffffu);
            if constexpr (requires { value.mHeadParts; value.mBodyPartsMale; value.mBodyPartsFemale; })
            {
                std::cout << "\n  RaceFlags: " << value.mRaceFlags;
                const auto writeParts = [&](std::string_view label, const auto& parts) {
                    for (std::size_t index = 0; index < parts.size(); ++index)
                        std::cout << "\n  " << label << index << ": " << parts[index].mesh << "\t"
                                  << parts[index].texture;
                };
                writeParts("HeadPartMale", value.mHeadParts);
                writeParts("HeadPartFemale", value.mHeadPartsFemale);
                writeParts("BodyPartMale", value.mBodyPartsMale);
                writeParts("BodyPartFemale", value.mBodyPartsFemale);
                std::cout << "\n  FaceGenShapeModesMale: " << value.mSymShapeModeCoefficients.size() << "/"
                          << value.mAsymShapeModeCoefficients.size();
                std::cout << "\n  FaceGenShapeModesFemale: " << value.mSymShapeModeCoeffFemale.size() << "/"
                          << value.mAsymShapeModeCoeffFemale.size();
                std::cout << "\n  FaceGenTextureModesMale: " << value.mSymTextureModeCoefficients.size();
                std::cout << "\n  FaceGenTextureModesFemale: " << value.mSymTextureModeCoeffFemale.size();
            }
            if constexpr (requires { value.mSymShapeModeCoefficients; value.mAsymShapeModeCoefficients;
                              value.mSymTextureModeCoefficients; })
            {
                const auto writeRange = [&](std::string_view label, const std::vector<float>& values) {
                    std::cout << "\n  " << label << ": " << values.size();
                    if (!values.empty())
                    {
                        const auto [minimum, maximum] = std::minmax_element(values.begin(), values.end());
                        std::cout << " range=" << *minimum << ".." << *maximum;
                    }
                };
                writeRange("FaceGenShapeModes", value.mSymShapeModeCoefficients);
                writeRange("FaceGenAsymmetricModes", value.mAsymShapeModeCoefficients);
                writeRange("FaceGenTextureModes", value.mSymTextureModeCoefficients);
            }
            if constexpr (ESM4::HasNif<T>)
                std::cout << "\n  Nif:" << WriteArray("\n  - ", value.mNif);
            if constexpr (ESM4::HasKf<T>)
                std::cout << "\n  Kf:" << WriteArray("\n  - ", value.mKf);
            if constexpr (ESM4::HasType<T>)
                std::cout << "\n  Type: " << value.mType;
            if constexpr (ESM4::HasValue<T>)
                std::cout << "\n  Value: " << value.mValue;
            if constexpr (ESM4::HasData<T>)
                std::cout << "\n  Data: " << WriteData(value.mData);
            if constexpr (requires { value.mPos; })
            {
                std::cout << "\n  Position: " << value.mPos.pos[0] << ' ' << value.mPos.pos[1] << ' '
                          << value.mPos.pos[2];
                std::cout << "\n  Rotation: " << value.mPos.rot[0] << ' ' << value.mPos.rot[1] << ' '
                          << value.mPos.rot[2];
            }
            if constexpr (requires { value.mOwner; })
                if (!value.mOwner.isZeroOrUnset())
                    std::cout << "\n  Owner: " << value.mOwner;
            if constexpr (requires { value.mIsLocked; value.mLockLevel; value.mKey; })
            {
                std::cout << "\n  Lock: " << (value.mIsLocked ? "locked" : "unlocked") << ' '
                          << static_cast<int>(value.mLockLevel);
                if (!value.mKey.isZeroOrUnset())
                    std::cout << " key=" << value.mKey;
            }
            if constexpr (requires { value.mDoor.destDoor; value.mDoor.destPos; })
                if (!value.mDoor.destDoor.isZeroOrUnset())
                {
                    std::cout << "\n  TeleportDoor: " << value.mDoor.destDoor;
                    std::cout << "\n  TeleportPosition: " << value.mDoor.destPos.pos[0] << ' '
                              << value.mDoor.destPos.pos[1] << ' ' << value.mDoor.destPos.pos[2];
                }
            std::cout << '\n';
        }

        bool readRecord(const Params& params, ESM4::Reader& reader)
        {
            switch (static_cast<ESM4::RecordTypes>(reader.hdr().record.typeId))
            {
                case ESM4::REC_AACT:
                    break;
                case ESM4::REC_ACHR:
                    readTypedRecord<ESM4::ActorCharacter>(params, reader);
                    return true;
                case ESM4::REC_ACRE:
                    readTypedRecord<ESM4::ActorCreature>(params, reader);
                    return true;
                case ESM4::REC_ACTI:
                    readTypedRecord<ESM4::Activator>(params, reader);
                    return true;
                case ESM4::REC_ADDN:
                    break;
                case ESM4::REC_ALCH:
                    readTypedRecord<ESM4::Potion>(params, reader);
                    return true;
                case ESM4::REC_ALOC:
                    readTypedRecord<ESM4::MediaLocationController>(params, reader);
                    return true;
                case ESM4::REC_AMMO:
                    readTypedRecord<ESM4::Ammunition>(params, reader);
                    return true;
                case ESM4::REC_ANIO:
                    readTypedRecord<ESM4::AnimObject>(params, reader);
                    return true;
                case ESM4::REC_APPA:
                    readTypedRecord<ESM4::Apparatus>(params, reader);
                    return true;
                case ESM4::REC_ARMA:
                    readTypedRecord<ESM4::ArmorAddon>(params, reader);
                    return true;
                case ESM4::REC_ARMO:
                    readTypedRecord<ESM4::Armor>(params, reader);
                    return true;
                case ESM4::REC_ARTO:
                    break;
                case ESM4::REC_ASPC:
                    readTypedRecord<ESM4::AcousticSpace>(params, reader);
                    return true;
                case ESM4::REC_ASTP:
                    break;
                case ESM4::REC_AVIF:
                    break;
                case ESM4::REC_BOOK:
                    readTypedRecord<ESM4::Book>(params, reader);
                    return true;
                case ESM4::REC_BSGN:
                    readTypedRecord<ESM4::BirthSign>(params, reader);
                    return true;
                case ESM4::REC_BPTD:
                    readTypedRecord<ESM4::BodyPartData>(params, reader);
                    return true;
                case ESM4::REC_CAMS:
                    break;
                case ESM4::REC_CCRD:
                    break;
                case ESM4::REC_CELL:
                    readTypedRecord<ESM4::Cell>(params, reader);
                    return true;
                case ESM4::REC_CLAS:
                    readTypedRecord<ESM4::Class>(params, reader);
                    return true;
                case ESM4::REC_CLFM:
                    readTypedRecord<ESM4::Colour>(params, reader);
                    return true;
                case ESM4::REC_CLMT:
                    readTypedRecord<ESM4::Climate>(params, reader);
                    return true;
                case ESM4::REC_CLOT:
                    readTypedRecord<ESM4::Clothing>(params, reader);
                    return true;
                case ESM4::REC_CMNY:
                    break;
                case ESM4::REC_COBJ:
                    break;
                case ESM4::REC_COLL:
                    break;
                case ESM4::REC_CONT:
                    readTypedRecord<ESM4::Container>(params, reader);
                    return true;
                case ESM4::REC_CPTH:
                    break;
                case ESM4::REC_CREA:
                    readTypedRecord<ESM4::Creature>(params, reader);
                    return true;
                case ESM4::REC_CSTY:
                    readTypedRecord<ESM4::RawRecord>(params, reader);
                    return true;
                case ESM4::REC_DEBR:
                    break;
                case ESM4::REC_DIAL:
                    readTypedRecord<ESM4::Dialogue>(params, reader);
                    return true;
                case ESM4::REC_DLBR:
                    break;
                case ESM4::REC_DLVW:
                    break;
                case ESM4::REC_DOBJ:
                    readTypedRecord<ESM4::DefaultObj>(params, reader);
                    return true;
                case ESM4::REC_DOOR:
                    readTypedRecord<ESM4::Door>(params, reader);
                    return true;
                case ESM4::REC_DUAL:
                    break;
                case ESM4::REC_ECZN:
                    break;
                case ESM4::REC_EFSH:
                    readTypedRecord<ESM4::RawRecord>(params, reader);
                    return true;
                case ESM4::REC_ENCH:
                    readTypedRecord<ESM4::RawRecord>(params, reader);
                    return true;
                case ESM4::REC_EQUP:
                    break;
                case ESM4::REC_EXPL:
                    break;
                case ESM4::REC_EYES:
                    readTypedRecord<ESM4::Eyes>(params, reader);
                    return true;
                case ESM4::REC_FACT:
                    readTypedRecord<ESM4::RawRecord>(params, reader);
                    return true;
                case ESM4::REC_FLOR:
                    readTypedRecord<ESM4::Flora>(params, reader);
                    return true;
                case ESM4::REC_FLST:
                    readTypedRecord<ESM4::FormIdList>(params, reader);
                    return true;
                case ESM4::REC_FSTP:
                    break;
                case ESM4::REC_FSTS:
                    break;
                case ESM4::REC_FURN:
                    readTypedRecord<ESM4::Furniture>(params, reader);
                    return true;
                case ESM4::REC_GLOB:
                    readTypedRecord<ESM4::GlobalVariable>(params, reader);
                    return true;
                case ESM4::REC_GMST:
                    readTypedRecord<ESM4::GameSetting>(params, reader);
                    return true;
                case ESM4::REC_GRAS:
                    readTypedRecord<ESM4::Grass>(params, reader);
                    return true;
                case ESM4::REC_GRUP:
                    break;
                case ESM4::REC_HAIR:
                    readTypedRecord<ESM4::Hair>(params, reader);
                    return true;
                case ESM4::REC_HAZD:
                    break;
                case ESM4::REC_HDPT:
                    readTypedRecord<ESM4::HeadPart>(params, reader);
                    return true;
                case ESM4::REC_IDLE:
                    readTypedRecord<ESM4::IdleAnimation>(params, reader);
                    return true;
                    break;
                case ESM4::REC_IDLM:
                    readTypedRecord<ESM4::IdleMarker>(params, reader);
                    return true;
                case ESM4::REC_IMAD:
                    break;
                case ESM4::REC_IMGS:
                    break;
                case ESM4::REC_IMOD:
                    readTypedRecord<ESM4::ItemMod>(params, reader);
                    return true;
                case ESM4::REC_INFO:
                    readTypedRecord<ESM4::DialogInfo>(params, reader);
                    return true;
                case ESM4::REC_INGR:
                    readTypedRecord<ESM4::Ingredient>(params, reader);
                    return true;
                case ESM4::REC_IPCT:
                    break;
                case ESM4::REC_IPDS:
                    break;
                case ESM4::REC_KEYM:
                    readTypedRecord<ESM4::Key>(params, reader);
                    return true;
                case ESM4::REC_KYWD:
                    break;
                case ESM4::REC_LAND:
                    readTypedRecord<ESM4::Land>(params, reader);
                    return true;
                case ESM4::REC_LCRT:
                    break;
                case ESM4::REC_LCTN:
                    break;
                case ESM4::REC_LGTM:
                    readTypedRecord<ESM4::LightingTemplate>(params, reader);
                    return true;
                case ESM4::REC_LIGH:
                    readTypedRecord<ESM4::Light>(params, reader);
                    return true;
                case ESM4::REC_LSCR:
                    readTypedRecord<ESM4::RawRecord>(params, reader);
                    return true;
                case ESM4::REC_LTEX:
                    readTypedRecord<ESM4::LandTexture>(params, reader);
                    return true;
                case ESM4::REC_LVLC:
                    readTypedRecord<ESM4::LevelledCreature>(params, reader);
                    return true;
                case ESM4::REC_LVLI:
                    readTypedRecord<ESM4::LevelledItem>(params, reader);
                    return true;
                case ESM4::REC_LVLN:
                    readTypedRecord<ESM4::LevelledNpc>(params, reader);
                    return true;
                case ESM4::REC_LVSP:
                    readTypedRecord<ESM4::RawRecord>(params, reader);
                    return true;
                case ESM4::REC_MATO:
                    readTypedRecord<ESM4::Material>(params, reader);
                    return true;
                case ESM4::REC_MATT:
                    break;
                case ESM4::REC_MESG:
                    break;
                case ESM4::REC_MGEF:
                    readTypedRecord<ESM4::RawRecord>(params, reader);
                    return true;
                case ESM4::REC_MISC:
                    readTypedRecord<ESM4::MiscItem>(params, reader);
                    return true;
                case ESM4::REC_MOVT:
                    break;
                case ESM4::REC_MSET:
                    readTypedRecord<ESM4::MediaSet>(params, reader);
                    return true;
                case ESM4::REC_MSTT:
                    readTypedRecord<ESM4::MovableStatic>(params, reader);
                    return true;
                case ESM4::REC_MUSC:
                    readTypedRecord<ESM4::Music>(params, reader);
                    return true;
                case ESM4::REC_MUST:
                    break;
                case ESM4::REC_NAVI:
                    readTypedRecord<ESM4::Navigation>(params, reader);
                    return true;
                case ESM4::REC_NAVM:
                    readTypedRecord<ESM4::NavMesh>(params, reader);
                    return true;
                case ESM4::REC_NOTE:
                    readTypedRecord<ESM4::Note>(params, reader);
                    return true;
                case ESM4::REC_NPC_:
                    readTypedRecord<ESM4::Npc>(params, reader);
                    return true;
                case ESM4::REC_OTFT:
                    readTypedRecord<ESM4::Outfit>(params, reader);
                    return true;
                case ESM4::REC_PACK:
                    readTypedRecord<ESM4::AIPackage>(params, reader);
                    return true;
                case ESM4::REC_PERK:
                    break;
                case ESM4::REC_PGRD:
                    readTypedRecord<ESM4::Pathgrid>(params, reader);
                    return true;
                case ESM4::REC_PGRE:
                    readTypedRecord<ESM4::PlacedGrenade>(params, reader);
                    return true;
                case ESM4::REC_PHZD:
                    break;
                case ESM4::REC_PROJ:
                    break;
                case ESM4::REC_PWAT:
                    readTypedRecord<ESM4::PlaceableWater>(params, reader);
                    return true;
                case ESM4::REC_QUST:
                    readTypedRecord<ESM4::Quest>(params, reader);
                    return true;
                case ESM4::REC_RACE:
                    readTypedRecord<ESM4::Race>(params, reader);
                    return true;
                case ESM4::REC_REFR:
                    readTypedRecord<ESM4::Reference>(params, reader);
                    return true;
                case ESM4::REC_REGN:
                    readTypedRecord<ESM4::Region>(params, reader);
                    return true;
                case ESM4::REC_RELA:
                    break;
                case ESM4::REC_REVB:
                    break;
                case ESM4::REC_RFCT:
                    break;
                case ESM4::REC_ROAD:
                    readTypedRecord<ESM4::Road>(params, reader);
                    return true;
                case ESM4::REC_SBSP:
                    readTypedRecord<ESM4::SubSpace>(params, reader);
                    return true;
                case ESM4::REC_SCEN:
                    break;
                case ESM4::REC_SCOL:
                    readTypedRecord<ESM4::StaticCollection>(params, reader);
                    return true;
                case ESM4::REC_SCPT:
                    readTypedRecord<ESM4::Script>(params, reader);
                    return true;
                case ESM4::REC_SCRL:
                    readTypedRecord<ESM4::Scroll>(params, reader);
                    return true;
                case ESM4::REC_SGST:
                    readTypedRecord<ESM4::SigilStone>(params, reader);
                    return true;
                case ESM4::REC_SHOU:
                    break;
                case ESM4::REC_SLGM:
                    readTypedRecord<ESM4::SoulGem>(params, reader);
                    return true;
                case ESM4::REC_SMBN:
                    break;
                case ESM4::REC_SMEN:
                    break;
                case ESM4::REC_SMQN:
                    break;
                case ESM4::REC_SNCT:
                    break;
                case ESM4::REC_SNDR:
                    readTypedRecord<ESM4::SoundReference>(params, reader);
                    return true;
                case ESM4::REC_SOPM:
                    break;
                case ESM4::REC_SOUN:
                    readTypedRecord<ESM4::Sound>(params, reader);
                    return true;
                case ESM4::REC_SKIL:
                    readTypedRecord<ESM4::RawRecord>(params, reader);
                    return true;
                case ESM4::REC_SPEL:
                    readTypedRecord<ESM4::RawRecord>(params, reader);
                    return true;
                case ESM4::REC_SPGD:
                    break;
                case ESM4::REC_STAT:
                    readTypedRecord<ESM4::Static>(params, reader);
                    return true;
                case ESM4::REC_TACT:
                    readTypedRecord<ESM4::TalkingActivator>(params, reader);
                    return true;
                case ESM4::REC_TERM:
                    readTypedRecord<ESM4::Terminal>(params, reader);
                    return true;
                case ESM4::REC_TES4:
                    readTypedRecord<ESM4::Header>(params, reader);
                    return true;
                case ESM4::REC_TREE:
                    readTypedRecord<ESM4::Tree>(params, reader);
                    return true;
                case ESM4::REC_TXST:
                    readTypedRecord<ESM4::TextureSet>(params, reader);
                    return true;
                case ESM4::REC_VTYP:
                    break;
                case ESM4::REC_WATR:
                    readTypedRecord<ESM4::Water>(params, reader);
                    return true;
                case ESM4::REC_WEAP:
                    readTypedRecord<ESM4::Weapon>(params, reader);
                    return true;
                case ESM4::REC_WOOP:
                    break;
                case ESM4::REC_WRLD:
                    readTypedRecord<ESM4::World>(params, reader);
                    return true;
                case ESM4::REC_WTHR:
                    readTypedRecord<ESM4::Weather>(params, reader);
                    return true;
            }

            if (!params.mQuite)
                std::cout << "\n  Unsupported record: " << ESM::NAME(reader.hdr().record.typeId).toStringView() << '\n';
            if (params.mGraph != nullptr && !reader.getFormKeyFromHeader().isNull())
                throw std::runtime_error("TES4 graph audit encountered unsupported record "
                    + ESM::printName(reader.hdr().record.typeId));
            return false;
        }

        std::string jsonEscape(std::string_view value)
        {
            std::ostringstream stream;
            for (const unsigned char c : value)
            {
                if (c == '\\' || c == '"')
                    stream << '\\' << static_cast<char>(c);
                else if (c == '\n')
                    stream << "\\n";
                else if (c == '\r')
                    stream << "\\r";
                else if (c == '\t')
                    stream << "\\t";
                else if (c < 0x20)
                    stream << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<unsigned>(c)
                           << std::dec;
                else
                    stream << static_cast<char>(c);
            }
            return stream.str();
        }

        std::string fingerprint(std::span<const std::uint8_t> data)
        {
            std::uint64_t hash = 14695981039346656037ull;
            for (const std::uint8_t value : data)
            {
                hash ^= value;
                hash *= 1099511628211ull;
            }
            std::ostringstream stream;
            stream << "fnv1a64:" << std::hex << std::setw(16) << std::setfill('0') << hash;
            return stream.str();
        }

        ESM::FormKeyIndex scanGraph(
            const std::vector<std::filesystem::path>& files, const std::vector<std::uint32_t>& runtimeIndices)
        {
            if (files.size() != runtimeIndices.size())
                throw std::logic_error("TES4 graph runtime-index matrix is inconsistent");

            std::map<std::string, int> fileToModIndex;
            for (std::size_t i = 0; i < files.size(); ++i)
            {
                const std::string name = Misc::StringUtils::lowerCase(Files::pathToUnicodeString(files[i].filename()));
                if (!fileToModIndex.emplace(name, runtimeIndices[i]).second)
                    throw std::runtime_error("Duplicate TES4 graph plugin name: " + name);
            }

            const ToUTF8::StatelessUtf8Encoder encoder(ToUTF8::WINDOWS_1252);
            ESM::FormKeyIndex graph;
            const Params params(graph);
            for (std::size_t i = 0; i < files.size(); ++i)
            {
                auto stream = Files::openBinaryInputFileStream(files[i]);
                if (!stream->is_open())
                    throw std::runtime_error("Unable to open TES4 graph plugin: " + Files::pathToUnicodeString(files[i]));
                ESM4::Reader reader(std::move(stream), files[i], nullptr, &encoder, true);
                reader.setModIndex(runtimeIndices[i]);
                reader.updateModIndices(fileToModIndex);
                auto visitorRec = [&params](ESM4::Reader& value) { return readRecord(params, value); };
                auto visitorGroup = [](ESM4::Reader&) {};
                ESM4::ReaderUtils::readAll(reader, visitorRec, visitorGroup);
            }
            return graph;
        }

    }

    int loadTes4(const Arguments& info, std::unique_ptr<std::ifstream>&& stream)
    {
        std::cout << "Loading TES4 file: " << info.filename << '\n';

        try
        {
            const ToUTF8::StatelessUtf8Encoder encoder(ToUTF8::calculateEncoding(info.encoding));
            ESM4::Reader reader(std::move(stream), info.filename, nullptr, &encoder, true);
            const Params params(info);

            if (!params.mQuite)
            {
                std::cout << "Author: " << reader.getAuthor() << '\n'
                          << "Description: " << reader.getDesc() << '\n'
                          << "File format version: " << reader.esmVersionF() << '\n';

                if (const std::vector<ESM::MasterData>& masterData = reader.getGameFiles(); !masterData.empty())
                {
                    std::cout << "Masters:" << '\n';
                    for (const auto& master : masterData)
                        std::cout << "  " << master.name << ", " << master.size << " bytes\n";
                }
            }

            auto visitorRec = [&params](ESM4::Reader& r) { return readRecord(params, r); };
            auto visitorGroup = [&params](ESM4::Reader& r) {
                if (params.mQuite || (params.mTypes != nullptr && !params.mTypes->empty()))
                    return;
                auto groupType = static_cast<ESM4::GroupType>(r.hdr().group.type);
                std::cout << "\nGroup: " << toString(groupType) << " " << ESM::NAME(r.hdr().group.typeId).toStringView()
                          << '\n';
            };
            ESM4::ReaderUtils::readAll(reader, visitorRec, visitorGroup);

            if (!params.mQuite)
            {
                for (const auto& [types, stats] : reader.getSkippedSubRecords())
                {
                    std::cout << "\n  Skipped subrecord: " << ESM::NAME(types.first).toStringView() << '/'
                              << ESM::NAME(types.second).toStringView() << " calls=" << stats.calls
                              << " bytes=" << stats.bytes << '\n';
                }
            }
        }
        catch (const std::exception& e)
        {
            std::cout << "\nERROR:\n\n  " << e.what() << std::endl;
            return -1;
        }

        return 0;
    }

    int graphTes4(const Arguments& info)
    {
        try
        {
            std::vector<std::uint32_t> canonicalIndices(info.filenames.size());
            std::vector<std::uint32_t> reorderedIndices(info.filenames.size());
            for (std::size_t i = 0; i < info.filenames.size(); ++i)
            {
                canonicalIndices[i] = static_cast<std::uint32_t>(i);
                reorderedIndices[i] = static_cast<std::uint32_t>(info.filenames.size() - i - 1);
            }

            const ESM::FormKeyIndex graph = scanGraph(info.filenames, canonicalIndices);
            const std::vector<std::uint8_t> state = graph.serialize();
            const ESM::FormKeyIndex restored = ESM::FormKeyIndex::deserialize(state);
            const bool restartStable = restored.serialize() == state;
            const bool runtimeReorderStable = scanGraph(info.filenames, reorderedIndices).serialize() == state;
            const std::vector<ESM::UnresolvedFormReference> unresolved = graph.unresolvedReferences();
            const std::vector<std::vector<ESM::FormKey>> cycles = graph.enableParentCycles();

            std::filesystem::create_directories(info.outname.parent_path().empty()
                    ? std::filesystem::current_path()
                    : info.outname.parent_path());
            std::filesystem::path statePath = info.outname;
            statePath += ".formkeys.bin";
            std::ofstream stateStream(statePath, std::ios::binary);
            stateStream.write(reinterpret_cast<const char*>(state.data()), static_cast<std::streamsize>(state.size()));
            if (!stateStream)
                throw std::runtime_error("Unable to write TES4 FormKey graph state");

            std::ofstream report(info.outname);
            if (!report)
                throw std::runtime_error("Unable to write TES4 FormKey graph report");
            report << "{\n  \"schema_version\": 1,\n  \"plugins\": [";
            for (std::size_t i = 0; i < info.filenames.size(); ++i)
            {
                if (i != 0)
                    report << ',';
                report << "\n    \"" << jsonEscape(Files::pathToUnicodeString(info.filenames[i])) << '"';
            }
            report << "\n  ],\n  \"key_count\": " << graph.keyCount() << ",\n  \"revision_count\": "
                   << graph.revisionCount() << ",\n  \"reference_count\": " << graph.referenceCount()
                   << ",\n  \"serialized_bytes\": " << state.size() << ",\n  \"fingerprint\": \""
                   << fingerprint(state) << "\",\n  \"restart_stable\": " << (restartStable ? "true" : "false")
                   << ",\n  \"runtime_reorder_stable\": " << (runtimeReorderStable ? "true" : "false")
                   << ",\n  \"unresolved\": [";
            for (std::size_t i = 0; i < unresolved.size(); ++i)
            {
                const auto& edge = unresolved[i];
                if (i != 0)
                    report << ',';
                report << "\n    {\"source\": \"" << edge.mSource.serialize() << "\", \"target\": \""
                       << edge.mTarget.serialize() << "\", \"plugin\": \"" << jsonEscape(edge.mWinningPlugin)
                       << "\", \"record\": \"" << ESM::printName(edge.mRecordType) << "\", \"subrecord\": \""
                       << ESM::printName(edge.mSubRecordType) << "\", \"occurrence\": " << edge.mOccurrence
                       << ", \"reason\": \""
                       << (edge.mReason == ESM::UnresolvedFormReferenceReason::Missing ? "missing" : "deleted")
                       << "\"}";
            }
            report << "\n  ],\n  \"enable_parent_cycles\": [";
            for (std::size_t i = 0; i < cycles.size(); ++i)
            {
                if (i != 0)
                    report << ',';
                report << "\n    [";
                for (std::size_t j = 0; j < cycles[i].size(); ++j)
                {
                    if (j != 0)
                        report << ", ";
                    report << '"' << cycles[i][j].serialize() << '"';
                }
                report << ']';
            }
            report << "\n  ]\n}\n";
            if (!report)
                throw std::runtime_error("Unable to finish TES4 FormKey graph report");

            std::cout << "TES4 FormKey graph: keys=" << graph.keyCount() << " revisions=" << graph.revisionCount()
                      << " references=" << graph.referenceCount() << " unresolved=" << unresolved.size()
                      << " fingerprint=" << fingerprint(state) << '\n';
            if (!restartStable || !runtimeReorderStable)
                return 2;
            return 0;
        }
        catch (const std::exception& e)
        {
            std::cerr << "TES4 FormKey graph audit failed: " << e.what() << '\n';
            return -1;
        }
    }
}
