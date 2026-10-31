#include <ioj/levels/authoring/launch_state.h>
#include <ioj/levels/level_definition_components.h>

#include <gtest/gtest.h>

#include <limits>

namespace ioj::levels::authoring::tests {
TEST(LaunchState, TransfersNativeDefinitionExactlyOnceAndClearsPreviousError) {
    LevelDefinitionComponents components;
    components.metadata.id = LevelId{"example"};
    components.metadata.title = "Native Example";
    components.player_entity_id = EntityId{"player"};
    components.entities.push_back(
        {.id = EntityId{"player"}, .archetype = EntityArchetype::PlayerFighter});
    auto definition{LevelDefinition::make(std::move(components))};
    ASSERT_TRUE(definition);

    LaunchState state;
    state.set_launch_error("old error");
    state.set_pending_level(std::move(*definition),
                            "LevelScripts/Example.scm",
                            {.launch_mode = LaunchMode::Paused,
                             .requested_time_scale = 4.0,
                             .control_context = ControlContext::Benchmark});
    EXPECT_FALSE(state.has_launch_error());
    auto pending{state.take_pending_level()};
    ASSERT_TRUE(pending);
    EXPECT_EQ(pending->source_path, "LevelScripts/Example.scm");
    EXPECT_EQ(std::get<LevelDefinition>(pending->source).metadata().title, "Native Example");
    EXPECT_EQ(pending->options.launch_mode, LaunchMode::Paused);
    EXPECT_EQ(pending->options.requested_time_scale, 4.0);
    EXPECT_EQ(pending->options.control_context, ControlContext::Benchmark);
    EXPECT_FALSE(state.take_pending_level());
}

TEST(LaunchState, NavigationAndLaunchFailureInvalidatePendingWork) {
    LaunchState state;
    state.set_pending_level(std::string{"authored source"}, "level.scm");
    ASSERT_TRUE(state.request_menu({.destination = MenuDestination::Telemetry,
                                    .selected_telemetry_run_id = "run",
                                    .telemetry_error = "write failed"}));
    EXPECT_FALSE(state.take_pending_level());
    EXPECT_FALSE(state.request_menu({}));
    auto request{state.take_menu_request()};
    ASSERT_TRUE(request);
    EXPECT_EQ(request->destination, MenuDestination::Telemetry);
    EXPECT_EQ(request->selected_telemetry_run_id, "run");
    EXPECT_EQ(request->telemetry_error, "write failed");
    EXPECT_FALSE(state.take_menu_request());
    EXPECT_TRUE(state.request_menu({.preferred_level_id = LevelId{"retry"}}));

    state.set_pending_level(std::string{"source"}, "level.scm");
    state.set_launch_error("invalid configuration");
    EXPECT_FALSE(state.take_pending_level());
    EXPECT_EQ(state.take_launch_error(), "invalid configuration");
    EXPECT_FALSE(state.has_launch_error());
}

TEST(LaunchOptions, ValidatesTimeScaleAndHeadlessTerminationLimits) {
    for (auto const value : {0.25, 1.0, 100.0}) {
        EXPECT_TRUE(validate_launch_options({.requested_time_scale = value}));
    }
    for (auto const value : {0.0,
                             -1.0,
                             100.0001,
                             std::numeric_limits<double>::infinity(),
                             std::numeric_limits<double>::quiet_NaN()}) {
        EXPECT_FALSE(validate_launch_options({.requested_time_scale = value}));
    }
    LaunchOptions headless{.presentation_mode = PresentationMode::SimulationOnly};
    EXPECT_FALSE(validate_launch_options(headless));
    headless.stop_when_battle_resolved = true;
    EXPECT_TRUE(validate_launch_options(headless));
    headless.simulated_duration_seconds = -1.0;
    EXPECT_FALSE(validate_launch_options(headless));
    headless.simulated_duration_seconds = 2.0;
    EXPECT_TRUE(validate_launch_options(headless));
}

TEST(ModalResumeState, PreservesObserverContextAndTracksPlayerAvailabilityDuringSuspension) {
    ModalResumeState state;
    EXPECT_EQ(state.context(ControlContext::Player), ControlContext::Player);
    state.suspend(ControlContext::Observer);
    state.on_player_changed(false);
    EXPECT_EQ(state.context(ControlContext::Player), ControlContext::Observer);
    state.on_player_changed(true);
    EXPECT_EQ(state.context(ControlContext::Observer), ControlContext::Player);
    state.on_player_changed(false);
    EXPECT_EQ(state.context(ControlContext::Observer), ControlContext::None);
    state.clear();
    EXPECT_FALSE(state.pending());
    state.on_player_changed(true);
    EXPECT_EQ(state.context(ControlContext::Benchmark), ControlContext::Benchmark);
}
}
