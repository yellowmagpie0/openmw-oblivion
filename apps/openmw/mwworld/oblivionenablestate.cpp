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

        bool hasEnableParent(const CellRef& ref)
        {
            if (const auto* placed = ref.getNativeReference())
                return !placed->mEsp.parent.isZeroOrUnset();
            if (const auto* actor = ref.getNativeActor())
                return !actor->mEsp.parent.isZeroOrUnset();
            return false;
        }

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
            if (const Ptr resident = mWorldModel.getPtr(record->mId); !resident.isEmpty())
                return resident.getRefData().isEnabled();
            bool value = (record->mFlags & ESM4::Rec_Disabled) == 0;
            const auto* snapshot = references ? references->find(referenceKey) : findOblivionSavedReference(referenceKey);
            if (snapshot) value = snapshot->mEnabled;
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

    void World::propagateOblivionEnableChange(const ESM::FormKey& root, bool value)
    {
        if (mGameProfile != ESM::GameProfile::Oblivion || mSynchronizingOblivionEnableParents || root.isNull())
            return;
        const ESM::FormKeyResolver resolver(mContentFiles);
        if (const auto node = authoredEnableReference(mStore, resolver, root); node && !node->mParent.isNull())
        {
            // A direct edit to a linked child must still inherit its parent's state.
            synchronizeOblivionEnableParents();
            return;
        }
        if (!mOblivionEnableChildren)
        {
            EnableChildren children;
            const auto index = [&](const auto& records) {
                for (const auto& record : records)
                    if (!record.mEsp.parent.isZeroOrUnset())
                        children[resolver.toFormKey(record.mEsp.parent)].emplace_back(
                            record.mFormKey, (record.mEsp.flags & 1u) != 0);
            };
            index(mStore.get<ESM4::Reference>());
            index(mStore.get<ESM4::ActorCharacter>());
            index(mStore.get<ESM4::ActorCreature>());
            mOblivionEnableChildren.emplace(std::move(children));
        }
        if (!mOblivionEnableChildren->contains(root))
        {
            if (auto* saved = findOblivionSavedReference(root))
                saved->mEnabled = value;
            return;
        }

        // Every placed child has at most one parent. A tree reached from a root
        // can therefore be evaluated in a single pass, including inverted links.
        std::vector<std::pair<ESM::FormKey, bool>> pending{ { root, value } };
        std::map<ESM::FormKey, bool> flags;
        for (std::size_t i = 0; i < pending.size(); ++i)
        {
            const auto [key, parentEnabled] = pending[i];
            if (!flags.emplace(key, parentEnabled).second)
                continue;
            const auto children = mOblivionEnableChildren->find(key);
            if (children != mOblivionEnableChildren->end())
                for (const auto& [child, inverted] : children->second)
                    pending.emplace_back(child, parentEnabled != inverted);
        }
        mSynchronizingOblivionEnableParents = true;
        struct Reset { bool& flag; ~Reset() { flag = false; } } reset{mSynchronizingOblivionEnableParents};
        for (const auto& ptr : mWorldModel.getResidentPtrs())
        {
            const auto& key = ptr.getCellRef().getFormKey();
            const auto it = flags.find(key);
            if (it == flags.end() || key == root || ptr.getRefData().isEnabled() == it->second)
                continue;
            if (!mWorldScene)
                it->second ? ptr.getRefData().enable() : ptr.getRefData().disable();
            else if (it->second)
                enable(ptr);
            else
                disable(ptr);
            Log(Debug::Info) << "M15 enable-parent ref=" << key.serialize() << " enabled=" << (it->second ? "true" : "false");
        }
        // Some descendants have never been saved. A single pass also handles
        // unsorted runtime additions without an exhaustive search per miss.
        if (mOblivionRuntimeState)
            for (auto& saved : mOblivionRuntimeState->mReferences)
                if (const auto it = flags.find(saved.mKey); it != flags.end())
                    saved.mEnabled = it->second;
    }

    void World::synchronizeOblivionEnableParents(CellStore* loadingCell)
    {
        if (mGameProfile != ESM::GameProfile::Oblivion || mSynchronizingOblivionEnableParents) return;
        if (loadingCell)
        {
            // Streaming a cell with only roots at their already-authoritative
            // values introduces no changes to any enable-parent chain.
            bool needsSync = false;
            loadingCell->forEach([&](const Ptr& ptr) {
                const auto& ref = ptr.getCellRef();
                const auto* placed = ref.getNativeReference();
                const auto* actor = ref.getNativeActor();
                if (!placed && !actor) return true;
                const auto* snapshot = findOblivionSavedReference(placed ? placed->mFormKey : actor->mFormKey);
                const bool expected = snapshot ? snapshot->mEnabled
                    : ((placed ? placed->mFlags : actor->mFlags) & ESM4::Rec_Disabled) == 0;
                needsSync = hasEnableParent(ref) || ptr.getRefData().isEnabled() != expected;
                return !needsSync;
            }, true);
            if (!needsSync) return;
        }
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
            if (hasEnableParent(ptr.getCellRef()))
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
