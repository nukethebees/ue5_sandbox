#pragma once

#include <SpaceGame/ui/main_menu/LevelSelectWidget.h>
#include <SpaceGame/ui/style/GameUiStyle.h>
#include <SpaceGameS7/LevelScriptCatalog.h>

#include "ScriptLevelSelectWidget.generated.h"

namespace ml::ioj {
enum class ELevelLaunchMode : uint8;
class UGameSubsystem;
}

namespace ml::s7 {
class SScriptLevelSelectView;

enum class ELevelRowState : uint8 {
    Invalid,
    Locked,
    Unlocked,
    Completed,
};

SPACEGAMES7_API auto format_level_row_title(FString title, ELevelRowState state) -> FString;

struct FLevelSelectViewRow {
    FText text{};
    bool heading{};
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
    ELevelCatalogCategory category{ELevelCatalogCategory::Mission};
    int32 selected_button_index{INDEX_NONE};
    bool can_launch{};
};

UCLASS()
class SPACEGAMES7_API UScriptLevelSelectWidget : public ml::ioj::ULevelSelectWidget {
    GENERATED_BODY()
  public:
    UScriptLevelSelectWidget();

    [[nodiscard]] auto get_selected_level_id() const noexcept -> FName {
        return selected_level_id_;
    }
    [[nodiscard]] auto can_launch_selected_level() const noexcept -> bool {
        return view_state_.can_launch;
    }
    [[nodiscard]] auto get_active_category() const noexcept -> ELevelCatalogCategory {
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
    void launch_selected_level(ml::ioj::ELevelLaunchMode launch_mode);
    void refresh_levels();
    void rebuild_catalog(FName focus_level_id);
    void select_category(ELevelCatalogCategory category);
    void select_level(int32 button_index);
    void apply_level_selection(int32 button_index);

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
    ml::ioj::FGameUiStyle fallback_style_{};
    UPROPERTY(Transient)
    ml::ioj::UGameSubsystem* game_{nullptr};
    int32 selected_entry_index_{INDEX_NONE};
    FName selected_level_id_{NAME_None};
    ELevelCatalogCategory active_category_{ELevelCatalogCategory::Mission};
};
}
