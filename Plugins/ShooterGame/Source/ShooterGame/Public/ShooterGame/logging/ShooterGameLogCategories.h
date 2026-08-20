#pragma once

#include "Logging/LogMacros.h"

DECLARE_LOG_CATEGORY_EXTERN(LogShooterGameFrameCount, Log, All);

// Systems
DECLARE_LOG_CATEGORY_EXTERN(LogShooterGame, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogShooterGameUI, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogShooterGameMassEntity, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogShooterGameAI, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogShooterGameActor, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogShooterGameCharacter, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogShooterGameController, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogShooterGameActorComponent, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogShooterGameSubsystem, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogShooterGameGameMode, Log, All);

// Gameplay
DECLARE_LOG_CATEGORY_EXTERN(LogShooterGameWeapon, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogShooterGameInput, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogShooterGameInventory, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogShooterGameHealth, Log, All);

DECLARE_LOG_CATEGORY_EXTERN(LogShooterGameEntities, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogShooterGameNavigation, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogShooterGameTargeting, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogShooterGameCombat, Log, All);

// Misc
DECLARE_LOG_CATEGORY_EXTERN(LogShooterGameLearning, Log, All);
