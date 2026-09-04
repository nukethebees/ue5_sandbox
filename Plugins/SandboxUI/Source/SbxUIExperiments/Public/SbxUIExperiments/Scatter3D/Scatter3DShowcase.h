#pragma once

#include "EditorUtilityWidget.h"
#include "Input/Reply.h"

#include "Scatter3DShowcase.generated.h"

class SMultiLineEditableTextBox;

namespace SlateGenerated {
struct UScatter3DShowcaseBuilder;
}

UCLASS(Blueprintable)
class SBXUIEXPERIMENTS_API UScatter3DShowcase : public UEditorUtilityWidget {
    GENERATED_BODY()

    friend struct SlateGenerated::UScatter3DShowcaseBuilder;
  public:
    UScatter3DShowcase();
  protected:
    TSharedRef<SWidget> RebuildWidget() override;
  private:
    auto run_benchmark() -> FReply;

    TSharedPtr<SMultiLineEditableTextBox> benchmark_output_;
};
