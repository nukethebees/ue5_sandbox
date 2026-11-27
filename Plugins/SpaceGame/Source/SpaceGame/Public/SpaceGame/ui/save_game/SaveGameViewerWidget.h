#pragma once
#include "SpaceGamePresentation/ui/style/GameUiStyle.h"
#include <ioj/presentation/profile_view.h>
#include <ioj/profiles/create_profile_result.h>
#include <ioj/profiles/save_browser.h>

#include "Blueprint/UserWidget.h"

#include "SaveGameViewerWidget.generated.h"

namespace ioj {
struct MenuUpdate;
class SSaveGameViewerView;
class UGameUiStyleSubsystem;

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
    /* **************************************** */
    // Lifecycle and navigation
    /* **************************************** */
    USaveGameViewerWidget(FObjectInitializer const& object_initializer);

    void present(MenuUpdate const& update);

    void present_profiles(presentation::ProfileView const& browser);
    void show_create_result(profiles::CreateProfileResult result);
    void begin_create_profile();
    void cancel_create_profile();

    FSaveGameModalStateChanged modal_state_changed;
  protected:
    /* **************************************** */
    // Widget lifecycle
    /* **************************************** */
    void NativeOnInitialized() override;
    auto RebuildWidget() -> TSharedRef<SWidget> override;
    void ReleaseSlateResources(bool release_children) override;
    auto NativeOnFocusReceived(FGeometry const& geometry, FFocusEvent const& focus_event)
        -> FReply override;
  private:
    /* **************************************** */
    // Profile actions
    /* **************************************** */
    void show_create_profile_error(FText const& error);

    /* **************************************** */
    // Data sources and selection
    /* **************************************** */
    void rebuild_outcomes(presentation::ProfileReportView const& report);
    void show_profile(profiles::ProfileSummary const& profile, std::string_view active_profile_id);
    void show_outcome(profiles::LevelOutcomeSummary const& outcome);
    void show_empty_profiles();

    /* **************************************** */
    // State publication
    /* **************************************** */
    void publish_all();

    /* **************************************** */
    // State
    /* **************************************** */
    FSaveGameViewState view_state_{};
    TSharedPtr<SSaveGameViewerView> view_{};
    FGameUiStyle fallback_style_{};
    UPROPERTY(Transient)
    UGameUiStyleSubsystem* style_{nullptr};
};

} // namespace ioj
