#include "oblivionenablestate.hpp"
#include "worldimp.hpp"
#include "../mwmechanics/obliviondoorstate.hpp"
#include <components/esm4/referenceflags.hpp>
#include <components/esm4/loadachr.hpp>
#include <components/esm4/runtimestate.hpp>
#include <components/esm4/runtimereferences.hpp>

namespace MWWorld
{
    namespace
    {
        using ResidentFlags = std::map<ESM::FormKey, bool>;

        std::optional<OblivionEnableReference> authoredEnableReference(
            const ESMStore& store, const ESM::FormKeyResolver& resolver, const ESM::FormKey& key)
        {
            const auto read = [&](const auto* record) -> std::optional<OblivionEnableReference> {
                if (!record) return std::nullopt;
                return OblivionEnableReference{(record->mFlags & ESM4::Rec_Disabled) == 0,
                    resolver.toFormKey(record->mEsp.parent), (record->mEsp.flags & 1u) != 0};
            };
            if (auto value = read(store.get<ESM4::ActorCharacter>().searchStatic(key))) return value;
            if (auto value = read(store.get<ESM4::ActorCreature>().searchStatic(key))) return value;
            return read(store.get<ESM4::Reference>().searchStatic(key));
        }

        std::optional<bool> enabled(const ESMStore& store, const ESM::FormKeyResolver& resolver,
            const ESM::FormKey& key, const MWMechanics::OblivionSavedReferenceIndex* saved,
            const WorldModel* model, const ResidentFlags* residents = nullptr)
        {
            return resolveOblivionEnableState(key, [&](const ESM::FormKey& current)
                -> std::optional<OblivionEnableReference> {
                auto node = authoredEnableReference(store, resolver, current);
                const auto* snapshot = saved ? saved->find(current) : nullptr;
                if (!node && snapshot) node.emplace(OblivionEnableReference{snapshot->mEnabled, {}, false});
                if (snapshot) node->mEnabled = snapshot->mEnabled;
                if (model)
                {
                    Ptr ptr;
                    if (current.isDynamic()) ptr = model->getDynamicNativePtr(current);
                    else if (const auto id = resolver.toFormId(current)) ptr = model->getPtr(*id);
                    if (!ptr.isEmpty())
                    {
                        if (!node) node.emplace();
                        node->mEnabled = ptr.getRefData().isEnabled();
                    }
                }
                if (residents)
                    if (const auto found = residents->find(current); found != residents->end())
                    {
                        if (!node) node.emplace();
                        node->mEnabled = found->second;
                    }
                return node;
            });
        }
    }

    std::optional<bool> World::getOblivionReferenceEnabled(const ESM::FormKey& key,
        const MWMechanics::OblivionSavedReferenceIndex* references) const
    {
        if (mGameProfile != ESM::GameProfile::Oblivion) return std::nullopt;
        const ESM::FormKeyResolver resolver(mContentFiles);
        std::optional<MWMechanics::OblivionSavedReferenceIndex> owned;
        if (!references && mOblivionRuntimeState)
        {
            owned.emplace(std::span<const ESM4::RuntimeReferenceState>(mOblivionRuntimeState->mReferences));
            references = &*owned;
        }
        return enabled(mStore, resolver, ESM4::runtimeReferenceKey(key), references, &mWorldModel);
    }

    void World::normalizeOblivionEnableParents(ESM4::RuntimeState& state) const
    {
        const ESM::FormKeyResolver resolver(mContentFiles);
        const MWMechanics::OblivionSavedReferenceIndex references(state.mReferences);
        std::vector<std::pair<ESM4::RuntimeReferenceState*, bool>> changes;
        for (auto& reference : state.mReferences)
            if (const auto node = authoredEnableReference(mStore, resolver, reference.mKey);
                node && !node->mParent.isNull())
                changes.emplace_back(&reference,
                    *enabled(mStore, resolver, reference.mKey, &references, nullptr));
        // Resolve the whole batch before publication, including cycle checks.
        for (const auto& [reference, value] : changes) reference->mEnabled = value;
    }

    void World::synchronizeOblivionEnableParents(CellStore* loadingCell)
    {
        if (mGameProfile != ESM::GameProfile::Oblivion || mSynchronizingOblivionEnableParents) return;
        mSynchronizingOblivionEnableParents = true;
        struct Reset { bool& flag; ~Reset() { flag = false; } } reset{mSynchronizingOblivionEnableParents};
        const ESM::FormKeyResolver resolver(mContentFiles);
        auto residents = mWorldModel.getResidentPtrs();
        if (loadingCell)
            loadingCell->forEach([&](const Ptr& ptr) { residents.push_back(ptr); return true; }, true);
        ResidentFlags flags;
        std::map<ESM::FormKey, Ptr> pointers;
        for (const auto& ptr : residents)
        {
            const auto key = ptr.getCellRef().getFormKey();
            if (!key.isNull()) { flags[key] = ptr.getRefData().isEnabled(); pointers[key] = ptr; }
        }
        std::optional<MWMechanics::OblivionSavedReferenceIndex> references;
        if (mOblivionRuntimeState) references.emplace(mOblivionRuntimeState->mReferences);
        const auto* index = references ? &*references : nullptr;
        std::vector<std::pair<Ptr, bool>> changes;
        for (const auto& [key, ptr] : pointers)
            if (const auto node = authoredEnableReference(mStore, resolver, key);
                node && !node->mParent.isNull())
                changes.emplace_back(ptr, *enabled(mStore, resolver, key, index, &mWorldModel, &flags));
        for (const auto& [ptr, value] : changes)
        {
            if (ptr.getRefData().isEnabled() == value) continue;
            if (!mWorldScene || (loadingCell && ptr.getCell() == loadingCell))
                value ? ptr.getRefData().enable() : ptr.getRefData().disable();
            else if (value) enable(ptr);
            else disable(ptr);
            Log(Debug::Info) << "M15 enable-parent ref=" << ptr.getCellRef().getFormKey().serialize()
                << " enabled=" << (value ? "true" : "false");
        }
        if (mOblivionRuntimeState)
        {
            // Root edits and linked children share the same saved reference flags.
            for (auto& reference : mOblivionRuntimeState->mReferences)
                if (const auto ptr = pointers.find(reference.mKey); ptr != pointers.end())
                    reference.mEnabled = ptr->second.getRefData().isEnabled();
            normalizeOblivionEnableParents(*mOblivionRuntimeState);
        }
    }
}
