#include <sandbox/level_authoring/LevelDefinitionWriter.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <numeric>
#include <string_view>
#include <vector>

namespace ml::level_authoring {
namespace {
auto is_ascii_lower(char const value) -> bool {
    return value >= 'a' && value <= 'z';
}

auto is_ascii_digit(char const value) -> bool {
    return value >= '0' && value <= '9';
}

auto is_canonical_symbol(std::string_view const value) -> bool {
    if (value.empty() || !is_ascii_lower(value.front())) {
        return false;
    }
    return std::ranges::all_of(value, [](char const character) {
        return is_ascii_lower(character) || is_ascii_digit(character) || character == '-';
    });
}

auto validate_symbol(std::string const& value, std::string_view const owner) -> std::string {
    if (is_canonical_symbol(value)) {
        return {};
    }
    return std::string{owner} + " '" + value + "' is not a canonical lowercase Lisp symbol";
}

auto escape_string(std::string_view const value) -> std::string {
    std::string result;
    result.reserve(value.size());
    for (char const character : value) {
        switch (character) {
            case '\\':
                result += "\\\\";
                break;
            case '"':
                result += "\\\"";
                break;
            case '\n':
                result += "\\n";
                break;
            case '\r':
                result += "\\r";
                break;
            case '\t':
                result += "\\t";
                break;
            default:
                result += character;
                break;
        }
    }
    return result;
}

auto format_number(double const value) -> std::string {
    if (std::abs(value) < 0.0005) {
        return "0";
    }

    char buffer[64]{};
    auto const conversion{
        std::to_chars(std::begin(buffer), std::end(buffer), value, std::chars_format::fixed, 3)};
    std::string result{buffer, conversion.ptr};
    while (result.ends_with('0')) {
        result.pop_back();
    }
    if (result.ends_with('.')) {
        result.pop_back();
    }
    return result == "-0" ? "0" : result;
}

auto archetype_rank(std::string const& archetype) -> int {
    if (archetype == "player-fighter") {
        return 0;
    }
    if (archetype == "capital-ship") {
        return 1;
    }
    return 2;
}

void append_error(std::vector<std::string>& errors, std::string error) {
    if (!error.empty()) {
        errors.push_back(std::move(error));
    }
}

auto collect_errors(LevelDefinition const& definition) -> std::vector<std::string> {
    std::vector<std::string> errors;
    if (definition.metadata.par_time_seconds) {
        errors.emplace_back("Initial-state source does not support par-time");
    }
    if (!definition.unlock_level_ids.empty()) {
        errors.emplace_back("Initial-state source does not support unlock criteria");
    }
    if (definition.mission) {
        errors.emplace_back("Initial-state source does not support mission definitions");
    }
    if (!definition.mission_events.empty()) {
        errors.emplace_back("Initial-state source does not support mission events");
    }

    auto const validation{validate_level(definition)};
    for (auto const& error : validation.errors) {
        errors.push_back(error.message);
    }

    append_error(errors, validate_symbol(definition.metadata.id, "Level id"));
    for (auto const& team : definition.teams) {
        append_error(errors, validate_symbol(team, "Team id"));
    }
    if (!definition.player_entity_id.empty()) {
        append_error(errors, validate_symbol(definition.player_entity_id, "Player entity id"));
    }
    if (definition.camera) {
        for (auto const& target : definition.camera->target_entity_ids) {
            append_error(errors, validate_symbol(target, "Camera target id"));
        }
    }
    for (auto const& entity : definition.entities) {
        if (entity.spawn_time_seconds != 0.0) {
            errors.push_back("Entity '" + entity.id + "' is not an initial t=0 entity");
        }
        append_error(errors, validate_symbol(entity.id, "Entity id"));
        append_error(errors, validate_symbol(entity.archetype, "Entity archetype"));
        append_error(errors, validate_symbol(entity.team, "Entity team"));
    }
    return errors;
}

auto join_errors(std::vector<std::string> const& errors) -> std::string {
    std::string result;
    for (auto const& error : errors) {
        if (!result.empty()) {
            result += '\n';
        }
        result += error;
    }
    return result;
}
} // namespace

auto emit_initial_level_source(LevelDefinition const& definition)
    -> std::expected<std::string, std::string> {
    auto const errors{collect_errors(definition)};
    if (!errors.empty()) {
        return std::unexpected{join_errors(errors)};
    }

    std::string source{";; Initial-state seed exported from Unreal Editor.\n\n(level\n"};
    source += "  (id '" + definition.metadata.id + ")\n";
    source += "  (title \"" + escape_string(definition.metadata.title) + "\")\n";
    if (!definition.metadata.description.empty()) {
        source += "  (description \"" + escape_string(definition.metadata.description) + "\")\n";
    }

    auto teams{definition.teams};
    std::ranges::sort(teams);
    source += "\n  (teams\n";
    for (std::size_t index{}; index < teams.size(); ++index) {
        source += "    (team '" + teams[index] + ")";
        source += index + 1 == teams.size() ? ")\n" : "\n";
    }

    if (!definition.player_entity_id.empty()) {
        source += "\n  (player '" + definition.player_entity_id + ")\n";
    } else {
        auto targets{definition.camera->target_entity_ids};
        std::ranges::sort(targets);
        source += "\n  ;; Boilerplate observer camera for this playerless seed.\n";
        source += "  (camera\n    (look-at";
        for (auto const& target : targets) {
            source += " '" + target;
        }
        source += ")\n";
        source += "    (distance " + format_number(definition.camera->distance) + ")\n";
        source += "    (offset-direction " + format_number(definition.camera->offset_direction.x) +
                  " " + format_number(definition.camera->offset_direction.y) + " " +
                  format_number(definition.camera->offset_direction.z) + "))\n";
    }

    std::vector<std::size_t> indices(definition.entities.size());
    std::iota(indices.begin(), indices.end(), std::size_t{});
    std::ranges::sort(indices, [&](std::size_t const lhs, std::size_t const rhs) {
        auto const& left{definition.entities[lhs]};
        auto const& right{definition.entities[rhs]};
        if (left.team != right.team) {
            return left.team < right.team;
        }
        auto const left_rank{archetype_rank(left.archetype)};
        auto const right_rank{archetype_rank(right.archetype)};
        if (left_rank != right_rank) {
            return left_rank < right_rank;
        }
        return left.id < right.id;
    });

    source += "\n  (entities\n";
    std::string previous_team;
    std::string previous_archetype;
    for (std::size_t output_index{}; output_index < indices.size(); ++output_index) {
        auto const& entity{definition.entities[indices[output_index]]};
        if (entity.team != previous_team || entity.archetype != previous_archetype) {
            if (output_index > 0) {
                source += '\n';
            }
            std::size_t group_count{};
            for (std::size_t scan{output_index}; scan < indices.size(); ++scan) {
                auto const& candidate{definition.entities[indices[scan]]};
                if (candidate.team != entity.team || candidate.archetype != entity.archetype) {
                    break;
                }
                ++group_count;
            }
            source += "    ;; Team: " + entity.team + " | Archetype: " + entity.archetype +
                      " | Count: " + std::to_string(group_count) + "\n";
            previous_team = entity.team;
            previous_archetype = entity.archetype;
        }

        source += "    (entity '" + entity.id + " '" + entity.archetype + " '" + entity.team + "\n";
        source += "      (position " + format_number(entity.position.x) + " " +
                  format_number(entity.position.y) + " " + format_number(entity.position.z) + ")\n";
        source += "      (rotation " + format_number(entity.rotation.pitch) + " " +
                  format_number(entity.rotation.yaw) + " " + format_number(entity.rotation.roll);
        source += output_index + 1 == indices.size() ? "))))\n" : "))\n";
    }
    return source;
}
} // namespace ml::level_authoring
