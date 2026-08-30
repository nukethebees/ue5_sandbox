#pragma once

#include "CoreMinimal.h"

namespace SandboxImages::GenLab {

struct GENLAB_API FGeneratedImage {
    int32 width{};
    int32 height{};
    TArray<FColor> pixels;
    FString error;

    [[nodiscard]] auto is_valid() const -> bool;
};

struct FRadialGradientParameters {
    int32 width{256};
    int32 height{256};
    float inner_radius{0.05f};
    float outer_radius{0.95f};
};

struct FRingMaskParameters {
    int32 width{256};
    int32 height{256};
    float radius{0.62f};
    float thickness{0.08f};
    float falloff{0.035f};
};

struct FShockwaveFlipbookParameters {
    int32 width{512};
    int32 height{512};
    int32 columns{4};
    int32 rows{4};
    int32 frame_count{16};
    float start_radius{0.08f};
    float end_radius{0.78f};
    float thickness{0.10f};
    float falloff{0.04f};
    float start_intensity{1.0f};
    float end_intensity{0.15f};
};

struct FStarfieldParameters {
    int32 width{256};
    int32 height{256};
    uint32 seed{0x51A7F13Du};
    int32 star_count{180};
    float minimum_brightness{0.35f};
    float minimum_radius{0.55f};
    float maximum_radius{2.1f};
    bool transparent_background{true};
};

struct FNoiseParameters {
    int32 width{256};
    int32 height{256};
    uint32 seed{0xC0FFEE42u};
    float base_scale{48.0f};
    int32 octave_count{4};
    float persistence{0.5f};
    bool tileable{false};
};

struct FDomainWarpedNoiseParameters {
    int32 width{256};
    int32 height{256};
    uint32 base_seed{0x4E454255u};
    uint32 warp_seed{0x57415250u};
    float base_scale{64.0f};
    float warp_scale{96.0f};
    float warp_strength{36.0f};
    int32 base_octave_count{5};
    int32 warp_octave_count{3};
    float persistence{0.5f};
    bool tileable{true};
};

struct FCurlNoiseFlowParameters {
    int32 width{256};
    int32 height{256};
    uint32 seed{0x464C4F57u};
    float base_scale{96.0f};
    int32 octave_count{3};
    float persistence{0.35f};
    float derivative_step{2.5f};
    float strength{1.0f};
    bool tileable{true};
};

enum class ECellularMode : uint8 {
    Distance,
    Borders,
};

struct FCellularNoiseParameters {
    int32 width{256};
    int32 height{256};
    uint32 seed{0xCE11A123u};
    float cell_size{40.0f};
    float jitter{1.0f};
    ECellularMode mode{ECellularMode::Distance};
    float edge_width{1.0f};
    float falloff{1.0f};
    bool tileable{true};
};

struct FHexGridParameters {
    int32 width{256};
    int32 height{256};
    float cell_radius{22.0f};
    float line_thickness{1.5f};
    float falloff{1.0f};
};

enum class EGeneratorType : uint8 {
    RadialGradient,
    RingMask,
    ShockwaveFlipbook,
    Starfield,
    Noise,
    DomainWarpedNoise,
    CurlNoiseFlow,
    CellularNoise,
    HexGrid,
};

struct FImagePostProcessParameters {
    enum class EOutput : uint8 {
        Scalar,
        NormalMap,
        SignedDistance,
    };

    bool invert{false};
    float contrast{1.0f};
    bool threshold_enabled{false};
    float threshold{0.5f};
    float threshold_softness{0.1f};
    EOutput output{EOutput::Scalar};
    float normal_strength{8.0f};
    bool normal_wrap{false};
    float distance_threshold{0.5f};
    float distance_range{16.0f};
    bool distance_wrap{false};
};

struct FGenerationRequest {
    EGeneratorType generator{EGeneratorType::RadialGradient};
    FString output_name{TEXT("soft_radial_gradient")};
    FRadialGradientParameters radial_gradient;
    FRingMaskParameters ring_mask;
    FShockwaveFlipbookParameters shockwave_flipbook;
    FStarfieldParameters starfield;
    FNoiseParameters noise;
    FDomainWarpedNoiseParameters domain_warped_noise;
    FCurlNoiseFlowParameters curl_noise_flow;
    FCellularNoiseParameters cellular_noise;
    FHexGridParameters hex_grid;
    FImagePostProcessParameters post_process;
};

[[nodiscard]] GENLAB_API auto generate_radial_gradient(FRadialGradientParameters const& parameters)
    -> FGeneratedImage;
[[nodiscard]] GENLAB_API auto generate_ring_mask(FRingMaskParameters const& parameters) -> FGeneratedImage;
[[nodiscard]] GENLAB_API auto generate_shockwave_flipbook(FShockwaveFlipbookParameters const& parameters)
    -> FGeneratedImage;
[[nodiscard]] GENLAB_API auto generate_starfield(FStarfieldParameters const& parameters) -> FGeneratedImage;
[[nodiscard]] GENLAB_API auto generate_noise(FNoiseParameters const& parameters) -> FGeneratedImage;
[[nodiscard]] GENLAB_API auto generate_domain_warped_noise(FDomainWarpedNoiseParameters const& parameters)
    -> FGeneratedImage;
[[nodiscard]] GENLAB_API auto generate_curl_noise_flow(FCurlNoiseFlowParameters const& parameters)
    -> FGeneratedImage;
[[nodiscard]] GENLAB_API auto generate_cellular_noise(FCellularNoiseParameters const& parameters)
    -> FGeneratedImage;
[[nodiscard]] GENLAB_API auto generate_hex_grid(FHexGridParameters const& parameters) -> FGeneratedImage;
[[nodiscard]] GENLAB_API auto generate_normal_map(FGeneratedImage const& height_image,
                                       float strength,
                                       bool wrap) -> FGeneratedImage;
[[nodiscard]] GENLAB_API auto generate_signed_distance_field(FGeneratedImage const& mask_image,
                                                  float threshold,
                                                  float distance_range,
                                                  bool wrap) -> FGeneratedImage;
[[nodiscard]] GENLAB_API auto make_default_request(EGeneratorType generator) -> FGenerationRequest;
[[nodiscard]] GENLAB_API auto default_generation_requests() -> TArray<FGenerationRequest>;
[[nodiscard]] GENLAB_API auto generate_image(FGenerationRequest const& request) -> FGeneratedImage;
[[nodiscard]] GENLAB_API auto describe_request(FGenerationRequest const& request) -> FString;

}
