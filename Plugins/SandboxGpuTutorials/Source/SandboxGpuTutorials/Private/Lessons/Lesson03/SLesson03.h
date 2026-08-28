#pragma once

#include "Lessons/Lesson03/Lesson03State.h"
#include "TutorialMaterialResource.h"

#include "Widgets/SCompoundWidget.h"

class SLesson03 : public SCompoundWidget {
  public:
    using ThisClass = SLesson03;

    SLATE_BEGIN_ARGS(SLesson03) {}
    SLATE_END_ARGS()

    void Construct(FArguments const& args);
    void Tick(FGeometry const& allotted_geometry, double current_time, float delta_time) override;
  private:
    void update_material_parameters();
    void on_ring_thickness_changed(float value);
    void on_animation_speed_changed(float value);
    void on_pulse_amount_changed(float value);
    void on_animation_enabled_changed(ECheckBoxState state);
    auto set_color(FLinearColor color) -> FReply;

    FTutorialMaterialResource material_;
    FLesson03State state_;
};
