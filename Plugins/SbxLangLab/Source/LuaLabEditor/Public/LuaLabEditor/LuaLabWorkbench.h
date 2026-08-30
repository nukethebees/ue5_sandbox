#pragma once

#include "EditorUtilityWidget.h"

#include "LuaLabWorkbench.generated.h"

UCLASS(Blueprintable)
class LUALABEDITOR_API ULuaLabWorkbench final : public UEditorUtilityWidget {
    GENERATED_BODY()
  public:
    ULuaLabWorkbench();
  protected:
    TSharedRef<SWidget> RebuildWidget() override;
};
