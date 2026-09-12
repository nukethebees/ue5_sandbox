#include <SpaceGameS7/LevelDefinitionWriter.h>

#include <SpaceGame/levels/LevelEntityResolution.h>

#include <Algo/Sort.h>

namespace ml::s7 {
namespace {
auto is_ascii_lower(TCHAR const value) -> bool {
    return value >= TEXT('a') && value <= TEXT('z');
}

auto is_ascii_digit(TCHAR const value) -> bool {
    return value >= TEXT('0') && value <= TEXT('9');
}

auto is_canonical_symbol(FStringView const value) -> bool {
    if (value.IsEmpty() || !is_ascii_lower(value[0])) {
        return false;
    }
    for (TCHAR const character : value) {
        if (!is_ascii_lower(character) && !is_ascii_digit(character) && character != TEXT('-')) {
            return false;
        }
    }
    return true;
}

auto symbol_text(FName const value) -> FString {
    return value.ToString().ToLower();
}

auto validate_symbol(FName const value, FStringView const owner) -> FString {
    auto const text{symbol_text(value)};
    if (is_canonical_symbol(text)) {
        return {};
    }
    return FString::Printf(TEXT("%.*s '%s' is not a canonical lowercase Lisp symbol"),
                           owner.Len(),
                           owner.GetData(),
                           *value.ToString());
}

auto escape_string(FStringView const value) -> FString {
    FString result;
    result.Reserve(value.Len());
    for (TCHAR const character : value) {
        switch (character) {
            case TEXT('\\'):
                result += TEXT("\\\\");
                break;
            case TEXT('"'):
                result += TEXT("\\\"");
                break;
            case TEXT('\n'):
                result += TEXT("\\n");
                break;
            case TEXT('\r'):
                result += TEXT("\\r");
                break;
            case TEXT('\t'):
                result += TEXT("\\t");
                break;
            default:
                result.AppendChar(character);
                break;
        }
    }
    return result;
}

auto format_number(double const value) -> FString {
    if (FMath::Abs(value) < 0.0005) {
        return TEXT("0");
    }

    auto result{FString::Printf(TEXT("%.3f"), value)};
    while (result.EndsWith(TEXT("0"))) {
        result.LeftChopInline(1, EAllowShrinking::No);
    }
    if (result.EndsWith(TEXT("."))) {
        result.LeftChopInline(1, EAllowShrinking::No);
    }
    return result == TEXT("-0") ? FString{TEXT("0")} : result;
}

auto archetype_rank(FEntityArchetypeId const archetype) -> int32 {
    auto const resolved{resolve_level_archetype(archetype)};
    check(resolved.IsSet());
    switch (resolved.GetValue()) {
        case EResolvedLevelArchetype::PlayerFighter:
            return 0;
        case EResolvedLevelArchetype::CapitalShip:
            return 1;
        case EResolvedLevelArchetype::StaticTurret:
            return 2;
    }
    checkNoEntry();
    return 3;
}

auto compare_names(FName const lhs, FName const rhs) -> int32 {
    return symbol_text(lhs).Compare(symbol_text(rhs), ESearchCase::CaseSensitive);
}

auto collect_errors(FLevelDefinition const& definition) -> TArray<FString> {
    TArray<FString> errors;

    if (definition.metadata.par_time_seconds.IsSet()) {
        errors.Add(TEXT("Initial-state source does not support par-time"));
    }
    if (!definition.unlock_criteria.IsEmpty()) {
        errors.Add(TEXT("Initial-state source does not support unlock criteria"));
    }
    if (definition.mission.IsSet()) {
        errors.Add(TEXT("Initial-state source does not support mission definitions"));
    }
    if (!definition.mission_events.IsEmpty()) {
        errors.Add(TEXT("Initial-state source does not support mission events"));
    }

    auto const validation{validate_level(definition)};
    for (auto const& error : validation.errors) {
        errors.Add(error.message);
    }

    if (auto const error{validate_symbol(definition.metadata.id.value, TEXTVIEW("Level id"))};
        !error.IsEmpty()) {
        errors.Add(error);
    }
    for (auto const team : definition.teams) {
        if (auto const error{validate_symbol(team.value, TEXTVIEW("Team id"))}; !error.IsEmpty()) {
            errors.Add(error);
        }
    }
    if (definition.player_entity_id.is_set()) {
        if (auto const error{
                validate_symbol(definition.player_entity_id.value, TEXTVIEW("Player entity id"))};
            !error.IsEmpty()) {
            errors.Add(error);
        }
    }
    if (definition.camera.IsSet()) {
        for (auto const target : definition.camera->target_entity_ids) {
            if (auto const error{validate_symbol(target.value, TEXTVIEW("Camera target id"))};
                !error.IsEmpty()) {
                errors.Add(error);
            }
        }
    }

    auto const entities{definition.entities.get_const_view()};
    auto const entity_count{entities.num()};
    for (int32 i{}; i < entity_count; ++i) {
        if (entities.spawn_times_seconds[i] != 0.0) {
            errors.Add(FString::Printf(TEXT("Entity '%s' is not an initial t=0 entity"),
                                       *entities.ids[i].value.ToString()));
        }
        if (auto const error{validate_symbol(entities.ids[i].value, TEXTVIEW("Entity id"))};
            !error.IsEmpty()) {
            errors.Add(error);
        }
        if (auto const error{
                validate_symbol(entities.archetypes[i].value, TEXTVIEW("Entity archetype"))};
            !error.IsEmpty()) {
            errors.Add(error);
        }
        if (auto const error{validate_symbol(entities.teams[i].value, TEXTVIEW("Entity team"))};
            !error.IsEmpty()) {
            errors.Add(error);
        }
    }
    return errors;
}
}

auto emit_initial_level_source(FLevelDefinition const& definition)
    -> std::expected<FString, FString> {
    auto const errors{collect_errors(definition)};
    if (!errors.IsEmpty()) {
        return std::unexpected{FString::Join(errors, TEXT("\n"))};
    }

    FString source{TEXT(";; Initial-state seed exported from Unreal Editor.\n\n(level\n")};
    source += FString::Printf(TEXT("  (id '%s)\n"), *symbol_text(definition.metadata.id.value));
    source +=
        FString::Printf(TEXT("  (title \"%s\")\n"), *escape_string(definition.metadata.title));
    if (!definition.metadata.description.IsEmpty()) {
        source += FString::Printf(TEXT("  (description \"%s\")\n"),
                                  *escape_string(definition.metadata.description));
    }

    auto teams{definition.teams};
    teams.Sort([](FLevelTeamId const lhs, FLevelTeamId const rhs) {
        return compare_names(lhs.value, rhs.value) < 0;
    });
    source += TEXT("\n  (teams\n");
    auto const team_count{teams.Num()};
    for (int32 i{}; i < team_count; ++i) {
        source += FString::Printf(TEXT("    (team '%s)%s"),
                                  *symbol_text(teams[i].value),
                                  i + 1 == team_count ? TEXT(")\n") : TEXT("\n"));
    }

    if (definition.player_entity_id.is_set()) {
        source += FString::Printf(TEXT("\n  (player '%s)\n"),
                                  *symbol_text(definition.player_entity_id.value));
    } else {
        auto targets{definition.camera->target_entity_ids};
        targets.Sort([](FLevelEntityId const lhs, FLevelEntityId const rhs) {
            return compare_names(lhs.value, rhs.value) < 0;
        });

        source += TEXT("\n  ;; Boilerplate observer camera for this playerless seed.\n");
        source += TEXT("  (camera\n    (look-at");
        for (auto const target : targets) {
            source += FString::Printf(TEXT(" '%s"), *symbol_text(target.value));
        }
        source += TEXT(")\n");
        source += FString::Printf(TEXT("    (distance %s)\n"),
                                  *format_number(definition.camera->distance));
        source += FString::Printf(TEXT("    (offset-direction %s %s %s))\n"),
                                  *format_number(definition.camera->offset_direction.X),
                                  *format_number(definition.camera->offset_direction.Y),
                                  *format_number(definition.camera->offset_direction.Z));
    }

    auto const entities{definition.entities.get_const_view()};
    TArray<int32> entity_indices;
    auto const entity_count{entities.num()};
    entity_indices.Reserve(entity_count);
    for (int32 i{}; i < entity_count; ++i) {
        entity_indices.Add(i);
    }
    entity_indices.Sort([&](int32 const lhs, int32 const rhs) {
        auto const team_order{compare_names(entities.teams[lhs].value, entities.teams[rhs].value)};
        if (team_order != 0) {
            return team_order < 0;
        }
        auto const lhs_rank{archetype_rank(entities.archetypes[lhs])};
        auto const rhs_rank{archetype_rank(entities.archetypes[rhs])};
        if (lhs_rank != rhs_rank) {
            return lhs_rank < rhs_rank;
        }
        return compare_names(entities.ids[lhs].value, entities.ids[rhs].value) < 0;
    });

    source += TEXT("\n  (entities\n");
    FLevelTeamId previous_team;
    FEntityArchetypeId previous_archetype;
    for (int32 output_index{}; output_index < entity_count; ++output_index) {
        auto const index{entity_indices[output_index]};
        auto const team{entities.teams[index]};
        auto const archetype{entities.archetypes[index]};
        if (team != previous_team || archetype != previous_archetype) {
            if (output_index > 0) {
                source += TEXT("\n");
            }
            int32 group_count{};
            for (int32 scan{output_index}; scan < entity_count; ++scan) {
                auto const scan_index{entity_indices[scan]};
                if (entities.teams[scan_index] != team ||
                    entities.archetypes[scan_index] != archetype) {
                    break;
                }
                ++group_count;
            }
            source += FString::Printf(TEXT("    ;; Team: %s | Archetype: %s | Count: %d\n"),
                                      *symbol_text(team.value),
                                      *symbol_text(archetype.value),
                                      group_count);
            previous_team = team;
            previous_archetype = archetype;
        }

        source += FString::Printf(TEXT("    (entity '%s '%s '%s\n"),
                                  *symbol_text(entities.ids[index].value),
                                  *symbol_text(archetype.value),
                                  *symbol_text(team.value));
        source += FString::Printf(TEXT("      (position %s %s %s)\n"),
                                  *format_number(entities.positions.xs[index]),
                                  *format_number(entities.positions.ys[index]),
                                  *format_number(entities.positions.zs[index]));
        source += FString::Printf(TEXT("      (rotation %s %s %s)%s"),
                                  *format_number(entities.rotations.pitches[index]),
                                  *format_number(entities.rotations.yaws[index]),
                                  *format_number(entities.rotations.rolls[index]),
                                  output_index + 1 == entity_count ? TEXT(")))\n") : TEXT(")\n"));
    }
    return source;
}
}
