#ifndef OPENMW_COMPONENTS_MYGUIPLATFORM_GUIRUNTIME_H
#define OPENMW_COMPONENTS_MYGUIPLATFORM_GUIRUNTIME_H

#include <memory>

namespace MyGUIPlatform
{
    // Retains actual MyGUI layer/node trees, root widgets and input/controller
    // state, including actual per-widget/controller frame subscriptions. All UI/render/input users must be stopped during selection. External
    // widget owners (native windows and Lua elements) must release the retiring
    // selection before restore/discard destroys its remaining GUI-owned roots.
    // The prepared incoming GUI starts empty; populate its layers before widgets.
    class GuiRuntime
    {
    public:
        class RetainedRuntime
        {
            friend class GuiRuntime;
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

        static bool supported() noexcept;
        static RetainedRuntime retain();
        // Call after external widget owners have retired, before Gui::shutdown.
        // Invalidates a remaining handle while the managers are still alive.
        static void beforeShutdown() noexcept;
    };
}

#endif
