#ifndef OPENW_MWCLASS_LIGHT4
#define OPENW_MWCLASS_LIGHT4

#include "../mwworld/registeredclass.hpp"

#include "esm4base.hpp"

namespace MWClass
{
    class ESM4Light : public MWWorld::RegisteredClass<ESM4Light, ESM4Base<ESM4::Light>>
    {
        friend MWWorld::RegisteredClass<ESM4Light, ESM4Base<ESM4::Light>>;

        ESM4Light();

    public:
        std::string_view getName(const MWWorld::ConstPtr& ptr) const override;
        bool isItem(const MWWorld::ConstPtr& ptr) const override;
        bool hasToolTip(const MWWorld::ConstPtr& ptr) const override;
        MWGui::ToolTipInfo getToolTipInfo(const MWWorld::ConstPtr& ptr, int count) const override;
        std::unique_ptr<MWWorld::Action> activate(
            const MWWorld::Ptr& ptr, const MWWorld::Ptr& actor) const override;
        void insertObjectRendering(const MWWorld::Ptr& ptr, const std::string& model,
            MWRender::RenderingInterface& renderingInterface) const override;
        ///< Add reference into a cell for rendering
    };
}
#endif
