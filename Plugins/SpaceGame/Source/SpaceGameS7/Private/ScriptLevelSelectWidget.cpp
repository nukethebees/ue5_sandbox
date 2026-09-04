#include <SpaceGameS7/ScriptLevelSelectWidget.h>

#include <SpaceGame/persistence/SpaceSaveSubsystem.h>
#include <SpaceGame/support/logging/SandboxLogCategories.h>
#include <SpaceGame/system/GameSubsystem.h>

#include <Blueprint/WidgetTree.h>
#include <Components/MultiLineEditableTextBox.h>
#include <Components/TextBlock.h>
#include <Components/VerticalBox.h>
#include <Components/VerticalBoxSlot.h>
#include <Engine/GameInstance.h>
#include <Kismet/GameplayStatics.h>
#include <UObject/ConstructorHelpers.h>

namespace ml::s7 {
namespace {
auto progress_label(FLevelDefinition const& definition,
                    ml::ioj::FLevelProgressSummary const& progress) -> FString {
    if (!definition.mission.IsSet()) {
        return TEXT("No objective");
    }

    switch (progress.state) {
        case ml::ioj::ELevelProgressState::Completed:
            return TEXT("Completed");
        case ml::ioj::ELevelProgressState::Attempted:
            return TEXT("Attempted");
        case ml::ioj::ELevelProgressState::NotAttempted:
            return TEXT("Not attempted");
    }
    checkNoEntry();
    return {};
}

auto level_details(FLevelScriptEntry const& entry, ml::ioj::FLevelProgressSummary const& progress)
    -> FString {
    check(entry.definition.IsSet());
    auto const& definition{entry.definition.GetValue()};
    auto details{FString::Printf(TEXT("Level ID: %s\nTeams: %d    Entities: %d    Player: %s\n"
                                      "Progress: %s"),
                                 *definition.metadata.id.value.ToString(),
                                 definition.teams.Num(),
                                 definition.entities.num(),
                                 definition.player_entity_id.is_set() ? TEXT("Yes") : TEXT("No"),
                                 *progress_label(definition, progress))};
    if (!definition.mission.IsSet() || progress.attempt_count == 0) {
        return details;
    }

    details += FString::Printf(TEXT("\nAttempts: %d    Completions: %d    Best kills: %d"),
                               progress.attempt_count,
                               progress.completion_count,
                               progress.best_kills);
    if (progress.best_completion_time_seconds >= 0.0f) {
        details +=
            FString::Printf(TEXT("    Best time: %.1f s"), progress.best_completion_time_seconds);
    }
    return details;
}
}

UScriptLevelSelectWidget::UScriptLevelSelectWidget() {
    static ConstructorHelpers::FClassFinder<ml::ioj::UMenuButtonWidget> const menu_button_class{
        TEXT("/SpaceGame/UI/Common/WBP_MenuButton")};
    menu_button_class_ = menu_button_class.Class;
}

void UScriptLevelSelectWidget::NativeOnInitialized() {
    Super::NativeOnInitialized();

    if (!IsValid(level_list) || !IsValid(selected_file_text) || !IsValid(title_text) ||
        !IsValid(description_text) || !IsValid(status_text) || !IsValid(details_text) ||
        !IsValid(refresh_button) || !IsValid(launch_button) || !IsValid(start_paused_button)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("UScriptLevelSelectWidget::NativeOnInitialized: One or more bound widgets "
                    "are invalid."));
        return;
    }

    if (!create_script_preview()) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("UScriptLevelSelectWidget::NativeOnInitialized: Failed to create the native "
                    "script preview."));
        return;
    }

    refresh_button->OnClicked().AddUObject(this, &ThisClass::handle_refresh);
    launch_button->OnClicked().AddUObject(this, &ThisClass::handle_launch);
    start_paused_button->OnClicked().AddUObject(this, &ThisClass::handle_start_paused);
    launch_button->SetIsEnabled(false);
    start_paused_button->SetIsEnabled(false);
    refresh_levels();
}

auto UScriptLevelSelectWidget::create_script_preview() -> bool {
    auto* const parent{IsValid(status_text) ? Cast<UVerticalBox>(status_text->GetParent())
                                            : nullptr};
    if (!IsValid(parent) || !IsValid(WidgetTree)) {
        return false;
    }

    script_preview_ = WidgetTree->ConstructWidget<UMultiLineEditableTextBox>();
    if (!IsValid(script_preview_)) {
        return false;
    }

    script_preview_->SetIsReadOnly(true);
    auto* const preview_slot{parent->AddChildToVerticalBox(script_preview_)};
    if (!IsValid(preview_slot)) {
        return false;
    }
    preview_slot->SetSize(FSlateChildSize{ESlateSizeRule::Fill});
    preview_slot->SetPadding(FMargin{0.0f, 0.0f, 0.0f, 8.0f});
    return true;
}

void UScriptLevelSelectWidget::NativeOnActivated() {
    refresh_levels();
    if (!level_buttons_.IsEmpty() && IsValid(level_buttons_[0])) {
        desired_focus_target_ = level_buttons_[0];
    } else if (IsValid(refresh_button)) {
        desired_focus_target_ = refresh_button;
    } else {
        desired_focus_target_ = Super::NativeGetDesiredFocusTarget();
    }
    Super::NativeOnActivated();
}

auto UScriptLevelSelectWidget::NativeGetDesiredFocusTarget() const -> UWidget* {
    return IsValid(desired_focus_target_) ? desired_focus_target_.Get()
                                          : Super::NativeGetDesiredFocusTarget();
}

void UScriptLevelSelectWidget::refresh_levels() {
    if (!IsValid(level_list) || !IsValid(launch_button) || !IsValid(start_paused_button) ||
        !IsValid(status_text) || !IsValid(selected_file_text) || !IsValid(title_text) ||
        !IsValid(description_text) || !IsValid(details_text) || !IsValid(script_preview_)) {
        return;
    }

    level_list->ClearChildren();
    entries_.Reset();
    level_buttons_.Reset();
    selected_index_ = INDEX_NONE;
    launch_button->SetIsEnabled(false);
    start_paused_button->SetIsEnabled(false);
    script_preview_->SetText(FText::GetEmpty());
    selected_file_text->SetText(FText::GetEmpty());
    title_text->SetText(FText::FromString(TEXT("Select a level")));
    description_text->SetText(
        FText::FromString(TEXT("Choose a script to inspect its metadata and validation result.")));
    details_text->SetText(FText::GetEmpty());

    auto catalog{discover_level_scripts()};
    entries_ = MoveTemp(catalog.entries);
    auto status{MoveTemp(catalog.error)};

    auto* const game_instance{GetGameInstance()};
    auto* const subsystem{
        IsValid(game_instance) ? game_instance->GetSubsystem<ml::ioj::UGameSubsystem>() : nullptr};
    if (IsValid(subsystem) && subsystem->has_level_launch_error()) {
        auto launch_error{subsystem->take_level_launch_error()};
        status = status.IsEmpty() ? MoveTemp(launch_error)
                                  : status + TEXT("\n") + MoveTemp(launch_error);
    }

    if (!menu_button_class_) {
        status_text->SetText(FText::FromString(TEXT("Menu button class is unavailable.")));
        return;
    }

    auto const count{entries_.Num()};
    for (int32 i{0}; i < count; ++i) {
        auto const& entry{entries_[i]};
        auto* const button{
            CreateWidget<ml::ioj::UMenuButtonWidget>(GetWorld(), menu_button_class_)};
        if (!IsValid(button)) {
            UE_LOG(LogSandboxUI,
                   Error,
                   TEXT("UScriptLevelSelectWidget::refresh_levels: Failed to create level row."));
            return;
        }
        button->set_text(FText::FromString(entry.display_title));
        button->SetIsSelectable(true);
        button->SetIsToggleable(true);
        button->OnClicked().AddWeakLambda(this, [this, i] { select_level(i); });
        level_list->AddChildToVerticalBox(button);
        level_buttons_.Add(button);
    }

    auto const button_count{level_buttons_.Num()};
    for (int32 i{0}; i < button_count; ++i) {
        auto* const previous{i > 0 ? level_buttons_[i - 1].Get() : nullptr};
        auto* const next{i + 1 < button_count ? level_buttons_[i + 1].Get() : nullptr};
        if (IsValid(previous)) {
            level_buttons_[i]->SetNavigationRuleExplicit(EUINavigation::Up, previous);
        }
        if (IsValid(next)) {
            level_buttons_[i]->SetNavigationRuleExplicit(EUINavigation::Down, next);
        }
        level_buttons_[i]->SetNavigationRuleExplicit(EUINavigation::Left, launch_button);
    }

    if (status.IsEmpty()) {
        status = count == 0 ? FString::Printf(TEXT("No .scm files found in %s"), *catalog.directory)
                            : FString::Printf(TEXT("Found %d level script(s)."), count);
    }
    status_text->SetText(FText::FromString(status));
}

void UScriptLevelSelectWidget::select_level(int32 const index) {
    if (!entries_.IsValidIndex(index) || !IsValid(launch_button) || !IsValid(start_paused_button) ||
        !IsValid(selected_file_text) || !IsValid(title_text) || !IsValid(description_text) ||
        !IsValid(status_text) || !IsValid(details_text) || !IsValid(script_preview_)) {
        return;
    }

    selected_index_ = index;
    auto const button_count{level_buttons_.Num()};
    for (int32 i{0}; i < button_count; ++i) {
        level_buttons_[i]->SetIsSelected(i == index);
    }

    auto const& entry{entries_[index]};
    selected_file_text->SetText(FText::FromString(entry.filename));
    title_text->SetText(FText::FromString(entry.display_title));
    description_text->SetText(FText::FromString(entry.description));
    script_preview_->SetText(FText::FromString(entry.source_text));
    launch_button->SetIsEnabled(static_cast<bool>(entry));
    start_paused_button->SetIsEnabled(static_cast<bool>(entry));
    if (entry) {
        auto* const game_instance{GetGameInstance()};
        auto* const save_subsystem{
            IsValid(game_instance) ? game_instance->GetSubsystem<USpaceSaveSubsystem>() : nullptr};
        auto const progress{IsValid(save_subsystem) ? save_subsystem->get_level_progress(
                                                          entry.definition->metadata.id.value)
                                                    : ml::ioj::FLevelProgressSummary{}};
        status_text->SetText(FText::FromString(
            progress_label(entry.definition.GetValue(), progress) + TEXT(" - Ready to launch.")));
        details_text->SetText(FText::FromString(level_details(entry, progress)));
        desired_focus_target_ = launch_button;
        launch_button->SetNavigationRuleExplicit(EUINavigation::Right, level_buttons_[index]);
        RequestRefreshFocus();
    } else {
        status_text->SetText(FText::FromString(entry.error));
        details_text->SetText(FText::GetEmpty());
    }
}

void UScriptLevelSelectWidget::handle_refresh() {
    refresh_levels();
    desired_focus_target_ = !level_buttons_.IsEmpty() ? level_buttons_[0].Get() : refresh_button;
    RequestRefreshFocus();
}

void UScriptLevelSelectWidget::handle_launch() {
    launch_selected_level(ml::ioj::ELevelLaunchMode::Running);
}

void UScriptLevelSelectWidget::handle_start_paused() {
    launch_selected_level(ml::ioj::ELevelLaunchMode::Paused);
}

void UScriptLevelSelectWidget::launch_selected_level(ml::ioj::ELevelLaunchMode const launch_mode) {
    if (!entries_.IsValidIndex(selected_index_) || !entries_[selected_index_] ||
        !IsValid(status_text) || !IsValid(launch_button) || !IsValid(start_paused_button)) {
        return;
    }

    auto* const game_instance{GetGameInstance()};
    auto* const subsystem{
        IsValid(game_instance) ? game_instance->GetSubsystem<ml::ioj::UGameSubsystem>() : nullptr};
    if (!IsValid(subsystem)) {
        status_text->SetText(FText::FromString(TEXT("Game subsystem is unavailable.")));
        return;
    }

    auto& entry{entries_[selected_index_]};
    auto definition{MoveTemp(entry.definition.GetValue())};
    entry.definition.Reset();
    subsystem->set_pending_level(MoveTemp(definition), entry.path, launch_mode);
    launch_button->SetIsEnabled(false);
    start_paused_button->SetIsEnabled(false);
    status_text->SetText(FText::FromString(launch_mode == ml::ioj::ELevelLaunchMode::Paused
                                               ? TEXT("Loading level paused...")
                                               : TEXT("Loading level...")));
    UGameplayStatics::OpenLevel(this, FName{TEXT("/SpaceGame/Levels/GameRuntime")});
}
}
