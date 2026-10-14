#ifndef OPENMW_LUAUI_ADAPTER
#define OPENMW_LUAUI_ADAPTER

#include <memory>
#include <vector>

#include <MyGUI_Widget.h>

namespace LuaUi
{
    class LuaContainer;
    struct Element;
    class LuaAdapter : public MyGUI::Widget
    {
        MYGUI_RTTI_DERIVED(LuaAdapter)

    public:
        class RetainedRuntime
        {
            friend class LuaAdapter;
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
        LuaAdapter();
        ~LuaAdapter() override;
        RetainedRuntime retainRuntime();
        static std::vector<LuaAdapter*> instances();

        void attach(const std::shared_ptr<Element>& element);
        void detach();

    private:
        std::shared_ptr<Element> mElement;
        LuaContainer* mContainer;

        RetainedRuntime::Data* mRetainedRuntime = nullptr;
        void requireMutableRuntime() const;
        LuaContainer* createContainer();
        static void retireContainer(LuaContainer* container);
        void attachElement();
        void detachElement();

        void containerChangedCoord(MyGUI::Widget*);
        void widgetDestroyed(MyGUI::Widget*);
    };
}

#endif // !OPENMW_LUAUI_ADAPTER
