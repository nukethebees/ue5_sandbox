#pragma once

#include "Blueprint/UserWidget.h"

#include "SandboxUIShowcase.generated.h"

namespace SlateGenerated {
struct USandboxUIShowcaseBuilder;
}

UCLASS()
class SANDBOXUIEXAMPLES_API USandboxUIShowcase : public UUserWidget {
    GENERATED_BODY()

    friend struct SlateGenerated::USandboxUIShowcaseBuilder;
  protected:
    TSharedRef<SWidget> RebuildWidget() override;
  private:
    void build_widget_tree();
    void populate_examples();
};
