#pragma once

#include "CoreMinimal.h"
#include "Editor/GenLabSettings.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"

class IDetailsView;
class SGenLab final : public SCompoundWidget {
  public:
    SLATE_BEGIN_ARGS(SGenLab) {}
    SLATE_END_ARGS()

    void Construct(FArguments const& arguments);
    ~SGenLab() override;
  private:
    void on_property_changed(FPropertyChangedEvent const& event);
    void update_preview();
    auto generate_selected() -> FReply;
    auto generate_all() -> FReply;
    auto open_output_directory() -> FReply;

    TStrongObjectPtr<UGenLabSettings> settings_;
    TStrongObjectPtr<UTexture2D> preview_texture_;
    TSharedPtr<IDetailsView> details_view_;
    FSlateBrush preview_brush_;
    FText status_;
    EGenLabGenerator last_generator_{EGenLabGenerator::RadialGradient};
};
