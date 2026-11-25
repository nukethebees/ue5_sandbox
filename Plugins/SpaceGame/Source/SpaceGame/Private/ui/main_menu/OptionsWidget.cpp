#include "SpaceGame/ui/main_menu/OptionsWidget.h"

#include "SGameOptionsView.h"
#include "SpaceGame/settings/OptionsDraft.h"
#include "SpaceGameSimulation/support/logging/SandboxLogCategories.h"
#include <SpaceGame/UnrealMessages.h>
#include <SpaceGamePresentation/audio/GameAudio.h>
#include <SpaceGamePresentation/support/logging/PresentationLogCategories.h>
#include <SpaceGamePresentation/ui/style/GameUiStyleSubsystem.h>

#include <Engine/GameInstance.h>
#include <Widgets/Text/STextBlock.h>

namespace ioj {

UOptionsWidget::UOptionsWidget(FObjectInitializer const& object_initializer)
    : Super(object_initializer) {
    SetIsFocusable(true);
}

void UOptionsWidget::NativeOnInitialized() {
    Super::NativeOnInitialized();

    auto* const game_instance{GetGameInstance()};
    if (!IsValid(game_instance)) {
        UE_LOG(LogSandboxUI,
               Warning,
               TEXT("UOptionsWidget: Game instance is invalid; Options is unavailable."));
        return;
    }

    settings_ = NewObject<UOptionsDraft>(this);
    style_ = UGameInstance::GetSubsystem<UGameUiStyleSubsystem>(game_instance);
    if (!IsValid(settings_) || !IsValid(style_)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("UOptionsWidget: Required game or settings subsystem is invalid."));
        return;
    }

    settings_->save_completed.AddUObject(this, &ThisClass::handle_save_completed);
    settings_->settings_changed.AddUObject(this, &ThisClass::refresh_view);
    settings_->display_confirmation_changed.AddUObject(
        this, &ThisClass::handle_display_confirmation_changed);
}

void UOptionsWidget::NativeConstruct() {
    Super::NativeConstruct();
    refresh_view();
}

void UOptionsWidget::NativeDestruct() {
    cancel_leave();
    if (IsValid(settings_)) {
        settings_->cancel();
    }

    Super::NativeDestruct();
}

auto UOptionsWidget::RebuildWidget() -> TSharedRef<SWidget> {
    if (!IsValid(settings_) || !IsValid(style_)) {
        return SNew(STextBlock)
            .Text(NSLOCTEXT("OptionsMenu", "Unavailable", "Options are unavailable."));
    }

    return SAssignNew(options_view_, SGameOptionsView)
        .IsEnabled_Lambda([this] { return !settings_->save_pending(); })
        .Settings(settings_)
        .Capabilities(&player_capabilities())
        .Style(&style_->get_ui_style())
        .Audio(GetGameInstance()->GetSubsystem<UGameAudio>()->facade())
        .Page_UObject(this, &ThisClass::page)
        .OnApply(FSimpleDelegate::CreateUObject(this, &ThisClass::handle_apply))
        .OnReset(FSimpleDelegate::CreateUObject(this, &ThisClass::handle_reset))
        .OnDirtyApply(FSimpleDelegate::CreateUObject(this, &ThisClass::handle_dirty_apply))
        .OnDirtyDiscard(FSimpleDelegate::CreateUObject(this, &ThisClass::handle_dirty_discard))
        .OnDirtyStay(FSimpleDelegate::CreateUObject(this, &ThisClass::handle_dirty_stay))
        .OnConfirmDisplay(FSimpleDelegate::CreateUObject(this, &ThisClass::handle_confirm_display))
        .OnRevertDisplay(FSimpleDelegate::CreateUObject(this, &ThisClass::handle_revert_display))
        .OnInteractionModalChanged(FOnOptionsInteractionModalChanged::CreateUObject(
            this, &ThisClass::handle_interaction_modal_changed));
}

void UOptionsWidget::ReleaseSlateResources(bool const release_children) {
    Super::ReleaseSlateResources(release_children);
    options_view_.Reset();
}

auto UOptionsWidget::NativeOnFocusReceived(FGeometry const& geometry,
                                           FFocusEvent const& focus_event) -> FReply {
    static_cast<void>(geometry);
    static_cast<void>(focus_event);
    focus_content();
    return FReply::Handled();
}

void UOptionsWidget::select_page(EMenuPage page, FSimpleDelegate shown) {
    if (page == page_) {
        shown.ExecuteIfBound();
        return;
    }

    request_leave(
        FSimpleDelegate::CreateUObject(this, &ThisClass::open_page, page, MoveTemp(shown)));
}

void UOptionsWidget::open_page(EMenuPage page, FSimpleDelegate shown) {
    page_ = page;
    if (auto category{settings_category(page)}) {
        settings_->load(GetOwningLocalPlayer(), *category);
    }
    if (options_view_.IsValid()) {
        options_view_->show_page();
    }
    shown.ExecuteIfBound();
}

void UOptionsWidget::request_leave(FSimpleDelegate continuation) {
    if (!IsValid(settings_)) {
        continuation.ExecuteIfBound();
        return;
    }

    if (settings_->save_pending() || settings_->is_awaiting_display_confirmation()) {
        return;
    }
    if (!settings_->is_dirty()) {
        continuation.ExecuteIfBound();
        return;
    }

    leave_continuation_ = MoveTemp(continuation);
    if (options_view_.IsValid()) {
        options_view_->show_dirty_prompt();
    }
    modal_state_changed.Broadcast(true);
}

void UOptionsWidget::request_back() {
    if (IsValid(settings_) && settings_->save_pending()) {
        return;
    }

    if (options_view_.IsValid() && options_view_->dismiss_interaction_modal()) {
        return;
    }
    if (options_view_.IsValid() && options_view_->is_dirty_prompt_visible()) {
        options_view_->hide_dirty_prompt();
        cancel_leave();
        return;
    }
    if (IsValid(settings_) && settings_->is_awaiting_display_confirmation()) {
        settings_->revert_display_changes();
        return;
    }
    if (IsValid(settings_) && settings_->is_dirty()) {
        if (options_view_.IsValid()) {
            options_view_->show_dirty_prompt();
        }
        modal_state_changed.Broadcast(true);
        return;
    }
    if (IsValid(settings_)) {
        settings_->cancel();
    }
}

void UOptionsWidget::focus_content() {
    if (options_view_.IsValid()) {
        options_view_->focus_content();
    }
}

void UOptionsWidget::handle_apply() {
    if (auto category{settings_category(page_)}) {
        settings_->apply(*category);
    }
}

void UOptionsWidget::handle_reset() {
    auto const category{settings_category(page_)};
    if (IsValid(settings_) && category.IsSet()) {
        settings_->reset_category(category.GetValue());
        if (category.GetValue() == GameSettingCategory::Controls && options_view_.IsValid()) {
            options_view_->refresh_controls();
        }
    }
}

void UOptionsWidget::handle_dirty_apply() {
    if (!IsValid(settings_)) {
        return;
    }

    if (options_view_.IsValid()) {
        options_view_->hide_dirty_prompt();
    }

    if (!settings_->apply(settings_category(page_).GetValue())) {
        cancel_leave();
        return;
    }
}

void UOptionsWidget::handle_save_completed(bool succeeded) {
    if (succeeded && leave_continuation_.IsBound()) {
        complete_leave();
    } else {
        cancel_leave();
    }
}

void UOptionsWidget::handle_dirty_discard() {
    if (options_view_.IsValid()) {
        options_view_->hide_dirty_prompt();
    }
        settings_->cancel();
    complete_leave();
}

void UOptionsWidget::handle_dirty_stay() {
    if (options_view_.IsValid()) {
        options_view_->hide_dirty_prompt();
    }
    cancel_leave();
}

void UOptionsWidget::handle_confirm_display() {
    if (IsValid(settings_)) {
        settings_->confirm_display_changes();
    }
}

void UOptionsWidget::handle_revert_display() {
    if (IsValid(settings_)) {
        settings_->revert_display_changes();
    }
}

void UOptionsWidget::handle_display_confirmation_changed(bool const visible) {
    refresh_view();
    modal_state_changed.Broadcast(visible);
}

void UOptionsWidget::handle_interaction_modal_changed(bool const visible) {
    modal_state_changed.Broadcast(visible);
}

void UOptionsWidget::refresh_view() {
    if (options_view_.IsValid()) {
        options_view_->refresh();
    }
}

void UOptionsWidget::complete_leave() {
    auto continuation{MoveTemp(leave_continuation_)};
    leave_continuation_.Unbind();

    modal_state_changed.Broadcast(false);
    continuation.ExecuteIfBound();
}

void UOptionsWidget::cancel_leave() {
    leave_continuation_.Unbind();
    modal_state_changed.Broadcast(false);
}

} // namespace ioj
