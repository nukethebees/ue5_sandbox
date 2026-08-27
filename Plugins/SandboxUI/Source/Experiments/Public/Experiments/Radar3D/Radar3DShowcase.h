#pragma once

#include "EditorUtilityWidget.h"

#include "Radar3DShowcase.generated.h"

UCLASS(Blueprintable)
class EXPERIMENTS_API URadar3DShowcase : public UEditorUtilityWidget {
    GENERATED_BODY()
  public:
    URadar3DShowcase();
  protected:
    TSharedRef<SWidget> RebuildWidget() override;
};
