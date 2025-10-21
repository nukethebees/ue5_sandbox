#include "MyPlayerController.h"

#include "Camera/CameraComponent.h"

#include "Sandbox/misc/learning/actors/TalkingPillar.h"
#include "Sandbox/players/playable/actor_components/InteractorComponent.h"
#include "Sandbox/players/playable/actor_components/WarpComponent.h"
#include "Sandbox/ui/hud/huds/MyHud.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

#define FORWARD_CALL_TO_CHARACTER(METHOD_CALL, ...) \
    RETURN_IF_NULLPTR(controlled_character);        \
    controlled_character->METHOD_CALL(__VA_ARGS__);

void AMyPlayerController::BeginPlay() {
    Super::BeginPlay();

    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
    bEnableClickEvents = true;
    bEnableMouseOverEvents = false;

    set_game_input_mode();

    add_input_mapping_context(input.base_context);

    TRY_INIT_PTR(pawn, GetPawn());
    TRY_INIT_PTR(character, Cast<AMyCharacter>(pawn));
    TRY_INIT_PTR(hud, Cast<AMyHUD>(GetHUD()));
    character->OnMaxSpeedChanged.AddDynamic(hud, &AMyHUD::update_max_speed);
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

    TRY_INIT_PTR(eic, Cast<UEnhancedInputComponent>(InputComponent));

    using enum ETriggerEvent;

    auto bind{make_input_binder(eic)};

    // Movement
    bind(input.warp_to_cursor, Completed, &AMyPlayerController::warp_to_cursor);

    // Vision
    bind(input.look, Triggered, &AMyPlayerController::look);

    // Combat
    bind(input.attack, Started, &AMyPlayerController::attack_started);
    bind(input.attack, Ongoing, &AMyPlayerController::attack_continued);
    bind(input.attack, Completed, &AMyPlayerController::attack_ended);

    // Inventory
    bind(input.cycle_next_weapon, Started, &AMyPlayerController::cycle_next_weapon);
    bind(input.cycle_prev_weapon, Started, &AMyPlayerController::cycle_prev_weapon);
    bind(input.unequip_weapon, Triggered, &AMyPlayerController::unequip_weapon);
    bind(input.drop_weapon, Triggered, &AMyPlayerController::drop_weapon);
    bind(input.reload_weapon, Started, &AMyPlayerController::reload_weapon);

    // Interaction
    bind(input.toggle_mouse, Started, &AMyPlayerController::toggle_mouse);
    bind(input.mouse_click, Started, &AMyPlayerController::mouse_click);
    bind(input.interact, Started, &AMyPlayerController::interact);

    // UI
    bind(input.toggle_in_game_menu, Started, &AMyPlayerController::toggle_in_game_menu);
}

// Movement
void AMyPlayerController::warp_to_cursor(FInputActionValue const& value) {
    RETURN_IF_NULLPTR(controlled_character);

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

// Vision
void AMyPlayerController::look(FInputActionValue const& value) {
    RETURN_IF_NULLPTR(controlled_character);
    controlled_character->look(value);
}

// Combat
void AMyPlayerController::attack_started() {
    FORWARD_CALL_TO_CHARACTER(attack_started);
    attack_elapsed_time = 0.0f;
}
void AMyPlayerController::attack_continued(FInputActionInstance const& instance) {
    auto const now_elapsed{instance.GetElapsedTime()};
    auto const delta_time{now_elapsed - attack_elapsed_time};
    attack_elapsed_time = now_elapsed;

    FORWARD_CALL_TO_CHARACTER(attack_continued, delta_time);
}
void AMyPlayerController::attack_ended() {
    FORWARD_CALL_TO_CHARACTER(attack_ended);
}

// Inventory
void AMyPlayerController::cycle_next_weapon() {
    FORWARD_CALL_TO_CHARACTER(cycle_next_weapon);
}
void AMyPlayerController::cycle_prev_weapon() {
    FORWARD_CALL_TO_CHARACTER(cycle_prev_weapon);
}
void AMyPlayerController::unequip_weapon() {
    FORWARD_CALL_TO_CHARACTER(unequip_weapon);
}
void AMyPlayerController::drop_weapon() {
    FORWARD_CALL_TO_CHARACTER(drop_weapon);
}
void AMyPlayerController::reload_weapon() {
    FORWARD_CALL_TO_CHARACTER(reload_weapon);
}

// Interaction
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
        );
#endif
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
void AMyPlayerController::interact() {
    RETURN_IF_NULLPTR(controlled_character);

    FVector view_location;
    FRotator view_rotation;
    GetPlayerViewPoint(view_location, view_rotation);
    controlled_character->interact(view_location, view_rotation);
}

// UI
void AMyPlayerController::toggle_in_game_menu() {
    TRY_INIT_PTR(hud, Cast<AMyHUD>(GetHUD()));
    hud->toggle_in_game_menu();
}
void AMyPlayerController::set_game_input_mode() {
    auto input_mode{FInputModeGameOnly()};
    SetInputMode(input_mode);
    bShowMouseCursor = false;
    swap_input_mapping_context(input.cursor_mode_context, input.direct_mode_context);

    RETURN_IF_NULLPTR(controlled_character);
}
void AMyPlayerController::set_mouse_input_mode() {
    auto input_mode{FInputModeGameAndUI()};
    SetInputMode(input_mode);
    bShowMouseCursor = true;
    swap_input_mapping_context(input.direct_mode_context, input.cursor_mode_context);
}

void AMyPlayerController::add_input_mapping_context(UInputMappingContext* context) {
    TRY_INIT_PTR(local_player, GetLocalPlayer());
    TRY_INIT_PTR(subsystem,
                 ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(local_player));
    subsystem->AddMappingContext(context, 0);
}
void AMyPlayerController::swap_input_mapping_context(UInputMappingContext* to_remove,
                                                     UInputMappingContext* to_add) {
    TRY_INIT_PTR(local_player, GetLocalPlayer());
    TRY_INIT_PTR(subsystem,
                 ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(local_player));
    subsystem->RemoveMappingContext(to_remove);
    subsystem->AddMappingContext(to_add, 0);
}
