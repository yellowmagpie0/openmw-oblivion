#include <gtest/gtest.h>

#include "apps/openmw/mwclass/npc.hpp"
#include "apps/openmw/mwworld/failedaction.hpp"

#include "inventorywindowmanager.hpp"
#include "oblivionworldfixture.hpp"

#include <memory>
#include <string>

TEST(FailedActionTest, OwnsTemporaryLockMessageUntilExecution)
{
    TestingOpenMW::InventoryWindowManager windows;
    std::string displayed;
    windows.mMessageCallback = [&](std::string_view text, auto) { displayed = text; };
    TestingOpenMW::NativeWorldFixture fixture;
    fixture.mEnvironment.setWindowManager(windows);
    MWClass::Npc::registerSelf();
    fixture.mWorld.setupPlayer();

    // The locked-door activation returns before this message is displayed.
    auto action = std::make_unique<MWWorld::FailedAction>("Locked (level " + std::to_string(7) + ")");
    action->execute(fixture.mWorld.getPlayerPtr(), true);

    EXPECT_EQ(displayed, "Locked (level 7)");
}
