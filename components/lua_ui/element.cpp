#include "element.hpp"

#include <MyGUI_Gui.h>

#include "components/settings/values.hpp"
#include "content.hpp"
#include "util.hpp"
#include "registerscriptsettings.hpp"
#include "widget.hpp"

namespace LuaUi
{
    namespace
    {
        namespace LayoutKeys
        {
            constexpr std::string_view type = "type";
            constexpr std::string_view name = "name";
            constexpr std::string_view layer = "layer";
            constexpr std::string_view templateLayout = "template";
            constexpr std::string_view props = "props";
            constexpr std::string_view events = "events";
            constexpr std::string_view content = "content";
            constexpr std::string_view external = "external";
            constexpr std::string_view userData = "userData";

            const std::vector<std::string_view> allKeys
                = { type, name, layer, templateLayout, props, events, content, external, userData };
        }

        const std::string defaultWidgetType = "LuaWidget";

        constexpr uint64_t maxDepth = 250;

        std::string widgetType(const sol::table& layout)
        {
            sol::object typeField = LuaUtil::getFieldOrNil(layout, LayoutKeys::type);
            std::string type = LuaUtil::getValueOrDefault(typeField, defaultWidgetType);
            sol::object templateTypeField
                = LuaUtil::getFieldOrNil(layout, LayoutKeys::templateLayout, LayoutKeys::type);
            if (templateTypeField != sol::nil)
            {
                std::string templateType = LuaUtil::getValueOrDefault(templateTypeField, defaultWidgetType);
                if (typeField != sol::nil && templateType != type)
                    throw std::logic_error(std::string("Template layout type ") + type
                        + std::string(" doesn't match template type ") + templateType);
                type = std::move(templateType);
            }
            return type;
        }

        void destroyWidget(WidgetExtension* ext)
        {
            ext->deinitialize();
            MyGUI::Gui::getInstancePtr()->destroyWidget(ext->widget());
        }

        void detachElements(WidgetExtension* ext)
        {
            auto predicate = [](WidgetExtension* child) {
                if (child->isRoot())
                    return true;
                detachElements(child);
                return false;
            };
            ext->detachChildrenIf(predicate);
            ext->detachTemplateChildrenIf(predicate);
        }

        void destroyChild(WidgetExtension* ext)
        {
            if (!ext->isRoot())
            {
                detachElements(ext);
                destroyWidget(ext);
            }
            else
                ext->detachFromParent();
        }

        void destroyRoot(WidgetExtension* ext)
        {
            detachElements(ext);
            destroyWidget(ext);
        }

        void updateRootCoord(WidgetExtension* ext)
        {
            WidgetExtension* root = ext;
            while (root->getParent())
                root = root->getParent();
            root->updateCoord();
        }

        WidgetExtension* pluckElementRoot(const sol::object& child, uint64_t depth)
        {
            std::shared_ptr<Element> element = child.as<std::shared_ptr<Element>>();
            if (element->mState == Element::Destroyed || element->mState == Element::Destroy)
                throw std::logic_error("Using a destroyed element as a layout child");
            // child Element was created in the same frame and its action hasn't been processed yet
            if (element->mState == Element::New)
                element->create(depth + 1);
            WidgetExtension* root = element->mRoot;
            assert(root);
            return root;
        }

        WidgetExtension* createWidget(const sol::table& layout, bool isRoot, uint64_t depth);
        void updateWidget(WidgetExtension* ext, const sol::table& layout, uint64_t depth);

        std::vector<WidgetExtension*> updateContent(
            const std::vector<WidgetExtension*>& children, const sol::object& contentObj, uint64_t depth)
        {
            ++depth;
            std::vector<WidgetExtension*> result;
            if (contentObj == sol::nil)
            {
                for (WidgetExtension* w : children)
                    destroyChild(w);
                return result;
            }
            ContentView content(LuaUtil::cast<sol::table>(contentObj));
            result.resize(content.size());
            size_t minSize = std::min(children.size(), content.size());
            std::vector<WidgetExtension*> toDestroy;
            for (size_t i = 0; i < minSize; i++)
            {
                WidgetExtension* ext = children[i];
                sol::object child = content.at(i);
                if (child.is<Element>())
                {
                    WidgetExtension* root = pluckElementRoot(child, depth);
                    if (ext != root)
                        toDestroy.emplace_back(ext);
                    result[i] = root;
                }
                else
                {
                    sol::table newLayout = child.as<sol::table>();
                    if (ext->widget()->getTypeName() == widgetType(newLayout))
                    {
                        updateWidget(ext, newLayout, depth);
                    }
                    else
                    {
                        toDestroy.emplace_back(ext);
                        ext = createWidget(newLayout, false, depth);
                    }
                    result[i] = ext;
                }
            }
            for (size_t i = minSize; i < content.size(); i++)
            {
                sol::object child = content.at(i);
                if (child.is<Element>())
                    result[i] = pluckElementRoot(child, depth);
                else
                    result[i] = createWidget(child.as<sol::table>(), false, depth);
            }
            // Don't destroy anything until element creation has had a chance to throw
            for (size_t i = minSize; i < children.size(); i++)
                destroyChild(children[i]);
            for (WidgetExtension* ext : toDestroy)
                destroyChild(ext);
            return result;
        }

        void setTemplate(WidgetExtension* ext, const sol::object& templateLayout, uint64_t depth)
        {
            ++depth;
            sol::object props = LuaUtil::getFieldOrNil(templateLayout, LayoutKeys::props);
            ext->setTemplateProperties(props);
            sol::object content = LuaUtil::getFieldOrNil(templateLayout, LayoutKeys::content);
            ext->setTemplateChildren(updateContent(ext->templateChildren(), content, depth));
        }

        void setEventCallbacks(WidgetExtension* ext, const sol::object& eventsObj)
        {
            ext->clearCallbacks();
            if (eventsObj == sol::nil)
                return;
            if (!eventsObj.is<sol::table>())
                throw std::logic_error("The \"events\" layout field must be a table of callbacks");
            auto events = eventsObj.as<sol::table>();
            events.for_each([ext](const sol::object& name, const sol::object& callback) {
                if (name.is<std::string>() && LuaUtil::Callback::isLuaCallback(callback))
                    ext->setCallback(name.as<std::string>(), LuaUtil::Callback::fromLua(callback));
                else if (!name.is<std::string>())
                    Log(Debug::Warning) << "UI event key must be a string";
                else
                    Log(Debug::Warning) << "UI event handler for key \"" << name.as<std::string>()
                                        << "\" must be an openmw.async.callback";
            });
        }

        WidgetExtension* createWidget(const sol::table& layout, bool isRoot, uint64_t depth)
        {
            static auto widgetTypeMap = widgetTypeToName();
            std::string type = widgetType(layout);
            if (widgetTypeMap.find(type) == widgetTypeMap.end())
                throw std::logic_error(std::string("Invalid widget type ") += type);

            std::string name = layout.get_or(LayoutKeys::name, std::string());
            MyGUI::Widget* widget
                = MyGUI::Gui::getInstancePtr()->createWidgetT(type, {}, {}, MyGUI::Align::Default, {}, name);

            WidgetExtension* ext = dynamic_cast<WidgetExtension*>(widget);
            if (!ext)
                throw std::runtime_error("Invalid widget!");
            ext->initialize(layout.lua_state(), widget, isRoot);

            updateWidget(ext, layout, depth);
            return ext;
        }

        void updateWidget(WidgetExtension* ext, const sol::table& layout, uint64_t depth)
        {
            if (depth >= maxDepth)
                throw std::runtime_error("Maximum layout depth exceeded, probably caused by a circular reference");
            ext->reset();
            ext->setLayout(layout);
            ext->setExternal(layout.get<sol::object>(LayoutKeys::external));
            setTemplate(ext, layout.get<sol::object>(LayoutKeys::templateLayout), depth);
            ext->setProperties(layout.get<sol::object>(LayoutKeys::props));
            setEventCallbacks(ext, layout.get<sol::object>(LayoutKeys::events));
            ext->setChildren(updateContent(ext->children(), layout.get<sol::object>(LayoutKeys::content), depth));
            ext->updateCoord();
        }

        std::string setLayer(WidgetExtension* ext, const sol::table& layout)
        {
            MyGUI::ILayer* layerNode = ext->widget()->getLayer();
            std::string_view currentLayer;
            if (layerNode)
                currentLayer = layerNode->getName();
            std::string newLayer = layout.get_or(LayoutKeys::layer, std::string());
            if (!newLayer.empty() && !MyGUI::LayerManager::getInstance().isExist(newLayer))
                throw std::logic_error(std::string("Layer ") + newLayer + " doesn't exist");
            else if (newLayer != currentLayer)
            {
                MyGUI::LayerManager::getInstance().attachToLayerNode(newLayer, ext->widget());
            }
            return newLayer;
        }
    }

    std::map<Element*, std::shared_ptr<Element>> Element::sMenuElements;
    std::map<Element*, std::shared_ptr<Element>> Element::sGameElements;

    std::shared_ptr<Element::RuntimeDomain> Element::sRuntimeDomain = std::make_shared<RuntimeDomain>();
    Element::RetainedRuntime::Data* Element::sRetainedRuntime = nullptr;

    struct Element::RetainedRuntime::Data
    {
        std::map<Element*, std::shared_ptr<Element>> mGame;
        std::map<Element*, std::shared_ptr<Element>> mMenu;
        std::vector<sol::table> mPages;
        std::shared_ptr<RuntimeDomain> mDomain = std::make_shared<RuntimeDomain>();
        bool mDetached = false;
        bool mOriginalSelected = false;
        Data() { mDomain->mSelected = false; }
        static void park()
        {
            for (auto* map : { &sGameElements, &sMenuElements })
                for (auto& [_, element] : *map)
                    if (element->mRoot)
                    {
                        element->mRuntimeVisible = element->mRoot->widget()->getVisible();
                        element->mRoot->setRuntimeParked(true);
                    }
            for (auto* map : { &sGameElements, &sMenuElements })
                for (auto& [_, element] : *map)
                    if (element->mRoot) element->mRoot->widget()->setVisible(false);
        }
        static void unpark()
        {
            for (auto* map : { &sGameElements, &sMenuElements })
                for (auto& [_, element] : *map)
                    if (element->mRoot) element->mRoot->widget()->setVisible(element->mRuntimeVisible);
            for (auto* map : { &sGameElements, &sMenuElements })
                for (auto& [_, element] : *map)
                    if (element->mRoot) element->mRoot->setRuntimeParked(false);
        }
        void swap()
        {
            park();
            sRuntimeDomain->mSelected = false;
            sRuntimeDomain->mFrozen = false;
            sGameElements.swap(mGame);
            sMenuElements.swap(mMenu);
            exchangeSettingsPages(mPages);
            sRuntimeDomain.swap(mDomain);
            sRuntimeDomain->mSelected = true;
            sRuntimeDomain->mFrozen = false;
            unpark();
        }
        void release()
        {
            mDomain->mRetired = true;
            mDomain->mSelected = false;
            for (auto* map : { &mGame, &mMenu })
                for (auto& [_, element] : *map) element->destroyImpl();
            mPages.clear(); mGame.clear(); mMenu.clear();
            sRetainedRuntime = nullptr;
        }
    };
    void Element::requireSelectedRuntime()
    {
        if (!sRuntimeDomain->mSelected || sRuntimeDomain->mFrozen || sRuntimeDomain->mRetired)
            throw std::logic_error("Lua UI runtime is frozen or retired");
    }
    void Element::requireMutableRuntime() const
    {
        if (!mRuntimeDomain->mSelected || mRuntimeDomain->mFrozen || mRuntimeDomain->mRetired)
            throw std::logic_error("Lua UI element runtime is parked, frozen or retired");
    }
    Element::RetainedRuntime Element::retainRuntime()
    {
        requireSelectedRuntime();
        if (sRetainedRuntime) throw std::logic_error("Lua UI runtime is already retained");
        auto data = std::make_unique<RetainedRuntime::Data>();
        sRetainedRuntime = data.get();
        sRuntimeDomain->mFrozen = true;
        return RetainedRuntime(std::move(data));
    }
    Element::RetainedRuntime::RetainedRuntime(std::unique_ptr<Data> data) : mData(std::move(data)) {}
    Element::RetainedRuntime::~RetainedRuntime()
    { if (mData && !restore()) std::terminate(); }
    Element::RetainedRuntime::RetainedRuntime(RetainedRuntime&&) noexcept = default;
    Element::RetainedRuntime& Element::RetainedRuntime::operator=(RetainedRuntime&& other) noexcept
    {
        if (this != &other)
        {
            if (mData && !restore()) std::terminate();
            mData = std::move(other.mData);
        }
        return *this;
    }
    bool Element::RetainedRuntime::detach() noexcept
    {
        if (!mData || mData->mDetached) return false;
        mData->swap(); mData->mDetached = true;
        return true;
    }
    bool Element::RetainedRuntime::exchange() noexcept
    {
        if (!mData || !mData->mDetached) return false;
        mData->swap(); mData->mOriginalSelected = !mData->mOriginalSelected;
        return true;
    }
    bool Element::RetainedRuntime::restore() noexcept
    {
        if (!mData) return false;
        if (mData->mDetached && !mData->mOriginalSelected) mData->swap();
        sRuntimeDomain->mFrozen = false;
        mData->release(); mData.reset();
        return true;
    }
    void Element::RetainedRuntime::discard() noexcept
    {
        if (!mData) return;
        if (!mData->mDetached) { restore(); return; }
        if (mData->mOriginalSelected) exchange();
        mData->release(); mData.reset();
    }

    Element::Element(sol::table layout, sol::optional<sol::table> options)
        : mRoot(nullptr)
        , mLayout(std::move(layout))
        , mLayer()
        , mState(Element::New)
        , mRuntimeDomain(sRuntimeDomain)
    {
        if (options.has_value())
        {
            mNoWarnUnused = options->get_or("noWarnUnused", false);
        }
    }

    std::shared_ptr<Element> Element::make(sol::table layout, bool menu, sol::optional<sol::table> options)
    {
        requireSelectedRuntime();
        std::shared_ptr<Element> ptr(new Element(std::move(layout), std::move(options)));
        auto& container = menu ? sMenuElements : sGameElements;
        container[ptr.get()] = ptr;
        return ptr;
    }

    void Element::erase(Element* element)
    {
        element->requireMutableRuntime();
        element->destroy();
        sMenuElements.erase(element);
        sGameElements.erase(element);
    }

    const std::vector<std::string_view>& Element::allLayoutProperties()
    {
        return LayoutKeys::allKeys;
    }

    void Element::create(uint64_t depth)
    {
        requireMutableRuntime();
        if (mState == New)
        {
            assert(!mRoot);
            mRoot = createWidget(layout(), true, depth);
            mLayer = setLayer(mRoot, layout());
            updateRootCoord(mRoot);
            mState = Created;
            checkWarnings();
        }
    }

    void Element::update()
    {
        requireMutableRuntime();
        if (mState == Update)
        {
            assert(mRoot);
            if (mRoot->widget()->getTypeName() != widgetType(layout()))
            {
                // A root element can either be attached to another UI element or directly to a layer. Keep the
                // parent information before destroying the old widget: mRoot is no longer valid afterwards.
                WidgetExtension* oldRoot = mRoot;
                WidgetExtension* parent = oldRoot->getParent();
                if (parent == nullptr)
                {
                    destroyRoot(oldRoot);
                    try
                    {
                        mRoot = createWidget(layout(), true, 0);
                    }
                    catch (...)
                    {
                        mRoot = nullptr;
                        mState = New;
                        throw;
                    }
                }
                else
                {
                    auto children = parent->children();
                    auto it = std::find(children.begin(), children.end(), oldRoot);
                    assert(it != children.end());

                    destroyRoot(oldRoot);
                    try
                    {
                        mRoot = createWidget(layout(), true, 0);
                        *it = mRoot;
                    }
                    catch (...)
                    {
                        // Remove the old root from its parent's children even if we couldn't replace it.
                        children.erase(it);
                        parent->setChildren(children);
                        mRoot = nullptr;
                        mState = New;
                        throw;
                    }
                    parent->setChildren(children);
                }
            }
            else
            {
                updateWidget(mRoot, layout(), 0);
            }
            mLayer = setLayer(mRoot, layout());
            updateRootCoord(mRoot);
            mState = Created;
            checkWarnings();
        }
    }

    void Element::destroy()
    {
        requireMutableRuntime();
        destroyImpl();
    }
    void Element::destroyImpl()
    {
        if (mState != Destroyed)
        {
            if (mRoot != nullptr)
            {
                // If someone decided to destroy an element used as another element's content, we need to detach it
                // first so the parent doesn't end up holding a stale pointer
                if (WidgetExtension* parent = mRoot->getParent())
                    parent->detachChildrenIf([&](WidgetExtension* child) { return child == mRoot; });
                destroyRoot(mRoot);
                mRoot = nullptr;
            }
            mLayout.reset();
        }
        mState = Destroyed;
    }

    void Element::checkWarnings()
    {
        if (mNoWarnUnused)
            // Currently unused warnings are our only warnings so we can just early out here.
            return;

        if (Settings::lua().mLuaDebug)
        {
            assert(mRoot);
            WidgetExtension::Warnings warnings;
            mRoot->collectWarnings(warnings, 0, true);
            for (const auto& warning : warnings)
                Log(Debug::Warning) << warning;
        }
        else if (!mWarnedOnce)
        {
            mWarnedOnce = true;
            WidgetExtension::Warnings dummy;
            if (mRoot->collectWarnings(dummy, 0, false))
            {
                Log(Debug::Warning) << "Warning generated while parsing layouts of a(n) " + mRoot->diagnosticName() + ". Set 'lua "
                                       "debug=true' in the [Lua] section of settings.cfg to enable detailed warnings.";
            }
        }
    }
}
