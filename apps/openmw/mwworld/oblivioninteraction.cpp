#include "oblivioninteraction.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

#include <components/debug/debuglog.hpp>
#include <components/esm/formkey.hpp>
#include <components/esm4/loadammo.hpp>
#include <components/esm4/loadalch.hpp>
#include <components/esm4/loadarmo.hpp>
#include <components/esm4/loadbook.hpp>
#include <components/esm4/loadclot.hpp>
#include <components/esm4/loadcont.hpp>
#include <components/esm4/loadflor.hpp>
#include <components/esm4/loadingr.hpp>
#include <components/esm4/loadkeym.hpp>
#include <components/esm4/loadmisc.hpp>
#include <components/esm4/loadweap.hpp>
#include <components/esm4/runtimestate.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

#include "class.hpp"
#include "worldimp.hpp"

namespace MWWorld
{
    namespace
    {
        template <class T>
        std::string findName(const ESMStore& store, const ESM::RefId& id)
        {
            if (const T* value = store.get<T>().search(id))
                return value->mFullName;
            return {};
        }

        std::string getOblivionItemName(const ESMStore& store, const ESM::RefId& id)
        {
            std::string result;
#define OPENMW_FIND_TES4_NAME(Type) \
    if (result.empty()) \
        result = findName<ESM4::Type>(store, id)
            OPENMW_FIND_TES4_NAME(Ammunition);
            OPENMW_FIND_TES4_NAME(Armor);
            OPENMW_FIND_TES4_NAME(Book);
            OPENMW_FIND_TES4_NAME(Clothing);
            OPENMW_FIND_TES4_NAME(Ingredient);
            OPENMW_FIND_TES4_NAME(Key);
            OPENMW_FIND_TES4_NAME(MiscItem);
            OPENMW_FIND_TES4_NAME(Potion);
            OPENMW_FIND_TES4_NAME(Weapon);
#undef OPENMW_FIND_TES4_NAME
            return result.empty() ? id.toDebugString() : result;
        }

        void addInventoryItem(
            std::vector<ESM4::RuntimeInventoryItem>& inventory, const ESM::FormKey& base, std::int32_t count)
        {
            if (base.isNull() || count == 0)
                return;
            const auto found = std::find_if(inventory.begin(), inventory.end(),
                [&](const ESM4::RuntimeInventoryItem& item) { return item.mBase == base; });
            if (found == inventory.end())
                inventory.push_back({ base, count });
            else
                found->mCount += count;
            std::erase_if(inventory, [](const ESM4::RuntimeInventoryItem& item) { return item.mCount == 0; });
            std::sort(inventory.begin(), inventory.end(),
                [](const auto& left, const auto& right) { return left.mBase < right.mBase; });
        }

        std::string plainBookText(std::string_view input)
        {
            std::string result;
            result.reserve(std::min<std::size_t>(input.size(), 700));
            bool inTag = false;
            for (const char value : input)
            {
                if (value == '<')
                    inTag = true;
                else if (value == '>')
                    inTag = false;
                else if (!inTag && result.size() < 700)
                    result.push_back(value == '\r' ? '\n' : value);
            }
            while (!result.empty() && std::isspace(static_cast<unsigned char>(result.back())))
                result.pop_back();
            return result;
        }

        const char* kindName(OblivionInteractionKind kind)
        {
            switch (kind)
            {
                case OblivionInteractionKind::Activator:
                    return "activate";
                case OblivionInteractionKind::Book:
                    return "read";
                case OblivionInteractionKind::Container:
                    return "loot";
                case OblivionInteractionKind::Door:
                    return "door";
                case OblivionInteractionKind::Flora:
                    return "harvest";
                case OblivionInteractionKind::Take:
                    return "take";
            }
            return "unknown";
        }
    }

    OblivionInteractionAction::OblivionInteractionAction(const Ptr& target, OblivionInteractionKind kind)
        : Action(false, target)
        , mKind(kind)
    {
    }

    void OblivionInteractionAction::executeImp(const Ptr& actor)
    {
        if (actor != MWBase::Environment::get().getWorld()->getPlayerPtr())
            return;

        // World is the sole MWBase::World implementation in the game executable.
        // Keeping the profile API off MWBase::World avoids exposing a temporary
        // M5 interaction vocabulary to unrelated tools and test doubles.
        static_cast<MWWorld::World*>(static_cast<MWBase::World*>(MWBase::Environment::get().getWorld()))
            ->interactWithOblivionReference(getTarget(), mKind, actor);
    }

    bool World::oblivionPlayerHasItem(const ESM::RefId& id)
    {
        if (mGameProfile != ESM::GameProfile::Oblivion)
            return false;
        const ESM::FormId* formId = id.getIf<ESM::FormId>();
        if (formId == nullptr)
            return false;
        if (!mOblivionRuntimeState)
            mOblivionRuntimeState = std::make_unique<ESM4::RuntimeState>(captureOblivionRuntimeState());
        const ESM::FormKey key = ESM::FormKeyResolver(mContentFiles).toFormKey(*formId);
        return std::any_of(mOblivionRuntimeState->mPlayer.mInventory.begin(),
            mOblivionRuntimeState->mPlayer.mInventory.end(),
            [&](const ESM4::RuntimeInventoryItem& item) { return item.mBase == key && item.mCount > 0; });
    }

    void World::interactWithOblivionReference(const Ptr& ptr, OblivionInteractionKind kind, const Ptr& actor)
    {
        if (mGameProfile != ESM::GameProfile::Oblivion || ptr.isEmpty())
            return;

        if (dispatchOblivionActivation(ptr, actor))
            return;

        if (kind == OblivionInteractionKind::Door)
        {
            activateOblivionReferenceDefault(ptr, actor);
            return;
        }

        if (!mOblivionRuntimeState)
            mOblivionRuntimeState = std::make_unique<ESM4::RuntimeState>(captureOblivionRuntimeState());

        const ESM::FormKey key = ptr.getCellRef().getFormKey();
        const auto found = std::find_if(mOblivionRuntimeState->mReferences.begin(),
            mOblivionRuntimeState->mReferences.end(),
            [&](const ESM4::RuntimeReferenceState& state) { return state.mKey == key; });
        if (found == mOblivionRuntimeState->mReferences.end())
            throw std::runtime_error("M5 interaction reference is absent from native TES4 state: " + key.serialize());

        ESM4::RuntimeReferenceState& state = *found;
        auto report = [&](std::string_view result) {
            const Ptr player = getPlayerPtr();
            const ESM::Position& playerPosition = player.getRefData().getPosition();
            Log(Debug::Info) << "M5 interaction: kind=" << kindName(kind) << " result=" << result
                             << " ref=" << key.serialize() << " base=" << state.mBase.serialize()
                             << " grounded=" << (isOnGround(player) ? "true" : "false")
                             << " player_z=" << playerPosition.pos[2];
        };
        auto message = [&](std::string value) {
            MWBase::Environment::get().getWindowManager()->messageBox(value);
        };

        const bool isPlaceholderCreature = ptr.getClass().getType() == ESM::REC_CREA4;
        const bool isOwned = !isPlaceholderCreature && !ptr.getCellRef().getOwner().empty();
        if (isOwned && kind != OblivionInteractionKind::Activator && kind != OblivionInteractionKind::Book)
        {
            state.mCustomState["ownership_checked"] = true;
            message("Owned: interaction blocked");
            report("owned");
            return;
        }

        if (kind == OblivionInteractionKind::Container && !isPlaceholderCreature && ptr.getCellRef().isLocked())
        {
            state.mCustomState["lock_checked"] = true;
            message("Locked (level " + std::to_string(std::max(0, ptr.getCellRef().getLockLevel())) + ")");
            report("locked");
            return;
        }

        const ESM::FormKeyResolver resolver(mContentFiles);
        switch (kind)
        {
            case OblivionInteractionKind::Take:
            {
                const int count = std::max(1, ptr.getCellRef().getCount());
                addInventoryItem(mOblivionRuntimeState->mPlayer.mInventory, state.mBase, count);
                const std::string name(ptr.getClass().getName(ptr));
                state.mCustomState["taken"] = true;
                deleteObject(ptr);
                message("Taken: " + name + (count > 1 ? " (" + std::to_string(count) + ")" : ""));
                report("taken");
                break;
            }
            case OblivionInteractionKind::Container:
            {
                if (state.mInventory.empty())
                {
                    message("Empty");
                    report("empty");
                    break;
                }
                std::ostringstream summary;
                summary << "Looted:";
                int lines = 0;
                for (const ESM4::RuntimeInventoryItem& item : state.mInventory)
                {
                    addInventoryItem(mOblivionRuntimeState->mPlayer.mInventory, item.mBase, item.mCount);
                    if (lines++ < 8)
                    {
                        const std::optional<ESM::FormId> itemId = resolver.toFormId(item.mBase);
                        summary << "\n" << item.mCount << " x "
                                << (itemId ? getOblivionItemName(mStore, ESM::RefId(*itemId))
                                           : item.mBase.serialize());
                    }
                }
                state.mInventory.clear();
                state.mCustomState["opened"] = true;
                message(summary.str());
                report("looted");
                break;
            }
            case OblivionInteractionKind::Door:
                break;
            case OblivionInteractionKind::Book:
            {
                const ESM4::Book* book = ptr.get<ESM4::Book>()->mBase;
                state.mCustomState["read"] = true;
                std::string text = plainBookText(book->mText);
                message(book->mFullName + (text.empty() ? std::string{} : "\n\n" + text));
                report("read");
                break;
            }
            case OblivionInteractionKind::Flora:
            {
                if (const auto harvested = state.mCustomState.find("harvested");
                    harvested != state.mCustomState.end() && std::get_if<bool>(&harvested->second)
                    && *std::get_if<bool>(&harvested->second))
                {
                    message("Nothing to harvest");
                    report("empty");
                    break;
                }
                const ESM4::Flora* flora = ptr.get<ESM4::Flora>()->mBase;
                const ESM::FormKey ingredient = resolver.toFormKey(flora->mIngredient);
                if (ingredient.isNull())
                {
                    message("Nothing to harvest");
                    report("empty");
                    break;
                }
                addInventoryItem(mOblivionRuntimeState->mPlayer.mInventory, ingredient, 1);
                state.mCustomState["harvested"] = true;
                message("Harvested: " + getOblivionItemName(mStore, ESM::RefId(flora->mIngredient)));
                report("harvested");
                break;
            }
            case OblivionInteractionKind::Activator:
            {
                std::int64_t count = 0;
                if (const auto existing = state.mCustomState.find("activation_count");
                    existing != state.mCustomState.end())
                    if (const auto* value = std::get_if<std::int64_t>(&existing->second))
                        count = *value;
                state.mCustomState["activation_count"] = count + 1;
                message("Activated: " + std::string(ptr.getClass().getName(ptr)));
                report("activated");
                break;
            }
        }
    }
}
