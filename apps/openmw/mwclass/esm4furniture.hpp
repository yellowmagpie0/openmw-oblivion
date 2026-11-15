#ifndef GAME_MWCLASS_ESM4FURNITURE_H
#define GAME_MWCLASS_ESM4FURNITURE_H

#include <components/esm4/loadfurn.hpp>

#include "esm4base.hpp"

namespace MWClass
{
    class ESM4Furniture final : public MWWorld::RegisteredClass<ESM4Furniture, ESM4Base<ESM4::Furniture>>
    {
        friend MWWorld::RegisteredClass<ESM4Furniture, ESM4Base<ESM4::Furniture>>;
        ESM4Furniture();

    public:
        std::string_view getName(const MWWorld::ConstPtr& ptr) const override;
        bool hasToolTip(const MWWorld::ConstPtr& ptr) const override;
        MWGui::ToolTipInfo getToolTipInfo(const MWWorld::ConstPtr& ptr, int count) const override;
        std::unique_ptr<MWWorld::Action> activate(const MWWorld::Ptr& ptr, const MWWorld::Ptr& actor) const override;
    };
}

#endif
