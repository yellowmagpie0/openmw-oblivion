#include "oblivionnativeidle.hpp"

#include "esmstore.hpp"

#include <components/esm4/common.hpp>
#include <components/esm4/loadidle.hpp>

#include <algorithm>
#include <map>
#include <stdexcept>

namespace MWWorld
{
    ESM4::NativeIdleRegistrationIndex buildOblivionNativeIdleIndex(const ESMStore& store)
    {
        std::size_t indexed = 0;
        for (const auto& [key, history] : store.getFormKeyIndex().records())
        {
            if (std::none_of(history.begin(), history.end(),
                    [](const auto& entry) { return entry.mRecordType == ESM4::REC_IDLE; }))
                continue;
            if (!key.isContent() || ESM::normalizePluginName(history.front().mWinningPlugin) != key.mNamespace
                || std::any_of(history.begin(), history.end(), [](const auto& entry) {
                       return entry.mDeleted || entry.mRecordType != ESM4::REC_IDLE;
                   }))
                throw std::invalid_argument("native idle registry requires live same-type content history");
            ++indexed;
        }
        const auto& records = store.get<ESM4::IdleAnimation>();
        std::vector<const ESM4::IdleAnimation*> order;
        order.reserve(records.getSize());
        for (const auto& record : records)
        {
            const ESM::RefId id(record.mId);
            const auto key = records.findFormKey(id);
            if (!key || records.isDynamic(id) || records.searchStatic(*key) != &record)
                throw std::invalid_argument("native idle registry requires indexed static definitions");
            const auto* history = store.getFormKeyIndex().history(*key);
            if (!history || history->empty() || history->back().mRecordType != ESM4::REC_IDLE)
                throw std::invalid_argument("native idle registry requires matching content provenance");
            order.push_back(&record);
        }
        if (order.size() != indexed)
            throw std::invalid_argument("native idle registry content index disagrees with typed definitions");
        std::map<ESM::FormKey, std::size_t> consumed;
        std::vector<ESM4::NativeIdleRegistrationEvent> events;
        for (const auto& event : store.getOblivionNativeIdleLoadEvents())
        {
            const auto& metadata = event.mMetadata;
            const auto* history = store.getFormKeyIndex().history(metadata.mKey);
            const auto revision = consumed[metadata.mKey]++;
            const auto* record = records.searchStatic(metadata.mKey);
            if (!event.mIsTES4 || !history || revision >= history->size() || (*history)[revision] != metadata
                || !record || record->mId != event.mRegistration.mId
                || metadata.mDeleted || (event.mRegistration.mFlags & (ESM4::Rec_Deleted | 0x4000)))
                throw std::invalid_argument("native idle registry load event disagrees with content provenance");
            events.push_back(event.mRegistration);
        }
        for (const auto& [key, history] : store.getFormKeyIndex().records())
            if (std::any_of(history.begin(), history.end(),
                    [](const auto& entry) { return entry.mRecordType == ESM4::REC_IDLE; })
                && consumed[key] != history.size())
                throw std::invalid_argument("native idle registry is missing content load events");
        return ESM4::replayNativeIdleRegistration(order, events);
    }
}
