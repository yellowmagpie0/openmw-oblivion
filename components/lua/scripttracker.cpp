#include "scripttracker.hpp"

#include <stdexcept>

#include "luastate.hpp"

namespace LuaUtil
{
    namespace
    {
        constexpr unsigned sMinLoadedFrames = 50;
        constexpr unsigned sMaxLoadedFrames = 600;
        constexpr unsigned sUsageFrameGrowth = 10;
        constexpr std::size_t sMinToProcess = 1;
        constexpr std::size_t sToProcessDiv = 20; // 5%
    }

    struct ScriptTracker::RuntimeData
    {
        ScriptTracker* mOwner;
        LuaState* mLua;
        std::unique_ptr<ScriptTracker> mArchive = std::make_unique<ScriptTracker>();
        bool mDetached = false;
        bool mOriginalSelected = false;
        RuntimeData(ScriptTracker& owner, LuaState& lua) : mOwner(&owner), mLua(&lua)
        { mArchive->mLua = &lua; }
        bool idle() const noexcept { return mOwner && !mOwner->mUpdating && !mLua->hasActiveScript(); }
        void swap() noexcept
        {
            mOwner->mLoadedScripts.swap(mArchive->mLoadedScripts);
            std::swap(mOwner->mFrame, mArchive->mFrame);
            std::swap(mOwner->mThis, mArchive->mThis);
            *mOwner->mThis = mOwner;
            *mArchive->mThis = mArchive.get();
            mArchive->mParkedRuntime = true;
        }
        void release() noexcept { mArchive.reset(); mOwner = nullptr; }
    };
    ScriptTracker::~ScriptTracker()
    {
        *mThis = nullptr;
        if (mRetainedRuntime) mRetainedRuntime->release();
    }
    std::shared_ptr<ScriptTracker*> ScriptTracker::getLifetime(LuaState& lua)
    {
        requireMutableRuntime();
        if (mLua && mLua != &lua) throw std::logic_error("Lua tracker belongs to another VM");
        mLua = &lua;
        return mThis;
    }
    void ScriptTracker::requireMutableRuntime() const
    {
        if (mParkedRuntime || (mRetainedRuntime && !mRetainedRuntime->mDetached))
            throw std::logic_error("Lua script tracker is parked or frozen before detachment");
    }
    ScriptTracker::RetainedRuntime ScriptTracker::retainRuntime(LuaState& lua)
    {
        if (mRetainedRuntime || mParkedRuntime || mUpdating || lua.hasActiveScript())
            throw std::logic_error("Lua tracker retention requires an idle VM and tracker");
        if (mLua && mLua != &lua) throw std::logic_error("Lua tracker belongs to another VM");
        auto data = std::make_unique<RuntimeData>(*this, lua);
        mLua = &lua;
        mRetainedRuntime = data.get();
        return RetainedRuntime(std::move(data));
    }
    ScriptTracker::RetainedRuntime::RetainedRuntime(std::unique_ptr<RuntimeData> data) : mData(std::move(data)) {}
    ScriptTracker::RetainedRuntime::~RetainedRuntime()
    { if (mData && mData->mOwner && !restore()) std::terminate(); }
    ScriptTracker::RetainedRuntime::RetainedRuntime(RetainedRuntime&&) noexcept = default;
    ScriptTracker::RetainedRuntime& ScriptTracker::RetainedRuntime::operator=(RetainedRuntime&& other) noexcept
    {
        if (this != &other)
        {
            if (mData && mData->mOwner && !restore()) std::terminate();
            mData = std::move(other.mData);
        }
        return *this;
    }
    bool ScriptTracker::RetainedRuntime::detach() noexcept
    {
        if (!mData || !mData->idle() || mData->mDetached) return false;
        mData->swap(); mData->mDetached = true;
        return true;
    }
    bool ScriptTracker::RetainedRuntime::exchange() noexcept
    {
        if (!mData || !mData->idle() || !mData->mDetached) return false;
        mData->swap(); mData->mOriginalSelected = !mData->mOriginalSelected;
        return true;
    }
    bool ScriptTracker::RetainedRuntime::restore() noexcept
    {
        if (!mData || !mData->idle()) return false;
        if (mData->mDetached && !mData->mOriginalSelected) mData->swap();
        mData->mOwner->mRetainedRuntime = nullptr;
        mData->release(); mData.reset();
        return true;
    }
    void ScriptTracker::RetainedRuntime::discard() noexcept
    {
        if (!mData || !mData->mOwner) return;
        if (!mData->mDetached) { if (!restore()) std::terminate(); return; }
        if (!mData->idle()) std::terminate();
        if (mData->mOriginalSelected && !exchange()) std::terminate();
        mData->mOwner->mRetainedRuntime = nullptr;
        mData->release(); mData.reset();
    }

    void ScriptTracker::onLoad(ScriptsContainer& container)
    {
        requireMutableRuntime();
        container.requireMutableRuntime();
        mLoadedScripts.emplace(container.getWeakPointer(), sMinLoadedFrames + mFrame);
    }

    void ScriptTracker::unloadInactiveScripts(LuaView& lua)
    {
        requireMutableRuntime();
        if (mUpdating) throw std::logic_error("Recursive Lua tracker update");
        struct UpdateGuard
        {
            bool& mFlag;
            explicit UpdateGuard(bool& flag) : mFlag(flag) { mFlag = true; }
            ~UpdateGuard() { mFlag = false; }
        } guard(mUpdating);
        // This code is technically incorrect if mFrame overflows... but at 300fps that takes about half a year
        std::size_t toProcess = std::max(mLoadedScripts.size() / sToProcessDiv, sMinToProcess);
        while (toProcess && !mLoadedScripts.empty())
        {
            --toProcess;
            auto [ptr, ttl] = std::move(mLoadedScripts.front());
            mLoadedScripts.pop();
            ScriptsContainer* container = *ptr;
            // Object no longer exists, cease tracking
            if (!container)
                continue;
            // Ignore activity of local scripts in the active grid
            if (container->isActive())
                ttl = std::max(ttl, mFrame + sMinLoadedFrames);
            else
            {
                bool activeSinceLastPop = container->mRequiredLoading;
                if (activeSinceLastPop)
                {
                    container->mRequiredLoading = false;
                    ttl = std::min(ttl + sUsageFrameGrowth, mFrame + sMaxLoadedFrames);
                }
                else if (ttl < mFrame)
                {
                    container->ensureUnloaded(lua);
                    continue;
                }
            }
            mLoadedScripts.emplace(std::move(ptr), ttl);
        }
        ++mFrame;
    }
}
