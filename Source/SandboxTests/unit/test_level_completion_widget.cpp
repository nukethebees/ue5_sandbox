#include <SandboxTests/support/test_setup.h>
#include <SpaceGame/presentation/TestBatchGameUiData.h>
#include <SpaceGame/ui/LevelCompletionWidget.h>

#include <CommonInputSettings.h>
#include <CQTest.h>

TEST_CLASS(LevelCompletionWidget, "Sandbox.UnitTests")
{
    TEST_METHOD(ContentFocusAndActions)
    {
        auto const world_result{ioj::get_editor_world()};
        if (!TestRunner->TestTrue(TEXT("Editor world is available"), world_result.has_value())) {
            return;
        }

        auto* const ui_data{ioj::test_batch_game_ui_data::get_data_asset()};
        auto const widget_class{
            IsValid(ui_data) ? ui_data->get_widget_class<ioj::ULevelCompletionWidget>() : nullptr};
        if (!TestRunner->TestTrue(TEXT("Completion widget class is configured"),
                                  static_cast<bool>(widget_class))) {
            return;
        }

        auto const make_widget{[&] {
            auto* const widget{
                CreateWidget<ioj::ULevelCompletionWidget>(world_result.value(), widget_class)};
            if (IsValid(widget)) {
                auto const slate_widget{widget->TakeWidget()};
                (void)slate_widget;
                widget->prepare_for_open({.level_display_name = TEXT("Border Skirmish"),
                                          .state = ioj::ELevelOutcome::Victory,
                                          .end_reason = TEXT("level_completed"),
                                          .winner = TEXT("blue"),
                                          .elapsed_time = ioj::Duration::make(3723.0),
                                          .team_kills = 6,
                                          .par_time_seconds = 3800.0f,
                                          .new_best_time = true});
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
        TestRunner->TestEqual(TEXT("Completion retains elapsed time"),
                              widget->completion().elapsed_time.seconds(),
                              3723.0);
        TestRunner->TestEqual(TEXT("Completion retains profile team kills"),
                              widget->completion().team_kills.GetValue(),
                              6);
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
    }
};
