#pragma once

#include "EditorUtilityWidget.h"
#include "Input/Reply.h"

#include "VolumeHeatmap3DShowcase.generated.h"

class SMultiLineEditableTextBox;

namespace SlateGenerated {
struct UVolumeHeatmap3DShowcaseBuilder;
}

UCLASS(Blueprintable)
class SBXUIEXPERIMENTS_API UVolumeHeatmap3DShowcase : public UEditorUtilityWidget {
    GENERATED_BODY()

    friend struct SlateGenerated::UVolumeHeatmap3DShowcaseBuilder;
  public:
    UVolumeHeatmap3DShowcase();
  protected:
    TSharedRef<SWidget> RebuildWidget() override;
  private:
    auto run_benchmark() -> FReply;

    TSharedPtr<SMultiLineEditableTextBox> benchmark_output_;
};
