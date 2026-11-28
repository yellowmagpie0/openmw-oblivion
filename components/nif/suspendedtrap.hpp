#ifndef OPENMW_COMPONENTS_NIF_SUSPENDEDTRAP_HPP
#define OPENMW_COMPONENTS_NIF_SUSPENDEDTRAP_HPP

#include <memory>
#include <vector>

namespace Nif
{
    struct Record;

    void animateSuspendedTrap(std::vector<std::unique_ptr<Record>>& records);
}

#endif
