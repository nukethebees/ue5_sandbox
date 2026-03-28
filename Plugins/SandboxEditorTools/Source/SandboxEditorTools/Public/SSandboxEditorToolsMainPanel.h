#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SSandboxEditorToolsMainPanel : public SCompoundWidget {
  public:
    SLATE_BEGIN_ARGS(class SSandboxEditorToolsMainPanel) {}
    SLATE_END_ARGS()

    void Construct(FArguments const& args);
};
