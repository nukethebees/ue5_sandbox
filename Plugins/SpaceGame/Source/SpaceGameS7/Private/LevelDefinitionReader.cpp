#include <SpaceGameS7/LevelDefinitionReader.h>

#include <S7Lab/Interpreter.h>
#include <S7Lab/NativeApi.h>

#include <Containers/StringConv.h>
#include <Misc/FileHelper.h>

namespace ml::s7 {
namespace {
namespace s7_native = S7Lab::native;

constexpr TCHAR level_prelude[]{LR"(
(define (level . clauses) (cons 'level clauses))
(define (title value) (list 'title value))
(define (description value) (list 'description value))
(define (teams . values) (cons 'teams values))
(define (team id) (list 'team id))
(define (player id) (list 'player id))
(define (entities . values) (cons 'entities values))
(define (entity id archetype team position rotation)
  (list 'entity id archetype team position rotation))
(define (position x y z) (list 'position x y z))
(define (rotation pitch yaw roll) (list 'rotation pitch yaw roll))
)"};

auto to_fstring(ANSICHAR const* const value) -> FString {
    auto const converted{FUTF8ToTCHAR{value}};
    return FString{converted.Length(), converted.Get()};
}

class FDefinitionDecoder final {
  public:
    FDefinitionDecoder(s7_scheme& scheme, s7_native::FValue const root)
        : scheme_{scheme}
        , root_{root} {}

    auto decode() -> FLevelDefinitionReadResult {
        if (!expect_tagged_list(root_, TEXT("level"), TEXT("level"))) {
            return {.decode_errors = MoveTemp(errors_)};
        }

        FLevelBuilder builder;
        FLevelMetadata metadata;
        auto const clause_count{list_length(root_) - 1};
        bool has_title{false};
        bool has_description{false};
        bool has_teams{false};
        bool has_player{false};
        bool has_entities{false};
        for (int64 i{0}; i < clause_count; ++i) {
            auto const clause{list_value(root_, i + 1)};
            auto const path{FString::Printf(TEXT("level[%lld]"), i)};
            if (!is_non_empty_list(clause)) {
                add_error(path, TEXT("Expected a level clause"));
                continue;
            }

            auto const tag{list_value(clause, 0)};
            if (!s7_native::is_symbol(tag)) {
                add_error(path, TEXT("Level clause tag must be a symbol"));
                continue;
            }

            auto const tag_name{to_fstring(s7_native::symbol_name(tag))};
            if (tag_name == TEXT("title")) {
                if (has_title) {
                    add_error(path, TEXT("Duplicate title clause"));
                    continue;
                }
                has_title = true;
                read_text_clause(clause, path, metadata.title);
            } else if (tag_name == TEXT("description")) {
                if (has_description) {
                    add_error(path, TEXT("Duplicate description clause"));
                    continue;
                }
                has_description = true;
                read_text_clause(clause, path, metadata.description);
            } else if (tag_name == TEXT("teams")) {
                if (has_teams) {
                    add_error(path, TEXT("Duplicate teams clause"));
                    continue;
                }
                has_teams = true;
                read_teams(clause, path, builder);
            } else if (tag_name == TEXT("player")) {
                if (has_player) {
                    add_error(path, TEXT("Duplicate player clause"));
                    continue;
                }
                has_player = true;
                FName id;
                if (expect_length(clause, 2, path) &&
                    read_symbol(list_value(clause, 1), path + TEXT(".id"), id)) {
                    builder.set_player_entity(FLevelEntityId{id});
                }
            } else if (tag_name == TEXT("entities")) {
                if (has_entities) {
                    add_error(path, TEXT("Duplicate entities clause"));
                    continue;
                }
                has_entities = true;
                read_entities(clause, path, builder);
            } else {
                add_error(path, FString::Printf(TEXT("Unknown level clause '%s'"), *tag_name));
            }
        }

        builder.set_metadata(metadata);
        if (!errors_.IsEmpty()) {
            return {.decode_errors = MoveTemp(errors_)};
        }

        auto definition{builder.finish()};
        auto validation{validate_level(definition)};
        if (!validation) {
            return {.validation_errors = MoveTemp(validation.errors)};
        }

        FLevelDefinitionReadResult result;
        result.definition.Emplace(MoveTemp(definition));
        return result;
    }
  private:
    auto list_length(s7_native::FValue const value) const -> int64 {
        return s7_native::list_length(scheme_, value);
    }

    auto list_value(s7_native::FValue const value, int64 const index) const -> s7_native::FValue {
        return s7_native::list_value(scheme_, value, index);
    }

    auto is_non_empty_list(s7_native::FValue const value) const -> bool {
        return s7_native::is_list(scheme_, value) && list_length(value) > 0;
    }

    void add_error(FString const& path, FString message) {
        errors_.Add(FLevelDefinitionDecodeError{.path = path, .message = MoveTemp(message)});
    }

    auto expect_length(s7_native::FValue const value, int64 const expected, FString const& path)
        -> bool {
        if (!s7_native::is_list(scheme_, value)) {
            add_error(path, TEXT("Expected a list"));
            return false;
        }

        auto const actual{list_length(value)};
        if (actual != expected) {
            add_error(
                path,
                FString::Printf(TEXT("Expected %lld values but found %lld"), expected, actual));
            return false;
        }
        return true;
    }

    auto expect_tagged_list(s7_native::FValue const value,
                            FString const& expected_tag,
                            FString const& path) -> bool {
        if (!is_non_empty_list(value)) {
            add_error(path, TEXT("Expected a non-empty list"));
            return false;
        }

        auto const tag{list_value(value, 0)};
        if (!s7_native::is_symbol(tag) || to_fstring(s7_native::symbol_name(tag)) != expected_tag) {
            add_error(path, FString::Printf(TEXT("Expected a '%s' value"), *expected_tag));
            return false;
        }
        return true;
    }

    auto read_symbol(s7_native::FValue const value, FString const& path, FName& output) -> bool {
        if (!s7_native::is_symbol(value)) {
            add_error(path, TEXT("Expected a symbol"));
            return false;
        }
        output = FName{to_fstring(s7_native::symbol_name(value))};
        return true;
    }

    auto read_string(s7_native::FValue const value, FString const& path, FString& output) -> bool {
        if (!s7_native::is_string(value)) {
            add_error(path, TEXT("Expected a string"));
            return false;
        }
        output = to_fstring(s7_native::string_value(value));
        return true;
    }

    auto read_number(s7_native::FValue const value, FString const& path, double& output) -> bool {
        if (!s7_native::is_real(value)) {
            add_error(path, TEXT("Expected a real number"));
            return false;
        }
        output = s7_native::number_to_real(scheme_, value);
        return true;
    }

    void read_text_clause(s7_native::FValue const clause, FString const& path, FString& output) {
        if (expect_length(clause, 2, path)) {
            read_string(list_value(clause, 1), path + TEXT(".value"), output);
        }
    }

    void read_teams(s7_native::FValue const clause, FString const& path, FLevelBuilder& builder) {
        auto const count{list_length(clause) - 1};
        for (int64 i{0}; i < count; ++i) {
            auto const value{list_value(clause, i + 1)};
            auto const team_path{FString::Printf(TEXT("%s[%lld]"), *path, i)};
            if (!expect_tagged_list(value, TEXT("team"), team_path) ||
                !expect_length(value, 2, team_path)) {
                continue;
            }

            FName id;
            if (read_symbol(list_value(value, 1), team_path + TEXT(".id"), id)) {
                builder.add_team(FTeamDefinition{.id = FLevelTeamId{id}});
            }
        }
    }

    auto read_vector(s7_native::FValue const value,
                     FString const& tag,
                     FString const& path,
                     double (&components)[3]) -> bool {
        if (!expect_tagged_list(value, tag, path) || !expect_length(value, 4, path)) {
            return false;
        }

        bool valid{true};
        for (int64 i{0}; i < 3; ++i) {
            valid = read_number(list_value(value, i + 1),
                                FString::Printf(TEXT("%s[%lld]"), *path, i),
                                components[i]) &&
                    valid;
        }
        return valid;
    }

    void
        read_entities(s7_native::FValue const clause, FString const& path, FLevelBuilder& builder) {
        auto const count{list_length(clause) - 1};
        for (int64 i{0}; i < count; ++i) {
            auto const value{list_value(clause, i + 1)};
            auto const entity_path{FString::Printf(TEXT("%s[%lld]"), *path, i)};
            if (!expect_tagged_list(value, TEXT("entity"), entity_path) ||
                !expect_length(value, 6, entity_path)) {
                continue;
            }

            FName id;
            FName archetype;
            FName team;
            double position[3]{};
            double rotation[3]{};
            auto valid{read_symbol(list_value(value, 1), entity_path + TEXT(".id"), id)};
            valid =
                read_symbol(list_value(value, 2), entity_path + TEXT(".archetype"), archetype) &&
                valid;
            valid = read_symbol(list_value(value, 3), entity_path + TEXT(".team"), team) && valid;
            valid = read_vector(list_value(value, 4),
                                TEXT("position"),
                                entity_path + TEXT(".position"),
                                position) &&
                    valid;
            valid = read_vector(list_value(value, 5),
                                TEXT("rotation"),
                                entity_path + TEXT(".rotation"),
                                rotation) &&
                    valid;
            if (!valid) {
                continue;
            }

            builder.add_entity(FEntitySpawnDefinition{
                .id = FLevelEntityId{id},
                .archetype = FEntityArchetypeId{archetype},
                .team = FLevelTeamId{team},
                .position = FVector{position[0], position[1], position[2]},
                .rotation = FRotator{rotation[0], rotation[1], rotation[2]},
            });
        }
    }

    s7_scheme& scheme_;
    s7_native::FValue root_{};
    TArray<FLevelDefinitionDecodeError> errors_{};
};
}

auto FLevelDefinitionReader::read_source(FStringView const source) const
    -> FLevelDefinitionReadResult {
    S7Lab::FInterpreter interpreter;
    FString expression{TEXT("(begin\n")};
    expression.Append(level_prelude);
    expression.AppendChars(source.GetData(), source.Len());
    expression.Append(TEXT("\n)"));

    FLevelDefinitionReadResult decoded;
    auto const evaluation{interpreter.evaluate_value(
        expression, [&decoded](s7_scheme& scheme, s7_native::FValue const value) {
            decoded = FDefinitionDecoder{scheme, value}.decode();
        })};
    if (!evaluation.succeeded) {
        return {.script_error = evaluation.error};
    }
    return decoded;
}

auto FLevelDefinitionReader::read_file(FStringView const path) const -> FLevelDefinitionReadResult {
    FString source;
    auto const owned_path{FString{path}};
    if (!FFileHelper::LoadFileToString(source, *owned_path)) {
        return {.script_error =
                    FString::Printf(TEXT("Could not read level script '%s'."), *owned_path)};
    }

    return read_source(source);
}
}
