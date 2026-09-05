#include <SandboxTests/support/test_setup.h>

#include <SpaceGame/presentation/TestBatchGameUiData.h>
#include <SpaceGame/ui/common/MenuButtonWidget.h>
#include <SpaceGame/ui/PauseMenuWidget.h>

#include <SandboxUI/widgets/SGraphPlot.h>

#include <CommonInputSettings.h>
#include <Components/Button.h>
#include <Components/NativeWidgetHost.h>
#include <Components/TextBlock.h>
#include <Components/WidgetSwitcher.h>
#include <CQTest.h>
#include <InputAction.h>

TEST_CLASS(PauseMenuWidget, "Sandbox.UnitTests")
{
    TEST_METHOD(NavigationAndResume)
    {
        auto const world_result{ml::get_editor_world()};
        if (!TestRunner->TestTrue(TEXT("Editor world is available"), world_result.has_value())) {
            return;
        }

        auto* const ui_data{ml::test_batch_game_ui_data::get_data_asset()};
        if (!TestRunner->TestTrue(TEXT("Project UI data loads"), IsValid(ui_data))) {
            return;
        }

        auto const widget_class{ui_data->get_widget_class<ml::ioj::UPauseMenuWidget>()};
        if (!TestRunner->TestTrue(TEXT("Pause menu class is configured"),
                                  static_cast<bool>(widget_class))) {
            return;
        }

        auto* const widget{CreateWidget<ml::ioj::UPauseMenuWidget>(
            world_result.value(), widget_class, TEXT("pause_menu_test"))};
        if (!TestRunner->TestTrue(TEXT("Pause menu is created"), IsValid(widget))) {
            return;
        }

        auto* const pause_action{LoadObject<UInputAction>(
            nullptr, TEXT("/SpaceGame/Input/SpaceShip/IA_pause.IA_pause"))};
        if (!TestRunner->TestTrue(TEXT("Pause action loads"), IsValid(pause_action))) {
            return;
        }

        FLevelTelemetrySnapshot snapshot;
        snapshot.elapsed_seconds = 3723.0;
        snapshot.tick_period = 0.5;
        snapshot.spawned_entities = 25;
        snapshot.active_entities = 17;
        snapshot.destroyed_entities = 8;
        snapshot.kills = 6;
        snapshot.lasers_fired = 120;
        snapshot.active_lasers = 4;
        snapshot.active_entity_count_data.add(0, 20);
        snapshot.active_entity_count_data.add(4, 17);
        snapshot.cumulative_kill_count_data.add(0, 0);
        snapshot.cumulative_kill_count_data.add(4, 6);
        widget->prepare_for_open(*pause_action, MoveTemp(snapshot));

        FCommonInputBase::GetInputSettings()->LoadData();
        auto const slate_widget{widget->TakeWidget()};
        (void)slate_widget;
        widget->ActivateWidget();

        auto* const resume_button{
            Cast<ml::ioj::UMenuButtonWidget>(widget->GetWidgetFromName(TEXT("resume_button")))};
        auto* const overview_button{
            Cast<ml::ioj::UMenuButtonWidget>(widget->GetWidgetFromName(TEXT("overview_button")))};
        auto* const stats_button{
            Cast<ml::ioj::UMenuButtonWidget>(widget->GetWidgetFromName(TEXT("stats_button")))};
        auto* const options_button{
            Cast<ml::ioj::UMenuButtonWidget>(widget->GetWidgetFromName(TEXT("options_button")))};
        auto* const return_button{Cast<ml::ioj::UMenuButtonWidget>(
            widget->GetWidgetFromName(TEXT("return_to_level_select_button")))};
        auto* const quit_button{
            Cast<ml::ioj::UMenuButtonWidget>(widget->GetWidgetFromName(TEXT("quit_button")))};
        auto* const page_heading{Cast<UTextBlock>(widget->GetWidgetFromName(TEXT("page_heading")))};
        auto* const page_switcher{
            Cast<UWidgetSwitcher>(widget->GetWidgetFromName(TEXT("page_switcher")))};
        auto* const elapsed_time_value{
            Cast<UTextBlock>(widget->GetWidgetFromName(TEXT("elapsed_time_value")))};
        auto* const entities_spawned_value{
            Cast<UTextBlock>(widget->GetWidgetFromName(TEXT("entities_spawned_value")))};
        auto* const entities_active_value{
            Cast<UTextBlock>(widget->GetWidgetFromName(TEXT("entities_active_value")))};
        auto* const entities_destroyed_value{
            Cast<UTextBlock>(widget->GetWidgetFromName(TEXT("entities_destroyed_value")))};
        auto* const kills_value{Cast<UTextBlock>(widget->GetWidgetFromName(TEXT("kills_value")))};
        auto* const lasers_fired_value{
            Cast<UTextBlock>(widget->GetWidgetFromName(TEXT("lasers_fired_value")))};
        auto* const lasers_active_value{
            Cast<UTextBlock>(widget->GetWidgetFromName(TEXT("lasers_active_value")))};
        auto* const stats_graph_host{
            Cast<UNativeWidgetHost>(widget->GetWidgetFromName(TEXT("stats_graph_host")))};

        auto const bindings_valid{
            IsValid(resume_button) && IsValid(overview_button) && IsValid(stats_button) &&
            IsValid(options_button) && IsValid(return_button) && IsValid(quit_button) &&
            IsValid(page_heading) && IsValid(page_switcher) && IsValid(elapsed_time_value) &&
            IsValid(entities_spawned_value) && IsValid(entities_active_value) &&
            IsValid(entities_destroyed_value) && IsValid(kills_value) &&
            IsValid(lasers_fired_value) && IsValid(lasers_active_value) &&
            IsValid(stats_graph_host)};
        if (!TestRunner->TestTrue(TEXT("All required pause menu bindings are valid"),
                                  bindings_valid)) {
            return;
        }

        TestRunner->TestTrue(TEXT("Overview is active initially"),
                             widget->get_active_tab() == ml::ioj::EPauseMenuTab::Overview);
        TestRunner->TestTrue(TEXT("Resume is the deterministic initial focus target"),
                             widget->GetDesiredFocusTarget() == resume_button);
        TestRunner->TestEqual(TEXT("Overview heading is displayed"),
                              page_heading->GetText().ToString(),
                              TEXT("Overview"));

        stats_button->OnClicked().Broadcast();
        TestRunner->TestTrue(TEXT("Stats button activates Stats"),
                             widget->get_active_tab() == ml::ioj::EPauseMenuTab::Stats);
        TestRunner->TestEqual(
            TEXT("Stats heading is displayed"), page_heading->GetText().ToString(), TEXT("Stats"));
        TestRunner->TestEqual(TEXT("Stats page is displayed"),
                              page_switcher->GetActiveWidgetIndex(),
                              static_cast<int32>(ml::ioj::EPauseMenuTab::Stats));
        TestRunner->TestEqual(TEXT("Elapsed time is formatted for display"),
                              elapsed_time_value->GetText().ToString(),
                              TEXT("1:02:03"));
        TestRunner->TestEqual(TEXT("Spawned entity count is displayed"),
                              entities_spawned_value->GetText().ToString(),
                              TEXT("25"));
        TestRunner->TestEqual(TEXT("Active entity count is displayed"),
                              entities_active_value->GetText().ToString(),
                              TEXT("17"));
        TestRunner->TestEqual(TEXT("Destroyed entity count is displayed"),
                              entities_destroyed_value->GetText().ToString(),
                              TEXT("8"));
        TestRunner->TestEqual(
            TEXT("Kill count is displayed"), kills_value->GetText().ToString(), TEXT("6"));
        TestRunner->TestEqual(TEXT("Fired laser count is displayed"),
                              lasers_fired_value->GetText().ToString(),
                              TEXT("120"));
        TestRunner->TestEqual(TEXT("Active laser count is displayed"),
                              lasers_active_value->GetText().ToString(),
                              TEXT("4"));

        auto* const stats_graph{static_cast<SGraphPlot*>(stats_graph_host->GetContent().Get())};
        if (!TestRunner->TestNotNull(TEXT("Stats graph has Slate content"), stats_graph)) {
            return;
        }
        auto const graph_series{stats_graph->get_series()};
        if (TestRunner->TestEqual(
                TEXT("Stats graph has two series"), graph_series.Num(), int32{2})) {
            TestRunner->TestEqual(TEXT("Active series has its player-facing name"),
                                  graph_series[0].name.ToString(),
                                  TEXT("Active entities"));
            TestRunner->TestEqual(
                TEXT("Active series has two X samples"), graph_series[0].x.Num(), int32{2});
            TestRunner->TestEqual(
                TEXT("Active series has two Y samples"), graph_series[0].y.Num(), int32{2});
            TestRunner->TestEqual(
                TEXT("Active series converts ticks to seconds"), graph_series[0].x[1], 2.0f);
            TestRunner->TestEqual(
                TEXT("Active series preserves its final count"), graph_series[0].y[1], 17.0f);
            TestRunner->TestTrue(TEXT("Active series uses step interpolation"),
                                 graph_series[0].style.interpolation ==
                                     EGraphSeriesInterpolation::StepAfter);

            TestRunner->TestEqual(TEXT("Kill series has its player-facing name"),
                                  graph_series[1].name.ToString(),
                                  TEXT("Kills"));
            TestRunner->TestEqual(
                TEXT("Kill series converts ticks to seconds"), graph_series[1].x[1], 2.0f);
            TestRunner->TestEqual(
                TEXT("Kill series preserves its final count"), graph_series[1].y[1], 6.0f);
            TestRunner->TestTrue(TEXT("Kill series uses step interpolation"),
                                 graph_series[1].style.interpolation ==
                                     EGraphSeriesInterpolation::StepAfter);
        }
        TestRunner->TestTrue(TEXT("Stats has a distinct selected appearance"),
                             stats_button->GetSelected() && !overview_button->GetSelected());

        auto const check_elapsed_time{[&](double const seconds, TCHAR const* const expected) {
            FLevelTelemetrySnapshot time_snapshot;
            time_snapshot.elapsed_seconds = seconds;
            widget->prepare_for_open(*pause_action, MoveTemp(time_snapshot));
            TestRunner->TestEqual(TEXT("Elapsed-time boundary is formatted correctly"),
                                  elapsed_time_value->GetText().ToString(),
                                  expected);
        }};
        check_elapsed_time(0.0, TEXT("00:00"));
        check_elapsed_time(59.0, TEXT("00:59"));
        check_elapsed_time(60.0, TEXT("01:00"));
        check_elapsed_time(3599.0, TEXT("59:59"));
        check_elapsed_time(3600.0, TEXT("1:00:00"));
        TestRunner->TestTrue(TEXT("Opening with no telemetry clears previous graph data"),
                             stats_graph->get_series().IsEmpty());

        options_button->OnClicked().Broadcast();
        TestRunner->TestTrue(TEXT("Options button activates Options"),
                             widget->get_active_tab() == ml::ioj::EPauseMenuTab::Options);
        TestRunner->TestEqual(TEXT("Options heading is displayed"),
                              page_heading->GetText().ToString(),
                              TEXT("Options"));

        overview_button->OnClicked().Broadcast();
        TestRunner->TestTrue(TEXT("Overview button returns to Overview"),
                             widget->get_active_tab() == ml::ioj::EPauseMenuTab::Overview);

        resume_button->OnClicked().Broadcast();
        TestRunner->TestFalse(TEXT("Resume deactivates the pause menu"), widget->IsActivated());

        int32 return_requests{0};
        widget->return_to_level_select_requested.AddLambda(
            [&return_requests] { ++return_requests; });
        return_button->OnClicked().Broadcast();
        return_button->OnClicked().Broadcast();
        TestRunner->TestEqual(
            TEXT("Return to level select is emitted only once"), return_requests, 1);
    }
};
