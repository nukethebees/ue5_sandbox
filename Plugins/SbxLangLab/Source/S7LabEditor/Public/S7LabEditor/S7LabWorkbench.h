#pragma once

#include "EditorUtilityWidget.h"

#include "S7LabWorkbench.generated.h"

UCLASS(Blueprintable)
class S7LABEDITOR_API US7LabWorkbench final : public UEditorUtilityWidget {
    GENERATED_BODY()
  public:
    US7LabWorkbench();
  protected:
    TSharedRef<SWidget> RebuildWidget() override;
};
