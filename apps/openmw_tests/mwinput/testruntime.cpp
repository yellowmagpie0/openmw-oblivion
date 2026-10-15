#include <gtest/gtest.h>

#include <oics/ICSInputControlSystem.h>
#include <SDL.h>
#include <osgViewer/Viewer>
#include <functional>

#include "apps/openmw/mwbase/environment.hpp"
#include "apps/openmw/mwinput/inputmanagerimp.hpp"
#include "../mwworld/inventorywindowmanager.hpp"
#include <components/sdlutil/sdlinputwrapper.hpp>
#include <components/testing/util.hpp>

namespace
{
    struct ChannelListener final : ICS::ChannelListener
    {
        int calls = 0;
        void channelChanged(ICS::Channel*, float, float) override { ++calls; }
    };

    struct InspectableControl final : ICS::Control
    {
        InspectableControl() : Control("held", false, false, 0.f, 1.f, 1.f) {}
        const void* pendingNode() const { return mPendingActions.empty() ? nullptr : &mPendingActions.front(); }
    };
}

TEST(InputRuntime, HeldAndPendingActionsExchangeSilentlyWithoutReplacingBindings)
{
    ICS::InputControlSystem input("", true, nullptr, nullptr, 1);
    auto* control = new InspectableControl;
    input.addControl(control);
    auto* channel = input.getChannel(0);
    control->attachChannel(channel, ICS::Channel::DIRECT);
    input.addKeyBinding(control, SDL_SCANCODE_W, ICS::Control::INCREASE);
    ChannelListener listener;
    channel->addListener(&listener);
    control->setValue(0.75f);
    control->setIgnoreAutoReverse(true);
    control->setChangingDirection(ICS::Control::INCREASE);
    const void* pending = control->pendingNode();
    input.enableDetectingBindingState(control, ICS::Control::DECREASE);
    auto runtime = input.prepareRuntime();
    const int calls = listener.calls;

    ASSERT_TRUE(input.exchangeRuntime(runtime));
    EXPECT_EQ(listener.calls, calls);
    EXPECT_FLOAT_EQ(control->getValue(), 0.f);
    EXPECT_FLOAT_EQ(channel->getValue(), 0.f);
    EXPECT_EQ(control->getChangingDirection(), ICS::Control::STOP);
    EXPECT_EQ(control->pendingNode(), nullptr);
    EXPECT_FALSE(input.detectingBindingState());
    EXPECT_EQ(input.getKeyBinding(control, ICS::Control::INCREASE), SDL_SCANCODE_W);

    control->setValue(0.25f);
    control->setChangingDirection(ICS::Control::DECREASE);
    const void* candidatePending = control->pendingNode();
    const int candidateCalls = listener.calls;
    ASSERT_TRUE(input.exchangeRuntime(runtime));
    EXPECT_EQ(listener.calls, candidateCalls);
    EXPECT_EQ(input.getControl(0), control);
    EXPECT_EQ(input.getChannel(0), channel);
    EXPECT_EQ(control->pendingNode(), pending);
    EXPECT_FLOAT_EQ(control->getValue(), 0.75f);
    EXPECT_FLOAT_EQ(channel->getValue(), 0.75f);
    EXPECT_TRUE(control->isAutoReverseIgnored());
    EXPECT_TRUE(input.detectingBindingState());
    EXPECT_EQ(control->getChangingDirection(), ICS::Control::INCREASE);
    ASSERT_TRUE(input.exchangeRuntime(runtime));
    EXPECT_EQ(control->pendingNode(), candidatePending);
    EXPECT_FLOAT_EQ(control->getValue(), 0.25f);
    channel->removeListener(&listener);
}

TEST(InputRuntime, ForeignOwnerAndChangedTopologyRejectBeforeMutation)
{
    ICS::InputControlSystem input("", true, nullptr, nullptr, 1);
    ICS::InputControlSystem other("", true, nullptr, nullptr, 1);
    auto* control = new InspectableControl;
    input.addControl(control);
    control->setValue(0.75f);
    auto runtime = input.prepareRuntime();
    EXPECT_FALSE(other.exchangeRuntime(runtime));
    EXPECT_FLOAT_EQ(control->getValue(), 0.75f);
    input.addControl(new InspectableControl);
    EXPECT_FALSE(input.exchangeRuntime(runtime));
    EXPECT_FLOAT_EQ(control->getValue(), 0.75f);
}

namespace
{
    struct InputWindows final : TestingOpenMW::InventoryWindowManager
    {
        std::function<void()> cursorCallback;
        float getScalingFactor() const override { return 1.f; }
        bool getCursorVisible() override
        {
            if (cursorCallback) cursorCallback();
            return false;
        }
    };

    class InputRuntimeFacade : public testing::Test
    {
    protected:
        MWBase::Environment environment;
        InputWindows windows;
        SDL_Window* window = nullptr;
        osg::ref_ptr<osgViewer::Viewer> viewer = new osgViewer::Viewer;
        std::unique_ptr<MWInput::InputManager> input;
        static constexpr Uint32 subsystems = SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_SENSOR;
        void SetUp() override
        {
            ASSERT_EQ(SDL_InitSubSystem(subsystems), 0) << SDL_GetError();
            window = SDL_CreateWindow("input retention", 0, 0, 160, 120, SDL_WINDOW_HIDDEN);
            ASSERT_NE(window, nullptr) << SDL_GetError();
            environment.setWindowManager(windows);
            input = std::make_unique<MWInput::InputManager>(window, viewer, nullptr,
                TestingOpenMW::currentTestDirPath() / "bindings.xml", false,
                std::filesystem::path{}, std::filesystem::path{},
                false, ESM::GameProfile::Oblivion);
            environment.setInputManager(*input);
        }
        void TearDown() override
        {
            input.reset();
            if (window) SDL_DestroyWindow(window);
            SDL_QuitSubSystem(subsystems);
        }
    };
}

TEST_F(InputRuntimeFacade, SwitchesAndControllerModesRestoreAcrossMoveAndScope)
{
    input->toggleControlSwitch("playerjumping", false);
    input->setJoystickLastUsed(true);
    input->setGamepadGuiCursorEnabled(false);
    {
        auto retained = input->retainRuntime();
        EXPECT_THROW(input->clear(), std::logic_error);
        EXPECT_THROW(input->retainRuntime(), std::logic_error);
        ASSERT_TRUE(retained.detach());
        EXPECT_TRUE(input->getControlSwitch("playerjumping"));
        EXPECT_FALSE(input->joystickLastUsed());
        EXPECT_TRUE(input->isGamepadGuiCursorEnabled());
        input->toggleControlSwitch("playerfighting", false);
        ASSERT_TRUE(retained.exchange());
        EXPECT_FALSE(input->getControlSwitch("playerjumping"));
        EXPECT_TRUE(input->getControlSwitch("playerfighting"));
        EXPECT_TRUE(input->joystickLastUsed());
        EXPECT_FALSE(input->isGamepadGuiCursorEnabled());
        ASSERT_TRUE(retained.exchange());
        EXPECT_FALSE(input->getControlSwitch("playerfighting"));
        auto moved = std::move(retained);
        EXPECT_FALSE(retained.restore());
    }
    EXPECT_FALSE(input->getControlSwitch("playerjumping"));
    EXPECT_TRUE(input->getControlSwitch("playerfighting"));
    EXPECT_TRUE(input->joystickLastUsed());
    EXPECT_FALSE(input->isGamepadGuiCursorEnabled());
}

TEST_F(InputRuntimeFacade, CommitAndOwnerDeathReleaseArchives)
{
    input->toggleControlSwitch("playerjumping", false);
    auto retained = input->retainRuntime();
    ASSERT_TRUE(retained.detach());
    input->toggleControlSwitch("playerfighting", false);
    retained.discard();
    EXPECT_TRUE(input->getControlSwitch("playerjumping"));
    EXPECT_FALSE(input->getControlSwitch("playerfighting"));
    auto orphan = input->retainRuntime();
    ASSERT_TRUE(orphan.detach());
    input.reset();
    EXPECT_FALSE(orphan.exchange());
    EXPECT_FALSE(orphan.restore());
    orphan.discard();
}

TEST_F(InputRuntimeFacade, SelectionRejectsInsideInputCallbacksAndRecoversAfterUnwind)
{
    auto retained = input->retainRuntime();
    ASSERT_TRUE(retained.detach());
    windows.cursorCallback = [&]
    {
        EXPECT_FALSE(retained.exchange());
        EXPECT_FALSE(retained.restore());
        EXPECT_THROW(input->retainRuntime(), std::logic_error);
        throw std::runtime_error("stop before pumping fixture events");
    };
    EXPECT_THROW(input->update(0.f, true, true), std::runtime_error);
    windows.cursorCallback = {};
    EXPECT_TRUE(retained.restore());
}

TEST_F(InputRuntimeFacade, WrapperRestoresWishesAndFractionalWheelAgainstCurrentWindow)
{
    SDLUtil::InputWrapper wrapper(window, viewer, false);
    auto retained = wrapper.prepareRuntime();
    retained.wantMouseVisible = false;
    retained.wantRelative = true;
    retained.wantGrab = true;
    retained.pendingWheelY = 0.375;
    retained.mouseZ = 12;
    retained.warpX = 23;
    retained.warpY = 41;
    retained.warpCompensate = true;
    wrapper.exchangeRuntime(retained);
    auto incoming = wrapper.prepareRuntime();
    wrapper.exchangeRuntime(incoming);
    EXPECT_DOUBLE_EQ(incoming.pendingWheelY, 0.375);
    EXPECT_EQ(incoming.mouseZ, 12);
    EXPECT_TRUE(incoming.wantRelative);
    EXPECT_TRUE(incoming.wantGrab);
    EXPECT_FALSE(incoming.wantMouseVisible);
    EXPECT_EQ(incoming.warpX, 23);
    EXPECT_EQ(incoming.warpY, 41);
    EXPECT_TRUE(incoming.warpCompensate);
    wrapper.exchangeRuntime(incoming);
    SDLUtil::InputWrapper::RuntimeState check;
    wrapper.exchangeRuntime(check);
    EXPECT_DOUBLE_EQ(check.pendingWheelY, 0.375);
    EXPECT_TRUE(check.wantRelative);
}
