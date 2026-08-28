#pragma once

#include "TutorialMaterialResource.h"

#include "Widgets/SCompoundWidget.h"

class SLesson02 : public SCompoundWidget {
  public:
    using ThisClass = SLesson02;

    SLATE_BEGIN_ARGS(SLesson02) {}
    SLATE_END_ARGS()

    void Construct(FArguments const& args);
  private:
    FTutorialMaterialResource coordinates_;
    FTutorialMaterialResource aspect_ratio_;
    FTutorialMaterialResource circles_;
    FTutorialMaterialResource boxes_;
    FTutorialMaterialResource grid_;
};
