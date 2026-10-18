#include <ioj/levels/authoring/starfield_reader.h>

#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/settings/settings.h>

#include <expected>
#include <string>
#include <utility>

namespace ioj::levels::authoring {
namespace {
auto apply_setting(starfield::Settings& settings, ioj::settings::Setting const& setting) -> bool {
    auto const& path{setting.path};
    if (path == "starfield.generation.star-count") {
        return setting.assign(settings.generation.star_count);
    }
    if (path == "starfield.generation.random-seed") {
        return setting.assign(settings.generation.random_seed);
    }
    if (path == "starfield.star.scale") {
        return setting.assign(settings.star.scale);
    }
    if (path == "starfield.star.size-multiplier") {
        return setting.assign(settings.star.size_multiplier);
    }
    if (path == "starfield.star.global-brightness") {
        return setting.assign(settings.star.global_brightness);
    }
    if (path == "starfield.star.colour-variation-strength") {
        return setting.assign(settings.star.colour_variation_strength);
    }
    if (path == "starfield.star.parallax-strength") {
        return setting.assign(settings.star.parallax_strength);
    }
    if (path == "starfield.galactic-band.strength") {
        return setting.assign(settings.galactic_band.strength);
    }
    if (path == "starfield.galactic-band.width-degrees") {
        return setting.assign(settings.galactic_band.width_degrees);
    }
    if (path == "starfield.stellar-cluster.strength") {
        return setting.assign(settings.stellar_cluster.strength);
    }
    if (path == "starfield.stellar-cluster.width-degrees") {
        return setting.assign(settings.stellar_cluster.width_degrees);
    }
    if (path == "starfield.dust-lane.strength") {
        return setting.assign(settings.dust_lane.strength);
    }
    if (path == "starfield.dust-lane.width-degrees") {
        return setting.assign(settings.dust_lane.width_degrees);
    }
    if (path == "starfield.dust-lane.irregularity") {
        return setting.assign(settings.dust_lane.irregularity);
    }
    if (path == "starfield.bright-star.fraction") {
        return setting.assign(settings.bright_star.fraction);
    }
    if (path == "starfield.bright-star.size-multiplier") {
        return setting.assign(settings.bright_star.size_multiplier);
    }
    if (path == "starfield.bright-star.brightness-multiplier") {
        return setting.assign(settings.bright_star.brightness_multiplier);
    }
    if (path == "starfield.bright-star.shape-strength") {
        return setting.assign(settings.bright_star.shape_strength);
    }
    if (path == "starfield.twinkle.strength") {
        return setting.assign(settings.twinkle.strength);
    }
    if (path == "starfield.twinkle.speed") {
        return setting.assign(settings.twinkle.speed);
    }
    if (path == "starfield.galactic-haze.strength") {
        return setting.assign(settings.galactic_haze.strength);
    }
    if (path == "starfield.galactic-haze.width-degrees") {
        return setting.assign(settings.galactic_haze.width_degrees);
    }
    if (path == "starfield.galactic-core.strength") {
        return setting.assign(settings.galactic_core.strength);
    }
    if (path == "starfield.galactic-core.width-degrees") {
        return setting.assign(settings.galactic_core.width_degrees);
    }
    if (path == "starfield.nebular-knot.strength") {
        return setting.assign(settings.nebular_knot.strength);
    }
    if (path == "starfield.nebular-knot.size-degrees") {
        return setting.assign(settings.nebular_knot.size_degrees);
    }
    return false;
}

auto decode_settings(settings::Settings const& assignments)
    -> std::expected<starfield::Settings, std::string> {
    starfield::Settings result;
    for (auto const& setting : assignments) {
        if (!setting.path.starts_with("starfield.")) {
            continue;
        }
        if (!apply_setting(result, setting)) {
            return std::unexpected{"Unknown starfield setting or invalid value: " + setting.path};
        }
    }

    // Reject values that make allocation or angular distributions invalid.
    if (result.generation.star_count < 1 || result.generation.star_count > 1000000) {
        return std::unexpected{"starfield.generation.star-count must be between 1 and 1000000."};
    }
    if (result.star.scale <= 0.0f) {
        return std::unexpected{"starfield.star.scale must be positive."};
    }
    if (result.star.size_multiplier < 0.0f) {
        return std::unexpected{"starfield.star.size-multiplier must be nonnegative."};
    }
    if (result.bright_star.size_multiplier < 0.0f) {
        return std::unexpected{"starfield.bright-star.size-multiplier must be nonnegative."};
    }
    if (result.galactic_band.width_degrees <= 0.0f || result.galactic_band.width_degrees > 90.0f) {
        return std::unexpected{
            "starfield.galactic-band.width-degrees must be positive and at most 90."};
    }
    if (result.stellar_cluster.width_degrees <= 0.0f) {
        return std::unexpected{"starfield.stellar-cluster.width-degrees must be positive."};
    }
    if (result.dust_lane.width_degrees <= 0.0f) {
        return std::unexpected{"starfield.dust-lane.width-degrees must be positive."};
    }
    if (result.galactic_haze.width_degrees <= 0.0f) {
        return std::unexpected{"starfield.galactic-haze.width-degrees must be positive."};
    }
    if (result.galactic_core.width_degrees <= 0.0f) {
        return std::unexpected{"starfield.galactic-core.width-degrees must be positive."};
    }
    if (result.nebular_knot.size_degrees <= 0.0f) {
        return std::unexpected{"starfield.nebular-knot.size-degrees must be positive."};
    }

    return result;
}
}

auto read_starfield_settings_file(std::filesystem::path const& path,
                                  std::filesystem::path const& script_library_root)
    -> std::expected<starfield::Settings, Diagnostics> {
    auto assignments{DefinitionReader{script_library_root}.read_settings_file(path)};
    if (!assignments) {
        return std::unexpected{std::move(assignments.error())};
    }

    auto settings{decode_settings(*assignments)};
    if (!settings) {
        return std::unexpected{Diagnostics{
            {DiagnosticCode::InvalidNumber, "starfield", std::move(settings.error()), path}}};
    }

    return *settings;
}
}
