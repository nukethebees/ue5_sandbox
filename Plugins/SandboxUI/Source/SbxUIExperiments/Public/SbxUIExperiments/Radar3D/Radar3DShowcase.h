#pragma once

#include "EditorUtilityWidget.h"
#include "Input/Reply.h"

#include "Radar3DShowcase.generated.h"

class SMultiLineEditableTextBox;

namespace SlateGenerated {
struct URadar3DShowcaseBuilder;
}

UCLASS(Blueprintable)
class SBXUIEXPERIMENTS_API URadar3DShowcase : public UEditorUtilityWidget {
    GENERATED_BODY()

    friend struct SlateGenerated::URadar3DShowcaseBuilder;
  public:
    URadar3DShowcase();
  protected:
    TSharedRef<SWidget> RebuildWidget() override;
  private:
    auto run_benchmark() -> FReply;

    TSharedPtr<SMultiLineEditableTextBox> benchmark_output_;
};
