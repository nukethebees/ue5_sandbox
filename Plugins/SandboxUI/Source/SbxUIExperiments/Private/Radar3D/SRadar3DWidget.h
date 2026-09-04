#pragma once

#include "Radar3DRenderer.h"

#include "Containers/Array.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Styling/SlateBrush.h"
#include "Templates/SharedPointer.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"

class SImage;
class SRadar3DWidget final : public SCompoundWidget {
  public:
    static constexpr int32 radar_3d_output_texture_dimension{512};
    static constexpr int32 initial_contact_count{5};

    SLATE_BEGIN_ARGS(SRadar3DWidget) {}
    SLATE_END_ARGS()

    void Construct(FArguments const& args);
    ~SRadar3DWidget() override;

    void set_contact_count(int32 contact_count);
    void Tick(FGeometry const& allotted_geometry, double current_time, float delta_time) override;
  private:
    void initialise_contacts(int32 contact_count);
    [[nodiscard]] auto initialise_output_texture() -> bool;
    void submit_render();

    FRadar3DRenderer renderer_;
    TArray<FRadar3DContact> contacts_;
    TStrongObjectPtr<UTextureRenderTarget2D> output_texture_;
    FSlateBrush brush_;
    TSharedPtr<SImage> image_;
    float elapsed_time_{0.0f};
    bool output_ready_{false};
};
