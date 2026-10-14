#include "adapter.hpp"

#include <MyGUI_Gui.h>
#include <set>
#include <stdexcept>

#include "container.hpp"
#include "element.hpp"

namespace LuaUi
{
    namespace
    {
        sol::state luaState;
        std::set<LuaAdapter*> adapters;
    }

    struct LuaAdapter::RetainedRuntime::Data
    {
        LuaAdapter* mOwner;
        std::shared_ptr<Element> mElement;
        LuaContainer* mContainer;
        MyGUI::IntCoord mCoord;
        bool mVisible = true;
        bool mDetached = false;
        bool mOriginalSelected = false;
        explicit Data(LuaAdapter& owner)
            : mOwner(&owner), mContainer(owner.createContainer()), mCoord(owner.getCoord())
        {
            mContainer->widget()->setVisible(false);
        }
        ~Data() { if (mContainer) LuaAdapter::retireContainer(mContainer); }
        void swap()
        {
            const auto coord = mOwner->getCoord();
            const bool visible = mOwner->mContainer->widget()->getVisible();
            mOwner->mContainer->setRuntimeParked(true);
            mOwner->mContainer->widget()->setVisible(false);
            mOwner->mContainer->widget()->detachFromWidget();
            mOwner->mElement.swap(mElement);
            std::swap(mOwner->mContainer, mContainer);
            if (mOwner->mContainer->widget()->getParent() != mOwner)
                mOwner->mContainer->widget()->attachToWidget(mOwner);
            mOwner->setCoord(mCoord);
            mOwner->mContainer->widget()->setVisible(mVisible);
            mOwner->mContainer->setRuntimeParked(false);
            mCoord = coord; mVisible = visible;
        }
        void release()
        {
            if (mContainer) { LuaAdapter::retireContainer(mContainer); mContainer = nullptr; }
            mElement.reset();
            if (mOwner) mOwner->mRetainedRuntime = nullptr;
            mOwner = nullptr;
        }
    };
    LuaContainer* LuaAdapter::createContainer()
    {
        auto* container = MyGUI::Gui::getInstancePtr()->createWidget<LuaContainer>(
            "", MyGUI::IntCoord(), MyGUI::Align::Default, "", "");
        try
        {
            container->initialize(luaState, container, false);
            container->widget()->eventChangeCoord += MyGUI::newDelegate(this, &LuaAdapter::containerChangedCoord);
        }
        catch (...) { MyGUI::Gui::getInstancePtr()->destroyWidget(container); throw; }
        return container;
    }
    void LuaAdapter::retireContainer(LuaContainer* container)
    {
        container->setRuntimeParked(true);
        container->widget()->eventChangeCoord.clear();
        // Element roots have independent ownership; never let MyGUI delete them with the adapter container.
        container->detachChildrenIf([](WidgetExtension* child) { return child->isRoot(); });
        container->detachTemplateChildrenIf([](WidgetExtension* child) { return child->isRoot(); });
        container->deinitialize();
        MyGUI::Gui::getInstancePtr()->destroyWidget(container->widget());
    }
    LuaAdapter::LuaAdapter()
        : mElement(nullptr), mContainer(createContainer())
    {
        mContainer->widget()->attachToWidget(this);
        adapters.insert(this);
        eventWidgetDestroyed += MyGUI::newDelegate(this, &LuaAdapter::widgetDestroyed);
    }
    void LuaAdapter::widgetDestroyed(MyGUI::Widget*)
    {
        // MyGUI emits this before shutting down/deleting child widgets. The C++ destructor is too late.
        adapters.erase(this);
        if (mRetainedRuntime) mRetainedRuntime->release();
        if (mContainer) { retireContainer(mContainer); mContainer = nullptr; }
        mElement.reset();
    }
    LuaAdapter::~LuaAdapter()
    {
        adapters.erase(this);
        // Normal destruction has already released these in eventWidgetDestroyed.
        if (mContainer) widgetDestroyed(this);
    }
    std::vector<LuaAdapter*> LuaAdapter::instances() { return { adapters.begin(), adapters.end() }; }
    void LuaAdapter::requireMutableRuntime() const
    {
        if (mRetainedRuntime && !mRetainedRuntime->mDetached)
            throw std::logic_error("Lua adapter is frozen before detachment");
    }
    LuaAdapter::RetainedRuntime LuaAdapter::retainRuntime()
    {
        if (mRetainedRuntime) throw std::logic_error("Lua adapter already retained");
        auto data = std::make_unique<RetainedRuntime::Data>(*this);
        mRetainedRuntime = data.get();
        return RetainedRuntime(std::move(data));
    }
    LuaAdapter::RetainedRuntime::RetainedRuntime(std::unique_ptr<Data> data) : mData(std::move(data)) {}
    LuaAdapter::RetainedRuntime::~RetainedRuntime()
    { if (mData && mData->mOwner && !restore()) std::terminate(); }
    LuaAdapter::RetainedRuntime::RetainedRuntime(RetainedRuntime&&) noexcept = default;
    LuaAdapter::RetainedRuntime& LuaAdapter::RetainedRuntime::operator=(RetainedRuntime&& other) noexcept
    {
        if (this != &other)
        {
            if (mData && mData->mOwner && !restore()) std::terminate();
            mData = std::move(other.mData);
        }
        return *this;
    }
    bool LuaAdapter::RetainedRuntime::detach() noexcept
    {
        if (!mData || !mData->mOwner || mData->mDetached) return false;
        mData->swap(); mData->mDetached = true; return true;
    }
    bool LuaAdapter::RetainedRuntime::exchange() noexcept
    {
        if (!mData || !mData->mOwner || !mData->mDetached) return false;
        mData->swap(); mData->mOriginalSelected = !mData->mOriginalSelected; return true;
    }
    bool LuaAdapter::RetainedRuntime::restore() noexcept
    {
        if (!mData || !mData->mOwner) return false;
        if (mData->mDetached && !mData->mOriginalSelected) mData->swap();
        mData->release(); mData.reset(); return true;
    }
    void LuaAdapter::RetainedRuntime::discard() noexcept
    {
        if (!mData || !mData->mOwner) return;
        if (!mData->mDetached) { restore(); return; }
        if (mData->mOriginalSelected) exchange();
        mData->release(); mData.reset();
    }

    void LuaAdapter::containerChangedCoord(MyGUI::Widget*)
    {
        setSize(mContainer->getSize());
    }

    void LuaAdapter::attach(const std::shared_ptr<Element>& element)
    {
        requireMutableRuntime();
        element->requireMutableRuntime();
        detachElement();
        mElement = element;
        attachElement();
        setSize(mContainer->widget()->getSize());

        // workaround for MyGUI bug
        // parent visibility doesn't affect added children
        setVisible(!getVisible());
        setVisible(!getVisible());
    }

    void LuaAdapter::detach()
    {
        requireMutableRuntime();
        if (mElement) mElement->requireMutableRuntime();
        detachElement();
        setSize({ 0, 0 });
    }

    void LuaAdapter::attachElement()
    {
        if (!mElement.get())
            return;
        if (!mElement->mRoot)
            throw std::logic_error("Attempting to use a destroyed UI Element");
        mContainer->setChildren({ mElement->mRoot });
        mElement->mRoot->updateCoord();
        mContainer->updateCoord();
    }

    void LuaAdapter::detachElement()
    {
        mContainer->setChildren({});
        if (mElement && mElement->mRoot)
            mElement->mRoot->widget()->detachFromWidget();
        mElement = nullptr;
    }
}
