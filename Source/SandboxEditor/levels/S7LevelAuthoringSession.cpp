#include "SandboxEditor/levels/S7LevelAuthoringSession.h"

#include "SandboxEditor/levels/S7LevelAuthoringActors.h"
#include "SandboxEditor/levels/S7LevelAuthoringDocument.h"
#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/levels/authoring/identifiers.h>
#include <ioj/levels/authoring/level_definition_writer.h>
#include <SpaceGame/s7/GameSettingsReader.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>

#include <SandboxCoreEngine/strings.h>

#include <Editor.h>
#include <Engine/Level.h>
#include <Misc/PackageName.h>
#include <Misc/Paths.h>
#include <ScopedTransaction.h>

namespace ioj::editor {
namespace s7_level_authoring_session_detail {
using namespace levels;
using namespace levels::authoring;

auto text(FString const& value) -> std::string {
    auto const utf8{FTCHARToUTF8{*value, value.Len()}};
    return {utf8.Get(), static_cast<std::size_t>(utf8.Length())};
}

auto library_root(AS7LevelAuthoringDocument const& document) -> std::filesystem::path {
    auto const directory{document.source_path.IsEmpty()
                             ? FPaths::Combine(FPaths::ProjectDir(), TEXT("LevelScripts"))
                             : FPaths::GetPath(document.source_path)};
    return std::filesystem::path{*directory} / "Libraries";
}
auto entity_id(FName const value) -> EntityId {
    return EntityId{value.IsNone() ? std::string{} : text(value.ToString().ToLower())};
}
auto adopt(ULevel& level, TArray<FS7LevelEntityBinding>& bindings) -> int32 {
    TSet<AActor const*> bound;
    std::unordered_set<EntityId> ids;
    for (auto const& binding : bindings) {
        bound.Add(binding.actor);
        ids.insert(entity_id(binding.id));
    }
    int32 count{};
    for (auto const actor_ptr : level.Actors) {
        auto* const actor{actor_ptr.Get()};
        auto const resolved{IsValid(actor) ? resolve_s7_level_actor(*actor) : NullOpt};
        if (!resolved.IsSet() || bound.Contains(actor)) {
            continue;
        }
        auto const id{allocate_entity_id(
            text(actor->GetActorLabel()), to_serialized_string(resolved->archetype), ids)};
        bindings.Add({.id = FName{to_fstring(id.value)}, .actor = actor});
        bound.Add(actor);
        ++count;
    }
    return count;
}
}

auto s7_settings_path(ULevel const& level) -> FString {
    for (auto const actor : level.Actors) {
        if (auto const* const orchestrator{Cast<ATestBatchOrchestrator>(actor.Get())}) {
            return orchestrator->get_settings_file();
        }
    }
    return TEXT("LevelScripts/Settings/base.scm");
}

auto read_s7_document_settings(AS7LevelAuthoringDocument const& document)
    -> std::expected<settings::Settings, FString> {
    auto const root{FPaths::Combine(FPaths::ProjectDir(), TEXT("LevelScripts"))};
    levels::authoring::DefinitionReader const reader{std::filesystem::path{*root}};
    settings::Settings initial;
    if (IsValid(document.level_config)) {
        initial.presentation = document.level_config->presentation;
    }
    auto base{document.evaluated_base_settings_source.IsEmpty()
                  ? std::expected<settings::Settings, levels::Diagnostics>{std::move(initial)}
                  : reader.read_settings_source(s7_level_authoring_session_detail::text(
                                                    document.evaluated_base_settings_source),
                                                std::move(initial))};
    if (!base) {
        return std::unexpected{to_fstring(levels::format_diagnostics(base.error()))};
    }
    if (document.evaluated_level_source.IsEmpty()) {
        return std::move(*base);
    }
    levels::authoring::DefinitionReader const level_reader{
        s7_level_authoring_session_detail::library_root(document)};
    auto definition{level_reader.read_level_source(
        s7_level_authoring_session_detail::text(document.evaluated_level_source),
        std::move(*base))};
    if (!definition) {
        return std::unexpected{to_fstring(levels::format_diagnostics(definition.error()))};
    }
    return definition->settings();
}

auto emit_s7_scene_source(levels::LevelDefinition const& definition,
                          AS7LevelAuthoringDocument const& document)
    -> std::expected<FString, FString> {
    auto source{levels::authoring::emit_editor_level_source(
        definition,
        s7_level_authoring_session_detail::text(document.evaluated_level_source),
        s7_level_authoring_session_detail::library_root(document))};
    if (!source) {
        return std::unexpected{to_fstring(levels::format_diagnostics(source.error()))};
    }
    return to_fstring(*source);
}

auto collect_s7_scene_source(ULevel const& level, AS7LevelAuthoringDocument const& document)
    -> std::expected<FString, FString> {
    auto const definition{collect_s7_editor_level(level, document)};
    if (!definition) {
        return std::unexpected{definition.error()};
    }
    return emit_s7_scene_source(*definition, document);
}

auto find_level_authoring_document(ULevel const& level)
    -> std::expected<AS7LevelAuthoringDocument*, FString> {
    AS7LevelAuthoringDocument* result{};
    for (auto const actor_ptr : level.Actors) {
        auto* const document{Cast<AS7LevelAuthoringDocument>(actor_ptr.Get())};
        if (!IsValid(document)) {
            continue;
        }
        if (result) {
            return std::unexpected{
                TEXT("The current level contains multiple S7 authoring documents.")};
        }
        result = document;
    }
    return result;
}

auto create_level_authoring_document(ULevel& level)
    -> std::expected<AS7LevelAuthoringDocument*, FString> {
    auto const existing{find_level_authoring_document(level)};
    if (!existing) {
        return std::unexpected{existing.error()};
    }
    if (*existing) {
        return *existing;
    }
    if (!GEditor) {
        return std::unexpected{TEXT("GEditor is unavailable.")};
    }

    FScopedTransaction transaction{
        NSLOCTEXT("S7LevelAuthoring", "CreateDocument", "Create S7 Level Document")};
    auto* const document{
        Cast<AS7LevelAuthoringDocument>(GEditor->AddActor(&level,
                                                          AS7LevelAuthoringDocument::StaticClass(),
                                                          FTransform::Identity,
                                                          true,
                                                          RF_Transactional,
                                                          false))};
    if (!IsValid(document)) {
        transaction.Cancel();
        return std::unexpected{TEXT("Could not create the S7 authoring document.")};
    }
    auto const map_name{FPackageName::GetShortName(level.GetOutermost()->GetName())};
    document->level_id = canonical_s7_level_entity_id(map_name, TEXTVIEW("authored-level"));
    document->title = map_name;
    document->level_config = s7_level_config::load_canonical();
    if (!IsValid(document->level_config)) {
        document->Destroy();
        transaction.Cancel();
        return std::unexpected{
            TEXT("The canonical S7 runtime level configuration is unavailable.")};
    }
    auto const settings{
        read_game_settings_source(s7_settings_path(level), *document->level_config)};
    if (!settings) {
        document->Destroy();
        transaction.Cancel();
        return std::unexpected{settings.error()};
    }
    document->synchronized_base_settings_source = settings->source;
    document->evaluated_base_settings_source = settings->evaluated_source;
    return document;
}

auto adopt_unbound_level_entities(ULevel& level, AS7LevelAuthoringDocument& document)
    -> std::expected<int32, FString> {
    auto bindings{document.entities};
    auto const adopted{s7_level_authoring_session_detail::adopt(level, bindings)};
    if (adopted != 0) {
        FScopedTransaction transaction{
            NSLOCTEXT("S7LevelAuthoring", "Adopt", "Adopt Level Entities")};
        document.Modify();
        document.entities = MoveTemp(bindings);
    }
    return adopted;
}

auto repair_s7_level_bindings(ULevel& level, AS7LevelAuthoringDocument& document)
    -> std::expected<FS7LevelBindingRepairResult, FString> {
    FS7LevelBindingRepairResult result;
    TArray<FS7LevelEntityBinding> bindings;
    TSet<AActor const*> bound;
    TSet<FName> ids;
    TMap<AActor const*, double> spawn_times;
    for (auto const& binding : document.entities) {
        auto* const actor{binding.actor.Get()};
        if (IsValid(actor) && actor->GetLevel() == &level && !spawn_times.Contains(actor)) {
            spawn_times.Add(actor, binding.spawn_time_seconds);
        }
        if (!IsValid(actor) || actor->GetLevel() != &level ||
            !resolve_s7_level_actor(*actor).IsSet() ||
            !is_canonical_s7_level_entity_id(binding.id) || bound.Contains(actor) ||
            ids.Contains(binding.id)) {
            ++result.removed_bindings;
            continue;
        }
        bindings.Add(binding);
        bound.Add(actor);
        ids.Add(binding.id);
    }

    // Adopt before pruning so references to successfully recovered actors survive.
    result.adopted_entities = s7_level_authoring_session_detail::adopt(level, bindings);
    for (auto& binding : bindings) {
        bound.Add(binding.actor);
        if (auto const* const previous{spawn_times.Find(binding.actor)}; previous) {
            binding.spawn_time_seconds = *previous;
        }
    }
    auto camera{document.camera.targets};
    auto prune{[&](auto& references) {
        result.removed_references += references.RemoveAll(
            [&](auto const actor) { return !IsValid(actor) || !bound.Contains(actor); });
    }};
    prune(camera);
    if (result.has_changes()) {
        FScopedTransaction transaction{
            NSLOCTEXT("S7LevelAuthoring", "RepairBindings", "Repair S7 Level Bindings")};
        document.Modify();
        document.entities = MoveTemp(bindings);
        document.camera.targets = MoveTemp(camera);
    }
    return result;
}

auto collect_s7_scene(ULevel const& level, AS7LevelAuthoringDocument const& document)
    -> std::expected<levels::authoring::SceneSnapshot, FString> {
    using namespace s7_level_authoring_session_detail;
    SceneSnapshot snapshot;
    auto& definition{snapshot.definition};
    definition.metadata = {.id = LevelId{document.level_id.IsNone()
                                             ? std::string{}
                                             : text(document.level_id.ToString().ToLower())},
                           .title = text(document.title),
                           .description = text(document.description)};
    auto settings{read_s7_document_settings(document)};
    if (!settings) {
        return std::unexpected{settings.error()};
    }
    definition.settings = std::move(*settings);

    TMap<AActor const*, EntityId> ids_by_actor;
    TSet<FName> ids;
    int32 player_count{};
    for (auto const& binding : document.entities) {
        auto* const actor{binding.actor.Get()};
        if (!IsValid(actor) || actor->GetLevel() != &level || binding.id.IsNone()) {
            return std::unexpected{
                TEXT("Every entity binding must have a valid actor in this level and an id.")};
        }
        if (ids.Contains(binding.id) || ids_by_actor.Contains(actor)) {
            return std::unexpected{TEXT("Entity bindings contain a duplicate id or actor.")};
        }
        auto const id{entity_id(binding.id)};
        auto const resolved{resolve_s7_level_actor(*actor)};
        auto const rotation{actor->GetActorRotation()};
        definition.entities.push_back(
            {.id = id,
             .archetype = resolved.IsSet() ? resolved->archetype : EntityArchetype::PlayerFighter,
             .team = resolved.IsSet() ? resolved->team : static_cast<Team>(255),
             .position = Vector3d{actor->GetActorLocation()},
             .rotation = Rotator3d{rotation},
             .spawn_time_seconds = binding.spawn_time_seconds});
        snapshot.properties.push_back(
            {.label = text(actor->GetActorLabel()),
             .scale = Vector3d{actor->GetActorScale3D()},
             .class_matches = resolved.IsSet() && IsValid(document.level_config) &&
                              actor->GetClass() == s7_level_actor_class(resolved->archetype,
                                                                        *document.level_config)});
        ids.Add(binding.id);
        ids_by_actor.Add(actor, id);
        if (IsValid(Cast<ATestSpaceShip>(actor))) {
            definition.player_entity_id = id;
            ++player_count;
        }
    }

    auto references{[&](auto const& actors, bool& valid) {
        std::vector<EntityId> result;
        for (auto const actor : actors) {
            auto const* const id{ids_by_actor.Find(actor)};
            valid = valid && id != nullptr;
            result.push_back(id ? *id : EntityId{});
        }
        return result;
    }};
    if (document.use_observer_camera) {
        definition.player_entity_id = {};
        definition.camera = {.target_entity_ids =
                                 references(document.camera.targets, snapshot.viewpoint_valid),
                             .offset_direction = Vector3d{document.camera.offset_direction},
                             .distance = document.camera.distance};
    } else {
        snapshot.viewpoint_valid = player_count == 1;
    }

    return snapshot;
}

auto collect_s7_editor_level(ULevel const& level, AS7LevelAuthoringDocument const& document)
    -> std::expected<levels::LevelDefinition, FString> {
    using namespace s7_level_authoring_session_detail;
    auto const* const config{document.level_config.Get()};
    auto const* const canonical{s7_level_config::load_canonical()};
    if (!IsValid(config) || !IsValid(canonical) ||
        (config != canonical && !level.GetOutermost()->GetName().StartsWith(TEXT("/Temp/")))) {
        return std::unexpected{
            TEXT("S7 authoring requires the canonical GameRuntime level configuration asset.")};
    }
    auto scene{collect_s7_scene(level, document)};
    if (!scene) {
        return std::unexpected{scene.error()};
    }
    TSet<AActor const*> bound;
    for (auto const& binding : document.entities) {
        bound.Add(binding.actor);
    }
    for (auto const actor : level.Actors) {
        if (IsValid(actor) && resolve_s7_level_actor(*actor).IsSet() && !bound.Contains(actor)) {
            return std::unexpected{FString::Printf(
                TEXT("Supported actor '%s' is not adopted by the authoring document."),
                *actor->GetActorLabel())};
        }
    }
    if (!scene->viewpoint_valid || !scene->mission_references_valid) {
        return std::unexpected{
            TEXT("Every viewpoint and mission reference must identify an adopted entity.")};
    }
    auto checked{LevelDefinition::make(MoveTemp(scene->definition))};
    if (!checked) {
        return std::unexpected{to_fstring(format_diagnostics(checked.error()))};
    }
    return MoveTemp(*checked);
}
}
