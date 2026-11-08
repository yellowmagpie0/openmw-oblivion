#ifndef OPENMW_COMPONENTS_ESM4_CELLCONDITION_H
#define OPENMW_COMPONENTS_ESM4_CELLCONDITION_H

#include "loadcell.hpp"

namespace ESM4
{
    inline bool cellConditionMatches(const Cell* actual, const Cell* parameter)
    {
        if (!actual || !parameter)
            return false;
        const auto& prefix = parameter->mEditorId;
        const auto& name = actual->mEditorId;
        if (name.size() < prefix.size())
            return false;
        const auto lower = [](unsigned char c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; };
        for (std::size_t i = 0; i < prefix.size(); ++i)
            if (lower(name[i]) != lower(prefix[i]))
                return false;
        return true;
    }
}

#endif
