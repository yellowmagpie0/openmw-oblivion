#ifndef OPENMW_LUAUI_ELEMENT
#define OPENMW_LUAUI_ELEMENT

#include <map>
#include <memory>
#include <vector>

#include "widget.hpp"

namespace LuaUi
{
    struct Element
    {
        class RetainedRuntime
        {
            friend struct Element;
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
        // The Lua VM, MyGUI and all external adapters/layers must outlive retirement.
        static RetainedRuntime retainRuntime();
        static void requireSelectedRuntime();
        void requireMutableRuntime() const;
        static std::shared_ptr<Element> make(sol::table layout, bool menu, sol::optional<sol::table> options);
        static void erase(Element* element);

        template <class Callback>
        static void forEach(bool menu, Callback callback)
        {
            auto& container = menu ? sMenuElements : sGameElements;
            for (auto& [_, element] : container)
                callback(element.get());
        }

        template <class Callback>
        static void forEachShared(bool menu, Callback callback)
        {
            auto& container = menu ? sMenuElements : sGameElements;
            for (auto& [_, element] : container)
                callback(element);
        }

        static const std::vector<std::string_view>& allLayoutProperties();

        WidgetExtension* mRoot;
        sol::main_object mLayout;
        std::string mLayer;
        bool mWarnedOnce{ false };

        // From options
        bool mNoWarnUnused{ false };

        enum State
        {
            New,
            Created,
            Update,
            Destroy,
            Destroyed,
        };
        State mState;

        void create(uint64_t dept = 0);

        void update();

        void destroy();

        friend void clearGameInterface();
        friend void clearMenuInterface();

        void checkWarnings();

    private:
        Element(sol::table layout, sol::optional<sol::table> options);
        sol::table layout() { return LuaUtil::cast<sol::table>(mLayout); }
        static std::map<Element*, std::shared_ptr<Element>> sGameElements;
        struct RuntimeDomain
        {
            bool mSelected = true;
            bool mFrozen = false;
            bool mRetired = false;
        };
        static std::shared_ptr<RuntimeDomain> sRuntimeDomain;
        static RetainedRuntime::Data* sRetainedRuntime;
        std::shared_ptr<RuntimeDomain> mRuntimeDomain;
        bool mRuntimeVisible = false;
        void destroyImpl();
        static std::map<Element*, std::shared_ptr<Element>> sMenuElements;
    };
}

#endif // !OPENMW_LUAUI_ELEMENT
