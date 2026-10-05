#include "worldimp.hpp"
#include <components/esm4/runtimestate.hpp>
#include <limits>
#include <stdexcept>

namespace MWWorld
{
    World::PreparedOblivionDynamicReferenceKey::PreparedOblivionDynamicReferenceKey(
        World& owner, ESM::FormKey key,
        std::unique_ptr<WorldModel::PreparedPtrReplacement> registry)
        : mOwner(&owner), mIdentity(owner.mOblivionDynamicReferenceIdentity),
          mKey(std::move(key)), mRegistry(std::move(registry)) {}

    bool World::PreparedOblivionDynamicReferenceKey::isValid() const noexcept
    {
        const auto identity = mIdentity.lock();
        // Never touch the borrowed World after destruction or address reuse.
        return identity && !mCommitted
            && identity == mOwner->mOblivionDynamicReferenceIdentity
            && mOwner->mNextOblivionDynamicSerial == mKey.mValue
            && mRegistry->isValid();
    }

    bool World::PreparedOblivionDynamicReferenceKey::commit() noexcept
    {
        if (!isValid()) return false;
        ++mOwner->mNextOblivionDynamicSerial;
        mCommitted = true;
        return true;
    }

    std::unique_ptr<World::PreparedOblivionDynamicReferenceKey>
    World::prepareOblivionDynamicReferenceKey()
    {
        if (mGameProfile != ESM::GameProfile::Oblivion)
            throw std::invalid_argument("native reference identity requires the Oblivion profile");
        if (mNextOblivionDynamicSerial == 0
            || mNextOblivionDynamicSerial == std::numeric_limits<std::uint64_t>::max())
            throw std::overflow_error("native reference identity namespace is exhausted");
        const auto key = ESM::FormKey::dynamic("native-reference", mNextOblivionDynamicSerial);
        const auto check = [&](const ESM::FormKey& existing) {
            if (existing.isDynamic() && existing.mNamespace == key.mNamespace
                && existing.mValue >= key.mValue)
                throw std::invalid_argument("native reference serial would reuse an existing identity");
        };
        for (const auto& ptr : mWorldModel.getResidentPtrs())
            check(ptr.getCellRef().getFormKey());
        if (mOblivionRuntimeState)
            for (const auto& reference : mOblivionRuntimeState->mReferences)
                check(reference.mKey);
        auto registry = std::make_unique<WorldModel::PreparedPtrReplacement>(
            mWorldModel.preparePtrReplacement({}, {}));
        if (!mOblivionDynamicReferenceIdentity)
            mOblivionDynamicReferenceIdentity = std::make_shared<const char>();
        return std::unique_ptr<PreparedOblivionDynamicReferenceKey>(
            new PreparedOblivionDynamicReferenceKey(*this, key, std::move(registry)));
    }
}
