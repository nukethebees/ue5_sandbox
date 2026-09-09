#include <SandboxTests/support/test_setup.h>

#include <SpaceGame/presentation/TestBatchGameUiData.h>
#include <SpaceGame/ui/PauseMenuWidget.h>
#include <SpaceGamePresentation/presentation/widgets/TeamEntityTableWidget.h>
#include <SpaceGamePresentation/presentation/widgets/TopKillersWidget.h>
#include <SpaceGamePresentation/ui/common/MenuButtonWidget.h>

#include <SandboxUI/widgets/SGraphPlot.h>

#include <CommonInputSettings.h>
#include <Components/NativeWidgetHost.h>
#include <Components/ScrollBox.h>
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

        ml::ioj::FPauseMenuData data;
        auto& snapshot{data.telemetry};
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
        data.alive_per_team_and_type[std::to_underlying(ETestTeam::Red)]
                                    [std::to_underlying(ETestEntityType::CapitalShipFighter)] = 12;
        data.top_killers.add({.id = 7}, ETestEntityType::CapitalShip, ETestTeam::Blue, 5);
        data.team_kill_matrix.set(ETestTeam::Green, ETestEntityType::Turret, 3);
        widget->prepare_for_open(*pause_action, MoveTemp(data));

        FCommonInputBase::GetInputSettings()->LoadData();
        auto const slate_widget{widget->TakeWidget()};
        (void)slate_widget;
        widget->ActivateWidget();

        auto* const resume_button{
            Cast<ml::ioj::UMenuButtonWidget>(widget->GetWidgetFromName(TEXT("resume_button")))};
        auto* const overview_button{
            Cast<ml::ioj::UMenuButtonWidget>(widget->GetWidgetFromName(TEXT("overview_button")))};
        auto* const forces_button{
            Cast<ml::ioj::UMenuButtonWidget>(widget->GetWidgetFromName(TEXT("forces_button")))};
        auto* const combat_button{
            Cast<ml::ioj::UMenuButtonWidget>(widget->GetWidgetFromName(TEXT("combat_button")))};
        auto* const telemetry_button{
            Cast<ml::ioj::UMenuButtonWidget>(widget->GetWidgetFromName(TEXT("telemetry_button")))};
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
        auto* const telemetry_graph_host{
            Cast<UNativeWidgetHost>(widget->GetWidgetFromName(TEXT("telemetry_graph_host")))};
        auto* const forces_table{
            Cast<UTeamEntityTableWidget>(widget->GetWidgetFromName(TEXT("forces_table")))};
        auto* const top_killers_table{
            Cast<UTopKillersWidget>(widget->GetWidgetFromName(TEXT("combat_top_killers_table")))};
        auto* const team_kills_table{Cast<UTeamEntityTableWidget>(
            widget->GetWidgetFromName(TEXT("combat_team_kills_table")))};

        auto const bindings_valid{
            IsValid(resume_button) && IsValid(overview_button) && IsValid(forces_button) &&
            IsValid(combat_button) && IsValid(telemetry_button) && IsValid(options_button) &&
            IsValid(return_button) && IsValid(quit_button) && IsValid(page_heading) &&
            IsValid(page_switcher) && IsValid(elapsed_time_value) &&
            IsValid(entities_spawned_value) && IsValid(entities_active_value) &&
            IsValid(entities_destroyed_value) && IsValid(kills_value) &&
            IsValid(lasers_fired_value) && IsValid(lasers_active_value) &&
            IsValid(telemetry_graph_host) && IsValid(forces_table) && IsValid(top_killers_table) &&
            IsValid(team_kills_table)};
        if (!TestRunner->TestTrue(TEXT("All required pause menu bindings are valid"),
                                  bindings_valid)) {
            return;
        }

        for (auto const page_name : {TEXT("overview_scroll"),
                                     TEXT("forces_scroll"),
                                     TEXT("combat_scroll"),
                                     TEXT("telemetry_scroll")}) {
            TestRunner->TestTrue(TEXT("Battle-data page supports scrolling"),
                                 IsValid(Cast<UScrollBox>(widget->GetWidgetFromName(page_name))));
        }

        TestRunner->TestTrue(TEXT("Overview is active initially"),
                             widget->get_active_tab() == ml::ioj::EPauseMenuTab::Overview);
        TestRunner->TestTrue(TEXT("Resume is the deterministic initial focus target"),
                             widget->GetDesiredFocusTarget() == resume_button);
        TestRunner->TestEqual(TEXT("Overview heading is displayed"),
                              page_heading->GetText().ToString(),
                              TEXT("Overview"));
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

        forces_button->OnClicked().Broadcast();
        TestRunner->TestTrue(TEXT("Forces button activates Forces"),
                             widget->get_active_tab() == ml::ioj::EPauseMenuTab::Forces);
        TestRunner->TestEqual(TEXT("Forces heading is displayed"),
                              page_heading->GetText().ToString(),
                              TEXT("Forces"));
        auto* const fighter_count{
            Cast<UTextBlock>(forces_table->GetWidgetFromName(TEXT("entity_value_3_1")))};
        TestRunner->TestTrue(TEXT("Detailed fighter count is available"), IsValid(fighter_count));
        if (IsValid(fighter_count)) {
            TestRunner->TestEqual(TEXT("Detailed fighter count is current"),
                                  fighter_count->GetText().ToString(),
                                  TEXT("12"));
        }

        combat_button->OnClicked().Broadcast();
        TestRunner->TestTrue(TEXT("Combat button activates Combat"),
                             widget->get_active_tab() == ml::ioj::EPauseMenuTab::Combat);
        auto* const top_kills{
            Cast<UTextBlock>(top_killers_table->GetWidgetFromName(TEXT("kills_0")))};
        auto* const turret_kills{
            Cast<UTextBlock>(team_kills_table->GetWidgetFromName(TEXT("entity_value_1_2")))};
        TestRunner->TestTrue(TEXT("Top-killer data is available"), IsValid(top_kills));
        TestRunner->TestTrue(TEXT("Team-kill data is available"), IsValid(turret_kills));
        if (IsValid(top_kills)) {
            TestRunner->TestEqual(
                TEXT("Top-killer data is current"), top_kills->GetText().ToString(), TEXT("5"));
        }
        if (IsValid(turret_kills)) {
            TestRunner->TestEqual(
                TEXT("Team-kill data is current"), turret_kills->GetText().ToString(), TEXT("3"));
        }

        telemetry_button->OnClicked().Broadcast();
        TestRunner->TestTrue(TEXT("Telemetry button activates Telemetry"),
                             widget->get_active_tab() == ml::ioj::EPauseMenuTab::Telemetry);
        TestRunner->TestEqual(TEXT("Telemetry page is displayed"),
                              page_switcher->GetActiveWidgetIndex(),
                              static_cast<int32>(ml::ioj::EPauseMenuTab::Telemetry));
        TestRunner->TestEqual(TEXT("Fired laser count is displayed"),
                              lasers_fired_value->GetText().ToString(),
                              TEXT("120"));
        TestRunner->TestEqual(TEXT("Active laser count is displayed"),
                              lasers_active_value->GetText().ToString(),
                              TEXT("4"));

        auto* const telemetry_graph{
            static_cast<SGraphPlot*>(telemetry_graph_host->GetContent().Get())};
        if (!TestRunner->TestNotNull(TEXT("Telemetry graph has Slate content"), telemetry_graph)) {
            return;
        }
        auto const graph_series{telemetry_graph->get_series()};
        if (TestRunner->TestEqual(
                TEXT("Telemetry graph has two series"), graph_series.Num(), int32{2})) {
            TestRunner->TestEqual(TEXT("Active series has its player-facing name"),
                                  graph_series[0].name.ToString(),
                                  TEXT("Active entities"));
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
                TEXT("Kill series preserves its final count"), graph_series[1].y[1], 6.0f);
        }
        TestRunner->TestTrue(TEXT("Telemetry has a distinct selected appearance"),
                             telemetry_button->GetSelected() && !overview_button->GetSelected());

        auto const check_elapsed_time{[&](double const seconds, TCHAR const* const expected) {
            ml::ioj::FPauseMenuData time_data;
            time_data.telemetry.elapsed_seconds = seconds;
            widget->prepare_for_open(*pause_action, MoveTemp(time_data));
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
                             telemetry_graph->get_series().IsEmpty());

        options_button->OnClicked().Broadcast();
        TestRunner->TestTrue(TEXT("Options button activates Options"),
                             widget->get_active_tab() == ml::ioj::EPauseMenuTab::Options);
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
