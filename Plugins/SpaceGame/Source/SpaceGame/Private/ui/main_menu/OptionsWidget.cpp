#include "SpaceGame/ui/main_menu/OptionsWidget.h"

#include "SGameOptionsView.h"
#include "SpaceGameSimulation/support/logging/SandboxLogCategories.h"
#include <SpaceGame/UnrealEndpoint.h>
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

    style_ = UGameInstance::GetSubsystem<UGameUiStyleSubsystem>(game_instance);
    if (!IsValid(style_)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("UOptionsWidget: Required game or settings subsystem is invalid."));
        return;
    }
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
    if (!IsValid(style_)) {
        return SNew(STextBlock)
            .Text(NSLOCTEXT("OptionsMenu", "Unavailable", "Options are unavailable."));
    }

    return SAssignNew(options_view_, SGameOptionsView)
        .IsEnabled_Lambda(
            [] { return unreal_endpoint().menu().transaction != SettingsTransaction::Saving; })
        .Capabilities(&unreal_endpoint().capabilities())
        .Style(&style_->get_ui_style())
        .Audio(GetGameInstance()->GetSubsystem<UGameAudio>()->facade())
        .Page(unreal_endpoint().menu().page)
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
    if (options_view_) {
        options_view_->show_page(page);
    }
}

void UOptionsWidget::focus_content() {
    if (options_view_.IsValid()) {
        options_view_->focus_content();
    }
}

void UOptionsWidget::handle_interaction_modal_changed(bool const visible) {
    modal_state_changed.Broadcast(visible);
}

void UOptionsWidget::present(MenuUpdate const& update) {
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
