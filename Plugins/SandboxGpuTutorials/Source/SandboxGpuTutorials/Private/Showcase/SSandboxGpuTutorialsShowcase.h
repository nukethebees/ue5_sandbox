#pragma once

#include "Widgets/SCompoundWidget.h"

class SWidgetSwitcher;

class SSandboxGpuTutorialsShowcase : public SCompoundWidget {
  public:
    using ThisClass = SSandboxGpuTutorialsShowcase;

    SLATE_BEGIN_ARGS(SSandboxGpuTutorialsShowcase) {}
    SLATE_END_ARGS()

    void Construct(FArguments const& args);
  private:
    auto select_lesson(int32 lesson_index) -> FReply;
    auto open_lesson_documentation() const -> FReply;
    auto open_lesson_shader() const -> FReply;

    TSharedPtr<SWidgetSwitcher> lesson_switcher_;
    int32 selected_lesson_index_{0};
};
