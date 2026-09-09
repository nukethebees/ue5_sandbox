#pragma once

#include <SpaceGameSimulation/entities/TestEntityType.h>
#include <SpaceGameSimulation/entities/TestEntityUniqueId.h>
#include <SpaceGameSimulation/ships/common/ShipHealth.h>

#include <Blueprint/UserWidget.h>
#include <CoreMinimal.h>

#include "MissionEntityHealthRowWidget.generated.h"

class UHorizontalBox;
class UShipHealthWidget;
class UTextBlock;
namespace ml::ioj {
struct FGameHudStyle;
}

UCLASS()
class SPACEGAMEPRESENTATION_API UMissionEntityHealthRowWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    void set_entity(TestEntityUniqueId unique_id, ETestEntityType entity_type);
    void set_health(FShipHealth health);
    void apply_hud_style(ml::ioj::FGameHudStyle const& style);
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
