#pragma once

#include "Blueprint/UserWidget.h"
#include "SpaceGamePresentation/ui/style/GameUiStyle.h"

#include "SaveGameViewerWidget.generated.h"

class USpaceSaveSubsystem;

namespace ml::ioj {
struct FLevelOutcomeSummary;
struct FSaveGameBrowser;
struct FSaveProfileReport;
struct FSaveProfileSummary;
class SSaveGameViewerView;
class UGameSubsystem;

struct FSaveProfileViewRow {
    FString profile_id{};
    FText text{};
    bool active{};
};

struct FSaveOutcomeViewRow {
    FString outcome_id{};
    FText text{};
};

struct FSaveStatisticViewRow {
    FText label{};
    FText value{};
};

struct FSaveGameViewState {
    TArray<FSaveProfileViewRow> profiles{};
    TArray<FSaveOutcomeViewRow> outcomes{};
    TArray<FSaveStatisticViewRow> statistics{};
    FText archive_status{};
    FText profile_name{};
    FText profile_status{};
    FText profile_id{};
    FText profile_created{};
    FText profile_last_played{};
    FText profile_duration{};
    FText profile_totals{};
    FText outcome_name{};
    FText outcome_status{};
    FText outcome_completed{};
    FText outcome_duration{};
    FText outcome_kills{};
    int32 selected_profile_index{INDEX_NONE};
    int32 selected_outcome_index{INDEX_NONE};
    bool can_activate{};
};

DECLARE_MULTICAST_DELEGATE_OneParam(FSaveGameModalStateChanged, bool);

UCLASS()
class SPACEGAME_API USaveGameViewerWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    USaveGameViewerWidget(FObjectInitializer const& object_initializer);

    void set_browser(FSaveGameBrowser& browser);
    void focus_primary_action();
    [[nodiscard]] auto get_focus_target() const -> UWidget*;

    void select_profile(FString const& profile_id);
    void select_outcome(FString const& outcome_id);
    void refresh();
    void begin_create_profile();
    void cancel_create_profile();
    void request_back();

    [[nodiscard]] auto get_profile_count() const noexcept -> int32 {
        return view_state_.profiles.Num();
    }
    [[nodiscard]] auto get_outcome_count() const noexcept -> int32 {
        return view_state_.outcomes.Num();
    }
    [[nodiscard]] auto get_selected_profile_id() const -> FString const& {
        return selected_profile_id_;
    }
    [[nodiscard]] auto get_selected_profile_name() const -> FText const& {
        return view_state_.profile_name;
    }
    [[nodiscard]] auto get_selected_outcome_id() const -> FString const& {
        return selected_outcome_id_;
    }
    [[nodiscard]] auto can_activate_selected_profile() const noexcept -> bool {
        return view_state_.can_activate;
    }
    [[nodiscard]] auto is_create_profile_open() const noexcept -> bool {
        return create_profile_open_;
    }

    FSaveGameModalStateChanged modal_state_changed;
  protected:
    void NativeOnInitialized() override;
    auto RebuildWidget() -> TSharedRef<SWidget> override;
    void ReleaseSlateResources(bool release_children) override;
    auto NativeOnFocusReceived(FGeometry const& geometry, FFocusEvent const& focus_event)
        -> FReply override;
  private:
    auto resolve_browser() -> FSaveGameBrowser*;
    auto resolve_save_subsystem() const -> USpaceSaveSubsystem*;
    void rebuild_profiles();
    void apply_profile_selection(FString const& profile_id);
    void rebuild_outcomes(FSaveProfileReport const& report);
    void apply_outcome_selection(FString const& outcome_id);
    void show_profile(FSaveProfileSummary const& profile);
    void show_outcome(FLevelOutcomeSummary const& outcome);
    void show_empty_profiles();
    void publish_all();
    void publish_profile();
    void publish_outcome();

    void handle_create_profile(FString const& display_name);
    void handle_activate_profile();
    void handle_reset_test_profile();
    void refresh_and_select(FString const& profile_id);
    void show_create_profile_error(FText const& error);

    FString selected_profile_id_{};
    FString selected_outcome_id_{};
    FSaveGameViewState view_state_{};
    FSaveGameBrowser* browser_override_{nullptr};
    TSharedPtr<SSaveGameViewerView> view_{};
    FGameUiStyle fallback_style_{};
    UPROPERTY(Transient)
    UGameSubsystem* game_{nullptr};
    bool create_profile_open_{};
};
} // namespace ml::ioj
