#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "StatsMenuWidget.generated.h"

class UGridPanel;
class UHorizontalBox;

UCLASS()
class SANDBOX_API UStatsMenuWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    void on_widget_selected();
  protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))
    UHorizontalBox* stats_box{nullptr};

    UPROPERTY(meta = (BindWidget))
    UGridPanel* attributes_grid{nullptr};
    UPROPERTY(meta = (BindWidget))
    UGridPanel* tech_skills_grid{nullptr};
    UPROPERTY(meta = (BindWidget))
    UGridPanel* weapon_skills_grid{nullptr};
};
