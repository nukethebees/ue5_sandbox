#include <SpaceGameS7/ScriptLevelSelectWidget.h>

#include "SScriptLevelSelectView.h"

#include <SpaceGame/levels/LevelUnlock.h>
#include <SpaceGame/persistence/SpaceSaveSubsystem.h>
#include <SpaceGame/support/logging/SandboxLogCategories.h>
#include <SpaceGame/system/GameSubsystem.h>
#include <SpaceGame/ui/style/SpaceGameUiTheme.h>

#include <Engine/GameInstance.h>
#include <IPlatformCrypto.h>
#include <Kismet/GameplayStatics.h>
#include <Misc/FileHelper.h>

namespace ml::s7 {
auto format_level_row_title(FString title, ELevelRowState const state) -> FString {
    switch (state) {
        case ELevelRowState::Invalid:
            return TEXT("! ") + title;
        case ELevelRowState::Locked:
            return TEXT("\U0001F512 ") + title;
        case ELevelRowState::Unlocked:
            return TEXT("\u25CB ") + title;
        case ELevelRowState::Completed:
            return TEXT("\u2713 ") + title;
    }
    checkNoEntry();
    return title;
}

namespace {
auto progress_label(FLevelDefinition const& definition,
                    ml::ioj::FLevelProgressSummary const& progress) -> FString {
    if (!definition.mission.IsSet()) {
        return TEXT("No objective");
    }

    switch (progress.state) {
        case ml::ioj::ELevelProgressState::Completed:
            return TEXT("Completed");
        case ml::ioj::ELevelProgressState::Attempted:
            return TEXT("Attempted");
        case ml::ioj::ELevelProgressState::NotAttempted:
            return TEXT("Not attempted");
    }
    checkNoEntry();
    return {};
}

auto level_details(FLevelScriptEntry const& entry, ml::ioj::FLevelProgressSummary const& progress)
    -> FString {
    check(entry.definition.IsSet());
    auto const& definition{entry.definition.GetValue()};
    auto details{
        FString::Printf(TEXT("LEVEL ID  //  %s\nTEAMS  //  %d    ENTITIES  //  %d    "
                             "PLAYER ASSET  //  %s\nPROGRESS  //  %s"),
                        *definition.metadata.id.value.ToString(),
                        definition.teams.Num(),
                        definition.entities.num(),
                        definition.player_entity_id.is_set() ? TEXT("ASSIGNED") : TEXT("NONE"),
                        *progress_label(definition, progress).ToUpper())};
    if (!definition.mission.IsSet() || progress.attempt_count == 0) {
        return details;
    }

    details += FString::Printf(TEXT("\nATTEMPTS  //  %d    COMPLETIONS  //  %d    "
                                    "BEST KILLS  //  %d"),
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
        if (entry && entry.definition->metadata.id == id) {
            return FText::FromString(entry.display_title);
        }
    }
    return FText::FromName(id.value);
}

auto make_unlock_evaluator(TArray<FLevelScriptEntry> const& entries,
                           USpaceSaveSubsystem const* const save_subsystem)
    -> FLevelUnlockEvaluator {
    return FLevelUnlockEvaluator{
        [save_subsystem](FLevelId const id) {
            return IsValid(save_subsystem) && save_subsystem->is_level_completed(id);
        },
        [&entries](FLevelId const id) { return find_level_title(entries, id); },
        IsValid(save_subsystem) && save_subsystem->unlock_all_missions(),
    };
}

auto description_with_requirements(FLevelScriptEntry const& entry,
                                   FLevelUnlockStatus const& unlock_status) -> FString {
    auto description{entry.description};
    if (unlock_status.criteria.IsEmpty()) {
        return description;
    }

    if (!description.IsEmpty()) {
        description += TEXT("\n\n");
    }
    description += TEXT("DEPLOYMENT REQUIREMENTS");
    for (auto const& criterion : unlock_status.criteria) {
        description += FString::Printf(TEXT("\n%s %s"),
                                       criterion.satisfied ? TEXT("\u2713") : TEXT("\u2610"),
                                       *criterion.description.ToString());
    }
    return description;
}

auto row_state(FLevelUnlockStatus const& unlock_status,
               ml::ioj::FLevelProgressSummary const& progress) -> ELevelRowState {
    if (!unlock_status.unlocked) {
        return ELevelRowState::Locked;
    }
    return progress.state == ml::ioj::ELevelProgressState::Completed ? ELevelRowState::Completed
                                                                     : ELevelRowState::Unlocked;
}

auto entry_matches_category(FLevelScriptEntry const& entry, ELevelCatalogCategory const category)
    -> bool {
    if (!entry) {
        return category == ELevelCatalogCategory::Mission;
    }
    return catalog_category(entry.definition.GetValue()) == category;
}

auto source_sha256(FString const& path) -> FString {
    TArray<uint8> bytes;
    if (!FFileHelper::LoadFileToArray(bytes, *path)) {
        return {};
    }
    auto context{IPlatformCrypto::Get().CreateContext()};
    TArray<uint8> hash;
    if (!context || !context->CalcSHA256(bytes, hash)) {
        return {};
    }
    return BytesToHex(hash.GetData(), hash.Num()).ToLower();
}
} // namespace

UScriptLevelSelectWidget::UScriptLevelSelectWidget() = default;

void UScriptLevelSelectWidget::NativeOnInitialized() {
    Super::NativeOnInitialized();

    auto* const game_instance{GetGameInstance()};
    game_ =
        IsValid(game_instance) ? game_instance->GetSubsystem<ml::ioj::UGameSubsystem>() : nullptr;
    if (!IsValid(game_)) {
        UE_LOG(LogSandboxUI,
               Warning,
               TEXT("UScriptLevelSelectWidget: Game subsystem is unavailable; using the "
                    "default UI theme."));
        auto const* const default_theme{GetDefault<ml::ioj::USpaceGameUiTheme>()};
        check(IsValid(default_theme));
        fallback_style_ = default_theme->compile();
    }
}

auto UScriptLevelSelectWidget::RebuildWidget() -> TSharedRef<SWidget> {
    auto const* const style{IsValid(game_) ? &game_->get_ui_style() : &fallback_style_};
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
            .OnBattleDetailedTimingChanged(
                FOnBattleBoolChanged::CreateUObject(this, &ThisClass::set_battle_detailed_timing))
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
    active_category_ = ELevelCatalogCategory::Mission;
    entries_.Reset();
    campaigns_.Reset();
    catalog_directory_.Reset();
    catalog_error_.Reset();

    auto catalog{discover_level_scripts()};
    entries_ = MoveTemp(catalog.entries);
    campaigns_ = MoveTemp(catalog.campaigns);
    catalog_directory_ = MoveTemp(catalog.directory);
    catalog_error_ = MoveTemp(catalog.error);

    if (IsValid(game_) && game_->has_level_launch_error()) {
        auto launch_error{game_->take_level_launch_error()};
        catalog_error_ = catalog_error_.IsEmpty()
                           ? MoveTemp(launch_error)
                           : catalog_error_ + TEXT("\n") + MoveTemp(launch_error);
    }

    rebuild_catalog(get_preferred_level_id());
}

void UScriptLevelSelectWidget::rebuild_catalog(FName const focus_level_id) {
    level_entry_indices_.Reset();
    selected_entry_index_ = INDEX_NONE;
    selected_level_id_ = NAME_None;
    view_state_ = FLevelSelectViewState{};
    view_state_.category = active_category_;
    if (active_category_ == ELevelCatalogCategory::Mission) {
        view_state_.title = NSLOCTEXT("LevelSelect", "SelectLevel", "SELECT AN OPERATION");
        view_state_.description = NSLOCTEXT(
            "LevelSelect", "SelectLevelDetail", "Select a mission record to review its directive.");
    } else {
        view_state_.title = NSLOCTEXT("LevelSelect", "SelectBattle", "SELECT A BATTLE");
        view_state_.description = NSLOCTEXT(
            "LevelSelect", "SelectBattleDetail", "Select a scenario to review its definition.");
    }

    auto* const game_instance{GetGameInstance()};
    auto* const save_subsystem{
        IsValid(game_instance) ? game_instance->GetSubsystem<USpaceSaveSubsystem>() : nullptr};
    auto const evaluator{make_unlock_evaluator(entries_, save_subsystem)};
    auto const entry_indices{[this] {
        TMap<FLevelId, int32> result;
        auto const count{entries_.Num()};
        for (int32 index{}; index < count; ++index) {
            if (entries_[index]) {
                result.Add(entries_[index].definition->metadata.id, index);
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
    auto add_level = [this,
                      &evaluator,
                      save_subsystem,
                      focus_level_id,
                      &preferred_button_index,
                      &visible_entry_count](int32 const entry_index) {
        auto const& entry{entries_[entry_index]};
        auto state{ELevelRowState::Invalid};
        if (entry) {
            auto const id{entry.definition->metadata.id};
            auto const progress{IsValid(save_subsystem) ? save_subsystem->get_level_progress(id)
                                                        : ml::ioj::FLevelProgressSummary{}};
            state = row_state(evaluator.evaluate(entry.definition.GetValue()), progress);
            if (preferred_button_index == INDEX_NONE && !focus_level_id.IsNone() &&
                id.value == focus_level_id) {
                preferred_button_index = level_entry_indices_.Num();
            }
        }

        auto const row_title{format_level_row_title(entry.display_title, state)};
        view_state_.rows.Add(FLevelSelectViewRow{FText::FromString(row_title), false});
        level_entry_indices_.Add(entry_index);
        ++visible_entry_count;
    };

    TSet<FLevelId> grouped_levels;
    for (auto const& campaign : campaigns_) {
        if (!campaign) {
            continue;
        }
        TArray<int32> visible_indices;
        for (auto const level_id : campaign.definition->level_ids) {
            auto const* const entry_index{entry_indices.Find(level_id)};
            check(entry_index);
            if (!entry_matches_category(entries_[*entry_index], active_category_)) {
                continue;
            }
            visible_indices.Add(*entry_index);
        }
        if (visible_indices.IsEmpty()) {
            continue;
        }
        add_header(campaign.definition->title);
        ++visible_campaign_count;
        for (auto const entry_index : visible_indices) {
            add_level(entry_index);
            auto const level_id{entries_[entry_index].definition->metadata.id};
            grouped_levels.Add(level_id);
        }
    }

    bool has_other_levels{};
    auto const entry_count{entries_.Num()};
    for (int32 index{}; index < entry_count; ++index) {
        if (!entries_[index] || !entry_matches_category(entries_[index], active_category_) ||
            grouped_levels.Contains(entries_[index].definition->metadata.id)) {
            continue;
        }
        if (!has_other_levels) {
            add_header(active_category_ == ELevelCatalogCategory::Mission
                           ? TEXT("Other Operations")
                           : TEXT("Other Scenarios"));
            has_other_levels = true;
        }
        add_level(index);
    }

    if (active_category_ == ELevelCatalogCategory::Mission) {
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
        status = active_category_ == ELevelCatalogCategory::Mission
                   ? FString::Printf(TEXT("No mission scripts found in %s"), *catalog_directory_)
                   : FString::Printf(TEXT("No Battle Viewer scenarios found in %s"),
                                     *catalog_directory_);
    } else if (status.IsEmpty()) {
        status = active_category_ == ELevelCatalogCategory::Mission
                   ? FString::Printf(TEXT("%d mission records available across %d campaigns."),
                                     visible_entry_count,
                                     visible_campaign_count)
                   : FString::Printf(TEXT("%d Battle Viewer scenarios available across %d "
                                          "campaigns."),
                                     visible_entry_count,
                                     visible_campaign_count);
    }
    view_state_.status = FText::FromString(status);

    if (!level_entry_indices_.IsEmpty()) {
        apply_level_selection(preferred_button_index != INDEX_NONE ? preferred_button_index : 0);
    }
    publish_catalog();
}

void UScriptLevelSelectWidget::select_category(ELevelCatalogCategory const category) {
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
    battle_time_scale_ = MoveTemp(time_scale);
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
    selected_level_id_ =
        entries_[entry_index] ? entries_[entry_index].definition->metadata.id.value : NAME_None;
    view_state_.selected_button_index = button_index;

    auto const& entry{entries_[entry_index]};
    view_state_.filename = FText::FromString(entry.filename.ToUpper());
    view_state_.title = FText::FromString(entry.display_title);
    view_state_.script = FText::FromString(entry.source_text);
    view_state_.launch_mode_status = FText::GetEmpty();
    view_state_.can_launch = false;

    if (entry) {
        auto* const game_instance{GetGameInstance()};
        auto* const save_subsystem{
            IsValid(game_instance) ? game_instance->GetSubsystem<USpaceSaveSubsystem>() : nullptr};
        auto const progress{IsValid(save_subsystem)
                                ? save_subsystem->get_level_progress(entry.definition->metadata.id)
                                : ml::ioj::FLevelProgressSummary{}};
        auto const evaluator{make_unlock_evaluator(entries_, save_subsystem)};
        auto const unlock_status{evaluator.evaluate(entry.definition.GetValue())};
        view_state_.description =
            FText::FromString(description_with_requirements(entry, unlock_status));
        view_state_.details = FText::FromString(level_details(entry, progress));

        if (unlock_status.unlocked) {
            view_state_.can_launch = true;
            if (IsValid(save_subsystem) && save_subsystem->start_levels_paused()) {
                view_state_.launch_mode_status =
                    NSLOCTEXT("LevelSelect", "DebugStartPaused", "DEBUG // START PAUSED");
            }
            view_state_.status =
                FText::FromString(progress_label(entry.definition.GetValue(), progress).ToUpper() +
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
    auto const time_scale{active_category_ == ELevelCatalogCategory::BattleViewer
                              ? battle_time_scale_
                              : TOptional<double>{ml::ioj::level_launch::default_time_scale}};
    if (!time_scale.IsSet()) {
        return;
    }

    auto* const game_instance{GetGameInstance()};
    auto const* const save_subsystem{
        IsValid(game_instance) ? game_instance->GetSubsystem<USpaceSaveSubsystem>() : nullptr};
    auto const launch_mode{IsValid(save_subsystem) && save_subsystem->start_levels_paused()
                               ? ml::ioj::ELevelLaunchMode::Paused
                               : ml::ioj::ELevelLaunchMode::Running};
    ml::ioj::FLevelLaunchOptions options{
        .launch_mode = launch_mode,
        .requested_time_scale = time_scale.GetValue(),
    };
    if (active_category_ == ELevelCatalogCategory::BattleViewer) {
        options.presentation_mode = battle_simulation_only_
                                      ? ml::ioj::ELevelPresentationMode::SimulationOnly
                                      : ml::ioj::ELevelPresentationMode::Visual;
        options.simulated_duration_seconds = battle_duration_;
        options.stop_when_battle_resolved = true;
        options.detailed_timing = battle_detailed_timing_;
        options.results_navigation = ml::ioj::ELevelResultsNavigation::Telemetry;
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

void UScriptLevelSelectWidget::set_battle_detailed_timing(bool const enabled) {
    battle_detailed_timing_ = enabled;
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

void UScriptLevelSelectWidget::launch_selected_level(ml::ioj::FLevelLaunchOptions options) {
    if (!entries_.IsValidIndex(selected_entry_index_) || !entries_[selected_entry_index_]) {
        return;
    }

    auto* const game_instance{GetGameInstance()};
    auto* const save_subsystem{
        IsValid(game_instance) ? game_instance->GetSubsystem<USpaceSaveSubsystem>() : nullptr};
    auto const evaluator{make_unlock_evaluator(entries_, save_subsystem)};
    auto const& selected_entry{entries_[selected_entry_index_]};
    if (!evaluator.evaluate(selected_entry.definition.GetValue()).unlocked) {
        view_state_.can_launch = false;
        view_state_.status =
            NSLOCTEXT("LevelSelect", "Locked", "LOCKED  //  DEPLOYMENT REQUIREMENTS INCOMPLETE");
        publish_view();
        return;
    }

    if (!IsValid(game_)) {
        view_state_.status =
            NSLOCTEXT("LevelSelect", "SubsystemUnavailable", "MISSION CONTROL IS UNAVAILABLE");
        publish_view();
        return;
    }

    auto& entry{entries_[selected_entry_index_]};
    auto definition{MoveTemp(entry.definition.GetValue())};
    entry.definition.Reset();
    game_->set_pending_level(MoveTemp(definition), entry.path, source_sha256(entry.path), options);
    view_state_.can_launch = false;
    if (options.launch_mode == ml::ioj::ELevelLaunchMode::Paused) {
        view_state_.status =
            NSLOCTEXT("LevelSelect", "StagingPaused", "STAGING LEVEL IN PAUSED STATE...");
    } else if (active_category_ == ELevelCatalogCategory::BattleViewer) {
        view_state_.status =
            NSLOCTEXT("LevelSelect", "OpeningBattleViewer", "OPENING BATTLE VIEWER...");
    } else {
        view_state_.status = NSLOCTEXT("LevelSelect", "Deploying", "DEPLOYING MISSION...");
    }
    publish_view();
    UGameplayStatics::OpenLevel(this, FName{TEXT("/SpaceGame/Levels/GameRuntime")});
}
} // namespace ml::s7
