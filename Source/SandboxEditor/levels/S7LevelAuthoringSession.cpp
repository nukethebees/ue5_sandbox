#include "SandboxEditor/levels/S7LevelAuthoringSession.h"

#include "SandboxEditor/levels/S7LevelAuthoringActors.h"
#include "SandboxEditor/levels/S7LevelAuthoringDocument.h"
#include <ioj/files.h>
#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/levels/authoring/identifiers.h>
#include <ioj/levels/authoring/level_definition_writer.h>
#include <ioj/levels/authoring/level_inputs_reader.h>
#include <SpaceGame/defences/spinners/TestTubeSpinnerProxy.h>
#include <SpaceGame/defences/turrets/TestStaticTurretsProxy.h>
#include <SpaceGame/s7/GameSettingsReader.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>

#include <SandboxCoreEngine/strings.h>

#include <Editor.h>
#include <Engine/Level.h>
#include <HAL/FileManager.h>
#include <Misc/FileHelper.h>
#include <Misc/PackageName.h>
#include <Misc/Paths.h>
#include <ScopedTransaction.h>

#include <algorithm>
#include <unordered_set>

namespace ioj::editor {
namespace s7_level_authoring_session_detail {
auto id(FString const& name) -> levels::EntityId {
    return levels::EntityId{to_string(name)};
}
template <typename T>
auto authored_value(T const& value) {
    auto const underlying{value.get()};
    if constexpr (requires { underlying.value(); }) {
        return underlying.value();
    } else {
        return underlying;
    }
}
template <typename T>
auto optional(std::optional<T> const& value) {
    using Value = decltype(authored_value(std::declval<T>()));
    return value ? TOptional<Value>{authored_value(*value)} : TOptional<Value>{};
}
auto save_generated(FString const& path, FString const& source) -> std::expected<void, FString> {
    FString existing;
    if (FFileHelper::LoadFileToString(existing, *path) && existing == source) {
        return {};
    }
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(path), true);
    if (!FFileHelper::SaveStringToFile(
            source, *path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) {
        return std::unexpected{
            FString::Printf(TEXT("Cannot write generated level file '%s'."), *path)};
    }
    return {};
}
}

auto s7_editor_config_path(AS7LevelAuthoringDocument const& document) -> FString {
    return FPaths::Combine(FPaths::GetPath(document.source_path),
                           FPaths::GetBaseFilename(document.source_path),
                           TEXT("editor-config.scm"));
}

auto read_s7_editor_input(AS7LevelAuthoringDocument const& document)
    -> std::expected<levels::authoring::LevelInput, FString> {
    auto const path{s7_editor_config_path(document)};
    auto source{read_file(std::filesystem::path{*path})};
    if (!source) {
        return std::unexpected{FString::Printf(TEXT("Cannot read generated file '%s'."), *path)};
    }
    auto ast{levels::authoring::DefinitionReader{
        std::filesystem::path{*FPaths::GetPath(document.source_path)}}
                 .read_source_ast(*source)};
    if (!ast) {
        return std::unexpected{to_fstring(levels::format_diagnostics(ast.error()))};
    }
    auto input{s7::read<levels::authoring::LevelInput>(s7::Reader{*ast}, ast->root, "level")};
    if (!input) {
        FString errors;
        for (auto const& error : input.error()) {
            errors += to_fstring(error.node_path + ": " + error.message + "\n");
        }
        return std::unexpected{MoveTemp(errors)};
    }
    return std::move(*input);
}

auto find_level_authoring_document(ULevel const& level)
    -> std::expected<AS7LevelAuthoringDocument*, FString> {
    AS7LevelAuthoringDocument* result{};
    for (auto const actor : level.Actors) {
        if (auto* document{Cast<AS7LevelAuthoringDocument>(actor.Get())}; IsValid(document)) {
            if (result) {
                return std::unexpected{TEXT("The level contains multiple S7 authoring documents.")};
            }
            result = document;
        }
    }
    return result;
}

auto adopt_unbound_level_entities(ULevel& level, AS7LevelAuthoringDocument& document) -> int32 {
    TSet<AActor const*> bound;
    std::unordered_set<levels::EntityId> ids;
    document.Modify();
    document.entities.RemoveAll([](auto const& binding) { return !IsValid(binding.actor); });
    for (auto const& binding : document.entities) {
        bound.Add(binding.actor);
        ids.insert(s7_level_authoring_session_detail::id(binding.id));
    }
    int32 count{};
    for (auto const actor : level.Actors) {
        auto const resolved{IsValid(actor) ? resolve_s7_level_actor(*actor) : NullOpt};
        if (!resolved || bound.Contains(actor)) {
            continue;
        }
        auto const entity_id{levels::authoring::allocate_entity_id(
            to_string(actor->GetActorLabel()), to_serialized_string(resolved->archetype), ids)};
        actor->Modify();
        actor->bIsEditorOnlyActor = true;
        document.entities.Add(
            {.id = to_fstring(entity_id.value), .actor = actor, .team = to_unreal(resolved->team)});
        ++count;
    }
    if (count > 0) {
        document.refresh_entity_previews();
    }
    return count;
}

auto collect_s7_editor_input(ULevel const& level, AS7LevelAuthoringDocument const& document)
    -> std::expected<levels::authoring::LevelInput, FString> {
    using namespace s7_level_authoring_session_detail;
    auto baseline{read_s7_editor_input(document)};
    if (!baseline) {
        return std::unexpected{baseline.error()};
    }
    auto& input{*baseline};
    input.entities.clear();
    input.mission.reset();
    input.player = {};
    input.camera.reset();
    TMap<AActor const*, levels::EntityId> ids;
    for (auto const& binding : document.entities) {
        auto* const actor{binding.actor.Get()};
        if (!IsValid(actor) || actor->GetLevel() != &level || binding.id.IsEmpty()) {
            return std::unexpected{
                TEXT("Entity bindings require a stable ID and an actor in this level.")};
        }
        auto const entity_id{id(binding.id)};
        ids.Add(actor, entity_id);
        if (!binding.export_to_script) {
            continue;
        }
        auto const resolved{resolve_s7_level_actor(*actor)};
        if (!resolved) {
            return std::unexpected{TEXT("Unsupported actor in an exported entity binding.")};
        }
        auto const rotation{actor->GetActorRotation()};
        auto const spawn_at{NonNegativeFinite<double>::try_make(binding.spawn_time_seconds)};
        if (!spawn_at) {
            return std::unexpected{FString::Printf(
                TEXT("Entity '%s' requires a finite nonnegative spawn time."), *binding.id)};
        }
        levels::authoring::EntityInput entity{
            .id = entity_id,
            .archetype = resolved->archetype,
            .team = to_native(binding.team),
            .position = Vector3d{actor->GetActorLocation()},
            .rotation = {rotation.Pitch, rotation.Yaw, rotation.Roll},
            .spawn_at = *spawn_at};
        TCHAR const* invalid_field{};
        auto const convert{[&]<typename T, typename U>(std::optional<T>& output,
                                                       TOptional<U> const& value,
                                                       TCHAR const* name) {
            if (!value.IsSet()) {
                return true;
            }
            output = T::try_make(typename T::value_type{*value});
            if (!output) {
                invalid_field = name;
            }
            return output.has_value();
        }};
        if (!convert(entity.health, binding.health, TEXT("health")) ||
            !convert(entity.laser_damage, binding.laser_damage, TEXT("laser damage")) ||
            !convert(entity.initial_spawn_delay,
                     binding.initial_spawn_delay,
                     TEXT("initial spawn delay")) ||
            !convert(entity.spawn_cooldown, binding.spawn_cooldown, TEXT("spawn cooldown")) ||
            !convert(entity.initial_fire_point,
                     binding.initial_fire_point,
                     TEXT("initial fire point"))) {
            return std::unexpected{FString::Printf(
                TEXT("Entity '%s' has an invalid %s override."), *binding.id, invalid_field)};
        }
        if (Cast<ATestSpaceShip>(actor)) {
            input.player = entity_id;
        }
        input.entities.push_back(std::move(entity));
    }
    std::ranges::sort(input.entities, {}, &levels::authoring::EntityInput::id);
    if (document.use_observer_camera) {
        input.player = {};
        input.camera = levels::LevelCameraDefinition{.offset_direction =
                                                         Vector3d{document.camera.offset_direction},
                                                     .distance = document.camera.distance};
        for (auto const actor : document.camera.targets) {
            auto const* entity{ids.Find(actor)};
            if (!entity) {
                return std::unexpected{TEXT("Camera target has no authored entity binding.")};
            }
            input.camera->target_entity_ids.push_back(*entity);
        }
    }
    return std::move(input);
}

auto export_s7_editor_config(ULevel& level, AS7LevelAuthoringDocument& document)
    -> std::expected<void, FString> {
    adopt_unbound_level_entities(level, document);
    auto input{collect_s7_editor_input(level, document)};
    if (!input) {
        return std::unexpected{input.error()};
    }
    return s7_level_authoring_session_detail::save_generated(
        s7_editor_config_path(document), to_fstring(levels::authoring::emit_level_source(*input)));
}

auto read_s7_authored_level(AS7LevelAuthoringDocument const& document)
    -> std::expected<levels::LevelDefinition, FString> {
    return levels::authoring::DefinitionReader{}
        .read_level_file(std::filesystem::path{*document.source_path})
        .transform_error(
            [](auto const& errors) { return to_fstring(levels::format_diagnostics(errors)); });
}

auto create_level_authoring_document(ULevel& level, FString source_path)
    -> std::expected<AS7LevelAuthoringDocument*, FString> {
    auto existing{find_level_authoring_document(level)};
    if (!existing || *existing) {
        return existing;
    }
    auto const map_name{FPackageName::GetShortName(level.GetOutermost()->GetName())};
    auto const root{
        source_path.IsEmpty()
            ? FPaths::Combine(FPaths::ProjectDir(), TEXT("LevelScripts"), map_name + TEXT(".scm"))
            : FPaths::ConvertRelativePathToFull(source_path)};
    auto const human_exists{IFileManager::Get().FileExists(*root)};
    FScopedTransaction transaction{NSLOCTEXT("S7LevelAuthoring", "Create", "Configure S7 Level")};
    auto* document{
        Cast<AS7LevelAuthoringDocument>(GEditor->AddActor(&level,
                                                          AS7LevelAuthoringDocument::StaticClass(),
                                                          FTransform::Identity,
                                                          true,
                                                          RF_Transactional,
                                                          false))};
    if (!IsValid(document)) {
        return std::unexpected{TEXT("Could not create the S7 authoring document.")};
    }
    document->source_path = root;
    document->level_config = s7_level_config::load_canonical();
    if (!IsValid(document->level_config)) {
        return std::unexpected{TEXT("Canonical level resources are unavailable.")};
    }
    auto settings{
        read_game_settings_source(TEXT("LevelScripts/Settings/base.scm"), *document->level_config)};
    if (!settings) {
        return std::unexpected{settings.error()};
    }
    levels::authoring::LevelInput input{
        .id = levels::LevelId{to_string(
            canonical_s7_level_entity_id(map_name, TEXTVIEW("level")).ToString())},
        .title = to_string(map_name),
        .settings = std::move(*settings)};
    std::expected<void, FString> saved;
    if (!IFileManager::Get().FileExists(*s7_editor_config_path(*document))) {
        saved = s7_level_authoring_session_detail::save_generated(
            s7_editor_config_path(*document),
            to_fstring(levels::authoring::emit_level_source(input)));
    }
    if (!saved) {
        return std::unexpected{saved.error()};
    }
    if (human_exists) {
        return document;
    }
    saved = export_s7_editor_config(level, *document);
    if (!saved) {
        return std::unexpected{saved.error()};
    }
    auto const source{FString::Printf(TEXT("(load-script \"%s/editor-config.scm\")\n"),
                                      *FPaths::GetBaseFilename(root))};
    if (!FFileHelper::SaveStringToFile(source,
                                       *root,
                                       FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
                                       &IFileManager::Get(),
                                       FILEWRITE_NoReplaceExisting)) {
        return std::unexpected{
            TEXT("Could not create the human level file; existing files are never replaced.")};
    }
    return document;
}

auto import_s7_level(ULevel& level,
                     AS7LevelAuthoringDocument& document,
                     levels::LevelDefinition const& definition,
                     bool const reset_export_ownership) -> std::expected<void, FString> {
    using namespace s7_level_authoring_session_detail;
    if (!IFileManager::Get().FileExists(*s7_editor_config_path(document))) {
        levels::authoring::LevelInput input{.settings = definition.settings()};
        auto saved{save_generated(s7_editor_config_path(document),
                                  to_fstring(levels::authoring::emit_level_source(input)))};
        if (!saved) {
            return saved;
        }
    }
    auto baseline{read_s7_editor_input(document)};
    if (!baseline) {
        return std::unexpected{baseline.error()};
    }
    TMap<FString, FS7LevelEntityBinding> existing;
    for (auto const& binding : document.entities) {
        existing.Add(binding.id, binding);
    }
    auto entities{baseline->entities};
    for (auto const& entity : definition.entities()) {
        if (std::ranges::find(baseline->entities, entity.id, &levels::authoring::EntityInput::id) ==
            baseline->entities.end()) {
            entities.push_back(levels::authoring::make_entity_input(entity));
        }
    }
    FScopedTransaction transaction{NSLOCTEXT("S7LevelAuthoring", "Import", "Import S7 Level")};
    TArray<FS7LevelEntityBinding> bindings;
    for (auto const& candidate : entities) {
        auto const name{to_fstring(candidate.id.value)};
        auto const* previous{existing.Find(name)};
        auto const generated{std::ranges::find(
            baseline->entities, candidate.id, &levels::authoring::EntityInput::id)};
        auto const export_entity{previous && !reset_export_ownership
                                     ? previous->export_to_script
                                     : generated != baseline->entities.end()};
        auto const authored{std::ranges::find(
            definition.entities(), candidate.id, &levels::EntitySpawnDefinition::id)};
        auto const entity{!export_entity && authored != definition.entities().end()
                              ? levels::authoring::make_entity_input(*authored)
                              : candidate};
        auto* actor{previous ? previous->actor.Get() : nullptr};
        auto* type{s7_level_actor_class(entity.archetype, *document.level_config)};
        if (!IsValid(type)) {
            return std::unexpected{
                FString::Printf(TEXT("No editor class for entity '%s'."), *name)};
        }
        FTransform const transform{
            FRotator{entity.rotation.x(), entity.rotation.y(), entity.rotation.z()},
            FVector{entity.position.x(), entity.position.y(), entity.position.z()}};
        if (!IsValid(actor) || actor->GetClass() != type) {
            if (IsValid(actor)) {
                actor->Destroy();
            }
            actor = GEditor->AddActor(&level, type, transform, true, RF_Transactional, false);
        }
        if (!IsValid(actor)) {
            return std::unexpected{FString::Printf(TEXT("Could not materialize '%s'."), *name)};
        }
        actor->Modify();
        actor->bIsEditorOnlyActor = true;
        configure_s7_level_actor(*actor,
                                 entity.archetype,
                                 to_unreal(entity.team),
                                 *document.level_config,
                                 transform,
                                 FName{name});
        bindings.Add({.id = name,
                      .actor = actor,
                      .team = to_unreal(entity.team),
                      .spawn_time_seconds = entity.spawn_at.get(),
                      .export_to_script = export_entity,
                      .health = optional(entity.health),
                      .laser_damage = optional(entity.laser_damage),
                      .initial_spawn_delay = optional(entity.initial_spawn_delay),
                      .spawn_cooldown = optional(entity.spawn_cooldown),
                      .initial_fire_point = optional(entity.initial_fire_point)});
        existing.Remove(name);
    }
    for (auto const& entry : existing) {
        if (IsValid(entry.Value.actor)) {
            entry.Value.actor->Destroy();
        }
    }
    document.Modify();
    document.entities = MoveTemp(bindings);
    document.use_observer_camera = baseline->camera.has_value();
    document.camera.targets.Reset();
    if (baseline->camera) {
        auto const& camera{*baseline->camera};
        document.camera.distance = camera.distance;
        document.camera.offset_direction = FVector{
            camera.offset_direction.x(), camera.offset_direction.y(), camera.offset_direction.z()};
        for (auto const& target : camera.target_entity_ids) {
            auto const* binding{document.entities.FindByPredicate(
                [&](auto const& entry) { return entry.id == to_fstring(target.value); })};
            if (!binding) {
                return std::unexpected{
                    FString::Printf(TEXT("Generated camera target '%s' has no editor binding."),
                                    *to_fstring(target.value))};
            }
            document.camera.targets.Add(binding->actor);
        }
    }
    return {};
}
}
