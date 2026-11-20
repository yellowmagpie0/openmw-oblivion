#include "oblivionjournalwindow.hpp"

#include <algorithm>

#include <MyGUI_Button.h>
#include <MyGUI_EditBox.h>
#include <MyGUI_LanguageManager.h>
#include <MyGUI_ListBox.h>
#include <MyGUI_RenderManager.h>

#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

namespace
{
    std::string journalTag(std::string_view key)
    {
        return MyGUI::LanguageManager::getInstance().replaceTags("#{Interface:" + std::string(key) + "}");
    }
}

namespace MWGui
{
    OblivionJournalWindow::OblivionJournalWindow()
        : WindowBase("openmw_oblivion_journal.layout")
    {
        getWidget(mList, "QuestList");
        getWidget(mTitle, "QuestTitle");
        getWidget(mNotes, "QuestNotes");
        getWidget(mDebug, "QuestDebug");
        getWidget(mActiveButton, "ActiveQuests");
        getWidget(mFinishedButton, "FinishedQuests");
        getWidget(mDebugButton, "ShowDebug");
        getWidget(mTrackButton, "TrackQuest");
        MyGUI::Button* closeButton;
        getWidget(closeButton, "Close");
        mList->eventListChangePosition += MyGUI::newDelegate(this, &OblivionJournalWindow::selectQuest);
        mActiveButton->eventMouseButtonClick += MyGUI::newDelegate(this, &OblivionJournalWindow::selectTab);
        mFinishedButton->eventMouseButtonClick += MyGUI::newDelegate(this, &OblivionJournalWindow::selectTab);
        mDebugButton->eventMouseButtonClick += MyGUI::newDelegate(this, &OblivionJournalWindow::toggleDebug);
        mTrackButton->eventMouseButtonClick += MyGUI::newDelegate(this, &OblivionJournalWindow::track);
        closeButton->eventMouseButtonClick += MyGUI::newDelegate(this, &OblivionJournalWindow::close);
        mNotes->setEditReadOnly(true);
        mDebug->setEditReadOnly(true);
        mControllerScrollWidget = mNotes;
        mControllerButtons.mA = "#{Interface:QuestJournalTrack}";
        mControllerButtons.mB = "#{Interface:QuestJournalClose}";
        mControllerButtons.mDpad = "#{Interface:QuestJournalBrowse}";
        mControllerButtons.mY = "#{Interface:QuestJournalDebugShortcut}";
        const auto size = MyGUI::RenderManager::getInstance().getViewSize();
        onResChange(size.width, size.height);
    }

    void OblivionJournalWindow::onOpen()
    {
        mQuests = MWWorld::getOblivionJournal(*MWBase::Environment::get().getWorld());
        populate();
    }

    void OblivionJournalWindow::onResChange(int width, int height)
    {
        mMainWidget->setSize(std::clamp(width - 28, 480, 920), std::clamp(height - 32, 288, 620));
        center();
        layoutNotes();
    }

    void OblivionJournalWindow::layoutNotes()
    {
        const int bodyHeight = mList->getHeight() - 36;
        auto coord = mNotes->getCoord();
        coord.height = mDebug->getVisible() ? bodyHeight / 2 : bodyHeight;
        mNotes->setCoord(coord);
        if (mDebug->getVisible())
            mDebug->setCoord(coord.left, coord.top + coord.height + 8, coord.width, bodyHeight - coord.height - 8);
    }

    void OblivionJournalWindow::clear()
    {
        mQuests.clear();
        mRows.clear();
        mSelected = {};
        mTracked = {};
        mFinished = false;
        mShowDebug = false;
        mList->removeAllItems();
        mNotes->setOnlyText("");
        mDebug->setOnlyText("");
    }

    MyGUI::Widget* OblivionJournalWindow::getDefaultKeyFocus()
    {
        return mList;
    }

    void OblivionJournalWindow::populate()
    {
        mActiveButton->setStateSelected(!mFinished);
        mFinishedButton->setStateSelected(mFinished);
        mRows.clear();
        mList->removeAllItems();
        std::size_t index = MyGUI::ITEM_NONE;
        for (std::size_t i = 0; i < mQuests.size(); ++i)
        {
            const auto& quest = mQuests[i];
            if (quest.mFinished != mFinished)
                continue;
            if (quest.mQuest == mSelected)
                index = mRows.size();
            mRows.push_back(i);
            mList->addItem(quest.mTitle + (quest.mQuest == mTracked ? "  •" : ""));
        }
        if (index == MyGUI::ITEM_NONE && !mRows.empty())
            index = 0;
        mList->setIndexSelected(index);
        if (index != MyGUI::ITEM_NONE)
            mList->beginToItemAt(index);
        selectQuest(mList, index);
    }

    void OblivionJournalWindow::showSelection()
    {
        const auto index = mList->getIndexSelected();
        const auto* quest = index < mRows.size() ? &mQuests[mRows[index]] : nullptr;
        mDebugButton->setEnabled(quest != nullptr);
        mDebugButton->setStateSelected(mShowDebug);
        mTrackButton->setVisible(quest && !quest->mFinished);
        mTrackButton->setCaptionWithReplacing(quest && quest->mQuest == mTracked ? "#{Interface:QuestJournalAutomatic}"
                                                                                 : "#{Interface:QuestJournalTrack}");
        mTitle->setCaption(quest
                ? quest->mTitle + (quest->mQuest == mTracked ? "  •  " + journalTag("QuestJournalTracked") : "")
                : journalTag(mFinished ? "QuestJournalNoFinished" : "QuestJournalNoActive"));

        std::string text;
        if (quest && !quest->mSteps.empty())
        {
            text = journalTag("QuestJournalLatest") + "\n" + quest->mSteps.back().mText;
            if (quest->mSteps.size() > 1)
            {
                text += "\n\n" + journalTag("QuestJournalEarlier");
                for (auto step = quest->mSteps.rbegin() + 1; step != quest->mSteps.rend(); ++step)
                    text += "\n\n" + step->mText;
            }
        }
        else if (quest)
            text = journalTag("QuestJournalNoNotes");
        if (quest && quest->mIncompleteHistory)
            text += "\n\n" + journalTag("QuestJournalIncomplete");
        mNotes->setOnlyText(text);
        mNotes->setVScrollPosition(0);
        const bool debug = quest && mShowDebug;
        mDebug->setVisible(debug);
        mDebug->setOnlyText(debug
                ? MWWorld::getOblivionJournalDebug(*MWBase::Environment::get().getWorld(), quest->mQuest)
                : std::string{});
        mDebug->setVScrollPosition(0);
        mControllerScrollWidget = debug ? mDebug : mNotes;
        layoutNotes();
    }

    void OblivionJournalWindow::selectTab(MyGUI::Widget* sender)
    {
        mFinished = sender == mFinishedButton;
        populate();
    }

    void OblivionJournalWindow::selectQuest(MyGUI::ListBox*, std::size_t index)
    {
        mSelected = index < mRows.size() ? mQuests[mRows[index]].mQuest : ESM::FormKey{};
        showSelection();
    }

    void OblivionJournalWindow::toggleDebug(MyGUI::Widget*)
    {
        mShowDebug = !mShowDebug;
        showSelection();
    }

    void OblivionJournalWindow::track(MyGUI::Widget*)
    {
        if (!mFinished && !mSelected.isNull())
        {
            mTracked = mTracked == mSelected ? ESM::FormKey{} : mSelected;
            populate();
        }
    }

    void OblivionJournalWindow::checkTrackedQuest(const std::optional<MWWorld::QuestObjective>& objective)
    {
        if (mTracked.isNull() || (objective && objective->mQuest == mTracked))
            return;
        mTracked = {};
        if (isVisible())
            populate();
    }

    void OblivionJournalWindow::close(MyGUI::Widget*)
    {
        MWBase::Environment::get().getWindowManager()->removeGuiMode(GM_Journal);
    }

    bool OblivionJournalWindow::onControllerButtonEvent(const SDL_ControllerButtonEvent& arg)
    {
        switch (arg.button)
        {
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                selectTab(mFinished ? mActiveButton : mFinishedButton);
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_UP:
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                if (!mRows.empty())
                {
                    const auto index = wrap(
                        mList->getIndexSelected(), mRows.size(), arg.button == SDL_CONTROLLER_BUTTON_DPAD_UP ? -1 : 1);
                    mList->setIndexSelected(index);
                    mList->beginToItemAt(index);
                    selectQuest(mList, index);
                }
                break;
            case SDL_CONTROLLER_BUTTON_A:
                track(nullptr);
                break;
            case SDL_CONTROLLER_BUTTON_B:
                close(nullptr);
                break;
            case SDL_CONTROLLER_BUTTON_Y:
                if (!mSelected.isNull())
                    toggleDebug(nullptr);
                break;
        }
        return true;
    }
}
