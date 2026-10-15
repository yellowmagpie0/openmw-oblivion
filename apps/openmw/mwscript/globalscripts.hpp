#ifndef GAME_SCRIPT_GLOBALSCRIPTS_H
#define GAME_SCRIPT_GLOBALSCRIPTS_H

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include <components/esm/refid.hpp>
#include <components/misc/algorithm.hpp>

#include "locals.hpp"

#include "../mwworld/ptr.hpp"

namespace ESM
{
    class ESMWriter;
    class ESMReader;
    struct GlobalScript;
    struct FormId;
    using RefNum = FormId;
}

namespace Loading
{
    class Listener;
}

namespace MWWorld
{
    class ESMStore;
}

namespace MWScript
{
    struct GlobalScriptDesc
    {
        bool mRunning;
        Locals mLocals;
        std::variant<MWWorld::Ptr, std::pair<ESM::RefNum, ESM::RefId>> mTarget; // Used to start targeted script

        GlobalScriptDesc();

        const MWWorld::Ptr* getPtrIfPresent() const; // Returns a Ptr if one has been resolved

        MWWorld::Ptr getPtr(); // Resolves mTarget to a Ptr and caches the (potentially empty) result

        ESM::RefId getId() const; // Returns the target's ID -- if any
    };

    class GlobalScripts
    {
    public:
        class RetainedRuntime
        {
            friend class GlobalScripts;
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
        ~GlobalScripts();
    private:
        RetainedRuntime::Data* mRetainedRuntime = nullptr;
        unsigned mActiveCalls = 0;
        void requireMutableRuntime() const;

        const MWWorld::ESMStore& mStore;
        std::shared_ptr<const char> mRestoreLifetime = std::make_shared<const char>();
        std::uint64_t mRestoreEpoch = 0;
        std::map<ESM::RefId, std::shared_ptr<GlobalScriptDesc>> mScripts;

    public:
        GlobalScripts(const MWWorld::ESMStore& store);

        void addScript(const ESM::RefId& name, const MWWorld::Ptr& target = MWWorld::Ptr());

        void removeScript(const ESM::RefId& name);

        const std::map<ESM::RefId, std::shared_ptr<GlobalScriptDesc>>& getScripts() const { return mScripts; }

        bool isRunning(const ESM::RefId& name) const;

        void run();
        ///< run all active global scripts

        void clear();

        void addStartup();
        ///< Add startup script

        size_t countSavedGameRecords() const;

        void write(ESM::ESMWriter& writer, Loading::Listener& progress) const;

        bool readRecord(ESM::ESMReader& reader, uint32_t type);
        ///< Records for variables that do not exist are dropped silently.
        ///
        /// \return Known type?

        // Own decoding, descriptor/map storage and compiled local layout before clear.
        // Apply at the original GSCR position; existing targets remain unchanged.
        std::function<bool()> prepareRecord(ESM::GlobalScript&& script);

        Locals& getLocals(const ESM::RefId& name);
        ///< If the script \a name has not been added as a global script yet, it is added
        /// automatically, but is not set to running state.

        const GlobalScriptDesc* getScriptIfPresent(const ESM::RefId& name) const;

        void updatePtrs(const MWWorld::Ptr& base, const MWWorld::Ptr& updated);
        ///< Update the Ptrs stored in mTarget. Should be called after the reference has been moved to a new cell.
    };
}

#endif
