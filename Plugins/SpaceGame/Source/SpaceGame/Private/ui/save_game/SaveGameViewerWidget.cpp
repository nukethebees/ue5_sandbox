#include "SpaceGame/ui/save_game/SaveGameViewerWidget.h"
#include <SpaceGamePresentation/support/logging/PresentationLogCategories.h>

#include "SpaceGame/persistence/SaveGameBrowser.h"
#include "SpaceGame/persistence/SaveProfileManager.h"
#include "SpaceGame/persistence/SpaceSaveSubsystem.h"
#include "SpaceGame/system/GameSubsystem.h"
#include "SpaceGamePresentation/ui/style/SpaceGameUiTheme.h"
#include "SpaceGameSimulation/support/logging/SandboxLogCategories.h"
#include "SSaveGameViewerView.h"

#include <Engine/GameInstance.h>

namespace ml::ioj {
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

auto format_date(FDateTime const& date) -> FText {
    return date == FDateTime{} ? FText::FromString(TEXT("—"))
                               : FText::FromString(date.ToString(TEXT("%Y-%m-%d %H:%M")));
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
    game_ = IsValid(game_instance) ? game_instance->GetSubsystem<UGameSubsystem>() : nullptr;
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
void USaveGameViewerWidget::set_browser(FSaveGameBrowser& browser) {
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
        case ECreateSaveProfileResult::succeeded: {
            cancel_create_profile();
            refresh_and_select(response.profile_id);
            return;
        }
        case ECreateSaveProfileResult::empty_name: {
            show_create_profile_error(
                NSLOCTEXT("SaveGameViewer", "EmptyProfileName", "Enter a profile name."));
            return;
        }
        case ECreateSaveProfileResult::name_too_long: {
            show_create_profile_error(FText::Format(
                NSLOCTEXT("SaveGameViewer", "LongProfileName", "Use no more than {0} characters."),
                FText::AsNumber(FSaveProfileManager::max_profile_name_length)));
            return;
        }
        case ECreateSaveProfileResult::duplicate_name: {
            show_create_profile_error(NSLOCTEXT(
                "SaveGameViewer", "DuplicateProfileName", "That profile already exists."));
            return;
        }
        case ECreateSaveProfileResult::persistence_failed: {
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
    if (!save_subsystem->activate_profile(selected_profile_id_)) {
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

    refresh_and_select(save_subsystem->get_active_profile_id());
#endif
}

/* **************************************** */
// Data sources and selection
/* **************************************** */
auto USaveGameViewerWidget::resolve_browser() -> FSaveGameBrowser* {
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
    return IsValid(game_instance) ? game_instance->GetSubsystem<USpaceSaveSubsystem>() : nullptr;
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
                                : TConstArrayView<FSaveProfileSummary>{}};
    if (profiles.IsEmpty()) {
        show_empty_profiles();
        publish_all();
        return;
    }

    auto const profile_count{profiles.Num()};
    view_state_.profiles.Reserve(profile_count);
    for (auto const& profile : profiles) {
        auto const marker{profile.active ? TEXT("ACTIVE // ") : TEXT("")};
        view_state_.profiles.Add(FSaveProfileViewRow{
            .profile_id = profile.profile_id,
            .text = FText::FromString(marker + profile.display_name.ToUpper()),
            .active = profile.active,
        });
    }
    view_state_.archive_status =
        FText::Format(NSLOCTEXT("SaveGameViewer", "ArchiveStatus", "{0} SERVICE RECORDS AVAILABLE"),
                      FText::AsNumber(profile_count));

    auto const requested_profile{selected_profile_id_};
    selected_profile_id_.Reset();
    apply_profile_selection(requested_profile.IsEmpty() ? profiles[0].profile_id
                                                        : requested_profile);
    if (selected_profile_id_.IsEmpty()) {
        apply_profile_selection(profiles[0].profile_id);
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
    auto const profile_count{profiles.Num()};
    for (int32 index{}; index < profile_count; ++index) {
        auto const& profile{profiles[index]};
        if (profile.profile_id != profile_id) {
            continue;
        }

        auto const profile_changed{selected_profile_id_ != profile_id};
        selected_profile_id_ = profile_id;
        view_state_.selected_profile_index = index;
        if (profile_changed) {
            selected_outcome_id_.Reset();
        }
        show_profile(profile);

        if (browser->load_profile_report(profile_id)) {
            auto const* const report{browser->get_loaded_profile_report()};
            if (report) {
                rebuild_outcomes(*report);
            }
        } else {
            UE_LOG(LogSandboxUI,
                   Warning,
                   TEXT("USaveGameViewerWidget::select_profile: Failed to load profile '%s'."),
                   *profile_id);
            rebuild_outcomes(FSaveProfileReport{.profile_id = profile_id});
        }
        return;
    }
}

void USaveGameViewerWidget::rebuild_outcomes(FSaveProfileReport const& report) {
    view_state_.outcomes.Reset();
    view_state_.statistics.Reset();
    view_state_.selected_outcome_index = INDEX_NONE;
    view_state_.outcome_name = NSLOCTEXT("SaveGameViewer", "NoOperations", "NO OPERATIONS LOGGED");
    view_state_.outcome_status = NSLOCTEXT(
        "SaveGameViewer", "NoOperationsDetail", "This service record contains no mission reports.");
    view_state_.outcome_completed = FText::GetEmpty();
    view_state_.outcome_duration = FText::GetEmpty();
    view_state_.outcome_kills = FText::GetEmpty();

    if (report.outcomes.IsEmpty()) {
        selected_outcome_id_.Reset();
        return;
    }

    auto const outcome_count{report.outcomes.Num()};
    view_state_.outcomes.Reserve(outcome_count);
    for (auto const& outcome : report.outcomes) {
        view_state_.outcomes.Add(FSaveOutcomeViewRow{
            .outcome_id = outcome.outcome_id,
            .text = FText::FromString(outcome.display_name.ToUpper()),
        });
    }

    auto const requested_outcome{selected_outcome_id_};
    selected_outcome_id_.Reset();
    apply_outcome_selection(requested_outcome.IsEmpty() ? report.outcomes[0].outcome_id
                                                        : requested_outcome);
    if (selected_outcome_id_.IsEmpty()) {
        apply_outcome_selection(report.outcomes[0].outcome_id);
    }
}

void USaveGameViewerWidget::select_outcome(FString const& outcome_id) {
    apply_outcome_selection(outcome_id);
    publish_outcome();
}

void USaveGameViewerWidget::apply_outcome_selection(FString const& outcome_id) {
    auto* const browser{resolve_browser()};
    auto const* const report{browser ? browser->get_loaded_profile_report() : nullptr};
    if (!report || report->profile_id != selected_profile_id_) {
        return;
    }

    auto const outcome_count{report->outcomes.Num()};
    for (int32 index{}; index < outcome_count; ++index) {
        auto const& outcome{report->outcomes[index]};
        if (outcome.outcome_id == outcome_id) {
            selected_outcome_id_ = outcome_id;
            view_state_.selected_outcome_index = index;
            show_outcome(outcome);
            return;
        }
    }
}

void USaveGameViewerWidget::show_profile(FSaveProfileSummary const& profile) {
    view_state_.profile_name = FText::FromString(profile.display_name);
    view_state_.profile_status =
        profile.active ? NSLOCTEXT("SaveGameViewer", "ActiveProfile", "ACTIVE SERVICE RECORD")
                       : NSLOCTEXT("SaveGameViewer", "InactiveProfile", "ARCHIVED SERVICE RECORD");
    view_state_.profile_id = FText::Format(NSLOCTEXT("SaveGameViewer", "ProfileId", "ID  //  {0}"),
                                           FText::FromString(profile.profile_id.ToUpper()));
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

void USaveGameViewerWidget::show_outcome(FLevelOutcomeSummary const& outcome) {
    view_state_.outcome_name = FText::FromString(outcome.display_name);
    view_state_.outcome_status = FText::FromString(outcome.result.ToUpper());
    view_state_.outcome_completed =
        FText::Format(NSLOCTEXT("SaveGameViewer", "LevelCompleted", "COMPLETED  //  {0}"),
                      save_game_viewer_widget::format_date(outcome.completed_at));
    view_state_.outcome_duration = FText::Format(
        NSLOCTEXT("SaveGameViewer", "LevelDuration", "DURATION  //  {0}"),
        save_game_viewer_widget::format_duration(outcome.simulation_duration_seconds));
    view_state_.outcome_kills =
        FText::Format(NSLOCTEXT("SaveGameViewer", "LevelKills", "CONFIRMED KILLS  //  {0}"),
                      FText::AsNumber(outcome.kills));
    view_state_.statistics.Reset(outcome.statistics.Num());
    for (auto const& statistic : outcome.statistics) {
        view_state_.statistics.Add(FSaveStatisticViewRow{
            .label = FText::FromString(statistic.label.ToUpper()),
            .value = FText::FromString(statistic.value),
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
} // namespace ml::ioj
