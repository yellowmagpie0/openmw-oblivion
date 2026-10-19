#include "esm4base.hpp"

#include <MyGUI_TextIterator.h>
#include <MyGUI_UString.h>

#include <components/sceneutil/positionattitudetransform.hpp>

#include "../mwgui/tooltips.hpp"

#include "../mwrender/objects.hpp"
#include "../mwrender/renderinginterface.hpp"
#include "../mwrender/vismask.hpp"

#include "../mwphysics/physicssystem.hpp"
#include "../mwworld/ptr.hpp"
#include "../mwworld/worldimp.hpp"
#include "../mwworld/oblivionownership.hpp"
#include <components/esm4/bipedmodel.hpp>
#include <components/esm4/loadnpc.hpp>

namespace MWClass
{
    std::optional<VFS::Path::NormalizedView> ESM4Impl::nativeBipedWorldModel(
        const MWWorld::ConstPtr& ptr, const ESM::Path& male, const ESM::Path& female)
    {
        auto* baseWorld = MWBase::Environment::get().getWorldOrNull();
        if (!baseWorld || baseWorld->getGameProfile() != ESM::GameProfile::Oblivion) return std::nullopt;
        auto* nativeWorld = dynamic_cast<MWWorld::World*>(baseWorld);
        if (!nativeWorld) throw std::logic_error("native biped model requires its concrete World owner");
        auto& world = *nativeWorld;
        // Native no-reference previews choose the male world role.
        const auto owner = ptr.getCellRef().getNativeReference()
            ? MWWorld::oblivionBipedReferenceOwner(world, ptr) : ESM::FormId{};
        const auto* npc = world.getStore().get<ESM4::Npc>().search(ESM::RefId(owner));
        const bool isFemale = npc && (npc->mBaseConfig.tes4.flags & ESM4::Npc::TES4_Female);
        return ESM4::bipedModelForSex(male, female, isFemale).getNormalized();
    }

    void ESM4Impl::insertObjectRendering(
        const MWWorld::Ptr& ptr, const std::string& model, MWRender::RenderingInterface& renderingInterface)
    {
        if (!model.empty())
        {
            renderingInterface.getObjects().insertModel(ptr, model);
            ptr.getRefData().getBaseNode()->setNodeMask(MWRender::Mask_Static);
        }
    }

    void ESM4Impl::insertObjectPhysics(
        const MWWorld::Ptr& ptr, const std::string& model, const osg::Quat& rotation, MWPhysics::PhysicsSystem& physics)
    {
        physics.addObject(ptr, VFS::Path::toNormalized(model), rotation, MWPhysics::CollisionType_World);
    }

    MWGui::ToolTipInfo ESM4Impl::getToolTipInfo(std::string_view name, int count)
    {
        MWGui::ToolTipInfo info;
        info.caption = MyGUI::TextIterator::toTagsString(MyGUI::UString(name)) + MWGui::ToolTips::getCountString(count);
        return info;
    }
}
