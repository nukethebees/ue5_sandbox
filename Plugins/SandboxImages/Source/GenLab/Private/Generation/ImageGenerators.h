#pragma once

#include "CoreMinimal.h"

namespace SandboxImages::GenLab {

struct FGeneratedImage {
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
    Starfield,
    Noise,
    HexGrid,
};

struct FImagePostProcessParameters {
    bool invert{false};
    float contrast{1.0f};
};

struct FGenerationRequest {
    EGeneratorType generator{EGeneratorType::RadialGradient};
    FString output_name{TEXT("soft_radial_gradient")};
    FRadialGradientParameters radial_gradient;
    FRingMaskParameters ring_mask;
    FStarfieldParameters starfield;
    FNoiseParameters noise;
    FHexGridParameters hex_grid;
    FImagePostProcessParameters post_process;
};

[[nodiscard]] auto generate_radial_gradient(FRadialGradientParameters const& parameters)
    -> FGeneratedImage;
[[nodiscard]] auto generate_ring_mask(FRingMaskParameters const& parameters) -> FGeneratedImage;
[[nodiscard]] auto generate_starfield(FStarfieldParameters const& parameters) -> FGeneratedImage;
[[nodiscard]] auto generate_noise(FNoiseParameters const& parameters) -> FGeneratedImage;
[[nodiscard]] auto generate_hex_grid(FHexGridParameters const& parameters) -> FGeneratedImage;
[[nodiscard]] auto make_default_request(EGeneratorType generator) -> FGenerationRequest;
[[nodiscard]] auto default_generation_requests() -> TArray<FGenerationRequest>;
[[nodiscard]] auto generate_image(FGenerationRequest const& request) -> FGeneratedImage;
[[nodiscard]] auto describe_request(FGenerationRequest const& request) -> FString;

}
