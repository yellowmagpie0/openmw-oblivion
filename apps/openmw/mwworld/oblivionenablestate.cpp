#include "oblivionenablestate.hpp"
#include "worldimp.hpp"
#include "../mwmechanics/obliviondoorstate.hpp"
#include <algorithm>
#include <span>
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
            const WorldModel* model, const ResidentFlags* residents = nullptr,
            std::span<const ESM4::RuntimeReferenceState> source = {})
        {
            return resolveOblivionEnableState(key, [&](const ESM::FormKey& current)
                -> std::optional<OblivionEnableReference> {
                auto node = authoredEnableReference(store, resolver, current);
                const auto* snapshot = saved ? saved->find(current) : nullptr;
                if (!saved)
                {
                    const auto found = std::find_if(source.begin(), source.end(),
                        [&](const auto& reference) { return reference.mKey == current; });
                    if (found != source.end()) snapshot = &*found;
                }
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
        std::span<const ESM4::RuntimeReferenceState> source;
        if (mOblivionRuntimeState)
        {
            source = mOblivionRuntimeState->mReferences;
            // Package actions can insert saved references during a low-process
            // pass. Never dereference a borrowed index after vector growth.
            if (references && !references->matches(source)) references = nullptr;
        }
        else
            references = nullptr;
        const auto referenceKey = ESM4::runtimeReferenceKey(key);
        // Most offscreen actors have no enable parent. Read these roots using
        // the record's already resolved runtime ID: constructing a load-order
        // resolver and a cycle-detection set for every actor every frame is
        // unnecessary. Read live/saved flags each time, so package actions and
        // resident changes remain visible within the same low-process pass.
        const auto rootEnabled = [&](const auto* record) -> std::optional<bool> {
            if (!record || !record->mEsp.parent.isZeroOrUnset()) return std::nullopt;
            bool value = (record->mFlags & ESM4::Rec_Disabled) == 0;
            const auto* snapshot = references ? references->find(referenceKey) : nullptr;
            if (!references)
            {
                const auto found = std::find_if(source.begin(), source.end(),
                    [&](const auto& reference) { return reference.mKey == referenceKey; });
                if (found != source.end()) snapshot = &*found;
            }
            if (snapshot) value = snapshot->mEnabled;
            if (const Ptr resident = mWorldModel.getPtr(record->mId); !resident.isEmpty())
                value = resident.getRefData().isEnabled();
            return value;
        };
        if (auto value = rootEnabled(mStore.get<ESM4::ActorCharacter>().searchStatic(referenceKey))) return value;
        if (auto value = rootEnabled(mStore.get<ESM4::ActorCreature>().searchStatic(referenceKey))) return value;
        if (auto value = rootEnabled(mStore.get<ESM4::Reference>().searchStatic(referenceKey))) return value;
        const ESM::FormKeyResolver resolver(mContentFiles);
        // A single GetDisabled query must not allocate and sort an index of
        // the entire saved world. Batch AI callers supply their shared index;
        // standalone queries inspect only the requested enable-parent chain.
        return enabled(mStore, resolver, referenceKey, references, &mWorldModel, nullptr, source);
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
