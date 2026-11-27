#pragma once
#include <ioj/levels/level_catalog_category.h>
#include <ioj/profiles/completion_indicator_state.h>
#include <SpaceGame/ui/main_menu/LevelSelectWidget.h>
#include <SpaceGamePresentation/ui/style/GameUiStyle.h>
#include <SpaceGameS7/level_script_catalog.h>

#include "ScriptLevelSelectWidget.generated.h"

namespace ioj {
struct MenuUpdate;
class UGameUiStyleSubsystem;
}

namespace ioj::s7 {
using ioj::levels::authoring::FLevelScriptEntry;
using ioj::levels::authoring::LevelCatalogCategory;

class SScriptLevelSelectView;

struct FLevelSelectViewRow {
    FText text{};
    bool heading{};
    profiles::CompletionIndicatorState indicator_state{
        profiles::CompletionIndicatorState::Incomplete};
    std::string id;
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
    LevelCatalogCategory category{LevelCatalogCategory::Playable};
    int32 selected_button_index{INDEX_NONE};
    bool can_launch{};
};

UCLASS()
class SPACEGAMES7_API UScriptLevelSelectWidget : public ULevelSelectWidget {
    GENERATED_BODY()
  public:
    UScriptLevelSelectWidget();

    void refresh() override;
    void present_catalog(presentation::LevelCatalogView const& catalog) override;
    void present(MenuUpdate const& update) override;
  protected:
    void NativeOnInitialized() override;
    auto RebuildWidget() -> TSharedRef<SWidget> override;
    void ReleaseSlateResources(bool release_children) override;
    auto NativeOnFocusReceived(FGeometry const& geometry, FFocusEvent const& focus_event)
        -> FReply override;
  private:
    void refresh_levels();
    void rebuild_catalog();
    void apply_level_selection(int32 button_index);

    void publish_view();
    void publish_catalog();

    TArray<FLevelScriptEntry> entries_{};
    TArray<levels::CampaignDefinition> campaigns_{};
    TArray<int32> level_entry_indices_{};
    FString catalog_directory_{};
    FString catalog_error_{};
    FLevelSelectViewState view_state_{};
    TSharedPtr<SScriptLevelSelectView> view_{};
    FGameUiStyle fallback_style_{};
    UPROPERTY(Transient)
    UGameUiStyleSubsystem* style_{nullptr};
};
}
