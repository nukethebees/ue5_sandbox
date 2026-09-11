#include "SpaceGame/ui/main_menu/MainMenuWidget.h"
#include <SpaceGamePresentation/support/logging/PresentationLogCategories.h>

#include "SMainMenuView.h"
#include "SpaceGame/system/GameSubsystem.h"
#include "SpaceGame/ui/main_menu/DebugSettingsWidget.h"
#include "SpaceGame/ui/main_menu/LevelSelectWidget.h"
#include "SpaceGame/ui/main_menu/OptionsWidget.h"
#include "SpaceGame/ui/save_game/SaveGameViewerWidget.h"
#include "SpaceGame/ui/telemetry/TelemetryDashboardWidget.h"
#include "SpaceGamePresentation/ui/style/SpaceGameUiTheme.h"
#include "SpaceGameSimulation/support/logging/SandboxLogCategories.h"

#include <Engine/GameInstance.h>
#include <Kismet/KismetSystemLibrary.h>
#include <Widgets/SNullWidget.h>

namespace ml::ioj {
UMainMenuWidget::UMainMenuWidget() {
    SetIsFocusable(true);
}

void UMainMenuWidget::NativeOnInitialized() {
    Super::NativeOnInitialized();

    auto* const game_instance{GetGameInstance()};
    game_ = UGameSubsystem::get(game_instance);
    if (!IsValid(game_)) {
        UE_LOG(LogSandboxUI,
               Warning,
               TEXT("UMainMenuWidget: Game subsystem is unavailable; using the default UI theme."));
        auto const* const default_theme{GetDefault<USpaceGameUiTheme>()};
        check(IsValid(default_theme));
        fallback_style_ = default_theme->compile();
    }
}

void UMainMenuWidget::prepare_for_open(TSubclassOf<ULevelSelectWidget> level_select_class,
                                       bool const focus_mission_content,
                                       FName const preferred_level_id,
                                       bool const show_telemetry,
                                       FString telemetry_run_id,
                                       FString telemetry_error) {
    level_select_class_ = level_select_class;
    focus_mission_content_ = focus_mission_content;
    preferred_level_id_ = preferred_level_id;
    initial_telemetry_run_id_ = MoveTemp(telemetry_run_id);
    create_content_widgets();
    if (show_telemetry) {
        show_page(EMainMenuPage::Telemetry);
    }

    if (IsValid(level_select_widget_)) {
        level_select_widget_->prepare_for_open(preferred_level_id_);
        level_select_widget_->refresh();
        if (view_.IsValid()) {
            view_->set_mission_content(level_select_widget_->TakeWidget());
        }
    }
    if (focus_mission_content_ && view_.IsValid()) {
        view_->focus_content_on_next_focus();
    }
    if (show_telemetry && IsValid(telemetry_dashboard_)) {
        telemetry_dashboard_->set_external_error(MoveTemp(telemetry_error));
        if (!initial_telemetry_run_id_.IsEmpty()) {
            telemetry_dashboard_->select_run(initial_telemetry_run_id_);
        }
    }
}

void UMainMenuWidget::select_page(EMainMenuPage const page) {
    request_page(page);
}

auto UMainMenuWidget::RebuildWidget() -> TSharedRef<SWidget> {
    create_content_widgets();
    auto const* const style{IsValid(game_) ? &game_->get_ui_style() : &fallback_style_};
    auto const mission_content{IsValid(level_select_widget_) ? level_select_widget_->TakeWidget()
                                                             : SNullWidget::NullWidget};
    auto const archive_content{IsValid(save_game_viewer_) ? save_game_viewer_->TakeWidget()
                                                          : SNullWidget::NullWidget};
    auto const telemetry_content{IsValid(telemetry_dashboard_) ? telemetry_dashboard_->TakeWidget()
                                                               : SNullWidget::NullWidget};
    auto const options_content{IsValid(options_widget_) ? options_widget_->TakeWidget()
                                                        : SNullWidget::NullWidget};
    auto const debug_content{IsValid(debug_settings_widget_) ? debug_settings_widget_->TakeWidget()
                                                             : SNullWidget::NullWidget};

    auto result{
        SAssignNew(view_, SMainMenuView)
            .Style(style)
            .InitialPage(active_page_)
            .Audio(IsValid(game_) ? game_->get_audio() : FGameAudioFacade{})
            .MissionContent()[mission_content]
            .ArchiveContent()[archive_content]
            .TelemetryContent()[telemetry_content]
            .OptionsContent()[options_content]
            .DebugContent()[debug_content]
            .OnPageSelected(FOnMainMenuPageSelected::CreateUObject(this, &ThisClass::request_page))
            .OnQuit(FSimpleDelegate::CreateUObject(this, &ThisClass::request_quit))
            .OnFocusContent(
                FSimpleDelegate::CreateUObject(this, &ThisClass::focus_active_content))};
    if (focus_mission_content_) {
        view_->focus_content_on_next_focus();
    }
    return result;
}

void UMainMenuWidget::ReleaseSlateResources(bool const release_children) {
    Super::ReleaseSlateResources(release_children);
    view_.Reset();
}

auto UMainMenuWidget::NativeGetDesiredFocusTarget() const -> UWidget* {
    return const_cast<UMainMenuWidget*>(this);
}

auto UMainMenuWidget::NativeOnFocusReceived(FGeometry const& geometry,
                                            FFocusEvent const& focus_event) -> FReply {
    static_cast<void>(geometry);
    static_cast<void>(focus_event);
    if (view_.IsValid()) {
        if (focus_mission_content_) {
            focus_mission_content_ = false;
            focus_active_content();
        } else {
            view_->focus_navigation();
        }
    }
    return FReply::Handled();
}

auto UMainMenuWidget::NativeOnHandleBackAction() -> bool {
    if (page_modal_visible_) {
        if (active_page_ == EMainMenuPage::DataArchive && IsValid(save_game_viewer_)) {
            save_game_viewer_->request_back();
        } else if (is_options_page(active_page_) && IsValid(options_widget_)) {
            options_widget_->request_back();
        }
        return true;
    }
    if (view_.IsValid()) {
        view_->focus_navigation();
    }
    return true;
}

auto UMainMenuWidget::is_options_page(EMainMenuPage const page) -> bool {
    return page >= EMainMenuPage::Video && page <= EMainMenuPage::System;
}

auto UMainMenuWidget::options_tab_for_page(EMainMenuPage const page) -> EOptionsTab {
    switch (page) {
        case EMainMenuPage::Video:
            return EOptionsTab::Video;
        case EMainMenuPage::Gameplay:
            return EOptionsTab::Gameplay;
        case EMainMenuPage::Audio:
            return EOptionsTab::Audio;
        case EMainMenuPage::Controls:
            return EOptionsTab::Controls;
        case EMainMenuPage::Accessibility:
            return EOptionsTab::Accessibility;
        case EMainMenuPage::System:
            return EOptionsTab::System;
        case EMainMenuPage::SelectMission:
        case EMainMenuPage::DataArchive:
        case EMainMenuPage::Debug:
        case EMainMenuPage::Telemetry:
            break;
    }
    checkNoEntry();
    return EOptionsTab::Video;
}

void UMainMenuWidget::create_content_widgets() {
    auto* const game_instance{GetGameInstance()};
    auto* const owning_player{GetOwningPlayer()};
    auto* const world{GetWorld()};
    if (!IsValid(save_game_viewer_)) {
        save_game_viewer_ =
            IsValid(owning_player) ? CreateWidget<USaveGameViewerWidget>(
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
    if (!IsValid(debug_settings_widget_)) {
        debug_settings_widget_ =
            IsValid(owning_player) ? CreateWidget<UDebugSettingsWidget>(
                                         owning_player, UDebugSettingsWidget::StaticClass())
            : IsValid(game_instance) ? CreateWidget<UDebugSettingsWidget>(
                                           game_instance, UDebugSettingsWidget::StaticClass())
            : IsValid(world)
                ? CreateWidget<UDebugSettingsWidget>(world, UDebugSettingsWidget::StaticClass())
                : nullptr;
        if (IsValid(debug_settings_widget_)) {
            debug_settings_widget_->settings_changed.AddUObject(
                this, &ThisClass::handle_profile_debug_settings_changed);
        }
    }
    if (!IsValid(telemetry_dashboard_)) {
        telemetry_dashboard_ =
            IsValid(owning_player) ? CreateWidget<UTelemetryDashboardWidget>(
                                         owning_player, UTelemetryDashboardWidget::StaticClass())
            : IsValid(game_instance) ? CreateWidget<UTelemetryDashboardWidget>(
                                           game_instance, UTelemetryDashboardWidget::StaticClass())
            : IsValid(world) ? CreateWidget<UTelemetryDashboardWidget>(
                                   world, UTelemetryDashboardWidget::StaticClass())
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
        if (IsValid(level_select_widget_)) {
            level_select_widget_->prepare_for_open(preferred_level_id_);
        }
    }
}

void UMainMenuWidget::request_page(EMainMenuPage const page) {
    if (page == active_page_ || page_modal_visible_) {
        return;
    }
    if (is_options_page(active_page_) && !is_options_page(page) && IsValid(options_widget_)) {
        options_widget_->request_leave(
            FSimpleDelegate::CreateUObject(this, &ThisClass::show_page, page));
        return;
    }
    show_page(page);
}

void UMainMenuWidget::show_page(EMainMenuPage const page) {
    auto const entering_options{!options_open_ && is_options_page(page)};
    if (!is_options_page(page)) {
        options_open_ = false;
    } else if (entering_options) {
        options_open_ = true;
        if (IsValid(options_widget_)) {
            options_widget_->prepare_for_open();
        }
    }

    active_page_ = page;
    if (view_.IsValid()) {
        view_->set_active_page(page);
    }
    if (page == EMainMenuPage::SelectMission && IsValid(level_select_widget_)) {
        level_select_widget_->refresh();
    } else if (page == EMainMenuPage::Telemetry && IsValid(telemetry_dashboard_)) {
        telemetry_dashboard_->refresh();
    } else if (is_options_page(page) && IsValid(options_widget_)) {
        options_widget_->select_tab(options_tab_for_page(page));
    } else if (page == EMainMenuPage::Debug && IsValid(debug_settings_widget_)) {
        debug_settings_widget_->refresh();
    }
}

void UMainMenuWidget::request_quit() {
    if (page_modal_visible_) {
        return;
    }
    if (is_options_page(active_page_) && IsValid(options_widget_)) {
        options_widget_->request_leave(FSimpleDelegate::CreateUObject(this, &ThisClass::quit_game));
        return;
    }
    quit_game();
}

void UMainMenuWidget::quit_game() {
    UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

void UMainMenuWidget::focus_active_content() {
    if (active_page_ == EMainMenuPage::SelectMission && IsValid(level_select_widget_)) {
        level_select_widget_->focus_primary_action();
    } else if (active_page_ == EMainMenuPage::DataArchive && IsValid(save_game_viewer_)) {
        save_game_viewer_->focus_primary_action();
    } else if (active_page_ == EMainMenuPage::Telemetry && IsValid(telemetry_dashboard_)) {
        telemetry_dashboard_->focus_primary_action();
    } else if (is_options_page(active_page_) && IsValid(options_widget_)) {
        options_widget_->focus_content();
    } else if (active_page_ == EMainMenuPage::Debug && IsValid(debug_settings_widget_)) {
        debug_settings_widget_->focus_content();
    }
}

void UMainMenuWidget::handle_page_modal_changed(bool const visible) {
    page_modal_visible_ = visible;
    if (view_.IsValid()) {
        view_->set_navigation_enabled(!visible);
    }
}

void UMainMenuWidget::handle_profile_debug_settings_changed() {
    if (IsValid(level_select_widget_)) {
        level_select_widget_->refresh();
    }
}
} // namespace ml::ioj
