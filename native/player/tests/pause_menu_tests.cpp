#include "player/game_input.h"
#include "player/pause_menu.h"

#include <gtest/gtest.h>

namespace ioj::tests {
TEST(PauseMenu, ImmediateResumeWaitsOnlyForTheQueuedSimulationPause) {
    PauseMenu menu;
    using enum host::PlayerAction;
    using enum sim::LevelState;
    EXPECT_TRUE(menu.handle(OpenPause, Running));
    EXPECT_EQ(menu.simulation_request(Running), levels::LaunchMode::Paused);
    EXPECT_FALSE(menu.simulation_request(Running));
    EXPECT_TRUE(menu.handle(RequestResume, Running));
    EXPECT_TRUE(menu.widget_event(
        {host::PauseWidgetEventKind::LeaveAccepted, host::PauseMenuItem::Resume}));
    EXPECT_EQ(menu.route({InputContext::Observer}).context, InputContext::Observer);
    EXPECT_FALSE(menu.simulation_request(Running));
    EXPECT_EQ(menu.simulation_request(Paused), levels::LaunchMode::Running);
    EXPECT_FALSE(menu.simulation_request(Running));
}

TEST(PauseMenu, PausedLaunchPointerSelectionAndOptionsLeaveConfirmation) {
    PauseMenu menu;
    using enum host::PauseWidgetEventKind;
    using enum host::PauseMenuItem;
    EXPECT_TRUE(menu.handle(host::PlayerAction::OpenPause, sim::LevelState::Paused));
    EXPECT_FALSE(menu.simulation_request(sim::LevelState::Paused));
    EXPECT_TRUE(menu.widget_event({Select, Controls}));
    EXPECT_TRUE(menu.widget_event({Activate, Controls}));
    EXPECT_TRUE(menu.widget_event({LeaveAccepted, Controls}));
    EXPECT_EQ(menu.view, PauseMenu::View::Options);
    EXPECT_EQ(menu.page, Controls);
    EXPECT_TRUE(menu.widget_event({Select, Quit}));
    EXPECT_TRUE(menu.widget_event({Activate, Quit}));
    EXPECT_FALSE(menu.widget_event({ModalOpened}));
    EXPECT_FALSE(menu.route({}).resume_available);
    menu.widget_event({ModalClosed});
    menu.widget_event({LeaveCancelled, Quit});
    EXPECT_EQ(menu.view, PauseMenu::View::Sidebar);
    EXPECT_FALSE(menu.pending);
    EXPECT_FALSE(menu.widget_event({LeaveAccepted, Quit}));
    EXPECT_TRUE(menu.widget_event({Activate, Quit}));
    EXPECT_TRUE(menu.widget_event({LeaveAccepted, Quit}));
    EXPECT_EQ(menu.view, PauseMenu::View::Leaving);
}

}
