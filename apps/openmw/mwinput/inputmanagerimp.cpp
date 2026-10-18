#include "inputmanagerimp.hpp"

#include <osgViewer/ViewerEventHandlers>
#include <stdexcept>
#include <tuple>

#include <components/esm3/esmreader.hpp>
#include <components/esm3/esmwriter.hpp>
#include <components/sdlutil/sdlinputwrapper.hpp>
#include <components/settings/values.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwworld/esmstore.hpp"

#include "actionmanager.hpp"
#include "bindingsmanager.hpp"
#include "controllermanager.hpp"
#include "controlswitch.hpp"
#include "gyromanager.hpp"
#include "keyboardmanager.hpp"
#include "mousemanager.hpp"
#include "sensormanager.hpp"

namespace MWInput
{
    namespace
    {
        template<class... T>
        void exchangeFields(std::tuple<T...>& state, T&... fields) noexcept
        {
            const auto outgoing = std::make_tuple(fields...);
            std::tie(fields...) = state;
            state = outgoing;
        }

        struct RuntimeCall
        {
            unsigned& depth;
            explicit RuntimeCall(unsigned& value) : depth(value) { ++depth; }
            ~RuntimeCall() { --depth; }
        };
    }

    struct InputManager::RetainedRuntime::Data
    {
        InputManager* owner;
        BindingsManager::RuntimeHandle bindings;
        SDLUtil::InputWrapper::RuntimeState wrapper;
        ControlSwitch switches;
        float idleTime = 0;
        bool controlsDisabled = false;
        std::tuple<float, float, int, bool, bool, float, float, int, int> mouse;
        std::tuple<bool, bool, bool, bool, bool, bool> controller{true, true, false, false, false, false};
        bool gyroGuiCursor = true;
        osg::Vec3f gyroValues;
        float gyroTimer = 0;
        bool detached = false;
        bool originalSelected = true;

        explicit Data(InputManager& input)
            : owner(&input), bindings(input.mBindingsManager->prepareRuntime())
            , wrapper(input.mInputWrapper->prepareRuntime())
            , mouse(input.mMouseManager->mGuiCursorX, input.mMouseManager->mGuiCursorY,
                  0, false, true, -1.f, -1.f, 0, 0)
        {
        }

        bool select() noexcept
        {
            if (!owner || owner->mActiveCalls || !owner->mBindingsManager->exchangeRuntime(*bindings))
                return false;
            auto& input = *owner;
            input.mControlSwitch->mSwitches.swap(switches.mSwitches);
            std::swap(input.mActionManager->mTimeIdle, idleTime);
            std::swap(input.mControlsDisabled, controlsDisabled);
            auto& m = *input.mMouseManager;
            exchangeFields(mouse, m.mGuiCursorX, m.mGuiCursorY, m.mMouseWheel, m.mMouseLookEnabled, m.mGuiCursorEnabled,
                m.mLastWarpX, m.mLastWarpY, m.mMouseMoveX, m.mMouseMoveY);
            auto& c = *input.mControllerManager;
            exchangeFields(controller, c.mGamepadGuiCursorEnabled, c.mGuiCursorEnabled, c.mJoystickLastUsed, c.mGamepadMousePressed,
                c.mLeftTriggerGuiPressed, c.mRightTriggerGuiPressed);
            std::swap(input.mGyroManager->mGuiCursorEnabled, gyroGuiCursor);
            std::swap(input.mSensorManager->mGyroValues, gyroValues);
            std::swap(input.mSensorManager->mGyroUpdateTimer, gyroTimer);
            input.mInputWrapper->exchangeRuntime(wrapper);
            originalSelected = !originalSelected;
            return true;
        }
    };

    InputManager::RetainedRuntime::RetainedRuntime(std::unique_ptr<Data> data) : mData(std::move(data)) {}
    InputManager::RetainedRuntime::~RetainedRuntime() { if (mData && mData->owner && !restore()) std::terminate(); }
    InputManager::RetainedRuntime::RetainedRuntime(RetainedRuntime&&) noexcept = default;
    InputManager::RetainedRuntime& InputManager::RetainedRuntime::operator=(RetainedRuntime&& other) noexcept
    {
        if (this != &other)
        {
            if (mData && mData->owner && !restore()) std::terminate();
            mData = std::move(other.mData);
        }
        return *this;
    }
    InputManager::RetainedRuntime InputManager::retainRuntime()
    {
        if (mRetainedRuntime || mActiveCalls) throw std::logic_error("Input runtime retention requires idle users");
        auto data = std::make_unique<RetainedRuntime::Data>(*this);
        mRetainedRuntime = data.get();
        return RetainedRuntime(std::move(data));
    }
    void InputManager::requireMutableRuntime() const
    {
        if (mRetainedRuntime && !mRetainedRuntime->detached)
            throw std::logic_error("Input runtime is prepared but not detached");
    }
    bool InputManager::RetainedRuntime::detach() noexcept
    {
        if (!mData || !mData->owner || mData->detached || !mData->select()) return false;
        mData->detached = true;
        return true;
    }
    bool InputManager::RetainedRuntime::exchange() noexcept
    {
        return mData && mData->detached && mData->select();
    }
    bool InputManager::RetainedRuntime::restore() noexcept
    {
        if (!mData || !mData->owner || mData->owner->mActiveCalls) return false;
        if (mData->detached && !mData->originalSelected && !mData->select()) return false;
        mData->owner->mRetainedRuntime = nullptr;
        mData.reset();
        return true;
    }
    void InputManager::RetainedRuntime::discard() noexcept
    {
        if (!mData || !mData->owner) return;
        if (mData->owner->mActiveCalls) std::terminate();
        if (!mData->detached) { restore(); return; }
        if (mData->originalSelected && !mData->select()) std::terminate();
        mData->owner->mRetainedRuntime = nullptr;
        mData.reset();
    }

    InputManager::InputManager(SDL_Window* window, osg::ref_ptr<osgViewer::Viewer> viewer,
        osg::ref_ptr<osgViewer::ScreenCaptureHandler> screenCaptureHandler, const std::filesystem::path& userFile,
        bool userFileExists, const std::filesystem::path& userControllerBindingsFile,
        const std::filesystem::path& controllerBindingsFile, bool grab, ESM::GameProfile gameProfile)
        : mControlsDisabled(false)
        , mInputWrapper(std::make_unique<SDLUtil::InputWrapper>(window, viewer, grab))
        , mBindingsManager(std::make_unique<BindingsManager>(userFile, userFileExists, gameProfile))
        , mControlSwitch(std::make_unique<ControlSwitch>())
        , mActionManager(std::make_unique<ActionManager>(mBindingsManager.get(), viewer, screenCaptureHandler))
        , mKeyboardManager(std::make_unique<KeyboardManager>(mBindingsManager.get()))
        , mMouseManager(std::make_unique<MouseManager>(mBindingsManager.get(), mInputWrapper.get(), window))
        , mControllerManager(std::make_unique<ControllerManager>(
              mBindingsManager.get(), mMouseManager.get(), userControllerBindingsFile, controllerBindingsFile))
        , mSensorManager(std::make_unique<SensorManager>())
        , mGyroManager(std::make_unique<GyroManager>())
    {
        mInputWrapper->setWindowEventCallback(MWBase::Environment::get().getWindowManager());
        mInputWrapper->setKeyboardEventCallback(mKeyboardManager.get());
        mInputWrapper->setMouseEventCallback(mMouseManager.get());
        mInputWrapper->setControllerEventCallback(mControllerManager.get());
        mInputWrapper->setSensorEventCallback(mSensorManager.get());
    }

    void InputManager::clear()
    {
        requireMutableRuntime();
        RuntimeCall call(mActiveCalls);
        // Enable all controls
        mControlSwitch->clear();
    }

    InputManager::~InputManager()
    {
        if (mRetainedRuntime) mRetainedRuntime->owner = nullptr;
    }

    void InputManager::update(float dt, bool disableControls, bool disableEvents)
    {
        captureEvents(disableControls, disableEvents);
        updateControls(dt, disableControls);
    }

    void InputManager::captureEvents(bool disableControls, bool disableEvents)
    {
        requireMutableRuntime();
        RuntimeCall call(mActiveCalls);
        mControlsDisabled = disableControls;

        mInputWrapper->setMouseVisible(MWBase::Environment::get().getWindowManager()->getCursorVisible());
        mInputWrapper->capture(disableEvents);
    }

    void InputManager::updateControls(float dt, bool disableControls)
    {
        requireMutableRuntime();
        RuntimeCall call(mActiveCalls);
        if (disableControls)
        {
            mMouseManager->updateCursorMode();
            return;
        }

        mBindingsManager->update(dt);

        mMouseManager->updateCursorMode();

        mControllerManager->update(dt);
        mMouseManager->update(dt);
        mSensorManager->update(dt);
        mActionManager->update(dt);

        if (Settings::input().mEnableGyroscope)
        {
            bool controllerAvailable = mControllerManager->isGyroAvailable();
            bool sensorAvailable = mSensorManager->isGyroAvailable();
            if (controllerAvailable || sensorAvailable)
            {
                mGyroManager->update(
                    dt, controllerAvailable ? mControllerManager->getGyroValues() : mSensorManager->getGyroValues());
            }
        }
    }

    void InputManager::setDragDrop(bool dragDrop)
    {
        requireMutableRuntime();
        RuntimeCall call(mActiveCalls);
        mBindingsManager->setDragDrop(dragDrop);
    }

    void InputManager::setGamepadGuiCursorEnabled(bool enabled)
    {
        requireMutableRuntime();
        RuntimeCall call(mActiveCalls);
        mControllerManager->setGamepadGuiCursorEnabled(enabled);
    }

    bool InputManager::isGamepadGuiCursorEnabled()
    {
        return mControllerManager->gamepadGuiCursorEnabled();
    }

    void InputManager::changeInputMode(bool guiMode)
    {
        requireMutableRuntime();
        RuntimeCall call(mActiveCalls);
        mControllerManager->setGuiCursorEnabled(guiMode);
        mMouseManager->setGuiCursorEnabled(guiMode);
        mGyroManager->setGuiCursorEnabled(guiMode);
        mMouseManager->setMouseLookEnabled(!guiMode);
        if (guiMode)
            MWBase::Environment::get().getWindowManager()->showCrosshair(false);

        bool isCursorVisible
            = guiMode && (!mControllerManager->joystickLastUsed() || mControllerManager->gamepadGuiCursorEnabled());
        MWBase::Environment::get().getWindowManager()->setCursorVisible(isCursorVisible);
        // if not in gui mode, the camera decides whether to show crosshair or not.
    }

    void InputManager::processChangedSettings(const Settings::CategorySettingVector& changed)
    {
        requireMutableRuntime();
        RuntimeCall call(mActiveCalls);
        mSensorManager->processChangedSettings(changed);
    }

    bool InputManager::getControlSwitch(std::string_view sw)
    {
        return mControlSwitch->get(sw);
    }

    void InputManager::toggleControlSwitch(std::string_view sw, bool value)
    {
        requireMutableRuntime();
        RuntimeCall call(mActiveCalls);
        mControlSwitch->set(sw, value);
    }

    void InputManager::resetIdleTime()
    {
        requireMutableRuntime();
        RuntimeCall call(mActiveCalls);
        mActionManager->resetIdleTime();
    }

    bool InputManager::isIdle() const
    {
        return mActionManager->getIdleTime() > 0.5;
    }

    std::string_view InputManager::getActionDescription(int action) const
    {
        return mBindingsManager->getActionDescription(action);
    }

    std::string InputManager::getActionKeyBindingName(int action) const
    {
        return mBindingsManager->getActionKeyBindingName(action);
    }

    std::string InputManager::getActionControllerBindingName(int action) const
    {
        return mBindingsManager->getActionControllerBindingName(action);
    }

    bool InputManager::actionIsActive(int action) const
    {
        return mBindingsManager->actionIsActive(action);
    }

    float InputManager::getActionValue(int action) const
    {
        return mBindingsManager->getActionValue(action);
    }

    bool InputManager::isControllerButtonPressed(SDL_GameControllerButton button) const
    {
        return mControllerManager->isButtonPressed(button);
    }

    float InputManager::getControllerAxisValue(SDL_GameControllerAxis axis) const
    {
        return mControllerManager->getAxisValue(axis);
    }

    int InputManager::getMouseMoveX() const
    {
        return mMouseManager->getMouseMoveX();
    }

    int InputManager::getMouseMoveY() const
    {
        return mMouseManager->getMouseMoveY();
    }

    void InputManager::warpMouseToWidget(MyGUI::Widget* widget)
    {
        requireMutableRuntime();
        RuntimeCall call(mActiveCalls);
        // This is currently used to simulate mouse movement when the gamepad UI is used.
        // Sometimes this is called in reaction to layout changes.
        // It's a bad idea to do this if the user triggered one with the actual mouse.

        // Don't warp if a gamepad wasn't in use when this was triggered.
        if (!joystickLastUsed())
            return;

        // Don't warp if the mouse button is actively being held.
        // TODO: this should be a method somewhere so that it can be reused in, e.g., Lua bindings
        if (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_LMASK)
            return;

        // Don't warp if an emulated mouse press is occurring.
        if (isGamepadGuiCursorEnabled() && isControllerButtonPressed(SDL_CONTROLLER_BUTTON_A))
            return;

        MWBase::Environment::get().getWindowManager()->setCursorVisible(false);
        mMouseManager->warpMouseToWidget(widget);
        mMouseManager->injectMouseMove(1, 0, 0);
        MWBase::Environment::get().getWindowManager()->setCursorActive(true);
    }

    const std::initializer_list<int>& InputManager::getActionKeySorting()
    {
        return mBindingsManager->getActionKeySorting();
    }

    const std::initializer_list<int>& InputManager::getActionControllerSorting()
    {
        return mBindingsManager->getActionControllerSorting();
    }

    void InputManager::enableDetectingBindingMode(int action, bool keyboard)
    {
        requireMutableRuntime();
        RuntimeCall call(mActiveCalls);
        mBindingsManager->enableDetectingBindingMode(action, keyboard);
    }

    size_t InputManager::countSavedGameRecords() const
    {
        return mControlSwitch->countSavedGameRecords();
    }

    void InputManager::write(ESM::ESMWriter& writer, Loading::Listener& progress)
    {
        mControlSwitch->write(writer, progress);
    }

    void InputManager::readRecord(ESM::ESMReader& reader, uint32_t type)
    {
        requireMutableRuntime();
        RuntimeCall call(mActiveCalls);
        if (type == ESM::REC_INPU)
        {
            mControlSwitch->readRecord(reader, type);
        }
    }

    void InputManager::resetToDefaultKeyBindings()
    {
        requireMutableRuntime();
        RuntimeCall call(mActiveCalls);
        mBindingsManager->loadKeyDefaults(true);
    }

    void InputManager::resetToDefaultControllerBindings()
    {
        requireMutableRuntime();
        RuntimeCall call(mActiveCalls);
        mBindingsManager->loadControllerDefaults(true);
    }

    void InputManager::setJoystickLastUsed(bool enabled)
    {
        requireMutableRuntime();
        RuntimeCall call(mActiveCalls);
        mControllerManager->setJoystickLastUsed(enabled);
    }

    bool InputManager::joystickLastUsed()
    {
        return mControllerManager->joystickLastUsed();
    }

    std::string InputManager::getControllerButtonIcon(int button)
    {
        return mControllerManager->getControllerButtonIcon(button);
    }

    std::string InputManager::getControllerAxisIcon(int axis)
    {
        return mControllerManager->getControllerAxisIcon(axis);
    }

    void InputManager::executeAction(int action)
    {
        requireMutableRuntime();
        RuntimeCall call(mActiveCalls);
        mActionManager->executeAction(action);
    }

    void InputManager::saveBindings()
    {
        mBindingsManager->saveBindings();
    }
}
