#include <SpaceGameS7/ScriptLevelSelectWidget.h>

#include <SpaceGame/support/logging/SandboxLogCategories.h>
#include <SpaceGame/system/GameSubsystem.h>

#include <Components/TextBlock.h>
#include <Components/VerticalBox.h>
#include <Engine/GameInstance.h>
#include <Kismet/GameplayStatics.h>

namespace ml::s7 {
void ULevelScriptButton::initialise(int32 const index, FString const& label, bool const valid) {
    index_ = index;
    valid_ = valid;
    auto* const text{NewObject<UTextBlock>(this)};
    text->SetText(FText::FromString(label));
    AddChild(text);
    OnClicked.AddDynamic(this, &ThisClass::handle_clicked);
    set_selected(false);
}

void ULevelScriptButton::set_selected(bool const selected_value) {
    if (selected_value) {
        SetBackgroundColor(FLinearColor{0.08f, 0.3f, 0.65f});
    } else if (valid_) {
        SetBackgroundColor(FLinearColor::White);
    } else {
        SetBackgroundColor(FLinearColor{0.4f, 0.08f, 0.08f});
    }
}

void ULevelScriptButton::handle_clicked() {
    selected.Broadcast(index_);
}

void UScriptLevelSelectWidget::NativeOnInitialized() {
    Super::NativeOnInitialized();

    if (!IsValid(level_list) || !IsValid(selected_file_text) || !IsValid(title_text) ||
        !IsValid(description_text) || !IsValid(status_text) || !IsValid(refresh_button) ||
        !IsValid(launch_button)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("UScriptLevelSelectWidget::NativeOnInitialized: One or more bound widgets "
                    "are invalid."));
        return;
    }

    refresh_button->OnClicked.AddDynamic(this, &ThisClass::handle_refresh);
    launch_button->OnClicked.AddDynamic(this, &ThisClass::handle_launch);
    launch_button->SetIsEnabled(false);
}

void UScriptLevelSelectWidget::activate() {
    refresh_levels();
    if (IsValid(level_list) && level_list->GetChildrenCount() > 0) {
        level_list->GetChildAt(0)->SetKeyboardFocus();
    } else if (IsValid(refresh_button)) {
        refresh_button->SetKeyboardFocus();
    } else {
        Super::activate();
    }
}

void UScriptLevelSelectWidget::refresh_levels() {
    if (!IsValid(level_list) || !IsValid(launch_button) || !IsValid(status_text) ||
        !IsValid(selected_file_text) || !IsValid(title_text) || !IsValid(description_text)) {
        return;
    }

    level_list->ClearChildren();
    entries_.Reset();
    level_buttons_.Reset();
    selected_index_ = INDEX_NONE;
    launch_button->SetIsEnabled(false);
    selected_file_text->SetText(FText::GetEmpty());
    title_text->SetText(FText::FromString(TEXT("Select a level")));
    description_text->SetText(
        FText::FromString(TEXT("Choose a script to inspect its metadata and validation result.")));

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

    auto const count{entries_.Num()};
    for (int32 i{0}; i < count; ++i) {
        auto const& entry{entries_[i]};
        auto label{entry.display_title};
        if (!entry) {
            label += TEXT(" [Invalid]");
        }
        label += FString::Printf(TEXT("  (%s)"), *entry.filename);

        auto* const button{NewObject<ULevelScriptButton>(this)};
        button->initialise(i, label, static_cast<bool>(entry));
        button->selected.AddUObject(this, &ThisClass::select_level);
        level_list->AddChildToVerticalBox(button);
        level_buttons_.Add(button);
    }

    if (status.IsEmpty()) {
        status = count == 0 ? FString::Printf(TEXT("No .scm files found in %s"), *catalog.directory)
                            : FString::Printf(TEXT("Found %d level script(s)."), count);
    }
    status_text->SetText(FText::FromString(status));
}

void UScriptLevelSelectWidget::select_level(int32 const index) {
    if (!entries_.IsValidIndex(index) || !IsValid(launch_button) || !IsValid(selected_file_text) ||
        !IsValid(title_text) || !IsValid(description_text) || !IsValid(status_text)) {
        return;
    }

    selected_index_ = index;
    auto const button_count{level_buttons_.Num()};
    for (int32 i{0}; i < button_count; ++i) {
        level_buttons_[i]->set_selected(i == index);
    }

    auto const& entry{entries_[index]};
    selected_file_text->SetText(FText::FromString(entry.filename));
    title_text->SetText(FText::FromString(entry.display_title));
    description_text->SetText(FText::FromString(entry.description));
    status_text->SetText(
        FText::FromString(entry ? FString{TEXT("Ready to launch.")} : entry.error));
    launch_button->SetIsEnabled(static_cast<bool>(entry));
    if (entry) {
        launch_button->SetKeyboardFocus();
    }
}

void UScriptLevelSelectWidget::handle_refresh() {
    refresh_levels();
}

void UScriptLevelSelectWidget::handle_launch() {
    if (!entries_.IsValidIndex(selected_index_) || !entries_[selected_index_] ||
        !IsValid(status_text) || !IsValid(launch_button)) {
        return;
    }

    auto* const game_instance{GetGameInstance()};
    auto* const subsystem{
        IsValid(game_instance) ? game_instance->GetSubsystem<ml::ioj::UGameSubsystem>() : nullptr};
    if (!IsValid(subsystem)) {
        status_text->SetText(FText::FromString(TEXT("Game subsystem is unavailable.")));
        return;
    }

    auto const& entry{entries_[selected_index_]};
    subsystem->set_pending_level(entry.definition.GetValue(), entry.path);
    launch_button->SetIsEnabled(false);
    status_text->SetText(FText::FromString(TEXT("Loading level...")));
    UGameplayStatics::OpenLevel(this, FName{TEXT("/SpaceGame/Levels/GameRuntime")});
}
}
