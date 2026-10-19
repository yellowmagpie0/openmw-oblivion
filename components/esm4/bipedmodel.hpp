#ifndef OPENMW_COMPONENTS_ESM4_BIPEDMODEL_H
#define OPENMW_COMPONENTS_ESM4_BIPEDMODEL_H

#include <components/esm/path.hpp>

namespace ESM4
{
    // Original469330/4692E0: female role falls back to male when empty.
    // Carried and world roles are distinct pairs supplied by the caller.
    inline const ESM::Path& bipedModelForSex(const ESM::Path& male, const ESM::Path& female, bool isFemale)
    {
        return isFemale && !female.empty() ? female : male;
    }
}

#endif
