#pragma once

#include <ioj/gfx/starfield_settings.h>
#include <ioj/settings/settings.h>

#include <expected>
#include <string>

namespace ioj::gfx {
inline auto apply_starfield_setting(StarfieldSettings& settings,
                                    ioj::settings::Setting const& setting) -> bool {
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

inline auto read_starfield_settings(settings::Settings const& assignments)
    -> std::expected<StarfieldSettings, std::string> {
    StarfieldSettings result;
    for (auto const& setting : assignments) {
        if (!apply_starfield_setting(result, setting)) {
            return std::unexpected{"Unknown starfield setting or invalid value: " + setting.path};
        }
    }

    // Reject values that make allocation or angular distributions invalid.
    if (result.generation.star_count < 1 || result.generation.star_count > 1000000 ||
        result.star.scale <= 0.0f || result.star.size_multiplier < 0.0f ||
        result.bright_star.size_multiplier < 0.0f || result.galactic_band.width_degrees <= 0.0f ||
        result.galactic_band.width_degrees > 90.0f ||
        result.stellar_cluster.width_degrees <= 0.0f || result.dust_lane.width_degrees <= 0.0f ||
        result.galactic_haze.width_degrees <= 0.0f || result.galactic_core.width_degrees <= 0.0f ||
        result.nebular_knot.size_degrees <= 0.0f) {
        return std::unexpected{"Starfield count must be 1..1000000, scale and angular widths must "
                               "be positive, and star sizes must be nonnegative."};
    }

    return result;
}
}
