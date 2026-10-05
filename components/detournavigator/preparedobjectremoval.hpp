#ifndef OPENMW_COMPONENTS_DETOURNAVIGATOR_PREPAREDOBJECTREMOVAL_H
#define OPENMW_COMPONENTS_DETOURNAVIGATOR_PREPAREDOBJECTREMOVAL_H

namespace DetourNavigator
{
    // Scoped transaction: the navigator and any borrowed update guard must
    // outlive it. Destroy it before callbacks or unguarded navigation operations.
    class PreparedObjectRemoval
    {
    public:
        virtual ~PreparedObjectRemoval() = default;
        virtual bool isValid() const = 0;
        virtual bool commit() = 0;
    };
}

#endif
