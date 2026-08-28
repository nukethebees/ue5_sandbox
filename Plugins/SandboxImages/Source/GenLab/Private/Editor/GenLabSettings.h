#pragma once

#include "CoreMinimal.h"
#include "Generation/ImageGenerators.h"
#include "UObject/Object.h"

#include "GenLabSettings.generated.h"

UENUM()
enum class EGenLabGenerator : uint8 {
    RadialGradient UMETA(DisplayName = "Soft Radial Gradient"),
    RingMask UMETA(DisplayName = "Ring Mask"),
    Starfield UMETA(DisplayName = "Starfield"),
    Noise UMETA(DisplayName = "Coherent Noise"),
    HexGrid UMETA(DisplayName = "Hex Grid Mask"),
};

UENUM()
enum class EGenLabPreviewChannel : uint8 {
    Color UMETA(DisplayName = "Color + Alpha"),
    RGB UMETA(DisplayName = "RGB (Opaque)"),
    Red UMETA(DisplayName = "Red"),
    Green UMETA(DisplayName = "Green"),
    Blue UMETA(DisplayName = "Blue"),
    Alpha UMETA(DisplayName = "Alpha"),
};

UCLASS(Transient)
class UGenLabSettings final : public UObject {
    GENERATED_BODY()
  public:
    UPROPERTY(EditAnywhere, Category = "Output")
    EGenLabGenerator generator{EGenLabGenerator::RadialGradient};

    UPROPERTY(EditAnywhere, Category = "Output")
    FString output_name{TEXT("soft_radial_gradient")};

    UPROPERTY(EditAnywhere, Category = "Output", meta = (ClampMin = "1", ClampMax = "2048"))
    int32 width{256};

    UPROPERTY(EditAnywhere, Category = "Output", meta = (ClampMin = "1", ClampMax = "2048"))
    int32 height{256};

    UPROPERTY(EditAnywhere, Category = "Output Shaping")
    bool invert{false};

    UPROPERTY(EditAnywhere,
              Category = "Output Shaping",
              meta = (ClampMin = "0.0", ClampMax = "8.0"))
    float contrast{1.0f};

    UPROPERTY(EditAnywhere, Category = "Preview")
    EGenLabPreviewChannel preview_channel{EGenLabPreviewChannel::Color};

    UPROPERTY(EditAnywhere, Category = "Preview")
    bool tiled_preview{false};

    UPROPERTY(EditAnywhere,
              Category = "Radial Gradient",
              meta = (EditCondition = "generator == EGenLabGenerator::RadialGradient",
                      EditConditionHides,
                      ClampMin = "0.0",
                      ClampMax = "2.0"))
    float inner_radius{0.05f};

    UPROPERTY(EditAnywhere,
              Category = "Radial Gradient",
              meta = (EditCondition = "generator == EGenLabGenerator::RadialGradient",
                      EditConditionHides,
                      ClampMin = "0.001",
                      ClampMax = "2.0"))
    float outer_radius{0.95f};

    UPROPERTY(EditAnywhere,
              Category = "Ring Mask",
              meta = (EditCondition = "generator == EGenLabGenerator::RingMask",
                      EditConditionHides,
                      ClampMin = "0.0",
                      ClampMax = "2.0"))
    float ring_radius{0.62f};

    UPROPERTY(EditAnywhere,
              Category = "Ring Mask",
              meta = (EditCondition = "generator == EGenLabGenerator::RingMask",
                      EditConditionHides,
                      ClampMin = "0.001",
                      ClampMax = "1.0"))
    float ring_thickness{0.08f};

    UPROPERTY(EditAnywhere,
              Category = "Ring Mask",
              meta = (EditCondition = "generator == EGenLabGenerator::RingMask",
                      EditConditionHides,
                      ClampMin = "0.0",
                      ClampMax = "1.0"))
    float ring_falloff{0.035f};

    UPROPERTY(
        EditAnywhere,
        Category = "Seeded Generators",
        meta =
            (EditCondition =
                 "generator == EGenLabGenerator::Starfield || generator == EGenLabGenerator::Noise",
             EditConditionHides))
    uint32 seed{0x51A7F13Du};

    UPROPERTY(EditAnywhere,
              Category = "Starfield",
              meta = (EditCondition = "generator == EGenLabGenerator::Starfield",
                      EditConditionHides,
                      ClampMin = "0",
                      ClampMax = "100000"))
    int32 star_count{180};

    UPROPERTY(EditAnywhere,
              Category = "Starfield",
              meta = (EditCondition = "generator == EGenLabGenerator::Starfield",
                      EditConditionHides,
                      ClampMin = "0.0",
                      ClampMax = "1.0"))
    float minimum_brightness{0.35f};

    UPROPERTY(EditAnywhere,
              Category = "Starfield",
              meta = (EditCondition = "generator == EGenLabGenerator::Starfield",
                      EditConditionHides,
                      ClampMin = "0.01",
                      ClampMax = "64.0"))
    float minimum_star_radius{0.55f};

    UPROPERTY(EditAnywhere,
              Category = "Starfield",
              meta = (EditCondition = "generator == EGenLabGenerator::Starfield",
                      EditConditionHides,
                      ClampMin = "0.01",
                      ClampMax = "64.0"))
    float maximum_star_radius{2.1f};

    UPROPERTY(EditAnywhere,
              Category = "Starfield",
              meta = (EditCondition = "generator == EGenLabGenerator::Starfield",
                      EditConditionHides))
    bool transparent_background{true};

    UPROPERTY(EditAnywhere,
              Category = "Noise",
              meta = (EditCondition = "generator == EGenLabGenerator::Noise",
                      EditConditionHides,
                      ClampMin = "0.001"))
    float noise_base_scale{48.0f};

    UPROPERTY(EditAnywhere,
              Category = "Noise",
              meta = (EditCondition = "generator == EGenLabGenerator::Noise",
                      EditConditionHides,
                      ClampMin = "1",
                      ClampMax = "16"))
    int32 noise_octave_count{4};

    UPROPERTY(EditAnywhere,
              Category = "Noise",
              meta = (EditCondition = "generator == EGenLabGenerator::Noise",
                      EditConditionHides,
                      ClampMin = "0.0",
                      ClampMax = "1.0"))
    float noise_persistence{0.5f};

    UPROPERTY(EditAnywhere,
              Category = "Noise",
              meta = (EditCondition = "generator == EGenLabGenerator::Noise", EditConditionHides))
    bool tileable_noise{false};

    UPROPERTY(EditAnywhere,
              Category = "Hex Grid",
              meta = (EditCondition = "generator == EGenLabGenerator::HexGrid",
                      EditConditionHides,
                      ClampMin = "0.001"))
    float hex_cell_radius{22.0f};

    UPROPERTY(EditAnywhere,
              Category = "Hex Grid",
              meta = (EditCondition = "generator == EGenLabGenerator::HexGrid",
                      EditConditionHides,
                      ClampMin = "0.001"))
    float hex_line_thickness{1.5f};

    UPROPERTY(EditAnywhere,
              Category = "Hex Grid",
              meta = (EditCondition = "generator == EGenLabGenerator::HexGrid",
                      EditConditionHides,
                      ClampMin = "0.0"))
    float hex_falloff{1.0f};

    [[nodiscard]] auto to_request() const -> SandboxImages::GenLab::FGenerationRequest;
    void load_generator_defaults();
};
