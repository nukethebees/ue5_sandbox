#include <SandboxTests/support/test_setup.h>

#include <SpaceGame/presentation/TestBatchGameUiData.h>
#include <SpaceGame/ui/LevelCompletionWidget.h>

#include <CommonInputSettings.h>
#include <CQTest.h>

TEST_CLASS(LevelCompletionWidget, "Sandbox.UnitTests")
{
    TEST_METHOD(ContentFocusAndActions)
    {
        auto const world_result{ml::get_editor_world()};
        if (!TestRunner->TestTrue(TEXT("Editor world is available"), world_result.has_value())) {
            return;
        }

        auto* const ui_data{ml::test_batch_game_ui_data::get_data_asset()};
        auto const widget_class{IsValid(ui_data)
                                    ? ui_data->get_widget_class<ml::ioj::ULevelCompletionWidget>()
                                    : nullptr};
        if (!TestRunner->TestTrue(TEXT("Completion widget class is configured"),
                                  static_cast<bool>(widget_class))) {
            return;
        }

        auto const make_widget{[&] {
            auto* const widget{
                CreateWidget<ml::ioj::ULevelCompletionWidget>(world_result.value(), widget_class)};
            if (IsValid(widget)) {
                auto const slate_widget{widget->TakeWidget()};
                (void)slate_widget;
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
                widget->prepare_for_open(TEXT("Border Skirmish"),
                                         ETestMissionState::Succeeded,
                                         MoveTemp(snapshot),
                                         TOptional<float>{3800.0f},
                                         true);
                widget->ActivateWidget();
            }
            return widget;
        }};

        FCommonInputBase::GetInputSettings()->LoadData();
        auto* const widget{make_widget()};
        if (!TestRunner->TestTrue(TEXT("Completion report is created"), IsValid(widget))) {
            return;
        }

        TestRunner->TestEqual(TEXT("Authored level title is retained by the report"),
                              widget->get_level_display_name(),
                              FString{TEXT("Border Skirmish")});
        auto const& snapshot{widget->get_stats_snapshot()};
        TestRunner->TestEqual(
            TEXT("Completion report retains elapsed time"), snapshot.elapsed_seconds, 3723.0);
        TestRunner->TestEqual(TEXT("Completion report retains kills"), snapshot.kills, 6);
        TestRunner->TestEqual(
            TEXT("Completion report retains destroyed entities"), snapshot.destroyed_entities, 8);
        TestRunner->TestEqual(
            TEXT("Completion report retains laser count"), snapshot.lasers_fired, 120);
        TestRunner->TestEqual(TEXT("Completion report retains the par time"),
                              widget->get_par_time_seconds().GetValue(),
                              3800.0f);
        TestRunner->TestTrue(TEXT("Completion report retains the new-best result"),
                             widget->is_new_best_time());
        TestRunner->TestTrue(TEXT("The report is the deterministic initial focus target"),
                             widget->GetDesiredFocusTarget() == widget);

        int32 return_requests{0};
        widget->return_to_level_select_requested.AddLambda(
            [&return_requests] { ++return_requests; });
        widget->request_return_to_mission_control();
        widget->request_return_to_mission_control();
        TestRunner->TestEqual(TEXT("Return is emitted only once"), return_requests, 1);

        auto* const keep_operating_widget{make_widget()};
        if (TestRunner->TestTrue(TEXT("Keep Operating report is created"),
                                 IsValid(keep_operating_widget))) {
            keep_operating_widget->request_keep_operating();
            TestRunner->TestFalse(TEXT("Keep Operating deactivates completion"),
                                  keep_operating_widget->IsActivated());
        }
    }
};
