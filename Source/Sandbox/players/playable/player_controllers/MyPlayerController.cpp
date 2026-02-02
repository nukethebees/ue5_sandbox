#include "MyPlayerController.h"

#include "Camera/CameraComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/GameViewportClient.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "TimerManager.h"

#include "Sandbox/combat/pawn_weapon_component/PawnWeaponComponent.h"
#include "Sandbox/environment/interactive/interfaces/Clickable.h"
#include "Sandbox/game_flow/PlatformerGameState.h"
#include "Sandbox/health/HealthComponent.h"
#include "Sandbox/inventory/InventoryComponent.h"
#include "Sandbox/players/common/actor_components/ActorDescriptionScannerComponent.h"
#include "Sandbox/players/playable/actor_components/InteractorComponent.h"
#include "Sandbox/players/playable/actor_components/JetpackComponent.h"
#include "Sandbox/players/playable/actor_components/WarpComponent.h"
#include "Sandbox/ui/hud/widgets/umg/AmmoHUDWidget.h"
#include "Sandbox/ui/hud/widgets/umg/ItemDescriptionHUDWidget.h"
#include "Sandbox/ui/hud/widgets/umg/MainHUDWidget.h"
#include "Sandbox/ui/hud/widgets/umg/TargetOverlayHUDWidget.h"
#include "Sandbox/ui/in_game_menu/widgets/umg/InGamePlayerMenu.h"
#include "Sandbox/ui/widgets/umg/ValueWidget.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

#define FORWARD_CALL_TO_CHARACTER(METHOD_CALL, ...) \
    RETURN_IF_NULLPTR(controlled_character);        \
    controlled_character->METHOD_CALL(__VA_ARGS__);

AMyPlayerController::AMyPlayerController() {
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
    bEnableClickEvents = true;
    bEnableMouseOverEvents = false;
}

void AMyPlayerController::BeginPlay() {
    Super::BeginPlay();

    PlayerCameraManager->GetCameraViewPoint(cache.camera_location, cache.camera_rotation);
    set_game_input_mode();
    add_input_mapping_context(input.base_context);

    TRY_INIT_PTR(world, GetWorld());
    constexpr bool loop_timer{true};
    world->GetTimerManager().SetTimer(description_scanner_timer_handle,
                                      this,
                                      &AMyPlayerController::perform_description_scan,
                                      description_scan_interval,
                                      loop_timer);
}
void AMyPlayerController::OnPossess(APawn* InPawn) {
    Super::OnPossess(InPawn);

    controlled_character = Cast<AMyCharacter>(InPawn);
    RETURN_IF_NULLPTR(controlled_character);

    TRY_INIT_PTR(world, GetWorld());
    add_input_mapping_context(input.character_context);
    initialise_hud(*world, *controlled_character);
}
void AMyPlayerController::Tick(float DeltaSeconds) {
    constexpr auto logger{NestedLogger<"Tick">()};
    Super::Tick(DeltaSeconds);

    if (!controlled_character) {
        if (!tick_no_controller_character_warning_fired) {
            logger.log_warning(TEXT("No controlled character"));
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

    FVector current_camera_location;
    FRotator current_camera_rotation;
    PlayerCameraManager->GetCameraViewPoint(current_camera_location, current_camera_rotation);

    bool const has_camera_moved = !current_camera_location.Equals(cache.camera_location, 1.0f) ||
                                  !current_camera_rotation.Equals(cache.camera_rotation, 0.1f);

    if (has_camera_moved) {
        cache.camera_location = current_camera_location;
        cache.camera_rotation = current_camera_rotation;

        if (tracking_target_outline) {
            update_target_screen_bounds(cache.outline_corners);
        }
    }
}

void AMyPlayerController::SetupInputComponent() {
    Super::SetupInputComponent();

    TRY_INIT_PTR(eic, Cast<UEnhancedInputComponent>(InputComponent));

    using enum ETriggerEvent;

    auto bind{make_input_binder(eic)};

    // Movement
    bind(input.move, Triggered, &ThisClass::move);

    bind(input.jump, Started, &ThisClass::start_jump);
    bind(input.jump, Completed, &ThisClass::stop_jump);

    bind(input.crouch, Started, &ThisClass::start_crouch);
    bind(input.crouch, Completed, &ThisClass::stop_crouch);

    bind(input.sprint, Started, &ThisClass::start_sprint);
    bind(input.sprint, Completed, &ThisClass::stop_sprint);

    bind(input.warp_to_cursor, Completed, &ThisClass::warp_to_cursor);

    bind(input.jetpack, Triggered, &ThisClass::start_jetpack);
    bind(input.jetpack, Completed, &ThisClass::stop_jetpack);

    // Vision
    bind(input.look, Triggered, &ThisClass::look);
    bind(input.cycle_camera, Started, &ThisClass::cycle_camera);

    // Combat
    bind(input.attack, Started, &ThisClass::attack_started);
    bind(input.attack, Ongoing, &ThisClass::attack_continued);
    bind(input.attack, Completed, &ThisClass::attack_ended);

    // Torch
    bind(input.toggle_torch, Started, &ThisClass::toggle_torch);
    bind(input.scroll_torch_cone, Triggered, &ThisClass::scroll_torch_cone);

    // Inventory
    bind(input.cycle_next_weapon, Started, &ThisClass::cycle_next_weapon);
    bind(input.cycle_prev_weapon, Started, &ThisClass::cycle_prev_weapon);
    bind(input.unequip_weapon, Triggered, &ThisClass::unequip_weapon);
    bind(input.drop_weapon, Triggered, &ThisClass::drop_weapon);
    bind(input.reload_weapon, Started, &ThisClass::reload_weapon);

    // Interaction
    bind(input.toggle_mouse, Started, &ThisClass::toggle_mouse);
    bind(input.mouse_click, Started, &ThisClass::mouse_click);
    bind(input.interact, Started, &ThisClass::interact);
    bind(input.drop_waypoint, Started, &ThisClass::drop_waypoint);
    bind(input.possess_target, Started, &ThisClass::possess_target);

    // UI
    bind(input.toggle_in_game_menu, Started, &ThisClass::toggle_in_game_menu);
    bind(input.toggle_hud, Started, &ThisClass::toggle_hud);
}

// Movement
void AMyPlayerController::move(FInputActionValue const& value) {
    FORWARD_CALL_TO_CHARACTER(move, value);
}
void AMyPlayerController::start_jump() {
    FORWARD_CALL_TO_CHARACTER(Jump);
}
void AMyPlayerController::stop_jump() {
    FORWARD_CALL_TO_CHARACTER(StopJumping);
}
void AMyPlayerController::start_crouch() {
    FORWARD_CALL_TO_CHARACTER(start_crouch);
}
void AMyPlayerController::stop_crouch() {
    FORWARD_CALL_TO_CHARACTER(stop_crouch);
}
void AMyPlayerController::start_sprint() {
    FORWARD_CALL_TO_CHARACTER(start_sprint);
}
void AMyPlayerController::stop_sprint() {
    FORWARD_CALL_TO_CHARACTER(stop_sprint);
}
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
void AMyPlayerController::start_jetpack() {
    FORWARD_CALL_TO_CHARACTER(start_jetpack);
}
void AMyPlayerController::stop_jetpack() {
    FORWARD_CALL_TO_CHARACTER(stop_jetpack);
}

// Vision
void AMyPlayerController::look(FInputActionValue const& value) {
    RETURN_IF_NULLPTR(controlled_character);
    controlled_character->look(value);
}
void AMyPlayerController::cycle_camera() {
    FORWARD_CALL_TO_CHARACTER(cycle_camera);
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

// Torch
void AMyPlayerController::toggle_torch() {
    FORWARD_CALL_TO_CHARACTER(toggle_torch);
}
void AMyPlayerController::scroll_torch_cone(FInputActionValue const& value) {
    FORWARD_CALL_TO_CHARACTER(scroll_torch_cone, value);
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
void AMyPlayerController::drop_waypoint() {
    FORWARD_CALL_TO_CHARACTER(drop_waypoint);
}
void AMyPlayerController::possess_target() {
    log_log(TEXT("Trying to possess target"));

    FVector view_location;
    FRotator view_rotation;
    GetPlayerViewPoint(view_location, view_rotation);

    TRY_INIT_PTR(world, GetWorld());
    TRY_INIT_PTR(pawn, GetPawn());

    constexpr float raycast_distance{2000.f};
    FVector const direction{view_rotation.Vector()};
    FVector const start{pawn->GetActorLocation()};
    FVector const end{view_location + direction * raycast_distance};

    FHitResult hit_result;
    FCollisionQueryParams query_params;
    query_params.AddIgnoredActor(pawn);

    bool const hit{world->LineTraceSingleByChannel(hit_result, start, end, ECC_Pawn, query_params)};

    DrawDebugLine(world, start, end, FColor::Orange, false, 1.0f, 0U, 5.f);

    if (hit) {
        if (auto* character{Cast<AMyCharacter>(hit_result.GetActor())}) {
            if (character == pawn) {
                UE_LOG(LogSandboxController, Warning, TEXT("Possess raycast hit self."));
                return;
            }

            UE_LOG(LogSandboxController, Verbose, TEXT("Possess raycast hit new character."));
            Possess(character);
        }
    }
}

// UI
void AMyPlayerController::initialise_hud(UWorld& world, AMyCharacter& character) {
    using T = AMyPlayerController;
    RETURN_IF_NULLPTR(hud.main_widget_class);

    if (!hud.main_widget) {
        hud.main_widget = CreateWidget<UMainHUDWidget>(&world, hud.main_widget_class);
        hud.main_widget->AddToViewport();
    }

    if (auto* game_state{world.GetGameState<APlatformerGameState>()}) {
        game_state->on_coin_count_changed.AddUObject(this, &T::update_coin);
        update_coin(0);
    }

    TRY_INIT_PTR(pawn, GetPawn());
    TRY_INIT_PTR(health_component, pawn->FindComponentByClass<UHealthComponent>());

    health_component->on_health_percent_changed.AddUObject(this, &T::update_health);

    TRY_INIT_PTR(jetpack_component, pawn->FindComponentByClass<UJetpackComponent>());
    jetpack_component->on_fuel_changed.AddUObject(this, &T::update_fuel);
    jetpack_component->broadcast_fuel_state();

    TRY_INIT_PTR(weapon_component, pawn->FindComponentByClass<UPawnWeaponComponent>());
    weapon_component->on_weapon_ammo_changed.AddUObject(this, &T::update_current_ammo);
    weapon_component->on_weapon_equipped.AddUObject(this, &T::on_weapon_equipped);
    weapon_component->on_weapon_unequipped.AddUObject(this, &T::on_weapon_unequipped);
    weapon_component->on_weapon_reloaded.AddUObject(this, &T::on_weapon_reloaded);
    weapon_component->on_reserve_ammo_changed.AddUObject(this, &T::update_reserve_ammo);
    update_jump(0);

    TRY_INIT_PTR(scanner_component,
                 pawn->FindComponentByClass<UActorDescriptionScannerComponent>());
    scanner_component->on_description_update.BindUObject(this, &T::update_description);

    check(hud.main_widget->ammo_display);
    hud.main_widget->ammo_display->SetVisibility(ESlateVisibility::Collapsed);

    character.on_max_speed_changed.AddUObject(this, &AMyPlayerController::update_max_speed);
    character.on_jump_count_changed.BindUObject(this, &AMyPlayerController::update_jump);

    // Set up actor description scanner
    check(character.actor_description_scanner);
    character.actor_description_scanner->on_target_screen_bounds_update.BindUObject(
        this, &AMyPlayerController::update_target_screen_bounds);
    character.actor_description_scanner->on_target_screen_bounds_cleared.BindUObject(
        this, &AMyPlayerController::clear_target_screen_bounds);
}

void AMyPlayerController::toggle_in_game_menu() {
    constexpr auto logger{NestedLogger<"toggle_in_game_menu">()};
    logger.log_display(TEXT("Toggling menu."));

    TRY_INIT_PTR(game_viewport, GetWorld()->GetGameViewport());

    if (hud.is_in_game_menu_open) {
        RETURN_IF_NULLPTR(hud.umg_player_menu);
        hud.umg_player_menu->RemoveFromParent();
        set_game_input_mode();
        hud.is_in_game_menu_open = false;
    } else {
        if (!hud.umg_player_menu) {
            TRY_INIT_PTR(world, GetWorld());
            RETURN_IF_NULLPTR(controlled_character);
            construct_in_game_menu(*world, *controlled_character);
        }

        RETURN_IF_NULLPTR(hud.umg_player_menu);
        hud.umg_player_menu->refresh();
        hud.umg_player_menu->AddToViewport();
        set_mouse_input_mode();
        hud.is_in_game_menu_open = true;
    }
}
void AMyPlayerController::construct_in_game_menu(UWorld& world, AMyCharacter& character) {
    hud.umg_player_menu = CreateWidget<UInGamePlayerMenu>(&world, hud.umg_player_menu_class);
    RETURN_IF_NULLPTR(hud.umg_player_menu);
    hud.umg_player_menu->back_requested.AddUObject(this, &AMyPlayerController::toggle_in_game_menu);

    TRY_INIT_PTR(pawn, GetPawn());
    TRY_INIT_PTR(inventory_comp, pawn->FindComponentByClass<UInventoryComponent>());
    hud.umg_player_menu->set_inventory(*inventory_comp);
    hud.umg_player_menu->set_character(character);
}
void AMyPlayerController::set_game_input_mode() {
    auto input_mode{FInputModeGameOnly()};
    SetInputMode(input_mode);
    bShowMouseCursor = false;
    swap_input_mapping_context(input.cursor_mode_context, input.direct_mode_context);
}
void AMyPlayerController::set_mouse_input_mode() {
    auto input_mode{FInputModeGameAndUI()};
    SetInputMode(input_mode);
    bShowMouseCursor = true;
    swap_input_mapping_context(input.direct_mode_context, input.cursor_mode_context);
}
void AMyPlayerController::toggle_hud() {
    check(hud.main_widget);
    RETURN_IF_NULLPTR(hud.main_widget);

    auto const is_visible{hud.main_widget->IsVisible()};
    hud.main_widget->SetVisibility(is_visible ? ESlateVisibility::Collapsed
                                              : ESlateVisibility::SelfHitTestInvisible);
}
void AMyPlayerController::update_fuel(FJetpackState const& jetpack_state) {
    RETURN_IF_NULLPTR(hud.main_widget);
    RETURN_IF_NULLPTR(hud.main_widget->fuel_widget);

    hud.main_widget->fuel_widget->update(jetpack_state.fuel_remaining);
}
void AMyPlayerController::update_jump(int32 new_jump) {
    RETURN_IF_NULLPTR(hud.main_widget);
    RETURN_IF_NULLPTR(hud.main_widget->jump_widget);

    hud.main_widget->jump_widget->update(new_jump);
}
void AMyPlayerController::update_coin(int32 data) {
    if (hud.main_widget && hud.main_widget->coin_widget) {
        hud.main_widget->coin_widget->update(data);
    } else {
        UE_LOGFMT(LogTemp, Warning, "No coin widget.");
    }
}
void AMyPlayerController::update_health(FHealthData health_data) {
    RETURN_IF_NULLPTR(hud.main_widget);
    hud.main_widget->update_health(health_data);
}
void AMyPlayerController::update_max_speed(float data) {
    if (hud.main_widget && hud.main_widget->max_speed_widget) {
        hud.main_widget->max_speed_widget->update(data);
    }
}
void AMyPlayerController::on_weapon_reloaded(FAmmoData weapon_ammo, FAmmoData reserve_ammo) {
    RETURN_IF_NULLPTR(hud.main_widget);
    RETURN_IF_NULLPTR(hud.main_widget->ammo_display);

    hud.main_widget->ammo_display->set_current_ammo(weapon_ammo);
    hud.main_widget->ammo_display->set_reserve_ammo(reserve_ammo);
}
void AMyPlayerController::update_current_ammo(FAmmoData current_ammo) {
    RETURN_IF_NULLPTR(hud.main_widget);
    RETURN_IF_NULLPTR(hud.main_widget->ammo_display);

    hud.main_widget->ammo_display->set_current_ammo(current_ammo);
}
void AMyPlayerController::update_reserve_ammo(FAmmoData ammo) {
    RETURN_IF_NULLPTR(hud.main_widget);
    RETURN_IF_NULLPTR(hud.main_widget->ammo_display);

    hud.main_widget->ammo_display->set_reserve_ammo(ammo);
}
void AMyPlayerController::on_weapon_equipped(FAmmoData weapon_ammo,
                                             FAmmoData max_ammo,
                                             FAmmoData reserve_ammo) {
    RETURN_IF_NULLPTR(hud.main_widget);
    RETURN_IF_NULLPTR(hud.main_widget->ammo_display);

    hud.main_widget->ammo_display->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    hud.main_widget->ammo_display->set_max_ammo(max_ammo);
    hud.main_widget->ammo_display->set_current_ammo(weapon_ammo);
    hud.main_widget->ammo_display->set_reserve_ammo(reserve_ammo);
}
void AMyPlayerController::on_weapon_unequipped() {
    RETURN_IF_NULLPTR(hud.main_widget);
    RETURN_IF_NULLPTR(hud.main_widget->ammo_display);

    hud.main_widget->ammo_display->SetVisibility(ESlateVisibility::Collapsed);
}
void AMyPlayerController::update_description(FText const& text) {
    RETURN_IF_NULLPTR(hud.main_widget);
    RETURN_IF_NULLPTR(hud.main_widget->item_description_widget);
    hud.main_widget->item_description_widget->set_description(text);
}
void AMyPlayerController::update_target_screen_bounds(FActorCorners const& corners) {
    RETURN_IF_NULLPTR(hud.main_widget);
    RETURN_IF_NULLPTR(hud.main_widget->target_overlay);

    cache.outline_corners = corners;
    tracking_target_outline = true;

    hud.main_widget->target_overlay->update_target_screen_bounds(*this, corners);
}
void AMyPlayerController::clear_target_screen_bounds() {
    RETURN_IF_NULLPTR(hud.main_widget);
    RETURN_IF_NULLPTR(hud.main_widget->target_overlay);

    tracking_target_outline = false;

    hud.main_widget->target_overlay->hide();
}

// Private
// -------------------

// Input
void AMyPlayerController::perform_description_scan() {
    constexpr auto logger{NestedLogger<"perform_description_scan">()};

    RETURN_IF_NULLPTR(controlled_character);
    RETURN_IF_NULLPTR(controlled_character->actor_description_scanner);

    FVector view_location;
    FRotator view_rotation;
    GetPlayerViewPoint(view_location, view_rotation);

    controlled_character->actor_description_scanner->perform_raycast(
        *this, view_location, view_rotation);
}

// Input
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
