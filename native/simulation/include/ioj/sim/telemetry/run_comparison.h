#pragma once

namespace ioj::sim::telemetry {
struct RunComparison {
    bool different_level_source{};
    bool different_settings_source{};
    bool different_presentation_mode{};
};
}
