#include "sandbox/image/image_generation.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace sandbox::image {
namespace {
auto round_to_int(float const value) -> std::int32_t {
    return static_cast<std::int32_t>(std::floor(value + 0.5f));
}

auto preview_channel_value(Pixel const pixel, PreviewChannel const channel) -> std::uint8_t {
    switch (channel) {
        case PreviewChannel::Red:
            return pixel.red;
        case PreviewChannel::Green:
            return pixel.green;
        case PreviewChannel::Blue:
            return pixel.blue;
        case PreviewChannel::Alpha:
            return pixel.alpha;
        case PreviewChannel::Color:
        case PreviewChannel::RGB:
            break;
    }
    return 0;
}
}

auto scale_request_for_preview(GenerationRequest request,
                               std::int32_t const maximum_dimension,
                               bool const tiled) -> GenerationRequest {
    auto const maximum_source_dimension{tiled ? maximum_dimension / 2 : maximum_dimension};
    std::int32_t width{};
    std::int32_t height{};
    switch (request.generator) {
        case GeneratorType::RadialGradient:
            width = request.radial_gradient.width;
            height = request.radial_gradient.height;
            break;
        case GeneratorType::RingMask:
            width = request.ring_mask.width;
            height = request.ring_mask.height;
            break;
        case GeneratorType::ShockwaveFlipbook:
            return request;
        case GeneratorType::Starfield:
            width = request.starfield.width;
            height = request.starfield.height;
            break;
        case GeneratorType::Noise:
            width = request.noise.width;
            height = request.noise.height;
            break;
        case GeneratorType::DomainWarpedNoise:
            width = request.domain_warped_noise.width;
            height = request.domain_warped_noise.height;
            break;
        case GeneratorType::CurlNoiseFlow:
            width = request.curl_noise_flow.width;
            height = request.curl_noise_flow.height;
            break;
        case GeneratorType::CellularNoise:
            width = request.cellular_noise.width;
            height = request.cellular_noise.height;
            break;
        case GeneratorType::HexGrid:
            width = request.hex_grid.width;
            height = request.hex_grid.height;
            break;
    }

    if (width <= 0 || height <= 0) {
        return request;
    }

    auto const scale{std::min(1.0f,
                              static_cast<float>(maximum_source_dimension) /
                                  static_cast<float>(std::max(width, height)))};
    auto const preview_width{std::max(1, round_to_int(static_cast<float>(width) * scale))};
    auto const preview_height{std::max(1, round_to_int(static_cast<float>(height) * scale))};
    switch (request.generator) {
        case GeneratorType::RadialGradient:
            request.radial_gradient.width = preview_width;
            request.radial_gradient.height = preview_height;
            break;
        case GeneratorType::RingMask:
            request.ring_mask.width = preview_width;
            request.ring_mask.height = preview_height;
            break;
        case GeneratorType::ShockwaveFlipbook:
            break;
        case GeneratorType::Starfield:
            request.starfield.width = preview_width;
            request.starfield.height = preview_height;
            request.starfield.star_count =
                round_to_int(static_cast<float>(request.starfield.star_count) * scale * scale);
            request.starfield.minimum_radius *= scale;
            request.starfield.maximum_radius *= scale;
            break;
        case GeneratorType::Noise:
            request.noise.width = preview_width;
            request.noise.height = preview_height;
            request.noise.base_scale *= scale;
            break;
        case GeneratorType::DomainWarpedNoise:
            request.domain_warped_noise.width = preview_width;
            request.domain_warped_noise.height = preview_height;
            request.domain_warped_noise.base_scale *= scale;
            request.domain_warped_noise.warp_scale *= scale;
            request.domain_warped_noise.warp_strength *= scale;
            break;
        case GeneratorType::CurlNoiseFlow:
            request.curl_noise_flow.width = preview_width;
            request.curl_noise_flow.height = preview_height;
            request.curl_noise_flow.base_scale *= scale;
            request.curl_noise_flow.derivative_step *= scale;
            break;
        case GeneratorType::CellularNoise:
            request.cellular_noise.width = preview_width;
            request.cellular_noise.height = preview_height;
            request.cellular_noise.cell_size *= scale;
            request.cellular_noise.edge_width *= scale;
            request.cellular_noise.falloff *= scale;
            break;
        case GeneratorType::HexGrid:
            request.hex_grid.width = preview_width;
            request.hex_grid.height = preview_height;
            request.hex_grid.cell_radius *= scale;
            request.hex_grid.line_thickness *= scale;
            request.hex_grid.falloff *= scale;
            break;
    }
    return request;
}

auto make_preview_image(GeneratedImage const& source,
                        PreviewChannel const channel,
                        bool const tiled) -> GeneratedImage {
    auto const tile_count{tiled ? 2 : 1};
    GeneratedImage display{.width = source.width * tile_count,
                           .height = source.height * tile_count,
                           .pixels = {},
                           .error = {}};
    display.pixels.resize(static_cast<std::size_t>(display.width * display.height));
    for (std::int32_t y{0}; y < display.height; ++y) {
        for (std::int32_t x{0}; x < display.width; ++x) {
            auto const source_pixel{source.pixels[static_cast<std::size_t>(
                (y % source.height) * source.width + x % source.width)]};
            auto& display_pixel{display.pixels[static_cast<std::size_t>(y * display.width + x)]};
            if (channel == PreviewChannel::Color) {
                auto const checker_value{((x / 16 + y / 16) & 1) == 0 ? std::uint8_t{48}
                                                                      : std::uint8_t{80}};
                auto const alpha{static_cast<float>(source_pixel.alpha) / 255.0f};
                auto const blend = [checker_value, alpha](std::uint8_t const value) {
                    return static_cast<std::uint8_t>(round_to_int(
                        static_cast<float>(checker_value) +
                        (static_cast<float>(value) - static_cast<float>(checker_value)) * alpha));
                };
                display_pixel = {blend(source_pixel.red),
                                 blend(source_pixel.green),
                                 blend(source_pixel.blue),
                                 255};
            } else if (channel == PreviewChannel::RGB) {
                display_pixel = {source_pixel.red, source_pixel.green, source_pixel.blue, 255};
            } else {
                auto const value{preview_channel_value(source_pixel, channel)};
                display_pixel = {value, value, value, 255};
            }
        }
    }
    return display;
}

}
