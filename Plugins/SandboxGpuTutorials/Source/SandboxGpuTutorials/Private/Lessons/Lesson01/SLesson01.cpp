#include "Lessons/Lesson01/SLesson01.h"

#include "Rendering/DrawElements.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace ml::gpu_tutorials::lesson_01 {
FVector2D const preview_size{360.0f, 220.0f};

auto make_caption(TCHAR const* const heading, TCHAR const* const body) -> TSharedRef<SWidget> {
    return SNew(SVerticalBox) +
           SVerticalBox::Slot().AutoHeight().Padding(
               0.0f, 0.0f, 0.0f, 4.0f)[SNew(STextBlock).Text(FText::FromString(heading))] +
           SVerticalBox::Slot()
               .AutoHeight()[SNew(STextBlock).AutoWrapText(true).Text(FText::FromString(body))];
}
}

auto SLesson01SlateGradient::OnPaint(FPaintArgs const&,
                                     FGeometry const& allotted_geometry,
                                     FSlateRect const&,
                                     FSlateWindowElementList& out_draw_elements,
                                     int32 const layer_id,
                                     FWidgetStyle const&,
                                     bool const) const -> int32 {
    auto const size{allotted_geometry.GetLocalSize()};
    TArray<FSlateGradientStop> stops{
        FSlateGradientStop{FVector2D{0.0f, 0.0f}, FLinearColor{0.02f, 0.08f, 0.15f}},
        FSlateGradientStop{FVector2D{size.X, 0.0f}, FLinearColor{0.05f, 0.90f, 0.70f}},
    };
    FSlateDrawElement::MakeGradient(out_draw_elements,
                                    layer_id,
                                    allotted_geometry.ToPaintGeometry(),
                                    MoveTemp(stops),
                                    Orient_Horizontal);
    return layer_id;
}

auto SLesson01SlateGradient::ComputeDesiredSize(float const) const -> FVector2D {
    return ml::gpu_tutorials::lesson_01::preview_size;
}

void SLesson01::Construct(FArguments const&) {
    auto const material_loaded{
        gradient_material_.load(TEXT("/SandboxGpuTutorials/Showcases/Materials/Lesson01/"
                                     "M_Lesson01_Gradient.M_Lesson01_Gradient"),
                                ml::gpu_tutorials::lesson_01::preview_size)};

    auto const material_preview{
        material_loaded
            ? StaticCastSharedRef<SWidget>(SNew(SImage).Image(gradient_material_.get_brush()))
            : StaticCastSharedRef<SWidget>(
                  SNew(STextBlock)
                      .Text(FText::FromString(TEXT("Lesson 01 material failed to load. "
                                                   "See the Output Log."))))};

    ChildSlot[SNew(SVerticalBox) +
              SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
                  [SNew(STextBlock)
                       .Text(FText::FromString(TEXT("Lesson 01 — GPU Rendering Mental Model")))] +
              SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 16.0f)
                  [SNew(STextBlock)
                       .AutoWrapText(true)
                       .Text(FText::FromString(TEXT(
                           "Both panels submit a small Slate draw description on the editor "
                           "thread. Slate batches a quad, then the render thread and RHI issue "
                           "GPU work. The right panel selects generated material shader code "
                           "instead of Slate's built-in gradient shader.")))] +
              SVerticalBox::Slot().AutoHeight()
                  [SNew(SHorizontalBox) +
                   SHorizontalBox::Slot().AutoWidth().Padding(
                       4.0f)[SNew(SVerticalBox) +
                             SVerticalBox::Slot().AutoHeight().Padding(
                                 0.0f, 0.0f, 0.0f, 6.0f)[ml::gpu_tutorials::lesson_01::make_caption(
                                 TEXT("Conventional Slate gradient"),
                                 TEXT("CPU: two gradient stops. GPU: Slate gradient shader."))] +
                             SVerticalBox::Slot().AutoHeight()
                                 [SNew(SBox)
                                      .WidthOverride(ml::gpu_tutorials::lesson_01::preview_size.X)
                                      .HeightOverride(ml::gpu_tutorials::lesson_01::preview_size
                                                          .Y)[SNew(SLesson01SlateGradient)]]] +
                   SHorizontalBox::Slot().AutoWidth().Padding(
                       4.0f)[SNew(SVerticalBox) +
                             SVerticalBox::Slot().AutoHeight().Padding(
                                 0.0f, 0.0f, 0.0f, 6.0f)[ml::gpu_tutorials::lesson_01::make_caption(
                                 TEXT("UI material gradient"),
                                 TEXT("CPU: material brush. GPU: custom pixel math from "
                                      "Lesson01.ush."))] +
                             SVerticalBox::Slot().AutoHeight()
                                 [SNew(SBox)
                                      .WidthOverride(ml::gpu_tutorials::lesson_01::preview_size.X)
                                      .HeightOverride(ml::gpu_tutorials::lesson_01::preview_size
                                                          .Y)[material_preview]]]]];
}
