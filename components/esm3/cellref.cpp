#include "cellref.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <limits>

#include <components/debug/debuglog.hpp>

#include "esmreader.hpp"
#include "esmwriter.hpp"

namespace
{
    constexpr int ZeroLock = std::numeric_limits<int>::max();
}

namespace ESM
{
    namespace
    {
        template <bool load>
        void loadIdImpl(ESMReader& esm, bool wideRefNum, CellRef& cellRef)
        {
            // According to Hrnchamd, this does not belong to the actual ref. Instead, it is a marker indicating that
            // the following refs are part of a "temp refs" section. A temp ref is not being tracked by the moved
            // references system. Its only purpose is a performance optimization for "immovable" things. We don't need
            // this, and it's problematic anyway, because any item can theoretically be moved by a script.
            if (esm.isNextSub("NAM0"))
                esm.skipHSub();

            if constexpr (load)
            {
                cellRef.blank();
                cellRef.mRefNum = esm.getFormId(wideRefNum);
                cellRef.mRefID = esm.getHNORefId("NAME");

                if (cellRef.mRefID.empty())
                    Log(Debug::Warning) << "Warning: got CellRef with empty RefId in " << esm.getName() << " 0x"
                                        << std::hex << esm.getFileOffset();
            }
            else
            {
                esm.getFormId(wideRefNum);
                esm.skipHNORefId("NAME");
            }
        }

        template <bool load>
        void loadDataImpl(ESMReader& esm, bool& isDeleted, CellRef& cellRef)
        {
            [[maybe_unused]] bool seenNativeGlobal = false;
            const auto getRefIdOrSkip = [&](ESM::RefId& refId) {
                if constexpr (load)
                    refId = esm.getRefId();
                else
                    esm.skipHRefId();
            };

            const auto getHStringOrSkip = [&](std::string& value) {
                if constexpr (load)
                    value = esm.getHString();
                else
                    esm.skipHString();
            };

            const auto getHTOrSkip = [&](auto& value) {
                if constexpr (load)
                    esm.getHT(value);
                else
                    esm.skipHT<std::decay_t<decltype(value)>>();
            };

            if constexpr (load)
                isDeleted = false;

            bool isLoaded = false;
            while (!isLoaded && esm.hasMoreSubs())
            {
                esm.getSubName();
                switch (esm.retSubName().toInt())
                {
                    case fourCC("UNAM"):
                        getHTOrSkip(cellRef.mReferenceBlocked);
                        break;
                    case fourCC("XSCL"):
                        getHTOrSkip(cellRef.mScale);
                        if constexpr (load)
                            cellRef.mScale = std::clamp(cellRef.mScale, 0.5f, 2.0f);
                        break;
                    case fourCC("ANAM"):
                        getRefIdOrSkip(cellRef.mOwner);
                        break;
                    case fourCC("BNAM"):
                        getHStringOrSkip(cellRef.mGlobalVariable);
                        break;
                    case fourCC("XSOL"):
                        getRefIdOrSkip(cellRef.mSoul);
                        break;
                    case fourCC("CNAM"):
                        getRefIdOrSkip(cellRef.mFaction);
                        break;
                    case fourCC("INDX"):
                        getHTOrSkip(cellRef.mFactionRank);
                        break;
                    case fourCC("XCHG"):
                        getHTOrSkip(cellRef.mEnchantmentCharge);
                        break;
                    case fourCC("INTV"):
                        getHTOrSkip(cellRef.mChargeInt);
                        break;
                    case fourCC("NORK"):
                        if constexpr (load)
                        {
                            std::int32_t rank;
                            esm.getHT(rank);
                            if (cellRef.mNativeOwnershipRank)
                                throw std::runtime_error("Duplicate native ownership rank");
                            cellRef.mNativeOwnershipRank = rank;
                        }
                        else esm.skipHT<std::int32_t>();
                        break;
                    case fourCC("NOGB"):
                        if constexpr (load)
                        {
                            auto global = esm.getRefId();
                            if (seenNativeGlobal || (!global.empty() && !global.getIf<ESM::FormId>()))
                                throw std::runtime_error("Invalid native ownership global");
                            seenNativeGlobal = true;
                            const auto* id = global.getIf<ESM::FormId>();
                            cellRef.mNativeOwnershipGlobal = id && id->isZeroOrUnset() ? ESM::RefId{} : global;
                        }
                        else esm.skipHRefId();
                        break;
                    case fourCC("NCHL"):
                        if constexpr (load)
                        {
                            float value;
                            esm.getHT(value);
                            if (cellRef.mNativeItemCondition || !std::isfinite(value) || value < 0)
                                throw std::runtime_error("Invalid native item condition");
                            cellRef.mNativeItemCondition = value;
                        }
                        else esm.skipHT<float>();
                        break;
                    case fourCC("NAM9"):
                        getHTOrSkip(cellRef.mCount);
                        break;
                    case fourCC("DODT"):
                        if constexpr (load)
                        {
                            esm.getSubComposite(cellRef.mDoorDest);
                            cellRef.mTeleport = true;
                        }
                        else
                            esm.skipHSub();
                        break;
                    case fourCC("DNAM"):
                        getHStringOrSkip(cellRef.mDestCell);
                        break;
                    case fourCC("FLTV"):
                        getHTOrSkip(cellRef.mLockLevel);
                        break;
                    case fourCC("KNAM"):
                        getRefIdOrSkip(cellRef.mKey);
                        break;
                    case fourCC("TNAM"):
                        getRefIdOrSkip(cellRef.mTrap);
                        break;
                    case fourCC("DATA"):
                        if constexpr (load)
                            esm.getSubComposite(cellRef.mPos);
                        else
                            esm.skipHSub();
                        break;
                    case fourCC("NAM0"):
                    {
                        esm.skipHSub();
                        break;
                    }
                    case SREC_DELE:
                        esm.skipHSub();
                        if constexpr (load)
                            isDeleted = true;
                        break;
                    default:
                        esm.cacheSubName();
                        isLoaded = true;
                        break;
                }
            }

            if constexpr (load)
            {
                if (esm.getFormatVersion() == DefaultFormatVersion) // loading a content file
                    cellRef.mIsLocked = !cellRef.mKey.empty() || cellRef.mLockLevel > 0;
                else
                    cellRef.mIsLocked = cellRef.mLockLevel > 0;
                if (cellRef.mLockLevel == ZeroLock)
                    cellRef.mLockLevel = 0;
            }
        }
    }

    void CellRef::load(ESMReader& esm, bool& isDeleted, bool wideRefNum)
    {
        loadId(esm, wideRefNum);
        loadData(esm, isDeleted);
    }

    void CellRef::loadId(ESMReader& esm, bool wideRefNum)
    {
        loadIdImpl<true>(esm, wideRefNum, *this);
    }

    void CellRef::loadData(ESMReader& esm, bool& isDeleted)
    {
        loadDataImpl<true>(esm, isDeleted, *this);
    }

    void CellRef::save(ESMWriter& esm, bool wideRefNum, bool inInventory, bool isDeleted) const
    {
        esm.writeFormId(mRefNum, wideRefNum);

        esm.writeHNCRefId("NAME", mRefID);

        if (isDeleted)
        {
            esm.writeHNString("DELE", "", 3);
            return;
        }

        if (mScale != 1.0)
        {
            esm.writeHNT("XSCL", std::clamp(mScale, 0.5f, 2.0f));
        }

        if (!inInventory)
            esm.writeHNOCRefId("ANAM", mOwner);

        esm.writeHNOCString("BNAM", mGlobalVariable);
        esm.writeHNOCRefId("XSOL", mSoul);

        if (!inInventory)
        {
            esm.writeHNOCRefId("CNAM", mFaction);
            if (mFactionRank != -2)
            {
                esm.writeHNT("INDX", mFactionRank);
            }
        }

        if (mEnchantmentCharge != -1)
            esm.writeHNT("XCHG", mEnchantmentCharge);

        if (mChargeInt != -1)
            esm.writeHNT("INTV", mChargeInt);

        if (mNativeOwnershipRank)
            esm.writeHNT("NORK", *mNativeOwnershipRank);
        if (!mNativeOwnershipGlobal.empty())
        {
            const auto* id = mNativeOwnershipGlobal.getIf<ESM::FormId>();
            if (!id || id->isZeroOrUnset())
                throw std::runtime_error("Invalid native ownership global");
            esm.writeHNRefId("NOGB", mNativeOwnershipGlobal);
        }

        if (mNativeItemCondition)
        {
            if (!std::isfinite(*mNativeItemCondition) || *mNativeItemCondition < 0)
                throw std::runtime_error("Invalid native item condition");
            esm.writeHNT("NCHL", *mNativeItemCondition);
        }

        if (mCount != 1)
            esm.writeHNT("NAM9", mCount);

        if (!inInventory && mTeleport)
        {
            esm.writeNamedComposite("DODT", mDoorDest);
            esm.writeHNOCString("DNAM", mDestCell);
        }

        if (!inInventory)
        {
            if (mIsLocked)
            {
                int lockLevel = mLockLevel;
                if (lockLevel == 0)
                    lockLevel = ZeroLock;
                esm.writeHNT("FLTV", lockLevel);
                esm.writeHNOCRefId("KNAM", mKey);
            }
            esm.writeHNOCRefId("TNAM", mTrap);
        }

        if (mReferenceBlocked != -1)
            esm.writeHNT("UNAM", mReferenceBlocked);

        if (!inInventory)
            esm.writeNamedComposite("DATA", mPos);
    }

    void CellRef::blank()
    {
        mRefNum = RefNum{};
        mRefID = ESM::RefId();
        mScale = 1;
        mOwner = ESM::RefId();
        mGlobalVariable.clear();
        mSoul = ESM::RefId();
        mFaction = ESM::RefId();
        mFactionRank = -2;
        mChargeInt = -1;
        mChargeIntRemainder = 0.0f;
        mNativeItemCondition.reset();
        mNativeOwnershipRank.reset();
        mNativeOwnershipGlobal = {};
        mEnchantmentCharge = -1;
        mCount = 1;
        mDestCell.clear();
        mLockLevel = 0;
        mIsLocked = false;
        mKey = ESM::RefId();
        mTrap = ESM::RefId();
        mReferenceBlocked = -1;
        mTeleport = false;

        for (int i = 0; i < 3; ++i)
        {
            mDoorDest.pos[i] = 0;
            mDoorDest.rot[i] = 0;
            mPos.pos[i] = 0;
            mPos.rot[i] = 0;
        }
    }

    void skipLoadCellRef(ESMReader& esm, bool wideRefNum)
    {
        CellRef cellRef;
        loadIdImpl<false>(esm, wideRefNum, cellRef);
        bool isDeleted;
        loadDataImpl<false>(esm, isDeleted, cellRef);
    }

    CellRef makeBlankCellRef()
    {
        CellRef result;
        result.blank();
        return result;
    }
}
