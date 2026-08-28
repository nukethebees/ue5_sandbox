#include "Showcase/SSandboxGpuTutorialsShowcase.h"

#include "Lessons/Lesson01/SLesson01.h"
#include "Lessons/Lesson02/SLesson02.h"
#include "Lessons/Lesson03/SLesson03.h"

#include "HAL/PlatformProcess.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

DEFINE_LOG_CATEGORY_STATIC(LogSandboxGpuTutorialShowcase, Log, All);

namespace {
auto open_plugin_file(TCHAR const* const relative_path) -> FReply {
    auto const plugin{IPluginManager::Get().FindPlugin(TEXT("SandboxGpuTutorials"))};
    if (!plugin.IsValid()) {
        UE_LOG(LogSandboxGpuTutorialShowcase, Error, TEXT("SandboxGpuTutorials plugin not found."));
        return FReply::Handled();
    }

    auto const path{FPaths::ConvertRelativePathToFull(plugin->GetBaseDir(), relative_path)};
    if (!FPaths::FileExists(path)) {
        UE_LOG(LogSandboxGpuTutorialShowcase, Error, TEXT("Tutorial file not found: %s"), *path);
        return FReply::Handled();
    }

    FPlatformProcess::LaunchFileInDefaultExternalApplication(*path);
    return FReply::Handled();
}
}

void SSandboxGpuTutorialsShowcase::Construct(FArguments const&) {
    auto lesson_button{[this](TCHAR const* label, int32 const lesson_index) {
        return SNew(SButton)
            .ContentPadding(FMargin{12.0f, 6.0f})
            .OnClicked(this,
                       &ThisClass::select_lesson,
                       lesson_index)[SNew(STextBlock).Text(FText::FromString(label))];
    }};

    ChildSlot[SNew(SVerticalBox) +
              SVerticalBox::Slot().AutoHeight().Padding(
                  8.0f)[SNew(SHorizontalBox) +
                        SHorizontalBox::Slot().AutoWidth().Padding(
                            2.0f)[lesson_button(TEXT("01  Mental Model"), 0)] +
                        SHorizontalBox::Slot().AutoWidth().Padding(
                            2.0f)[lesson_button(TEXT("02  Coordinates and Shapes"), 1)] +
                        SHorizontalBox::Slot().AutoWidth().Padding(
                            2.0f)[lesson_button(TEXT("03  Parameters and Animation"), 2)]] +
              SVerticalBox::Slot().AutoHeight().Padding(10.0f, 0.0f, 10.0f, 4.0f)
                  [SNew(SHorizontalBox) +
                   SHorizontalBox::Slot().AutoWidth().Padding(
                       2.0f)[SNew(SButton)
                                 .Text(FText::FromString(TEXT("Open lesson README")))
                                 .OnClicked(this, &ThisClass::open_lesson_documentation)] +
                   SHorizontalBox::Slot().AutoWidth().Padding(
                       2.0f)[SNew(SButton)
                                 .Text(FText::FromString(TEXT("Open HLSL source")))
                                 .OnClicked(this, &ThisClass::open_lesson_shader)]] +
              SVerticalBox::Slot().FillHeight(1.0f).Padding(8.0f)[SNew(SBorder).Padding(
                  12.0f)[SNew(SScrollBox).Orientation(Orient_Vertical) +
                         SScrollBox::Slot()
                             [SNew(SScrollBox).Orientation(Orient_Horizontal) +
                              SScrollBox::Slot()[SAssignNew(lesson_switcher_, SWidgetSwitcher) +
                                                 SWidgetSwitcher::Slot()[SNew(SLesson01)] +
                                                 SWidgetSwitcher::Slot()[SNew(SLesson02)] +
                                                 SWidgetSwitcher::Slot()[SNew(SLesson03)]]]]]];
}

auto SSandboxGpuTutorialsShowcase::select_lesson(int32 const lesson_index) -> FReply {
    if (lesson_switcher_.IsValid()) {
        lesson_switcher_->SetActiveWidgetIndex(lesson_index);
        selected_lesson_index_ = lesson_index;
    }
    return FReply::Handled();
}

auto SSandboxGpuTutorialsShowcase::open_lesson_documentation() const -> FReply {
    TCHAR const* const paths[]{TEXT("Docs/Lesson01/README.md"),
                               TEXT("Docs/Lesson02/README.md"),
                               TEXT("Docs/Lesson03/README.md")};
    return open_plugin_file(paths[selected_lesson_index_]);
}

auto SSandboxGpuTutorialsShowcase::open_lesson_shader() const -> FReply {
    TCHAR const* const paths[]{TEXT("Shaders/Lesson01/Lesson01.ush"),
                               TEXT("Shaders/Lesson02/Lesson02.ush"),
                               TEXT("Shaders/Lesson03/Lesson03.ush")};
    return open_plugin_file(paths[selected_lesson_index_]);
}
