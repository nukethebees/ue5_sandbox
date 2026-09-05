#include <SpaceGameS7/ScriptLevelSelectWidget.h>

#include <SpaceGame/levels/LevelUnlock.h>
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
auto format_level_row_title(FString title, ELevelRowState const state) -> FString {
    switch (state) {
        case ELevelRowState::Invalid:
            return TEXT("! ") + title;
        case ELevelRowState::Locked:
            return TEXT("\U0001F512 ") + title;
        case ELevelRowState::Unlocked:
            return TEXT("\u25CB ") + title;
        case ELevelRowState::Completed:
            return TEXT("\u2713 ") + title;
    }
    checkNoEntry();
    return title;
}

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

auto find_level_title(TArray<FLevelScriptEntry> const& entries, FLevelId const id) -> FText {
    for (auto const& entry : entries) {
        if (entry && entry.definition->metadata.id == id) {
            return FText::FromString(entry.display_title);
        }
    }
    return FText::FromName(id.value);
}

auto make_unlock_evaluator(TArray<FLevelScriptEntry> const& entries,
                           USpaceSaveSubsystem const* const save_subsystem)
    -> FLevelUnlockEvaluator {
    return FLevelUnlockEvaluator{
        [save_subsystem](FLevelId const id) {
            return IsValid(save_subsystem) && save_subsystem->is_level_completed(id);
        },
        [&entries](FLevelId const id) { return find_level_title(entries, id); },
    };
}

auto description_with_requirements(FLevelScriptEntry const& entry,
                                   FLevelUnlockStatus const& unlock_status) -> FString {
    auto description{entry.description};
    if (unlock_status.criteria.IsEmpty()) {
        return description;
    }

    if (!description.IsEmpty()) {
        description += TEXT("\n\n");
    }
    description += TEXT("Unlock requirements:");
    for (auto const& criterion : unlock_status.criteria) {
        description += FString::Printf(TEXT("\n%s %s"),
                                       criterion.satisfied ? TEXT("\u2713") : TEXT("\u2610"),
                                       *criterion.description.ToString());
    }
    return description;
}

auto row_state(FLevelUnlockStatus const& unlock_status,
               ml::ioj::FLevelProgressSummary const& progress) -> ELevelRowState {
    if (!unlock_status.unlocked) {
        return ELevelRowState::Locked;
    }
    return progress.state == ml::ioj::ELevelProgressState::Completed ? ELevelRowState::Completed
                                                                     : ELevelRowState::Unlocked;
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
    if (!IsValid(desired_focus_target_) && !level_buttons_.IsEmpty() &&
        IsValid(level_buttons_[0])) {
        desired_focus_target_ = level_buttons_[0];
    } else if (!IsValid(desired_focus_target_) && IsValid(refresh_button)) {
        desired_focus_target_ = refresh_button;
    } else if (!IsValid(desired_focus_target_)) {
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
        !IsValid(description_text) || !IsValid(details_text) || !IsValid(script_preview_) ||
        !IsValid(WidgetTree)) {
        return;
    }

    auto const focus_level_id{selected_level_id_.IsNone() ? get_preferred_level_id()
                                                          : selected_level_id_};

    level_list->ClearChildren();
    entries_.Reset();
    level_buttons_.Reset();
    level_entry_indices_.Reset();
    selected_entry_index_ = INDEX_NONE;
    selected_level_id_ = NAME_None;
    desired_focus_target_ = nullptr;
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
    auto campaigns{MoveTemp(catalog.campaigns)};
    auto status{MoveTemp(catalog.error)};

    auto* const game_instance{GetGameInstance()};
    auto* const game_subsystem{
        IsValid(game_instance) ? game_instance->GetSubsystem<ml::ioj::UGameSubsystem>() : nullptr};
    if (IsValid(game_subsystem) && game_subsystem->has_level_launch_error()) {
        auto launch_error{game_subsystem->take_level_launch_error()};
        status = status.IsEmpty() ? MoveTemp(launch_error)
                                  : status + TEXT("\n") + MoveTemp(launch_error);
    }

    if (!menu_button_class_) {
        status_text->SetText(FText::FromString(TEXT("Menu button class is unavailable.")));
        return;
    }

    auto* const save_subsystem{
        IsValid(game_instance) ? game_instance->GetSubsystem<USpaceSaveSubsystem>() : nullptr};
    auto const evaluator{make_unlock_evaluator(entries_, save_subsystem)};
    auto const entry_indices{[this] {
        TMap<FLevelId, int32> result;
        auto const count{entries_.Num()};
        for (int32 i{0}; i < count; ++i) {
            if (entries_[i]) {
                result.Add(entries_[i].definition->metadata.id, i);
            }
        }
        return result;
    }()};

    int32 preferred_button_index{INDEX_NONE};
    auto add_header = [this](FString const& label) {
        auto* const header{WidgetTree->ConstructWidget<UTextBlock>()};
        if (!IsValid(header)) {
            UE_LOG(LogSandboxUI,
                   Error,
                   TEXT("UScriptLevelSelectWidget::refresh_levels: Failed to create header."));
            return;
        }
        header->SetText(FText::FromString(label));
        header->SetColorAndOpacity(FSlateColor{FLinearColor{0.65f, 0.8f, 1.0f, 1.0f}});
        header->SetAutoWrapText(true);
        auto* const slot{level_list->AddChildToVerticalBox(header)};
        if (IsValid(slot)) {
            slot->SetPadding(FMargin{4.0f, 12.0f, 4.0f, 4.0f});
        }
    };
    auto add_level = [this, &evaluator, save_subsystem, focus_level_id, &preferred_button_index](
                         int32 const entry_index) {
        auto const& entry{entries_[entry_index]};
        auto* const button{
            CreateWidget<ml::ioj::UMenuButtonWidget>(GetWorld(), menu_button_class_)};
        if (!IsValid(button)) {
            UE_LOG(LogSandboxUI,
                   Error,
                   TEXT("UScriptLevelSelectWidget::refresh_levels: Failed to create level row."));
            return;
        }

        auto state{ELevelRowState::Invalid};
        auto unlocked{false};
        if (entry) {
            auto const id{entry.definition->metadata.id};
            auto const progress{IsValid(save_subsystem) ? save_subsystem->get_level_progress(id)
                                                        : ml::ioj::FLevelProgressSummary{}};
            auto const unlock_status{evaluator.evaluate(entry.definition.GetValue())};
            unlocked = unlock_status.unlocked;
            state = row_state(unlock_status, progress);
            if (preferred_button_index == INDEX_NONE && !focus_level_id.IsNone() &&
                id.value == focus_level_id) {
                preferred_button_index = level_buttons_.Num();
            }
        }

        button->set_text(FText::FromString(format_level_row_title(entry.display_title, state)));
        button->SetIsSelectable(true);
        button->SetIsToggleable(true);
        auto const button_index{level_buttons_.Num()};
        button->OnClicked().AddWeakLambda(this,
                                          [this, button_index] { select_level(button_index); });
        if (unlocked) {
            button->SetNavigationRuleExplicit(EUINavigation::Left, launch_button);
        }
        level_list->AddChildToVerticalBox(button);
        level_buttons_.Add(button);
        level_entry_indices_.Add(entry_index);
    };

    TSet<FLevelId> grouped_levels;
    for (auto const& campaign : campaigns) {
        if (!campaign) {
            continue;
        }
        add_header(campaign.definition->title);
        for (auto const level_id : campaign.definition->level_ids) {
            auto const* const entry_index{entry_indices.Find(level_id)};
            check(entry_index);
            add_level(*entry_index);
            grouped_levels.Add(level_id);
        }
    }

    bool has_other_levels{};
    auto const entry_count{entries_.Num()};
    for (int32 i{0}; i < entry_count; ++i) {
        if (!entries_[i] || grouped_levels.Contains(entries_[i].definition->metadata.id)) {
            continue;
        }
        if (!has_other_levels) {
            add_header(TEXT("Other Levels"));
            has_other_levels = true;
        }
        add_level(i);
    }

    bool has_invalid_levels{};
    for (int32 i{0}; i < entry_count; ++i) {
        if (entries_[i]) {
            continue;
        }
        if (!has_invalid_levels) {
            add_header(TEXT("Invalid Level Scripts"));
            has_invalid_levels = true;
        }
        add_level(i);
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
    }

    if (status.IsEmpty()) {
        status = entry_count == 0
                   ? FString::Printf(TEXT("No .scm files found in %s"), *catalog.directory)
                   : FString::Printf(TEXT("Found %d level script(s) in %d campaign(s)."),
                                     entry_count,
                                     campaigns.Num());
    }
    status_text->SetText(FText::FromString(status));

    if (!IsValid(desired_focus_target_)) {
        desired_focus_target_ =
            !level_buttons_.IsEmpty() ? level_buttons_[0].Get() : refresh_button;
    }

    if (preferred_button_index != INDEX_NONE) {
        restore_level_selection(preferred_button_index);
    }
}

void UScriptLevelSelectWidget::select_level(int32 const button_index) {
    apply_level_selection(button_index, true);
}

void UScriptLevelSelectWidget::restore_level_selection(int32 const button_index) {
    apply_level_selection(button_index, false);
}

void UScriptLevelSelectWidget::apply_level_selection(int32 const button_index,
                                                     bool const refresh_focus) {
    if (!level_buttons_.IsValidIndex(button_index) ||
        !level_entry_indices_.IsValidIndex(button_index) || !IsValid(launch_button) ||
        !IsValid(start_paused_button) || !IsValid(selected_file_text) || !IsValid(title_text) ||
        !IsValid(description_text) || !IsValid(status_text) || !IsValid(details_text) ||
        !IsValid(script_preview_)) {
        return;
    }

    auto const entry_index{level_entry_indices_[button_index]};
    if (!entries_.IsValidIndex(entry_index)) {
        return;
    }
    selected_entry_index_ = entry_index;
    selected_level_id_ =
        entries_[entry_index] ? entries_[entry_index].definition->metadata.id.value : NAME_None;
    auto const button_count{level_buttons_.Num()};
    for (int32 i{0}; i < button_count; ++i) {
        level_buttons_[i]->SetIsSelected(i == button_index);
    }

    auto const& entry{entries_[entry_index]};
    selected_file_text->SetText(FText::FromString(entry.filename));
    title_text->SetText(FText::FromString(entry.display_title));
    script_preview_->SetText(FText::FromString(entry.source_text));
    launch_button->SetIsEnabled(false);
    start_paused_button->SetIsEnabled(false);
    if (entry) {
        auto* const game_instance{GetGameInstance()};
        auto* const save_subsystem{
            IsValid(game_instance) ? game_instance->GetSubsystem<USpaceSaveSubsystem>() : nullptr};
        auto const progress{IsValid(save_subsystem)
                                ? save_subsystem->get_level_progress(entry.definition->metadata.id)
                                : ml::ioj::FLevelProgressSummary{}};
        auto const evaluator{make_unlock_evaluator(entries_, save_subsystem)};
        auto const unlock_status{evaluator.evaluate(entry.definition.GetValue())};
        description_text->SetText(
            FText::FromString(description_with_requirements(entry, unlock_status)));
        details_text->SetText(FText::FromString(level_details(entry, progress)));

        if (unlock_status.unlocked) {
            launch_button->SetIsEnabled(true);
            start_paused_button->SetIsEnabled(true);
            status_text->SetText(
                FText::FromString(progress_label(entry.definition.GetValue(), progress) +
                                  TEXT(" - Ready to launch.")));
            desired_focus_target_ = refresh_focus
                                      ? static_cast<UWidget*>(launch_button)
                                      : static_cast<UWidget*>(level_buttons_[button_index]);
            launch_button->SetNavigationRuleExplicit(EUINavigation::Right,
                                                     level_buttons_[button_index]);
        } else {
            status_text->SetText(
                FText::FromString(TEXT("Locked - Complete the unlock requirements below.")));
            desired_focus_target_ = level_buttons_[button_index];
        }
        if (refresh_focus) {
            RequestRefreshFocus();
        }
    } else {
        description_text->SetText(FText::FromString(entry.description));
        status_text->SetText(FText::FromString(entry.error));
        details_text->SetText(FText::GetEmpty());
        desired_focus_target_ = level_buttons_[button_index];
    }
}

void UScriptLevelSelectWidget::handle_refresh() {
    refresh_levels();
    RequestRefreshFocus();
}

void UScriptLevelSelectWidget::handle_launch() {
    launch_selected_level(ml::ioj::ELevelLaunchMode::Running);
}

void UScriptLevelSelectWidget::handle_start_paused() {
    launch_selected_level(ml::ioj::ELevelLaunchMode::Paused);
}

void UScriptLevelSelectWidget::launch_selected_level(ml::ioj::ELevelLaunchMode const launch_mode) {
    if (!entries_.IsValidIndex(selected_entry_index_) || !entries_[selected_entry_index_] ||
        !IsValid(status_text) || !IsValid(launch_button) || !IsValid(start_paused_button)) {
        return;
    }

    auto* const game_instance{GetGameInstance()};
    auto* const save_subsystem{
        IsValid(game_instance) ? game_instance->GetSubsystem<USpaceSaveSubsystem>() : nullptr};
    auto const evaluator{make_unlock_evaluator(entries_, save_subsystem)};
    auto const& selected_entry{entries_[selected_entry_index_]};
    if (!evaluator.evaluate(selected_entry.definition.GetValue()).unlocked) {
        launch_button->SetIsEnabled(false);
        start_paused_button->SetIsEnabled(false);
        status_text->SetText(
            FText::FromString(TEXT("Locked - Complete the unlock requirements below.")));
        return;
    }

    auto* const game_subsystem{
        IsValid(game_instance) ? game_instance->GetSubsystem<ml::ioj::UGameSubsystem>() : nullptr};
    if (!IsValid(game_subsystem)) {
        status_text->SetText(FText::FromString(TEXT("Game subsystem is unavailable.")));
        return;
    }

    auto& entry{entries_[selected_entry_index_]};
    auto definition{MoveTemp(entry.definition.GetValue())};
    entry.definition.Reset();
    game_subsystem->set_pending_level(MoveTemp(definition), entry.path, launch_mode);
    launch_button->SetIsEnabled(false);
    start_paused_button->SetIsEnabled(false);
    status_text->SetText(FText::FromString(launch_mode == ml::ioj::ELevelLaunchMode::Paused
                                               ? TEXT("Loading level paused...")
                                               : TEXT("Loading level...")));
    UGameplayStatics::OpenLevel(this, FName{TEXT("/SpaceGame/Levels/GameRuntime")});
}
}
