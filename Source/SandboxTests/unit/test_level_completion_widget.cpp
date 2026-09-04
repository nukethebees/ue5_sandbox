#include <SandboxTests/support/test_setup.h>

#include <SpaceGame/presentation/TestBatchGameUiData.h>
#include <SpaceGame/ui/common/MenuButtonWidget.h>
#include <SpaceGame/ui/LevelCompletionWidget.h>

#include <CommonInputSettings.h>
#include <Components/TextBlock.h>
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
                widget->prepare_for_open(TEXT("Border Skirmish"));
                widget->ActivateWidget();
            }
            return widget;
        }};

        FCommonInputBase::GetInputSettings()->LoadData();
        auto* const widget{make_widget()};
        auto* const heading{IsValid(widget) ? Cast<UTextBlock>(widget->GetWidgetFromName(
                                                  TEXT("mission_complete_text")))
                                            : nullptr};
        auto* const level_name{
            IsValid(widget) ? Cast<UTextBlock>(widget->GetWidgetFromName(TEXT("level_name_text")))
                            : nullptr};
        auto* const return_button{IsValid(widget)
                                      ? Cast<ml::ioj::UMenuButtonWidget>(widget->GetWidgetFromName(
                                            TEXT("return_to_level_select_button")))
                                      : nullptr};
        auto* const keep_playing_button{
            IsValid(widget) ? Cast<ml::ioj::UMenuButtonWidget>(
                                  widget->GetWidgetFromName(TEXT("keep_playing_button")))
                            : nullptr};
        if (!TestRunner->TestTrue(TEXT("Completion widget bindings are valid"),
                                  IsValid(heading) && IsValid(level_name) &&
                                      IsValid(return_button) && IsValid(keep_playing_button))) {
            return;
        }

        TestRunner->TestEqual(TEXT("Completion heading is displayed"),
                              heading->GetText().ToString(),
                              FString{TEXT("Mission Complete")});
        TestRunner->TestEqual(TEXT("Authored level title is displayed"),
                              level_name->GetText().ToString(),
                              FString{TEXT("Border Skirmish")});
        TestRunner->TestTrue(TEXT("Return is the deterministic initial focus target"),
                             widget->GetDesiredFocusTarget() == return_button);

        int32 return_requests{0};
        widget->return_to_level_select_requested.AddLambda(
            [&return_requests] { ++return_requests; });
        return_button->OnClicked().Broadcast();
        return_button->OnClicked().Broadcast();
        TestRunner->TestEqual(TEXT("Return is emitted only once"), return_requests, 1);

        auto* const keep_playing_widget{make_widget()};
        auto* const keep_button{
            IsValid(keep_playing_widget)
                ? Cast<ml::ioj::UMenuButtonWidget>(
                      keep_playing_widget->GetWidgetFromName(TEXT("keep_playing_button")))
                : nullptr};
        if (TestRunner->TestTrue(TEXT("Keep Playing button is valid"), IsValid(keep_button))) {
            keep_button->OnClicked().Broadcast();
            TestRunner->TestFalse(TEXT("Keep Playing deactivates completion"),
                                  keep_playing_widget->IsActivated());
        }
    }
};
