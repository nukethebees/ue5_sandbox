#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "Sandbox/logging/mixins/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"

#include "StatsMenuWidget.generated.h"

class UGridPanel;
class UHorizontalBox;
class UTextBlock;

class AMyCharacter;

UCLASS()
class SANDBOX_API UStatsMenuWidget
    : public UUserWidget
    , public ml::LogMsgMixin<"UStatsMenuWidget", LogSandboxUI> {
    GENERATED_BODY()
  public:
    void on_widget_selected();
    void set_character(AMyCharacter& my_char);
  protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))
    UHorizontalBox* stats_box{nullptr};

    // Attributes
    UPROPERTY(meta = (BindWidget))
    UGridPanel* attributes_grid{nullptr};

    UPROPERTY(meta = (BindWidget))
    UTextBlock* strength_label{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* strength_value{nullptr};

    UPROPERTY(meta = (BindWidget))
    UTextBlock* endurance_label{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* endurance_value{nullptr};

    UPROPERTY(meta = (BindWidget))
    UTextBlock* agility_label{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* agility_value{nullptr};

    UPROPERTY(meta = (BindWidget))
    UTextBlock* cyber_label{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* cyber_value{nullptr};

    UPROPERTY(meta = (BindWidget))
    UTextBlock* psi_label{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* psi_value{nullptr};

    // Tech skills
    UPROPERTY(meta = (BindWidget))
    UGridPanel* tech_skills_grid{nullptr};

    UPROPERTY(meta = (BindWidget))
    UTextBlock* hacking_label{nullptr};
    UPROPERTY(meta = (BindWidget))
    UTextBlock* hacking_value{nullptr};

    // Weapon skills
    UPROPERTY(meta = (BindWidget))
    UGridPanel* weapon_skills_grid{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TWeakObjectPtr<AMyCharacter> character{nullptr};
};
