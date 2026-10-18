#include <ioj/levels/authoring/starfield_reader.h>

#include "generated/starfield_fields.h"
#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/settings/settings.h>

#include <expected>
#include <string>
#include <utility>

namespace ioj::levels::authoring {
namespace {
auto invalid_number(std::string field, std::string message) -> std::unexpected<Diagnostic> {
    return std::unexpected{
        Diagnostic{DiagnosticCode::InvalidNumber, std::move(field), std::move(message)}};
}
auto must_be_positive(std::string field) -> std::unexpected<Diagnostic> {
    return invalid_number(std::move(field), "Must be positive.");
}
auto must_be_nonnegative(std::string field) -> std::unexpected<Diagnostic> {
    return invalid_number(std::move(field), "Must be nonnegative.");
}

auto decode_settings(settings::Settings const& assignments)
    -> std::expected<starfield::Settings, Diagnostic> {
    starfield::Settings result;
    for (auto const& setting : assignments) {
        if (!setting.path.starts_with("starfield.")) {
            continue;
        }

        auto const field{s7::starfield_fields::try_parse_fields_field(setting.path)};
        if (!field) {
            return std::unexpected{Diagnostic{
                DiagnosticCode::UnknownProperty, setting.path, "Unknown starfield setting."}};
        }

        auto const assigned{s7::starfield_fields::visit_fields_field(
            result, *field, [&setting](auto& member) { return setting.assign(member); })};
        if (!assigned) {
            return std::unexpected{Diagnostic{DiagnosticCode::InvalidType,
                                              setting.path,
                                              "Expected a number within the field's range."}};
        }
    }

    // Reject values that make allocation or angular distributions invalid.
    if (result.generation.star_count < 1 || result.generation.star_count > 1000000) {
        return invalid_number("starfield.generation.star-count", "Must be between 1 and 1000000.");
    }
    if (result.star.scale <= 0.0f) {
        return must_be_positive("starfield.star.scale");
    }
    if (result.star.size_multiplier < 0.0f) {
        return must_be_nonnegative("starfield.star.size-multiplier");
    }
    if (result.bright_star.size_multiplier < 0.0f) {
        return must_be_nonnegative("starfield.bright-star.size-multiplier");
    }
    if (result.galactic_band.width_degrees <= 0.0f || result.galactic_band.width_degrees > 90.0f) {
        return invalid_number("starfield.galactic-band.width-degrees",
                              "Must be positive and at most 90.");
    }
    if (result.stellar_cluster.width_degrees <= 0.0f) {
        return must_be_positive("starfield.stellar-cluster.width-degrees");
    }
    if (result.dust_lane.width_degrees <= 0.0f) {
        return must_be_positive("starfield.dust-lane.width-degrees");
    }
    if (result.galactic_haze.width_degrees <= 0.0f) {
        return must_be_positive("starfield.galactic-haze.width-degrees");
    }
    if (result.galactic_core.width_degrees <= 0.0f) {
        return must_be_positive("starfield.galactic-core.width-degrees");
    }
    if (result.nebular_knot.size_degrees <= 0.0f) {
        return must_be_positive("starfield.nebular-knot.size-degrees");
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
        auto error{std::move(settings.error())};
        error.source_path = path;
        return std::unexpected{Diagnostics{std::move(error)}};
    }

    return *settings;
}
}
