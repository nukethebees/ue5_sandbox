#!/usr/bin/env python3
"""Fix broken include paths after directory reorganization."""

import re
from pathlib import Path
from typing import Dict, List, Tuple

# Define the mapping from old paths to new paths
INCLUDE_MAPPINGS: List[Tuple[str, str]] = [
    # Health domain
    (r'#include "Sandbox/actor_components/HealthComponent\.h"',
     r'#include "Sandbox/health/actor_components/HealthComponent.h"'),
    (r'#include "Sandbox/data/health/HealthChange\.h"',
     r'#include "Sandbox/health/data/HealthChange.h"'),
    (r'#include "Sandbox/data/health/HealthStationPayload\.h"',
     r'#include "Sandbox/health/data/HealthStationPayload.h"'),
    (r'#include "Sandbox/interfaces/health/DeathHandler\.h"',
     r'#include "Sandbox/health/interfaces/DeathHandler.h"'),
    (r'#include "Sandbox/actors/HealthStationActor\.h"',
     r'#include "Sandbox/health/actors/HealthStationActor.h"'),
    (r'#include "Sandbox/actors/HealthPackActor\.h"',
     r'#include "Sandbox/health/actors/HealthPackActor.h"'),
    (r'#include "Sandbox/subsystems/world/DamageManagerSubsystem\.h"',
     r'#include "Sandbox/health/subsystems/DamageManagerSubsystem.h"'),

    # Players domain
    (r'#include "Sandbox/characters/MyCharacter\.h"',
     r'#include "Sandbox/players/playable/characters/MyCharacter.h"'),
    (r'#include "Sandbox/characters/TestEnemy\.h"',
     r'#include "Sandbox/players/npcs/characters/TestEnemy.h"'),
    (r'#include "Sandbox/characters/SimpleCharacter\.h"',
     r'#include "Sandbox/players/npcs/characters/SimpleCharacter.h"'),
    (r'#include "Sandbox/player_controllers/MyPlayerController\.h"',
     r'#include "Sandbox/players/playable/player_controllers/MyPlayerController.h"'),
    (r'#include "Sandbox/data/jetpack/JetpackState\.h"',
     r'#include "Sandbox/players/playable/data/JetpackState.h"'),
    (r'#include "Sandbox/data/CombatProfile\.h"',
     r'#include "Sandbox/players/npcs/data/CombatProfile.h"'),
    (r'#include "Sandbox/actor_components/JetpackComponent\.h"',
     r'#include "Sandbox/players/playable/actor_components/JetpackComponent.h"'),
    (r'#include "Sandbox/actor_components/SpeedBoostComponent\.h"',
     r'#include "Sandbox/players/playable/actor_components/SpeedBoostComponent.h"'),
    (r'#include "Sandbox/actor_components/InteractorComponent\.h"',
     r'#include "Sandbox/players/playable/actor_components/InteractorComponent.h"'),
    (r'#include "Sandbox/actor_components/WarpToPlayerStartActorComponent\.h"',
     r'#include "Sandbox/players/playable/actor_components/WarpToPlayerStartActorComponent.h"'),
    (r'#include "Sandbox/actor_components/CoinCollectorActorComponent\.h"',
     r'#include "Sandbox/players/playable/actor_components/CoinCollectorActorComponent.h"'),
    (r'#include "Sandbox/interfaces/playable/MaxSpeedChangeListener\.h"',
     r'#include "Sandbox/players/playable/interfaces/MaxSpeedChangeListener.h"'),
    (r'#include "Sandbox/actor_components/DamageAuraComponent\.h"',
     r'#include "Sandbox/players/npcs/actor_components/DamageAuraComponent.h"'),
    (r'#include "Sandbox/actors/PawnSpawner\.h"',
     r'#include "Sandbox/players/npcs/actors/PawnSpawner.h"'),

    # Items & Inventory
    (r'#include "Sandbox/actors/PickupActor\.h"',
     r'#include "Sandbox/items/actors/PickupActor.h"'),
    (r'#include "Sandbox/actors/Coin\.h"',
     r'#include "Sandbox/items/actors/Coin.h"'),
    (r'#include "Sandbox/actors/JumpUpgradePickup\.h"',
     r'#include "Sandbox/items/actors/JumpUpgradePickup.h"'),
    (r'#include "Sandbox/actors/SpeedBoostItemActor\.h"',
     r'#include "Sandbox/items/actors/SpeedBoostItemActor.h"'),
    (r'#include "Sandbox/actor_components/SpeedBoostItemComponent\.h"',
     r'#include "Sandbox/items/actor_components/SpeedBoostItemComponent.h"'),
    (r'#include "Sandbox/interfaces/inventory/InventoryItem\.h"',
     r'#include "Sandbox/inventory/interfaces/InventoryItem.h"'),
    (r'#include "Sandbox/actor_components/inventory/InventoryComponent\.h"',
     r'#include "Sandbox/inventory/actor_components/InventoryComponent.h"'),
    (r'#include "Sandbox/data/inventory/InventorySlot\.h"',
     r'#include "Sandbox/inventory/data/InventorySlot.h"'),
    (r'#include "Sandbox/data/inventory/Dimensions\.h"',
     r'#include "Sandbox/inventory/data/Dimensions.h"'),
    (r'#include "Sandbox/data/inventory/Coord\.h"',
     r'#include "Sandbox/inventory/data/Coord.h"'),

    # Environment
    (r'#include "Sandbox/actor_components/RotatingActorComponent\.h"',
     r'#include "Sandbox/environment/effects/actor_components/RotatingActorComponent.h"'),
    (r'#include "Sandbox/actor_components/DynamicPivotActorComponent\.h"',
     r'#include "Sandbox/environment/effects/actor_components/DynamicPivotActorComponent.h"'),
    (r'#include "Sandbox/subsystems/world/RotationManagerSubsystem\.h"',
     r'#include "Sandbox/environment/effects/subsystems/RotationManagerSubsystem.h"'),
    (r'#include "Sandbox/subsystems/world/NiagaraNdcWriterSubsystem\.h"',
     r'#include "Sandbox/environment/effects/subsystems/NiagaraNdcWriterSubsystem.h"'),
    (r'#include "Sandbox/data/RotatePayload\.h"',
     r'#include "Sandbox/environment/effects/data/RotatePayload.h"'),
    (r'#include "Sandbox/actors/DayNightCycle\.h"',
     r'#include "Sandbox/environment/effects/actors/DayNightCycle.h"'),
    (r'#include "Sandbox/actor_components/LaunchpadComponent\.h"',
     r'#include "Sandbox/environment/traversal/actor_components/LaunchpadComponent.h"'),
    (r'#include "Sandbox/actor_components/RandLaunchpadComponent\.h"',
     r'#include "Sandbox/environment/traversal/actor_components/RandLaunchpadComponent.h"'),
    (r'#include "Sandbox/actor_components/LoopingLiftComponent\.h"',
     r'#include "Sandbox/environment/traversal/actor_components/LoopingLiftComponent.h"'),
    (r'#include "Sandbox/actor_components/LoopingPlatformComponent\.h"',
     r'#include "Sandbox/environment/traversal/actor_components/LoopingPlatformComponent.h"'),
    (r'#include "Sandbox/actor_components/SlidingPlatformActorComponent\.h"',
     r'#include "Sandbox/environment/traversal/actor_components/SlidingPlatformActorComponent.h"'),
    (r'#include "Sandbox/actor_components/TeleporterActorComponent\.h"',
     r'#include "Sandbox/environment/traversal/actor_components/TeleporterActorComponent.h"'),
    (r'#include "Sandbox/actor_components/HorizAntigravLiftComponent\.h"',
     r'#include "Sandbox/environment/traversal/actor_components/HorizAntigravLiftComponent.h"'),
    (r'#include "Sandbox/actor_components/AntigravLiftActorComponent\.h"',
     r'#include "Sandbox/environment/traversal/actor_components/AntigravLiftActorComponent.h"'),
    (r'#include "Sandbox/actors/LoopingLift\.h"',
     r'#include "Sandbox/environment/traversal/actors/LoopingLift.h"'),
    (r'#include "Sandbox/actors/TriggerButtonActor\.h"',
     r'#include "Sandbox/environment/interactive/actors/TriggerButtonActor.h"'),
    (r'#include "Sandbox/actors/SlidingDoor\.h"',
     r'#include "Sandbox/environment/interactive/actors/SlidingDoor.h"'),
    (r'#include "Sandbox/interfaces/Clickable\.h"',
     r'#include "Sandbox/environment/interactive/interfaces/Clickable.h"'),
    (r'#include "Sandbox/actors/LandMine\.h"',
     r'#include "Sandbox/environment/obstacles/actors/LandMine.h"'),
    (r'#include "Sandbox/actors/ForcefieldActor\.h"',
     r'#include "Sandbox/environment/obstacles/actors/ForcefieldActor.h"'),
    (r'#include "Sandbox/actors/KillableCube\.h"',
     r'#include "Sandbox/environment/obstacles/actors/KillableCube.h"'),
    (r'#include "Sandbox/data/LandMinePayload\.h"',
     r'#include "Sandbox/environment/obstacles/data/LandMinePayload.h"'),
    (r'#include "Sandbox/data/ForcefieldPayload\.h"',
     r'#include "Sandbox/environment/obstacles/data/ForcefieldPayload.h"'),
    (r'#include "Sandbox/actors/TiledCubeActor\.h"',
     r'#include "Sandbox/environment/structures/actors/TiledCubeActor.h"'),
    (r'#include "Sandbox/actors/TiledWallActor\.h"',
     r'#include "Sandbox/environment/structures/actors/TiledWallActor.h"'),

    # UI
    (r'#include "Sandbox/huds/MyHUD\.h"',
     r'#include "Sandbox/ui/hud/huds/MyHUD.h"'),
    (r'#include "Sandbox/widgets/HealthWidget\.h"',
     r'#include "Sandbox/ui/in_world/widgets/umg/HealthWidget.h"'),
    (r'#include "Sandbox/widgets/HealthStationWidget\.h"',
     r'#include "Sandbox/ui/hud/widgets/umg/HealthWidget.h"'),
    (r'#include "Sandbox/widgets/MainHUDWidget\.h"',
     r'#include "Sandbox/ui/hud/widgets/umg/MainHUDWidget.h"'),
    (r'#include "Sandbox/widgets/MainMenu2Widget\.h"',
     r'#include "Sandbox/ui/main_menu/widgets/umg/MainMenu2Widget.h"'),
    (r'#include "Sandbox/widgets/LevelSelect2Widget\.h"',
     r'#include "Sandbox/ui/main_menu/widgets/umg/LevelSelect2Widget.h"'),
    (r'#include "Sandbox/widgets/LoadLevelButtonWidget\.h"',
     r'#include "Sandbox/ui/main_menu/widgets/umg/LoadLevelButtonWidget.h"'),
    (r'#include "Sandbox/widgets/ValueWidget\.h"',
     r'#include "Sandbox/ui/widgets/umg/ValueWidget.h"'),
    (r'#include "Sandbox/widgets/TextButtonWidget\.h"',
     r'#include "Sandbox/ui/widgets/umg/TextButtonWidget.h"'),
    (r'#include "Sandbox/widgets/options_menu/OptionsWidget\.h"',
     r'#include "Sandbox/ui/options_menu/widgets/umg/OptionsWidget.h"'),
    (r'#include "Sandbox/widgets/options_menu/VideoSettingRowWidget\.h"',
     r'#include "Sandbox/ui/options_menu/widgets/umg/VideoSettingRowWidget.h"'),
    (r'#include "Sandbox/widgets/options_menu/VisualsOptionsWidget\.h"',
     r'#include "Sandbox/ui/options_menu/widgets/umg/VisualsOptionsWidget.h"'),
    (r'#include "Sandbox/widgets/options_menu/AudioOptionsWidget\.h"',
     r'#include "Sandbox/ui/options_menu/widgets/umg/AudioOptionsWidget.h"'),
    (r'#include "Sandbox/widgets/options_menu/GameplayOptionsWidget\.h"',
     r'#include "Sandbox/ui/options_menu/widgets/umg/GameplayOptionsWidget.h"'),
    (r'#include "Sandbox/widgets/options_menu/ControlsOptionsWidget\.h"',
     r'#include "Sandbox/ui/options_menu/widgets/umg/ControlsOptionsWidget.h"'),
    (r'#include "Sandbox/player_controllers/MainMenu2PlayerController\.h"',
     r'#include "Sandbox/ui/controllers/MainMenu2PlayerController.h"'),

    # Interaction (trigger/collision)
    (r'#include "Sandbox/subsystems/world/TriggerSubsystem\.h"',
     r'#include "Sandbox/interaction/triggering/subsystems/TriggerSubsystem.h"'),
    (r'#include "Sandbox/subsystems/world/TriggerSubsystemCore\.h"',
     r'#include "Sandbox/interaction/triggering/subsystems/TriggerSubsystemCore.h"'),
    (r'#include "Sandbox/data/trigger/TriggerContext\.h"',
     r'#include "Sandbox/interaction/triggering/data/TriggerContext.h"'),
    (r'#include "Sandbox/data/trigger/TriggerResult\.h"',
     r'#include "Sandbox/interaction/triggering/data/TriggerResult.h"'),
    (r'#include "Sandbox/data/trigger/TriggerableId\.h"',
     r'#include "Sandbox/interaction/triggering/data/TriggerableId.h"'),
    (r'#include "Sandbox/data/trigger/TriggerOtherPayload\.h"',
     r'#include "Sandbox/interaction/triggering/data/TriggerOtherPayload.h"'),
    (r'#include "Sandbox/concepts/TriggerPayloadConcept\.h"',
     r'#include "Sandbox/interaction/triggering/concepts/TriggerPayloadConcept.h"'),
    (r'#include "Sandbox/subsystems/world/CollisionEffectSubsystem\.h"',
     r'#include "Sandbox/interaction/collision/subsystems/CollisionEffectSubsystem.h"'),
    (r'#include "Sandbox/subsystems/world/CollisionEffectSubsystemCore\.h"',
     r'#include "Sandbox/interaction/collision/subsystems/CollisionEffectSubsystemCore.h"'),
    (r'#include "Sandbox/data/collision/CollisionPayloads\.h"',
     r'#include "Sandbox/interaction/collision/data/CollisionPayloads.h"'),

    # Movement
    (r'#include "Sandbox/actor_components/MoveToTargetActorComponent\.h"',
     r'#include "Sandbox/movement/actor_components/MoveToTargetActorComponent.h"'),

    # Game flow
    (r'#include "Sandbox/game_modes/MyGameModeBase\.h"',
     r'#include "Sandbox/game_flow/game_modes/MyGameModeBase.h"'),
    (r'#include "Sandbox/game_states/PlatformerGameState\.h"',
     r'#include "Sandbox/game_flow/game_states/PlatformerGameState.h"'),

    # Core
    (r'#include "Sandbox/subsystems/world/ObjectPoolSubsystem\.h"',
     r'#include "Sandbox/core/object_pooling/subsystems/ObjectPoolSubsystem.h"'),
    (r'#include "Sandbox/subsystems/world/ObjectPoolSubsystemCore\.h"',
     r'#include "Sandbox/core/object_pooling/subsystems/ObjectPoolSubsystemCore.h"'),
    (r'#include "Sandbox/data/pool/PoolConfig\.h"',
     r'#include "Sandbox/core/object_pooling/data/PoolConfig.h"'),
    (r'#include "Sandbox/actors/ObjectPoolWarmer\.h"',
     r'#include "Sandbox/core/object_pooling/actors/ObjectPoolWarmer.h"'),
    (r'#include "Sandbox/subsystems/world/DestructionManagerSubsystem\.h"',
     r'#include "Sandbox/core/destruction/subsystems/DestructionManagerSubsystem.h"'),
    (r'#include "Sandbox/actor_components/RemoveGhostsOnStartComponent\.h"',
     r'#include "Sandbox/core/destruction/actor_components/RemoveGhostsOnStartComponent.h"'),
    (r'#include "Sandbox/actor_components/StaticOrbitCameraComponent\.h"',
     r'#include "Sandbox/core/camera/StaticOrbitCameraComponent.h"'),
    (r'#include "Sandbox/subsystems/world/MassArchetypeSubsystem\.h"',
     r'#include "Sandbox/mass_entity/subsystems/MassArchetypeSubsystem.h"'),
    (r'#include "Sandbox/subsystems/FrameLogFooterSubsystem\.h"',
     r'#include "Sandbox/logging/subsystems/FrameLogFooterSubsystem.h"'),
    (r'#include "Sandbox/data/VideoSettingsData\.h"',
     r'#include "Sandbox/core/video_settings/VideoSettingsData.h"'),
    (r'#include "Sandbox/data/VideoSettingConfig\.h"',
     r'#include "Sandbox/core/video_settings/VideoSettingConfig.h"'),
    (r'#include "Sandbox/data/VideoRowData\.h"',
     r'#include "Sandbox/core/video_settings/VideoRowData.h"'),
    (r'#include "Sandbox/enums/VisualQualityLevel\.h"',
     r'#include "Sandbox/core/video_settings/VisualQualityLevel.h"'),

    # Learning/misc
    (r'#include "Sandbox/actor_components/InteractRotateComponent\.h"',
     r'#include "Sandbox/misc/learning/actor_components/InteractRotateComponent.h"'),
    (r'#include "Sandbox/actors/BoxSplineMover\.h"',
     r'#include "Sandbox/misc/learning/actors/BoxSplineMover.h"'),
    (r'#include "Sandbox/actors/ClickableLightChangeActor\.h"',
     r'#include "Sandbox/misc/learning/actors/ClickableLightChangeActor.h"'),
    (r'#include "Sandbox/actors/MovesWhenClickedActor\.h"',
     r'#include "Sandbox/misc/learning/actors/MovesWhenClickedActor.h"'),
    (r'#include "Sandbox/actors/RotatingOnClickActor\.h"',
     r'#include "Sandbox/misc/learning/actors/RotatingOnClickActor.h"'),
    (r'#include "Sandbox/actors/TalkingPillar\.h"',
     r'#include "Sandbox/misc/learning/actors/TalkingPillar.h"'),
    (r'#include "Sandbox/actors/WaterResetTriggerBox\.h"',
     r'#include "Sandbox/misc/learning/actors/WaterResetTriggerBox.h"'),

    # Additional patterns - round 2
    (r'#include "Sandbox/actor_components/WarpComponent\.h"',
     r'#include "Sandbox/players/playable/actor_components/WarpComponent.h"'),
    (r'#include "Sandbox/actor_components/weapons/PawnWeaponComponent\.h"',
     r'#include "Sandbox/combat/weapons/actor_components/PawnWeaponComponent.h"'),
    (r'#include "Sandbox/actors/BulletActor\.h"',
     r'#include "Sandbox/combat/projectiles/actors/BulletActor.h"'),
    (r'#include "Sandbox/actors/doors/SlidingDoor\.h"',
     r'#include "Sandbox/environment/interactive/actors/SlidingDoor.h"'),
    (r'#include "Sandbox/actors/Explosion\.h"',
     r'#include "Sandbox/combat/effects/actors/Explosion.h"'),
    (r'#include "Sandbox/actors/MassBulletSubsystemData\.h"',
     r'#include "Sandbox/combat/projectiles/actors/MassBulletSubsystemData.h"'),
    (r'#include "Sandbox/actors/MassBulletVisualizationActor\.h"',
     r'#include "Sandbox/combat/projectiles/actors/MassBulletVisualizationActor.h"'),
    (r'#include "Sandbox/actors/mobs/PawnSpawner\.h"',
     r'#include "Sandbox/players/npcs/actors/PawnSpawner.h"'),
    (r'#include "Sandbox/actors/weapons/WeaponBase\.h"',
     r'#include "Sandbox/combat/weapons/actors/WeaponBase.h"'),
    (r'#include "Sandbox/data/ai/CombatProfile\.h"',
     r'#include "Sandbox/players/npcs/data/CombatProfile.h"'),
    (r'#include "Sandbox/data/collision/CollisionContext\.h"',
     r'#include "Sandbox/interaction/collision/data/CollisionContext.h"'),
    (r'#include "Sandbox/data/collision/LandMinePayload\.h"',
     r'#include "Sandbox/environment/obstacles/data/LandMinePayload.h"'),
    (r'#include "Sandbox/data/health/HealthData\.h"',
     r'#include "Sandbox/health/data/HealthData.h"'),
    (r'#include "Sandbox/data/health/StationStateData\.h"',
     r'#include "Sandbox/health/data/StationStateData.h"'),
    (r'#include "Sandbox/data/HumanoidMovement\.h"',
     r'#include "Sandbox/players/playable/data/HumanoidMovement.h"'),
    (r'#include "Sandbox/data/JetpackState\.h"',
     r'#include "Sandbox/players/playable/data/JetpackState.h"'),
    (r'#include "Sandbox/data/PayloadIndex\.h"',
     r'#include "Sandbox/interaction/collision/data/PayloadIndex.h"'),
    (r'#include "Sandbox/data/SpeedBoost\.h"',
     r'#include "Sandbox/items/data/SpeedBoost.h"'),
    (r'#include "Sandbox/data/trigger/ForcefieldPayload\.h"',
     r'#include "Sandbox/environment/obstacles/data/ForcefieldPayload.h"'),
    (r'#include "Sandbox/data/trigger/RotatePayload\.h"',
     r'#include "Sandbox/environment/effects/data/RotatePayload.h"'),
    (r'#include "Sandbox/data/trigger/StrongIds\.h"',
     r'#include "Sandbox/interaction/StrongIds.h"'),
    (r'#include "Sandbox/data/trigger/TriggerCapabilities\.h"',
     r'#include "Sandbox/interaction/triggering/data/TriggerCapabilities.h"'),
    (r'#include "Sandbox/data/trigger/WeaponPickupPayload\.h"',
     r'#include "Sandbox/combat/weapons/data/WeaponPickupPayload.h"'),
    (r'#include "Sandbox/data/video_options/VideoRowData\.h"',
     r'#include "Sandbox/core/video_settings/VideoRowData.h"'),
    (r'#include "Sandbox/data/video_options/VideoSettingConfig\.h"',
     r'#include "Sandbox/core/video_settings/VideoSettingConfig.h"'),
    (r'#include "Sandbox/data/video_options/VideoSettingRange\.h"',
     r'#include "Sandbox/core/video_settings/VideoSettingRange.h"'),
    (r'#include "Sandbox/data/video_options/VideoSettingsData\.h"',
     r'#include "Sandbox/core/video_settings/VideoSettingsData.h"'),
    (r'#include "Sandbox/data/video_options/VisualQualityLevel\.h"',
     r'#include "Sandbox/core/video_settings/VisualQualityLevel.h"'),
    (r'#include "Sandbox/enums/ForcefieldState\.h"',
     r'#include "Sandbox/environment/obstacles/enums/ForcefieldState.h"'),
    (r'#include "Sandbox/enums/LandMineState\.h"',
     r'#include "Sandbox/environment/obstacles/enums/LandMineState.h"'),
    (r'#include "Sandbox/enums/MobAttackMode\.h"',
     r'#include "Sandbox/players/npcs/enums/MobAttackMode.h"'),
    (r'#include "Sandbox/enums/RotationType\.h"',
     r'#include "Sandbox/environment/effects/enums/RotationType.h"'),
    (r'#include "Sandbox/enums/SimpleAIState\.h"',
     r'#include "Sandbox/players/npcs/enums/SimpleAIState.h"'),
    (r'#include "Sandbox/enums/TeamID\.h"',
     r'#include "Sandbox/players/common/enums/TeamID.h"'),
    (r'#include "Sandbox/huds/MyHud\.h"',
     r'#include "Sandbox/ui/hud/huds/MyHud.h"'),
    (r'#include "Sandbox/interfaces/clickable\.h"',
     r'#include "Sandbox/environment/interactive/interfaces/Clickable.h"'),
    (r'#include "Sandbox/interfaces/CollisionOwner\.h"',
     r'#include "Sandbox/interaction/collision/interfaces/CollisionOwner.h"'),
    (r'#include "Sandbox/interfaces/CombatActor\.h"',
     r'#include "Sandbox/players/npcs/interfaces/CombatActor.h"'),
    (r'#include "Sandbox/interfaces/DeathHandler\.h"',
     r'#include "Sandbox/health/interfaces/DeathHandler.h"'),
    (r'#include "Sandbox/interfaces/MaxSpeedChangeListener\.h"',
     r'#include "Sandbox/players/playable/interfaces/MaxSpeedChangeListener.h"'),
    (r'#include "Sandbox/interfaces/MovementMultiplierReceiver\.h"',
     r'#include "Sandbox/players/playable/interfaces/MovementMultiplierReceiver.h"'),
    (r'#include "Sandbox/interfaces/SandboxMobInterface\.h"',
     r'#include "Sandbox/players/npcs/interfaces/SandboxMobInterface.h"'),
    (r'#include "Sandbox/subsystems/game_instance/FrameLogFooterSubsystem\.h"',
     r'#include "Sandbox/logging/subsystems/FrameLogFooterSubsystem.h"'),
    (r'#include "Sandbox/widgets/CommonMenuDelegates\.h"',
     r'#include "Sandbox/ui/main_menu/delegates/CommonMenuDelegates.h"'),
]


def fix_file(filepath: Path) -> int:
    """Fix includes in a single file. Returns the number of changes made."""
    try:
        content = filepath.read_text(encoding='utf-8')
        original_content = content
        changes = 0

        for old_pattern, new_pattern in INCLUDE_MAPPINGS:
            new_content, n = re.subn(old_pattern, new_pattern, content)
            if n > 0:
                content = new_content
                changes += n

        if content != original_content:
            filepath.write_text(content, encoding='utf-8')
            return changes
        return 0
    except Exception as e:
        print(f"Error processing {filepath}: {e}")
        return 0


def main():
    """Fix all broken includes in the Sandbox directory."""
    source_dir = Path(r"C:\Users\matthew\source\repos\nukethebees\ue5_sandbox\Source\Sandbox")

    total_files = 0
    total_changes = 0

    for ext in ('*.h', '*.cpp'):
        for filepath in source_dir.rglob(ext):
            changes = fix_file(filepath)
            if changes > 0:
                total_files += 1
                total_changes += changes
                print(f"Fixed {changes} includes in {filepath.relative_to(source_dir)}")

    print(f"\nFixed {total_changes} includes across {total_files} files")


if __name__ == "__main__":
    main()
