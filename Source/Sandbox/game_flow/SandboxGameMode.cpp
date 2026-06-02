#include "SandboxGameMode.h"

#include "Sandbox/core/SandboxDeveloperSettings.h"
#include "Sandbox/logging/SandboxLogCategories.h"

#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

ASandboxGameMode::ASandboxGameMode() {
    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;
}

void ASandboxGameMode::InitGame(FString const& MapName,
                                FString const& Options,
                                FString& ErrorMessage) {
    Super::InitGame(MapName, Options, ErrorMessage);
}

void ASandboxGameMode::BeginPlay() {
    Super::BeginPlay();

    auto const* settings{GetDefault<USandboxDeveloperSettings>()};
    auto* const world{GetWorld()};
    auto* pc{UGameplayStatics::GetPlayerController(this, 0)};

    if (!pc) {
        UE_LOG(LogSandboxGameMode, Warning, TEXT("ASandboxGameMode::BeginPlay: pc is nullptr"));
        return;
    }
    if (!world) {
        UE_LOG(LogSandboxGameMode, Warning, TEXT("ASandboxGameMode::BeginPlay: world is nullptr"));
        return;
    }

#if WITH_EDITOR
    if (settings->show_collision) {
        pc->ConsoleCommand(TEXT("show collision"));
    }
#endif
}
