#ifndef COMPONENTS_LUA_SCRIPTTRACKER_H
#define COMPONENTS_LUA_SCRIPTTRACKER_H

#include <memory>
#include <queue>
#include <utility>

#include "scriptscontainer.hpp"

namespace LuaUtil
{
    class ScriptTracker
    {
        using Frame = unsigned int;
        using TrackedScriptContainer = std::pair<ScriptsContainerWeakPtr, Frame>;
        std::queue<TrackedScriptContainer> mLoadedScripts;
        Frame mFrame = 0;
        std::shared_ptr<ScriptTracker*> mThis = std::make_shared<ScriptTracker*>(this);
        struct RuntimeData;
        RuntimeData* mRetainedRuntime = nullptr;
        bool mParkedRuntime = false;
        bool mUpdating = false;
        LuaState* mLua = nullptr;

    public:
        class RetainedRuntime
        {
            friend class ScriptTracker;
            std::unique_ptr<RuntimeData> mData;
            explicit RetainedRuntime(std::unique_ptr<RuntimeData> data);
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
        ScriptTracker() = default;
        ~ScriptTracker();
        ScriptTracker(const ScriptTracker&) = delete;
        ScriptTracker& operator=(const ScriptTracker&) = delete;
        // The VM and both cell domains must stay alive until runtime retirement.
        RetainedRuntime retainRuntime(LuaState& lua);
        std::shared_ptr<ScriptTracker*> getLifetime(LuaState& lua);
        void requireMutableRuntime() const;
        void unloadInactiveScripts(LuaView& lua);

        void onLoad(ScriptsContainer& container);

        std::size_t size() const { return mLoadedScripts.size(); }
    };
}

#endif // COMPONENTS_LUA_SCRIPTTRACKER_H
