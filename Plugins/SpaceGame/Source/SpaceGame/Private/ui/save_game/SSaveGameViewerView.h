#pragma once

#include "SpaceGame/ui/save_game/SaveGameViewerWidget.h"

#include <Widgets/SCompoundWidget.h>

class SEditableText;
class SBox;
class STextBlock;
class SVerticalBox;

namespace ml::ioj {
class SGameButton;

DECLARE_DELEGATE_OneParam(FOnSaveProfileSelected, FString const&);
DECLARE_DELEGATE_OneParam(FOnSaveOutcomeSelected, FString const&);
DECLARE_DELEGATE_OneParam(FOnCreateSaveProfile, FString const&);

class SSaveGameViewerView final : public SCompoundWidget {
  public:
    SLATE_BEGIN_ARGS(SSaveGameViewerView)
        : _Style(nullptr) {}
    SLATE_ARGUMENT(FGameUiStyle const*, Style)
    SLATE_EVENT(FOnSaveProfileSelected, OnProfileSelected)
    SLATE_EVENT(FOnSaveOutcomeSelected, OnOutcomeSelected)
    SLATE_EVENT(FSimpleDelegate, OnRefresh)
    SLATE_EVENT(FSimpleDelegate, OnBeginCreate)
    SLATE_EVENT(FOnCreateSaveProfile, OnCreate)
    SLATE_EVENT(FSimpleDelegate, OnCancelCreate)
    SLATE_EVENT(FSimpleDelegate, OnActivate)
    SLATE_EVENT(FSimpleDelegate, OnResetTestProfile)
    SLATE_EVENT(FSimpleDelegate, OnBack)
    SLATE_END_ARGS()

    void Construct(FArguments const& args);
    void replace_state(FSaveGameViewState const& state);
    void replace_profile(FSaveGameViewState const& state);
    void update_outcome(FSaveGameViewState const& state);
    void focus_selected_profile();
    void show_create_profile();
    void hide_create_profile();
    void show_create_error(FText const& error);

    auto SupportsKeyboardFocus() const -> bool override { return true; }
    auto OnFocusReceived(FGeometry const& geometry, FFocusEvent const& focus_event)
        -> FReply override;
    auto OnKeyDown(FGeometry const& geometry, FKeyEvent const& key_event) -> FReply override;
  private:
    auto build_header() const -> TSharedRef<SWidget>;
    auto build_profiles() -> TSharedRef<SWidget>;
    auto build_outcomes() -> TSharedRef<SWidget>;
    auto build_report() -> TSharedRef<SWidget>;
    auto build_footer() -> TSharedRef<SWidget>;
    auto build_create_prompt() -> TSharedRef<SWidget>;
    auto handle_profile(FString profile_id) -> FReply;
    auto handle_outcome(FString outcome_id) -> FReply;
    auto handle_action(FSimpleDelegate delegate) -> FReply;
    auto handle_confirm_create() -> FReply;
    void handle_name_committed(FText const& text, ETextCommit::Type commit_type);
    void rebuild_profiles();
    void rebuild_outcomes();
    void rebuild_statistics();
    void update_profile_selection();
    void update_outcome_selection();
    void update_details();

    FGameUiStyle const* style_{};
    FSaveGameViewState state_{};
    FOnSaveProfileSelected on_profile_selected_{};
    FOnSaveOutcomeSelected on_outcome_selected_{};
    FSimpleDelegate on_refresh_{};
    FSimpleDelegate on_begin_create_{};
    FOnCreateSaveProfile on_create_{};
    FSimpleDelegate on_cancel_create_{};
    FSimpleDelegate on_activate_{};
    FSimpleDelegate on_reset_test_profile_{};
    FSimpleDelegate on_back_{};

    TSharedPtr<SVerticalBox> profile_rows_{};
    TSharedPtr<SVerticalBox> outcome_rows_{};
    TSharedPtr<SVerticalBox> statistic_rows_{};
    TSharedPtr<STextBlock> archive_status_{};
    TSharedPtr<STextBlock> profile_name_{};
    TSharedPtr<STextBlock> profile_status_{};
    TSharedPtr<STextBlock> profile_metadata_{};
    TSharedPtr<STextBlock> outcome_name_{};
    TSharedPtr<STextBlock> outcome_status_{};
    TSharedPtr<STextBlock> outcome_metadata_{};
    TSharedPtr<SGameButton> activate_button_{};
    TSharedPtr<SGameButton> refresh_button_{};
    TSharedPtr<SGameButton> create_button_{};
    TSharedPtr<SGameButton> back_button_{};
    TSharedPtr<SBox> create_prompt_{};
    TSharedPtr<SEditableText> profile_name_input_{};
    TSharedPtr<STextBlock> create_error_{};
    TArray<TSharedPtr<SGameButton>> profile_buttons_{};
    TArray<TSharedPtr<SGameButton>> outcome_buttons_{};
};
} // namespace ml::ioj
