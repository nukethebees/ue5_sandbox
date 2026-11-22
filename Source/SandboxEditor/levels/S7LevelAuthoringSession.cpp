#include "SandboxEditor/levels/S7LevelAuthoringSession.h"

#include "SandboxEditor/levels/S7LevelAuthoringActors.h"
#include "SandboxEditor/levels/S7LevelAuthoringDocument.h"
#include "SandboxEditor/levels/S7LevelPlayableSetup.h"
#include "SandboxEditor/levels/S7SceneActors.h"
#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/levels/authoring/identifiers.h>
#include <ioj/levels/authoring/level_definition_writer.h>
#include <ioj/levels/authoring/level_inputs_reader.h>
#include <ioj/levels/authoring/level_inputs_writer.h>
#include <ioj/s7/interpreter_options.h>
#include <SpaceGame/defences/spinners/TestTubeSpinnerProxy.h>
#include <SpaceGame/defences/turrets/TestStaticTurretsProxy.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/ships/fighter/TestFighterProxy.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>

#include <SandboxCoreEngine/strings.h>

#include <Editor.h>
#include <Engine/Level.h>
#include <Engine/World.h>
#include <HAL/FileManager.h>
#include <Misc/FileHelper.h>
#include <Misc/Guid.h>
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
auto save_generated(FString const& path, FString const& source) -> S7AuthoringResult<void> {
    FString existing;
    if (FFileHelper::LoadFileToString(existing, *path) && existing == source) {
        return {};
    }
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(path), true);
    auto const temporary{path + TEXT(".") + FGuid::NewGuid().ToString()};
    if (!FFileHelper::SaveStringToFile(
            source, *temporary, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) {
        IFileManager::Get().Delete(*temporary);
        return S7AuthoringResult<void>{
            std::unexpect, FString::Printf(TEXT("Cannot write generated level file '%s'."), *path)};
    }
    std::error_code error;
    std::filesystem::rename(*temporary, *path, error);
    if (error) {
        IFileManager::Get().Delete(*temporary);
        return S7AuthoringResult<void>{std::unexpect,
                                       FString::Printf(TEXT("Cannot replace '%s'."), *path)};
    }
    return {};
}
}

auto s7_mission_path(AS7LevelAuthoringDocument const& document) -> FString {
    return FPaths::ConvertRelativePathToFull(
        FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()), document.source_path);
}

auto s7_editor_config_path(AS7LevelAuthoringDocument const& document) -> FString {
    return FPaths::Combine(FPaths::GetPath(s7_mission_path(document)), TEXT("editor-config.scm"));
}

namespace {
auto read_editor_ast(AS7LevelAuthoringDocument const& document) -> S7AuthoringResult<s7::Ast> {
    std::filesystem::path const path{*s7_editor_config_path(document)};
    return levels::authoring::DefinitionReader::for_level(path)
        .read_script_file(path, "scene")
        .transform_error(
            [](auto const& errors) { return to_fstring(levels::format_diagnostics(errors)); });
}

auto decode_editor_input(s7::Ast const& ast) -> S7AuthoringResult<levels::authoring::LevelInput> {
    auto input{s7::read<levels::authoring::LevelInput>(s7::Reader{ast}, ast.root, "level")};
    if (!input) {
        FString errors;
        for (auto const& error : input.error()) {
            errors += to_fstring(error.node_path + ": " + error.message + "\n");
        }
        return S7AuthoringResult<levels::authoring::LevelInput>{std::unexpect, MoveTemp(errors)};
    }
    return std::move(*input);
}
}

auto read_s7_editor_input(AS7LevelAuthoringDocument const& document)
    -> S7AuthoringResult<levels::authoring::LevelInput> {
    return read_editor_ast(document).and_then(decode_editor_input);
}

auto find_level_authoring_document(ULevel const& level)
    -> S7AuthoringResult<AS7LevelAuthoringDocument*> {
    AS7LevelAuthoringDocument* result{};
    for (auto const actor : level.Actors) {
        if (auto* document{Cast<AS7LevelAuthoringDocument>(actor.Get())}; IsValid(document)) {
            if (result) {
                return S7AuthoringResult<AS7LevelAuthoringDocument*>{
                    std::unexpect, TEXT("The level contains multiple S7 authoring documents.")};
            }
            result = document;
        }
    }
    return result;
}

namespace {
auto collect_editor_scene(ULevel const& level,
                          AS7LevelAuthoringDocument const& document,
                          levels::authoring::LevelInput& input,
                          TArray<FS7LevelEntityBinding>& new_bindings) -> S7AuthoringResult<void> {
    using Result = S7AuthoringResult<void>;

    using namespace s7_level_authoring_session_detail;
    input.entities.clear();
    input.player = {};
    input.camera.reset();
    TMap<AActor const*, FString> bound;
    for (auto const& binding : document.entities) {
        if (IsValid(binding.actor)) {
            bound.Add(binding.actor, binding.id);
        }
    }
    TMap<AActor const*, levels::EntityId> ids;
    TSet<FString> names;
    auto const collected{SceneActors{level}.for_each_scene_actor(
        [&](auto& actor, levels::EntityArchetype archetype) -> Result {
            using Actor = std::remove_reference_t<decltype(actor)>;
            auto const* known{bound.Find(&actor)};
            if (!actor.export_to_script && !known) {
                return {};
            }
            auto const name{
                known ? *known
                      : canonical_s7_level_entity_id(actor.GetActorLabel(), TEXTVIEW("entity"))
                            .ToString()};
            if (!is_canonical_s7_level_entity_id(name) || names.Contains(name)) {
                return Result{
                    std::unexpect,
                    FString::Printf(TEXT("Invalid or duplicate entity ID '%s'. Rename the entity "
                                         "or its unbound actor label."),
                                    *name)};
            }
            names.Add(name);
            auto const entity_id{id(name)};
            ids.Add(&actor, entity_id);
            if (!known) {
                new_bindings.Add({.id = name, .actor = &actor});
            }
            if (!actor.export_to_script) {
                return {};
            }
            auto rotation{actor.GetActorRotation()};
            if constexpr (std::is_same_v<Actor, ATestFighterProxy>) {
                rotation.Roll = 0.;
            }
            double time{};
            if constexpr (!std::is_same_v<Actor, ATestSpaceShip>) {
                time = actor.spawn_time_seconds;
            }
            auto const spawn_at{NonNegativeFinite<double>::try_make(time)};
            if (!spawn_at) {
                return Result{
                    std::unexpect,
                    FString::Printf(TEXT("Entity '%s' requires a finite nonnegative spawn time."),
                                    *name)};
            }
            Team team{Team::White};
            if constexpr (!std::is_same_v<Actor, ATestTubeSpinnerProxy>) {
                team = to_native(actor.get_team());
            }
            levels::authoring::EntityInput entity{
                .id = entity_id,
                .archetype = archetype,
                .team = team,
                .position = Vector3d{actor.GetActorLocation()},
                .rotation = {rotation.Pitch, rotation.Yaw, rotation.Roll},
                .spawn_at = *spawn_at};
            TCHAR const* invalid_field{};
            auto const convert{[&]<typename T, typename U>(std::optional<T>& output,
                                                           TOptional<U> const& value,
                                                           TCHAR const* field) {
                if (!value.IsSet()) {
                    return true;
                }
                output = T::try_make(typename T::value_type{*value});
                if (!output) {
                    invalid_field = field;
                }
                return output.has_value();
            }};
            bool valid{true};
            if constexpr (std::is_same_v<Actor, ATestSpaceShip>) {
                if (!document.use_observer_camera && !input.player.value.empty()) {
                    return Result{std::unexpect,
                                  TEXT("Multiple exported players. Export one player or "
                                       "select an observer camera.")};
                }
                input.player = entity_id;
                valid = convert(entity.health, actor.health_override, TEXT("health"));
            } else if constexpr (std::is_same_v<Actor, ATestCapitalShipProxy>) {
                valid =
                    convert(entity.health, actor.health, TEXT("health")) &&
                    convert(entity.initial_spawn_delay,
                            actor.initial_spawn_delay,
                            TEXT("initial spawn delay")) &&
                    convert(entity.spawn_cooldown, actor.spawn_cooldown, TEXT("spawn cooldown"));
            } else if constexpr (std::is_same_v<Actor, ATestStaticTurretsProxy>) {
                valid = convert(entity.health, actor.health, TEXT("health")) &&
                        convert(entity.laser_damage, actor.laser_damage, TEXT("laser damage"));
            } else if constexpr (std::is_same_v<Actor, ATestTubeSpinnerProxy>) {
                valid = convert(entity.initial_fire_point,
                                actor.initial_fire_point,
                                TEXT("initial fire point"));
            } else {
                static_assert(std::is_same_v<Actor, ATestFighterProxy>,
                              "Handle this actor's export overrides.");
            }
            if (!valid) {
                return Result{std::unexpect,
                              FString::Printf(TEXT("Entity '%s' has an invalid %s override."),
                                              *name,
                                              invalid_field)};
            }
            input.entities.push_back(std::move(entity));
            return {};
        })};
    if (!collected) {
        return collected;
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
                return Result{std::unexpect,
                              TEXT("Export the camera target or select a bound level actor.")};
            }
            input.camera->target_entity_ids.push_back(*entity);
        }
    }
    return {};
}
}

auto s7_scene_needs_export(AS7LevelAuthoringDocument const& document) -> S7AuthoringResult<bool> {
    using Result = S7AuthoringResult<bool>;
    if (!document.needs_export) {
        return false;
    }

    s7::ModuleInput baseline{*s7_editor_config_path(document)};
    std::filesystem::path const path{*s7_mission_path(document)};
    auto mission{levels::authoring::DefinitionReader::for_level(path).read_script_file(
        path, "mission", &baseline)};
    if (!mission) {
        return Result{std::unexpect, to_fstring(levels::format_diagnostics(mission.error()))};
    }
    return baseline.imported;
}

namespace {
struct PreparedExport {
    FString source;
    levels::LevelDefinition definition;
    TArray<FS7LevelEntityBinding> bindings;
};

auto prepare_export(ULevel const& level, AS7LevelAuthoringDocument const& document)
    -> S7AuthoringResult<PreparedExport> {
    using Result = S7AuthoringResult<PreparedExport>;
    if (!IsValid(document.level_config)) {
        return Result{std::unexpect,
                      TEXT("Shared level asset unavailable. Link the level script to restore its "
                           "asset reference.")};
    }

    auto ast{read_editor_ast(document)};
    if (!ast) {
        return Result{std::unexpect,
                      TEXT("Cannot read the editor baseline. Check the linked level script and "
                           "editor-config.scm.\n") +
                          ast.error()};
    }
    auto input{decode_editor_input(*ast)};
    if (!input) {
        return Result{std::unexpect, std::move(input.error())};
    }
    TArray<FS7LevelEntityBinding> bindings;
    auto collected{collect_editor_scene(level, document, *input, bindings)};
    if (!collected) {
        return Result{std::unexpect, std::move(collected.error())};
    }
    auto const source{levels::authoring::emit_editor_level_source(*input, &*ast)};
    s7::ModuleInput baseline{*s7_editor_config_path(document), source};
    std::filesystem::path const path{*s7_mission_path(document)};
    auto definition{
        levels::authoring::DefinitionReader::for_level(path).read_level_file(path, &baseline)};
    if (!definition) {
        auto message{to_fstring(levels::format_diagnostics(definition.error()))};
        if (std::ranges::any_of(definition.error(), [](auto const& diagnostic) {
                return diagnostic.code == levels::DiagnosticCode::DuplicateEntityId;
            })) {
            message += TEXT(
                "\nRename the duplicate entity ID or disable Export on the script-owned actor.");
        }
        return Result{std::unexpect, MoveTemp(message)};
    }
    if (!baseline.imported) {
        return Result{
            std::unexpect,
            TEXT(
                "Import the editor-config.scm scene in the linked level script before exporting.")};
    }
    auto compiled{compile_playable_s7_definition(*definition, *document.level_config)};
    if (!compiled) {
        return Result{std::unexpect, std::move(compiled.error())};
    }
    return Result{std::in_place, to_fstring(source), std::move(*definition), MoveTemp(bindings)};
}
}
auto check_s7_scene(ULevel const& level, AS7LevelAuthoringDocument const& document)
    -> S7AuthoringResult<FS7SceneCheck> {
    auto prepared{prepare_export(level, document)};
    if (!prepared) {
        return S7AuthoringResult<FS7SceneCheck>{std::unexpect, std::move(prepared.error())};
    }
    TArray<FString> warnings;
    document.level_config->get_validation_warnings(warnings,
                                                   prepared->definition.settings().presentation);
    return S7AuthoringResult<FS7SceneCheck>{
        std::in_place, std::move(prepared->definition), MoveTemp(warnings)};
}
auto export_s7_editor_config(ULevel& level, AS7LevelAuthoringDocument& document)
    -> S7AuthoringResult<void> {
    auto prepared{prepare_export(level, document)};
    if (!prepared) {
        document.set_needs_export(true);
        return S7AuthoringResult<void>{std::unexpect, std::move(prepared.error())};
    }
    auto saved{s7_level_authoring_session_detail::save_generated(s7_editor_config_path(document),
                                                                 prepared->source)};
    auto const& bindings{prepared->bindings};
    if (saved &&
        (!bindings.IsEmpty() || document.entities.ContainsByPredicate(
                                    [](auto const& value) { return !IsValid(value.actor); }))) {
        FScopedTransaction transaction{
            NSLOCTEXT("S7LevelAuthoring", "BindExport", "Bind exported actors")};
        document.Modify();
        document.entities.RemoveAll([](auto const& value) { return !IsValid(value.actor); });
        for (auto const& binding : bindings) {
            binding.actor->Modify();
            binding.actor->bIsEditorOnlyActor = true;
        }
        document.entities.Append(bindings);
    }
    document.set_needs_export(!saved);
    return saved;
}

auto read_s7_authored_level(AS7LevelAuthoringDocument const& document)
    -> S7AuthoringResult<levels::LevelDefinition> {
    std::filesystem::path const path{*s7_mission_path(document)};
    return levels::authoring::DefinitionReader::for_level(path)
        .read_level_file(path)
        .transform_error(
            [](auto const& errors) { return to_fstring(levels::format_diagnostics(errors)); });
}

auto set_up_s7_mission(ULevel& level, MissionSetup operation, FString source_path)
    -> S7AuthoringResult<AS7LevelAuthoringDocument*> {
    using Result = S7AuthoringResult<AS7LevelAuthoringDocument*>;

    auto existing{find_level_authoring_document(level)};
    if (!existing) {
        return existing;
    }
    if (operation == MissionSetup::Create && *existing) {
        return Result{std::unexpect, TEXT("This map already has a linked level script.")};
    }
    auto const map_name{FPackageName::GetShortName(level.GetOutermost()->GetName())};
    auto const root{FPaths::ConvertRelativePathToFull(
        source_path.IsEmpty()
            ? FPaths::Combine(
                  FPaths::ProjectDir(), TEXT("LevelScripts"), map_name, map_name + TEXT(".scm"))
            : source_path)};
    FString relative{root};
    auto const project{FPaths::ConvertRelativePathToFull(FPaths::ProjectDir())};
    if (!FPaths::IsUnderDirectory(root, project) ||
        !FPaths::MakePathRelativeTo(relative, *project)) {
        return Result{std::unexpect, TEXT("Select a level script inside this project.")};
    }
    auto* resources{s7_level_config::load_canonical()};
    if (!IsValid(resources)) {
        return Result{std::unexpect, TEXT("Canonical level resources are unavailable.")};
    }
    auto const human_exists{IFileManager::Get().FileExists(*root)};
    auto const directory{FPaths::GetPath(root)};
    if (FPaths::GetCleanFilename(directory) != FPaths::GetBaseFilename(root)) {
        return Result{std::unexpect, TEXT("Choose a level script stored as <level>/<level>.scm.")};
    }
    auto const baseline_path{FPaths::Combine(directory, TEXT("editor-config.scm"))};
    auto const baseline_existed{IFileManager::Get().FileExists(*baseline_path)};
    if (operation == MissionSetup::Link) {
        auto mission{levels::authoring::DefinitionReader::for_level(*root).read_level_file(*root)};
        if (!mission) {
            return Result{std::unexpect, to_fstring(levels::format_diagnostics(mission.error()))};
        }
        auto valid{compile_playable_s7_definition(*mission, *resources)};
        if (!valid) {
            return Result{std::unexpect, std::move(valid.error())};
        }
    } else {
        if (human_exists || baseline_existed) {
            return Result{std::unexpect,
                          TEXT("Level files already exist. Use Link Existing Script to select "
                               "the level script.")};
        }
        levels::authoring::LevelInput input{
            .id = levels::LevelId{to_string(
                canonical_s7_level_entity_id(map_name, TEXTVIEW("level")).ToString())},
            .title = to_string(map_name)};
        auto saved{s7_level_authoring_session_detail::save_generated(
            baseline_path, to_fstring(levels::authoring::emit_editor_level_source(input)))};
        if (!saved) {
            return Result{std::unexpect, std::move(saved.error())};
        }
        auto const source{FString::Printf(
            TEXT("(export mission)\n"
                 "(define scene (module-ref (import-module \"%s/editor-config.scm\") 'scene))\n"
                 "(define mission\n"
                 "  (copy-with scene :settings (settings-layers\n"
                 "    (module-ref (import-module \"Settings/level-defaults.scm\") 'game-settings)\n"
                 "    (level-ref scene :settings))))\n"),
            *FPaths::GetBaseFilename(root))};
        if (!FFileHelper::SaveStringToFile(source,
                                           *root,
                                           FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
                                           &IFileManager::Get(),
                                           FILEWRITE_NoReplaceExisting)) {
            IFileManager::Get().Delete(*baseline_path);
            return Result{std::unexpect,
                          TEXT("Could not create the level script. Choose an unused name.")};
        }
    }
    FScopedTransaction transaction{NSLOCTEXT("S7LevelAuthoring", "Link", "Link Level Script")};
    auto* document{*existing ? *existing
                             : Cast<AS7LevelAuthoringDocument>(
                                   GEditor->AddActor(&level,
                                                     AS7LevelAuthoringDocument::StaticClass(),
                                                     FTransform::Identity,
                                                     true,
                                                     RF_Transactional,
                                                     false))};
    FString failure{TEXT("Could not create the S7 authoring document.")};
    if (IsValid(document)) {
        auto const old_resources{document->level_config};
        auto const previous{document->source_path};
        document->Modify();
        document->source_path = relative;
        document->level_config = resources;
        auto setup{set_up_playable_s7_level(level, *document)};
        if (setup) {
            if (operation == MissionSetup::Create) {
                document->set_needs_export(level.Actors.ContainsByPredicate([](auto const actor) {
                    if (!IsValid(actor)) {
                        return false;
                    }
                    auto const resolved{resolve_s7_level_actor(*actor)};
                    return resolved && resolved->export_to_script;
                }));
            } else if (previous != relative) {
                document->set_needs_export(false);
            }
            return document;
        }
        failure = setup.error();
        document->source_path = previous;
        document->level_config = old_resources;
        if (!*existing) {
            document->Destroy();
        }
    }
    transaction.Cancel();
    if (operation == MissionSetup::Create) {
        IFileManager::Get().Delete(*root);
        IFileManager::Get().Delete(*baseline_path);
    }
    return Result{std::unexpect, MoveTemp(failure)};
}

auto import_s7_level(ULevel& level, AS7LevelAuthoringDocument& document)
    -> S7AuthoringResult<void> {
    using Result = S7AuthoringResult<void>;

    using namespace s7_level_authoring_session_detail;
    s7::ModuleInput module{*s7_editor_config_path(document)};
    std::filesystem::path const path{*s7_mission_path(document)};
    auto definition{
        levels::authoring::DefinitionReader::for_level(path).read_level_file(path, &module)};
    if (!definition) {
        return Result{std::unexpect, to_fstring(levels::format_diagnostics(definition.error()))};
    }
    auto baseline{module.imported
                      ? read_s7_editor_input(document)
                      : S7AuthoringResult<levels::authoring::LevelInput>{std::in_place}};
    if (!baseline) {
        return Result{std::unexpect, std::move(baseline.error())};
    }
    auto const& viewpoint{module.imported ? baseline->camera : definition->camera()};
    auto entities{baseline->entities};
    std::unordered_set<levels::EntityId> ids;
    for (auto const& entity : entities) {
        if (!ids.insert(entity.id).second) {
            return Result{std::unexpect, TEXT("Duplicate entity ID in the editor baseline.")};
        }
    }
    auto const baseline_count{entities.size()};
    for (auto const& entity : definition->entities()) {
        if (ids.insert(entity.id).second) {
            entities.push_back(levels::authoring::make_entity_input(entity));
        }
    }
    if (viewpoint) {
        for (auto const& target : viewpoint->target_entity_ids) {
            if (!ids.contains(target)) {
                return Result{std::unexpect,
                              FString::Printf(TEXT("Unknown camera target '%s'."),
                                              *to_fstring(target.value))};
            }
        }
    }
    for (auto const& entity : entities) {
        if (!IsValid(s7_level_actor_class(entity.archetype, *document.level_config))) {
            return Result{
                std::unexpect,
                FString::Printf(TEXT("No editor class for '%s'."), *to_fstring(entity.id.value))};
        }
    }
    TMap<FString, AActor*> previous_actors;
    for (auto const& binding : document.entities) {
        if (IsValid(binding.actor) && binding.actor->GetLevel() == &level) {
            previous_actors.Add(binding.id, binding.actor.Get());
        }
    }

    FScopedTransaction transaction{NSLOCTEXT("S7LevelAuthoring", "Rebuild", "Rebuild S7 Scene")};
    level.Modify();
    TArray<FS7LevelEntityBinding> bindings;
    for (auto const& entity : entities) {
        auto const name{to_fstring(entity.id.value)};
        auto const export_entity{static_cast<size_t>(bindings.Num()) < baseline_count};
        auto* type{s7_level_actor_class(entity.archetype, *document.level_config)};
        FTransform const transform{
            FRotator{entity.rotation.x(), entity.rotation.y(), entity.rotation.z()},
            FVector{entity.position.x(), entity.position.y(), entity.position.z()}};
        auto* actor{GEditor->AddActor(&level, type, transform, true, RF_Transactional, false)};
        if (!IsValid(actor)) {
            for (auto const& spawned : bindings) {
                spawned.actor->Destroy();
            }
            transaction.Cancel();
            return Result{std::unexpect,
                          FString::Printf(TEXT("Could not materialize '%s'."), *name)};
        }
        actor->Modify();
        actor->bIsEditorOnlyActor = true;
        auto const team{to_unreal(entity.team)};
        auto& config{*document.level_config};
        SceneActors::visit_archetype(entity.archetype, [&]<typename Actor>() {
            auto& proxy{*CastChecked<Actor>(actor)};
            proxy.export_to_script = export_entity;
            if constexpr (!std::is_same_v<Actor, ATestTubeSpinnerProxy>) {
                proxy.set_team(team);
            }
            if constexpr (!std::is_same_v<Actor, ATestSpaceShip>) {
                proxy.spawn_time_seconds = entity.spawn_at.get();
            }
            if constexpr (std::is_same_v<Actor, ATestSpaceShip>) {
                proxy.set_actor_config(&config.player_ship);
                proxy.health_override = optional(entity.health);
            } else if constexpr (std::is_same_v<Actor, ATestCapitalShipProxy>) {
                proxy.set_preview_settings(definition->settings());
                proxy.health = optional(entity.health);
                proxy.initial_spawn_delay = optional(entity.initial_spawn_delay);
                proxy.spawn_cooldown = optional(entity.spawn_cooldown);
            } else if constexpr (std::is_same_v<Actor, ATestStaticTurretsProxy>) {
                proxy.set_level_config_asset(&config);
                proxy.set_preview_radius(definition->settings().turrets.detection_radius);
                proxy.health = optional(entity.health);
                proxy.laser_damage = optional(entity.laser_damage);
            } else if constexpr (std::is_same_v<Actor, ATestTubeSpinnerProxy>) {
                proxy.set_level_config_asset(&config);
                proxy.initial_fire_point = optional(entity.initial_fire_point);
            } else {
                static_assert(std::is_same_v<Actor, ATestFighterProxy>,
                              "Handle this actor's import overrides.");
            }
        });
        actor->SetActorTransform(transform);
        actor->RerunConstructionScripts();
        actor->PostEditMove(true);
        auto const* previous{previous_actors.FindRef(name)};
        actor->SetActorLabel(previous && previous->IsA(type) ? previous->GetActorLabel() : name,
                             true);
        actor->SetFolderPath(
            FName{StaticEnum<ETestTeam>()->GetNameStringByValue(std::to_underlying(team))});
        bindings.Add({.id = name, .actor = actor});
    }
    document.Modify();
    for (auto const& entry : document.entities) {
        if (IsValid(entry.actor) && entry.actor->GetLevel() == &level) {
            entry.actor->Modify();
            level.OwningWorld->EditorDestroyActor(entry.actor, true);
        }
    }
    for (auto const& binding : bindings) {
        update_s7_proxy_label(*binding.actor);
    }
    document.entities = MoveTemp(bindings);
    document.use_observer_camera = viewpoint.has_value();
    document.camera.targets.Reset();
    if (viewpoint) {
        auto const& camera{*viewpoint};
        document.camera.distance = camera.distance;
        document.camera.offset_direction = FVector{
            camera.offset_direction.x(), camera.offset_direction.y(), camera.offset_direction.z()};
        for (auto const& target : camera.target_entity_ids) {
            auto const* binding{document.entities.FindByPredicate(
                [&](auto const& entry) { return entry.id == to_fstring(target.value); })};
            check(binding);
            document.camera.targets.Add(binding->actor);
        }
    }
    document.set_needs_export(false);
    return {};
}
}
