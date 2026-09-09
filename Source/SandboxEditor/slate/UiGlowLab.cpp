#include "SandboxEditor/slate/UiGlowLab.h"

#include "SandboxUI/EntityOverlay/SEntityOverlayWidget.h"
#include "SpaceGamePresentation/ui/style/SpaceGameUiTheme.h"

#include "HAL/FileManager.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "ShaderCompiler.h"
#include "Slate/WidgetRenderer.h"
#include "Styling/AppStyle.h"
#include "TextureResource.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace ml::ui::glow_lab {
inline FLinearColor const space_background{0.035f, 0.045f, 0.075f, 1.0f};

auto make_style() -> FEntityOverlayStyle {
    auto const theme{GetDefault<ml::ioj::USpaceGameUiTheme>()->compile()};
    FEntityOverlayStyle style;
    style.soft_target_in_range_color = theme.palette().honey;
    style.soft_target_neutral_color = theme.palette().text_secondary;
    return style;
}

auto make_frame(bool const in_range,
                float const progress = 1.0f,
                float const pulse = 0.0f,
                bool const fading = false,
                float const radius = 72.0f) -> FEntityOverlayFrame {
    FEntityOverlayFrame frame;
    frame.instances.Add({.world_position = {0.0f, 0.0f, 0.0f},
                         .health = 0.75f,
                         .world_radius = 200.0f,
                         .display_data = static_cast<uint32>(EEntityOverlaySoftTargetRole::Active)
                                      << FEntityOverlayInstance::soft_target_role_shift});
    frame.soft_target_radius_pixels = radius;
    frame.soft_target_range_progress = progress;
    frame.soft_target_in_range = in_range;
    frame.soft_target_pulse = pulse;
    if (fading) {
        frame.instances.Add(
            {.world_position = {-0.15f, 0.0f, 0.0f},
             .health = 0.5f,
             .world_radius = 200.0f,
             .display_data = static_cast<uint32>(EEntityOverlaySoftTargetRole::Fading)
                          << FEntityOverlayInstance::soft_target_role_shift});
        frame.fading_soft_target_radius_pixels = radius;
        frame.fading_soft_target_range_progress = 1.0f;
        frame.fading_soft_target_in_range = true;
        frame.fading_soft_target_visibility = 0.5f;
    }
    return frame;
}

class SUiGlowLab final : public SCompoundWidget {
  public:
    SLATE_BEGIN_ARGS(SUiGlowLab) {}
    SLATE_END_ARGS()

    void Construct(FArguments const&) {
        frame_store_ = MakeShared<FEntityOverlayFrameStore, ESPMode::ThreadSafe>();
        SAssignNew(preview_, SEntityOverlayWidget);
        preview_->set_frame_store(frame_store_);
        auto controls{SNew(SVerticalBox)};
        auto add_control{[controls](FString const& label, TSharedRef<SWidget> const& control) {
            controls->AddSlot().AutoHeight().Padding(
                0, 4)[SNew(SVerticalBox) +
                      SVerticalBox::Slot()
                          .AutoHeight()[SNew(STextBlock).Text(FText::FromString(label))] +
                      SVerticalBox::Slot().AutoHeight()[control]];
        }};
        auto add_float{[&add_control](TCHAR const* label, float& value, float const maximum) {
            add_control(label,
                        SNew(SSpinBox<float>)
                            .MinValue(0.0f)
                            .MaxValue(maximum)
                            .Value_Lambda([&value] { return value; })
                            .OnValueChanged_Lambda([&value](float const next) { value = next; }));
        }};
        auto add_toggle{[&add_control](TCHAR const* label, bool& value) {
            add_control(label,
                        SNew(SCheckBox)
                            .IsChecked_Lambda([&value] {
                                return value ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
                            })
                            .OnCheckStateChanged_Lambda([&value](ECheckBoxState const next) {
                                value = next == ECheckBoxState::Checked;
                            }));
        }};
        add_toggle(TEXT("In firing range"), in_range_);
        add_toggle(TEXT("Show core"), show_core_);
        add_toggle(TEXT("Show glow"), show_glow_);
        add_toggle(TEXT("Keep original edge pixels"), style_.soft_target_glow.preserve_core_pixels);
        add_toggle(TEXT("Fading previous target"), fading_);
        add_float(TEXT("Emission"), style_.soft_target_glow.intensity, 4.0f);
        add_float(TEXT("Near glow sigma (px)"), style_.soft_target_glow.near_sigma_pixels, 16.0f);
        add_float(TEXT("Near glow weight"), style_.soft_target_glow.near_weight, 1.0f);
        add_float(TEXT("Halo sigma (px)"), style_.soft_target_glow.halo_sigma_pixels, 32.0f);
        add_float(TEXT("Halo weight"), style_.soft_target_glow.halo_weight, 1.0f);
        add_float(TEXT("Approach progress"), progress_, 1.0f);
        add_float(TEXT("Indicator radius (px)"), radius_, 160.0f);
        add_float(TEXT("Existing activation pulse"), pulse_, 1.0f);
        add_toggle(TEXT("Freeze activation"), freeze_);
        add_control(TEXT("Activation"),
                    SNew(SButton).Text(FText::FromString(TEXT("Replay"))).OnClicked_Lambda([this] {
                        in_range_ = true;
                        pulse_ = 1.0f;
                        freeze_ = false;
                        return FReply::Handled();
                    }));
        add_control(TEXT("Background"),
                    SNew(SButton)
                        .Text(FText::FromString(TEXT("Cycle black / space / bright")))
                        .OnClicked_Lambda([this] {
                            background_index_ = (background_index_ + 1) % 3;
                            return FReply::Handled();
                        }));
        ChildSlot[SNew(SHorizontalBox) +
                  SHorizontalBox::Slot().AutoWidth().Padding(
                      12)[SNew(SBox).WidthOverride(250)[controls]] +
                  SHorizontalBox::Slot().FillWidth(
                      1.0f)[SNew(SBorder)
                                .Padding(0)
                                .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
                                .BorderBackgroundColor_Lambda([this] {
                                    return background_index_ == 0 ? FLinearColor::Black
                                         : background_index_ == 1
                                             ? space_background
                                             : FLinearColor{0.35f, 0.38f, 0.42f, 1.0f};
                                })[preview_.ToSharedRef()]]];
    }

    void Tick(FGeometry const& geometry,
              double const current_time,
              float const delta_time) override {
        SCompoundWidget::Tick(geometry, current_time, delta_time);
        if (!freeze_) {
            pulse_ = FMath::Max(0.0f, pulse_ - delta_time / 0.3f);
        }
        auto const& preview_geometry{preview_->GetCachedGeometry()};
        auto const pixels{preview_geometry.GetLocalSize() *
                          preview_geometry.GetAccumulatedLayoutTransform().GetScale()};
        FIntPoint const size{FMath::RoundToInt(pixels.X), FMath::RoundToInt(pixels.Y)};
        if (size.X <= 0 || size.Y <= 0) {
            return;
        }
        frame_store_->next() = make_frame(in_range_, progress_, pulse_, fading_, radius_);
        frame_store_->publish();
        auto style{style_};
        if (!show_glow_) {
            style.soft_target_glow.intensity = 0.0f;
        }
        preview_->set_style(style);
        preview_->set_core_visibility(show_core_);
        preview_->render({.view_rect = {0, 0, size.X, size.Y}, .output_size = size});
    }
  private:
    FEntityOverlayFrameStorePtr frame_store_;
    TSharedPtr<SEntityOverlayWidget> preview_;
    FEntityOverlayStyle style_{make_style()};
    bool in_range_{true};
    bool show_core_{true};
    bool show_glow_{true};
    bool fading_{};
    bool freeze_{true};
    float progress_{1.0f};
    float radius_{72.0f};
    float pulse_{};
    int32 background_index_{1};
};

auto make_widget() -> TSharedRef<SWidget> {
    return SNew(SUiGlowLab);
}

auto capture(FString const& output_directory) -> bool {
    auto* const material{
        LoadObject<UMaterialInterface>(nullptr, SEntityOverlayWidget::glow_material_path)};
    if (material == nullptr) {
        UE_LOG(LogTemp, Error, TEXT("Generate the UI glow material before capturing the lab."));
        return false;
    }
    GShaderCompilingManager->FinishAllCompilation();
    IFileManager::Get().MakeDirectory(*output_directory, true);
    auto const frame_store{MakeShared<FEntityOverlayFrameStore, ESPMode::ThreadSafe>()};
    auto const preview{SNew(SEntityOverlayWidget)};
    preview->set_frame_store(frame_store);
    auto const background{SNew(SBorder)
                              .Padding(0)
                              .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
                              .BorderBackgroundColor(space_background)[preview]};
    FWidgetRenderer renderer{true};
    TStrongObjectPtr<UTextureRenderTarget2D> output;

    auto draw{[&](FString const& name,
                  FIntPoint const size,
                  FEntityOverlayFrame const& frame,
                  FEntityOverlayStyle const& style,
                  bool const core,
                  float const scale,
                  TArray<FColor>& pixels) {
        if (!output.IsValid() || output->SizeX != size.X || output->SizeY != size.Y) {
            output.Reset(NewObject<UTextureRenderTarget2D>());
            // Match the SDR backbuffer: Slate encodes gamma, so the target must not encode again.
            output->ClearColor = FLinearColor::Transparent;
            output->InitCustomFormat(size.X, size.Y, PF_B8G8R8A8, true);
            output->UpdateResourceImmediate(true);
        }
        frame_store->next() = frame;
        frame_store->publish();
        preview->set_style(style);
        preview->set_core_visibility(core);
        for (int32 warmup{0}; warmup < 3; ++warmup) {
            preview->render({.view_rect = {0, 0, size.X, size.Y}, .output_size = size});
            FlushRenderingCommands();
            renderer.DrawWidget(output.Get(), background, scale, FVector2D{size}, 0.0f);
            FlushRenderingCommands();
        }
        FReadSurfaceDataFlags read_flags{RCM_MinMax};
        read_flags.SetLinearToGamma(false);
        if (!output->GameThread_GetRenderTargetResource()->ReadPixels(pixels, read_flags)) {
            return false;
        }
        auto const filename{FPaths::Combine(output_directory, name + TEXT(".png"))};
        // Slate has already encoded these bytes for display; do not encode them a second time.
        return FImageUtils::SaveImageByExtension(*filename,
                                                 FImageView{pixels.GetData(), size.X, size.Y});
    }};

    auto const glowing_style{make_style()};
    auto baseline_style{glowing_style};
    baseline_style.soft_target_glow.intensity = 0.0f;
    for (FIntPoint const size :
         {FIntPoint{512, 512}, FIntPoint{1920, 1080}, FIntPoint{3840, 2160}}) {
        auto const prefix{FString::Printf(TEXT("%dx%d_"), size.X, size.Y)};
        auto const frame{make_frame(true)};
        TArray<FColor> baseline;
        TArray<FColor> combined;
        TArray<FColor> other;
        background->SetBorderBackgroundColor(space_background);
        if (!draw(prefix + TEXT("baseline"), size, frame, baseline_style, true, 1.0f, baseline) ||
            !draw(prefix + TEXT("steady"), size, frame, glowing_style, true, 1.0f, combined) ||
            !draw(prefix + TEXT("glow_only"), size, frame, glowing_style, false, 1.0f, other)) {
            return false;
        }
        int32 changed_pixels{0};
        int32 changed_core_pixels{0};
        int32 darkened_pixels{0};
        int32 escaped_pixels{0};
        auto const pixel_count{baseline.Num()};
        auto const background_pixel{baseline[0]};
        for (int32 index{0}; index < pixel_count; ++index) {
            if (baseline[index] != combined[index]) {
                ++changed_pixels;
                auto const x{static_cast<float>(index % size.X) + 0.5f - size.X * 0.5f};
                auto const y{static_cast<float>(index / size.X) + 0.5f - size.Y * 0.5f};
                escaped_pixels +=
                    FMath::Abs(x) > frame.soft_target_radius_pixels * 1.154701f + 33.0f ||
                            FMath::Abs(y) > frame.soft_target_radius_pixels + 33.0f
                        ? 1
                        : 0;
                changed_core_pixels += baseline[index] != background_pixel ? 1 : 0;
                darkened_pixels += combined[index].R < baseline[index].R ||
                                           combined[index].G < baseline[index].G ||
                                           combined[index].B < baseline[index].B
                                     ? 1
                                     : 0;
            }
        }
        UE_LOG(LogTemp,
               Display,
               TEXT("GlowLab %s pulse=0: changed=%d core_changed=%d darkened=%d"),
               *prefix,
               changed_pixels,
               changed_core_pixels,
               darkened_pixels);
        if (changed_pixels < 100 || changed_core_pixels == 0 || darkened_pixels != 0 ||
            escaped_pixels != 0) {
            UE_LOG(LogTemp, Error, TEXT("GlowLab steady composition failed visual invariants."));
            return false;
        }
        if (!draw(prefix + TEXT("dpi_150"), size, frame, glowing_style, true, 1.5f, other) ||
            other != combined) {
            UE_LOG(LogTemp, Error, TEXT("GlowLab physical pixel alignment changed at 150%% DPI."));
            return false;
        }
        auto out_of_range{make_frame(false, 0.5f)};
        auto strict_style{glowing_style};
        strict_style.soft_target_glow.preserve_core_pixels = true;
        if (!draw(prefix + TEXT("strict_edges"), size, frame, strict_style, true, 1.0f, other)) {
            return false;
        }
        for (int32 index{0}; index < pixel_count; ++index) {
            if (baseline[index] != background_pixel && baseline[index] != other[index]) {
                UE_LOG(LogTemp, Error, TEXT("GlowLab strict composition changed a core pixel."));
                return false;
            }
        }
        if (!draw(prefix + TEXT("out_of_range_baseline"),
                  size,
                  out_of_range,
                  baseline_style,
                  true,
                  1.0f,
                  baseline) ||
            !draw(prefix + TEXT("out_of_range"),
                  size,
                  out_of_range,
                  glowing_style,
                  true,
                  1.0f,
                  combined) ||
            baseline != combined) {
            UE_LOG(LogTemp, Error, TEXT("GlowLab out-of-range output changed."));
            return false;
        }
        if (!draw(prefix + TEXT("activation"),
                  size,
                  make_frame(true, 1.0f, 1.0f),
                  glowing_style,
                  true,
                  1.0f,
                  other) ||
            !draw(prefix + TEXT("switching"),
                  size,
                  make_frame(true, 1.0f, 0.0f, true),
                  glowing_style,
                  true,
                  1.0f,
                  other)) {
            return false;
        }
        background->SetBorderBackgroundColor(FLinearColor::Black);
        if (!draw(prefix + TEXT("black"), size, frame, glowing_style, true, 1.0f, other)) {
            return false;
        }
        background->SetBorderBackgroundColor(FLinearColor{0.35f, 0.38f, 0.42f, 1.0f});
        if (!draw(prefix + TEXT("bright"), size, frame, glowing_style, true, 1.0f, other) ||
            !draw(prefix + TEXT("target_lost"), size, {}, glowing_style, true, 1.0f, other)) {
            return false;
        }
        for (FColor const pixel : other) {
            if (pixel != other[0]) {
                UE_LOG(LogTemp, Error, TEXT("GlowLab stale pixels after target loss."));
                return false;
            }
        }
    }
    UE_LOG(LogTemp, Display, TEXT("GlowLab captures saved to %s"), *output_directory);
    return true;
}
}
