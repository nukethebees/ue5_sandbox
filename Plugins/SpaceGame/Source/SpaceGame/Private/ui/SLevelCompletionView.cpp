#include "SLevelCompletionView.h"

#include "LevelTelemetryPresentation.h"
#include "SpaceGame/ui/common/HiveWidgets.h"
#include "SpaceGame/ui/common/SGameButton.h"

#include "SandboxUI/widgets/SGraphPlot.h"

#include <InputCoreTypes.h>
#include <Widgets/Images/SImage.h>
#include <Widgets/Layout/SBorder.h>
#include <Widgets/Layout/SBox.h>
#include <Widgets/SBoxPanel.h>
#include <Widgets/Text/STextBlock.h>

namespace ml::ioj {
void SLevelCompletionView::Construct(FArguments const& args) {
    style_ = args._Style;
    audio_ = args._Audio;
    check(style_ != nullptr);
    on_return_to_mission_control_ = args._OnReturnToMissionControl;
    on_keep_operating_ = args._OnKeepOperating;

    auto const body{
        SNew(SHorizontalBox) + SHorizontalBox::Slot().AutoWidth()[build_summary()] +
        SHorizontalBox::Slot().FillWidth(1.0f).Padding(FMargin{2.0f, 0.0f})[build_report()]};

    ChildSlot[SNew(SBorder)
                  .BorderImage(&style_->chrome().canvas)
                  .Padding(FMargin{})[SNew(SHiveFrame)
                                          .Style(&style_->chrome())
                                              [SNew(SVerticalBox) +
                                               SVerticalBox::Slot().AutoHeight()[build_header()] +
                                               SVerticalBox::Slot().FillHeight(1.0f)[body] +
                                               SVerticalBox::Slot().AutoHeight()[build_footer()]]]];

    level_telemetry_presentation::apply_activity_graph_style(
        *activity_graph_,
        *style_,
        NSLOCTEXT("LevelCompletion", "NoActivity", "No engagement activity recorded"),
        {760.0f, 320.0f});
}

void SLevelCompletionView::update_report(FString const& level_display_name,
                                         ETestMissionState const state,
                                         FLevelTelemetrySnapshot const& snapshot) {
    check(state == ETestMissionState::Succeeded || state == ETestMissionState::Failed);
    auto const succeeded{state == ETestMissionState::Succeeded};
    mission_name_->SetText(level_display_name.IsEmpty()
                               ? NSLOCTEXT("LevelCompletion", "UnknownMission", "UNNAMED OPERATION")
                               : FText::FromString(level_display_name.ToUpper()));
    mission_result_->SetText(
        succeeded ? NSLOCTEXT("LevelCompletion", "MissionComplete", "MISSION COMPLETE")
                  : NSLOCTEXT("LevelCompletion", "MissionFailed", "MISSION FAILED"));
    mission_result_->SetColorAndOpacity(succeeded ? style_->palette().success
                                                  : style_->palette().danger);
    objective_status_->SetText(
        succeeded
            ? NSLOCTEXT("LevelCompletion", "ObjectiveSatisfied", "OBJECTIVE STATUS // SATISFIED")
            : NSLOCTEXT(
                  "LevelCompletion", "ObjectiveUnsatisfied", "OBJECTIVE STATUS // UNSATISFIED"));
    elapsed_time_->SetText(
        level_telemetry_presentation::format_elapsed_time(snapshot.elapsed_seconds));
    kills_->SetText(FText::AsNumber(snapshot.kills));
    destroyed_entities_->SetText(FText::AsNumber(snapshot.destroyed_entities));
    lasers_fired_->SetText(FText::AsNumber(snapshot.lasers_fired));
    spawned_entities_->SetText(FText::AsNumber(snapshot.spawned_entities));
    active_entities_->SetText(FText::AsNumber(snapshot.active_entities));
    active_lasers_->SetText(FText::AsNumber(snapshot.active_lasers));
    level_telemetry_presentation::update_activity_graph(
        *activity_graph_, snapshot, style_->palette().honey, style_->palette().danger);
}

void SLevelCompletionView::focus_primary_action() {
    focus_action(ELevelCompletionAction::ReturnToMissionControl);
}

auto SLevelCompletionView::OnFocusReceived(FGeometry const& geometry,
                                           FFocusEvent const& focus_event) -> FReply {
    static_cast<void>(geometry);
    static_cast<void>(focus_event);
    focus_action(focused_action_);
    return FReply::Handled();
}

auto SLevelCompletionView::OnKeyDown(FGeometry const& geometry, FKeyEvent const& key_event)
    -> FReply {
    auto const key{key_event.GetKey()};
    if (key != EKeys::Left && key != EKeys::Right && key != EKeys::Gamepad_DPad_Left &&
        key != EKeys::Gamepad_DPad_Right) {
        return SCompoundWidget::OnKeyDown(geometry, key_event);
    }

    auto const action{focused_action_ == ELevelCompletionAction::ReturnToMissionControl
                          ? ELevelCompletionAction::KeepOperating
                          : ELevelCompletionAction::ReturnToMissionControl};
    focus_action(action);
    return FReply::Handled();
}

auto SLevelCompletionView::build_header() const -> TSharedRef<SWidget> {
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
                 .VAlign(VAlign_Center)[SNew(STextBlock)
                                            .Text(NSLOCTEXT(
                                                "LevelCompletion", "Title", "AFTER-ACTION REPORT"))
                                            .TextStyle(&style_->text(EGameTextStyle::Heading2))] +
             SHorizontalBox::Slot().AutoWidth().VAlign(
                 VAlign_Center)[SNew(STextBlock)
                                    .Text(NSLOCTEXT("LevelCompletion",
                                                    "SystemContext",
                                                    "TACTICAL OPERATIONS // MISSION ASSESSMENT"))
                                    .TextStyle(&style_->text(EGameTextStyle::Caption))]];
}

auto SLevelCompletionView::build_summary() -> TSharedRef<SWidget> {
    auto details{SNew(SVerticalBox)};
    details->AddSlot().AutoHeight().Padding(
        FMargin{0.0f,
                0.0f,
                0.0f,
                18.0f})[SNew(STextBlock)
                            .Text(NSLOCTEXT("LevelCompletion", "SummaryCaption", "MISSION RECORD"))
                            .TextStyle(&style_->text(EGameTextStyle::Caption))];
    details->AddSlot().AutoHeight()[SAssignNew(mission_name_, STextBlock)
                                        .TextStyle(&style_->text(EGameTextStyle::Heading2))
                                        .AutoWrapText(true)];
    details->AddSlot().AutoHeight().Padding(
        FMargin{0.0f, 10.0f, 0.0f, 4.0f})[SAssignNew(mission_result_, STextBlock)
                                              .TextStyle(&style_->text(EGameTextStyle::Heading3))];
    details->AddSlot().AutoHeight()[SAssignNew(objective_status_, STextBlock)
                                        .TextStyle(&style_->text(EGameTextStyle::Caption))];
    details->AddSlot().AutoHeight().Padding(FMargin{0.0f, 22.0f})[SNew(SBox).HeightOverride(
        2.0f)[SNew(SBorder).BorderImage(&style_->chrome().focus)]];
    details->AddSlot().AutoHeight().Padding(
        FMargin{0.0f,
                0.0f,
                0.0f,
                12.0f})[SNew(STextBlock)
                            .Text(NSLOCTEXT("LevelCompletion", "ForceState", "TERMINAL STATE"))
                            .TextStyle(&style_->text(EGameTextStyle::Heading3))];
    details->AddSlot().AutoHeight()[build_detail(
        NSLOCTEXT("LevelCompletion", "EntitiesSpawned", "ENTITIES SPAWNED"), spawned_entities_)];
    details->AddSlot().AutoHeight()[build_detail(
        NSLOCTEXT("LevelCompletion", "EntitiesActive", "ENTITIES ACTIVE"), active_entities_)];
    details->AddSlot().AutoHeight()[build_detail(
        NSLOCTEXT("LevelCompletion", "LasersActive", "LASERS ACTIVE"), active_lasers_)];
    details->AddSlot().FillHeight(1.0f);
    details->AddSlot().AutoHeight()[SNew(STextBlock)
                                        .Text(NSLOCTEXT("LevelCompletion",
                                                        "RecordClassification",
                                                        "RECORD CLASS // OPERATIONAL"))
                                        .TextStyle(&style_->text(EGameTextStyle::Caption))];

    return SNew(SBorder)
        .BorderImage(&style_->chrome().navigation_background)
        .Padding(
            FMargin{24.0f})[SNew(SBox).WidthOverride(320.0f).MinDesiredHeight(400.0f)[details]];
}

auto SLevelCompletionView::build_report() -> TSharedRef<SWidget> {
    auto metrics{SNew(SHorizontalBox)};
    metrics->AddSlot().FillWidth(1.0f)[build_metric(
        NSLOCTEXT("LevelCompletion", "ElapsedTime", "OPERATION TIME"), elapsed_time_)];
    metrics->AddSlot().FillWidth(1.0f).Padding(FMargin{
        8.0f,
        0.0f})[build_metric(NSLOCTEXT("LevelCompletion", "Kills", "CONFIRMED KILLS"), kills_)];
    metrics->AddSlot().FillWidth(1.0f)[build_metric(
        NSLOCTEXT("LevelCompletion", "Destroyed", "ENTITIES DESTROYED"), destroyed_entities_)];
    metrics->AddSlot().FillWidth(1.0f).Padding(FMargin{8.0f, 0.0f, 0.0f, 0.0f})[build_metric(
        NSLOCTEXT("LevelCompletion", "LasersFired", "LASERS FIRED"), lasers_fired_)];

    auto const graph{
        SNew(SBorder)
            .BorderImage(&style_->panel().background)
            .Padding(FMargin{
                20.0f})[SNew(SVerticalBox) +
                        SVerticalBox::Slot().AutoHeight()
                            [SNew(STextBlock)
                                 .Text(NSLOCTEXT(
                                     "LevelCompletion", "ActivityTitle", "ENGAGEMENT ACTIVITY"))
                                 .TextStyle(&style_->text(EGameTextStyle::Heading3))] +
                        SVerticalBox::Slot().AutoHeight().Padding(FMargin{0.0f, 5.0f, 0.0f, 14.0f})
                            [SNew(STextBlock)
                                 .Text(NSLOCTEXT(
                                     "LevelCompletion",
                                     "ActivityDescription",
                                     "Active entities and confirmed kills across simulation time."))
                                 .TextStyle(&style_->text(EGameTextStyle::Caption))] +
                        SVerticalBox::Slot().FillHeight(1.0f)[SNew(SBox).MinDesiredHeight(
                            260.0f)[SAssignNew(activity_graph_, SGraphPlot)]]]};

    return SNew(SBorder)
        .BorderImage(&style_->chrome().body_background)
        .Padding(FMargin{28.0f})
            [SNew(SVerticalBox) +
             SVerticalBox::Slot().AutoHeight()
                 [SNew(STextBlock)
                      .Text(NSLOCTEXT("LevelCompletion", "ReportHeading", "OPERATIONAL SUMMARY"))
                      .TextStyle(&style_->text(EGameTextStyle::Heading1))] +
             SVerticalBox::Slot().AutoHeight().Padding(FMargin{0.0f, 5.0f, 0.0f, 22.0f})
                 [SNew(STextBlock)
                      .Text(NSLOCTEXT("LevelCompletion",
                                      "ReportDescription",
                                      "Final telemetry snapshot recorded at mission completion."))
                      .TextStyle(&style_->text(EGameTextStyle::BodySecondary))] +
             SVerticalBox::Slot().AutoHeight().Padding(FMargin{0.0f, 0.0f, 0.0f, 16.0f})[metrics] +
             SVerticalBox::Slot().FillHeight(1.0f)[graph]];
}

auto SLevelCompletionView::build_footer() -> TSharedRef<SWidget> {
    auto return_button{
        SNew(SGameButton)
            .Style(&style_->button(EGameButtonStyle::Primary))
            .Audio(audio_)
            .Text(NSLOCTEXT("LevelCompletion", "Return", "Return to Mission Control"))
            .OnClicked(this,
                       &SLevelCompletionView::activate_action,
                       ELevelCompletionAction::ReturnToMissionControl,
                       on_return_to_mission_control_)};
    auto keep_operating_button{
        SNew(SGameButton)
            .Style(&style_->button(EGameButtonStyle::Secondary))
            .Audio(audio_)
            .Text(NSLOCTEXT("LevelCompletion", "KeepOperating", "Keep Operating"))
            .OnClicked(this,
                       &SLevelCompletionView::activate_action,
                       ELevelCompletionAction::KeepOperating,
                       on_keep_operating_)};
    action_buttons_.Add(return_button);
    action_buttons_.Add(keep_operating_button);

    return SNew(SBorder)
        .BorderImage(&style_->chrome().footer_background)
        .Padding(style_->chrome().footer_padding)
            [SNew(SHorizontalBox) +
             SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
                 [SNew(STextBlock)
                      .Text(
                          NSLOCTEXT("LevelCompletion", "ReportStatus", "REPORT STATUS // ARCHIVED"))
                      .TextStyle(&style_->text(EGameTextStyle::Caption))] +
             SHorizontalBox::Slot().AutoWidth().Padding(FMargin{
                 0.0f, 0.0f, 8.0f, 0.0f})[SNew(SBox).MinDesiredWidth(230.0f)[return_button]] +
             SHorizontalBox::Slot()
                 .AutoWidth()[SNew(SBox).MinDesiredWidth(180.0f)[keep_operating_button]]];
}

auto SLevelCompletionView::build_metric(FText label, TSharedPtr<STextBlock>& value) const
    -> TSharedRef<SWidget> {
    return SNew(SBorder)
        .BorderImage(&style_->panel().background)
        .Padding(FMargin{16.0f})
            [SNew(SVerticalBox) +
             SVerticalBox::Slot()
                 .AutoHeight()[SNew(STextBlock)
                                   .Text(MoveTemp(label))
                                   .TextStyle(&style_->text(EGameTextStyle::Caption))] +
             SVerticalBox::Slot().AutoHeight().Padding(FMargin{
                 0.0f, 8.0f, 0.0f, 0.0f})[SAssignNew(value, STextBlock)
                                              .TextStyle(&style_->text(EGameTextStyle::Heading2))]];
}

auto SLevelCompletionView::build_detail(FText label, TSharedPtr<STextBlock>& value) const
    -> TSharedRef<SWidget> {
    return SNew(SHorizontalBox) +
           SHorizontalBox::Slot().FillWidth(1.0f).Padding(
               FMargin{0.0f, 5.0f})[SNew(STextBlock)
                                        .Text(MoveTemp(label))
                                        .TextStyle(&style_->text(EGameTextStyle::Caption))] +
           SHorizontalBox::Slot().AutoWidth().Padding(FMargin{
               12.0f,
               5.0f})[SAssignNew(value, STextBlock).TextStyle(&style_->text(EGameTextStyle::Body))];
}

auto SLevelCompletionView::activate_action(ELevelCompletionAction const action,
                                           FSimpleDelegate delegate) -> FReply {
    focused_action_ = action;
    delegate.ExecuteIfBound();
    return FReply::Handled();
}

void SLevelCompletionView::focus_action(ELevelCompletionAction const action) {
    focused_action_ = action;
    auto const index{static_cast<int32>(action)};
    if (action_buttons_.IsValidIndex(index) && action_buttons_[index].IsValid()) {
        action_buttons_[index]->focus();
    }
}
} // namespace ml::ioj
