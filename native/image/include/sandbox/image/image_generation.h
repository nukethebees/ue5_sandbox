#pragma once

#include <ioj/constrained_numbers.h>
#include <ioj/min_max.h>

#include <cstdint>
#include <limits>
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
    [[nodiscard]] auto operator==(GeneratedImage const&) const -> bool = default;
    [[nodiscard]] auto is_valid() const -> bool;

    std::int32_t width{};
    std::int32_t height{};
    std::vector<Pixel> pixels;
    std::string error;
};

struct RadialGradientInput {
    std::int32_t width{256};
    std::int32_t height{256};
    float inner_radius{0.05f};
    float outer_radius{0.95f};
};

struct RingMaskInput {
    std::int32_t width{256};
    std::int32_t height{256};
    float radius{0.62f};
    float thickness{0.08f};
    float falloff{0.035f};
};

struct ShockwaveFlipbookInput {
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

struct StarfieldInput {
    std::int32_t width{256};
    std::int32_t height{256};
    std::uint32_t seed{0x51A7F13Du};
    std::int32_t star_count{180};
    float minimum_brightness{0.35f};
    float minimum_radius{0.55f};
    float maximum_radius{2.1f};
    bool transparent_background{true};
};

struct NoiseInput {
    std::int32_t width{256};
    std::int32_t height{256};
    std::uint32_t seed{0xC0FFEE42u};
    float base_scale{48.0f};
    std::int32_t octave_count{4};
    float persistence{0.5f};
    bool tileable{false};
};

struct DomainWarpedNoiseInput {
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

struct CurlNoiseFlowInput {
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

struct CellularNoiseInput {
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

struct HexGridInput {
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
    RadialGradientInput radial_gradient;
    RingMaskInput ring_mask;
    ShockwaveFlipbookInput shockwave_flipbook;
    StarfieldInput starfield;
    NoiseInput noise;
    DomainWarpedNoiseInput domain_warped_noise;
    CurlNoiseFlowInput curl_noise_flow;
    CellularNoiseInput cellular_noise;
    HexGridInput hex_grid;
    ImagePostProcessParameters post_process;
};

struct NormalMapInput {
    float strength{8.f};
    bool wrap{};
};
struct DistanceFieldInput {
    float threshold{0.5f};
    float distance_range{16.f};
    bool wrap{};
};
namespace detail {
inline constexpr ioj::MinMax<float> normalized_range{0.f, 1.f};
inline constexpr ioj::MinMax<std::int32_t> octave_range{1, 16};
inline constexpr ioj::MinMax<float> distance_pixel_range{1.f, 4096.f};

constexpr auto in_closed_range(auto const value, auto const range) -> bool {
    return value >= range.min && value <= range.max;
}
using NonNegative = ioj::NonNegativeFinite<float>;
using Positive = ioj::PositiveFinite<float>;
constexpr auto valid_dimensions(auto const& value) -> bool {
    return value.width > 0 && value.height > 0 &&
           value.width <= std::numeric_limits<std::int32_t>::max() / value.height;
}
constexpr auto valid_noise(float scale, std::int32_t octaves, float persistence) -> bool {
    return Positive::valid(scale) && in_closed_range(octaves, octave_range) &&
           in_closed_range(persistence, normalized_range);
}
constexpr auto valid_parameters(RadialGradientInput const& p) -> bool {
    return valid_dimensions(p) && NonNegative::valid(p.inner_radius) &&
           Positive::valid(p.outer_radius) && p.outer_radius > p.inner_radius;
}
constexpr auto valid_parameters(RingMaskInput const& p) -> bool {
    return valid_dimensions(p) && NonNegative::valid(p.radius) && Positive::valid(p.thickness) &&
           NonNegative::valid(p.falloff);
}
constexpr auto valid_parameters(ShockwaveFlipbookInput const& p) -> bool {
    return valid_dimensions(p) && p.columns > 0 && p.rows > 0 && p.frame_count > 0 &&
           p.columns <= std::numeric_limits<std::int32_t>::max() / p.rows &&
           p.frame_count <= p.columns * p.rows && p.width % p.columns == 0 &&
           p.height % p.rows == 0 && NonNegative::valid(p.start_radius) &&
           NonNegative::valid(p.end_radius) && p.end_radius >= p.start_radius &&
           Positive::valid(p.thickness) && NonNegative::valid(p.falloff) &&
           in_closed_range(p.start_intensity, normalized_range) &&
           in_closed_range(p.end_intensity, normalized_range);
}
constexpr auto valid_parameters(StarfieldInput const& p) -> bool {
    return valid_dimensions(p) && p.star_count >= 0 &&
           in_closed_range(p.minimum_brightness, normalized_range) &&
           Positive::valid(p.minimum_radius) && p.maximum_radius >= p.minimum_radius &&
           Positive::valid(p.maximum_radius);
}
constexpr auto valid_parameters(NoiseInput const& p) -> bool {
    return valid_dimensions(p) && valid_noise(p.base_scale, p.octave_count, p.persistence);
}
constexpr auto valid_parameters(DomainWarpedNoiseInput const& p) -> bool {
    return valid_dimensions(p) && valid_noise(p.base_scale, p.base_octave_count, p.persistence) &&
           Positive::valid(p.warp_scale) && in_closed_range(p.warp_octave_count, octave_range) &&
           NonNegative::valid(p.warp_strength);
}
constexpr auto valid_parameters(CurlNoiseFlowInput const& p) -> bool {
    return valid_dimensions(p) && valid_noise(p.base_scale, p.octave_count, p.persistence) &&
           Positive::valid(p.derivative_step) && in_closed_range(p.strength, normalized_range);
}
constexpr auto valid_parameters(CellularNoiseInput const& p) -> bool {
    return valid_dimensions(p) && Positive::valid(p.cell_size) &&
           in_closed_range(p.jitter, normalized_range) && NonNegative::valid(p.edge_width) &&
           NonNegative::valid(p.falloff);
}
constexpr auto valid_parameters(HexGridInput const& p) -> bool {
    return valid_dimensions(p) && Positive::valid(p.cell_radius) &&
           Positive::valid(p.line_thickness) && NonNegative::valid(p.falloff);
}
constexpr auto valid_parameters(ImagePostProcessParameters const& parameters) -> bool {
    return NonNegative::valid(parameters.contrast) &&
           !(parameters.threshold_enabled &&
             (!in_closed_range(parameters.threshold, normalized_range) ||
              !in_closed_range(parameters.threshold_softness, normalized_range))) &&
           !(parameters.output == ImagePostProcessParameters::Output::NormalMap &&
             !NonNegative::valid(parameters.normal_strength)) &&
           !(parameters.output == ImagePostProcessParameters::Output::SignedDistance &&
             (!in_closed_range(parameters.distance_threshold, normalized_range) ||
              !in_closed_range(parameters.distance_range, distance_pixel_range)));
}
constexpr auto valid_parameters(GenerationRequest const& request) -> bool {
    if (request.generator != GeneratorType::CurlNoiseFlow &&
        !valid_parameters(request.post_process)) {
        return false;
    }
    switch (request.generator) {
        case GeneratorType::RadialGradient:
            return valid_parameters(request.radial_gradient);
        case GeneratorType::RingMask:
            return valid_parameters(request.ring_mask);
        case GeneratorType::ShockwaveFlipbook:
            return valid_parameters(request.shockwave_flipbook);
        case GeneratorType::Starfield:
            return valid_parameters(request.starfield);
        case GeneratorType::Noise:
            return valid_parameters(request.noise);
        case GeneratorType::DomainWarpedNoise:
            return valid_parameters(request.domain_warped_noise);
        case GeneratorType::CurlNoiseFlow:
            return valid_parameters(request.curl_noise_flow);
        case GeneratorType::CellularNoise:
            return valid_parameters(request.cellular_noise);
        case GeneratorType::HexGrid:
            return valid_parameters(request.hex_grid);
    }
    return false;
}
constexpr auto valid_parameters(GeneratedImage const& image) -> bool {
    return valid_dimensions(image) &&
           image.pixels.size() == static_cast<std::size_t>(image.width) * image.height;
}
constexpr auto valid_parameters(NormalMapInput const& value) -> bool {
    return NonNegative::valid(value.strength);
}
constexpr auto valid_parameters(DistanceFieldInput const& value) -> bool {
    return in_closed_range(value.threshold, normalized_range) &&
           in_closed_range(value.distance_range, distance_pixel_range);
}
inline constexpr auto parameter_policy{
    ioj::ConstrainedValuePolicy{[](auto const& value) { return valid_parameters(value); },
                                "Invalid image generation parameters"}};
}
template <typename T>
using Validated = ioj::ConstrainedValue<T, detail::parameter_policy>;
using RadialGradientParameters = Validated<RadialGradientInput>;
using RingMaskParameters = Validated<RingMaskInput>;
using ShockwaveFlipbookParameters = Validated<ShockwaveFlipbookInput>;
using StarfieldParameters = Validated<StarfieldInput>;
using NoiseParameters = Validated<NoiseInput>;
using DomainWarpedNoiseParameters = Validated<DomainWarpedNoiseInput>;
using CurlNoiseFlowParameters = Validated<CurlNoiseFlowInput>;
using CellularNoiseParameters = Validated<CellularNoiseInput>;
using HexGridParameters = Validated<HexGridInput>;
namespace detail {
auto generate_image(RadialGradientInput const& parameters) -> GeneratedImage;
auto generate_image(RingMaskInput const& parameters) -> GeneratedImage;
auto generate_image(ShockwaveFlipbookInput const& parameters) -> GeneratedImage;
auto generate_image(StarfieldInput const& parameters) -> GeneratedImage;
auto generate_image(NoiseInput const& parameters) -> GeneratedImage;
auto generate_image(DomainWarpedNoiseInput const& parameters) -> GeneratedImage;
auto generate_image(CurlNoiseFlowInput const& parameters) -> GeneratedImage;
auto generate_image(CellularNoiseInput const& parameters) -> GeneratedImage;
auto generate_image(HexGridInput const& parameters) -> GeneratedImage;
auto generate_normal_map(GeneratedImage const&, float, bool) -> GeneratedImage;
auto generate_signed_distance_field(GeneratedImage const&, float, float, bool) -> GeneratedImage;
auto generate_image(GenerationRequest const&) -> GeneratedImage;
}
inline auto generate_normal_map(Validated<GeneratedImage> const& input,
                                Validated<NormalMapInput> const& settings) -> GeneratedImage {
    auto const& p{settings.get_ref()};
    return detail::generate_normal_map(input.get_ref(), p.strength, p.wrap);
}
inline auto generate_signed_distance_field(Validated<GeneratedImage> const& input,
                                           Validated<DistanceFieldInput> const& settings)
    -> GeneratedImage {
    auto const& p{settings.get_ref()};
    return detail::generate_signed_distance_field(
        input.get_ref(), p.threshold, p.distance_range, p.wrap);
}
template <typename T>
inline auto generate_image(Validated<T> const& input) -> GeneratedImage {
    return detail::generate_image(input.get_ref());
}

[[nodiscard]] auto make_energy_filaments_request() -> GenerationRequest;
[[nodiscard]] auto make_shield_distortion_flow_request() -> GenerationRequest;

[[nodiscard]] auto make_default_request(GeneratorType generator) -> GenerationRequest;
[[nodiscard]] auto default_generation_requests() -> std::vector<GenerationRequest>;

[[nodiscard]] auto describe_request(GenerationRequest const& request) -> std::string;
[[nodiscard]] auto scale_request_for_preview(GenerationRequest request,
                                             std::int32_t maximum_dimension,
                                             bool tiled) -> GenerationRequest;
[[nodiscard]] auto make_preview_image(GeneratedImage const& source,
                                      PreviewChannel channel,
                                      bool tiled) -> GeneratedImage;

}
