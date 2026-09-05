#include "SSaveGameViewerView.h"

#include "SpaceGame/ui/common/HiveWidgets.h"
#include "SpaceGame/ui/common/SGameButton.h"

#include <Framework/Application/SlateApplication.h>
#include <InputCoreTypes.h>
#include <Widgets/Images/SImage.h>
#include <Widgets/Input/SEditableText.h>
#include <Widgets/Layout/SBorder.h>
#include <Widgets/Layout/SBox.h>
#include <Widgets/Layout/SScrollBox.h>
#include <Widgets/SBoxPanel.h>
#include <Widgets/SOverlay.h>
#include <Widgets/Text/STextBlock.h>

namespace ml::ioj {
namespace {
auto fixed_button(TSharedPtr<SGameButton>& button,
                  FGameUiStyle const& style,
                  EGameButtonStyle const button_style,
                  FText text,
                  FOnClicked on_clicked) -> TSharedRef<SWidget> {
    return SNew(SBox).WidthOverride(190.0f)[SAssignNew(button, SGameButton)
                                                .Style(&style.button(button_style))
                                                .Text(MoveTemp(text))
                                                .OnClicked(MoveTemp(on_clicked))];
}

auto metadata_text(FText const& first, FText const& second, FText const& third, FText const& fourth)
    -> FText {
    return FText::Format(NSLOCTEXT("SaveGameViewer", "Metadata", "{0}\n{1}\n{2}\n{3}"),
                         first,
                         second,
                         third,
                         fourth);
}
} // namespace

void SSaveGameViewerView::Construct(FArguments const& args) {
    style_ = args._Style;
    check(style_ != nullptr);
    on_profile_selected_ = args._OnProfileSelected;
    on_outcome_selected_ = args._OnOutcomeSelected;
    on_refresh_ = args._OnRefresh;
    on_begin_create_ = args._OnBeginCreate;
    on_create_ = args._OnCreate;
    on_cancel_create_ = args._OnCancelCreate;
    on_activate_ = args._OnActivate;
    on_reset_test_profile_ = args._OnResetTestProfile;
    on_back_ = args._OnBack;

    auto const body{
        SNew(SHorizontalBox) + SHorizontalBox::Slot().AutoWidth()[build_profiles()] +
        SHorizontalBox::Slot().AutoWidth().Padding(FMargin{2.0f, 0.0f})[build_outcomes()] +
        SHorizontalBox::Slot().FillWidth(1.0f)[build_report()]};
    auto const frame{
        SNew(SBorder)
            .BorderImage(&style_->chrome().canvas)
            .Padding(FMargin{})
                [SNew(SHiveFrame)
                     .Style(&style_->chrome())[SNew(SVerticalBox) +
                                               SVerticalBox::Slot().AutoHeight()[build_header()] +
                                               SVerticalBox::Slot().FillHeight(1.0f)[body] +
                                               SVerticalBox::Slot().AutoHeight()[build_footer()]]]};

    SAssignNew(create_prompt_, SBox);
    create_prompt_->SetVisibility(EVisibility::Collapsed);
    ChildSlot[SNew(SOverlay) + SOverlay::Slot()[frame] +
              SOverlay::Slot()[create_prompt_.ToSharedRef()]];
}

void SSaveGameViewerView::replace_state(FSaveGameViewState const& state) {
    state_ = state;
    rebuild_profiles();
    rebuild_outcomes();
    rebuild_statistics();
    update_details();
}

void SSaveGameViewerView::replace_profile(FSaveGameViewState const& state) {
    state_ = state;
    update_profile_selection();
    rebuild_outcomes();
    rebuild_statistics();
    update_details();
}

void SSaveGameViewerView::update_outcome(FSaveGameViewState const& state) {
    state_ = state;
    update_outcome_selection();
    rebuild_statistics();
    update_details();
}

void SSaveGameViewerView::focus_selected_profile() {
    if (profile_buttons_.IsEmpty()) {
        if (refresh_button_.IsValid()) {
            refresh_button_->focus();
        }
        return;
    }

    auto const index{profile_buttons_.IsValidIndex(state_.selected_profile_index)
                         ? state_.selected_profile_index
                         : 0};
    profile_buttons_[index]->focus();
}

void SSaveGameViewerView::show_create_profile() {
    if (!create_prompt_.IsValid()) {
        return;
    }
    if (!profile_name_input_.IsValid()) {
        create_prompt_->SetContent(build_create_prompt());
    }
    profile_name_input_->SetText(FText::GetEmpty());
    create_error_->SetText(FText::GetEmpty());
    create_error_->SetVisibility(EVisibility::Collapsed);
    create_prompt_->SetVisibility(EVisibility::Visible);
    FSlateApplication::Get().SetKeyboardFocus(profile_name_input_, EFocusCause::SetDirectly);
}

void SSaveGameViewerView::hide_create_profile() {
    if (create_prompt_.IsValid()) {
        create_prompt_->SetVisibility(EVisibility::Collapsed);
    }
    if (create_button_.IsValid()) {
        create_button_->focus();
    }
}

void SSaveGameViewerView::show_create_error(FText const& error) {
    create_error_->SetText(error);
    create_error_->SetVisibility(EVisibility::Visible);
}

auto SSaveGameViewerView::OnFocusReceived(FGeometry const& geometry, FFocusEvent const& focus_event)
    -> FReply {
    static_cast<void>(geometry);
    static_cast<void>(focus_event);
    focus_selected_profile();
    return FReply::Handled();
}

auto SSaveGameViewerView::OnKeyDown(FGeometry const& geometry, FKeyEvent const& key_event)
    -> FReply {
    if (create_prompt_.IsValid() && create_prompt_->GetVisibility() == EVisibility::Visible) {
        if (key_event.GetKey() == EKeys::Escape ||
            key_event.GetKey() == EKeys::Gamepad_FaceButton_Right) {
            on_cancel_create_.ExecuteIfBound();
            return FReply::Handled();
        }
        return SCompoundWidget::OnKeyDown(geometry, key_event);
    }

    auto direction{0};
    auto const key{key_event.GetKey()};
    if (key == EKeys::Up || key == EKeys::Gamepad_DPad_Up) {
        direction = -1;
    } else if (key == EKeys::Down || key == EKeys::Gamepad_DPad_Down) {
        direction = 1;
    }
    if (direction == 0 || profile_buttons_.IsEmpty()) {
        return SCompoundWidget::OnKeyDown(geometry, key_event);
    }

    auto current_index{state_.selected_profile_index};
    auto const button_count{profile_buttons_.Num()};
    for (int32 index{}; index < button_count; ++index) {
        if (profile_buttons_[index]->has_focus()) {
            current_index = index;
            break;
        }
    }
    if (!profile_buttons_.IsValidIndex(current_index)) {
        current_index = direction > 0 ? 0 : button_count - 1;
    } else {
        current_index = (current_index + direction + button_count) % button_count;
    }
    profile_buttons_[current_index]->focus();
    return FReply::Handled();
}

auto SSaveGameViewerView::build_header() const -> TSharedRef<SWidget> {
    return SNew(SBorder)
        .BorderImage(&style_->chrome().header_background)
        .Padding(style_->chrome().header_padding)
            [SNew(SHorizontalBox) +
             SHorizontalBox::Slot().AutoWidth().VAlign(
                 VAlign_Center)[SNew(SImage)
                                    .Image(&style_->icon(EGameUiIcon::Hive))
                                    .ColorAndOpacity(style_->palette().honey)
                                    .DesiredSizeOverride(FVector2D{28.0f, 28.0f})] +
             SHorizontalBox::Slot()
                 .FillWidth(1.0f)
                 .Padding(FMargin{14.0f, 0.0f})
                 .VAlign(
                     VAlign_Center)[SNew(STextBlock)
                                        .Text(NSLOCTEXT("SaveGameViewer", "Title", "DATA ARCHIVE"))
                                        .TextStyle(&style_->text(EGameTextStyle::Heading2))] +
             SHorizontalBox::Slot().AutoWidth().VAlign(
                 VAlign_Center)[SNew(STextBlock)
                                    .Text(NSLOCTEXT("SaveGameViewer",
                                                    "SystemContext",
                                                    "HIVE SYSTEMS // SERVICE RECORDS"))
                                    .TextStyle(&style_->text(EGameTextStyle::Caption))]];
}

auto SSaveGameViewerView::build_profiles() -> TSharedRef<SWidget> {
    auto const contents{
        SNew(SVerticalBox) +
        SVerticalBox::Slot().AutoHeight().Padding(FMargin{
            0.0f,
            0.0f,
            0.0f,
            4.0f})[SNew(STextBlock)
                       .Text(NSLOCTEXT("SaveGameViewer", "ProfileCaption", "PERSONNEL ARCHIVE"))
                       .TextStyle(&style_->text(EGameTextStyle::Caption))] +
        SVerticalBox::Slot().AutoHeight().Padding(
            FMargin{0.0f,
                    0.0f,
                    0.0f,
                    8.0f})[SNew(STextBlock)
                               .Text(NSLOCTEXT("SaveGameViewer", "ProfileTitle", "SERVICE RECORDS"))
                               .TextStyle(&style_->text(EGameTextStyle::Heading3))] +
        SVerticalBox::Slot().AutoHeight().Padding(
            FMargin{0.0f, 0.0f, 0.0f, 16.0f})[SAssignNew(archive_status_, STextBlock)
                                                  .TextStyle(&style_->text(EGameTextStyle::Caption))
                                                  .AutoWrapText(true)] +
        SVerticalBox::Slot().FillHeight(
            1.0f)[SNew(SScrollBox).ScrollBarStyle(&style_->chrome().scroll_bar) +
                  SScrollBox::Slot()[SAssignNew(profile_rows_, SVerticalBox)]]};

    return SNew(SBorder)
        .BorderImage(&style_->chrome().navigation_background)
        .Padding(
            FMargin{18.0f})[SNew(SBox).WidthOverride(290.0f).MinDesiredHeight(400.0f)[contents]];
}

auto SSaveGameViewerView::build_outcomes() -> TSharedRef<SWidget> {
    auto const contents{
        SNew(SVerticalBox) +
        SVerticalBox::Slot().AutoHeight().Padding(FMargin{
            0.0f,
            0.0f,
            0.0f,
            4.0f})[SNew(STextBlock)
                       .Text(NSLOCTEXT("SaveGameViewer", "OutcomeCaption", "MISSION HISTORY"))
                       .TextStyle(&style_->text(EGameTextStyle::Caption))] +
        SVerticalBox::Slot().AutoHeight().Padding(FMargin{
            0.0f,
            0.0f,
            0.0f,
            16.0f})[SNew(STextBlock)
                        .Text(NSLOCTEXT("SaveGameViewer", "OutcomeTitle", "OPERATION REPORTS"))
                        .TextStyle(&style_->text(EGameTextStyle::Heading3))] +
        SVerticalBox::Slot().FillHeight(
            1.0f)[SNew(SScrollBox).ScrollBarStyle(&style_->chrome().scroll_bar) +
                  SScrollBox::Slot()[SAssignNew(outcome_rows_, SVerticalBox)]]};

    return SNew(SBorder)
        .BorderImage(&style_->chrome().navigation_background)
        .Padding(
            FMargin{18.0f})[SNew(SBox).WidthOverride(280.0f).MinDesiredHeight(400.0f)[contents]];
}

auto SSaveGameViewerView::build_report() -> TSharedRef<SWidget> {
    auto const profile_header{
        SNew(SHorizontalBox) +
        SHorizontalBox::Slot().FillWidth(
            1.0f)[SNew(SVerticalBox) +
                  SVerticalBox::Slot()
                      .AutoHeight()[SAssignNew(profile_name_, STextBlock)
                                        .TextStyle(&style_->text(EGameTextStyle::Heading1))
                                        .AutoWrapText(true)] +
                  SVerticalBox::Slot().AutoHeight().Padding(FMargin{
                      0.0f, 6.0f})[SAssignNew(profile_status_, STextBlock)
                                       .TextStyle(&style_->text(EGameTextStyle::BodySecondary))] +
                  SVerticalBox::Slot().AutoHeight().Padding(FMargin{0.0f, 8.0f, 0.0f, 0.0f})
                      [SAssignNew(profile_metadata_, STextBlock)
                           .TextStyle(&style_->text(EGameTextStyle::Caption))
                           .AutoWrapText(true)]] +
        SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(FMargin{24.0f, 0.0f})
            .VAlign(VAlign_Top)[fixed_button(
                activate_button_,
                *style_,
                EGameButtonStyle::Primary,
                NSLOCTEXT("SaveGameViewer", "ActivateProfile", "Set Active Record"),
                FOnClicked::CreateSP(this, &SSaveGameViewerView::handle_action, on_activate_))]};

    auto const report_panel{
        SNew(SBorder)
            .BorderImage(&style_->chrome().frame_border)
            .Padding(FMargin{1.0f})
                [SNew(SBorder)
                     .BorderImage(&style_->chrome().navigation_background)
                     .Padding(FMargin{20.0f})
                         [SNew(SVerticalBox) +
                          SVerticalBox::Slot()
                              .AutoHeight()[SAssignNew(outcome_name_, STextBlock)
                                                .TextStyle(&style_->text(EGameTextStyle::Heading2))
                                                .AutoWrapText(true)] +
                          SVerticalBox::Slot().AutoHeight().Padding(FMargin{0.0f, 6.0f})
                              [SAssignNew(outcome_status_, STextBlock)
                                   .TextStyle(&style_->text(EGameTextStyle::BodySecondary))] +
                          SVerticalBox::Slot().AutoHeight().Padding(FMargin{
                              0.0f, 8.0f, 0.0f, 18.0f})[SAssignNew(outcome_metadata_, STextBlock)
                                                            .TextStyle(&style_->text(
                                                                EGameTextStyle::Caption))
                                                            .AutoWrapText(true)] +
                          SVerticalBox::Slot().AutoHeight().Padding(FMargin{
                              0.0f, 4.0f, 0.0f, 12.0f})[SNew(SHiveSectionHeader)
                                                            .Style(style_)
                                                            .Text(NSLOCTEXT("SaveGameViewer",
                                                                            "Statistics",
                                                                            "RECORDED METRICS"))] +
                          SVerticalBox::Slot().FillHeight(1.0f)
                              [SNew(SScrollBox).ScrollBarStyle(&style_->chrome().scroll_bar) +
                               SScrollBox::Slot()[SAssignNew(statistic_rows_, SVerticalBox)]]]]};

    return SNew(SBorder)
        .BorderImage(&style_->chrome().body_background)
        .Padding(FMargin{
            30.0f})[SNew(SVerticalBox) + SVerticalBox::Slot().AutoHeight()[profile_header] +
                    SVerticalBox::Slot().AutoHeight().Padding(FMargin{0.0f, 26.0f, 0.0f, 12.0f})
                        [SNew(SHiveSectionHeader)
                             .Style(style_)
                             .Text(NSLOCTEXT("SaveGameViewer", "Report", "AFTER-ACTION REPORT"))] +
                    SVerticalBox::Slot().FillHeight(1.0f)[report_panel]];
}

auto SSaveGameViewerView::build_footer() -> TSharedRef<SWidget> {
    auto footer{SNew(SHorizontalBox)};
    footer->AddSlot().AutoWidth()[fixed_button(
        back_button_,
        *style_,
        EGameButtonStyle::Secondary,
        NSLOCTEXT("SaveGameViewer", "Back", "Back"),
        FOnClicked::CreateSP(this, &SSaveGameViewerView::handle_action, on_back_))];
    footer->AddSlot().AutoWidth().Padding(FMargin{10.0f, 0.0f})[fixed_button(
        create_button_,
        *style_,
        EGameButtonStyle::Primary,
        NSLOCTEXT("SaveGameViewer", "CreateProfile", "New Service Record"),
        FOnClicked::CreateSP(this, &SSaveGameViewerView::handle_action, on_begin_create_))];
#if !UE_BUILD_SHIPPING
    TSharedPtr<SGameButton> reset_button;
    footer->AddSlot().AutoWidth()[fixed_button(
        reset_button,
        *style_,
        EGameButtonStyle::Secondary,
        NSLOCTEXT("SaveGameViewer", "ResetTestProfile", "Reset Test Record"),
        FOnClicked::CreateSP(this, &SSaveGameViewerView::handle_action, on_reset_test_profile_))];
#endif
    footer->AddSlot().FillWidth(1.0f);
    footer->AddSlot().AutoWidth()[fixed_button(
        refresh_button_,
        *style_,
        EGameButtonStyle::Secondary,
        NSLOCTEXT("SaveGameViewer", "Refresh", "Refresh Archive"),
        FOnClicked::CreateSP(this, &SSaveGameViewerView::handle_action, on_refresh_))];

    return SNew(SBorder)
        .BorderImage(&style_->chrome().footer_background)
        .Padding(style_->chrome().footer_padding)[footer];
}

auto SSaveGameViewerView::build_create_prompt() -> TSharedRef<SWidget> {
    TSharedPtr<SGameButton> confirm_button;
    TSharedPtr<SGameButton> cancel_button;
    auto const input{
        SNew(SBorder)
            .BorderImage(&style_->chrome().frame_border)
            .Padding(FMargin{
                1.0f})[SNew(SBorder)
                           .BorderImage(&style_->chrome().body_background)
                           .Padding(FMargin{
                               12.0f,
                               8.0f})[SAssignNew(profile_name_input_, SEditableText)
                                          .HintText(NSLOCTEXT(
                                              "SaveGameViewer", "ProfileNameHint", "Record name"))
                                          .Font(style_->text(EGameTextStyle::Body).Font)
                                          .ColorAndOpacity(style_->palette().text_primary)
                                          .OnTextCommitted(
                                              this, &SSaveGameViewerView::handle_name_committed)]]};

    auto const prompt{SNew(SBox).WidthOverride(
        560.0f)[SNew(SBorder)
                    .BorderImage(&style_->chrome().frame_border)
                    .Padding(FMargin{2.0f})
                        [SNew(SBorder)
                             .BorderImage(&style_->chrome().navigation_background)
                             .Padding(FMargin{28.0f})
                                 [SNew(SVerticalBox) +
                                  SVerticalBox::Slot().AutoHeight()
                                      [SNew(SHiveSectionHeader)
                                           .Style(style_)
                                           .Icon(&style_->icon(EGameUiIcon::Hive))
                                           .Text(NSLOCTEXT("SaveGameViewer",
                                                           "CreateTitle",
                                                           "CREATE SERVICE RECORD"))] +
                                  SVerticalBox::Slot().AutoHeight().Padding(
                                      FMargin{0.0f, 18.0f, 0.0f, 8.0f})
                                      [SNew(STextBlock)
                                           .Text(NSLOCTEXT("SaveGameViewer",
                                                           "CreateDetail",
                                                           "Enter the designation for the new "
                                                           "service record."))
                                           .TextStyle(
                                               &style_->text(EGameTextStyle::BodySecondary))] +
                                  SVerticalBox::Slot().AutoHeight()[input] +
                                  SVerticalBox::Slot().AutoHeight().Padding(
                                      FMargin{0.0f, 8.0f, 0.0f, 14.0f})
                                      [SAssignNew(create_error_, STextBlock)
                                           .TextStyle(&style_->text(EGameTextStyle::Warning))
                                           .AutoWrapText(true)] +
                                  SVerticalBox::Slot().AutoHeight()
                                      [SNew(SHorizontalBox) +
                                       SHorizontalBox::Slot().FillWidth(1.0f) +
                                       SHorizontalBox::Slot().AutoWidth()[fixed_button(
                                           cancel_button,
                                           *style_,
                                           EGameButtonStyle::Secondary,
                                           NSLOCTEXT("SaveGameViewer", "CancelCreate", "Cancel"),
                                           FOnClicked::CreateSP(this,
                                                                &SSaveGameViewerView::handle_action,
                                                                on_cancel_create_))] +
                                       SHorizontalBox::Slot().AutoWidth().Padding(
                                           FMargin{10.0f, 0.0f, 0.0f, 0.0f})[fixed_button(
                                           confirm_button,
                                           *style_,
                                           EGameButtonStyle::Primary,
                                           NSLOCTEXT(
                                               "SaveGameViewer", "ConfirmCreate", "Create Record"),
                                           FOnClicked::CreateSP(
                                               this,
                                               &SSaveGameViewerView::handle_confirm_create))]]]]]};

    return SNew(SBorder)
        .BorderImage(&style_->chrome().modal_overlay)
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Center)[prompt];
}

auto SSaveGameViewerView::handle_profile(FString profile_id) -> FReply {
    on_profile_selected_.ExecuteIfBound(profile_id);
    return FReply::Handled();
}

auto SSaveGameViewerView::handle_outcome(FString outcome_id) -> FReply {
    on_outcome_selected_.ExecuteIfBound(outcome_id);
    return FReply::Handled();
}

auto SSaveGameViewerView::handle_action(FSimpleDelegate delegate) -> FReply {
    delegate.ExecuteIfBound();
    return FReply::Handled();
}

auto SSaveGameViewerView::handle_confirm_create() -> FReply {
    on_create_.ExecuteIfBound(profile_name_input_->GetText().ToString());
    return FReply::Handled();
}

void SSaveGameViewerView::handle_name_committed(FText const& text,
                                                ETextCommit::Type const commit_type) {
    static_cast<void>(text);
    if (commit_type == ETextCommit::OnEnter) {
        handle_confirm_create();
    }
}

void SSaveGameViewerView::rebuild_profiles() {
    profile_rows_->ClearChildren();
    profile_buttons_.Reset(state_.profiles.Num());
    auto const row_count{state_.profiles.Num()};
    for (int32 index{}; index < row_count; ++index) {
        auto const& row{state_.profiles[index]};
        auto const button{
            SNew(SGameButton)
                .Style(&style_->button(EGameButtonStyle::Secondary))
                .ContentAlignment(HAlign_Left)
                .Text(row.text)
                .Selected(index == state_.selected_profile_index)
                .OnClicked(this, &SSaveGameViewerView::handle_profile, row.profile_id)};
        profile_rows_->AddSlot().AutoHeight().Padding(
            FMargin{0.0f, 0.0f, 0.0f, style_->chrome().navigation_spacing})[button];
        profile_buttons_.Add(button);
    }
    archive_status_->SetText(state_.archive_status);
}

void SSaveGameViewerView::rebuild_outcomes() {
    outcome_rows_->ClearChildren();
    outcome_buttons_.Reset(state_.outcomes.Num());
    auto const row_count{state_.outcomes.Num()};
    for (int32 index{}; index < row_count; ++index) {
        auto const& row{state_.outcomes[index]};
        auto const button{
            SNew(SGameButton)
                .Style(&style_->button(EGameButtonStyle::Secondary))
                .ContentAlignment(HAlign_Left)
                .Text(row.text)
                .Selected(index == state_.selected_outcome_index)
                .OnClicked(this, &SSaveGameViewerView::handle_outcome, row.outcome_id)};
        outcome_rows_->AddSlot().AutoHeight().Padding(
            FMargin{0.0f, 0.0f, 0.0f, style_->chrome().navigation_spacing})[button];
        outcome_buttons_.Add(button);
    }
}

void SSaveGameViewerView::rebuild_statistics() {
    statistic_rows_->ClearChildren();
    if (state_.statistics.IsEmpty()) {
        statistic_rows_->AddSlot()
            .AutoHeight()[SNew(STextBlock)
                              .Text(NSLOCTEXT(
                                  "SaveGameViewer", "NoMetrics", "No additional metrics recorded."))
                              .TextStyle(&style_->text(EGameTextStyle::BodySecondary))];
        return;
    }

    for (auto const& statistic : state_.statistics) {
        statistic_rows_->AddSlot().AutoHeight().Padding(FMargin{
            0.0f, 5.0f})[SNew(SHorizontalBox) +
                         SHorizontalBox::Slot().FillWidth(
                             1.0f)[SNew(STextBlock)
                                       .Text(statistic.label)
                                       .TextStyle(&style_->text(EGameTextStyle::Caption))] +
                         SHorizontalBox::Slot()
                             .AutoWidth()[SNew(STextBlock)
                                              .Text(statistic.value)
                                              .TextStyle(&style_->text(EGameTextStyle::Body))]];
    }
}

void SSaveGameViewerView::update_profile_selection() {
    auto const button_count{profile_buttons_.Num()};
    for (int32 index{}; index < button_count; ++index) {
        profile_buttons_[index]->set_selected(index == state_.selected_profile_index);
    }
}

void SSaveGameViewerView::update_outcome_selection() {
    auto const button_count{outcome_buttons_.Num()};
    for (int32 index{}; index < button_count; ++index) {
        outcome_buttons_[index]->set_selected(index == state_.selected_outcome_index);
    }
}

void SSaveGameViewerView::update_details() {
    profile_name_->SetText(state_.profile_name);
    profile_status_->SetText(state_.profile_status);
    profile_metadata_->SetText(
        metadata_text(state_.profile_id,
                      state_.profile_created,
                      state_.profile_last_played,
                      FText::Format(NSLOCTEXT("SaveGameViewer", "ProfileDutySummary", "{0}    {1}"),
                                    state_.profile_duration,
                                    state_.profile_totals)));
    outcome_name_->SetText(state_.outcome_name);
    outcome_status_->SetText(state_.outcome_status);
    outcome_metadata_->SetText(metadata_text(state_.outcome_completed,
                                             state_.outcome_duration,
                                             state_.outcome_kills,
                                             FText::GetEmpty()));
    activate_button_->SetEnabled(state_.can_activate);
}
} // namespace ml::ioj
