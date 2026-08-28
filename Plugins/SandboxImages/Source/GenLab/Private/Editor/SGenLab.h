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
    void on_preset_selected(TSharedPtr<SandboxImages::GenLab::FGenerationRequest> preset,
                            ESelectInfo::Type selection_type);
    auto make_preset_widget(TSharedPtr<SandboxImages::GenLab::FGenerationRequest> preset) const
        -> TSharedRef<SWidget>;
    [[nodiscard]] auto preset_text() const -> FText;
    void update_preview();
    auto generate_selected() -> FReply;
    auto generate_all() -> FReply;
    auto open_output_directory() -> FReply;

    TStrongObjectPtr<UGenLabSettings> settings_;
    TStrongObjectPtr<UTexture2D> preview_texture_;
    TSharedPtr<IDetailsView> details_view_;
    TArray<TSharedPtr<SandboxImages::GenLab::FGenerationRequest>> presets_;
    TSharedPtr<SComboBox<TSharedPtr<SandboxImages::GenLab::FGenerationRequest>>> preset_combo_;
    TSharedPtr<SandboxImages::GenLab::FGenerationRequest> selected_preset_;
    FSlateBrush preview_brush_;
    FText status_;
    EGenLabGenerator last_generator_{EGenLabGenerator::RadialGradient};
};
