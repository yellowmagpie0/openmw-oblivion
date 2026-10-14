#ifndef GAME_MWWORLD_LOCALSCRIPTS_H
#define GAME_MWWORLD_LOCALSCRIPTS_H

#include <list>
#include <memory>
#include <string>

#include "ptr.hpp"

namespace MWWorld
{
    class ESMStore;
    class CellStore;
    class RefData;

    /// \brief List of active local scripts
    class LocalScripts
    {
        std::list<std::pair<ESM::RefId, Ptr>> mScripts;
        std::list<std::pair<ESM::RefId, Ptr>>::iterator mIter;
        const MWWorld::ESMStore& mStore;

    public:
        class PreparedRegistration
        {
            friend class LocalScripts;
            struct Data;
            std::unique_ptr<Data> mData;
        public:
            PreparedRegistration(const ESM::RefId& script, const ESMStore& content, const ESMStore& incoming);
            ~PreparedRegistration();
            PreparedRegistration(const PreparedRegistration&);
            PreparedRegistration& operator=(const PreparedRegistration&);
            PreparedRegistration(PreparedRegistration&&) noexcept;
            PreparedRegistration& operator=(PreparedRegistration&&) noexcept;
        };

        // World-owned parked list/cursor. Referenced objects and their bases
        // must remain alive; exchange only between script passes.
        class RuntimeState
        {
            friend class LocalScripts;
            std::list<std::pair<ESM::RefId, Ptr>> mScripts;
            decltype(mScripts)::iterator mIter = mScripts.end();
        public:
            RuntimeState() = default;
            RuntimeState(const RuntimeState&) = delete;
            RuntimeState& operator=(const RuntimeState&) = delete;
        };
        void swapRuntime(RuntimeState& state) noexcept;

        LocalScripts(const MWWorld::ESMStore& store);

        void startIteration();
        ///< Set the iterator to the begin of the script list.

        bool getNext(std::pair<ESM::RefId, Ptr>& script);
        ///< Get next local script
        /// @return Did we get a script?

        void add(const ESM::RefId& scriptName, const Ptr& ptr, PreparedRegistration* prepared = nullptr);
        ///< Add script to collection of active local scripts.

        void addCell(CellStore* cell);
        ///< Add all local scripts in a cell.

        void clear();
        ///< Clear active local scripts collection.

        void clearCell(CellStore* cell);
        ///< Remove all scripts belonging to \a cell.

        void remove(const MWWorld::CellRef* ref);

        void remove(const Ptr& ptr);
        ///< Remove script for given reference (ignored if reference does not have a script listed).

        bool isRunning(const ESM::RefId&, const Ptr&) const;
        ///< Is the local script running?.
    };
}

#endif
