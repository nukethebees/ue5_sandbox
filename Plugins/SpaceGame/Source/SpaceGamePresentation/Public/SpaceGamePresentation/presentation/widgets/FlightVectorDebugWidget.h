#pragma once

#include "SpaceGamePresentation/ui/style/GameUiStyle.h"
#include <ioj/presentation/flight_vector_debug_data.h>

#include <Blueprint/UserWidget.h>
#include <CoreMinimal.h>

#include "FlightVectorDebugWidget.generated.h"

class UUniformGridPanel;
namespace ioj {
class UVector2DWidget;

UCLASS()
class SPACEGAMEPRESENTATION_API UFlightVectorDebugWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    void update(presentation::FlightVectorDebugData const& data);
    void apply_hud_style(FGameHudStyle const& style);
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

    presentation::FlightVectorDebugData data_{};
    TOptional<FGameHudStyle> hud_style_{};
};

} // namespace ioj
