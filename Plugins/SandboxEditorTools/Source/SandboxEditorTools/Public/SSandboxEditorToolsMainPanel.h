#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Widgets/SCompoundWidget.h"

class SObjectPropertyEntryBox;
class IDetailsView;

class SSandboxEditorToolsMainPanel : public SCompoundWidget {
  public:
    using ThisClass = SSandboxEditorToolsMainPanel;

    SLATE_BEGIN_ARGS(class SSandboxEditorToolsMainPanel) {}
    SLATE_END_ARGS()

    void Construct(FArguments const& args);
  protected:
    auto on_select_actor_button_clicked() -> FReply;

    TSharedPtr<SObjectPropertyEntryBox> actor_picker;
    TSharedPtr<IDetailsView> actor_picker_detail;
};
