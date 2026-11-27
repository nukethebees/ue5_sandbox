#include "SpaceGame/ui/main_menu/MainMenuWidget.h"

#include "SMainMenuView.h"
#include "SpaceGame/ui/main_menu/LevelSelectWidget.h"
#include "SpaceGame/ui/main_menu/OptionsWidget.h"
#include "SpaceGame/ui/save_game/SaveGameViewerWidget.h"
#include "SpaceGamePresentation/ui/style/SpaceGameUiTheme.h"
#include "SpaceGameSimulation/support/logging/SandboxLogCategories.h"
#include <SpaceGame/UnrealMessages.h>
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

void UMainMenuWidget::prepare_for_open(TSubclassOf<ULevelSelectWidget> level_select_class,
                                       bool const focus_level_content,
                                       FName const preferred_level_id) {
    level_select_class_ = level_select_class;
    focus_level_content_ = focus_level_content;
    preferred_level_id_ = preferred_level_id;
    create_content_widgets();

    if (IsValid(level_select_widget_)) {
        level_select_widget_->prepare_for_open(preferred_level_id_);
        level_select_widget_->refresh();
        if (view_.IsValid()) {
            view_->set_level_content(level_select_widget_->TakeWidget());
        }
    }

    if (focus_level_content_ && view_.IsValid()) {
        view_->focus_content_on_next_focus();
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
            .Page_UObject(options_widget_, &UOptionsWidget::page)
            .Audio(IsValid(style_) ? GetGameInstance()->GetSubsystem<UGameAudio>()->facade()
                                   : FGameAudioFacade{})
            .LevelContent()[level_content]
            .ArchiveContent()[archive_content]
            .OptionsContent()[options_content]
            .OnPageSelected(FOnMainMenuPageSelected::CreateUObject(this, &ThisClass::request_page))
            .OnQuit(FSimpleDelegate::CreateUObject(this, &ThisClass::request_quit))
            .OnFocusContent(
                FSimpleDelegate::CreateUObject(this, &ThisClass::focus_active_content))};
    if (focus_level_content_) {
        view_->focus_content_on_next_focus();
    }

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
        if (focus_level_content_) {
            focus_level_content_ = false;
            focus_active_content();
        } else {
            view_->focus_navigation();
        }
    }

    return FReply::Handled();
}

auto UMainMenuWidget::handle_back() -> bool {
    if (page_modal_visible_) {
        if (IsValid(options_widget_) && options_widget_->page() == EMenuPage::DataArchive &&
            IsValid(save_game_viewer_)) {
            save_game_viewer_->request_back();
        } else if (IsValid(options_widget_) && options_widget_->page() >= EMenuPage::Video) {
            options_widget_->request_back();
        }

        return true;
    }

    if (view_.IsValid()) {
        view_->focus_navigation();
    }

    return true;
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
        if (IsValid(save_game_viewer_)) {
            save_game_viewer_->modal_state_changed.AddUObject(
                this, &ThisClass::handle_page_modal_changed);
        }
    }

    if (!IsValid(options_widget_)) {
        options_widget_ =
            IsValid(owning_player)
                ? CreateWidget<UOptionsWidget>(owning_player, UOptionsWidget::StaticClass())
            : IsValid(game_instance)
                ? CreateWidget<UOptionsWidget>(game_instance, UOptionsWidget::StaticClass())
            : IsValid(world) ? CreateWidget<UOptionsWidget>(world, UOptionsWidget::StaticClass())
                             : nullptr;
        if (IsValid(options_widget_)) {
            options_widget_->modal_state_changed.AddUObject(this,
                                                            &ThisClass::handle_page_modal_changed);
        }
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
        if (IsValid(level_select_widget_)) {
            level_select_widget_->prepare_for_open(preferred_level_id_);
        }
    }
}

void UMainMenuWidget::request_page(EMenuPage const page) {
    if (page_modal_visible_ || !IsValid(options_widget_)) {
        return;
    }
    options_widget_->select_page(page,
                                 FSimpleDelegate::CreateUObject(this, &ThisClass::show_page, page));
}

void UMainMenuWidget::show_page(EMenuPage const page) {
    if (view_.IsValid()) {
        view_->refresh_page();
    }

    if (page == EMenuPage::SelectLevel && IsValid(level_select_widget_)) {
        level_select_widget_->refresh();
    }
}

void UMainMenuWidget::request_quit() {
    if (page_modal_visible_) {
        return;
    }

    if (IsValid(options_widget_) && options_widget_->page() >= EMenuPage::Video) {
        options_widget_->request_leave(FSimpleDelegate::CreateUObject(this, &ThisClass::quit_game));
        return;
    }

    quit_game();
}

void UMainMenuWidget::quit_game() {
    UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

void UMainMenuWidget::focus_active_content() {
    if (IsValid(options_widget_) && options_widget_->page() == EMenuPage::SelectLevel &&
        IsValid(level_select_widget_)) {
        level_select_widget_->focus_primary_action();
    } else if (IsValid(options_widget_) && options_widget_->page() == EMenuPage::DataArchive &&
               IsValid(save_game_viewer_)) {
        save_game_viewer_->focus_primary_action();
    } else if (IsValid(options_widget_) && options_widget_->page() >= EMenuPage::Video) {
        options_widget_->focus_content();
    }
}

void UMainMenuWidget::handle_page_modal_changed(bool const visible) {
    page_modal_visible_ = visible;
    if (view_.IsValid()) {
        view_->set_navigation_enabled(!visible);
    }
}

} // namespace ioj
