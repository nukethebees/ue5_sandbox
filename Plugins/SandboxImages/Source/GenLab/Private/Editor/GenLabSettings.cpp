#include "Editor/GenLabSettings.h"

namespace {
auto to_generator_type(EGenLabGenerator const generator) -> SandboxImages::GenLab::EGeneratorType {
    using enum SandboxImages::GenLab::EGeneratorType;
    switch (generator) {
        case EGenLabGenerator::RadialGradient:
            return RadialGradient;
        case EGenLabGenerator::RingMask:
            return RingMask;
        case EGenLabGenerator::Starfield:
            return Starfield;
        case EGenLabGenerator::Noise:
            return Noise;
        case EGenLabGenerator::DomainWarpedNoise:
            return DomainWarpedNoise;
        case EGenLabGenerator::CurlNoiseFlow:
            return CurlNoiseFlow;
        case EGenLabGenerator::CellularNoise:
            return CellularNoise;
        case EGenLabGenerator::HexGrid:
            return HexGrid;
    }
    return RadialGradient;
}

auto to_editor_generator(SandboxImages::GenLab::EGeneratorType const generator)
    -> EGenLabGenerator {
    using enum SandboxImages::GenLab::EGeneratorType;
    switch (generator) {
        case RadialGradient:
            return EGenLabGenerator::RadialGradient;
        case RingMask:
            return EGenLabGenerator::RingMask;
        case Starfield:
            return EGenLabGenerator::Starfield;
        case Noise:
            return EGenLabGenerator::Noise;
        case DomainWarpedNoise:
            return EGenLabGenerator::DomainWarpedNoise;
        case CurlNoiseFlow:
            return EGenLabGenerator::CurlNoiseFlow;
        case CellularNoise:
            return EGenLabGenerator::CellularNoise;
        case HexGrid:
            return EGenLabGenerator::HexGrid;
    }
    return EGenLabGenerator::RadialGradient;
}

auto to_cellular_mode(EGenLabCellularMode const mode) -> SandboxImages::GenLab::ECellularMode {
    return mode == EGenLabCellularMode::Distance ? SandboxImages::GenLab::ECellularMode::Distance
                                                 : SandboxImages::GenLab::ECellularMode::Borders;
}
}

auto UGenLabSettings::to_request() const -> SandboxImages::GenLab::FGenerationRequest {
    using namespace SandboxImages::GenLab;
    auto request{make_default_request(to_generator_type(generator))};
    request.output_name = output_name;
    request.radial_gradient = {.width = width,
                               .height = height,
                               .inner_radius = inner_radius,
                               .outer_radius = outer_radius};
    request.ring_mask = {.width = width,
                         .height = height,
                         .radius = ring_radius,
                         .thickness = ring_thickness,
                         .falloff = ring_falloff};
    request.starfield = {.width = width,
                         .height = height,
                         .seed = seed,
                         .star_count = star_count,
                         .minimum_brightness = minimum_brightness,
                         .minimum_radius = minimum_star_radius,
                         .maximum_radius = maximum_star_radius,
                         .transparent_background = transparent_background};
    request.noise = {.width = width,
                     .height = height,
                     .seed = seed,
                     .base_scale = noise_base_scale,
                     .octave_count = noise_octave_count,
                     .persistence = noise_persistence,
                     .tileable = tileable_noise};
    request.domain_warped_noise = {.width = width,
                                   .height = height,
                                   .base_seed = domain_base_seed,
                                   .warp_seed = domain_warp_seed,
                                   .base_scale = domain_base_scale,
                                   .warp_scale = domain_warp_scale,
                                   .warp_strength = domain_warp_strength,
                                   .base_octave_count = domain_base_octave_count,
                                   .warp_octave_count = domain_warp_octave_count,
                                   .persistence = domain_persistence,
                                   .tileable = tileable_domain_noise};
    request.curl_noise_flow = {.width = width,
                               .height = height,
                               .seed = flow_seed,
                               .base_scale = flow_base_scale,
                               .octave_count = flow_octave_count,
                               .persistence = flow_persistence,
                               .derivative_step = flow_derivative_step,
                               .strength = flow_strength,
                               .tileable = tileable_flow};
    request.cellular_noise = {.width = width,
                              .height = height,
                              .seed = cellular_seed,
                              .cell_size = cellular_cell_size,
                              .jitter = cellular_jitter,
                              .mode = to_cellular_mode(cellular_mode),
                              .edge_width = cellular_edge_width,
                              .falloff = cellular_falloff,
                              .tileable = tileable_cellular};
    request.hex_grid = {.width = width,
                        .height = height,
                        .cell_radius = hex_cell_radius,
                        .line_thickness = hex_line_thickness,
                        .falloff = hex_falloff};
    request.post_process = {.invert = invert,
                            .contrast = contrast,
                            .threshold_enabled = threshold_enabled,
                            .threshold = threshold,
                            .threshold_softness = threshold_softness,
                            .output = output == EGenLabOutput::Scalar
                                        ? FImagePostProcessParameters::EOutput::Scalar
                                        : FImagePostProcessParameters::EOutput::NormalMap,
                            .normal_strength = normal_strength,
                            .normal_wrap = normal_wrap};
    return request;
}

void UGenLabSettings::load_request(SandboxImages::GenLab::FGenerationRequest const& request) {
    using namespace SandboxImages::GenLab;
    generator = to_editor_generator(request.generator);
    output_name = request.output_name;
    invert = request.post_process.invert;
    contrast = request.post_process.contrast;
    threshold_enabled = request.post_process.threshold_enabled;
    threshold = request.post_process.threshold;
    threshold_softness = request.post_process.threshold_softness;
    output = request.post_process.output == FImagePostProcessParameters::EOutput::Scalar
               ? EGenLabOutput::Scalar
               : EGenLabOutput::NormalMap;
    normal_strength = request.post_process.normal_strength;
    normal_wrap = request.post_process.normal_wrap;
    switch (generator) {
        case EGenLabGenerator::RadialGradient:
            width = request.radial_gradient.width;
            height = request.radial_gradient.height;
            inner_radius = request.radial_gradient.inner_radius;
            outer_radius = request.radial_gradient.outer_radius;
            break;
        case EGenLabGenerator::RingMask:
            width = request.ring_mask.width;
            height = request.ring_mask.height;
            ring_radius = request.ring_mask.radius;
            ring_thickness = request.ring_mask.thickness;
            ring_falloff = request.ring_mask.falloff;
            break;
        case EGenLabGenerator::Starfield:
            width = request.starfield.width;
            height = request.starfield.height;
            seed = request.starfield.seed;
            star_count = request.starfield.star_count;
            minimum_brightness = request.starfield.minimum_brightness;
            minimum_star_radius = request.starfield.minimum_radius;
            maximum_star_radius = request.starfield.maximum_radius;
            transparent_background = request.starfield.transparent_background;
            break;
        case EGenLabGenerator::Noise:
            width = request.noise.width;
            height = request.noise.height;
            seed = request.noise.seed;
            noise_base_scale = request.noise.base_scale;
            noise_octave_count = request.noise.octave_count;
            noise_persistence = request.noise.persistence;
            tileable_noise = request.noise.tileable;
            break;
        case EGenLabGenerator::DomainWarpedNoise:
            width = request.domain_warped_noise.width;
            height = request.domain_warped_noise.height;
            domain_base_seed = request.domain_warped_noise.base_seed;
            domain_warp_seed = request.domain_warped_noise.warp_seed;
            domain_base_scale = request.domain_warped_noise.base_scale;
            domain_warp_scale = request.domain_warped_noise.warp_scale;
            domain_warp_strength = request.domain_warped_noise.warp_strength;
            domain_base_octave_count = request.domain_warped_noise.base_octave_count;
            domain_warp_octave_count = request.domain_warped_noise.warp_octave_count;
            domain_persistence = request.domain_warped_noise.persistence;
            tileable_domain_noise = request.domain_warped_noise.tileable;
            break;
        case EGenLabGenerator::CurlNoiseFlow:
            width = request.curl_noise_flow.width;
            height = request.curl_noise_flow.height;
            flow_seed = request.curl_noise_flow.seed;
            flow_base_scale = request.curl_noise_flow.base_scale;
            flow_octave_count = request.curl_noise_flow.octave_count;
            flow_persistence = request.curl_noise_flow.persistence;
            flow_derivative_step = request.curl_noise_flow.derivative_step;
            flow_strength = request.curl_noise_flow.strength;
            tileable_flow = request.curl_noise_flow.tileable;
            break;
        case EGenLabGenerator::CellularNoise:
            width = request.cellular_noise.width;
            height = request.cellular_noise.height;
            cellular_seed = request.cellular_noise.seed;
            cellular_cell_size = request.cellular_noise.cell_size;
            cellular_jitter = request.cellular_noise.jitter;
            cellular_mode = request.cellular_noise.mode == ECellularMode::Distance
                              ? EGenLabCellularMode::Distance
                              : EGenLabCellularMode::Borders;
            cellular_edge_width = request.cellular_noise.edge_width;
            cellular_falloff = request.cellular_noise.falloff;
            tileable_cellular = request.cellular_noise.tileable;
            break;
        case EGenLabGenerator::HexGrid:
            width = request.hex_grid.width;
            height = request.hex_grid.height;
            hex_cell_radius = request.hex_grid.cell_radius;
            hex_line_thickness = request.hex_grid.line_thickness;
            hex_falloff = request.hex_grid.falloff;
            break;
    }
}

void UGenLabSettings::load_generator_defaults() {
    using namespace SandboxImages::GenLab;
    auto request{SandboxImages::GenLab::make_default_request(to_generator_type(generator))};
    request.post_process = {.invert = invert,
                            .contrast = contrast,
                            .threshold_enabled = threshold_enabled,
                            .threshold = threshold,
                            .threshold_softness = threshold_softness,
                            .output = output == EGenLabOutput::Scalar
                                        ? FImagePostProcessParameters::EOutput::Scalar
                                        : FImagePostProcessParameters::EOutput::NormalMap,
                            .normal_strength = normal_strength,
                            .normal_wrap = normal_wrap};
    if (generator == EGenLabGenerator::CurlNoiseFlow) {
        request.post_process.output =
            SandboxImages::GenLab::FImagePostProcessParameters::EOutput::Scalar;
    }
    load_request(request);
}
