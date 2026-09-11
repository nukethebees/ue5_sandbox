#include "SpaceGame/ui/main_menu/OptionsWidget.h"
#include <SpaceGamePresentation/support/logging/PresentationLogCategories.h>

#include "SGameOptionsView.h"
#include "SpaceGame/settings/GameSettingsSubsystem.h"
#include "SpaceGame/system/GameSubsystem.h"
#include "SpaceGameSimulation/support/logging/SandboxLogCategories.h"

#include <Engine/GameInstance.h>
#include <Widgets/Text/STextBlock.h>

namespace ml::ioj {

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
    settings_ = game_instance->GetSubsystem<UGameSettingsSubsystem>();
    game_ = UGameSubsystem::get(game_instance);
    if (!IsValid(settings_) || !IsValid(game_)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("UOptionsWidget: Required game or settings subsystem is invalid."));
        return;
    }

    settings_->settings_changed.AddUObject(this, &ThisClass::refresh_view);
    settings_->display_confirmation_changed.AddUObject(
        this, &ThisClass::handle_display_confirmation_changed);
}

void UOptionsWidget::NativeConstruct() {
    Super::NativeConstruct();
    refresh_view();
}

auto UOptionsWidget::RebuildWidget() -> TSharedRef<SWidget> {
    if (!IsValid(settings_) || !IsValid(game_)) {
        return SNew(STextBlock)
            .Text(NSLOCTEXT("OptionsMenu", "Unavailable", "Options are unavailable."));
    }

    return SAssignNew(options_view_, SGameOptionsView)
        .Settings(settings_)
        .Capabilities(&game_->get_platform_capabilities())
        .Style(&game_->get_ui_style())
        .Audio(game_->get_audio())
        .InitialTab(active_tab_)
        .OnTabChanged(FOnOptionsTabChanged::CreateUObject(this, &ThisClass::handle_tab_changed))
        .OnApply(FSimpleDelegate::CreateUObject(this, &ThisClass::handle_apply))
        .OnReset(FSimpleDelegate::CreateUObject(this, &ThisClass::handle_reset))
        .OnDirtyApply(FSimpleDelegate::CreateUObject(this, &ThisClass::handle_dirty_apply))
        .OnDirtyDiscard(FSimpleDelegate::CreateUObject(this, &ThisClass::handle_dirty_discard))
        .OnDirtyStay(FSimpleDelegate::CreateUObject(this, &ThisClass::handle_dirty_stay))
        .OnConfirmDisplay(FSimpleDelegate::CreateUObject(this, &ThisClass::handle_confirm_display))
        .OnRevertDisplay(FSimpleDelegate::CreateUObject(this, &ThisClass::handle_revert_display));
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

void UOptionsWidget::select_tab(EOptionsTab const tab) {
    active_tab_ = tab;
    if (options_view_.IsValid()) {
        options_view_->set_active_tab(tab);
    }
}

void UOptionsWidget::prepare_for_open() {
    exit_after_confirmation_ = false;
    leave_continuation_.Unbind();
    if (IsValid(settings_)) {
        settings_->begin_edit(GetOwningLocalPlayer());
    }
    if (options_view_.IsValid()) {
        options_view_->hide_dirty_prompt();
        options_view_->refresh_controls();
    }
}

void UOptionsWidget::request_leave(FSimpleDelegate continuation) {
    if (!IsValid(settings_)) {
        continuation.ExecuteIfBound();
        return;
    }
    if (settings_->is_awaiting_display_confirmation()) {
        return;
    }
    if (!settings_->is_dirty()) {
        settings_->cancel();
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

auto UOptionsWidget::get_focus_target() const -> UWidget* {
    return const_cast<UOptionsWidget*>(this);
}

void UOptionsWidget::handle_tab_changed(EOptionsTab const tab) {
    active_tab_ = tab;
}

void UOptionsWidget::handle_apply() {
    exit_after_confirmation_ = false;
    if (IsValid(settings_)) {
        settings_->apply();
    }
}

void UOptionsWidget::handle_reset() {
    auto const category{active_category()};
    if (IsValid(settings_) && category.IsSet()) {
        settings_->reset_category(category.GetValue());
    }
}

void UOptionsWidget::handle_dirty_apply() {
    if (!IsValid(settings_)) {
        return;
    }
    exit_after_confirmation_ = true;
    if (options_view_.IsValid()) {
        options_view_->hide_dirty_prompt();
    }
    settings_->apply();
    if (!settings_->is_awaiting_display_confirmation()) {
        complete_leave();
    }
}

void UOptionsWidget::handle_dirty_discard() {
    if (options_view_.IsValid()) {
        options_view_->hide_dirty_prompt();
    }
    if (IsValid(settings_)) {
        settings_->cancel();
    }
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
    if (!visible && exit_after_confirmation_) {
        if (IsValid(settings_)) {
            settings_->cancel();
        }
        complete_leave();
        return;
    }
    modal_state_changed.Broadcast(visible);
}

void UOptionsWidget::refresh_view() {
    if (options_view_.IsValid()) {
        options_view_->refresh();
    }
}

auto UOptionsWidget::active_category() const -> TOptional<EGameSettingCategory> {
    switch (active_tab_) {
        case EOptionsTab::Video:
            return EGameSettingCategory::Video;
        case EOptionsTab::Gameplay:
            return EGameSettingCategory::Gameplay;
        case EOptionsTab::Audio:
            return EGameSettingCategory::Audio;
        case EOptionsTab::Controls:
            return EGameSettingCategory::Controls;
        case EOptionsTab::Accessibility:
            return EGameSettingCategory::Accessibility;
        case EOptionsTab::System:
            return {};
    }
    return {};
}

void UOptionsWidget::complete_leave() {
    auto continuation{MoveTemp(leave_continuation_)};
    leave_continuation_.Unbind();
    exit_after_confirmation_ = false;
    if (IsValid(settings_)) {
        settings_->cancel();
    }
    modal_state_changed.Broadcast(false);
    continuation.ExecuteIfBound();
}

void UOptionsWidget::cancel_leave() {
    leave_continuation_.Unbind();
    exit_after_confirmation_ = false;
    modal_state_changed.Broadcast(false);
}

} // namespace ml::ioj
