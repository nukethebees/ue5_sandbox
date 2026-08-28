#include "Lessons/Lesson02/SLesson02.h"

#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace {
auto make_preview_card(TCHAR const* const title,
                       TCHAR const* const description,
                       FSlateBrush const* const brush,
                       FVector2D const size) -> TSharedRef<SWidget> {
    auto const preview{
        brush != nullptr
            ? StaticCastSharedRef<SWidget>(SNew(SImage).Image(brush))
            : StaticCastSharedRef<SWidget>(
                  SNew(STextBlock).Text(FText::FromString(TEXT("Material failed to load."))))};

    return SNew(SVerticalBox) +
           SVerticalBox::Slot().AutoHeight().Padding(
               0.0f, 0.0f, 0.0f, 3.0f)[SNew(STextBlock).Text(FText::FromString(title))] +
           SVerticalBox::Slot().AutoHeight().Padding(
               0.0f, 0.0f, 0.0f, 6.0f)[SNew(STextBlock)
                                           .AutoWrapText(true)
                                           .WrapTextAt(size.X)
                                           .Text(FText::FromString(description))] +
           SVerticalBox::Slot()
               .AutoHeight()[SNew(SBox).WidthOverride(size.X).HeightOverride(size.Y)[preview]];
}
}

void SLesson02::Construct(FArguments const&) {
    FVector2D const square_size{260.0f, 260.0f};
    FVector2D const wide_size{520.0f, 260.0f};
    auto const base_path{TEXT("/SandboxGpuTutorials/Showcases/Materials/Lesson02/")};

    coordinates_.load(*(FString{base_path} + TEXT("M_Lesson02_Coordinates.M_Lesson02_Coordinates")),
                      square_size);
    aspect_ratio_.load(
        *(FString{base_path} + TEXT("M_Lesson02_AspectRatio.M_Lesson02_AspectRatio")), wide_size);
    circles_.load(*(FString{base_path} + TEXT("M_Lesson02_Circles.M_Lesson02_Circles")),
                  square_size);
    boxes_.load(*(FString{base_path} + TEXT("M_Lesson02_Boxes.M_Lesson02_Boxes")), square_size);
    grid_.load(*(FString{base_path} + TEXT("M_Lesson02_Grid.M_Lesson02_Grid")), square_size);

    ChildSlot[SNew(SVerticalBox) +
              SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
                  [SNew(STextBlock)
                       .Text(FText::FromString(
                           TEXT("Lesson 02 — Shader Coordinates and Analytic Shapes")))] +
              SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 16.0f)
                  [SNew(STextBlock)
                       .AutoWrapText(true)
                       .Text(FText::FromString(
                           TEXT("Each card is still one quad. Its pixel shader derives every line, "
                                "edge, circle, and fill from interpolated UV coordinates.")))] +
              SVerticalBox::Slot()
                  .AutoHeight()[SNew(SHorizontalBox) +
                                SHorizontalBox::Slot().AutoWidth().Padding(4.0f)[make_preview_card(
                                    TEXT("UVs, remapping, interpolation, frac"),
                                    TEXT("Colour exposes normalized UVs; repeated grid lines "
                                         "come from frac; centered axes show [0,1] to [-1,1]."),
                                    coordinates_.get_brush(),
                                    square_size)] +
                                SHorizontalBox::Slot().AutoWidth().Padding(4.0f)[make_preview_card(
                                    TEXT("Aspect-ratio correction"),
                                    TEXT("Left: UV distance stretches with the 2:1 target. "
                                         "Right: scaling X by aspect restores physical distance."),
                                    aspect_ratio_.get_brush(),
                                    wide_size)]] +
              SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f)
                  [SNew(SHorizontalBox) +
                   SHorizontalBox::Slot().AutoWidth().Padding(4.0f)[make_preview_card(
                       TEXT("step, smoothstep, circle, ring"),
                       TEXT("Hard threshold on the left; derivative-smoothed ring "
                            "on the right."),
                       circles_.get_brush(),
                       square_size)] +
                   SHorizontalBox::Slot().AutoWidth().Padding(4.0f)[make_preview_card(
                       TEXT("Analytic boxes"),
                       TEXT("max(abs(p) - half_extent) is a compact box signed "
                            "distance field."),
                       boxes_.get_brush(),
                       square_size)] +
                   SHorizontalBox::Slot().AutoWidth().Padding(4.0f)[make_preview_card(
                       TEXT("Lines and grid"),
                       TEXT("Distance to axes and repeated frac cells create a "
                            "crosshair and grid without line geometry."),
                       grid_.get_brush(),
                       square_size)]]];
}
