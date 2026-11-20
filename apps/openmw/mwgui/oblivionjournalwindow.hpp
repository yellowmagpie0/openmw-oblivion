#ifndef OPENMW_MWGUI_OBLIVIONJOURNALWINDOW_H
#define OPENMW_MWGUI_OBLIVIONJOURNALWINDOW_H

#include "../mwworld/oblivionquestguidance.hpp"
#include "windowbase.hpp"

namespace MWGui
{
    class OblivionJournalWindow final : public WindowBase
    {
    public:
        OblivionJournalWindow();

        void onOpen() override;
        void onResChange(int width, int height) override;
        void clear() override;
        MyGUI::Widget* getDefaultKeyFocus() override;
        bool onControllerButtonEvent(const SDL_ControllerButtonEvent& arg) override;
        std::string_view getWindowIdForLua() const override { return "Journal"; }

        const ESM::FormKey& trackedQuest() const { return mTracked; }
        void checkTrackedQuest(const std::optional<MWWorld::QuestObjective>& objective);

    private:
        void populate();
        void showSelection();
        void layoutNotes();
        void selectTab(MyGUI::Widget* sender);
        void selectQuest(MyGUI::ListBox*, std::size_t index);
        void toggleDebug(MyGUI::Widget*);
        void track(MyGUI::Widget*);
        void close(MyGUI::Widget*);

        MyGUI::ListBox* mList;
        MyGUI::TextBox* mTitle;
        MyGUI::EditBox* mNotes;
        MyGUI::EditBox* mDebug;
        MyGUI::Button* mActiveButton;
        MyGUI::Button* mFinishedButton;
        MyGUI::Button* mDebugButton;
        MyGUI::Button* mTrackButton;
        std::vector<MWWorld::OblivionJournalQuest> mQuests;
        std::vector<std::size_t> mRows;
        ESM::FormKey mSelected;
        ESM::FormKey mTracked;
        bool mFinished = false;
        bool mShowDebug = false;
    };
}

#endif
