#include "player/loaded_level.h"
#include <ioj/menu/player_menu.h>

#include <gtest/gtest.h>

namespace ioj::tests {
TEST(PauseMenu, ImmediateResumeWaitsOnlyForTheQueuedSimulationPause) {
    LevelStorage storage;
    sim::LevelSimInitData data{};
    data.collision.grid_spec = {{4, 4, 4}, {{1000.f, 1000.f, 1000.f}}};
    for (auto type : EnumTraits<sim::EntityType>::values) {
        data.collision.entity_bounds.set_half_extents(type, {{10.f, 10.f, 10.f}});
    }
    storage.load(std::move(data), {});
    auto& level{*storage.get()};
    using enum sim::LevelState;
    level.set_simulation_mode(levels::LaunchMode::Running);
    level.update_pause(true);
    level.update_pause(false);
    EXPECT_EQ(level.simulation.state(), Running);
    level.simulation.advance(level.simulation.clock().tick_period());
    ASSERT_EQ(level.simulation.state(), Paused);
    level.update_pause(false);
    EXPECT_EQ(level.simulation.state(), Running);
}
TEST(PauseMenu, PausedLaunchPointerSelectionAndOptionsLeaveConfirmation) {
    PlayerMenu menu;
    using enum host::PauseMenuItem;
    menu.open(MenuScreen::Pause);
    EXPECT_TRUE(menu.select_pause_item(Controls));
    EXPECT_EQ(menu.navigate_pause(MenuCommand::Accept, false), Controls);
    menu.select_page(MainMenuItem::Controls);
    menu.pause_sidebar = false;
    EXPECT_TRUE(menu.draft.set_setting(GameSetting::MasterVolume, 0.4f));
    EXPECT_TRUE(menu.select_pause_item(Quit));
    EXPECT_FALSE(menu.leave_ready());
    EXPECT_TRUE(menu.navigation_blocked());
    EXPECT_FALSE(menu.select_pause_item(Resume));
    menu.set_transaction(SettingsTransaction::Editing);
    EXPECT_EQ(menu.pause_selection, Quit);
    menu.draft.cancel();
    EXPECT_TRUE(menu.leave_ready());
}
}
