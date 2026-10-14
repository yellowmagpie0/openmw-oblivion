#ifndef COMPONENTS_LUA_STORAGE_H
#define COMPONENTS_LUA_STORAGE_H

#include <map>
#include <sol/sol.hpp>
#include <stdexcept>

#include "asyncpackage.hpp"
#include "serialization.hpp"

namespace LuaUtil
{
    class LuaView;

    class LuaStorage
    {
    public:
        // Prepared at an idle boundary. Detach selects a callback-free baseline;
        // exchange preserves original section/view identity without dispatching Lua.
        // The Lua state must outlive both the owner and its retained runtime.
        class RetainedRuntime
        {
            friend class LuaStorage;
            struct Data;
            std::unique_ptr<Data> mData;
            explicit RetainedRuntime(std::unique_ptr<Data> data);
        public:
            ~RetainedRuntime();
            RetainedRuntime(RetainedRuntime&&) noexcept;
            RetainedRuntime& operator=(RetainedRuntime&&) noexcept;
            RetainedRuntime(const RetainedRuntime&) = delete;
            bool detach() noexcept;
            bool exchange() noexcept;
            bool restore() noexcept;
            void discard() noexcept;
        };
        RetainedRuntime retainRuntime();
        static void initLuaBindings(LuaUtil::LuaView& view);
        static sol::table initGlobalPackage(LuaUtil::LuaView& view, LuaStorage* globalStorage);
        static sol::table initLocalPackage(LuaUtil::LuaView& view, LuaStorage* globalStorage);
        static sol::table initPlayerPackage(
            LuaUtil::LuaView& view, LuaStorage* globalStorage, LuaStorage* playerStorage);
        static sol::table initMenuPackage(LuaUtil::LuaView& view, LuaStorage* globalStorage, LuaStorage* playerStorage);
        static sol::table initLoadPackage(LuaUtil::LuaView& view, LuaStorage* playerStorage);

        explicit LuaStorage() {}
        ~LuaStorage();
        LuaStorage(const LuaStorage&) = delete;
        LuaStorage& operator=(const LuaStorage&) = delete;

        void clearTemporaryAndRemoveCallbacks();
        void load(lua_State* state, const std::filesystem::path& path);
        void save(lua_State* state, const std::filesystem::path& path) const;

        sol::object getSection(
            lua_State* state, std::string_view sectionName, bool readOnly, bool forMenuScripts = false);
        sol::object getMutableSection(lua_State* state, std::string_view sectionName, bool forMenuScripts = false)
        {
            return getSection(state, sectionName, false, forMenuScripts);
        }
        sol::object getReadOnlySection(lua_State* state, std::string_view sectionName)
        {
            return getSection(state, sectionName, true);
        }
        sol::table getAllSections(lua_State* state, bool readOnly = false);

        void setSingleValue(std::string_view section, std::string_view key, const sol::object& value)
        {
            getSection(section)->set(key, value);
        }

        void setSectionValues(std::string_view section, const sol::optional<sol::table>& values)
        {
            getSection(section)->setAll(values);
        }

        class Listener
        {
        public:
            virtual ~Listener() = default;
            virtual void valueChanged(
                std::string_view section, std::string_view key, const sol::object& value) const = 0;
            virtual void sectionReplaced(std::string_view section, const sol::optional<sol::table>& values) const = 0;
        };
        void setListener(const Listener* listener) { requireMutableRuntime(); mListener = listener; }
        void setActive(bool active) { requireMutableRuntime(); mActive = active; }

    private:
        class Value
        {
        public:
            Value() {}
            Value(const sol::object& value)
                : mSerializedValue(serialize(value))
            {
            }
            sol::object getCopy(lua_State* state) const;
            sol::object getReadOnly(lua_State* state) const;

        private:
            std::string mSerializedValue;
            mutable sol::main_object mReadOnlyValue = sol::nil;
        };

        struct Section
        {
            enum LifeTime
            {
                Persistent,
                GameSession,
                Temporary
            };

            explicit Section(LuaStorage* storage, std::string name)
                : mOwner(storage->mThis)
                , mSectionName(std::move(name))
            {
            }
            const Value& get(std::string_view key) const;
            void set(std::string_view key, const sol::object& value);
            void setAll(const sol::optional<sol::table>& values);
            sol::table asTable(lua_State* state);
            void runCallbacks(sol::optional<std::string_view> changedKey);
            void throwIfCallbackRecursionIsTooDeep();

            std::shared_ptr<LuaStorage*> mOwner;
            std::string mSectionName;
            std::map<std::string, Value, std::less<>> mValues;
            std::vector<Callback> mCallbacks;
            std::vector<Callback> mMenuScriptsCallbacks; // menu callbacks are in a separate vector because we don't
                                                         // remove them in clear()
            LifeTime mLifeTime = Persistent;
            static Value sEmpty;

            LuaStorage& storage() const
            {
                if (!mOwner || !*mOwner) throw std::logic_error("Storage section owner was destroyed");
                return **mOwner;
            }
            void checkIfActive() const { storage().checkIfActive(); }
        };
        struct SectionView
        {
            std::shared_ptr<Section> mSection;
            bool mReadOnly;
            bool mForMenuScripts = false;
        };

        const std::shared_ptr<Section>& getSection(std::string_view sectionName);

        std::map<std::string_view, std::shared_ptr<Section>> mData;
        const Listener* mListener = nullptr;
        std::set<const Section*> mRunningCallbacks;
        bool mActive = false;
        std::shared_ptr<LuaStorage*> mThis = std::make_shared<LuaStorage*>(this);
        RetainedRuntime::Data* mRetainedRuntime = nullptr;
        bool mParkedRuntime = false;
        void requireMutableRuntime() const;
        void checkIfActive() const
        {
            requireMutableRuntime();
            if (!mActive)
                throw std::logic_error("Trying to access inactive storage");
        }
        static void registerLifeTime(LuaUtil::LuaView& view, sol::table& res);
    };

}

#endif // COMPONENTS_LUA_STORAGE_H
