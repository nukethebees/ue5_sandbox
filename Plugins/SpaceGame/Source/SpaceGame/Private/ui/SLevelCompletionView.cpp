#include "SLevelCompletionView.h"

#include "SpaceGame/ui/common/HiveWidgets.h"
#include "SpaceGame/ui/common/SGameButton.h"

#include <Widgets/Images/SImage.h>
#include <Widgets/Layout/SBorder.h>
#include <Widgets/Layout/SBox.h>
#include <Widgets/SBoxPanel.h>
#include <Widgets/Text/STextBlock.h>

namespace ioj {
namespace level_completion_detail {
auto format_elapsed_time(Duration const elapsed) -> FText {
    auto const total_seconds{FMath::FloorToInt64(elapsed.seconds())};
    auto const seconds{total_seconds % 60};
    auto const total_minutes{total_seconds / 60};
    auto const minutes{total_minutes % 60};
    auto const hours{total_minutes / 60};
    if (hours > 0) {
        return FText::FromString(
            FString::Printf(TEXT("%lld:%02lld:%02lld"), hours, minutes, seconds));
    }
    return FText::FromString(FString::Printf(TEXT("%02lld:%02lld"), minutes, seconds));
}

auto reason_text(FString const& end_reason) -> FText {
    return FText::Format(NSLOCTEXT("LevelCompletion", "EndReason", "END REASON // {0}"),
                         FText::FromString(end_reason));
}
} // namespace level_completion_detail

void SLevelCompletionView::Construct(FArguments const& args) {
    style_ = args._Style;
    audio_ = args._Audio;
    check(style_ != nullptr);
    on_return_to_mission_control_ = args._OnReturnToMissionControl;

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
}

void SLevelCompletionView::update_report(FLevelCompletion const& completion) {
    auto const& level_display_name{completion.level_display_name};
    auto const state{completion.state};
    auto const& end_reason{completion.end_reason};
    auto const par_time_seconds{completion.par_time_seconds};
    auto const new_best_time{completion.new_best_time};

    auto const succeeded{state == ELevelOutcome::Victory};
    mission_name_->SetText(level_display_name.IsEmpty()
                               ? NSLOCTEXT("LevelCompletion", "UnknownMission", "UNNAMED OPERATION")
                               : FText::FromString(level_display_name.ToUpper()));
    mission_result_->SetText(
        state == ELevelOutcome::Stopped ? NSLOCTEXT("LevelCompletion", "Stopped", "LEVEL ENDED")
        : succeeded ? NSLOCTEXT("LevelCompletion", "MissionComplete", "MISSION COMPLETE")
                    : NSLOCTEXT("LevelCompletion", "MissionFailed", "MISSION FAILED"));
    mission_result_->SetColorAndOpacity(succeeded ? style_->palette().success
                                                  : style_->palette().danger);
    failure_reason_->SetText(level_completion_detail::reason_text(end_reason));
    failure_reason_->SetColorAndOpacity(style_->palette().danger);
    failure_reason_->SetVisibility(succeeded ? EVisibility::Collapsed
                                             : EVisibility::HitTestInvisible);
    objective_status_->SetText(
        completion.winner.IsEmpty()
            ? NSLOCTEXT("LevelCompletion", "NoWinner", "NO WINNER")
            : FText::Format(NSLOCTEXT("LevelCompletion", "Winner", "WINNER // {0}"),
                            FText::FromString(completion.winner.ToUpper())));
    auto const elapsed_time{level_completion_detail::format_elapsed_time(completion.elapsed_time)};
    elapsed_time_->SetText(
        par_time_seconds.IsSet()
            ? FText::Format(NSLOCTEXT("LevelCompletion", "ElapsedTimeWithPar", "{0} (Par: {1})"),
                            elapsed_time,
                            level_completion_detail::format_elapsed_time(
                                Duration::make(par_time_seconds.GetValue())))
            : elapsed_time);
    new_best_time_->SetVisibility(succeeded && new_best_time ? EVisibility::HitTestInvisible
                                                             : EVisibility::Collapsed);
    kills_->SetText(completion.team_kills.IsSet()
                        ? FText::AsNumber(completion.team_kills.GetValue())
                        : NSLOCTEXT("LevelCompletion", "NoPlayerScore", "N/A"));
}

void SLevelCompletionView::focus_primary_action() {
    return_button_->focus();
}

auto SLevelCompletionView::OnFocusReceived(FGeometry const& geometry,
                                           FFocusEvent const& focus_event) -> FReply {
    static_cast<void>(geometry);
    static_cast<void>(focus_event);
    focus_primary_action();
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
    details->AddSlot().AutoHeight()[SAssignNew(failure_reason_, STextBlock)
                                        .TextStyle(&style_->text(EGameTextStyle::Caption))
                                        .Visibility(EVisibility::Collapsed)];
    details->AddSlot().AutoHeight()[SAssignNew(objective_status_, STextBlock)
                                        .TextStyle(&style_->text(EGameTextStyle::Caption))];
    details->AddSlot().AutoHeight().Padding(
        FMargin{0.0f,
                10.0f,
                0.0f,
                0.0f})[SAssignNew(new_best_time_, STextBlock)
                           .Text(NSLOCTEXT("LevelCompletion", "NewBestTime", "NEW BEST TIME!"))
                           .TextStyle(&style_->text(EGameTextStyle::Heading3))
                           .ColorAndOpacity(style_->palette().success)
                           .Visibility(EVisibility::Collapsed)];
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
        8.0f, 0.0f})[build_metric(NSLOCTEXT("LevelCompletion", "Kills", "TEAM KILLS"), kills_)];

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
                                      "Level outcome and profile score."))
                      .TextStyle(&style_->text(EGameTextStyle::BodySecondary))] +
             SVerticalBox::Slot().AutoHeight().Padding(FMargin{0.0f, 0.0f, 0.0f, 16.0f})[metrics]];
}

auto SLevelCompletionView::build_footer() -> TSharedRef<SWidget> {
    auto return_button{
        SNew(SGameButton)
            .Style(&style_->button(EGameButtonStyle::Primary))
            .Audio(audio_)
            .Text(NSLOCTEXT("LevelCompletion", "Return", "Return to Mission Control"))
            .OnClicked(
                this, &SLevelCompletionView::activate_action, on_return_to_mission_control_)};
    return_button_ = return_button;

    return SNew(SBorder)
        .BorderImage(&style_->chrome().footer_background)
        .Padding(style_->chrome().footer_padding)
            [SNew(SHorizontalBox) +
             SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
                 [SNew(STextBlock)
                      .Text(NSLOCTEXT("LevelCompletion", "ReportStatus", "LEVEL ENDED"))
                      .TextStyle(&style_->text(EGameTextStyle::Caption))] +
             SHorizontalBox::Slot().AutoWidth().Padding(FMargin{
                 0.0f, 0.0f, 8.0f, 0.0f})[SNew(SBox).MinDesiredWidth(230.0f)[return_button]]];
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

auto SLevelCompletionView::activate_action(FSimpleDelegate delegate) -> FReply {
    delegate.ExecuteIfBound();
    return FReply::Handled();
}

} // namespace ioj
