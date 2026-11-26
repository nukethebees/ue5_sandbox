#include <SpaceGameS7/ScriptLevelSelectWidget.h>

#include "SScriptLevelSelectView.h"
#include <ioj/levels/launch_options.h>
#include <SpaceGame/levels/native_level_definition_conversion.h>
#include <SpaceGame/UnrealMessages.h>
#include <SpaceGamePresentation/audio/GameAudio.h>
#include <SpaceGamePresentation/support/logging/PresentationLogCategories.h>
#include <SpaceGamePresentation/ui/style/GameUiStyleSubsystem.h>
#include <SpaceGamePresentation/ui/style/SpaceGameUiTheme.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <SandboxCoreEngine/strings.h>

#include <Engine/GameInstance.h>
#include <Kismet/GameplayStatics.h>
#include <Misc/Paths.h>

namespace ioj::s7 {
using ioj::levels::authoring::FLevelScriptEntry;
using ioj::levels::authoring::LevelCatalogCategory;

namespace {
auto progress_label(profiles::LevelProgressSummary const& progress) -> FString {
    switch (progress.state()) {
        case profiles::LevelProgressState::Completed:
            return TEXT("Completed");
        case profiles::LevelProgressState::Attempted:
            return TEXT("Attempted");
        case profiles::LevelProgressState::NotAttempted:
            return TEXT("Not attempted");
    }

    checkNoEntry();
    return {};
}

auto level_details(FLevelScriptEntry const& entry, profiles::LevelProgressSummary const& progress)
    -> FString {
    auto details{FString::Printf(TEXT("LEVEL ID  //  %s\nTEAMS  //  %d    ENTITIES  //  %d    "
                                      "PLAYER ASSET  //  %s\nPROGRESS  //  %s"),
                                 *to_fstring(entry.id.value),
                                 entry.team_count,
                                 entry.entity_count,
                                 entry.has_player ? TEXT("ASSIGNED") : TEXT("NONE"),
                                 *progress_label(progress).ToUpper())};
    auto const par_time{entry.par_time};
    if (par_time.has_value()) {
        auto const par_achieved{entry.indicator == profiles::CompletionIndicatorState::ParAchieved};
        details += FString::Printf(TEXT("\nPAR TIME  //  %.1f S    PAR STATUS  //  %s"),
                                   *par_time,
                                   par_achieved ? TEXT("ACHIEVED") : TEXT("NOT ACHIEVED"));
    }

    if (progress.attempt_count == 0) {
        return details;
    }

    details += FString::Printf(TEXT("\nATTEMPTS  //  %d    COMPLETIONS  //  %d    "
                                    "BEST KILLS  //  %u"),
                               progress.attempt_count,
                               progress.completion_count,
                               progress.best_kills);
    if (progress.best_completion_time_seconds >= 0.0f) {
        details += FString::Printf(TEXT("    BEST TIME  //  %.1f S"),
                                   progress.best_completion_time_seconds);
    }

    return details;
}

auto find_level_title(TArray<FLevelScriptEntry> const& entries, FLevelId const id) -> FText {
    for (auto const& entry : entries) {
        if (entry && levels::authoring::to_unreal(entry.id) == id) {
            return FText::FromString(entry.display_title);
        }
    }

    return FText::FromName(id.value);
}

auto description_with_requirements(FLevelScriptEntry const& entry,
                                   TArray<FLevelScriptEntry> const& entries) -> FString {
    auto description{entry.description};
    if (entry.prerequisites.criteria.empty()) {
        return description;
    }

    if (!description.IsEmpty()) {
        description += TEXT("\n\n");
    }
    description += TEXT("DEPLOYMENT REQUIREMENTS");
    for (auto const& criterion : entry.prerequisites.criteria) {
        description += FString::Printf(
            TEXT("\n%s %s"),
            criterion.satisfied ? TEXT("\u2713") : TEXT("\u2610"),
            *FText::Format(
                 NSLOCTEXT("LevelUnlock", "CompleteLevel", "Complete {0}"),
                 find_level_title(entries, levels::authoring::to_unreal(criterion.level_id)))
                 .ToString());
    }

    return description;
}

auto entry_matches_category(FLevelScriptEntry const& entry, LevelCatalogCategory const category)
    -> bool {
    if (!entry) {
        return category == LevelCatalogCategory::Mission;
    }

    auto const content_category{entry.category};
    return category == LevelCatalogCategory::Mission
             ? content_category == LevelCatalogCategory::Mission
             : content_category == LevelCatalogCategory::BattleViewer;
}

} // namespace

UScriptLevelSelectWidget::UScriptLevelSelectWidget() = default;

void UScriptLevelSelectWidget::NativeOnInitialized() {
    Super::NativeOnInitialized();

    auto* const game_instance{GetGameInstance()};
    style_ =
        IsValid(game_instance) ? game_instance->GetSubsystem<UGameUiStyleSubsystem>() : nullptr;
    if (!IsValid(style_)) {
        UE_LOG(LogSandboxUI,
               Warning,
               TEXT("UScriptLevelSelectWidget: Game subsystem is unavailable; using the "
                    "default UI theme."));
        auto const* const default_theme{GetDefault<USpaceGameUiTheme>()};
        check(IsValid(default_theme));
        fallback_style_ = default_theme->compile();
    }
}

auto UScriptLevelSelectWidget::RebuildWidget() -> TSharedRef<SWidget> {
    auto const* const style{IsValid(style_) ? &style_->get_ui_style() : &fallback_style_};
    auto result{
        SAssignNew(view_, SScriptLevelSelectView)
            .Style(style)
            .OnLevelSelected(FOnLevelRowSelected::CreateUObject(this, &ThisClass::select_level))
            .OnCategorySelected(
                FOnLevelCategorySelected::CreateUObject(this, &ThisClass::select_category))
            .OnBattleSpeedChanged(
                FOnBattleSpeedChanged::CreateUObject(this, &ThisClass::set_battle_time_scale))
            .OnBattleDurationChanged(
                FOnBattleDurationChanged::CreateUObject(this, &ThisClass::set_battle_duration))
            .OnBattleSimulationOnlyChanged(
                FOnBattleBoolChanged::CreateUObject(this, &ThisClass::set_battle_simulation_only))
            .OnLaunch(FSimpleDelegate::CreateUObject(this, &ThisClass::handle_launch))};
    refresh_levels();
    return result;
}

void UScriptLevelSelectWidget::refresh() {
    refresh_levels();
}

void UScriptLevelSelectWidget::focus_primary_action() {
    if (view_.IsValid()) {
        view_->focus_selected_level();
    }
}

void UScriptLevelSelectWidget::ReleaseSlateResources(bool const release_children) {
    Super::ReleaseSlateResources(release_children);
    view_.Reset();
}

auto UScriptLevelSelectWidget::NativeOnFocusReceived(FGeometry const& geometry,
                                                     FFocusEvent const& focus_event) -> FReply {
    static_cast<void>(geometry);
    static_cast<void>(focus_event);
    if (view_.IsValid()) {
        view_->focus_selected_level();
    }

    return FReply::Handled();
}

void UScriptLevelSelectWidget::refresh_levels() {
    active_category_ = LevelCatalogCategory::Mission;
    entries_.Reset();
    campaigns_.Reset();
    catalog_directory_.Reset();
    catalog_error_.Reset();

    request_mission_catalog(*this);
}

void UScriptLevelSelectWidget::present_catalog(presentation::MissionCatalogView const& catalog) {
    entries_.Reset(static_cast<int32>(catalog.missions.size()));
    for (auto const& row : catalog.missions) {
        auto& entry{entries_.Emplace_GetRef()};
        entry.path = to_fstring(row.path);
        entry.filename = FPaths::GetCleanFilename(entry.path);
        entry.source_text = to_fstring(row.script);
        if (!row.metadata) {
            entry.display_title = FPaths::GetBaseFilename(entry.path);
            entry.error = to_fstring(row.error);
            continue;
        }
        entry.display_title = to_fstring(row.metadata->title);
        entry.description = to_fstring(row.metadata->description);
        entry.id = row.metadata->id;
        entry.par_time = row.metadata->par_time_seconds;
        entry.category = row.category;
        entry.team_count = static_cast<int32>(row.team_count);
        entry.entity_count = static_cast<int32>(row.entity_count);
        entry.has_player = row.has_player;
        entry.progress = row.progress;
        entry.indicator = row.indicator;
        entry.prerequisites = row.prerequisites;
        entry.unlocked = row.unlocked;
    }
    campaigns_.Reset(static_cast<int32>(catalog.campaigns.size()));
    for (auto const* campaign : catalog.campaigns) {
        campaigns_.Add(*campaign);
    }
    catalog_directory_ = to_fstring(catalog.directory);
    catalog_error_ = to_fstring(catalog.error);

    if (auto& destination{menu_selection()}; destination && !destination->launch_error.empty()) {
        auto launch_error{to_fstring(destination->launch_error)};
        catalog_error_ = catalog_error_.IsEmpty()
                           ? MoveTemp(launch_error)
                           : catalog_error_ + TEXT("\n") + MoveTemp(launch_error);
    }

    menu_selection().reset();
    rebuild_catalog(get_preferred_level_id());
}

void UScriptLevelSelectWidget::rebuild_catalog(FName const focus_level_id) {
    level_entry_indices_.Reset();
    selected_entry_index_ = INDEX_NONE;
    selected_level_id_ = NAME_None;
    view_state_ = FLevelSelectViewState{};
    view_state_.category = active_category_;
    if (active_category_ != LevelCatalogCategory::Mission) {
        view_state_.playerless_time_scale = get_active_playerless_time_scale();
    }

    if (active_category_ == LevelCatalogCategory::Mission) {
        view_state_.title = NSLOCTEXT("LevelSelect", "SelectLevel", "SELECT AN OPERATION");
        view_state_.description = NSLOCTEXT(
            "LevelSelect", "SelectLevelDetail", "Select a mission record to review its directive.");
    } else if (active_category_ == LevelCatalogCategory::Benchmark) {
        view_state_.title = NSLOCTEXT("LevelSelect", "SelectBenchmark", "SELECT A BENCHMARK");
        view_state_.description = NSLOCTEXT(
            "LevelSelect", "SelectBenchmarkDetail", "Select a scenario to run as a benchmark.");
    } else {
        view_state_.title = NSLOCTEXT("LevelSelect", "SelectBattle", "SELECT A BATTLE");
        view_state_.description = NSLOCTEXT(
            "LevelSelect", "SelectBattleDetail", "Select a scenario to review its definition.");
    }

    auto* const game_instance{GetGameInstance()};
    auto const entry_indices{[this] {
        TMap<FLevelId, int32> result;
        auto const count{entries_.Num()};
        for (int32 index{}; index < count; ++index) {
            if (entries_[index]) {
                result.Add(levels::authoring::to_unreal(entries_[index].id), index);
            }
        }

        return result;
    }()};

    int32 preferred_button_index{INDEX_NONE};
    int32 visible_entry_count{};
    int32 visible_campaign_count{};
    auto add_header = [this](FString const& label) {
        view_state_.rows.Add(FLevelSelectViewRow{FText::FromString(label.ToUpper()), true});
    };
    auto add_level = [this, focus_level_id, &preferred_button_index, &visible_entry_count](
                         int32 const entry_index) {
        auto const& entry{entries_[entry_index]};
        auto indicator_state{profiles::CompletionIndicatorState::Incomplete};
        if (entry) {
            indicator_state = entry.indicator;
            if (preferred_button_index == INDEX_NONE && !focus_level_id.IsNone() &&
                levels::authoring::to_unreal(entry.id).value == focus_level_id) {
                preferred_button_index = level_entry_indices_.Num();
            }
        }

        view_state_.rows.Add(
            FLevelSelectViewRow{FText::FromString(entry.display_title), false, indicator_state});
        level_entry_indices_.Add(entry_index);
        ++visible_entry_count;
    };

    TSet<FLevelId> grouped_levels;
    for (auto const& campaign : campaigns_) {
        TArray<int32> visible_indices;
        for (auto const level_id : campaign.level_ids) {
            auto const* const entry_index{
                entry_indices.Find(levels::authoring::to_unreal(level_id))};
            check(entry_index);
            if (!entry_matches_category(entries_[*entry_index], active_category_)) {
                continue;
            }

            visible_indices.Add(*entry_index);
        }

        if (visible_indices.IsEmpty()) {
            continue;
        }

        add_header(to_fstring(campaign.title));
        ++visible_campaign_count;
        for (auto const entry_index : visible_indices) {
            add_level(entry_index);
            auto const level_id{levels::authoring::to_unreal(entries_[entry_index].id)};
            grouped_levels.Add(level_id);
        }
    }

    bool has_other_levels{};
    auto const entry_count{entries_.Num()};
    for (int32 index{}; index < entry_count; ++index) {
        if (!entries_[index] || !entry_matches_category(entries_[index], active_category_) ||
            grouped_levels.Contains(levels::authoring::to_unreal(entries_[index].id))) {
            continue;
        }

        if (!has_other_levels) {
            add_header(active_category_ == LevelCatalogCategory::Mission ? TEXT("Other Operations")
                                                                         : TEXT("Other Scenarios"));
            has_other_levels = true;
        }

        add_level(index);
    }

    if (active_category_ == LevelCatalogCategory::Mission) {
        bool has_invalid_levels{};
        for (int32 index{}; index < entry_count; ++index) {
            if (entries_[index]) {
                continue;
            }

            if (!has_invalid_levels) {
                add_header(TEXT("Invalid Mission Records"));
                has_invalid_levels = true;
            }

            add_level(index);
        }
    }

    auto status{catalog_error_};
    if (status.IsEmpty() && visible_entry_count == 0) {
        if (active_category_ == LevelCatalogCategory::Mission) {
            status = FString::Printf(TEXT("No mission scripts found in %s"), *catalog_directory_);
        } else if (active_category_ == LevelCatalogCategory::Benchmark) {
            status =
                FString::Printf(TEXT("No benchmark scenarios found in %s"), *catalog_directory_);
        } else {
            status = FString::Printf(TEXT("No Battle Viewer scenarios found in %s"),
                                     *catalog_directory_);
        }
    } else if (status.IsEmpty()) {
        if (active_category_ == LevelCatalogCategory::Mission) {
            status = FString::Printf(TEXT("%d mission records available across %d campaigns."),
                                     visible_entry_count,
                                     visible_campaign_count);
        } else if (active_category_ == LevelCatalogCategory::Benchmark) {
            status = FString::Printf(TEXT("%d benchmark scenarios available across %d campaigns."),
                                     visible_entry_count,
                                     visible_campaign_count);
        } else {
            status = FString::Printf(TEXT("%d Battle Viewer scenarios available across %d "
                                          "campaigns."),
                                     visible_entry_count,
                                     visible_campaign_count);
        }
    }

    view_state_.status = FText::FromString(status);

    if (!level_entry_indices_.IsEmpty()) {
        apply_level_selection(preferred_button_index != INDEX_NONE ? preferred_button_index : 0);
    }

    publish_catalog();
}

void UScriptLevelSelectWidget::select_category(LevelCatalogCategory const category) {
    if (active_category_ == category) {
        return;
    }

    active_category_ = category;
    rebuild_catalog(NAME_None);
}

void UScriptLevelSelectWidget::select_level(int32 const button_index) {
    apply_level_selection(button_index);
    publish_view();
}

void UScriptLevelSelectWidget::set_battle_time_scale(TOptional<double> time_scale) {
    auto& active_time_scale{active_category_ == LevelCatalogCategory::Benchmark
                                ? benchmark_time_scale_
                                : battle_viewer_time_scale_};
    active_time_scale = MoveTemp(time_scale);
    view_state_.playerless_time_scale = active_time_scale;
}

void UScriptLevelSelectWidget::apply_level_selection(int32 const button_index) {
    if (!level_entry_indices_.IsValidIndex(button_index)) {
        return;
    }

    auto const entry_index{level_entry_indices_[button_index]};
    if (!entries_.IsValidIndex(entry_index)) {
        return;
    }

    selected_entry_index_ = entry_index;
    selected_level_id_ = entries_[entry_index]
                           ? levels::authoring::to_unreal(entries_[entry_index].id).value
                           : NAME_None;
    view_state_.selected_button_index = button_index;

    auto const& entry{entries_[entry_index]};
    view_state_.filename = FText::FromString(entry.filename.ToUpper());
    view_state_.title = FText::FromString(entry.display_title);
    view_state_.script = FText::FromString(entry.source_text);
    view_state_.launch_mode_status = FText::GetEmpty();
    view_state_.can_launch = false;

    if (entry) {
        auto* const game_instance{GetGameInstance()};
        auto const progress{entry.progress};
        view_state_.description = FText::FromString(description_with_requirements(entry, entries_));
        view_state_.details = FText::FromString(level_details(entry, progress));

        if (entry.unlocked) {
            view_state_.can_launch = true;
            if (game_instance && player_settings().application.start_levels_paused) {
                view_state_.launch_mode_status =
                    NSLOCTEXT("LevelSelect", "DebugStartPaused", "DEBUG // START PAUSED");
            }

            view_state_.status = FText::FromString(progress_label(progress).ToUpper() +
                                                   TEXT("  //  CLEARED FOR DEPLOYMENT"));
        } else {
            view_state_.status = NSLOCTEXT(
                "LevelSelect", "Locked", "LOCKED  //  DEPLOYMENT REQUIREMENTS INCOMPLETE");
        }
    } else {
        view_state_.description = FText::FromString(entry.description);
        view_state_.status = FText::FromString(entry.error);
        view_state_.details = FText::GetEmpty();
    }
}

void UScriptLevelSelectWidget::handle_launch() {
    auto const time_scale{active_category_ != LevelCatalogCategory::Mission
                              ? get_active_playerless_time_scale()
                              : TOptional<double>{levels::launch::default_time_scale}};
    if (!time_scale.IsSet()) {
        return;
    }

    ::ioj::levels::LaunchOptions options{
        .requested_time_scale = levels::launch::TimeScale::make(time_scale.GetValue()),
    };
    if (active_category_ != LevelCatalogCategory::Mission) {
        options.presentation_mode = battle_simulation_only_
                                      ? ::ioj::levels::PresentationMode::SimulationOnly
                                      : ::ioj::levels::PresentationMode::Visual;
        if (battle_duration_.IsSet()) {
            options.simulated_duration_seconds =
                PositiveFinite<double>::make(battle_duration_.GetValue());
        }
    }

    launch_selected_level(MoveTemp(options));
}

void UScriptLevelSelectWidget::set_battle_duration(TOptional<double> duration, bool const valid) {
    battle_duration_ = duration;
    battle_duration_valid_ = valid;
}

void UScriptLevelSelectWidget::set_battle_simulation_only(bool const simulation_only) {
    battle_simulation_only_ = simulation_only;
}

void UScriptLevelSelectWidget::publish_view() {
    if (view_.IsValid()) {
        view_->update_state(view_state_);
    }
}

void UScriptLevelSelectWidget::publish_catalog() {
    if (view_.IsValid()) {
        view_->replace_catalog(view_state_);
    }
}

void UScriptLevelSelectWidget::launch_selected_level(::ioj::levels::LaunchOptions options) {
    if (!entries_.IsValidIndex(selected_entry_index_) || !entries_[selected_entry_index_]) {
        return;
    }

    auto const& selected_entry{entries_[selected_entry_index_]};
    if (!selected_entry.unlocked) {
        view_state_.can_launch = false;
        view_state_.status =
            NSLOCTEXT("LevelSelect", "Locked", "LOCKED  //  DEPLOYMENT REQUIREMENTS INCOMPLETE");
        publish_view();
        return;
    }

    if (!IsValid(style_)) {
        view_state_.status =
            NSLOCTEXT("LevelSelect", "SubsystemUnavailable", "MISSION CONTROL IS UNAVAILABLE");
        publish_view();
        return;
    }

    options.control_context = active_category_ == LevelCatalogCategory::Mission
                                ? ::ioj::levels::ControlContext::Player
                            : active_category_ == LevelCatalogCategory::Benchmark
                                ? ::ioj::levels::ControlContext::Benchmark
                                : ::ioj::levels::ControlContext::Observer;
    request_mission({selected_entry.id, options});
    view_state_.can_launch = false;
    if (options.control_context == ::ioj::levels::ControlContext::Benchmark) {
        view_state_.status = NSLOCTEXT("LevelSelect", "StartingBenchmark", "STARTING BENCHMARK...");
    } else if (active_category_ == LevelCatalogCategory::BattleViewer) {
        view_state_.status =
            NSLOCTEXT("LevelSelect", "OpeningBattleViewer", "OPENING BATTLE VIEWER...");
    } else {
        view_state_.status = NSLOCTEXT("LevelSelect", "Deploying", "DEPLOYING MISSION...");
    }

    publish_view();
}
} // namespace ioj::s7
