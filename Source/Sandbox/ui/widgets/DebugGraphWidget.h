#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "DebugGraphWidget.generated.h"

class SGraphPlot;

UCLASS()
class SANDBOX_API UDebugGraphWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    void set_samples(TConstArrayView<FVector2d> in_samples, int32 oldest_index);
  protected:
    TSharedRef<SWidget> RebuildWidget() override;
    void ReleaseSlateResources(bool release_children) override;
  private:
    void update_slate_series();

    TArray<float> x_;
    TArray<float> y_;
    TSharedPtr<SGraphPlot> graph_widget_;
};
