#include <SandboxTests/support/test_setup.h>
#include <SpaceGame/presentation/TestBatchGameUiData.h>
#include <SpaceGame/ui/main_menu/OptionsWidget.h>
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
        auto const world_result{ioj::get_editor_world()};
        if (!TestRunner->TestTrue(TEXT("Editor world is available"), world_result.has_value())) {
            return;
        }

        auto* const ui_data{ioj::test_batch_game_ui_data::get_data_asset()};
        if (!TestRunner->TestTrue(TEXT("Project UI data loads"), IsValid(ui_data))) {
            return;
        }

        auto const widget_class{ui_data->get_widget_class<ioj::UPauseMenuWidget>()};
        if (!TestRunner->TestTrue(TEXT("Pause menu class is configured"),
                                  static_cast<bool>(widget_class))) {
            return;
        }

        auto* const widget{CreateWidget<ioj::UPauseMenuWidget>(
            world_result.value(), widget_class, TEXT("pause_menu_test"))};
        if (!TestRunner->TestTrue(TEXT("Pause menu is created"), IsValid(widget))) {
            return;
        }

        auto* const pause_action{LoadObject<UInputAction>(
            nullptr, TEXT("/SpaceGame/Input/SpaceShip/IA_pause.IA_pause"))};
        if (!TestRunner->TestTrue(TEXT("Pause action loads"), IsValid(pause_action))) {
            return;
        }

        ioj::FPauseMenuData data;
        auto& snapshot{data.telemetry};
        snapshot.elapsed_seconds = ioj::Duration::make(3723.0);
        snapshot.tick_rate_hz = ioj::Frequency::make(2.0);
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
                                    [std::to_underlying(ioj::sim::EntityType::Fighter)] = 12;
        data.top_killers.Add({ioj::sim::EntityId(7, ioj::sim::EntityType::CapitalShip),
                              ioj::sim::EntityType::CapitalShip,
                              ETestTeam::Blue,
                              5});
        data.team_kill_matrix.set(ETestTeam::Green, ioj::sim::EntityType::Turret, 3);
        widget->prepare_for_open(*pause_action, MoveTemp(data));

        FCommonInputBase::GetInputSettings()->LoadData();
        auto const slate_widget{widget->TakeWidget()};
        (void)slate_widget;
        widget->ActivateWidget();

        auto const get_button{[widget](FName const name) {
            return Cast<ioj::UMenuButtonWidget>(widget->GetWidgetFromName(name));
        }};
        auto const get_text{[widget](FName const name) {
            return Cast<UTextBlock>(widget->GetWidgetFromName(name));
        }};
        auto* const resume_button{get_button(TEXT("resume_button"))};
        auto* const overview_button{get_button(TEXT("overview_button"))};
        auto* const forces_button{get_button(TEXT("forces_button"))};
        auto* const combat_button{get_button(TEXT("combat_button"))};
        auto* const telemetry_button{get_button(TEXT("telemetry_button"))};
        auto* const audio_button{get_button(TEXT("audio_button"))};
        auto* const controls_button{get_button(TEXT("controls_button"))};
        auto* const return_button{get_button(TEXT("return_to_level_select_button"))};
        auto* const quit_button{get_button(TEXT("quit_button"))};
        auto* const page_heading{get_text(TEXT("page_heading"))};
        auto* const page_switcher{
            Cast<UWidgetSwitcher>(widget->GetWidgetFromName(TEXT("page_switcher")))};
        auto* const options_widget{widget->get_options_widget()};
        auto* const elapsed_time_value{get_text(TEXT("elapsed_time_value"))};
        auto* const entities_spawned_value{get_text(TEXT("entities_spawned_value"))};
        auto* const entities_active_value{get_text(TEXT("entities_active_value"))};
        auto* const entities_destroyed_value{get_text(TEXT("entities_destroyed_value"))};
        auto* const kills_value{get_text(TEXT("kills_value"))};
        auto* const lasers_fired_value{get_text(TEXT("lasers_fired_value"))};
        auto* const lasers_active_value{get_text(TEXT("lasers_active_value"))};
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
            IsValid(combat_button) && IsValid(telemetry_button) && IsValid(audio_button) &&
            IsValid(controls_button) && IsValid(return_button) && IsValid(quit_button) &&
            IsValid(page_heading) && IsValid(page_switcher) && IsValid(options_widget) &&
            IsValid(elapsed_time_value) && IsValid(entities_spawned_value) &&
            IsValid(entities_active_value) && IsValid(entities_destroyed_value) &&
            IsValid(kills_value) && IsValid(lasers_fired_value) && IsValid(lasers_active_value) &&
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

        auto const check_text{[this](UTextBlock const* const block,
                                     TCHAR const* const expected,
                                     TCHAR const* const description) {
            TestRunner->TestEqual(description, block->GetText().ToString(), expected);
        }};
        TestRunner->TestTrue(TEXT("Overview is active initially"),
                             widget->get_active_tab() == ioj::EPauseMenuTab::Overview);
        TestRunner->TestTrue(TEXT("Resume is the deterministic initial focus target"),
                             widget->GetDesiredFocusTarget() == resume_button);
        check_text(page_heading, TEXT("Overview"), TEXT("Overview heading is displayed"));
        check_text(
            elapsed_time_value, TEXT("1:02:03"), TEXT("Elapsed time is formatted for display"));
        check_text(entities_spawned_value, TEXT("25"), TEXT("Spawned entity count is displayed"));
        check_text(entities_active_value, TEXT("17"), TEXT("Active entity count is displayed"));
        check_text(
            entities_destroyed_value, TEXT("8"), TEXT("Destroyed entity count is displayed"));
        check_text(kills_value, TEXT("6"), TEXT("Kill count is displayed"));

        forces_button->OnClicked().Broadcast();
        TestRunner->TestTrue(TEXT("Forces button activates Forces"),
                             widget->get_active_tab() == ioj::EPauseMenuTab::Forces);
        check_text(page_heading, TEXT("Forces"), TEXT("Forces heading is displayed"));
        auto* const fighter_count{
            Cast<UTextBlock>(forces_table->GetWidgetFromName(TEXT("entity_value_3_1")))};
        TestRunner->TestTrue(TEXT("Detailed fighter count is available"), IsValid(fighter_count));
        if (IsValid(fighter_count)) {
            check_text(fighter_count, TEXT("12"), TEXT("Detailed fighter count is current"));
        }

        combat_button->OnClicked().Broadcast();
        TestRunner->TestTrue(TEXT("Combat button activates Combat"),
                             widget->get_active_tab() == ioj::EPauseMenuTab::Combat);
        auto* const top_kills{
            Cast<UTextBlock>(top_killers_table->GetWidgetFromName(TEXT("kills_0")))};
        auto* const turret_kills{
            Cast<UTextBlock>(team_kills_table->GetWidgetFromName(TEXT("entity_value_1_2")))};
        TestRunner->TestTrue(TEXT("Top-killer data is available"), IsValid(top_kills));
        TestRunner->TestTrue(TEXT("Team-kill data is available"), IsValid(turret_kills));
        if (IsValid(top_kills)) {
            check_text(top_kills, TEXT("5"), TEXT("Top-killer data is current"));
        }
        if (IsValid(turret_kills)) {
            check_text(turret_kills, TEXT("3"), TEXT("Team-kill data is current"));
        }

        telemetry_button->OnClicked().Broadcast();
        TestRunner->TestTrue(TEXT("Telemetry button activates Telemetry"),
                             widget->get_active_tab() == ioj::EPauseMenuTab::Telemetry);
        TestRunner->TestEqual(TEXT("Telemetry page is displayed"),
                              page_switcher->GetActiveWidgetIndex(),
                              static_cast<int32>(ioj::EPauseMenuTab::Telemetry));
        check_text(lasers_fired_value, TEXT("120"), TEXT("Fired laser count is displayed"));
        check_text(lasers_active_value, TEXT("4"), TEXT("Active laser count is displayed"));

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
            ioj::FPauseMenuData time_data;
            time_data.telemetry.elapsed_seconds = ioj::Duration::make(seconds);
            widget->prepare_for_open(*pause_action, MoveTemp(time_data));
            check_text(
                elapsed_time_value, expected, TEXT("Elapsed-time boundary is formatted correctly"));
        }};
        check_elapsed_time(0.0, TEXT("00:00"));
        check_elapsed_time(59.0, TEXT("00:59"));
        check_elapsed_time(60.0, TEXT("01:00"));
        check_elapsed_time(3599.0, TEXT("59:59"));
        check_elapsed_time(3600.0, TEXT("1:00:00"));
        TestRunner->TestTrue(TEXT("Opening with no telemetry clears previous graph data"),
                             telemetry_graph->get_series().IsEmpty());

        audio_button->OnClicked().Broadcast();
        TestRunner->TestTrue(TEXT("Audio button activates Options"),
                             widget->get_active_tab() == ioj::EPauseMenuTab::Options);
        TestRunner->TestTrue(TEXT("Audio opens the shared options widget on its Audio tab"),
                             options_widget->get_active_tab() == ioj::EOptionsTab::Audio);
        controls_button->OnClicked().Broadcast();
        TestRunner->TestTrue(TEXT("Controls uses the shared options page"),
                             widget->get_active_tab() == ioj::EPauseMenuTab::Options);
        TestRunner->TestTrue(TEXT("Controls selects the Controls options tab"),
                             options_widget->get_active_tab() == ioj::EOptionsTab::Controls);
        overview_button->OnClicked().Broadcast();
        TestRunner->TestTrue(TEXT("Overview button returns to Overview"),
                             widget->get_active_tab() == ioj::EPauseMenuTab::Overview);

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
