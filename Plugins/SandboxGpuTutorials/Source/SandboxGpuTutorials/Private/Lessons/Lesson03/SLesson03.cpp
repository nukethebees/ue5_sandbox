#include "Lessons/Lesson03/SLesson03.h"

#include "Materials/MaterialInstanceDynamic.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace {
auto make_control(TCHAR const* const label, TSharedRef<SWidget> control) -> TSharedRef<SWidget> {
    return SNew(SHorizontalBox) +
           SHorizontalBox::Slot().FillWidth(0.35f).VAlign(
               VAlign_Center)[SNew(STextBlock).Text(FText::FromString(label))] +
           SHorizontalBox::Slot().FillWidth(0.65f).VAlign(VAlign_Center)[control];
}
}

void SLesson03::Construct(FArguments const&) {
    FVector2D const preview_size{520.0f, 520.0f};
    auto const material_loaded{
        material_.load(TEXT("/SandboxGpuTutorials/Showcases/Materials/Lesson03/"
                            "M_Lesson03_AnimatedRings.M_Lesson03_AnimatedRings"),
                       preview_size,
                       true)};
    update_material_parameters();
    SetCanTick(material_loaded);

    auto const preview{
        material_loaded ? StaticCastSharedRef<SWidget>(SNew(SImage).Image(material_.get_brush()))
                        : StaticCastSharedRef<SWidget>(
                              SNew(STextBlock)
                                  .Text(FText::FromString(TEXT(
                                      "Lesson 03 material failed to load. See the Output Log."))))};

    auto const color_buttons{
        SNew(SHorizontalBox) +
        SHorizontalBox::Slot().AutoWidth().Padding(
            2.0f)[SNew(SButton).OnClicked(
            this, &ThisClass::set_color, FLinearColor{0.10f, 0.85f, 1.0f})
                      [SNew(STextBlock).Text(FText::FromString(TEXT("Cyan")))]] +
        SHorizontalBox::Slot().AutoWidth().Padding(
            2.0f)[SNew(SButton).OnClicked(
            this, &ThisClass::set_color, FLinearColor{1.0f, 0.55f, 0.08f})
                      [SNew(STextBlock).Text(FText::FromString(TEXT("Amber")))]] +
        SHorizontalBox::Slot().AutoWidth().Padding(
            2.0f)[SNew(SButton).OnClicked(
            this, &ThisClass::set_color, FLinearColor{0.95f, 0.16f, 0.80f})
                      [SNew(STextBlock).Text(FText::FromString(TEXT("Magenta")))]]};

    ChildSlot
        [SNew(SVerticalBox) +
         SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
             [SNew(STextBlock)
                  .Text(FText::FromString(TEXT("Lesson 03 — Parameters and Animation")))] +
         SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 16.0f)
             [SNew(STextBlock)
                  .AutoWrapText(true)
                  .Text(FText::FromString(
                      TEXT("The CPU changes a handful of material parameters. Slate keeps "
                           "submitting the same quad; no shape vertices are regenerated.")))] +
         SVerticalBox::Slot().AutoHeight()
             [SNew(SHorizontalBox) +
              SHorizontalBox::Slot().AutoWidth().Padding(
                  4.0f)[SNew(SBox)
                            .WidthOverride(preview_size.X)
                            .HeightOverride(preview_size.Y)[preview]] +
              SHorizontalBox::Slot().FillWidth(1.0f).Padding(20.0f, 4.0f)
                  [SNew(SVerticalBox) +
                   SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f)[make_control(
                       TEXT("Animate"),
                       SNew(SCheckBox)
                           .IsChecked_Lambda([this]() {
                               return state_.animation_enabled ? ECheckBoxState::Checked
                                                               : ECheckBoxState::Unchecked;
                           })
                           .OnCheckStateChanged(this, &ThisClass::on_animation_enabled_changed))] +
                   SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f)[make_control(
                       TEXT("Ring thickness"),
                       SNew(SHorizontalBox) +
                           SHorizontalBox::Slot().FillWidth(1.0f)
                               [SNew(SSlider)
                                    .Value_Lambda(
                                        [this]() { return state_.ring_thickness_slider_value(); })
                                    .OnValueChanged(this, &ThisClass::on_ring_thickness_changed)] +
                           SHorizontalBox::Slot().AutoWidth().Padding(
                               8.0f, 0.0f)[SNew(STextBlock).Text_Lambda([this]() {
                               return FText::FromString(
                                   FString::Printf(TEXT("%.3f"), state_.ring_thickness));
                           })])] +
                   SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f)[make_control(
                       TEXT("Animation speed"),
                       SNew(SHorizontalBox) +
                           SHorizontalBox::Slot().FillWidth(1.0f)
                               [SNew(SSlider)
                                    .Value_Lambda(
                                        [this]() { return state_.animation_speed_slider_value(); })
                                    .OnValueChanged(this, &ThisClass::on_animation_speed_changed)] +
                           SHorizontalBox::Slot().AutoWidth().Padding(
                               8.0f, 0.0f)[SNew(STextBlock).Text_Lambda([this]() {
                               return FText::FromString(
                                   FString::Printf(TEXT("%.2f"), state_.animation_speed));
                           })])] +
                   SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f)[make_control(
                       TEXT("Pulse amount"),
                       SNew(SHorizontalBox) +
                           SHorizontalBox::Slot().FillWidth(1.0f)
                               [SNew(SSlider)
                                    .Value_Lambda(
                                        [this]() { return state_.pulse_amount_slider_value(); })
                                    .OnValueChanged(this, &ThisClass::on_pulse_amount_changed)] +
                           SHorizontalBox::Slot().AutoWidth().Padding(
                               8.0f, 0.0f)[SNew(STextBlock).Text_Lambda([this]() {
                               return FText::FromString(
                                   FString::Printf(TEXT("%.3f"), state_.pulse_amount));
                           })])] +
                   SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 4.0f)
                       [SNew(STextBlock).Text(FText::FromString(TEXT("Primary colour")))] +
                   SVerticalBox::Slot().AutoHeight()[color_buttons]]]];
}

void SLesson03::Tick(FGeometry const& allotted_geometry,
                     double const current_time,
                     float const delta_time) {
    SCompoundWidget::Tick(allotted_geometry, current_time, delta_time);
    if (!state_.animation_enabled) {
        return;
    }

    state_.advance(delta_time);
    if (auto* const dynamic_material{material_.get_dynamic_material()}) {
        dynamic_material->SetScalarParameterValue(TEXT("TimeSeconds"), state_.elapsed_time);
        Invalidate(EInvalidateWidgetReason::Paint);
    }
}

void SLesson03::update_material_parameters() {
    auto* const dynamic_material{material_.get_dynamic_material()};
    if (dynamic_material == nullptr) {
        return;
    }

    dynamic_material->SetScalarParameterValue(TEXT("TimeSeconds"), state_.elapsed_time);
    dynamic_material->SetScalarParameterValue(TEXT("RingThickness"), state_.ring_thickness);
    dynamic_material->SetScalarParameterValue(TEXT("AnimationSpeed"), state_.animation_speed);
    dynamic_material->SetScalarParameterValue(TEXT("PulseAmount"), state_.pulse_amount);
    dynamic_material->SetVectorParameterValue(TEXT("PrimaryColor"), state_.primary_color);
}

void SLesson03::on_ring_thickness_changed(float const value) {
    state_.set_ring_thickness_from_slider(value);
    update_material_parameters();
}

void SLesson03::on_animation_speed_changed(float const value) {
    state_.set_animation_speed_from_slider(value);
    update_material_parameters();
}

void SLesson03::on_pulse_amount_changed(float const value) {
    state_.set_pulse_amount_from_slider(value);
    update_material_parameters();
}

void SLesson03::on_animation_enabled_changed(ECheckBoxState const state) {
    state_.animation_enabled = state == ECheckBoxState::Checked;
}

auto SLesson03::set_color(FLinearColor const color) -> FReply {
    state_.primary_color = color;
    update_material_parameters();
    return FReply::Handled();
}
