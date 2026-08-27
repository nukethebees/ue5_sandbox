#pragma once

#include "EditorUtilityWidget.h"
#include "Input/Reply.h"

#include "Scatter3DShowcase.generated.h"

class SMultiLineEditableTextBox;

UCLASS(Blueprintable)
class EXPERIMENTS_API UScatter3DShowcase : public UEditorUtilityWidget {
    GENERATED_BODY()
  public:
    UScatter3DShowcase();
  protected:
    TSharedRef<SWidget> RebuildWidget() override;
  private:
    auto run_benchmark() -> FReply;

    TSharedPtr<SMultiLineEditableTextBox> benchmark_output_;
};
