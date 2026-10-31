#include "SpaceGame/ui/save_game/SaveGameViewerWidget.h"

#include "SpaceGame/persistence/SpaceSaveSubsystem.h"
#include "SpaceGame/system/GameSubsystem.h"
#include "SpaceGamePresentation/ui/style/SpaceGameUiTheme.h"
#include "SpaceGameSimulation/support/logging/SandboxLogCategories.h"
#include "SSaveGameViewerView.h"
#include <ioj/persistence/profile_manager.h>
#include <ioj/persistence/save_browser.h>
#include <SpaceGamePresentation/support/logging/PresentationLogCategories.h>

#include <SandboxCoreEngine/strings.h>

#include <Engine/GameInstance.h>

namespace ioj {
namespace save_game_viewer_widget {
auto format_duration(float const duration_seconds) -> FText {
    auto const total_seconds{FMath::Max(0, FMath::RoundToInt(duration_seconds))};
    auto const hours{total_seconds / 3600};
    auto const minutes{total_seconds % 3600 / 60};
    auto const seconds{total_seconds % 60};
    return FText::Format(NSLOCTEXT("SaveGameViewer", "DurationValue", "{0}:{1}:{2}"),
                         FText::AsNumber(hours, &FNumberFormattingOptions::DefaultNoGrouping()),
                         FText::FromString(FString::Printf(TEXT("%02d"), minutes)),
                         FText::FromString(FString::Printf(TEXT("%02d"), seconds)));
}

auto format_date(persistence::Timestamp const date) -> FText {
    return date == 0 ? FText::FromString(TEXT("-"))
                     : FText::FromString(
                           FDateTime::FromUnixTimestamp(date).ToString(TEXT("%Y-%m-%d %H:%M")));
}
} // namespace save_game_viewer_widget

/* **************************************** */
// Widget lifecycle
/* **************************************** */
USaveGameViewerWidget::USaveGameViewerWidget(FObjectInitializer const& object_initializer)
    : Super(object_initializer) {
    SetIsFocusable(true);
}

void USaveGameViewerWidget::NativeOnInitialized() {
    Super::NativeOnInitialized();

    auto* const game_instance{GetGameInstance()};
    game_ = UGameSubsystem::get(game_instance);
    if (!IsValid(game_)) {
        UE_LOG(LogSandboxUI,
               Warning,
               TEXT("USaveGameViewerWidget: Game subsystem is unavailable; using the default UI "
                    "theme."));
        auto const* const default_theme{GetDefault<USpaceGameUiTheme>()};
        check(IsValid(default_theme));
        fallback_style_ = default_theme->compile();
    }

    rebuild_profiles();
}

auto USaveGameViewerWidget::RebuildWidget() -> TSharedRef<SWidget> {
    auto const* const style{IsValid(game_) ? &game_->get_ui_style() : &fallback_style_};
    auto result{
        SAssignNew(view_, SSaveGameViewerView)
            .Style(style)
            .Audio(IsValid(game_) ? game_->get_audio() : FGameAudioFacade{})
            .OnProfileSelected(
                FOnSaveProfileSelected::CreateUObject(this, &ThisClass::select_profile))
            .OnOutcomeSelected(
                FOnSaveOutcomeSelected::CreateUObject(this, &ThisClass::select_outcome))
            .OnRefresh(FSimpleDelegate::CreateUObject(this, &ThisClass::refresh))
            .OnBeginCreate(FSimpleDelegate::CreateUObject(this, &ThisClass::begin_create_profile))
            .OnCreate(FOnCreateSaveProfile::CreateUObject(this, &ThisClass::handle_create_profile))
            .OnCancelCreate(FSimpleDelegate::CreateUObject(this, &ThisClass::cancel_create_profile))
            .OnActivate(FSimpleDelegate::CreateUObject(this, &ThisClass::handle_activate_profile))
            .OnResetTestProfile(
                FSimpleDelegate::CreateUObject(this, &ThisClass::handle_reset_test_profile))};
    publish_all();
    return result;
}

void USaveGameViewerWidget::ReleaseSlateResources(bool const release_children) {
    Super::ReleaseSlateResources(release_children);
    view_.Reset();
}

auto USaveGameViewerWidget::NativeOnFocusReceived(FGeometry const& geometry,
                                                  FFocusEvent const& focus_event) -> FReply {
    static_cast<void>(geometry);
    static_cast<void>(focus_event);
    focus_primary_action();
    return FReply::Handled();
}

/* **************************************** */
// Navigation and profile actions
/* **************************************** */
void USaveGameViewerWidget::set_browser(persistence::SaveGameBrowser& browser) {
    browser_override_ = &browser;
    rebuild_profiles();
}

void USaveGameViewerWidget::focus_primary_action() {
    if (view_.IsValid()) {
        view_->focus_selected_profile();
    }
}

auto USaveGameViewerWidget::get_focus_target() const -> UWidget* {
    return const_cast<USaveGameViewerWidget*>(this);
}

void USaveGameViewerWidget::refresh() {
    auto* const browser{resolve_browser()};
    if (!browser) {
        show_empty_profiles();
        publish_all();
        return;
    }

    browser->refresh();
    rebuild_profiles();
    focus_primary_action();
}

void USaveGameViewerWidget::begin_create_profile() {
    create_profile_open_ = true;
    if (view_.IsValid()) {
        view_->show_create_profile();
    }
    modal_state_changed.Broadcast(true);
}

void USaveGameViewerWidget::cancel_create_profile() {
    create_profile_open_ = false;
    if (view_.IsValid()) {
        view_->hide_create_profile();
    }
    modal_state_changed.Broadcast(false);
}

void USaveGameViewerWidget::request_back() {
    if (create_profile_open_) {
        cancel_create_profile();
        return;
    }
}

void USaveGameViewerWidget::handle_create_profile(FString const& display_name) {
    auto* const save_subsystem{resolve_save_subsystem()};
    if (!IsValid(save_subsystem)) {
        show_create_profile_error(
            NSLOCTEXT("SaveGameViewer", "CreateUnavailable", "Save profiles are unavailable."));
        return;
    }

    auto const response{save_subsystem->create_profile(display_name)};
    switch (response.result) {
        case persistence::CreateProfileResult::succeeded: {
            cancel_create_profile();
            refresh_and_select(to_fstring(response.profile_id));
            return;
        }
        case persistence::CreateProfileResult::empty_name: {
            show_create_profile_error(
                NSLOCTEXT("SaveGameViewer", "EmptyProfileName", "Enter a profile name."));
            return;
        }
        case persistence::CreateProfileResult::name_too_long: {
            show_create_profile_error(FText::Format(
                NSLOCTEXT("SaveGameViewer", "LongProfileName", "Use no more than {0} characters."),
                FText::AsNumber(persistence::ProfileManager::max_profile_name_length)));
            return;
        }
        case persistence::CreateProfileResult::duplicate_name: {
            show_create_profile_error(NSLOCTEXT(
                "SaveGameViewer", "DuplicateProfileName", "That profile already exists."));
            return;
        }
        case persistence::CreateProfileResult::persistence_failed: {
            show_create_profile_error(
                NSLOCTEXT("SaveGameViewer", "CreateFailed", "The profile could not be saved."));
            return;
        }
    }
}

void USaveGameViewerWidget::handle_activate_profile() {
    auto* const save_subsystem{resolve_save_subsystem()};
    if (!IsValid(save_subsystem) || selected_profile_id_.IsEmpty()) {
        return;
    }
    if (!save_subsystem->activate_profile(TCHAR_TO_UTF8(*selected_profile_id_))) {
        UE_LOG(LogSandboxUI,
               Warning,
               TEXT("USaveGameViewerWidget::handle_activate_profile: Failed to activate '%s'."),
               *selected_profile_id_);
        return;
    }
    refresh_and_select(selected_profile_id_);
}

void USaveGameViewerWidget::handle_reset_test_profile() {
#if !UE_BUILD_SHIPPING
    auto* const save_subsystem{resolve_save_subsystem()};
    if (!IsValid(save_subsystem) || !save_subsystem->reset_test_profile()) {
        UE_LOG(LogSandboxUI,
               Warning,
               TEXT("USaveGameViewerWidget::handle_reset_test_profile: Failed to reset the test "
                    "profile."));
        return;
    }

    refresh_and_select(to_fstring(save_subsystem->get_active_profile_id()));
#endif
}

/* **************************************** */
// Data sources and selection
/* **************************************** */
auto USaveGameViewerWidget::resolve_browser() -> persistence::SaveGameBrowser* {
    if (browser_override_) {
        return browser_override_;
    }

    if (!IsValid(game_)) {
        UE_LOG(LogSandboxUI,
               Warning,
               TEXT("USaveGameViewerWidget::resolve_browser: Game subsystem is invalid."));
        return nullptr;
    }
    return &game_->get_save_game_browser();
}

auto USaveGameViewerWidget::resolve_save_subsystem() const -> USpaceSaveSubsystem* {
    auto* const game_instance{GetGameInstance()};
    return USpaceSaveSubsystem::get(game_instance);
}

void USaveGameViewerWidget::refresh_and_select(FString const& profile_id) {
    auto* const browser{resolve_browser()};
    if (!browser) {
        return;
    }
    browser->refresh();
    selected_profile_id_ = profile_id;
    rebuild_profiles();
}

void USaveGameViewerWidget::show_create_profile_error(FText const& error) {
    if (view_.IsValid()) {
        view_->show_create_error(error);
    }
}

void USaveGameViewerWidget::rebuild_profiles() {
    view_state_ = FSaveGameViewState{};
    auto* const browser{resolve_browser()};
    auto const profiles{browser ? browser->get_summaries()
                                : std::span<persistence::ProfileSummary const>{}};
    if (profiles.empty()) {
        show_empty_profiles();
        publish_all();
        return;
    }

    auto const profile_count{static_cast<int32>(profiles.size())};
    view_state_.profiles.Reserve(profile_count);
    for (auto const& profile : profiles) {
        auto const marker{profile.active ? TEXT("ACTIVE // ") : TEXT("")};
        view_state_.profiles.Add(FSaveProfileViewRow{
            .profile_id = to_fstring(profile.profile_id),
            .text = FText::FromString(marker + to_fstring(profile.display_name).ToUpper()),
            .active = profile.active,
        });
    }
    view_state_.archive_status =
        FText::Format(NSLOCTEXT("SaveGameViewer", "ArchiveStatus", "{0} SERVICE RECORDS AVAILABLE"),
                      FText::AsNumber(profile_count));

    auto const requested_profile{selected_profile_id_};
    selected_profile_id_.Reset();
    apply_profile_selection(requested_profile.IsEmpty() ? to_fstring(profiles[0].profile_id)
                                                        : requested_profile);
    if (selected_profile_id_.IsEmpty()) {
        apply_profile_selection(to_fstring(profiles[0].profile_id));
    }
    publish_all();
}

void USaveGameViewerWidget::select_profile(FString const& profile_id) {
    apply_profile_selection(profile_id);
    publish_profile();
}

void USaveGameViewerWidget::apply_profile_selection(FString const& profile_id) {
    auto* const browser{resolve_browser()};
    if (!browser) {
        show_empty_profiles();
        return;
    }

    auto const profiles{browser->get_summaries()};
    auto const profile_count{static_cast<int32>(profiles.size())};
    for (int32 index{}; index < profile_count; ++index) {
        auto const& profile{profiles[index]};
        if (profile.profile_id != TCHAR_TO_UTF8(*profile_id)) {
            continue;
        }

        auto const profile_changed{selected_profile_id_ != profile_id};
        selected_profile_id_ = profile_id;
        view_state_.selected_profile_index = index;
        if (profile_changed) {
            selected_outcome_id_.Reset();
        }
        show_profile(profile);

        if (browser->load_profile_report(TCHAR_TO_UTF8(*profile_id))) {
            auto const* const report{browser->get_loaded_profile_report()};
            if (report) {
                rebuild_outcomes(*report);
            }
        } else {
            UE_LOG(LogSandboxUI,
                   Warning,
                   TEXT("USaveGameViewerWidget::select_profile: Failed to load profile '%s'."),
                   *profile_id);
            rebuild_outcomes(persistence::ProfileReport{.profile_id = TCHAR_TO_UTF8(*profile_id)});
        }
        return;
    }
}

void USaveGameViewerWidget::rebuild_outcomes(persistence::ProfileReport const& report) {
    view_state_.outcomes.Reset();
    view_state_.statistics.Reset();
    view_state_.selected_outcome_index = INDEX_NONE;
    view_state_.outcome_name = NSLOCTEXT("SaveGameViewer", "NoOperations", "NO OPERATIONS LOGGED");
    view_state_.outcome_status = NSLOCTEXT(
        "SaveGameViewer", "NoOperationsDetail", "This service record contains no mission reports.");
    view_state_.outcome_completed = FText::GetEmpty();
    view_state_.outcome_duration = FText::GetEmpty();
    view_state_.outcome_kills = FText::GetEmpty();

    if (report.outcomes.empty()) {
        selected_outcome_id_.Reset();
        return;
    }

    auto const outcome_count{static_cast<int32>(report.outcomes.size())};
    view_state_.outcomes.Reserve(outcome_count);
    for (auto const& outcome : report.outcomes) {
        view_state_.outcomes.Add(FSaveOutcomeViewRow{
            .outcome_id = to_fstring(outcome.outcome_id),
            .text = FText::FromString(to_fstring(outcome.display_name).ToUpper()),
        });
    }

    auto const requested_outcome{selected_outcome_id_};
    selected_outcome_id_.Reset();
    apply_outcome_selection(requested_outcome.IsEmpty() ? to_fstring(report.outcomes[0].outcome_id)
                                                        : requested_outcome);
    if (selected_outcome_id_.IsEmpty()) {
        apply_outcome_selection(to_fstring(report.outcomes[0].outcome_id));
    }
}

void USaveGameViewerWidget::select_outcome(FString const& outcome_id) {
    apply_outcome_selection(outcome_id);
    publish_outcome();
}

void USaveGameViewerWidget::apply_outcome_selection(FString const& outcome_id) {
    auto* const browser{resolve_browser()};
    auto const* const report{browser ? browser->get_loaded_profile_report() : nullptr};
    if (!report || report->profile_id != TCHAR_TO_UTF8(*selected_profile_id_)) {
        return;
    }

    auto const outcome_count{static_cast<int32>(report->outcomes.size())};
    for (int32 index{}; index < outcome_count; ++index) {
        auto const& outcome{report->outcomes[index]};
        if (outcome.outcome_id == TCHAR_TO_UTF8(*outcome_id)) {
            selected_outcome_id_ = outcome_id;
            view_state_.selected_outcome_index = index;
            show_outcome(outcome);
            return;
        }
    }
}

void USaveGameViewerWidget::show_profile(persistence::ProfileSummary const& profile) {
    view_state_.profile_name = FText::FromString(to_fstring(profile.display_name));
    view_state_.profile_status =
        profile.active ? NSLOCTEXT("SaveGameViewer", "ActiveProfile", "ACTIVE SERVICE RECORD")
                       : NSLOCTEXT("SaveGameViewer", "InactiveProfile", "ARCHIVED SERVICE RECORD");
    view_state_.profile_id =
        FText::Format(NSLOCTEXT("SaveGameViewer", "ProfileId", "ID  //  {0}"),
                      FText::FromString(to_fstring(profile.profile_id).ToUpper()));
    view_state_.profile_created =
        FText::Format(NSLOCTEXT("SaveGameViewer", "Created", "CREATED  //  {0}"),
                      save_game_viewer_widget::format_date(profile.created_at));
    view_state_.profile_last_played =
        FText::Format(NSLOCTEXT("SaveGameViewer", "LastPlayed", "LAST OPERATION  //  {0}"),
                      save_game_viewer_widget::format_date(profile.last_played_at));
    view_state_.profile_duration = FText::Format(
        NSLOCTEXT("SaveGameViewer", "TotalDuration", "DUTY TIME  //  {0}"),
        save_game_viewer_widget::format_duration(profile.total_simulation_duration_seconds));
    view_state_.profile_totals = FText::Format(
        NSLOCTEXT("SaveGameViewer", "ProfileTotals", "REPORTS  //  {0}    KILLS  //  {1}"),
        FText::AsNumber(profile.outcome_count),
        FText::AsNumber(profile.total_kills));
    view_state_.can_activate = !profile.active;
}

void USaveGameViewerWidget::show_outcome(persistence::LevelOutcomeSummary const& outcome) {
    view_state_.outcome_name = FText::FromString(to_fstring(outcome.display_name));
    view_state_.outcome_status = FText::FromString(to_fstring(outcome.result).ToUpper());
    view_state_.outcome_completed =
        FText::Format(NSLOCTEXT("SaveGameViewer", "LevelCompleted", "COMPLETED  //  {0}"),
                      save_game_viewer_widget::format_date(outcome.completed_at));
    view_state_.outcome_duration = FText::Format(
        NSLOCTEXT("SaveGameViewer", "LevelDuration", "DURATION  //  {0}"),
        save_game_viewer_widget::format_duration(outcome.simulation_duration_seconds));
    view_state_.outcome_kills =
        FText::Format(NSLOCTEXT("SaveGameViewer", "LevelKills", "CONFIRMED KILLS  //  {0}"),
                      FText::AsNumber(outcome.kills));
    view_state_.statistics.Reset(static_cast<int32>(outcome.statistics.size()));
    for (auto const& statistic : outcome.statistics) {
        view_state_.statistics.Add(FSaveStatisticViewRow{
            .label = FText::FromString(to_fstring(statistic.label).ToUpper()),
            .value = FText::FromString(to_fstring(statistic.value)),
        });
    }
}

void USaveGameViewerWidget::show_empty_profiles() {
    selected_profile_id_.Reset();
    selected_outcome_id_.Reset();
    view_state_ = FSaveGameViewState{};
    view_state_.archive_status =
        NSLOCTEXT("SaveGameViewer", "NoProfiles", "NO SERVICE RECORDS AVAILABLE");
    view_state_.profile_name = NSLOCTEXT("SaveGameViewer", "ArchiveEmpty", "ARCHIVE EMPTY");
    view_state_.profile_status = NSLOCTEXT("SaveGameViewer",
                                           "ArchiveEmptyDetail",
                                           "Create a service record to begin mission logging.");
    view_state_.outcome_name = NSLOCTEXT("SaveGameViewer", "NoReport", "NO REPORT SELECTED");
}

/* **************************************** */
// State publication
/* **************************************** */
void USaveGameViewerWidget::publish_all() {
    if (view_.IsValid()) {
        view_->replace_state(view_state_);
    }
}

void USaveGameViewerWidget::publish_profile() {
    if (view_.IsValid()) {
        view_->replace_profile(view_state_);
    }
}

void USaveGameViewerWidget::publish_outcome() {
    if (view_.IsValid()) {
        view_->update_outcome(view_state_);
    }
}
} // namespace ioj
