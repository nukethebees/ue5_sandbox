#pragma once

#include "SandboxUI/EntityOverlay/EntityOverlayFrameStore.h"
#include "SandboxUI/EntityOverlay/EntityOverlayTypes.h"
#include "SpaceGame/entities/TestEntityRegistry.h"
#include "SpaceGame/entities/TestTeamVisualData.h"

#include <Blueprint/UserWidget.h>
#include <CoreMinimal.h>

#include "SimulationHudWidget.generated.h"

class UForceStatusWidget;
class UMissionStatusWidget;
class SEntityOverlayWidget;

namespace ml::hud_manager {
struct FMissionDataCache;
}
namespace ml::ioj {
class FGameUiStyle;
}

UCLASS(Abstract)
class SPACEGAME_API USimulationHudWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    virtual void apply_ui_style(ml::ioj::FGameUiStyle const& style);
    void set_entity_counts(FTestEntityRegistry::EntityCounts const& counts);
    void set_entity_colours(UTestTeamVisualData::FColourArray const& colours);
    void set_mission_data(ml::hud_manager::FMissionDataCache const& data);

    void set_entity_overlay_frame_store(FEntityOverlayFrameStoreConstPtr frame_store);
    void set_entity_overlay_style(FEntityOverlayStyle const& style);
    [[nodiscard]] auto try_get_entity_overlay_view(FEntityOverlayView& view) const -> bool;
  protected:
    auto RebuildWidget() -> TSharedRef<SWidget> override;
    void ReleaseSlateResources(bool release_children) override;
    void NativeTick(FGeometry const& geometry, float delta_time) override;

    UPROPERTY(meta = (BindWidget))
    UForceStatusWidget* force_status_widget{nullptr};

    UPROPERTY(meta = (BindWidget))
    UMissionStatusWidget* mission_status_panel{nullptr};

    [[nodiscard]] auto has_ui_style() const -> bool { return has_ui_style_; }
    [[nodiscard]] auto entity_overlay_defend_colour() const -> FLinearColor {
        return entity_overlay_defend_colour_;
    }
  private:
    void apply_entity_overlay_colours();

    FEntityOverlayFrameStoreConstPtr entity_overlay_frame_store_;
    FEntityOverlayStyle entity_overlay_style_;
    TSharedPtr<SEntityOverlayWidget> entity_overlay_widget_;
    FLinearColor entity_overlay_background_colour_{};
    FLinearColor entity_overlay_fill_colour_{};
    FLinearColor entity_overlay_defend_colour_{};
    FLinearColor entity_overlay_destroy_colour_{};
    FLinearColor entity_overlay_soft_target_neutral_colour_{};
    FLinearColor entity_overlay_soft_target_in_range_colour_{};
    bool has_ui_style_{};
};
