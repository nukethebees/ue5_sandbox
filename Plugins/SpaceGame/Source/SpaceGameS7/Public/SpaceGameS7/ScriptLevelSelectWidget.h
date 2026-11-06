#include <ioj/levels/launch_options.h>
#pragma once
#include <ioj/levels/authoring/level_catalog_category.h>
#include <ioj/profiles/completion_indicator_state.h>
#include <SpaceGame/ui/main_menu/LevelSelectWidget.h>
#include <SpaceGamePresentation/ui/style/GameUiStyle.h>
#include <SpaceGameS7/level_script_catalog.h>

#include "ScriptLevelSelectWidget.generated.h"

namespace ioj {
class UGameSubsystem;
}

namespace ioj::s7 {
using ioj::levels::authoring::FCampaignScriptEntry;
using ioj::levels::authoring::FLevelScriptEntry;
using ioj::levels::authoring::LevelCatalogCategory;

class SScriptLevelSelectView;

struct FLevelSelectViewRow {
    FText text{};
    bool heading{};
    profiles::CompletionIndicatorState indicator_state{
        profiles::CompletionIndicatorState::Incomplete};
};

struct FLevelSelectViewState {
    TArray<FLevelSelectViewRow> rows{};
    FText title{};
    FText status{};
    FText description{};
    FText filename{};
    FText details{};
    FText script{};
    FText launch_mode_status{};
    LevelCatalogCategory category{LevelCatalogCategory::Mission};
    TOptional<double> playerless_time_scale{};
    int32 selected_button_index{INDEX_NONE};
    bool can_launch{};
};

UCLASS()
class SPACEGAMES7_API UScriptLevelSelectWidget : public ULevelSelectWidget {
    GENERATED_BODY()
  public:
    UScriptLevelSelectWidget();

    [[nodiscard]] auto get_selected_level_id() const noexcept -> FName {
        return selected_level_id_;
    }
    [[nodiscard]] auto can_launch_selected_level() const noexcept -> bool {
        return view_state_.can_launch &&
               (active_category_ == LevelCatalogCategory::Mission ||
                (get_active_playerless_time_scale().IsSet() && battle_duration_valid_));
    }
    [[nodiscard]] auto get_active_category() const noexcept -> LevelCatalogCategory {
        return active_category_;
    }
    void refresh() override;
    void focus_primary_action() override;
  protected:
    void NativeOnInitialized() override;
    auto RebuildWidget() -> TSharedRef<SWidget> override;
    void ReleaseSlateResources(bool release_children) override;
    auto NativeOnFocusReceived(FGeometry const& geometry, FFocusEvent const& focus_event)
        -> FReply override;
  private:
    void launch_selected_level(::ioj::levels::LaunchOptions options);
    void refresh_levels();
    void rebuild_catalog(FName focus_level_id);
    void select_category(LevelCatalogCategory category);
    void select_level(int32 button_index);
    void set_battle_time_scale(TOptional<double> time_scale);
    void set_battle_duration(TOptional<double> duration, bool valid);
    void set_battle_simulation_only(bool simulation_only);
    void apply_level_selection(int32 button_index);

    [[nodiscard]] auto get_active_playerless_time_scale() const noexcept
        -> TOptional<double> const& {
        return active_category_ == LevelCatalogCategory::Benchmark ? benchmark_time_scale_
                                                                   : battle_viewer_time_scale_;
    }

    void handle_launch();
    void publish_view();
    void publish_catalog();

    TArray<FLevelScriptEntry> entries_{};
    TArray<FCampaignScriptEntry> campaigns_{};
    TArray<int32> level_entry_indices_{};
    FString catalog_directory_{};
    FString catalog_error_{};
    FLevelSelectViewState view_state_{};
    TSharedPtr<SScriptLevelSelectView> view_{};
    FGameUiStyle fallback_style_{};
    UPROPERTY(Transient)
    UGameSubsystem* game_{nullptr};
    int32 selected_entry_index_{INDEX_NONE};
    FName selected_level_id_{NAME_None};
    LevelCatalogCategory active_category_{LevelCatalogCategory::Mission};
    TOptional<double> battle_viewer_time_scale_{1.0};
    TOptional<double> benchmark_time_scale_{10.0};
    TOptional<double> battle_duration_{levels::launch::placeholder_duration_seconds};
    bool battle_duration_valid_{true};
    bool battle_simulation_only_{};
};
}
