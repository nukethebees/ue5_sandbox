#include "SandboxEditor/levels/S7InitialStateExporter.h"

#include <SandboxGameShared/core/levels/levels.h>
#include <SpaceGame/defences/turrets/TestStaticTurretsProxy.h>
#include <SpaceGame/entities/TestEntity.h>
#include <SpaceGame/levels/LevelEntityResolution.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGameS7/LevelDefinitionWriter.h>
#include <SpaceGameS7/LevelScriptCatalog.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <DesktopPlatformModule.h>
#include <Editor.h>
#include <Engine/Level.h>
#include <Engine/World.h>
#include <Framework/Application/SlateApplication.h>
#include <GameFramework/Actor.h>
#include <IDesktopPlatform.h>
#include <Misc/FileHelper.h>
#include <Misc/PackageName.h>
#include <Misc/Paths.h>

namespace ml::editor {
namespace {
struct FExportCandidate {
    EResolvedLevelArchetype archetype{};
    FLevelTeamId team{};
    FTransform transform{FTransform::Identity};
    FString actor_label{};
    FString id_base{};
    FLevelEntityId id{};
};

auto count_text(int32 const count, FStringView const singular, FStringView const plural)
    -> FString {
    auto const text{count == 1 ? singular : plural};
    return FString::Printf(TEXT("%d %.*s"), count, text.Len(), text.GetData());
}

auto is_ascii_alpha(TCHAR const value) -> bool {
    return (value >= TEXT('a') && value <= TEXT('z')) || (value >= TEXT('A') && value <= TEXT('Z'));
}

auto is_ascii_digit(TCHAR const value) -> bool {
    return value >= TEXT('0') && value <= TEXT('9');
}

void append_separator(FString& value) {
    if (!value.IsEmpty() && !value.EndsWith(TEXT("-"))) {
        value.AppendChar(TEXT('-'));
    }
}

auto canonical_symbol(FStringView const value, FStringView const fallback) -> FString {
    FString result;
    result.Reserve(value.Len());
    TCHAR previous_input{};
    for (TCHAR const character : value) {
        if (!is_ascii_alpha(character) && !is_ascii_digit(character)) {
            append_separator(result);
            previous_input = character;
            continue;
        }

        if (character >= TEXT('A') && character <= TEXT('Z') &&
            ((previous_input >= TEXT('a') && previous_input <= TEXT('z')) ||
             is_ascii_digit(previous_input))) {
            append_separator(result);
        }
        result.AppendChar(FChar::ToLower(character));
        previous_input = character;
    }
    while (result.EndsWith(TEXT("-"))) {
        result.LeftChopInline(1, EAllowShrinking::No);
    }

    if (result.IsEmpty()) {
        result = FString{fallback};
    }
    if (!is_ascii_alpha(result[0])) {
        result = FString::Printf(TEXT("%.*s-%s"), fallback.Len(), fallback.GetData(), *result);
    }
    return result;
}

auto archetype_name(EResolvedLevelArchetype const archetype) -> FString {
    return to_level_archetype_id(archetype).value.ToString();
}

auto transform_is_finite(FTransform const& transform) -> bool {
    auto const location{transform.GetLocation()};
    auto const rotation{transform.Rotator()};
    return FMath::IsFinite(location.X) && FMath::IsFinite(location.Y) &&
           FMath::IsFinite(location.Z) && FMath::IsFinite(rotation.Pitch) &&
           FMath::IsFinite(rotation.Yaw) && FMath::IsFinite(rotation.Roll);
}

auto compare_candidates(FExportCandidate const& lhs, FExportCandidate const& rhs) -> bool {
    auto order{lhs.id_base.Compare(rhs.id_base, ESearchCase::CaseSensitive)};
    if (order != 0) {
        return order < 0;
    }
    order = lhs.actor_label.Compare(rhs.actor_label, ESearchCase::CaseSensitive);
    if (order != 0) {
        return order < 0;
    }
    order =
        lhs.team.value.ToString().Compare(rhs.team.value.ToString(), ESearchCase::CaseSensitive);
    if (order != 0) {
        return order < 0;
    }
    order = archetype_name(lhs.archetype)
                .Compare(archetype_name(rhs.archetype), ESearchCase::CaseSensitive);
    if (order != 0) {
        return order < 0;
    }

    auto const lhs_location{lhs.transform.GetLocation()};
    auto const rhs_location{rhs.transform.GetLocation()};
    if (lhs_location.X != rhs_location.X) {
        return lhs_location.X < rhs_location.X;
    }
    if (lhs_location.Y != rhs_location.Y) {
        return lhs_location.Y < rhs_location.Y;
    }
    if (lhs_location.Z != rhs_location.Z) {
        return lhs_location.Z < rhs_location.Z;
    }
    auto const lhs_rotation{lhs.transform.Rotator()};
    auto const rhs_rotation{rhs.transform.Rotator()};
    if (lhs_rotation.Pitch != rhs_rotation.Pitch) {
        return lhs_rotation.Pitch < rhs_rotation.Pitch;
    }
    if (lhs_rotation.Yaw != rhs_rotation.Yaw) {
        return lhs_rotation.Yaw < rhs_rotation.Yaw;
    }
    return lhs_rotation.Roll < rhs_rotation.Roll;
}

void count_ignored_properties(AActor const& actor, FS7InitialStateExportWarnings& warnings) {
    if (!actor.GetActorScale3D().Equals(FVector::OneVector, KINDA_SMALL_NUMBER)) {
        ++warnings.ignored_scale_actor_count;
    }
    if (actor.GetAttachParentActor()) {
        ++warnings.flattened_attachment_actor_count;
    }

    if (auto const* const capital{Cast<ATestCapitalShipProxy>(&actor)}) {
        warnings.ignored_property_override_count += capital->get_health().IsSet() ? 1 : 0;
        warnings.ignored_property_override_count +=
            capital->get_initial_spawn_delay().IsSet() ? 1 : 0;
        warnings.ignored_property_override_count += capital->get_spawn_cooldown().IsSet() ? 1 : 0;
        warnings.ignored_property_override_count += IsValid(capital->get_target_ship()) ? 1 : 0;
    } else if (auto const* const turret{Cast<ATestStaticTurretsProxy>(&actor)}) {
        warnings.ignored_property_override_count += turret->get_health().IsSet() ? 1 : 0;
        warnings.ignored_property_override_count += turret->get_laser_damage().IsSet() ? 1 : 0;
    }
}

auto select_export_path(ULevel const& level) -> TOptional<FString> {
    auto* const desktop_platform{FDesktopPlatformModule::Get()};
    if (!desktop_platform) {
        return NullOpt;
    }

    auto const level_name{FPackageName::GetShortName(level.GetOutermost()->GetName())};
    auto const default_filename{level_name + TEXT(".scm")};
    auto const parent_window{
        FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr)};
    TArray<FString> filenames;
    auto const selected{desktop_platform->SaveFileDialog(parent_window,
                                                         TEXT("Export S7 Initial State"),
                                                         s7::default_level_script_directory(),
                                                         default_filename,
                                                         TEXT("S7 level (*.scm)|*.scm"),
                                                         EFileDialogFlags::None,
                                                         filenames)};
    if (!selected || filenames.Num() != 1) {
        return NullOpt;
    }

    auto path{MoveTemp(filenames[0])};
    if (FPaths::GetExtension(path).IsEmpty()) {
        path += TEXT(".scm");
    }
    return path;
}

auto metadata_for_path(FStringView const path) -> FLevelMetadata {
    auto const stem{FPaths::GetBaseFilename(FString{path})};
    auto display_stem{stem.Replace(TEXT("-"), TEXT("_"))};
    auto title{format_level_display_name(FName{display_stem})};
    if (title.IsEmpty()) {
        title = TEXT("Exported Level");
    }
    return {
        .id = FLevelId{FName{canonical_symbol(stem, TEXTVIEW("exported-level"))}},
        .title = MoveTemp(title),
    };
}

void log_export_error(FString const& message) {
    UE_LOG(LogSandbox, Error, TEXT("S7 initial-state export failed: %s"), *message);
}
}

auto FS7InitialStateExportWarnings::is_empty() const noexcept -> bool {
    return unsupported_actor_classes.IsEmpty() && invalid_team_actor_count == 0 &&
           invalid_transform_actor_count == 0 && ignored_scale_actor_count == 0 &&
           flattened_attachment_actor_count == 0 && ignored_property_override_count == 0;
}

auto FS7InitialStateExportWarnings::format() const -> FString {
    TArray<FString> lines;
    TArray<FName> class_names;
    unsupported_actor_classes.GenerateKeyArray(class_names);
    class_names.Sort(FNameLexicalLess{});
    for (auto const class_name : class_names) {
        auto const count{unsupported_actor_classes.FindChecked(class_name)};
        lines.Add(FString::Printf(TEXT("- %s (%s)"),
                                  *count_text(count,
                                              TEXTVIEW("unsupported level actor"),
                                              TEXTVIEW("unsupported level actors")),
                                  *class_name.ToString()));
    }
    if (invalid_team_actor_count > 0) {
        lines.Add(FString::Printf(TEXT("- %s skipped because of an invalid team"),
                                  *count_text(invalid_team_actor_count,
                                              TEXTVIEW("level actor was"),
                                              TEXTVIEW("level actors were"))));
    }
    if (invalid_transform_actor_count > 0) {
        lines.Add(FString::Printf(TEXT("- %s skipped because of a non-finite transform"),
                                  *count_text(invalid_transform_actor_count,
                                              TEXTVIEW("level actor was"),
                                              TEXTVIEW("level actors were"))));
    }
    if (ignored_scale_actor_count > 0) {
        lines.Add(FString::Printf(TEXT("- %s with non-unit scale (scale ignored)"),
                                  *count_text(ignored_scale_actor_count,
                                              TEXTVIEW("level actor"),
                                              TEXTVIEW("level actors"))));
    }
    if (flattened_attachment_actor_count > 0) {
        lines.Add(FString::Printf(TEXT("- %s with an attachment (world transform exported)"),
                                  *count_text(flattened_attachment_actor_count,
                                              TEXTVIEW("level actor"),
                                              TEXTVIEW("level actors"))));
    }
    if (ignored_property_override_count > 0) {
        lines.Add(FString::Printf(TEXT("- %s not represented by the initial-state schema"),
                                  *count_text(ignored_property_override_count,
                                              TEXTVIEW("per-instance property override"),
                                              TEXTVIEW("per-instance property overrides"))));
    }
    return FString::Join(lines, TEXT("\n"));
}

auto collect_s7_initial_state(ULevel const& level, FLevelMetadata const& metadata)
    -> std::expected<FS7InitialStateExportPlan, FString> {
    FS7InitialStateExportPlan plan;
    TArray<FExportCandidate> candidates;
    int32 player_actor_count{};

    for (auto const actor_ptr : level.Actors) {
        auto const* const actor{actor_ptr.Get()};
        if (!IsValid(actor) || actor->IsTemplate()) {
            continue;
        }

        EResolvedLevelArchetype archetype{};
        ETestTeam team{};
        if (auto const* const player{Cast<ATestSpaceShip>(actor)}) {
            archetype = EResolvedLevelArchetype::PlayerFighter;
            team = player->get_team();
            ++player_actor_count;
        } else if (auto const* const capital{Cast<ATestCapitalShipProxy>(actor)}) {
            archetype = EResolvedLevelArchetype::CapitalShip;
            team = capital->get_team();
        } else if (auto const* const turret{Cast<ATestStaticTurretsProxy>(actor)}) {
            archetype = EResolvedLevelArchetype::StaticTurret;
            team = turret->get_team();
        } else {
            if (Cast<ITestEntity>(actor)) {
                ++plan.warnings.unsupported_actor_classes.FindOrAdd(actor->GetClass()->GetFName());
            }
            continue;
        }

        auto const level_team{to_level_team_id(team)};
        if (!level_team.IsSet()) {
            ++plan.warnings.invalid_team_actor_count;
            continue;
        }
        auto const transform{actor->GetActorTransform()};
        if (!transform_is_finite(transform)) {
            ++plan.warnings.invalid_transform_actor_count;
            continue;
        }

        count_ignored_properties(*actor, plan.warnings);
        auto const archetype_text{archetype_name(archetype)};
        auto const label{actor->GetActorLabel()};
        candidates.Add({
            .archetype = archetype,
            .team = level_team.GetValue(),
            .transform = transform,
            .actor_label = label,
            .id_base = canonical_symbol(label, archetype_text),
        });
    }

    if (player_actor_count > 1) {
        return std::unexpected{
            FString::Printf(TEXT("The current level contains %d player-fighter actors; only one "
                                 "can be represented."),
                            player_actor_count)};
    }
    if (candidates.IsEmpty()) {
        return std::unexpected{FString{TEXT("The current level contains no exportable entities.")}};
    }

    candidates.Sort(compare_candidates);
    TSet<FName> used_ids;
    used_ids.Reserve(candidates.Num());
    for (auto& candidate : candidates) {
        auto id_text{candidate.id_base};
        int32 suffix{2};
        while (used_ids.Contains(FName{id_text})) {
            id_text = FString::Printf(TEXT("%s-%d"), *candidate.id_base, suffix);
            ++suffix;
        }
        candidate.id = FLevelEntityId{FName{id_text}};
        used_ids.Add(candidate.id.value);
    }

    FLevelBuilder builder;
    builder.set_metadata(metadata);
    TSet<FLevelTeamId> used_teams;
    for (auto const& candidate : candidates) {
        used_teams.Add(candidate.team);
    }
    TArray<FLevelTeamId> teams{used_teams.Array()};
    teams.Sort([](FLevelTeamId const lhs, FLevelTeamId const rhs) {
        return lhs.value.LexicalLess(rhs.value);
    });
    for (auto const team : teams) {
        builder.add_team(team);
    }

    TOptional<FLevelEntityId> player_id{NullOpt};
    for (auto const& candidate : candidates) {
        builder.add_entity({
            .id = candidate.id,
            .archetype = to_level_archetype_id(candidate.archetype),
            .team = candidate.team,
            .position = candidate.transform.GetLocation(),
            .rotation = candidate.transform.Rotator(),
        });
        if (candidate.archetype == EResolvedLevelArchetype::PlayerFighter) {
            player_id = candidate.id;
        }
    }

    if (player_id.IsSet()) {
        builder.set_player_entity(player_id.GetValue());
    } else {
        TArray<FLevelEntityId> targets;
        FVector focus{FVector::ZeroVector};
        for (auto const team : teams) {
            auto const* const representative{candidates.FindByPredicate(
                [team](FExportCandidate const& candidate) { return candidate.team == team; })};
            check(representative);
            targets.Add(representative->id);
            focus += representative->transform.GetLocation();
        }
        focus /= targets.Num();

        double maximum_radius{};
        for (auto const& candidate : candidates) {
            maximum_radius = FMath::Max(
                maximum_radius, FVector::Distance(focus, candidate.transform.GetLocation()));
        }
        auto const camera_distance{
            FMath::CeilToDouble(FMath::Max(100000.0, maximum_radius * 2.0) / 1000.0) * 1000.0};
        builder.set_camera({
            .target_entity_ids = MoveTemp(targets),
            .offset_direction = FVector{-1.0, -1.0, 0.7},
            .distance = camera_distance,
        });
    }

    plan.definition = builder.finish();
    auto const validation{validate_level(plan.definition)};
    if (!validation) {
        TArray<FString> errors;
        for (auto const& error : validation.errors) {
            errors.Add(error.message);
        }
        return std::unexpected{FString::Join(errors, TEXT("\n"))};
    }
    return plan;
}

void execute_s7_initial_state_export() {
    if (!GEditor || IsValid(GEditor->PlayWorld)) {
        log_export_error(
            TEXT("Export is available only while editing a level outside Play In Editor."));
        return;
    }

    auto* const world{GEditor->GetEditorWorldContext().World()};
    auto* const level{IsValid(world) ? world->GetCurrentLevel() : nullptr};
    if (!IsValid(world) || !IsValid(level)) {
        log_export_error(TEXT("The current editor level is unavailable."));
        return;
    }

    auto const selected_path{select_export_path(*level)};
    if (!selected_path.IsSet()) {
        return;
    }

    auto const plan{collect_s7_initial_state(*level, metadata_for_path(selected_path.GetValue()))};
    if (!plan) {
        log_export_error(plan.error());
        return;
    }
    auto const source{s7::emit_initial_level_source(plan->definition)};
    if (!source) {
        log_export_error(source.error());
        return;
    }
    if (!FFileHelper::SaveStringToFile(*source,
                                       *selected_path.GetValue(),
                                       FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) {
        log_export_error(FString::Printf(TEXT("Could not write '%s'."), *selected_path.GetValue()));
        return;
    }

    auto const entity_count{plan->definition.entities.num()};
    auto summary{FString::Printf(
        TEXT("Exported %d entities to %s."), entity_count, *selected_path.GetValue())};
    if (!plan->warnings.is_empty()) {
        summary += TEXT("\n\nSkipped / unsupported:\n");
        summary += plan->warnings.format();
        UE_LOG(LogSandbox, Warning, TEXT("%s"), *summary);
    } else {
        UE_LOG(LogSandbox, Display, TEXT("%s"), *summary);
    }
}
}
