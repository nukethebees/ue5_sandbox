#pragma once

#include "Blueprint/UserWidget.h"

#include "SandboxUIShowcase.generated.h"

UCLASS()
class SANDBOXUIEXAMPLES_API USandboxUIShowcase : public UUserWidget {
    GENERATED_BODY()
  protected:
    TSharedRef<SWidget> RebuildWidget() override;
  private:
    void build_widget_tree();
    void populate_examples();
};
