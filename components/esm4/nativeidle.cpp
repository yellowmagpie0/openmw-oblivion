#include "nativeidle.hpp"

#include "common.hpp"
#include "loadidle.hpp"

#include <algorithm>
#include <set>
#include <stdexcept>
#include <utility>

namespace ESM4
{
    namespace
    {
        std::string_view asciiCString(std::string_view value)
        {
            value = value.substr(0, value.find('\0'));
            if (std::any_of(value.begin(), value.end(),
                    [](unsigned char c) { return c >= 128; }))
                throw std::invalid_argument("native idle path requires an ASCII C string");
            return value;
        }

        std::string_view modelPath(std::string_view value)
        {
            value = asciiCString(value);
            if (value.size() >= 260)
                throw std::invalid_argument("native idle model path must be shorter than 260 bytes");
            return value;
        }

        std::string foldedKey(std::string_view key)
        {
            std::string result(asciiCString(key));
            for (auto& c : result)
                if (c >= 'A' && c <= 'Z')
                    c += 'a' - 'A';
            return result;
        }
    }

    std::string nativeIdleModelKey(std::string_view model)
    {
        model = modelPath(model);
        constexpr std::string_view directory = "IdleAnims";
        if (const auto found = model.find(directory); found != std::string_view::npos)
            return std::string(model.substr(0, found + directory.size()));
        if (const auto found = model.rfind('\\'); found != std::string_view::npos)
            return std::string(model.substr(0, found + 1)) + std::string(directory);
        return {};
    }

    bool nativeIdleHasFile(std::string_view model)
    {
        model = modelPath(model);
        const auto dot = model.rfind('.');
        return dot != std::string_view::npos && foldedKey(model.substr(dot)) == ".kf";
    }

    NativeIdleRegistrationEvent nativeIdleRegistrationEvent(const IdleAnimation& record)
    {
        return {record.mId, record.mParent, record.mPrevious, std::string(record.mModel.getOriginal()), record.mFlags};
    }

    NativeIdleRegistrationIndex replayNativeIdleRegistration(std::span<const IdleAnimation* const> records,
        std::span<const NativeIdleRegistrationEvent> events)
    {
        NativeIdleRegistrationIndex result;
        result.mNodes.reserve(records.size());
        std::map<ESM::FormId, std::size_t> identities;
        for (const auto* record : records)
        {
            if (!record || record->mId.mIndex == 0 || !identities.emplace(record->mId, result.mNodes.size()).second)
                throw std::invalid_argument("native idle replay requires distinct winning typed identities");
            result.mNodes.push_back({record, {}});
        }
        std::vector<bool> defined(records.size());
        std::vector<std::optional<std::size_t>> rootBanks(records.size());
        std::map<std::string, std::size_t> banks;
        const auto resolve = [&](ESM::FormId id) -> std::optional<std::size_t> {
            if (id.mIndex == 0)
                return {};
            const auto found = identities.find(id);
            if (found == identities.end() || !defined[found->second])
                throw std::invalid_argument("native idle replay requires earlier parent/predecessor definitions");
            return found->second;
        };
        for (const auto& event : events)
        {
            const auto found = identities.find(event.mId);
            if (event.mId.mIndex == 0 || found == identities.end())
                throw std::invalid_argument("native idle replay event has no winning typed identity");
            const auto index = found->second;
            auto& node = result.mNodes[index];
            if (defined[index])
            {
                // Original44DCF0 dispatches reset/init before an ordinary
                // override. The serialized Deleted/special-flag lifecycle
                // cannot be reconstructed by pretending it takes this path.
                if (event.mFlags & (Rec_Deleted | 0x4000))
                    throw std::invalid_argument("native idle replay requires ordinary override flags");
                auto& siblings = node.mParent ? result.mNodes[*node.mParent].mChildren
                                              : result.mBanks.at(rootBanks[index].value()).mRoots;
                const auto position = std::find(siblings.begin(), siblings.end(), index);
                if (position == siblings.end())
                    throw std::invalid_argument("native idle replay lost its previous registration");
                if (position + 1 != siblings.end())
                    result.mNodes[*(position + 1)].mPrevious = node.mPrevious;
                siblings.erase(position);
                node.mParent.reset();
                node.mPrevious.reset();
                rootBanks[index].reset();
            }
            const auto parent = resolve(event.mParent);
            const auto previous = resolve(event.mPrevious);
            node.mParent = parent;
            std::vector<std::size_t>* siblings;
            if (parent)
                siblings = &result.mNodes[*parent].mChildren;
            else
            {
                auto key = nativeIdleModelKey(event.mModel);
                if (key.empty())
                    throw std::invalid_argument("native idle registration requires a resolved nonempty root model bank");
                const auto [bank, inserted] = banks.emplace(foldedKey(key), result.mBanks.size());
                if (inserted)
                    result.mBanks.push_back({std::move(key), {}});
                rootBanks[index] = bank->second;
                siblings = &result.mBanks[bank->second].mRoots;
            }
            // Actual520B2F dispatch: null predecessor starts a live chain at
            // the front; Deleted starts at the end. A known predecessor in
            // another array is absent from this one and also appends.
            auto position = previous || (event.mFlags & Rec_Deleted) ? siblings->size() : 0;
            if (previous)
                if (const auto sibling = std::find(siblings->begin(), siblings->end(), *previous);
                    sibling != siblings->end())
                    position = static_cast<std::size_t>(sibling - siblings->begin()) + 1;
            node.mPrevious = position == 0 ? std::nullopt : std::optional((*siblings)[position - 1]);
            if (position != siblings->size())
                result.mNodes[(*siblings)[position]].mPrevious = index;
            siblings->insert(siblings->begin() + position, index);
            defined[index] = true;
        }
        if (std::find(defined.begin(), defined.end(), false) != defined.end())
            throw std::invalid_argument("native idle replay is missing a winning record's load events");
        // Overrides can move an ancestor beneath its descendant. Validate the
        // complete finite forest before handing it to any actor or selector.
        NativeIdleSelector validate(result.mNodes, result.mBanks);
        return result;
    }

    NativeIdleRegistrationIndex registerNativeIdleRecords(std::span<const IdleAnimation* const> records)
    {
        std::vector<NativeIdleRegistrationEvent> events;
        events.reserve(records.size());
        for (const auto* record : records)
        {
            if (!record)
                throw std::invalid_argument("native idle registration requires a typed record");
            events.push_back(nativeIdleRegistrationEvent(*record));
        }
        return replayNativeIdleRegistration(records, events);
    }

    NativeIdleSelector::NativeIdleSelector(
        std::vector<NativeIdleNode> nodes, std::span<const NativeIdleModelBank> banks)
        : mNodes(std::move(nodes))
    {
        std::set<ESM::FormId> identities;
        std::vector<unsigned> parents(mNodes.size());
        for (const auto& node : mNodes)
        {
            if (!node.mRecord || node.mRecord->mId.mIndex == 0 || !identities.insert(node.mRecord->mId).second)
                throw std::invalid_argument("native idle forest requires distinct typed records");
            for (const auto child : node.mChildren)
                if (child >= mNodes.size() || ++parents[child] != 1)
                    throw std::invalid_argument("native idle forest has an invalid or shared child");
        }
        const auto incoming = parents;
        std::vector<std::size_t> ready;
        for (std::size_t i = 0; i < parents.size(); ++i)
            if (parents[i] == 0)
                ready.push_back(i);
        std::size_t visited = 0;
        while (!ready.empty())
        {
            const auto index = ready.back();
            ready.pop_back();
            ++visited;
            for (const auto child : mNodes[index].mChildren)
                if (--parents[child] == 0)
                    ready.push_back(child);
        }
        if (visited != mNodes.size())
            throw std::invalid_argument("native idle forest contains a cycle");
        for (const auto& bank : banks)
        {
            std::set<std::size_t> roots;
            for (const auto root : bank.mRoots)
                if (root >= mNodes.size() || incoming[root] != 0 || !roots.insert(root).second)
                    throw std::invalid_argument("native idle bank has an invalid or duplicate root");
            if (!mBanks.emplace(foldedKey(bank.mKey), bank.mRoots).second)
                throw std::invalid_argument("native idle bank has duplicate case-insensitive keys");
        }
    }

    const IdleAnimation* NativeIdleSelector::select(std::string_view actorModel,
        const std::function<bool(const IdleAnimation&)>& matches) const
    {
        const auto key = nativeIdleModelKey(actorModel);
        if (key.empty())
            return nullptr;
        const auto bank = mBanks.find(foldedKey(key));
        if (bank == mBanks.end())
            return nullptr;
        struct Pending
        {
            std::size_t mIndex;
            bool mFallback;
        };
        std::vector<Pending> pending;
        const auto append = [&](const auto& indices) {
            for (auto i = indices.rbegin(); i != indices.rend(); ++i)
                pending.push_back({*i, false});
        };
        append(bank->second);
        while (!pending.empty())
        {
            const auto current = pending.back();
            pending.pop_back();
            const auto& node = mNodes[current.mIndex];
            const auto& record = *node.mRecord;
            if (current.mFallback)
                return !(record.mAnimationGroup & 0x80) || nativeIdleHasFile(record.mModel.getOriginal())
                    ? &record : nullptr;
            if ((record.mFlags & Rec_Deleted) || !matches(record))
                continue;
            // Consume the parent's file query before children, unless the
            // optional bit bypasses it. ANAM's low seven bits do not filter.
            if ((record.mAnimationGroup & 0x80) || nativeIdleHasFile(record.mModel.getOriginal()))
                pending.push_back({current.mIndex, true});
            append(node.mChildren);
        }
        return nullptr;
    }
}
