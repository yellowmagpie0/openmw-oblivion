#ifndef OPENMW_TESTS_INVENTORYWINDOWMANAGER_H
#define OPENMW_TESTS_INVENTORYWINDOWMANAGER_H

#include <stdexcept>
#include <functional>
#include "apps/openmw/mwbase/windowmanager.hpp"
#include "apps/openmw/mwworld/ptr.hpp"

namespace TestingOpenMW
{
    // Headless inventory tests observe real update callbacks. Every other UI
    // entry point rejects so the fixture cannot silently bypass new dependencies.
    struct InventoryWindowManager final : MWBase::WindowManager
    {
        mutable std::size_t mInventoryUpdates = 0;
        std::function<void(std::string_view, MWGui::ShowInDialogueMode)> mMessageCallback;
        void playVideo(std::string_view, bool, bool) override { throw std::logic_error("Unexpected WindowManager::playVideo call"); }
        void setNewGame(bool) override { throw std::logic_error("Unexpected WindowManager::setNewGame call"); }
        void pushGuiMode(MWGui::GuiMode, const MWWorld::Ptr&) override { throw std::logic_error("Unexpected WindowManager::pushGuiMode call"); }
        void pushGuiMode(MWGui::GuiMode) override { throw std::logic_error("Unexpected WindowManager::pushGuiMode call"); }
        void popGuiMode(bool) override { throw std::logic_error("Unexpected WindowManager::popGuiMode call"); }
        void removeGuiMode(MWGui::GuiMode) override { throw std::logic_error("Unexpected WindowManager::removeGuiMode call"); }
        void goToJail(int) override { throw std::logic_error("Unexpected WindowManager::goToJail call"); }
        void updatePlayer() override { throw std::logic_error("Unexpected WindowManager::updatePlayer call"); }
        MWGui::GuiMode getMode() const override { throw std::logic_error("Unexpected WindowManager::getMode call"); }
        bool containsMode(MWGui::GuiMode) const override { throw std::logic_error("Unexpected WindowManager::containsMode call"); }
        bool isGuiMode() const override { throw std::logic_error("Unexpected WindowManager::isGuiMode call"); }
        bool isConsoleMode() const override { throw std::logic_error("Unexpected WindowManager::isConsoleMode call"); }
        bool isPostProcessorHudVisible() const override { throw std::logic_error("Unexpected WindowManager::isPostProcessorHudVisible call"); }
        bool isSettingsWindowVisible() const override { throw std::logic_error("Unexpected WindowManager::isSettingsWindowVisible call"); }
        bool isInteractiveMessageBoxActive() const override { throw std::logic_error("Unexpected WindowManager::isInteractiveMessageBoxActive call"); }
        void toggleVisible(MWGui::GuiWindow) override { throw std::logic_error("Unexpected WindowManager::toggleVisible call"); }
        void forceHide(MWGui::GuiWindow) override { throw std::logic_error("Unexpected WindowManager::forceHide call"); }
        void unsetForceHide(MWGui::GuiWindow) override { throw std::logic_error("Unexpected WindowManager::unsetForceHide call"); }
        void disallowAll() override { throw std::logic_error("Unexpected WindowManager::disallowAll call"); }
        void allow(MWGui::GuiWindow) override { throw std::logic_error("Unexpected WindowManager::allow call"); }
        bool isAllowed(MWGui::GuiWindow) const override { throw std::logic_error("Unexpected WindowManager::isAllowed call"); }
        MWGui::InventoryWindow* getInventoryWindow() override { throw std::logic_error("Unexpected WindowManager::getInventoryWindow call"); }
        MWGui::CountDialog* getCountDialog() override { throw std::logic_error("Unexpected WindowManager::getCountDialog call"); }
        MWGui::ConfirmationDialog* getConfirmationDialog() override { throw std::logic_error("Unexpected WindowManager::getConfirmationDialog call"); }
        MWGui::TradeWindow* getTradeWindow() override { throw std::logic_error("Unexpected WindowManager::getTradeWindow call"); }
        MWGui::HUD* getHud() override { throw std::logic_error("Unexpected WindowManager::getHud call"); }
        MWGui::PostProcessorHud* getPostProcessorHud() override { throw std::logic_error("Unexpected WindowManager::getPostProcessorHud call"); }
        std::vector<MWGui::WindowBase*> getGuiModeWindows(MWGui::GuiMode) override { throw std::logic_error("Unexpected WindowManager::getGuiModeWindows call"); }
        void useItem(const MWWorld::Ptr&, bool) override { throw std::logic_error("Unexpected WindowManager::useItem call"); }
        void updateSpellWindow() override { throw std::logic_error("Unexpected WindowManager::updateSpellWindow call"); }
        void setConsoleSelectedObject(const MWWorld::Ptr&) override { throw std::logic_error("Unexpected WindowManager::setConsoleSelectedObject call"); }
        MWWorld::Ptr getConsoleSelectedObject() const override { throw std::logic_error("Unexpected WindowManager::getConsoleSelectedObject call"); }
        void setConsoleMode(std::string_view) override { throw std::logic_error("Unexpected WindowManager::setConsoleMode call"); }
        const std::string& getConsoleMode() override { throw std::logic_error("Unexpected WindowManager::getConsoleMode call"); }
        void printToConsole(const std::string&, std::string_view) override { throw std::logic_error("Unexpected WindowManager::printToConsole call"); }
        void setDrowningTimeLeft(float, float) override { throw std::logic_error("Unexpected WindowManager::setDrowningTimeLeft call"); }
        void changeCell(const MWWorld::CellStore*) override { throw std::logic_error("Unexpected WindowManager::changeCell call"); }
        void setFocusObject(const MWWorld::Ptr&) override { throw std::logic_error("Unexpected WindowManager::setFocusObject call"); }
        void setFocusObjectScreenCoords(float, float) override { throw std::logic_error("Unexpected WindowManager::setFocusObjectScreenCoords call"); }
        void setCursorVisible(bool) override { throw std::logic_error("Unexpected WindowManager::setCursorVisible call"); }
        void setCursorActive(bool) override { throw std::logic_error("Unexpected WindowManager::setCursorActive call"); }
        void getMousePosition(int&, int&) override { throw std::logic_error("Unexpected WindowManager::getMousePosition call"); }
        void getMousePosition(float&, float&) override { throw std::logic_error("Unexpected WindowManager::getMousePosition call"); }
        void setDragDrop(bool) override { throw std::logic_error("Unexpected WindowManager::setDragDrop call"); }
        bool getWorldMouseOver() override { throw std::logic_error("Unexpected WindowManager::getWorldMouseOver call"); }
        float getScalingFactor() const override { throw std::logic_error("Unexpected WindowManager::getScalingFactor call"); }
        bool toggleFogOfWar() override { throw std::logic_error("Unexpected WindowManager::toggleFogOfWar call"); }
        bool toggleFullHelp() override { throw std::logic_error("Unexpected WindowManager::toggleFullHelp call"); }
        bool getFullHelp() const override { throw std::logic_error("Unexpected WindowManager::getFullHelp call"); }
        void setDrowningBarVisibility(bool) override { throw std::logic_error("Unexpected WindowManager::setDrowningBarVisibility call"); }
        void setHMSVisibility(bool) override { throw std::logic_error("Unexpected WindowManager::setHMSVisibility call"); }
        void setMinimapVisibility(bool) override { throw std::logic_error("Unexpected WindowManager::setMinimapVisibility call"); }
        void setWeaponVisibility(bool) override { throw std::logic_error("Unexpected WindowManager::setWeaponVisibility call"); }
        void setSpellVisibility(bool) override { throw std::logic_error("Unexpected WindowManager::setSpellVisibility call"); }
        void setSneakVisibility(bool) override { throw std::logic_error("Unexpected WindowManager::setSneakVisibility call"); }
        void activateQuickKey(int) override { throw std::logic_error("Unexpected WindowManager::activateQuickKey call"); }
        void updateActivatedQuickKey() override { throw std::logic_error("Unexpected WindowManager::updateActivatedQuickKey call"); }
        const ESM::RefId& getSelectedSpell() override { throw std::logic_error("Unexpected WindowManager::getSelectedSpell call"); }
        void setSelectedSpell(const ESM::RefId&, int) override { throw std::logic_error("Unexpected WindowManager::setSelectedSpell call"); }
        void setSelectedEnchantItem(const MWWorld::Ptr&) override { throw std::logic_error("Unexpected WindowManager::setSelectedEnchantItem call"); }
        const MWWorld::Ptr& getSelectedEnchantItem() const override { throw std::logic_error("Unexpected WindowManager::getSelectedEnchantItem call"); }
        void setSelectedWeapon(const MWWorld::Ptr&) override { throw std::logic_error("Unexpected WindowManager::setSelectedWeapon call"); }
        const MWWorld::Ptr& getSelectedWeapon() const override { throw std::logic_error("Unexpected WindowManager::getSelectedWeapon call"); }
        void unsetSelectedSpell() override { throw std::logic_error("Unexpected WindowManager::unsetSelectedSpell call"); }
        void unsetSelectedWeapon() override { throw std::logic_error("Unexpected WindowManager::unsetSelectedWeapon call"); }
        void showCrosshair(bool) override { throw std::logic_error("Unexpected WindowManager::showCrosshair call"); }
        bool setHudVisibility(bool) override { throw std::logic_error("Unexpected WindowManager::setHudVisibility call"); }
        bool isHudVisible() const override { throw std::logic_error("Unexpected WindowManager::isHudVisible call"); }
        void disallowMouse() override { throw std::logic_error("Unexpected WindowManager::disallowMouse call"); }
        void allowMouse() override { throw std::logic_error("Unexpected WindowManager::allowMouse call"); }
        void notifyInputActionBound() override { throw std::logic_error("Unexpected WindowManager::notifyInputActionBound call"); }
        void addVisitedLocation(const std::string&, int, int) override { throw std::logic_error("Unexpected WindowManager::addVisitedLocation call"); }
        void removeDialog(std::unique_ptr<MWGui::Layout>&&) override { throw std::logic_error("Unexpected WindowManager::removeDialog call"); }
        void exitCurrentGuiMode() override { throw std::logic_error("Unexpected WindowManager::exitCurrentGuiMode call"); }
        void messageBox(std::string_view message, MWGui::ShowInDialogueMode mode) override
        {
            if (mMessageCallback)
                mMessageCallback(message, mode);
            else
                throw std::logic_error("Unexpected WindowManager::messageBox call");
        }
        void scheduleMessageBox(std::string, enum MWGui::ShowInDialogueMode) override { throw std::logic_error("Unexpected WindowManager::scheduleMessageBox call"); }
        void staticMessageBox(std::string_view) override { throw std::logic_error("Unexpected WindowManager::staticMessageBox call"); }
        void removeStaticMessageBox() override { throw std::logic_error("Unexpected WindowManager::removeStaticMessageBox call"); }
        void interactiveMessageBox(std::string_view, const std::vector<std::string>&, bool, int) override { throw std::logic_error("Unexpected WindowManager::interactiveMessageBox call"); }
        int readPressedButton() override { throw std::logic_error("Unexpected WindowManager::readPressedButton call"); }
        void updateConsoleObjectPtr(const MWWorld::Ptr&, const MWWorld::Ptr&) override { throw std::logic_error("Unexpected WindowManager::updateConsoleObjectPtr call"); }
        std::string_view getGameSettingString(std::string_view, std::string_view) override { throw std::logic_error("Unexpected WindowManager::getGameSettingString call"); }
        void processChangedSettings(const std::set<std::pair<std::string, std::string>>&) override { throw std::logic_error("Unexpected WindowManager::processChangedSettings call"); }
        void executeInConsole(const std::filesystem::path&) override { throw std::logic_error("Unexpected WindowManager::executeInConsole call"); }
        void enableRest() override { throw std::logic_error("Unexpected WindowManager::enableRest call"); }
        bool getRestEnabled() override { throw std::logic_error("Unexpected WindowManager::getRestEnabled call"); }
        bool getJournalAllowed() override { throw std::logic_error("Unexpected WindowManager::getJournalAllowed call"); }
        bool getPlayerSleeping() override { throw std::logic_error("Unexpected WindowManager::getPlayerSleeping call"); }
        void wakeUpPlayer() override { throw std::logic_error("Unexpected WindowManager::wakeUpPlayer call"); }
        void showSoulgemDialog(MWWorld::Ptr) override { throw std::logic_error("Unexpected WindowManager::showSoulgemDialog call"); }
        void changePointer(const std::string&) override { throw std::logic_error("Unexpected WindowManager::changePointer call"); }
        void setEnemy(const MWWorld::Ptr&) override { throw std::logic_error("Unexpected WindowManager::setEnemy call"); }
        std::size_t getMessagesCount() const override { throw std::logic_error("Unexpected WindowManager::getMessagesCount call"); }
        const Translation::Storage& getTranslationDataStorage() const override { throw std::logic_error("Unexpected WindowManager::getTranslationDataStorage call"); }
        void setKeyFocusWidget(MyGUI::Widget*) override { throw std::logic_error("Unexpected WindowManager::setKeyFocusWidget call"); }
        Loading::Listener* getLoadingScreen() override { throw std::logic_error("Unexpected WindowManager::getLoadingScreen call"); }
        bool getCursorVisible() override { throw std::logic_error("Unexpected WindowManager::getCursorVisible call"); }
        void clear() override { throw std::logic_error("Unexpected WindowManager::clear call"); }
        void write(ESM::ESMWriter&, Loading::Listener&) override { throw std::logic_error("Unexpected WindowManager::write call"); }
        void readRecord(ESM::ESMReader&, uint32_t) override { throw std::logic_error("Unexpected WindowManager::readRecord call"); }
        std::function<void()> prepareGlobalMap(const ESM::GlobalMap&) override { throw std::logic_error("Unexpected WindowManager::void call"); }
        void prepareQuickKeys(const ESM::QuickKeys&, const MWWorld::ESMStore&, const ESM4::RuntimeState*) override { throw std::logic_error("Unexpected WindowManager::prepareQuickKeys call"); }
        size_t countSavedGameRecords() const override { throw std::logic_error("Unexpected WindowManager::countSavedGameRecords call"); }
        bool isSavingAllowed() const override { throw std::logic_error("Unexpected WindowManager::isSavingAllowed call"); }
        void exitCurrentModal() override { throw std::logic_error("Unexpected WindowManager::exitCurrentModal call"); }
        void addCurrentModal(MWGui::WindowModal*) override { throw std::logic_error("Unexpected WindowManager::addCurrentModal call"); }
        void removeCurrentModal(MWGui::WindowModal*) override { throw std::logic_error("Unexpected WindowManager::removeCurrentModal call"); }
        void pinWindow(MWGui::GuiWindow) override { throw std::logic_error("Unexpected WindowManager::pinWindow call"); }
        void toggleMaximized(MWGui::Layout*) override { throw std::logic_error("Unexpected WindowManager::toggleMaximized call"); }
        void fadeScreenIn(const float, bool, float) override { throw std::logic_error("Unexpected WindowManager::fadeScreenIn call"); }
        void fadeScreenOut(const float, bool, float) override { throw std::logic_error("Unexpected WindowManager::fadeScreenOut call"); }
        void fadeScreenTo(const int, const float, bool, float) override { throw std::logic_error("Unexpected WindowManager::fadeScreenTo call"); }
        void setBlindness(const int) override { throw std::logic_error("Unexpected WindowManager::setBlindness call"); }
        void activateHitOverlay(bool) override { throw std::logic_error("Unexpected WindowManager::activateHitOverlay call"); }
        void setWerewolfOverlay(bool) override { throw std::logic_error("Unexpected WindowManager::setWerewolfOverlay call"); }
        void toggleConsole() override { throw std::logic_error("Unexpected WindowManager::toggleConsole call"); }
        void toggleDebugWindow() override { throw std::logic_error("Unexpected WindowManager::toggleDebugWindow call"); }
        void togglePostProcessorHud() override { throw std::logic_error("Unexpected WindowManager::togglePostProcessorHud call"); }
        void toggleSettingsWindow() override { throw std::logic_error("Unexpected WindowManager::toggleSettingsWindow call"); }
        void cycleSpell(bool) override { throw std::logic_error("Unexpected WindowManager::cycleSpell call"); }
        void cycleWeapon(bool) override { throw std::logic_error("Unexpected WindowManager::cycleWeapon call"); }
        void playSound(const ESM::RefId&, float, float) override { throw std::logic_error("Unexpected WindowManager::playSound call"); }
        void addCell(MWWorld::CellStore*) override { throw std::logic_error("Unexpected WindowManager::addCell call"); }
        void removeCell(MWWorld::CellStore*) override { throw std::logic_error("Unexpected WindowManager::removeCell call"); }
        void writeFog(MWWorld::CellStore*) override { throw std::logic_error("Unexpected WindowManager::writeFog call"); }
        const MWGui::TextColours& getTextColours() override { throw std::logic_error("Unexpected WindowManager::getTextColours call"); }
        bool injectKeyPress(MyGUI::KeyCode, unsigned int, bool) override { throw std::logic_error("Unexpected WindowManager::injectKeyPress call"); }
        bool injectKeyRelease(MyGUI::KeyCode) override { throw std::logic_error("Unexpected WindowManager::injectKeyRelease call"); }
        void windowVisibilityChange(bool) override { throw std::logic_error("Unexpected WindowManager::windowVisibilityChange call"); }
        void windowResized(int, int) override { throw std::logic_error("Unexpected WindowManager::windowResized call"); }
        void windowClosed() override { throw std::logic_error("Unexpected WindowManager::windowClosed call"); }
        bool isWindowVisible() const override { throw std::logic_error("Unexpected WindowManager::isWindowVisible call"); }
        void watchActor(const MWWorld::Ptr&) override { throw std::logic_error("Unexpected WindowManager::watchActor call"); }
        MWWorld::Ptr getWatchedActor() const override { throw std::logic_error("Unexpected WindowManager::getWatchedActor call"); }
        const std::string& getVersionDescription() const override { throw std::logic_error("Unexpected WindowManager::getVersionDescription call"); }
        void onDeleteCustomData(const MWWorld::Ptr&) override { throw std::logic_error("Unexpected WindowManager::onDeleteCustomData call"); }
        void forceLootMode(const MWWorld::Ptr&) override { throw std::logic_error("Unexpected WindowManager::forceLootMode call"); }
        void asyncPrepareSaveMap() override { throw std::logic_error("Unexpected WindowManager::asyncPrepareSaveMap call"); }
        void setCullMask(uint32_t) override { throw std::logic_error("Unexpected WindowManager::setCullMask call"); }
        uint32_t getCullMask() override { throw std::logic_error("Unexpected WindowManager::getCullMask call"); }
        void inventoryUpdated(const MWWorld::Ptr&) const override { ++mInventoryUpdates; }
        MWGui::WindowBase* getActiveControllerWindow() override { throw std::logic_error("Unexpected WindowManager::getActiveControllerWindow call"); }
        int getControllerMenuHeight() override { throw std::logic_error("Unexpected WindowManager::getControllerMenuHeight call"); }
        void cycleActiveControllerWindow(bool) override { throw std::logic_error("Unexpected WindowManager::cycleActiveControllerWindow call"); }
        void setActiveControllerWindow(MWGui::GuiMode, size_t) override { throw std::logic_error("Unexpected WindowManager::setActiveControllerWindow call"); }
        bool getControllerTooltipVisible() const override { throw std::logic_error("Unexpected WindowManager::getControllerTooltipVisible call"); }
        void setControllerTooltipVisible(bool) override { throw std::logic_error("Unexpected WindowManager::setControllerTooltipVisible call"); }
        bool getControllerTooltipEnabled() const override { throw std::logic_error("Unexpected WindowManager::getControllerTooltipEnabled call"); }
        void setControllerTooltipEnabled(bool) override { throw std::logic_error("Unexpected WindowManager::setControllerTooltipEnabled call"); }
        void restoreControllerTooltips() override { throw std::logic_error("Unexpected WindowManager::restoreControllerTooltips call"); }
        void updateControllerButtonsOverlay() override { throw std::logic_error("Unexpected WindowManager::updateControllerButtonsOverlay call"); }
        const std::vector<MWGui::GuiMode>& getGuiModeStack() const override { throw std::logic_error("Unexpected WindowManager::getGuiModeStack call"); }
        void setDisabledByLua(std::string_view, bool) override { throw std::logic_error("Unexpected WindowManager::setDisabledByLua call"); }
        bool isWindowVisible(std::string_view) const override { throw std::logic_error("Unexpected WindowManager::isWindowVisible call"); }
        std::vector<std::string_view> getAllWindowIds() const override { throw std::logic_error("Unexpected WindowManager::getAllWindowIds call"); }
        std::vector<std::string_view> getAllowedWindowIds(MWGui::GuiMode) const override { throw std::logic_error("Unexpected WindowManager::getAllowedWindowIds call"); }
    };
}

#endif
