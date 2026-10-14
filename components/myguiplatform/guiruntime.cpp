#include "guiruntime.hpp"

#include <MyGUI_ControllerManager.h>
#include <MyGUI_Gui.h>
#include <MyGUI_DynLibManager.h>
#include <MyGUI_InputManager.h>
#include <MyGUI_LayerManager.h>
#include <MyGUI_PointerManager.h>
#include <MyGUI_ToolTipManager.h>
#include <MyGUI_WidgetManager.h>

#include <stdexcept>
#include <utility>

#ifdef MYGUI_OPENMW_RUNTIME_ACCESS
namespace MyGUI
{
    // The pinned dependency grants friendship without changing its layout/ABI.
    // Keep resource/factory registries and unlinkers shared;
    // only the selected runtime's actual collections and temporal state move.
    class RuntimeAccess
    {
    public:
        struct State
        {
            EventHandle_FrameEventDelegate::ListDelegate mFrames;
            std::vector<DynLib*> mDelayedLibs;
            VectorWidgetPtr mRoots;
            LayerManager::VectorLayer mLayers;
            std::list<std::pair<Widget*, ControllerItem*>> mControllers;
            VectorWidgetPtr mDestroyWidgets;
            Widget* mMouseFocus = nullptr;
            Widget* mKeyFocus = nullptr;
            ILayer* mMouseLayer = nullptr;
            float mDoubleClickTime = 0.25f;
            bool mShift = false, mControl = false, mAlt = false, mMeta = false;
            IntPoint mMousePosition;
            IntPoint mLastPressed[MouseButton::MAX];
            bool mMouseCapture[MouseButton::MAX]{};
            KeyCode mHoldKey{ KeyCode::None };
            Char mHoldChar = 0;
            bool mFirstPressKey = true;
            float mKeyTime = 0;
            int mOldAbsZ = 0;
            VectorWidgetPtr mModals;
            VectorWidgetPtr mPointerRoots;
            IntPoint mPointerPoint, mOldPointerPoint;
            bool mPointerVisible = false;
            Widget* mPointerOwner = nullptr;
            ImageBox* mMousePointer = nullptr;
            IPointer* mPointer = nullptr;
            std::string mCurrentPointer;
            Widget* mTooltipFocus = nullptr;
            IntPoint mTooltipPoint;
            bool mTooltipVisible = false;
            float mTooltipTime = 0;
            size_t mTooltipIndex = ITEM_NONE;
            bool mNeedTooltip = false;
        };

        static bool available() noexcept
        {
            return Gui::getInstancePtr() && Gui::getInstance().mIsInitialise
                && LayerManager::getInstancePtr() && InputManager::getInstancePtr()
                && ControllerManager::getInstancePtr() && WidgetManager::getInstancePtr()
                && PointerManager::getInstancePtr() && ToolTipManager::getInstancePtr()
                && DynLibManager::getInstancePtr() && idle();
        }

        static bool idle() noexcept
        {
            return !Gui::getInstance().eventFrameStart.mRunning
                && !InputManager::getInstance().eventChangeMouseFocus.mRunning
                && !InputManager::getInstance().eventChangeKeyFocus.mRunning;
        }

        static void prepareFrames(State& state)
        {
            // Match the pinned Gui::initialise subscription order. Controllers
            // and widget-specific timers subscribe when the incoming UI adds them.
            state.mFrames.emplace_back(newDelegate(&WidgetManager::getInstance(), &WidgetManager::notifyEventFrameStart));
            state.mFrames.emplace_back(newDelegate(&InputManager::getInstance(), &InputManager::frameEntered));
            state.mFrames.emplace_back(newDelegate(&PointerManager::getInstance(), &PointerManager::notifyFrameStart));
            state.mFrames.emplace_back(newDelegate(&DynLibManager::getInstance(), &DynLibManager::notifyEventFrameStart));
            state.mFrames.emplace_back(newDelegate(&ToolTipManager::getInstance(), &ToolTipManager::notifyEventFrameStart));
        }

        static void exchange(State& state) noexcept
        {
            using std::swap;
            Gui::getInstance().eventFrameStart.mListDelegates.swap(state.mFrames);
            DynLibManager::getInstance().mDelayDynLib.swap(state.mDelayedLibs);
            Gui::getInstance().mWidgetChild.swap(state.mRoots);
            LayerManager::getInstance().mLayerNodes.swap(state.mLayers);
            ControllerManager::getInstance().mListItem.swap(state.mControllers);
            WidgetManager::getInstance().mDestroyWidgets.swap(state.mDestroyWidgets);
            auto& input = InputManager::getInstance();
            swap(input.mWidgetMouseFocus, state.mMouseFocus);
            swap(input.mWidgetKeyFocus, state.mKeyFocus);
            swap(input.mLayerMouseFocus, state.mMouseLayer);
            swap(input.mTimerDoubleClick, state.mDoubleClickTime);
            swap(input.mIsShiftPressed, state.mShift);
            swap(input.mIsControlPressed, state.mControl);
            swap(input.mIsAltPressed, state.mAlt);
            swap(input.mIsMetaPressed, state.mMeta);
            swap(input.mMousePosition, state.mMousePosition);
            swap(input.mLastPressed, state.mLastPressed);
            swap(input.mMouseCapture, state.mMouseCapture);
            swap(input.mHoldKey, state.mHoldKey);
            swap(input.mHoldChar, state.mHoldChar);
            swap(input.mFirstPressKey, state.mFirstPressKey);
            swap(input.mTimerKey, state.mKeyTime);
            swap(input.mOldAbsZ, state.mOldAbsZ);
            input.mVectorModalRootWidget.swap(state.mModals);
            auto& pointer = PointerManager::getInstance();
            pointer.mWidgetChild.swap(state.mPointerRoots);
            swap(pointer.mPoint, state.mPointerPoint);
            swap(pointer.mOldPoint, state.mOldPointerPoint);
            swap(pointer.mVisible, state.mPointerVisible);
            swap(pointer.mWidgetOwner, state.mPointerOwner);
            swap(pointer.mMousePointer, state.mMousePointer);
            swap(pointer.mPointer, state.mPointer);
            pointer.mCurrentMousePointer.swap(state.mCurrentPointer);
            auto& tooltip = ToolTipManager::getInstance();
            swap(tooltip.mOldFocusWidget, state.mTooltipFocus);
            swap(tooltip.mOldMousePoint, state.mTooltipPoint);
            swap(tooltip.mToolTipVisible, state.mTooltipVisible);
            swap(tooltip.mCurrentTime, state.mTooltipTime);
            swap(tooltip.mOldIndex, state.mTooltipIndex);
            swap(tooltip.mNeedToolTip, state.mNeedTooltip);
        }

        static void retireSelected() noexcept
        {
            // External owners must already have released this selection. Drain
            // pending widget deletion while its matching layers/input are active.
            Gui::getInstance().destroyAllChildWidget();
            PointerManager::getInstance()._destroyAllChildWidget();
            WidgetManager::getInstance()._deleteDelayWidgets();
            ControllerManager::getInstance().clear();
            LayerManager::getInstance().clear();
            DynLibManager::getInstance()._unloadDelayDynLibs();
        }
    };
}
#endif

namespace MyGUIPlatform
{
    struct GuiRuntime::RetainedRuntime::Data
    {
        inline static Data* sActive = nullptr;
        MyGUI::Gui* mOwner;
        bool mDetached = false;
        bool mOriginalSelected = false;
#ifdef MYGUI_OPENMW_RUNTIME_ACCESS
        MyGUI::RuntimeAccess::State mState;
#endif
        Data() : mOwner(MyGUI::Gui::getInstancePtr())
        {
#ifdef MYGUI_OPENMW_RUNTIME_ACCESS
            MyGUI::RuntimeAccess::prepareFrames(mState);
#endif
        }
        ~Data() { release(); }
        bool alive() const noexcept
        { return mOwner && MyGUI::Gui::getInstancePtr() == mOwner; }
        bool idle() const noexcept
        {
#ifdef MYGUI_OPENMW_RUNTIME_ACCESS
            return alive() && MyGUI::RuntimeAccess::idle();
#else
            return false;
#endif
        }
        void exchange() noexcept
        {
#ifdef MYGUI_OPENMW_RUNTIME_ACCESS
            MyGUI::RuntimeAccess::exchange(mState);
#endif
        }
        void retire() noexcept
        {
#ifdef MYGUI_OPENMW_RUNTIME_ACCESS
            MyGUI::RuntimeAccess::retireSelected();
#endif
        }
        void release() noexcept
        {
            if (sActive == this) sActive = nullptr;
            mOwner = nullptr;
        }
    };

    bool GuiRuntime::supported() noexcept
    {
#ifdef MYGUI_OPENMW_RUNTIME_ACCESS
        return true;
#else
        return false;
#endif
    }

    GuiRuntime::RetainedRuntime GuiRuntime::retain()
    {
#ifdef MYGUI_OPENMW_RUNTIME_ACCESS
        if (!MyGUI::RuntimeAccess::available() || RetainedRuntime::Data::sActive)
            throw std::logic_error("MyGUI runtime retention is unavailable or already active");
        auto data = std::make_unique<RetainedRuntime::Data>();
        RetainedRuntime::Data::sActive = data.get();
        return RetainedRuntime(std::move(data));
#else
        throw std::logic_error("MyGUI runtime retention requires the patched fetched dependency");
#endif
    }

    GuiRuntime::RetainedRuntime::RetainedRuntime(std::unique_ptr<Data> data) : mData(std::move(data)) {}
    GuiRuntime::RetainedRuntime::~RetainedRuntime()
    { if (mData && mData->alive() && !restore()) std::terminate(); }
    GuiRuntime::RetainedRuntime::RetainedRuntime(RetainedRuntime&&) noexcept = default;
    GuiRuntime::RetainedRuntime& GuiRuntime::RetainedRuntime::operator=(RetainedRuntime&& other) noexcept
    {
        if (this != &other)
        {
            if (mData && mData->alive() && !restore()) std::terminate();
            mData = std::move(other.mData);
        }
        return *this;
    }
    bool GuiRuntime::RetainedRuntime::detach() noexcept
    {
        if (!mData || !mData->idle() || mData->mDetached) return false;
        mData->exchange(); mData->mDetached = true; return true;
    }
    bool GuiRuntime::RetainedRuntime::exchange() noexcept
    {
        if (!mData || !mData->idle() || !mData->mDetached) return false;
        mData->exchange(); mData->mOriginalSelected = !mData->mOriginalSelected; return true;
    }
    bool GuiRuntime::RetainedRuntime::restore() noexcept
    {
        if (!mData || !mData->idle()) return false;
        if (mData->mDetached)
        {
            if (mData->mOriginalSelected) mData->exchange();
            mData->retire();
            mData->exchange();
        }
        mData->release(); mData.reset(); return true;
    }
    void GuiRuntime::RetainedRuntime::discard() noexcept
    {
        if (!mData || !mData->idle()) return;
        if (!mData->mDetached) { restore(); return; }
        if (!mData->mOriginalSelected) mData->exchange();
        mData->retire();
        mData->exchange();
        mData->release(); mData.reset();
    }
    void GuiRuntime::beforeShutdown() noexcept
    {
        auto* data = RetainedRuntime::Data::sActive;
        if (!data || !data->alive()) return;
        if (!data->idle()) std::terminate();
        if (data->mDetached)
        {
            data->exchange();
            data->retire();
            data->exchange();
        }
        data->release();
    }
}
