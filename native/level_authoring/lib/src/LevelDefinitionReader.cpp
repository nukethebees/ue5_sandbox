#include <sandbox/level_authoring/LevelDefinitionReader.h>

#include <native/s7/interpreter.h>
#include <native/s7/value.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <limits>
#include <utility>

namespace ml::level_authoring {
namespace level_definition_reader_detail {
inline constexpr std::string_view level_prelude{R"(
(define (level . clauses) (cons 'level clauses))
(define (id value) (list 'id value))
(define (title value) (list 'title value))
(define (description value) (list 'description value))
(define (par-time seconds) (list 'par-time seconds))
(define (unlock . criteria) (cons 'unlock criteria))
(define (level-completed level-id) (list 'level-completed level-id))
(define (teams . values) (cons 'teams values))
(define (team id) (list 'team id))
(define (player id) (list 'player id))
(define (camera targets camera-distance direction)
  (list 'camera targets camera-distance direction))
(define (look-at . ids) (cons 'look-at ids))
(define (distance value) (list 'distance value))
(define (offset-direction x y z) (list 'offset-direction x y z))
(define (mission . clauses) (cons 'mission clauses))
(define (mode value) (list 'mode value))
(define (time-limit seconds) (list 'time-limit seconds))
(define (kill-count value) (list 'kill-count value))
(define (heroes . ids) (cons 'heroes ids))
(define (must-survive . ids) (cons 'must-survive ids))
(define (required-kills . ids) (cons 'required-kills ids))
(define (mission-events . values) (cons 'mission-events values))
(define (mission-event event-time . clauses) (cons 'mission-event (cons event-time clauses)))
(define (at seconds) (list 'at seconds))
(define (add-must-survive . ids) (cons 'add-must-survive ids))
(define (add-required-kills . ids) (cons 'add-required-kills ids))
(define (increase-kill-count value) (list 'increase-kill-count value))
(define (entities . values) (cons 'entities values))
(define (entity id archetype team position rotation . clauses)
  (append (list 'entity id archetype team position rotation) clauses))
(define (position x y z) (list 'position x y z))
(define (rotation pitch yaw roll) (list 'rotation pitch yaw roll))
(define (spawn-at seconds) (list 'spawn-at seconds))
)"};
} // namespace level_definition_reader_detail

namespace {
auto indexed_path(std::string const& path, std::int64_t const index) -> std::string {
    return path + "[" + std::to_string(index) + "]";
}

void lowercase_ascii(std::string& value) {
    std::ranges::transform(value, value.begin(), [](unsigned char const character) {
        if (character >= 'A' && character <= 'Z') {
            return static_cast<char>(character - 'A' + 'a');
        }
        return static_cast<char>(character);
    });
}

class DefinitionDecoder final {
  public:
    DefinitionDecoder(s7::Scheme& scheme, s7::Value const root)
        : scheme_{scheme}
        , root_{root} {}

    auto decode() -> LevelDefinitionReadResult {
        if (!expect_tagged_list(root_, "level", "level")) {
            return {.decode_errors = std::move(errors_)};
        }

        auto const clause_count{list_length(root_) - 1};
        bool has_id{};
        bool has_title{};
        bool has_description{};
        bool has_par_time{};
        bool has_unlock{};
        bool has_teams{};
        bool has_player{};
        bool has_camera{};
        bool has_mission{};
        bool has_mission_events{};
        bool has_entities{};
        for (std::int64_t index{}; index < clause_count; ++index) {
            auto const clause{list_value(root_, index + 1)};
            auto const path{indexed_path("level", index)};
            if (!is_non_empty_list(clause)) {
                add_error(path, "Expected a level clause");
                continue;
            }

            auto const tag_value{list_value(clause, 0)};
            if (!s7::is_symbol(tag_value)) {
                add_error(path, "Level clause tag must be a symbol");
                continue;
            }

            auto const tag{std::string{s7::symbol_name(tag_value)}};
            if (tag == "id") {
                if (has_id) {
                    add_error(path, "Duplicate id clause");
                    continue;
                }
                has_id = true;
                if (expect_length(clause, 2, path)) {
                    read_symbol(list_value(clause, 1), path + ".value", definition_.metadata.id);
                }
            } else if (tag == "title") {
                if (has_title) {
                    add_error(path, "Duplicate title clause");
                    continue;
                }
                has_title = true;
                read_text_clause(clause, path, definition_.metadata.title);
            } else if (tag == "description") {
                if (has_description) {
                    add_error(path, "Duplicate description clause");
                    continue;
                }
                has_description = true;
                read_text_clause(clause, path, definition_.metadata.description);
            } else if (tag == "par-time") {
                if (has_par_time) {
                    add_error(path, "Duplicate par-time clause");
                    continue;
                }
                has_par_time = true;
                double seconds{};
                if (expect_length(clause, 2, path) &&
                    read_number(list_value(clause, 1), path + ".seconds", seconds)) {
                    definition_.metadata.par_time_seconds = static_cast<float>(seconds);
                }
            } else if (tag == "unlock") {
                if (has_unlock) {
                    add_error(path, "Duplicate unlock clause");
                    continue;
                }
                has_unlock = true;
                read_unlock(clause, path);
            } else if (tag == "teams") {
                if (has_teams) {
                    add_error(path, "Duplicate teams clause");
                    continue;
                }
                has_teams = true;
                read_teams(clause, path);
            } else if (tag == "player") {
                if (has_player) {
                    add_error(path, "Duplicate player clause");
                    continue;
                }
                has_player = true;
                if (expect_length(clause, 2, path)) {
                    read_symbol(list_value(clause, 1), path + ".id", definition_.player_entity_id);
                }
            } else if (tag == "camera") {
                if (has_camera) {
                    add_error(path, "Duplicate camera clause");
                    continue;
                }
                has_camera = true;
                read_camera(clause, path);
            } else if (tag == "mission") {
                if (has_mission) {
                    add_error(path, "Duplicate mission clause");
                    continue;
                }
                has_mission = true;
                read_mission(clause, path);
            } else if (tag == "mission-events") {
                if (has_mission_events) {
                    add_error(path, "Duplicate mission-events clause");
                    continue;
                }
                has_mission_events = true;
                read_mission_events(clause, path);
            } else if (tag == "entities") {
                if (has_entities) {
                    add_error(path, "Duplicate entities clause");
                    continue;
                }
                has_entities = true;
                read_entities(clause, path);
            } else {
                add_error(path, "Unknown level clause '" + tag + "'");
            }
        }

        if (!errors_.empty()) {
            return {.decode_errors = std::move(errors_)};
        }

        auto validation{validate_level(definition_)};
        if (!validation) {
            return {.validation_errors = std::move(validation.errors)};
        }
        return {.definition = std::move(definition_)};
    }
  private:
    auto list_length(s7::Value const value) const -> std::int64_t {
        return s7::list_length(scheme_, value);
    }

    auto list_value(s7::Value const value, std::int64_t const index) const -> s7::Value {
        return s7::list_value(scheme_, value, index);
    }

    auto is_non_empty_list(s7::Value const value) const -> bool {
        return s7::is_list(scheme_, value) && list_length(value) > 0;
    }

    void add_error(std::string path, std::string message) {
        errors_.push_back({std::move(path), std::move(message)});
    }

    auto expect_length(s7::Value const value, std::int64_t const expected, std::string const& path)
        -> bool {
        if (!s7::is_list(scheme_, value)) {
            add_error(path, "Expected a list");
            return false;
        }

        auto const actual{list_length(value)};
        if (actual != expected) {
            add_error(path,
                      "Expected " + std::to_string(expected) + " values but found " +
                          std::to_string(actual));
            return false;
        }
        return true;
    }

    auto expect_tagged_list(s7::Value const value,
                            std::string_view const expected_tag,
                            std::string const& path) -> bool {
        if (!is_non_empty_list(value)) {
            add_error(path, "Expected a non-empty list");
            return false;
        }

        auto const tag{list_value(value, 0)};
        if (!s7::is_symbol(tag) || s7::symbol_name(tag) != expected_tag) {
            add_error(path, "Expected a '" + std::string{expected_tag} + "' value");
            return false;
        }
        return true;
    }

    auto read_symbol(s7::Value const value, std::string const& path, std::string& output) -> bool {
        if (!s7::is_symbol(value)) {
            add_error(path, "Expected a symbol");
            return false;
        }
        output = s7::symbol_name(value);
        lowercase_ascii(output);
        return true;
    }

    auto read_string(s7::Value const value, std::string const& path, std::string& output) -> bool {
        if (!s7::is_string(value)) {
            add_error(path, "Expected a string");
            return false;
        }
        output = s7::string_value(value);
        return true;
    }

    auto read_number(s7::Value const value, std::string const& path, double& output) -> bool {
        if (!s7::is_real(value)) {
            add_error(path, "Expected a real number");
            return false;
        }
        output = s7::number_to_real(scheme_, value);
        return true;
    }

    auto read_int32(s7::Value const value, std::string const& path, std::int32_t& output) -> bool {
        double number{};
        if (!read_number(value, path, number)) {
            return false;
        }
        if (!std::isfinite(number) || std::trunc(number) != number ||
            number < std::numeric_limits<std::int32_t>::min() ||
            number > std::numeric_limits<std::int32_t>::max()) {
            add_error(path, "Expected a 32-bit integer");
            return false;
        }
        output = static_cast<std::int32_t>(number);
        return true;
    }

    void read_text_clause(s7::Value const clause, std::string const& path, std::string& output) {
        if (expect_length(clause, 2, path)) {
            read_string(list_value(clause, 1), path + ".value", output);
        }
    }

    void read_unlock(s7::Value const clause, std::string const& path) {
        auto const count{list_length(clause) - 1};
        if (count == 0) {
            add_error(path, "Unlock clause must contain at least one criterion");
            return;
        }

        for (std::int64_t index{}; index < count; ++index) {
            auto const value{list_value(clause, index + 1)};
            auto const criterion_path{indexed_path(path, index)};
            if (!is_non_empty_list(value)) {
                add_error(criterion_path, "Expected an unlock criterion");
                continue;
            }

            auto const tag_value{list_value(value, 0)};
            if (!s7::is_symbol(tag_value)) {
                add_error(criterion_path, "Unlock criterion tag must be a symbol");
                continue;
            }
            auto const tag{std::string{s7::symbol_name(tag_value)}};
            if (tag != "level-completed") {
                add_error(criterion_path, "Unknown unlock criterion '" + tag + "'");
                continue;
            }
            if (!expect_length(value, 2, criterion_path)) {
                continue;
            }

            std::string level_id;
            if (read_symbol(list_value(value, 1), criterion_path + ".level-id", level_id)) {
                definition_.unlock_level_ids.push_back(std::move(level_id));
            }
        }
    }

    void read_teams(s7::Value const clause, std::string const& path) {
        auto const count{list_length(clause) - 1};
        for (std::int64_t index{}; index < count; ++index) {
            auto const value{list_value(clause, index + 1)};
            auto const team_path{indexed_path(path, index)};
            if (!expect_tagged_list(value, "team", team_path) ||
                !expect_length(value, 2, team_path)) {
                continue;
            }

            std::string id;
            if (read_symbol(list_value(value, 1), team_path + ".id", id)) {
                definition_.teams.push_back(std::move(id));
            }
        }
    }

    void read_camera(s7::Value const clause, std::string const& path) {
        if (!expect_length(clause, 4, path)) {
            return;
        }

        auto const targets{list_value(clause, 1)};
        auto const targets_path{path + ".look-at"};
        if (!expect_tagged_list(targets, "look-at", targets_path)) {
            return;
        }

        ::ioj::sim::levels::LevelCameraDefinition camera;
        auto const target_count{list_length(targets) - 1};
        camera.target_entity_ids.reserve(static_cast<std::size_t>(target_count));
        bool valid{true};
        for (std::int64_t index{}; index < target_count; ++index) {
            std::string id;
            auto const target_valid{
                read_symbol(list_value(targets, index + 1), indexed_path(targets_path, index), id)};
            valid = target_valid && valid;
            if (target_valid) {
                camera.target_entity_ids.push_back(std::move(id));
            }
        }

        auto const distance_value{list_value(clause, 2)};
        auto const distance_path{path + ".distance"};
        auto distance_valid{expect_tagged_list(distance_value, "distance", distance_path)};
        distance_valid = expect_length(distance_value, 2, distance_path) && distance_valid;
        if (distance_valid) {
            distance_valid = read_number(
                list_value(distance_value, 1), distance_path + ".value", camera.distance);
        }
        valid = distance_valid && valid;

        double direction[3]{};
        valid =
            read_vector(
                list_value(clause, 3), "offset-direction", path + ".offset-direction", direction) &&
            valid;
        if (valid) {
            camera.offset_direction = {direction[0], direction[1], direction[2]};
            definition_.camera = std::move(camera);
        }
    }

    auto read_mission_mode(s7::Value const value,
                           std::string const& path,
                           ::ioj::sim::levels::LevelMissionMode& output) -> bool {
        std::string mode;
        if (!read_symbol(value, path, mode)) {
            return false;
        }
        if (mode == "survive-time") {
            output = ::ioj::sim::levels::LevelMissionMode::SurviveTime;
        } else if (mode == "kill-enemies") {
            output = ::ioj::sim::levels::LevelMissionMode::KillEnemies;
        } else if (mode == "kill-enemies-within-time") {
            output = ::ioj::sim::levels::LevelMissionMode::KillEnemiesWithinTime;
        } else {
            add_error(path, "Unknown mission mode '" + mode + "'");
            return false;
        }
        return true;
    }

    void read_entity_id_list(s7::Value const value,
                             std::string_view const tag,
                             std::string const& path,
                             std::vector<std::string>& output) {
        if (!expect_tagged_list(value, tag, path)) {
            return;
        }

        auto const count{list_length(value) - 1};
        output.reserve(static_cast<std::size_t>(count));
        for (std::int64_t index{}; index < count; ++index) {
            std::string id;
            if (read_symbol(list_value(value, index + 1), indexed_path(path, index), id)) {
                output.push_back(std::move(id));
            }
        }
    }

    void read_mission(s7::Value const clause, std::string const& path) {
        ::ioj::sim::levels::LevelMissionDefinition mission;
        bool has_mode{};
        bool has_time_limit{};
        bool has_kill_count{};
        bool has_heroes{};
        bool has_must_survive{};
        bool has_required_kills{};
        auto const clause_count{list_length(clause) - 1};
        for (std::int64_t index{}; index < clause_count; ++index) {
            auto const value{list_value(clause, index + 1)};
            auto const clause_path{indexed_path(path, index)};
            if (!is_non_empty_list(value)) {
                add_error(clause_path, "Expected a mission clause");
                continue;
            }

            auto const tag_value{list_value(value, 0)};
            if (!s7::is_symbol(tag_value)) {
                add_error(clause_path, "Mission clause tag must be a symbol");
                continue;
            }
            auto const tag{std::string{s7::symbol_name(tag_value)}};
            if (tag == "mode") {
                if (has_mode) {
                    add_error(clause_path, "Duplicate mission mode clause");
                    continue;
                }
                has_mode = true;
                if (expect_length(value, 2, clause_path)) {
                    read_mission_mode(list_value(value, 1), clause_path + ".value", mission.mode);
                }
            } else if (tag == "time-limit") {
                if (has_time_limit) {
                    add_error(clause_path, "Duplicate mission time-limit clause");
                    continue;
                }
                has_time_limit = true;
                double seconds{};
                if (expect_length(value, 2, clause_path) &&
                    read_number(list_value(value, 1), clause_path + ".seconds", seconds)) {
                    mission.time_limit_seconds = static_cast<float>(seconds);
                }
            } else if (tag == "kill-count") {
                if (has_kill_count) {
                    add_error(clause_path, "Duplicate mission kill-count clause");
                    continue;
                }
                has_kill_count = true;
                std::int32_t count{};
                if (expect_length(value, 2, clause_path) &&
                    read_int32(list_value(value, 1), clause_path + ".value", count)) {
                    mission.kill_count = count;
                }
            } else if (tag == "heroes") {
                if (has_heroes) {
                    add_error(clause_path, "Duplicate mission heroes clause");
                    continue;
                }
                has_heroes = true;
                read_entity_id_list(value, "heroes", clause_path, mission.hero_entity_ids);
            } else if (tag == "must-survive") {
                if (has_must_survive) {
                    add_error(clause_path, "Duplicate mission must-survive clause");
                    continue;
                }
                has_must_survive = true;
                read_entity_id_list(
                    value, "must-survive", clause_path, mission.must_survive_entity_ids);
            } else if (tag == "required-kills") {
                if (has_required_kills) {
                    add_error(clause_path, "Duplicate mission required-kills clause");
                    continue;
                }
                has_required_kills = true;
                read_entity_id_list(
                    value, "required-kills", clause_path, mission.required_kill_entity_ids);
            } else {
                add_error(clause_path, "Unknown mission clause '" + tag + "'");
            }
        }
        definition_.mission = std::move(mission);
    }

    void read_mission_events(s7::Value const clause, std::string const& path) {
        auto const event_count{list_length(clause) - 1};
        for (std::int64_t event_index{}; event_index < event_count; ++event_index) {
            auto const value{list_value(clause, event_index + 1)};
            auto const event_path{indexed_path(path, event_index)};
            if (!expect_tagged_list(value, "mission-event", event_path) || list_length(value) < 2) {
                continue;
            }

            ::ioj::sim::levels::LevelMissionObjectiveEvent event;
            auto const time{list_value(value, 1)};
            if (!expect_tagged_list(time, "at", event_path + ".at") ||
                !expect_length(time, 2, event_path + ".at") ||
                !read_number(list_value(time, 1), event_path + ".at.seconds", event.time_seconds)) {
                continue;
            }

            bool valid{true};
            auto const clause_count{list_length(value) - 2};
            for (std::int64_t index{}; index < clause_count; ++index) {
                auto const event_clause{list_value(value, index + 2)};
                auto const clause_path{indexed_path(event_path, index)};
                if (!is_non_empty_list(event_clause)) {
                    add_error(clause_path, "Expected a mission event clause");
                    valid = false;
                    continue;
                }
                auto const tag_value{list_value(event_clause, 0)};
                if (!s7::is_symbol(tag_value)) {
                    add_error(clause_path, "Mission event clause tag must be a symbol");
                    valid = false;
                    continue;
                }
                auto const tag{std::string{s7::symbol_name(tag_value)}};
                if (tag == "add-must-survive") {
                    read_entity_id_list(event_clause,
                                        "add-must-survive",
                                        clause_path,
                                        event.must_survive_entity_ids);
                } else if (tag == "add-required-kills") {
                    read_entity_id_list(event_clause,
                                        "add-required-kills",
                                        clause_path,
                                        event.required_kill_entity_ids);
                } else if (tag == "increase-kill-count") {
                    valid = expect_length(event_clause, 2, clause_path) && valid;
                    if (valid) {
                        valid = read_int32(list_value(event_clause, 1),
                                           clause_path + ".value",
                                           event.kill_target_increase) &&
                                valid;
                    }
                } else {
                    add_error(clause_path, "Unknown mission event clause '" + tag + "'");
                    valid = false;
                }
            }
            if (valid) {
                definition_.mission_events.push_back(std::move(event));
            }
        }
    }

    auto read_vector(s7::Value const value,
                     std::string_view const tag,
                     std::string const& path,
                     double (&components)[3]) -> bool {
        if (!expect_tagged_list(value, tag, path) || !expect_length(value, 4, path)) {
            return false;
        }

        bool valid{true};
        for (std::int64_t index{}; index < 3; ++index) {
            valid = read_number(list_value(value, index + 1),
                                indexed_path(path, index),
                                components[index]) &&
                    valid;
        }
        return valid;
    }

    void read_entities(s7::Value const clause, std::string const& path) {
        auto const count{list_length(clause) - 1};
        for (std::int64_t index{}; index < count; ++index) {
            auto const value{list_value(clause, index + 1)};
            auto const entity_path{indexed_path(path, index)};
            if (!expect_tagged_list(value, "entity", entity_path)) {
                continue;
            }
            auto const entity_length{list_length(value)};
            if (entity_length != 6 && entity_length != 7) {
                add_error(entity_path, "Expected an entity with zero or one spawn clause");
                continue;
            }

            ::ioj::sim::levels::EntitySpawnDefinition entity;
            double position[3]{};
            double rotation[3]{};
            auto valid{read_symbol(list_value(value, 1), entity_path + ".id", entity.id)};
            valid =
                read_symbol(list_value(value, 2), entity_path + ".archetype", entity.archetype) &&
                valid;
            valid = read_symbol(list_value(value, 3), entity_path + ".team", entity.team) && valid;
            valid = read_vector(
                        list_value(value, 4), "position", entity_path + ".position", position) &&
                    valid;
            valid = read_vector(
                        list_value(value, 5), "rotation", entity_path + ".rotation", rotation) &&
                    valid;
            if (entity_length == 7) {
                auto const spawn_at{list_value(value, 6)};
                auto const spawn_path{entity_path + ".spawn-at"};
                valid = expect_tagged_list(spawn_at, "spawn-at", spawn_path) && valid;
                valid = expect_length(spawn_at, 2, spawn_path) && valid;
                if (valid) {
                    valid = read_number(list_value(spawn_at, 1),
                                        spawn_path + ".seconds",
                                        entity.spawn_time_seconds) &&
                            valid;
                }
            }
            if (!valid) {
                continue;
            }

            entity.position = {position[0], position[1], position[2]};
            entity.rotation = {rotation[0], rotation[1], rotation[2]};
            definition_.entities.push_back(std::move(entity));
        }
    }

    s7::Scheme& scheme_;
    s7::Value root_{};
    ::ioj::sim::levels::LevelDefinition definition_{};
    std::vector<LevelDefinitionDecodeError> errors_{};
};
} // namespace

LevelDefinitionReader::LevelDefinitionReader(std::string script_library_root)
    : script_library_root_{std::move(script_library_root)} {}

auto LevelDefinitionReader::read_file(std::filesystem::path const& path) const
    -> LevelDefinitionReadResult {
    auto input{std::ifstream{path, std::ios::binary}};
    if (!input) {
        return {.script_error = "Unable to open level file: " + path.string()};
    }

    auto source{std::string{std::istreambuf_iterator<char>{input}, {}}};
    if (input.bad()) {
        return {.script_error = "Unable to read level file: " + path.string()};
    }

    if (!script_library_root_.empty()) {
        return read_source(source);
    }

    auto const library_root{path.parent_path() / "Libraries"};
    return LevelDefinitionReader{library_root.string()}.read_source(source);
}

auto LevelDefinitionReader::read_source(std::string_view const source) const
    -> LevelDefinitionReadResult {
    s7::InterpreterOptions options;
    if (!script_library_root_.empty()) {
        options.script_library_root_utf8 = script_library_root_;
    }
    s7::Interpreter interpreter{std::move(options)};

    std::string expression;
    expression.reserve(level_definition_reader_detail::level_prelude.size() + source.size() + 10);
    expression.append("(begin\n");
    expression.append(level_definition_reader_detail::level_prelude);
    expression.append(source);
    expression.append("\n)");

    LevelDefinitionReadResult decoded;
    auto const evaluation{interpreter.evaluate_value(
        expression, [&decoded](s7::Scheme& scheme, s7::Value const value) {
            decoded = DefinitionDecoder{scheme, value}.decode();
        })};
    if (!evaluation.succeeded) {
        return {.script_error = evaluation.error};
    }
    return decoded;
}
} // namespace ml::level_authoring
