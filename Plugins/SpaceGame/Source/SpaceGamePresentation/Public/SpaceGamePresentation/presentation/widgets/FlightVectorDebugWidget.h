#pragma once

#include "SpaceGamePresentation/presentation/widgets/FlightVectorDebugData.h"
#include "SpaceGamePresentation/ui/style/GameUiStyle.h"

#include <Blueprint/UserWidget.h>
#include <CoreMinimal.h>

#include "FlightVectorDebugWidget.generated.h"

class UUniformGridPanel;
class UVector2DWidget;

UCLASS()
class SPACEGAMEPRESENTATION_API UFlightVectorDebugWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    void update(ml::ship_hud::FFlightVectorDebugData const& data);
    void apply_hud_style(ml::ioj::FGameHudStyle const& style);
    void set_font_size(int32 new_font_size);
    auto get_font_size() const noexcept -> int32 { return font_size; }
  protected:
    void NativePreConstruct() override;
  private:
    void rebuild_widget_tree();
    void update_widgets();
    void configure_vector_widget(UVector2DWidget*& widget, FName name, FText label);

    UPROPERTY(Transient)
    UUniformGridPanel* vector_grid_{nullptr};
    UPROPERTY(Transient)
    UVector2DWidget* turn_input_widget_{nullptr};
    UPROPERTY(Transient)
    UVector2DWidget* move_input_widget_{nullptr};
    UPROPERTY(Transient)
    UVector2DWidget* target_velocity_widget_{nullptr};
    UPROPERTY(Transient)
    UVector2DWidget* local_velocity_widget_{nullptr};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "UI",
              meta = (AllowPrivateAccess = "true"))
    int32 font_size{24};

    ml::ship_hud::FFlightVectorDebugData data_{};
    TOptional<ml::ioj::FGameHudStyle> hud_style_{};
};
