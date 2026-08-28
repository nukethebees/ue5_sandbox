#pragma once

#include "EditorUtilityWidget.h"
#include "Input/Reply.h"

#include "VolumeHeatmap3DShowcase.generated.h"

class SMultiLineEditableTextBox;

UCLASS(Blueprintable)
class EXPERIMENTS_API UVolumeHeatmap3DShowcase : public UEditorUtilityWidget {
    GENERATED_BODY()
  public:
    UVolumeHeatmap3DShowcase();
  protected:
    TSharedRef<SWidget> RebuildWidget() override;
  private:
    auto run_benchmark() -> FReply;

    TSharedPtr<SMultiLineEditableTextBox> benchmark_output_;
};
