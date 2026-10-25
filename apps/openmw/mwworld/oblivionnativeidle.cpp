#include "oblivionnativeidle.hpp"

#include "esmstore.hpp"

#include <components/esm4/common.hpp>
#include <components/esm4/loadidle.hpp>

#include <algorithm>
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
            if (history.size() != 1 || history.front().mDeleted || history.front().mRecordType != ESM4::REC_IDLE
                || !key.isContent() || ESM::normalizePluginName(history.front().mWinningPlugin) != key.mNamespace)
                throw std::invalid_argument("native idle registry requires original single-definition content history");
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
            if (!history || history->size() != 1 || history->front().mRecordType != ESM4::REC_IDLE)
                throw std::invalid_argument("native idle registry requires matching content provenance");
            order.push_back(&record);
        }
        if (order.size() != indexed)
            throw std::invalid_argument("native idle registry content index disagrees with typed definitions");
        return ESM4::registerNativeIdleRecords(order);
    }
}
