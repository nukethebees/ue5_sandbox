#pragma once

#include <Sandbox/batch_game/test_entity_registry/TestEntityUniqueId.h>
#include <Sandbox/batch_game/TestEntityType.h>
#include <Sandbox/health/ShipHealth.h>

#include <Blueprint/UserWidget.h>
#include <CoreMinimal.h>

#include "MissionEntityHealthRowWidget.generated.h"

class UHorizontalBox;
class UShipHealthWidget;
class UTextBlock;

UCLASS()
class SANDBOX_API UMissionEntityHealthRowWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    void set_entity(TestEntityUniqueId unique_id, ETestEntityType entity_type);
    void set_health(FShipHealth health);
    void set_font_size(int32 const new_font_size);
    auto get_font_size() const noexcept -> int32 { return font_size; }
  protected:
    void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))
    UHorizontalBox* row_box{nullptr};

    UPROPERTY(meta = (BindWidget))
    UTextBlock* entity_name{nullptr};

    UPROPERTY(meta = (BindWidget))
    UShipHealthWidget* health_widget{nullptr};

    UPROPERTY(EditAnywhere, Category = "UI")
    int32 font_size{24};
  private:
    auto check_widget_bindings() const -> bool;
};
