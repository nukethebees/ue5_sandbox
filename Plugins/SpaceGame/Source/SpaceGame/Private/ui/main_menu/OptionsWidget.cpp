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

    settings_->settings_changed.AddUObject(this, &ThisClass::refresh_view);
    settings_->display_confirmation_changed.AddUObject(
        this, &ThisClass::handle_display_confirmation_changed);
}

void UOptionsWidget::NativeConstruct() {
    Super::NativeConstruct();
    refresh_view();
}

void UOptionsWidget::NativeOnAddedToFocusPath(FFocusEvent const& event) {
    Super::NativeOnAddedToFocusPath(event);
    content_focused.Broadcast();
}

auto UOptionsWidget::RebuildWidget() -> TSharedRef<SWidget> {
    if (!IsValid(settings_) || !IsValid(style_)) {
        return SNew(STextBlock)
            .Text(NSLOCTEXT("OptionsMenu", "Unavailable", "Options are unavailable."));
    }

    return SAssignNew(options_view_, SGameOptionsView)
        .IsEnabled_Lambda([this] { return !menu_presentation().saving; })
        .Capabilities(&system_capabilities())
        .Style(&style_->get_ui_style())
        .Audio(GetGameInstance()->GetSubsystem<UGameAudio>()->facade())
        .Page_UObject(this, &ThisClass::page)
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

void UOptionsWidget::show_page(MainMenuItem page) {
    if (rendered_page_ == page) {
        return;
    }
    rendered_page_ = page;
    if (auto category{settings_category(page)}) {
        settings_->load(GetOwningLocalPlayer());
    }
    if (options_view_) {
        options_view_->show_page();
    }
}

auto UOptionsWidget::page() const -> MainMenuItem {
    return menu_presentation().page;
}
void UOptionsWidget::focus_content() {
    if (options_view_.IsValid()) {
        options_view_->focus_content();
    }
}

void UOptionsWidget::handle_apply() {
    if (auto category{settings_category(page())}) {
        if (!settings_->apply(*category)) {
            queue_input_action(host::SettingsSaveResult{false});
        }
    }
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

void UOptionsWidget::present(MenuUpdate const& update) {
    if (update.operation == MenuOperation::Activate &&
        update.target.kind == MenuTarget::Kind::Control) {
        switch (update.target.control()) {
            case MenuControl::Apply:
            case MenuControl::Save:
                handle_apply();
                break;
            case MenuControl::KeepDisplay:
                handle_confirm_display();
                break;
            case MenuControl::RevertDisplay:
                handle_revert_display();
                break;
            default:
                break;
        }
    }
    if (options_view_) {
        options_view_->present(update);
    }
}
void UOptionsWidget::refresh_view() {
    if (options_view_.IsValid()) {
        options_view_->refresh();
    }
}

} // namespace ioj
