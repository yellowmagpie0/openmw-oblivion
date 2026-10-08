#include "weatherstate.hpp"

#include "esmreader.hpp"
#include "esmwriter.hpp"

#include <set>
#include <limits>
#include <stdexcept>

namespace ESM
{
    namespace
    {
        constexpr NAME currentRegionRecord = "CREG";
        constexpr NAME timePassedRecord = "TMPS";
        constexpr NAME fastForwardRecord = "FAST";
        constexpr NAME weatherUpdateTimeRecord = "WUPD";
        constexpr NAME transitionFactorRecord = "TRFC";
        constexpr NAME currentWeatherRecord = "CWTH";
        constexpr NAME nextWeatherRecord = "NWTH";
        constexpr NAME queuedWeatherRecord = "QWTH";
        constexpr NAME weatherOverrideRecord = "OWTH";
        constexpr NAME identityVersionRecord = "WXVR";
        constexpr NAME weatherIdentityRecord = "WXID";
        constexpr NAME regionNameRecord = "RGNN";
        constexpr NAME regionWeatherRecord = "RGNW";
        constexpr NAME regionChanceRecord = "RGNC";
        constexpr NAME regionSelectionRecord = "RGIX";
        constexpr NAME regionFallbackRecord = "RGDF";
    }
}

namespace ESM
{
    void validateWeatherIdentityCatalog(std::span<const ESM::FormKey> identities)
    {
        if (identities.empty() || identities.size() > std::numeric_limits<int32_t>::max())
            throw std::runtime_error("Saved weather identity catalog has an invalid size");
        std::set<ESM::FormKey> seen;
        for (const auto& key : identities)
        {
            if (!key.isContent() || ESM::FormKey::deserialize(key.serialize()) != key)
                throw std::runtime_error("Saved weather identity is not a canonical content key");
            if (!seen.insert(key).second)
                throw std::runtime_error("Saved weather identity catalog contains duplicate keys");
        }
    }

    void WeatherState::load(ESMReader& esm, bool rejectDuplicateRegions)
    {
        mCurrentRegion = esm.getHNRefId(currentRegionRecord);
        esm.getHNT(mTimePassed, timePassedRecord);
        esm.getHNT(mFastForward, fastForwardRecord);
        esm.getHNT(mWeatherUpdateTime, weatherUpdateTimeRecord);
        esm.getHNT(mTransitionFactor, transitionFactorRecord);
        esm.getHNT(mCurrentWeather, currentWeatherRecord);
        esm.getHNT(mNextWeather, nextWeatherRecord);
        esm.getHNT(mQueuedWeather, queuedWeatherRecord);
        esm.getHNOT(mWeatherOverride, weatherOverrideRecord);
        mWeatherIdentities.reset();
        if (esm.isNextSub(identityVersionRecord))
        {
            uint32_t version;
            esm.getHT(version);
            if (version != 1)
                esm.fail("Unsupported saved weather identity version");
            mWeatherIdentities.emplace();
            try
            {
                while (esm.isNextSub(weatherIdentityRecord))
                    mWeatherIdentities->push_back(ESM::FormKey::deserialize(esm.getHStringView()));
                validateWeatherIdentityCatalog(*mWeatherIdentities);
            }
            catch (const std::exception& e)
            {
                esm.fail(e.what());
            }
        }
        else if (esm.isNextSub(weatherIdentityRecord))
            esm.fail("Saved weather identities require a version");

        std::set<ESM::RefId> savedRegions;
        while (esm.isNextSub(regionNameRecord))
        {
            ESM::RefId regionID = rejectDuplicateRegions ? esm.getUnmappedRefId() : esm.getRefId();
            if (rejectDuplicateRegions && !savedRegions.insert(regionID).second)
                esm.fail("Saved weather contains duplicate region identities");
            bool removed = false;
            if (rejectDuplicateRegions)
                if (const auto* form = regionID.getIf<ESM::FormId>())
                {
                    auto remapped = *form;
                    removed = !esm.applyContentFileMapping(remapped);
                    if (!removed)
                        regionID = ESM::RefId(remapped);
                }
            RegionWeatherState region;
            esm.getHNT(region.mWeather, regionWeatherRecord);
            if (esm.isNextSub(regionFallbackRecord))
            {
                if (!mWeatherIdentities)
                    esm.fail("Saved weather fallback identity requires a catalog");
                esm.getHT(region.mFallbackWeather);
            }
            std::optional<bool> explicitOrder;
            std::set<int32_t> selections;
            while (esm.isNextSub(regionChanceRecord))
            {
                uint8_t chance;
                esm.getHT(chance);
                region.mChances.push_back(chance);
                const bool selected = esm.isNextSub(regionSelectionRecord);
                if (explicitOrder && selected != *explicitOrder)
                    esm.fail("Saved weather selection order is incomplete");
                explicitOrder = selected;
                if (selected)
                {
                    if (!mWeatherIdentities)
                        esm.fail("Saved weather selection identities require a catalog");
                    int32_t index;
                    esm.getHT(index);
                    if (index < -1 || (index >= 0 && std::size_t(index) >= mWeatherIdentities->size()))
                        esm.fail("Saved weather selection index is outside saved identity catalog");
                    if (index >= 0 && !selections.insert(index).second)
                        esm.fail("Saved weather selection order contains duplicate identities");
                    region.mSelectionOrder.push_back(index);
                }
            }
            if (mWeatherIdentities && (region.mFallbackWeather < 0
                    || std::size_t(region.mFallbackWeather) >= mWeatherIdentities->size()))
                esm.fail("Saved weather fallback is outside saved identity catalog");

            if (removed)
                continue;
            const auto inserted = mRegions.emplace(regionID, std::move(region));
            if (rejectDuplicateRegions && !inserted.second)
                esm.fail("Saved weather contains duplicate region identities");
        }
    }

    void WeatherState::save(ESMWriter& esm) const
    {
        if (mWeatherIdentities)
            validateWeatherIdentityCatalog(*mWeatherIdentities);
        for (const auto& [id, region] : mRegions)
        {
            if (!region.mSelectionOrder.empty() && region.mSelectionOrder.size() != region.mChances.size())
                throw std::runtime_error("Saved weather selection order is incomplete");
            if (!mWeatherIdentities && (!region.mSelectionOrder.empty() || region.mFallbackWeather != 0))
                throw std::runtime_error("Saved weather selection identities require a catalog");
            if (mWeatherIdentities)
            {
                if (region.mFallbackWeather < 0 || std::size_t(region.mFallbackWeather) >= mWeatherIdentities->size())
                    throw std::runtime_error("Saved weather fallback is outside saved identity catalog");
                std::set<int32_t> seen;
                for (const auto index : region.mSelectionOrder)
                {
                    if (index < -1 || (index >= 0 && std::size_t(index) >= mWeatherIdentities->size()))
                        throw std::runtime_error("Saved weather selection index is outside saved identity catalog");
                    if (index >= 0 && !seen.insert(index).second)
                        throw std::runtime_error("Saved weather selection order contains duplicate identities");
                }
            }
        }
        esm.writeHNCRefId(currentRegionRecord, mCurrentRegion);
        esm.writeHNT(timePassedRecord, mTimePassed);
        esm.writeHNT(fastForwardRecord, mFastForward);
        esm.writeHNT(weatherUpdateTimeRecord, mWeatherUpdateTime);
        esm.writeHNT(transitionFactorRecord, mTransitionFactor);
        esm.writeHNT(currentWeatherRecord, mCurrentWeather);
        esm.writeHNT(nextWeatherRecord, mNextWeather);
        esm.writeHNT(queuedWeatherRecord, mQueuedWeather);
        if (mWeatherOverride)
            esm.writeHNT(weatherOverrideRecord, mWeatherOverride);
        if (mWeatherIdentities)
        {
            esm.writeHNT(identityVersionRecord, uint32_t{1});
            for (const auto& key : *mWeatherIdentities)
                esm.writeHNString(weatherIdentityRecord, key.serialize());
        }

        auto it = mRegions.begin();
        for (; it != mRegions.end(); ++it)
        {
            esm.writeHNCRefId(regionNameRecord, it->first);
            esm.writeHNT(regionWeatherRecord, it->second.mWeather);
            if (it->second.mFallbackWeather != 0)
                esm.writeHNT(regionFallbackRecord, it->second.mFallbackWeather);
            for (std::size_t i = 0; i < it->second.mChances.size(); ++i)
            {
                esm.writeHNT(regionChanceRecord, it->second.mChances[i]);
                if (!it->second.mSelectionOrder.empty())
                    esm.writeHNT(regionSelectionRecord, it->second.mSelectionOrder[i]);
            }
        }
    }
}
