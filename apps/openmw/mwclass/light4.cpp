#include "light4.hpp"

#include "../mwworld/nullaction.hpp"
#include "../mwworld/oblivioninteraction.hpp"
#include "../mwrender/objects.hpp"
#include "../mwrender/renderinginterface.hpp"
#include "../mwworld/ptr.hpp"
#include "../mwworld/customdata.hpp"

#include <cmath>
#include <stdexcept>

#include <components/esm4/loadligh.hpp>

namespace MWClass
{
    namespace
    {
        struct LightUsageData final : MWWorld::TypedCustomData<LightUsageData>
        {
            explicit LightUsageData(float duration) : mRemaining(duration) {}
            float mRemaining;
        };
    }

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
        if (const auto* usage = ptr.getRefData().getCustomData())
            return dynamic_cast<const LightUsageData&>(*usage).mRemaining;
        return (data.flags & inactiveCarryable) == inactiveCarryable && data.time > 0
            ? static_cast<float>(data.time) : -1.f;
    }

    std::unique_ptr<MWWorld::CustomData> ESM4Light::prepareRemainingUsageTime(float duration)
    {
        if (!std::isfinite(duration) || duration <= 0)
            throw std::invalid_argument("Native light lifetime must be finite and positive");
        return std::make_unique<LightUsageData>(duration);
    }

    void ESM4Light::setRemainingUsageTime(const MWWorld::Ptr& ptr, float duration) const
    {
        const auto& data = ptr.get<ESM4::Light>()->mBase->mData;
        constexpr int required = ESM4::Light::Carryable | ESM4::Light::OffDefault;
        if ((data.flags & required) != required || data.time <= 0 || duration > float(data.time))
            throw std::invalid_argument("Native light lifetime disagrees with its winning base");
        ptr.getRefData().setCustomData(prepareRemainingUsageTime(duration));
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
