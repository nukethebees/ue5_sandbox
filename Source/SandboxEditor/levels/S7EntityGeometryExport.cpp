#include "SandboxEditor/levels/S7EntityGeometryExport.h"

#include <ioj/settings/entity_geometry_writer.h>
#include <ioj/sim/levels/level_simulation_builder.h>
#include <ioj/sim/levels/simulation_settings.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/LevelCollisionHost.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>

#include <SandboxCoreEngine/strings.h>
#include <SandboxCoreEngine/TransformConversion.h>

#include <Components/StaticMeshComponent.h>
#include <Engine/StaticMesh.h>
#include <Engine/StaticMeshSocket.h>
#include <HAL/FileManager.h>
#include <HAL/IConsoleManager.h>
#include <Misc/FileHelper.h>
#include <Misc/Paths.h>

namespace ioj::editor {
auto entity_geometry_path() -> FString {
    return FPaths::Combine(FPaths::ProjectDir(), TEXT("LevelScripts/Settings/entity-geometry.scm"));
}

auto export_entity_geometry(FString const& path) -> std::expected<void, FString> {
    auto const* config{s7_level_config::load_canonical()};
    if (!IsValid(config) || !config->classes.player_ship_class) {
        return std::unexpected{TEXT("Canonical level resources or player class are unavailable.")};
    }
    auto const* player{config->classes.player_ship_class.GetDefaultObject()};
    auto const mesh{player->get_presentation_resources().mesh};
    using enum sim::EntityType;
    FLevelCollisionHost::EntityMeshes meshes{};
    meshes[PlayerShip] = player->get_collision_mesh();
    meshes[CapitalShip] = config->capital_ships.mesh;
    meshes[Fighter] = config->fighters.mesh;
    meshes[Turret] = config->turrets.mesh;
    meshes[TubeSpinner] = config->tube_spinners.mesh;
    for (auto const type : EnumTraits<sim::EntityType>::values) {
        if (!IsValid(meshes[type])) {
            return std::unexpected{FString::Printf(TEXT("Missing collision mesh for %s."),
                                                   *to_fstring(to_serialized_string(type)))};
        }
    }
    auto const bounds{FLevelCollisionHost::extract_entity_bounds(meshes)};
    if (!bounds) {
        return std::unexpected{bounds.error().format()};
    }
    auto box = [&](sim::EntityType type) -> settings::MeshBounds {
        return {bounds->centre(type), bounds->half_extents(type)};
    };
    auto attachment = [](FTransform const& input) -> settings::AttachmentTransform {
        auto const value{to_native(input)};
        return {value.location, value.rotation.components, value.scale};
    };
    settings::Settings settings;
    auto& geometry{settings.entity_geometry};
    geometry.player_ship = box(PlayerShip);
    geometry.capital_ship = box(CapitalShip);
    geometry.fighter = box(Fighter);
    geometry.turret = box(Turret);
    geometry.tube_spinner = box(TubeSpinner);

    for (auto const& slot : config->capital_ships.fighter_spawn_slots_relative_transforms) {
        geometry.fighter_launch_slots.push_back(attachment(slot));
    }
    auto const* gun{config->fighters.mesh->FindSocket(TEXT("Gun"))};
    if (!IsValid(gun)) {
        return std::unexpected{TEXT("The fighter mesh is missing its Gun socket.")};
    }
    geometry.fighter_fire_point_distance = static_cast<float>(gun->RelativeLocation.Size());
    geometry.turret_fire_point = Vector3f{config->turrets.fire_point_offset.GetLocation()};
    for (auto const& point : config->tube_spinners.fire_point_offsets) {
        geometry.spinner_fire_points.push_back(attachment(point));
    }

    using Sockets = ATestSpaceShip::Sockets;
    for (auto const socket : {Sockets::left, Sockets::middle, Sockets::right}) {
        if (!mesh->DoesSocketExist(socket)) {
            return std::unexpected{
                FString::Printf(TEXT("Missing player socket %s."), *socket.ToString())};
        }
    }
    geometry.player_body = attachment(mesh->GetRelativeTransform());
    geometry.player_left_socket =
        attachment(mesh->GetSocketTransform(Sockets::left, RTS_Component));
    geometry.player_middle_socket =
        attachment(mesh->GetSocketTransform(Sockets::middle, RTS_Component));
    geometry.player_right_socket =
        attachment(mesh->GetSocketTransform(Sockets::right, RTS_Component));

    sim::LevelSimInitData preview;
    preview.player.emplace();
    sim::apply_settings(settings, preview);
    if (auto valid{sim::validate_level_simulation_init_data(preview)}; !valid) {
        return std::unexpected{to_fstring(valid.error().front())};
    }

    auto const source{to_fstring("(export entity-geometry)\n(define entity-geometry " +
                                 s7::serialize(geometry) + ")\n")};
    FString existing;
    if (FFileHelper::LoadFileToString(existing, *path) && source == existing) {
        return {};
    }
    auto const temp{FPaths::CreateTempFilename(*FPaths::GetPath(path))};
    if (!FFileHelper::SaveStringToFile(
            source, *temp, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM) ||
        !IFileManager::Get().Move(*path, *temp)) {
        IFileManager::Get().Delete(*temp);
        return std::unexpected{FString::Printf(TEXT("Cannot write geometry to %s."), *path)};
    }
    return {};
}

static FAutoConsoleCommand export_geometry_command{
    TEXT("Sandbox.ExportEntityGeometry"),
    TEXT("Export the shared ship geometry for native simulation."),
    FConsoleCommandDelegate::CreateLambda([] {
        auto const path{entity_geometry_path()};
        if (auto result{export_entity_geometry(path)}; !result) {
            UE_LOG(LogTemp, Error, TEXT("%s"), *result.error());
        } else {
            UE_LOG(LogTemp, Display, TEXT("Exported entity geometry to %s"), *path);
        }
    })};
}
