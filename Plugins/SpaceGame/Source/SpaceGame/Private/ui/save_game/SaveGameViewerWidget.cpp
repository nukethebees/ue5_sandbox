#include "SpaceGame/ui/save_game/SaveGameViewerWidget.h"

#include "SpaceGame/UnrealEndpoint.h"
#include "SpaceGamePresentation/ui/style/SpaceGameUiTheme.h"
#include "SpaceGameSimulation/support/logging/SandboxLogCategories.h"
#include "SSaveGameViewerView.h"
#include <ioj/presentation/profile_view.h>
#include <ioj/profiles/profile_manager.h>
#include <SpaceGamePresentation/audio/GameAudio.h>
#include <SpaceGamePresentation/support/logging/PresentationLogCategories.h>
#include <SpaceGamePresentation/ui/style/GameUiStyleSubsystem.h>

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

auto format_date(profiles::Timestamp const date) -> FText {
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
    style_ = UGameInstance::GetSubsystem<UGameUiStyleSubsystem>(game_instance);
    if (!IsValid(style_)) {
        UE_LOG(LogSandboxUI,
               Warning,
               TEXT("USaveGameViewerWidget: Game subsystem is unavailable; using the default UI "
                    "theme."));
        auto const* const default_theme{GetDefault<USpaceGameUiTheme>()};
        check(IsValid(default_theme));
        fallback_style_ = default_theme->compile();
    }

    unreal_endpoint().send(host::UnrealMessageKind::RefreshProfiles);
}

auto USaveGameViewerWidget::RebuildWidget() -> TSharedRef<SWidget> {
    auto const* const style{IsValid(style_) ? &style_->get_ui_style() : &fallback_style_};
    auto result{SAssignNew(view_, SSaveGameViewerView)
                    .Style(style)
                    .Audio(IsValid(style_) ? GetGameInstance()->GetSubsystem<UGameAudio>()->facade()
                                           : FGameAudioFacade{})};
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
    present({unreal_endpoint().menu().focus, MenuOperation::Focus});
    return FReply::Handled();
}

/* **************************************** */
// Navigation and profile actions
/* **************************************** */
void USaveGameViewerWidget::present(MenuUpdate const& update) {
    if (update.operation == MenuOperation::Activate &&
        update.target == MenuTarget{MenuControl::CreateProfile}) {
        begin_create_profile();
        return;
    }
    if (update.operation == MenuOperation::Activate &&
        update.target == MenuTarget{MenuControl::CancelCreate}) {
        cancel_create_profile();
        return;
    }
    if (view_) {
        view_->present(update);
    }
}

void USaveGameViewerWidget::begin_create_profile() {
    if (view_.IsValid()) {
        view_->show_create_profile();
    }
}

void USaveGameViewerWidget::cancel_create_profile() {
    if (view_.IsValid()) {
        view_->hide_create_profile();
    }
}

void USaveGameViewerWidget::show_create_result(profiles::CreateProfileResult result) {
    switch (result) {
        case profiles::CreateProfileResult::succeeded: {
            cancel_create_profile();
            return;
        }
        case profiles::CreateProfileResult::empty_name: {
            show_create_profile_error(
                NSLOCTEXT("SaveGameViewer", "EmptyProfileName", "Enter a profile name."));
            return;
        }
        case profiles::CreateProfileResult::name_too_long: {
            show_create_profile_error(FText::Format(
                NSLOCTEXT("SaveGameViewer", "LongProfileName", "Use no more than {0} characters."),
                FText::AsNumber(profiles::ProfileManager::max_profile_name_length)));
            return;
        }
        case profiles::CreateProfileResult::duplicate_name: {
            show_create_profile_error(NSLOCTEXT(
                "SaveGameViewer", "DuplicateProfileName", "That profile already exists."));
            return;
        }
        case profiles::CreateProfileResult::persistence_failed: {
            show_create_profile_error(
                NSLOCTEXT("SaveGameViewer", "CreateFailed", "The profile could not be saved."));
            return;
        }
    }
}

/* **************************************** */
// Data sources and selection
/* **************************************** */
void USaveGameViewerWidget::show_create_profile_error(FText const& error) {
    if (view_.IsValid()) {
        view_->show_create_error(error);
    }
}

void USaveGameViewerWidget::present_profiles(presentation::ProfileView const& browser) {
    view_state_ = FSaveGameViewState{};
    auto const profiles{browser.summaries};
    if (profiles.empty()) {
        show_empty_profiles();
        publish_all();
        return;
    }

    auto const profile_count{static_cast<int32>(profiles.size())};
    view_state_.profiles.Reserve(profile_count);
    for (auto const& profile : profiles) {
        auto const active{profile.profile_id == browser.active_profile_id};
        auto const marker{active ? TEXT("ACTIVE // ") : TEXT("")};
        view_state_.profiles.Add(FSaveProfileViewRow{
            .profile_id = to_fstring(profile.profile_id),
            .text = FText::FromString(marker + to_fstring(profile.display_name).ToUpper()),
            .active = active,
        });
    }

    view_state_.archive_status =
        FText::Format(NSLOCTEXT("SaveGameViewer", "ArchiveStatus", "{0} SERVICE RECORDS AVAILABLE"),
                      FText::AsNumber(profile_count));

    for (int32 index{}; index < profile_count; ++index) {
        if (profiles[index].profile_id == unreal_endpoint().menu().selected_profile) {
            view_state_.selected_profile_index = index;
            show_profile(profiles[index], browser.active_profile_id);
            break;
        }
    }
    rebuild_outcomes(browser.report.value_or(presentation::ProfileReportView{}));
    publish_all();
}

void USaveGameViewerWidget::rebuild_outcomes(presentation::ProfileReportView const& report) {
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

    for (int32 index{}; index < outcome_count; ++index) {
        if (report.outcomes[index].outcome_id == unreal_endpoint().menu().selected_outcome) {
            view_state_.selected_outcome_index = index;
            show_outcome(report.outcomes[index]);
            break;
        }
    }
}

void USaveGameViewerWidget::show_profile(profiles::ProfileSummary const& profile,
                                         std::string_view const active_profile_id) {
    auto const active{profile.profile_id == active_profile_id};
    view_state_.profile_name = FText::FromString(to_fstring(profile.display_name));
    view_state_.profile_status =
        active ? NSLOCTEXT("SaveGameViewer", "ActiveProfile", "ACTIVE SERVICE RECORD")
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
    view_state_.can_activate = !active;
}

void USaveGameViewerWidget::show_outcome(profiles::LevelOutcomeSummary const& outcome) {
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

} // namespace ioj
