#include "light4.hpp"

#include "../mwworld/nullaction.hpp"
#include "../mwworld/oblivioninteraction.hpp"
#include "../mwrender/objects.hpp"
#include "../mwrender/renderinginterface.hpp"
#include "../mwworld/ptr.hpp"

#include <components/esm4/loadligh.hpp>

namespace MWClass
{
    ESM4Light::ESM4Light()
        : MWWorld::RegisteredClass<ESM4Light, ESM4Base<ESM4::Light>>(ESM4::Light::sRecordId)
    {
    }

    std::string_view ESM4Light::getName(const MWWorld::ConstPtr& ptr) const
    {
        return ptr.get<ESM4::Light>()->mBase->mFullName;
    }

    bool ESM4Light::isItem(const MWWorld::ConstPtr& ptr) const
    {
        return (ptr.get<ESM4::Light>()->mBase->mData.flags & ESM4::Light::Carryable) != 0;
    }

    bool ESM4Light::hasToolTip(const MWWorld::ConstPtr& ptr) const
    {
        return isItem(ptr) && !getName(ptr).empty();
    }

    float ESM4Light::getRemainingUsageTime(const MWWorld::ConstPtr& ptr) const
    {
        const auto& data = ptr.get<ESM4::Light>()->mBase->mData;
        constexpr int inactiveCarryable = ESM4::Light::Carryable | ESM4::Light::OffDefault;
        // Native drops currently admit only full, inactive lights. Their
        // unspent lifetime is base-derived; used/lit instances need their own state.
        return (data.flags & inactiveCarryable) == inactiveCarryable && data.time > 0
            ? static_cast<float>(data.time) : -1.f;
    }

    MWGui::ToolTipInfo ESM4Light::getToolTipInfo(const MWWorld::ConstPtr& ptr, int count) const
    {
        return ESM4Impl::getToolTipInfo(getName(ptr), count);
    }

    std::unique_ptr<MWWorld::Action> ESM4Light::activate(
        const MWWorld::Ptr& ptr, const MWWorld::Ptr&) const
    {
        if (!isItem(ptr))
            return std::make_unique<MWWorld::NullAction>();
        return std::make_unique<MWWorld::OblivionInteractionAction>(
            ptr, MWWorld::OblivionInteractionKind::Take);
    }

    void ESM4Light ::insertObjectRendering(
        const MWWorld::Ptr& ptr, const std::string& model, MWRender::RenderingInterface& renderingInterface) const
    {
        MWWorld::LiveCellRef<ESM4::Light>* ref = ptr.get<ESM4::Light>();

        // Insert even if model is empty, so that the light is added
        renderingInterface.getObjects().insertModel(ptr, model, !(ref->mBase->mData.flags & ESM4::Light::OffDefault));
    }
}
