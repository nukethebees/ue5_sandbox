#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Widgets/SCompoundWidget.h"

class STextBlock;
class SEditableTextBox;

class SSandboxEditorToolsMainPanel : public SCompoundWidget {
  public:
    using ThisClass = SSandboxEditorToolsMainPanel;

    SLATE_BEGIN_ARGS(class SSandboxEditorToolsMainPanel) {}
    SLATE_END_ARGS()

    void Construct(FArguments const& args);
  protected:
    auto on_select_actor_button_clicked() -> FReply;
    auto on_spawn_cursor_button_clicked() -> FReply;

    TSharedPtr<SEditableTextBox> selected_actor_name;
};
