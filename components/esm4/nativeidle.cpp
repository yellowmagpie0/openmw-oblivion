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
