#include "SpaceGame/ui/main_menu/MainMenuWidget.h"

#include "SMainMenuView.h"
#include "SpaceGame/ui/main_menu/LevelSelectWidget.h"
#include "SpaceGame/ui/main_menu/OptionsWidget.h"
#include "SpaceGame/ui/save_game/SaveGameViewerWidget.h"
#include "SpaceGamePresentation/ui/style/SpaceGameUiTheme.h"
#include "SpaceGameSimulation/support/logging/SandboxLogCategories.h"
#include <SpaceGame/UnrealEndpoint.h>
#include <SpaceGamePresentation/audio/GameAudio.h>
#include <SpaceGamePresentation/support/logging/PresentationLogCategories.h>
#include <SpaceGamePresentation/ui/style/GameUiStyleSubsystem.h>

#include <Engine/GameInstance.h>
#include <Kismet/KismetSystemLibrary.h>
#include <Widgets/SNullWidget.h>

namespace ioj {
UMainMenuWidget::UMainMenuWidget() {
    SetIsFocusable(true);
}

void UMainMenuWidget::NativeOnInitialized() {
    Super::NativeOnInitialized();

    auto* const game_instance{GetGameInstance()};
    style_ = UGameInstance::GetSubsystem<UGameUiStyleSubsystem>(game_instance);
    if (!IsValid(style_)) {
        UE_LOG(LogSandboxUI,
               Warning,
               TEXT("UMainMenuWidget: Game subsystem is unavailable; using the default UI theme."));
        auto const* const default_theme{GetDefault<USpaceGameUiTheme>()};
        check(IsValid(default_theme));
        fallback_style_ = default_theme->compile();
    }
}

void UMainMenuWidget::prepare_for_open(TSubclassOf<ULevelSelectWidget> level_select_class) {
    level_select_class_ = level_select_class;
    create_content_widgets();

    if (IsValid(level_select_widget_)) {
        level_select_widget_->refresh();
        if (view_.IsValid()) {
            view_->set_level_content(level_select_widget_->TakeWidget());
        }
    }
}

auto UMainMenuWidget::RebuildWidget() -> TSharedRef<SWidget> {
    create_content_widgets();
    auto const* const style{IsValid(style_) ? &style_->get_ui_style() : &fallback_style_};
    auto const level_content{IsValid(level_select_widget_) ? level_select_widget_->TakeWidget()
                                                           : SNullWidget::NullWidget};
    auto const archive_content{IsValid(save_game_viewer_) ? save_game_viewer_->TakeWidget()
                                                          : SNullWidget::NullWidget};
    auto const options_content{IsValid(options_widget_) ? options_widget_->TakeWidget()
                                                        : SNullWidget::NullWidget};

    auto result{
        SAssignNew(view_, SMainMenuView)
            .Style(style)
            .Audio(IsValid(style_) ? GetGameInstance()->GetSubsystem<UGameAudio>()->facade()
                                   : FGameAudioFacade{})
            .LevelContent()[level_content]
            .ArchiveContent()[archive_content]
            .OptionsContent()[options_content]
            .OnItem(FOnMainMenuItem::CreateLambda(
                [](MainMenuItem item) { unreal_endpoint().hit(item); }))
            .OnRestoreFocus(FSimpleDelegate::CreateUObject(this, &ThisClass::restore_focus))};

    return result;
}

void UMainMenuWidget::ReleaseSlateResources(bool const release_children) {
    Super::ReleaseSlateResources(release_children);
    view_.Reset();
}

auto UMainMenuWidget::focus_target() const -> UWidget* {
    return const_cast<UMainMenuWidget*>(this);
}

auto UMainMenuWidget::NativeOnFocusReceived(FGeometry const& geometry,
                                            FFocusEvent const& focus_event) -> FReply {
    static_cast<void>(geometry);
    static_cast<void>(focus_event);
    if (view_.IsValid()) {
        restore_focus();
    }

    return FReply::Handled();
}

void UMainMenuWidget::create_content_widgets() {
    auto* const game_instance{GetGameInstance()};
    auto* const owning_player{GetOwningPlayer()};
    auto* const world{GetWorld()};
    if (!IsValid(save_game_viewer_)) {
        save_game_viewer_ =
            IsValid(owning_player)   ? CreateWidget<USaveGameViewerWidget>(
                                           owning_player, USaveGameViewerWidget::StaticClass())
            : IsValid(game_instance) ? CreateWidget<USaveGameViewerWidget>(
                                           game_instance, USaveGameViewerWidget::StaticClass())
            : IsValid(world)
                ? CreateWidget<USaveGameViewerWidget>(world, USaveGameViewerWidget::StaticClass())
                : nullptr;
    }

    if (!IsValid(options_widget_)) {
        options_widget_ =
            IsValid(owning_player)
                ? CreateWidget<UOptionsWidget>(owning_player, UOptionsWidget::StaticClass())
            : IsValid(game_instance)
                ? CreateWidget<UOptionsWidget>(game_instance, UOptionsWidget::StaticClass())
            : IsValid(world) ? CreateWidget<UOptionsWidget>(world, UOptionsWidget::StaticClass())
                             : nullptr;
    }

    if (!IsValid(level_select_widget_) && level_select_class_) {
        level_select_widget_ =
            IsValid(owning_player)   ? CreateWidget<ULevelSelectWidget>(owning_player,
                                                                        level_select_class_,
                                                                        TEXT("level_select_page"))
            : IsValid(game_instance) ? CreateWidget<ULevelSelectWidget>(game_instance,
                                                                        level_select_class_,
                                                                        TEXT("level_select_page"))
            : IsValid(world)         ? CreateWidget<ULevelSelectWidget>(
                                           world, level_select_class_, TEXT("level_select_page"))
                                     : nullptr;
    }
}

void UMainMenuWidget::present(MenuUpdate const& update) {
    if (update.target.kind == MenuTarget::Kind::Page) {
        auto const item{update.target.page()};
        if (options_widget_) {
            options_widget_->refresh_view();
        }
        if (update.operation == MenuOperation::Activate) {
            show_page(item);
        } else {
            focus_item(item);
        }
    } else if (unreal_endpoint().menu().page == MainMenuItem::SelectLevel && level_select_widget_) {
        level_select_widget_->present(update);
    } else if (unreal_endpoint().menu().page == MainMenuItem::DataArchive && save_game_viewer_) {
        save_game_viewer_->present(update);
    } else if (options_widget_) {
        options_widget_->present(update);
    }
}
void UMainMenuWidget::focus_item(MainMenuItem item) {
    if (view_) {
        view_->focus_item(item);
    }
}
void UMainMenuWidget::show_page(MainMenuItem page) {
    options_widget_->show_page(page);
    if (view_) {
        view_->refresh_page();
    }
    if (page == MainMenuItem::SelectLevel && level_select_widget_) {
        level_select_widget_->refresh();
    }
}

void UMainMenuWidget::restore_focus() {
    present({unreal_endpoint().menu().focus, MenuOperation::Focus});
}

void UMainMenuWidget::refresh_navigation() {
    if (view_.IsValid()) {
        view_->set_navigation_enabled(!unreal_endpoint().menu().navigation_blocked());
    }
}

} // namespace ioj
