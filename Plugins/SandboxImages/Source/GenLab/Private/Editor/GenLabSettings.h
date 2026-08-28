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
    DomainWarpedNoise UMETA(DisplayName = "Domain-Warped Noise"),
    CurlNoiseFlow UMETA(DisplayName = "Curl-Noise Flow Map"),
    CellularNoise UMETA(DisplayName = "Cellular Noise"),
    HexGrid UMETA(DisplayName = "Hex Grid Mask"),
};

UENUM()
enum class EGenLabCellularMode : uint8 {
    Distance UMETA(DisplayName = "Distance"),
    Borders UMETA(DisplayName = "Borders"),
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

UENUM()
enum class EGenLabOutput : uint8 {
    Scalar UMETA(DisplayName = "Scalar / Color"),
    NormalMap UMETA(DisplayName = "Tangent-Space Normal Map"),
    SignedDistance UMETA(DisplayName = "Signed Distance Field"),
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

    UPROPERTY(EditAnywhere,
              Category = "Output Shaping",
              meta = (EditCondition = "generator != EGenLabGenerator::CurlNoiseFlow",
                      EditConditionHides))
    bool invert{false};

    UPROPERTY(EditAnywhere,
              Category = "Output Shaping",
              meta = (EditCondition = "generator != EGenLabGenerator::CurlNoiseFlow",
                      EditConditionHides,
                      ClampMin = "0.0",
                      ClampMax = "8.0"))
    float contrast{1.0f};

    UPROPERTY(EditAnywhere,
              Category = "Output Shaping",
              meta = (EditCondition = "generator != EGenLabGenerator::CurlNoiseFlow",
                      EditConditionHides))
    bool threshold_enabled{false};

    UPROPERTY(
        EditAnywhere,
        Category = "Output Shaping",
        meta = (EditCondition = "generator != EGenLabGenerator::CurlNoiseFlow && threshold_enabled",
                EditConditionHides,
                ClampMin = "0.0",
                ClampMax = "1.0"))
    float threshold{0.5f};

    UPROPERTY(
        EditAnywhere,
        Category = "Output Shaping",
        meta = (EditCondition = "generator != EGenLabGenerator::CurlNoiseFlow && threshold_enabled",
                EditConditionHides,
                ClampMin = "0.0",
                ClampMax = "1.0"))
    float threshold_softness{0.1f};

    UPROPERTY(EditAnywhere,
              Category = "Output Shaping",
              meta = (EditCondition = "generator != EGenLabGenerator::CurlNoiseFlow",
                      EditConditionHides))
    EGenLabOutput output{EGenLabOutput::Scalar};

    UPROPERTY(EditAnywhere,
              Category = "Output Shaping",
              meta = (EditCondition = "output == EGenLabOutput::NormalMap",
                      EditConditionHides,
                      ClampMin = "0.0"))
    float normal_strength{8.0f};

    UPROPERTY(EditAnywhere,
              Category = "Output Shaping",
              meta = (EditCondition = "output == EGenLabOutput::NormalMap", EditConditionHides))
    bool normal_wrap{false};

    UPROPERTY(EditAnywhere,
              Category = "Output Shaping",
              meta = (EditCondition = "output == EGenLabOutput::SignedDistance",
                      EditConditionHides,
                      ClampMin = "0.0",
                      ClampMax = "1.0"))
    float distance_threshold{0.5f};

    UPROPERTY(EditAnywhere,
              Category = "Output Shaping",
              meta = (EditCondition = "output == EGenLabOutput::SignedDistance",
                      EditConditionHides,
                      ClampMin = "1.0",
                      ClampMax = "4096.0"))
    float distance_range{16.0f};

    UPROPERTY(EditAnywhere,
              Category = "Output Shaping",
              meta = (EditCondition = "output == EGenLabOutput::SignedDistance",
                      EditConditionHides))
    bool distance_wrap{false};

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
              Category = "Domain-Warped Noise",
              meta = (EditCondition = "generator == EGenLabGenerator::DomainWarpedNoise",
                      EditConditionHides))
    uint32 domain_base_seed{0x4E454255u};

    UPROPERTY(EditAnywhere,
              Category = "Domain-Warped Noise",
              meta = (EditCondition = "generator == EGenLabGenerator::DomainWarpedNoise",
                      EditConditionHides))
    uint32 domain_warp_seed{0x57415250u};

    UPROPERTY(EditAnywhere,
              Category = "Domain-Warped Noise",
              meta = (EditCondition = "generator == EGenLabGenerator::DomainWarpedNoise",
                      EditConditionHides,
                      ClampMin = "0.001"))
    float domain_base_scale{64.0f};

    UPROPERTY(EditAnywhere,
              Category = "Domain-Warped Noise",
              meta = (EditCondition = "generator == EGenLabGenerator::DomainWarpedNoise",
                      EditConditionHides,
                      ClampMin = "0.001"))
    float domain_warp_scale{96.0f};

    UPROPERTY(EditAnywhere,
              Category = "Domain-Warped Noise",
              meta = (EditCondition = "generator == EGenLabGenerator::DomainWarpedNoise",
                      EditConditionHides,
                      ClampMin = "0.0"))
    float domain_warp_strength{36.0f};

    UPROPERTY(EditAnywhere,
              Category = "Domain-Warped Noise",
              meta = (EditCondition = "generator == EGenLabGenerator::DomainWarpedNoise",
                      EditConditionHides,
                      ClampMin = "1",
                      ClampMax = "16"))
    int32 domain_base_octave_count{5};

    UPROPERTY(EditAnywhere,
              Category = "Domain-Warped Noise",
              meta = (EditCondition = "generator == EGenLabGenerator::DomainWarpedNoise",
                      EditConditionHides,
                      ClampMin = "1",
                      ClampMax = "16"))
    int32 domain_warp_octave_count{3};

    UPROPERTY(EditAnywhere,
              Category = "Domain-Warped Noise",
              meta = (EditCondition = "generator == EGenLabGenerator::DomainWarpedNoise",
                      EditConditionHides,
                      ClampMin = "0.0",
                      ClampMax = "1.0"))
    float domain_persistence{0.5f};

    UPROPERTY(EditAnywhere,
              Category = "Domain-Warped Noise",
              meta = (EditCondition = "generator == EGenLabGenerator::DomainWarpedNoise",
                      EditConditionHides))
    bool tileable_domain_noise{true};

    UPROPERTY(EditAnywhere,
              Category = "Curl-Noise Flow Map",
              meta = (EditCondition = "generator == EGenLabGenerator::CurlNoiseFlow",
                      EditConditionHides))
    uint32 flow_seed{0x464C4F57u};

    UPROPERTY(EditAnywhere,
              Category = "Curl-Noise Flow Map",
              meta = (EditCondition = "generator == EGenLabGenerator::CurlNoiseFlow",
                      EditConditionHides,
                      ClampMin = "0.001"))
    float flow_base_scale{96.0f};

    UPROPERTY(EditAnywhere,
              Category = "Curl-Noise Flow Map",
              meta = (EditCondition = "generator == EGenLabGenerator::CurlNoiseFlow",
                      EditConditionHides,
                      ClampMin = "1",
                      ClampMax = "16"))
    int32 flow_octave_count{3};

    UPROPERTY(EditAnywhere,
              Category = "Curl-Noise Flow Map",
              meta = (EditCondition = "generator == EGenLabGenerator::CurlNoiseFlow",
                      EditConditionHides,
                      ClampMin = "0.0",
                      ClampMax = "1.0"))
    float flow_persistence{0.35f};

    UPROPERTY(EditAnywhere,
              Category = "Curl-Noise Flow Map",
              meta = (EditCondition = "generator == EGenLabGenerator::CurlNoiseFlow",
                      EditConditionHides,
                      ClampMin = "0.001"))
    float flow_derivative_step{2.5f};

    UPROPERTY(EditAnywhere,
              Category = "Curl-Noise Flow Map",
              meta = (EditCondition = "generator == EGenLabGenerator::CurlNoiseFlow",
                      EditConditionHides,
                      ClampMin = "0.0",
                      ClampMax = "1.0"))
    float flow_strength{1.0f};

    UPROPERTY(EditAnywhere,
              Category = "Curl-Noise Flow Map",
              meta = (EditCondition = "generator == EGenLabGenerator::CurlNoiseFlow",
                      EditConditionHides))
    bool tileable_flow{true};

    UPROPERTY(EditAnywhere,
              Category = "Cellular Noise",
              meta = (EditCondition = "generator == EGenLabGenerator::CellularNoise",
                      EditConditionHides))
    uint32 cellular_seed{0xCE11A123u};

    UPROPERTY(EditAnywhere,
              Category = "Cellular Noise",
              meta = (EditCondition = "generator == EGenLabGenerator::CellularNoise",
                      EditConditionHides,
                      ClampMin = "0.001"))
    float cellular_cell_size{40.0f};

    UPROPERTY(EditAnywhere,
              Category = "Cellular Noise",
              meta = (EditCondition = "generator == EGenLabGenerator::CellularNoise",
                      EditConditionHides,
                      ClampMin = "0.0",
                      ClampMax = "1.0"))
    float cellular_jitter{1.0f};

    UPROPERTY(EditAnywhere,
              Category = "Cellular Noise",
              meta = (EditCondition = "generator == EGenLabGenerator::CellularNoise",
                      EditConditionHides))
    EGenLabCellularMode cellular_mode{EGenLabCellularMode::Distance};

    UPROPERTY(EditAnywhere,
              Category = "Cellular Noise",
              meta = (EditCondition = "generator == EGenLabGenerator::CellularNoise",
                      EditConditionHides,
                      ClampMin = "0.0"))
    float cellular_edge_width{1.0f};

    UPROPERTY(EditAnywhere,
              Category = "Cellular Noise",
              meta = (EditCondition = "generator == EGenLabGenerator::CellularNoise",
                      EditConditionHides,
                      ClampMin = "0.0"))
    float cellular_falloff{1.0f};

    UPROPERTY(EditAnywhere,
              Category = "Cellular Noise",
              meta = (EditCondition = "generator == EGenLabGenerator::CellularNoise",
                      EditConditionHides))
    bool tileable_cellular{true};

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
    void load_request(SandboxImages::GenLab::FGenerationRequest const& request);
    void load_generator_defaults();
};
