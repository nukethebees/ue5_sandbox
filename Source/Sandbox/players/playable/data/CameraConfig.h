#pragma once

#include <utility>

#include "CoreMinimal.h"

#include "Sandbox/players/playable/enums/CharacterCameraMode.h"

struct FCameraConfig {
    constexpr FCameraConfig(ECharacterCameraMode camera_mode,
                            char const* component_name,
                            bool needs_spring_arm,
                            bool use_pawn_control_rotation)
        : camera_mode(camera_mode)
        , camera_index(std::to_underlying(camera_mode))
        , component_name(component_name)
        , needs_spring_arm(needs_spring_arm)
        , use_pawn_control_rotation(use_pawn_control_rotation) {}

    ECharacterCameraMode camera_mode;
    std::underlying_type_t<ECharacterCameraMode> camera_index;
    char const* component_name;
    bool needs_spring_arm;
    bool use_pawn_control_rotation;
};
