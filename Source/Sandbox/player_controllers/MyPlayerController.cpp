#include "MyPlayerController.h"

#include "Sandbox/actors/TalkingPillar.h"
#include "Sandbox/huds/MyHud.h"

void AMyPlayerController::BeginPlay() {
    Super::BeginPlay();

    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
    bEnableClickEvents = true;
    bEnableMouseOverEvents = false;

    set_game_input_mode();

    add_input_mapping_context(input.base_context);

    if (auto* character{Cast<AMyCharacter>(GetPawn())}) {
        if (auto* hud{Cast<AMyHUD>(GetHUD())}) {
            character->OnMaxSpeedChanged.AddDynamic(hud, &AMyHUD::update_max_speed);
        }
    } else {
        log_warning(TEXT("Could not cast to AMyCharacter."));
    }
}
void AMyPlayerController::OnPossess(APawn* InPawn) {
    Super::OnPossess(InPawn);

    controlled_character = Cast<AMyCharacter>(InPawn);
}
void AMyPlayerController::Tick(float DeltaSeconds) {
    Super::Tick(DeltaSeconds);

    if (!controlled_character) {
        if (!tick_no_controller_character_warning_fired) {
            log_warning(TEXT("No controlled character"));
            tick_no_controller_character_warning_fired = true;
        }

        return;
    }
    tick_no_controller_character_warning_fired = false;

    constexpr float torch_target_scale{1000.0f};

    if (bShowMouseCursor) {
        FVector world_location;
        FVector world_direction;
        if (DeprojectMousePositionToWorld(world_location, world_direction)) {
            auto const target_location{world_location + world_direction * torch_target_scale};
            controlled_character->aim_torch(target_location);
        }
    } else {
        if (auto const* camera{controlled_character->get_active_camera()}) {
            auto const camera_location{camera->GetComponentLocation()};
            auto const camera_rotation{camera->GetComponentRotation()};
            auto const look_target{camera_location + camera_rotation.Vector() * torch_target_scale};

            controlled_character->aim_torch(look_target);
        }
    }
}

void AMyPlayerController::SetupInputComponent() {
    Super::SetupInputComponent();

    if (auto* eic{Cast<UEnhancedInputComponent>(InputComponent)}) {
        using enum ETriggerEvent;

        auto bind{make_input_binder(eic)};

        bind(input.look, Triggered, &AMyPlayerController::look);
        bind(input.attack, Started, &AMyPlayerController::attack);
        bind(input.toggle_mouse, Started, &AMyPlayerController::toggle_mouse);
        bind(input.mouse_click, Started, &AMyPlayerController::mouse_click);
        bind(input.warp_to_cursor, Completed, &AMyPlayerController::warp_to_cursor);
    } else {
        log_warning(TEXT("Did not get the UEnhancedInputComponent."));
    }
}

void AMyPlayerController::look(FInputActionValue const& value) {
    if (controlled_character) {
        controlled_character->look(value);
    }
}
void AMyPlayerController::attack() {
    if (controlled_character) {
        controlled_character->attack();
    }
}
void AMyPlayerController::toggle_mouse() {
    if (bShowMouseCursor) {
        set_game_input_mode();
    } else {
        set_mouse_input_mode();
    }
}
void AMyPlayerController::mouse_click(FInputActionValue const& value) {
    if (!bShowMouseCursor) {
        return;
    }
    log_verbose(TEXT("Shooting a ray."));

    FVector world_location;
    FVector world_direction;
    if (DeprojectMousePositionToWorld(world_location, world_direction)) {
        auto const end{world_location + (world_direction * 10000)};
        FHitResult hit_result;
        GetWorld()->LineTraceSingleByChannel(hit_result, world_location, end, ECC_Visibility);
#if WITH_EDITOR
        DrawDebugLine(GetWorld(),
                      world_location - FVector(0.0f, 0.0f, 10.0f),
                      end,
                      FColor::Green,
                      false, // Not persistent
                      2.0f,  // Duration in seconds
                      0,     // Depth priority
                      1.0f   // Thickness
#endif
        );
        if (hit_result.bBlockingHit) {
            DrawDebugSphere(GetWorld(),
                            hit_result.ImpactPoint,
                            10.0f, // Radius
                            12,    // Segments
                            FColor::Red,
                            false,
                            2.0f);
        }

        if (auto* actor_hit{hit_result.GetActor()}) {
            IClickable::try_click(actor_hit);
            for (auto* component : actor_hit->GetComponents().Array()) {
                IClickable::try_click(component);
            }
        }
    }
}
void AMyPlayerController::warp_to_cursor(FInputActionValue const& value) {
    if (!controlled_character) {
        log_warning(TEXT("No char to warp"));
        return;
    }
    if (!bShowMouseCursor) {
        return;
    }

    FVector world_location;
    FVector world_direction;

    if (DeprojectMousePositionToWorld(world_location, world_direction)) {
        auto const start{world_location};
        auto const end{start + world_direction * 10000.0f};

        FHitResult hit;
        FCollisionQueryParams params;
        params.AddIgnoredActor(controlled_character);

        if (GetWorld()->LineTraceSingleByChannel(hit, start, end, ECC_Visibility, params)) {
            controlled_character->warp->warp_to_location(controlled_character, hit.Location);
        }
    }
}

void AMyPlayerController::set_game_input_mode() {
    auto input_mode{FInputModeGameOnly()};
    SetInputMode(input_mode);
    bShowMouseCursor = false;
    swap_input_mapping_context(input.cursor_mode_context, input.direct_mode_context);
    if (controlled_character) {
        controlled_character->reset_torch_position();
    }
}
void AMyPlayerController::set_mouse_input_mode() {
    auto input_mode{FInputModeGameAndUI()};
    SetInputMode(input_mode);
    bShowMouseCursor = true;
    swap_input_mapping_context(input.direct_mode_context, input.cursor_mode_context);
}

void AMyPlayerController::add_input_mapping_context(UInputMappingContext* context) {
    if (auto* local_player{GetLocalPlayer()}) {
        if (auto* subsystem{
                ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(local_player)}) {
            subsystem->AddMappingContext(context, 0);
        } else {
            log_warning(TEXT("Could not get UEnhancedInputLocalPlayerSubsystem."));
        }
    } else {
        log_warning(TEXT("Could not get local player."));
    }
}
void AMyPlayerController::swap_input_mapping_context(UInputMappingContext* to_remove,
                                                     UInputMappingContext* to_add) {
    if (auto* local_player{GetLocalPlayer()}) {
        if (auto* subsystem{
                ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(local_player)}) {
            subsystem->RemoveMappingContext(to_remove);
            subsystem->AddMappingContext(to_add, 0);
        } else {
            log_warning(TEXT("Could not get UEnhancedInputLocalPlayerSubsystem."));
        }
    } else {
        log_warning(TEXT("Could not get local player."));
    }
}
