#pragma once

#include <cstdint>

namespace ioj::gfx {
struct StarfieldGenerationSettings {
    std::int32_t star_count{10000};
    std::int32_t random_seed{1337};
};

struct StarfieldStarSettings {
    float scale{1.0f};
    float size_multiplier{1.0f};
    float global_brightness{8.0f};
    float colour_variation_strength{0.35f};
    float parallax_strength{0.01f};
};

struct StarfieldGalacticBandSettings {
    float strength{0.65f};
    float width_degrees{15.0f};
};

struct StarfieldStellarClusterSettings {
    float strength{0.12f};
    float width_degrees{4.0f};
};

struct StarfieldDustLaneSettings {
    float strength{0.9f};
    float width_degrees{5.0f};
    float irregularity{0.7f};
};

struct StarfieldBrightStarSettings {
    float fraction{0.01f};
    float size_multiplier{1.75f};
    float brightness_multiplier{2.0f};
    float shape_strength{0.25f};
};

struct StarfieldTwinkleSettings {
    float strength{0.1f};
    float speed{0.35f};
};

struct StarfieldGalacticHazeSettings {
    float strength{0.15f};
    float width_degrees{18.0f};
};

struct StarfieldGalacticCoreSettings {
    float strength{0.65f};
    float width_degrees{32.0f};
};

struct StarfieldNebularKnotSettings {
    float strength{0.18f};
    float size_degrees{9.0f};
};

struct StarfieldSettings {
    StarfieldGenerationSettings generation;
    StarfieldStarSettings star;
    StarfieldGalacticBandSettings galactic_band;
    StarfieldStellarClusterSettings stellar_cluster;
    StarfieldDustLaneSettings dust_lane;
    StarfieldBrightStarSettings bright_star;
    StarfieldTwinkleSettings twinkle;
    StarfieldGalacticHazeSettings galactic_haze;
    StarfieldGalacticCoreSettings galactic_core;
    StarfieldNebularKnotSettings nebular_knot;
};
}
