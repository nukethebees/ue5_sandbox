#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace sandbox::image {

struct Pixel {
    std::uint8_t red{};
    std::uint8_t green{};
    std::uint8_t blue{};
    std::uint8_t alpha{};

    [[nodiscard]] auto operator==(Pixel const&) const -> bool = default;
};

inline constexpr Pixel transparent{};
inline constexpr Pixel black{0, 0, 0, 255};
inline constexpr Pixel white{255, 255, 255, 255};

struct GeneratedImage {
    std::int32_t width{};
    std::int32_t height{};
    std::vector<Pixel> pixels;
    std::string error;

    [[nodiscard]] auto is_valid() const -> bool;
};

struct RadialGradientParameters {
    std::int32_t width{256};
    std::int32_t height{256};
    float inner_radius{0.05f};
    float outer_radius{0.95f};
};

struct RingMaskParameters {
    std::int32_t width{256};
    std::int32_t height{256};
    float radius{0.62f};
    float thickness{0.08f};
    float falloff{0.035f};
};

struct ShockwaveFlipbookParameters {
    std::int32_t width{512};
    std::int32_t height{512};
    std::int32_t columns{4};
    std::int32_t rows{4};
    std::int32_t frame_count{16};
    float start_radius{0.08f};
    float end_radius{0.78f};
    float thickness{0.10f};
    float falloff{0.04f};
    float start_intensity{1.0f};
    float end_intensity{0.15f};
};

struct StarfieldParameters {
    std::int32_t width{256};
    std::int32_t height{256};
    std::uint32_t seed{0x51A7F13Du};
    std::int32_t star_count{180};
    float minimum_brightness{0.35f};
    float minimum_radius{0.55f};
    float maximum_radius{2.1f};
    bool transparent_background{true};
};

struct NoiseParameters {
    std::int32_t width{256};
    std::int32_t height{256};
    std::uint32_t seed{0xC0FFEE42u};
    float base_scale{48.0f};
    std::int32_t octave_count{4};
    float persistence{0.5f};
    bool tileable{false};
};

struct DomainWarpedNoiseParameters {
    std::int32_t width{256};
    std::int32_t height{256};
    std::uint32_t base_seed{0x4E454255u};
    std::uint32_t warp_seed{0x57415250u};
    float base_scale{64.0f};
    float warp_scale{96.0f};
    float warp_strength{36.0f};
    std::int32_t base_octave_count{5};
    std::int32_t warp_octave_count{3};
    float persistence{0.5f};
    bool tileable{true};
};

struct CurlNoiseFlowParameters {
    std::int32_t width{256};
    std::int32_t height{256};
    std::uint32_t seed{0x464C4F57u};
    float base_scale{96.0f};
    std::int32_t octave_count{3};
    float persistence{0.35f};
    float derivative_step{2.5f};
    float strength{1.0f};
    bool tileable{true};
};

enum class CellularMode : std::uint8_t {
    Distance,
    Borders,
};

struct CellularNoiseParameters {
    std::int32_t width{256};
    std::int32_t height{256};
    std::uint32_t seed{0xCE11A123u};
    float cell_size{40.0f};
    float jitter{1.0f};
    CellularMode mode{CellularMode::Distance};
    float edge_width{1.0f};
    float falloff{1.0f};
    bool tileable{true};
};

struct HexGridParameters {
    std::int32_t width{256};
    std::int32_t height{256};
    float cell_radius{22.0f};
    float line_thickness{1.5f};
    float falloff{1.0f};
};

enum class GeneratorType : std::uint8_t {
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

enum class PreviewChannel : std::uint8_t {
    Color,
    RGB,
    Red,
    Green,
    Blue,
    Alpha,
};

struct ImagePostProcessParameters {
    enum class Output : std::uint8_t {
        Scalar,
        NormalMap,
        SignedDistance,
    };

    bool invert{false};
    float contrast{1.0f};
    bool threshold_enabled{false};
    float threshold{0.5f};
    float threshold_softness{0.1f};
    Output output{Output::Scalar};
    float normal_strength{8.0f};
    bool normal_wrap{false};
    float distance_threshold{0.5f};
    float distance_range{16.0f};
    bool distance_wrap{false};
};

struct GenerationRequest {
    GeneratorType generator{GeneratorType::RadialGradient};
    std::string output_name{"soft_radial_gradient"};
    RadialGradientParameters radial_gradient;
    RingMaskParameters ring_mask;
    ShockwaveFlipbookParameters shockwave_flipbook;
    StarfieldParameters starfield;
    NoiseParameters noise;
    DomainWarpedNoiseParameters domain_warped_noise;
    CurlNoiseFlowParameters curl_noise_flow;
    CellularNoiseParameters cellular_noise;
    HexGridParameters hex_grid;
    ImagePostProcessParameters post_process;
};

[[nodiscard]] auto generate_radial_gradient(RadialGradientParameters const& parameters)
    -> GeneratedImage;
[[nodiscard]] auto generate_ring_mask(RingMaskParameters const& parameters) -> GeneratedImage;
[[nodiscard]] auto generate_shockwave_flipbook(ShockwaveFlipbookParameters const& parameters)
    -> GeneratedImage;
[[nodiscard]] auto generate_starfield(StarfieldParameters const& parameters) -> GeneratedImage;
[[nodiscard]] auto generate_noise(NoiseParameters const& parameters) -> GeneratedImage;
[[nodiscard]] auto generate_domain_warped_noise(DomainWarpedNoiseParameters const& parameters)
    -> GeneratedImage;
[[nodiscard]] auto generate_curl_noise_flow(CurlNoiseFlowParameters const& parameters)
    -> GeneratedImage;
[[nodiscard]] auto generate_cellular_noise(CellularNoiseParameters const& parameters)
    -> GeneratedImage;
[[nodiscard]] auto generate_hex_grid(HexGridParameters const& parameters) -> GeneratedImage;
[[nodiscard]] auto make_energy_filaments_request() -> GenerationRequest;
[[nodiscard]] auto make_shield_distortion_flow_request() -> GenerationRequest;
[[nodiscard]] auto generate_normal_map(GeneratedImage const& height_image,
                                       float strength,
                                       bool wrap) -> GeneratedImage;
[[nodiscard]] auto generate_signed_distance_field(GeneratedImage const& mask_image,
                                                  float threshold,
                                                  float distance_range,
                                                  bool wrap) -> GeneratedImage;
[[nodiscard]] auto make_default_request(GeneratorType generator) -> GenerationRequest;
[[nodiscard]] auto default_generation_requests() -> std::vector<GenerationRequest>;
[[nodiscard]] auto generate_image(GenerationRequest const& request) -> GeneratedImage;
[[nodiscard]] auto describe_request(GenerationRequest const& request) -> std::string;
[[nodiscard]] auto scale_request_for_preview(GenerationRequest request,
                                             std::int32_t maximum_dimension,
                                             bool tiled) -> GenerationRequest;
[[nodiscard]] auto make_preview_image(GeneratedImage const& source,
                                      PreviewChannel channel,
                                      bool tiled) -> GeneratedImage;

}
