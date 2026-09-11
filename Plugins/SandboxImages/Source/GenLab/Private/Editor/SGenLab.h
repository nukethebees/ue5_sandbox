#pragma once

#include "CoreMinimal.h"
#include "Editor/GenLabSettings.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"

class IDetailsView;
template <typename OptionType>
class SComboBox;
class SGenLab final : public SCompoundWidget {
  public:
    SLATE_BEGIN_ARGS(SGenLab) {}
    SLATE_END_ARGS()

    void Construct(FArguments const& arguments);
    ~SGenLab() override;
  private:
    void on_property_changed(FPropertyChangedEvent const& event);
    void on_preset_selected(TSharedPtr<sandbox::image::GenerationRequest> preset,
                            ESelectInfo::Type selection_type);
    auto make_preset_widget(TSharedPtr<sandbox::image::GenerationRequest> preset) const
        -> TSharedRef<SWidget>;
    [[nodiscard]] auto preset_text() const -> FText;
    void update_preview();
    auto generate_selected() -> FReply;
    auto generate_all() -> FReply;
    auto open_output_directory() -> FReply;

    TStrongObjectPtr<UGenLabSettings> settings_;
    TStrongObjectPtr<UTexture2D> preview_texture_;
    TSharedPtr<IDetailsView> details_view_;
    TArray<TSharedPtr<sandbox::image::GenerationRequest>> presets_;
    TSharedPtr<SComboBox<TSharedPtr<sandbox::image::GenerationRequest>>> preset_combo_;
    TSharedPtr<sandbox::image::GenerationRequest> selected_preset_;
    FSlateBrush preview_brush_;
    FText status_;
    EGenLabGenerator last_generator_{EGenLabGenerator::RadialGradient};
};
