#include <ioj/levels/level_definition_components.h>
#include <ioj/session/launch_state.h>
#include <ioj/session/modal_resume_state.h>

#include <gtest/gtest.h>

#include <limits>

namespace ioj::session::tests {
using namespace levels;
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
                             .requested_time_scale = launch::TimeScale{4.0},
                             .control_context = ControlContext::Benchmark});
    EXPECT_FALSE(state.has_launch_error());
    auto pending{state.take_pending_level()};
    ASSERT_TRUE(pending);
    EXPECT_EQ(pending->source_path, "LevelScripts/Example.scm");
    EXPECT_EQ(pending->definition.metadata().title, "Native Example");
    EXPECT_EQ(pending->options.launch_mode, LaunchMode::Paused);
    EXPECT_EQ(pending->options.requested_time_scale.get(), 4.0);
    EXPECT_EQ(pending->options.control_context, ControlContext::Benchmark);
    EXPECT_FALSE(state.take_pending_level());
}

TEST(LaunchState, NavigationAndLaunchFailureInvalidatePendingWork) {
    LevelDefinitionComponents components;
    components.metadata.id = LevelId{"navigation"};
    components.metadata.title = "Navigation";
    components.player_entity_id = EntityId{"player"};
    components.entities.push_back(
        {.id = EntityId{"player"}, .archetype = EntityArchetype::PlayerFighter});
    auto definition{LevelDefinition::make(std::move(components))};
    ASSERT_TRUE(definition);
    LaunchState state;
    state.set_pending_level(*definition, "level.scm");
    ASSERT_TRUE(state.request_menu(TelemetryRequest{std::unexpected{"write failed"}}));
    EXPECT_FALSE(state.take_pending_level());
    EXPECT_FALSE(state.request_menu({}));
    auto request{state.take_menu_request()};
    ASSERT_TRUE(request);
    auto const* telemetry{std::get_if<TelemetryRequest>(&*request)};
    ASSERT_NE(telemetry, nullptr);
    ASSERT_FALSE(telemetry->run);
    EXPECT_EQ(telemetry->run.error(), "write failed");
    EXPECT_FALSE(state.take_menu_request());
    ASSERT_TRUE(state.request_menu(TelemetryRequest{}));
    request = state.take_menu_request();
    ASSERT_TRUE(request);
    telemetry = std::get_if<TelemetryRequest>(&*request);
    ASSERT_NE(telemetry, nullptr);
    ASSERT_TRUE(telemetry->run);
    EXPECT_TRUE(telemetry->run->empty());
    EXPECT_TRUE(state.request_menu(LevelSelectRequest{.preferred_level_id = LevelId{"retry"}}));

    state.set_pending_level(*definition, "level.scm");
    state.set_launch_error("invalid configuration");
    EXPECT_FALSE(state.take_pending_level());
    EXPECT_EQ(state.take_launch_error(), "invalid configuration");
    EXPECT_FALSE(state.has_launch_error());
}

TEST(LaunchOptions, ValidatesTimeScaleAndOptionalDuration) {
    for (auto const value : {0.25, 1.0, 100.0}) {
        EXPECT_TRUE(launch::TimeScale::try_make(value));
    }
    for (auto const value : {0.0,
                             -1.0,
                             100.0001,
                             std::numeric_limits<double>::infinity(),
                             std::numeric_limits<double>::quiet_NaN()}) {
        EXPECT_FALSE(launch::TimeScale::try_make(value));
    }
    for (auto const mode : {PresentationMode::Visual, PresentationMode::SimulationOnly}) {
        LaunchOptions options{.presentation_mode = mode};
        EXPECT_FALSE(options.simulated_duration_seconds);
        options.simulated_duration_seconds = PositiveFinite<double>{2.0};
        EXPECT_EQ(options.simulated_duration_seconds->get(), 2.0);
    }
    for (auto const value : {0.0,
                             -1.0,
                             std::numeric_limits<double>::infinity(),
                             std::numeric_limits<double>::quiet_NaN()}) {
        EXPECT_FALSE(PositiveFinite<double>::try_make(value));
    }
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
    state.suspend(ControlContext::None);
    EXPECT_TRUE(state.pending());
    EXPECT_EQ(state.context(ControlContext::Benchmark), ControlContext::None);
}
}
