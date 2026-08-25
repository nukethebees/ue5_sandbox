#include "HeatmapRDG/HeatmapRDGEditorUtilityWidget.h"

#include "Experiments/HeatmapRDG/HeatmapRDGWidget.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

DEFINE_LOG_CATEGORY_STATIC(LogHeatmapRDGEditorUtility, Log, All);

namespace {
auto make_size_button(FText const& label, FOnClicked on_clicked) -> TSharedRef<SWidget> {
    return SNew(SButton).Text(label).OnClicked(MoveTemp(on_clicked));
}
} // namespace

UHeatmapRDGEditorUtilityWidget::UHeatmapRDGEditorUtilityWidget() {
    TabDisplayName = NSLOCTEXT("HeatmapRDG", "EditorUtilityTabName", "RDG Heatmap Experiment");
    bAlwaysReregisterWithWindowsMenu = true;
}

TSharedRef<SWidget> UHeatmapRDGEditorUtilityWidget::RebuildWidget() {
    heatmap_widget_ = NewObject<UHeatmapRDGWidget>(this);
    check(heatmap_widget_);

    return SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
        .Padding(
            12.0f)[SNew(SVerticalBox) +
                   SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
                       [SNew(STextBlock)
                            .Text(NSLOCTEXT("HeatmapRDG", "Title", "RDG GPU Heatmap"))
                            .TextStyle(FAppStyle::Get(), "HeadingExtraSmall")] +
                   SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
                       [SNew(STextBlock)
                            .Text(NSLOCTEXT("HeatmapRDG",
                                            "Description",
                                            "Each update uploads one dense scalar grid and draws "
                                            "one Slate image."))] +
                   SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
                       [SNew(SHorizontalBox) +
                        SHorizontalBox::Slot().AutoWidth().Padding(
                            0.0f, 0.0f, 4.0f, 0.0f)[make_size_button(
                            FText::FromString(TEXT("32 x 32")),
                            FOnClicked::CreateUObject(
                                this, &UHeatmapRDGEditorUtilityWidget::select_grid_size, 32))] +
                        SHorizontalBox::Slot().AutoWidth().Padding(
                            0.0f, 0.0f, 4.0f, 0.0f)[make_size_button(
                            FText::FromString(TEXT("64 x 64")),
                            FOnClicked::CreateUObject(
                                this, &UHeatmapRDGEditorUtilityWidget::select_grid_size, 64))] +
                        SHorizontalBox::Slot().AutoWidth().Padding(
                            0.0f, 0.0f, 4.0f, 0.0f)[make_size_button(
                            FText::FromString(TEXT("128 x 128")),
                            FOnClicked::CreateUObject(
                                this, &UHeatmapRDGEditorUtilityWidget::select_grid_size, 128))] +
                        SHorizontalBox::Slot().AutoWidth().Padding(
                            0.0f, 0.0f, 4.0f, 0.0f)[make_size_button(
                            FText::FromString(TEXT("256 x 256")),
                            FOnClicked::CreateUObject(
                                this, &UHeatmapRDGEditorUtilityWidget::select_grid_size, 256))] +
                        SHorizontalBox::Slot().AutoWidth()[make_size_button(
                            FText::FromString(TEXT("512 x 512")),
                            FOnClicked::CreateUObject(
                                this, &UHeatmapRDGEditorUtilityWidget::select_grid_size, 512))]] +
                   SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
                       [SNew(SHorizontalBox) +
                        SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
                            [SNew(SButton)
                                 .Text(NSLOCTEXT("HeatmapRDG", "Hotspots", "Hotspots"))
                                 .OnClicked_UObject(
                                     this, &UHeatmapRDGEditorUtilityWidget::show_hotspots)] +
                        SHorizontalBox::Slot().AutoWidth()
                            [SNew(SButton)
                                 .Text(NSLOCTEXT("HeatmapRDG", "Gradient", "Gradient + checker"))
                                 .OnClicked_UObject(
                                     this, &UHeatmapRDGEditorUtilityWidget::show_gradient)]] +
                   SVerticalBox::Slot().FillHeight(
                       1.0f)[SNew(SBox).MinDesiredWidth(512.0f).MinDesiredHeight(
                       512.0f)[heatmap_widget_->TakeWidget()]]];
}

auto UHeatmapRDGEditorUtilityWidget::select_grid_size(int32 grid_size) -> FReply {
    grid_size_ = grid_size;
    heatmap_widget_->generate_demo_grid(grid_size_, grid_size_);
    return FReply::Handled();
}

auto UHeatmapRDGEditorUtilityWidget::show_hotspots() -> FReply {
    heatmap_widget_->generate_demo_grid(grid_size_, grid_size_);
    return FReply::Handled();
}

auto UHeatmapRDGEditorUtilityWidget::show_gradient() -> FReply {
    generate_gradient_grid();
    return FReply::Handled();
}

void UHeatmapRDGEditorUtilityWidget::generate_gradient_grid() {
    FHeatmapGrid grid;
    grid.width = grid_size_;
    grid.height = grid_size_;
    grid.values.SetNumUninitialized(grid.width * grid.height);

    auto const width_denominator = static_cast<float>(grid.width - 1);
    auto const height_denominator = static_cast<float>(grid.height - 1);
    for (int32 y = 0; y < grid.height; ++y) {
        for (int32 x = 0; x < grid.width; ++x) {
            auto const gradient = (static_cast<float>(x) / width_denominator) *
                                  (static_cast<float>(y) / height_denominator);
            auto const checker = ((x / 8) + (y / 8)) % 2 == 0 ? 0.18f : 0.0f;
            grid.values[y * grid.width + x] = FMath::Clamp(gradient + checker, 0.0f, 1.0f);
        }
    }

    if (!heatmap_widget_->set_grid(MoveTemp(grid))) {
        UE_LOG(LogHeatmapRDGEditorUtility, Warning, TEXT("Failed to submit the gradient grid."));
    }
}
