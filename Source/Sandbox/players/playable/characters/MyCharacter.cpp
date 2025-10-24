#include "MyCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SpotLightComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ScriptInterface.h"

#include "Sandbox/combat/weapons/actor_components/PawnWeaponComponent.h"
#include "Sandbox/constants/collision_channels.h"
#include "Sandbox/health/actor_components/HealthComponent.h"
#include "Sandbox/inventory/actor_components/InventoryComponent.h"
#include "Sandbox/players/common/actor_components/ActorDescriptionScannerComponent.h"
#include "Sandbox/players/playable/actor_components/CoinCollectorActorComponent.h"
#include "Sandbox/players/playable/actor_components/InteractorComponent.h"
#include "Sandbox/players/playable/actor_components/JetpackComponent.h"
#include "Sandbox/players/playable/actor_components/SpeedBoostComponent.h"
#include "Sandbox/players/playable/actor_components/WarpComponent.h"
#include "Sandbox/ui/hud/huds/MyHud.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

AMyCharacter::AMyCharacter()
    : coins(CreateDefaultSubobject<UCoinCollectorActorComponent>(TEXT("Coins")))
    , interactor(CreateDefaultSubobject<UInteractorComponent>(TEXT("Interactor")))
    , health(CreateDefaultSubobject<UHealthComponent>(TEXT("Health")))
    , speed_boost(CreateDefaultSubobject<USpeedBoostComponent>(TEXT("SpeedBoost")))
    , jetpack(CreateDefaultSubobject<UJetpackComponent>(TEXT("Jetpack")))
    , torch(CreateDefaultSubobject<USpotLightComponent>(TEXT("Torch")))
    , warp(CreateDefaultSubobject<UWarpComponent>(TEXT("WarpComponent")))
    , weapon_component(CreateDefaultSubobject<UPawnWeaponComponent>(TEXT("PawnWeapon")))
    , inventory(CreateDefaultSubobject<UInventoryComponent>(TEXT("Inventory")))
    , weapon_attach_point(CreateDefaultSubobject<UArrowComponent>(TEXT("WeaponAttachPoint")))
    , actor_description_scanner(CreateDefaultSubobject<UActorDescriptionScannerComponent>(
          TEXT("ActorDescriptionScanner"))) {
    PrimaryActorTick.bCanEverTick = false;

    // Initialise arrays
    cameras.Init(nullptr, camera_count);
    spring_arms.Init(nullptr, spring_arm_count);

    // Create cameras using configurations
    int32 spring_arm_index{0};
    for (auto const& config : ml::AMyCharacter::camera_configs) {
        auto& cc{cameras[config.camera_index]};
        cc = CreateDefaultSubobject<UCameraComponent>(
            StringCast<TCHAR>(config.component_name).Get());
        cc->bUsePawnControlRotation = config.use_pawn_control_rotation;

        if (config.needs_spring_arm) {
            auto& arm{spring_arms[spring_arm_index]};
            arm = CreateDefaultSubobject<USpringArmComponent>(
                *FString::Printf(TEXT("SpringArm_%s"), *cc->GetName()));
            arm->SetupAttachment(RootComponent);
            cc->SetupAttachment(arm);

            ++spring_arm_index;
        } else {
            cc->SetupAttachment(RootComponent);
        }
    }

    torch->SetupAttachment(RootComponent);
    weapon_attach_point->SetupAttachment(cameras[0]);

    TRY_INIT_PTR(capsule, GetCapsuleComponent());

    capsule->SetCollisionResponseToChannel(ml::collision::interaction,
                                           ECollisionResponse::ECR_Ignore);
}

void AMyCharacter::BeginPlay() {
    Super::BeginPlay();

    check(GEngine != nullptr);

    is_forced_movement = false;

    if (torch) {
        torch->bCastVolumetricShadow = true;
        torch->VolumetricScatteringIntensity = 1.0f;
        torch->AttenuationRadius = 2000.0f;
        set_torch(false);
    }

    auto& char_movement{*GetCharacterMovement()};
    movement.is_running = false; // Initialize movement state
    update_speed();              // Apply initial speed
    char_movement.MaxAcceleration = movement.acceleration;

    reset_max_jump_count();

    using PC = std::remove_pointer<decltype(player_controller)>::type;
    player_controller = Cast<PC>(this->Controller);

    if (player_controller) {
        using EIPS = UEnhancedInputLocalPlayerSubsystem;
        auto const* local_player{player_controller->GetLocalPlayer()};

        if (auto* subsystem{ULocalPlayer::GetSubsystem<EIPS>(local_player)}) {
            subsystem->AddMappingContext(this->input_actions.character_context, 0);
        }

        using HUD = std::remove_pointer<decltype(hud)>::type;
        hud = Cast<HUD>(player_controller->GetHUD());

        if (hud) {
            hud->update_jump(JumpCurrentCount);
        }
    }

    RETURN_IF_NULLPTR(weapon_attach_point);
    RETURN_IF_NULLPTR(weapon_component);

    weapon_component->set_attach_location(*weapon_attach_point);
}

void AMyCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) {
    if (auto* eic{CastChecked<UEnhancedInputComponent>(PlayerInputComponent)}) {
        using enum ETriggerEvent;
        auto bind{make_input_binder(eic)};

        // Movement
        bind(input_actions.move, Triggered, &AMyCharacter::move);

        bind(input_actions.jump, Started, &ACharacter::Jump);
        bind(input_actions.jump, Completed, &ACharacter::StopJumping);

        bind(input_actions.crouch, Started, &AMyCharacter::start_crouch);
        bind(input_actions.crouch, Completed, &AMyCharacter::stop_crouch);

        bind(input_actions.sprint, Started, &AMyCharacter::start_sprint);
        bind(input_actions.sprint, Completed, &AMyCharacter::stop_sprint);

        bind(input_actions.jetpack, Triggered, &AMyCharacter::start_jetpack);
        bind(input_actions.jetpack, Completed, &AMyCharacter::stop_jetpack);

        // Vision
        bind(input_actions.cycle_camera, Started, &AMyCharacter::cycle_camera);

        // Torch
        bind(input_actions.toggle_torch, Started, &AMyCharacter::toggle_torch);
        bind(input_actions.scroll_torch_cone, Triggered, &AMyCharacter::scroll_torch_cone);
    }
}

UCameraComponent const* AMyCharacter::get_active_camera() const {
    auto const camera_index{static_cast<int32>(camera_mode)};
    if (cameras.IsValidIndex(camera_index)) {
        return cameras[camera_index];
    }

    log_warning(TEXT("Invalid camera mode. Returning first person."));
    return cameras[static_cast<int32>(ECharacterCameraMode::FirstPerson)];
}

// Movement
void AMyCharacter::move(FInputActionValue const& value) {
    auto const movement_value{value.Get<FVector>()};

    if (this->Controller) {
        if (!is_forced_movement) {
            AddMovementInput(GetActorRightVector(), movement_value.X);
            AddMovementInput(GetActorForwardVector(), movement_value.Y);
        }

        // Allow slow vertical motion when in the forced movement state
        // to allow the characters to get unstuck if needed
        auto const z_scale{is_forced_movement ? 0.2f : 1.0f};
        AddMovementInput(GetActorUpVector(), movement_value.Z * z_scale);
    }
}
void AMyCharacter::start_crouch() {
    Crouch();
}
void AMyCharacter::stop_crouch() {
    UnCrouch();
}
void AMyCharacter::start_sprint() {
    movement.is_running = true;
    update_speed();
}
void AMyCharacter::stop_sprint() {
    movement.is_running = false;
    update_speed();
}
void AMyCharacter::start_jetpack() {
    if (jetpack != nullptr) {
        jetpack->start_jetpack();
    }
}
void AMyCharacter::stop_jetpack() {
    if (jetpack != nullptr) {
        jetpack->stop_jetpack();
    }
}

// Vision
void AMyCharacter::look(FInputActionValue const& value) {
    auto const look_axis_value{value.Get<FVector2D>()};

    if (Controller) {
        AddControllerYawInput(look_axis_value.X);
        AddControllerPitchInput(look_axis_value.Y);
    }
}
void AMyCharacter::cycle_camera() {
    change_camera_to(get_next(camera_mode));
}

// Combat
void AMyCharacter::attack_started() {
    RETURN_IF_NULLPTR(weapon_component);
    weapon_component->start_firing();
}
void AMyCharacter::attack_continued(float delta_time) {
    RETURN_IF_NULLPTR(weapon_component);
    weapon_component->sustain_firing(0.1f);
}
void AMyCharacter::attack_ended() {
    RETURN_IF_NULLPTR(weapon_component);
    weapon_component->stop_firing();
}

// Inventory
void AMyCharacter::cycle_next_weapon() {
    RETURN_IF_NULLPTR(weapon_component);
    weapon_component->cycle_next_weapon();
}
void AMyCharacter::cycle_prev_weapon() {
    RETURN_IF_NULLPTR(weapon_component);
    weapon_component->cycle_prev_weapon();
}
void AMyCharacter::unequip_weapon() {
    RETURN_IF_NULLPTR(inventory);
    RETURN_IF_NULLPTR(weapon_component);
    log_display(TEXT("unequip_weapon"));

    weapon_component->unequip_weapon();
}
void AMyCharacter::drop_weapon() {
    RETURN_IF_NULLPTR(inventory);
    log_display(TEXT("drop_weapon"));
}
void AMyCharacter::reload_weapon() {
    RETURN_IF_NULLPTR(weapon_component);
    log_display(TEXT("reload_weapon"));
    weapon_component->reload();
}

// Torch
void AMyCharacter::aim_torch(FVector const& world_location) {
    RETURN_IF_NULLPTR(torch);

    auto const location{torch->GetComponentLocation()};
    auto const direction{(world_location - location).GetSafeNormal()};
    auto const look_at_rotation{direction.Rotation()};
    torch->SetWorldRotation(look_at_rotation);
}
void AMyCharacter::reset_torch_position() {
    RETURN_IF_NULLPTR(torch);

    auto const fwd_rot{GetActorForwardVector().Rotation()};
    torch->SetWorldRotation(fwd_rot);
}
void AMyCharacter::toggle_torch() {
    set_torch(!torch_on);
}
void AMyCharacter::set_torch(bool on) {
    RETURN_IF_NULLPTR(torch);

    torch_on = on;
    torch->SetVisibility(torch_on);
}
void AMyCharacter::scroll_torch_cone(FInputActionValue const& value) {
    if (!torch) {
        log_warning(TEXT("No  torch"));
    }

    auto const scroll_delta{value.Get<float>()};

    static constexpr auto min_cone{5.0f};
    static constexpr auto max_cone{15.0f};

    auto const new_outer{
        FMath::Clamp(torch->OuterConeAngle + scroll_delta * 2.0f, min_cone, max_cone)};
    torch->SetOuterConeAngle(new_outer);

    auto const new_inner{FMath::Clamp(new_outer * 0.7f, 2.0f, new_outer)};
    torch->SetInnerConeAngle(new_inner);
}

// Jumping
void AMyCharacter::reset_max_jump_count() {
    this->JumpMaxCount = default_max_jump_count;
}
void AMyCharacter::increase_max_jump_count(int32 jumps) {
    this->JumpMaxCount += jumps;
}
void AMyCharacter::OnJumped_Implementation() {
    Super::OnJumped_Implementation();
    if (hud) {
        hud->update_jump(JumpCurrentCount);
    }
}
void AMyCharacter::Landed(FHitResult const& Hit) {
    Super::Landed(Hit);
    if (hud) {
        hud->update_jump(0);
    }
}

// Interaction
void AMyCharacter::interact(FVector sweep_origin, FRotator sweep_direction) {
    RETURN_IF_NULLPTR(interactor);
    RETURN_IF_NULLPTR(weapon_attach_point);

    interactor->try_interact(sweep_origin, sweep_direction);
}

void AMyCharacter::handle_death() {
    if (auto const* world{GetWorld()}) {
        UGameplayStatics::OpenLevel(world, "MainMenu");
    }
}

FGenericTeamId AMyCharacter::GetGenericTeamId() const {
    return FGenericTeamId(static_cast<uint8>(team_id));
}

void AMyCharacter::SetGenericTeamId(FGenericTeamId const& TeamID) {
    team_id = static_cast<ETeamID>(TeamID.GetId());
}

void AMyCharacter::on_speed_changed(float new_speed) {
    OnMaxSpeedChanged.Broadcast(new_speed);
}

void AMyCharacter::set_movement_multiplier(float multiplier) {
    movement.boost_scale_factor = multiplier;
    update_speed();
}
void AMyCharacter::disable_all_cameras() {
    for (auto* camera : cameras) {
        if (camera) {
            camera->SetActive(false);
        }
    }
}
void AMyCharacter::change_camera_to(ECharacterCameraMode mode) {
    camera_mode = mode;

    log_verbose(TEXT("Changing camera mode to %s."), *UEnum::GetValueAsString(camera_mode));

    disable_all_cameras();

    constexpr auto default_index{std::to_underlying(ECharacterCameraMode::FirstPerson)};
    auto const camera_index{std::to_underlying(camera_mode)};

    if (cameras.IsValidIndex(camera_index) && cameras[camera_index]) {
        cameras[camera_index]->SetActive(true);
    } else {
        log_warning(TEXT("Invalid camera mode. Switching to first person."));
        if (cameras.IsValidIndex(default_index)) {
            cameras[default_index]->SetActive(true);
        }
    }
}

void AMyCharacter::update_speed() {
    auto const base_speed{movement.is_running ? movement.run_speed : movement.walk_speed};
    set_speed(base_speed);
}

void AMyCharacter::set_speed(float new_speed) {
    if (auto* char_movement{GetCharacterMovement()}) {
        auto const boosted_speed{new_speed * movement.boost_scale_factor};
        char_movement->MaxWalkSpeed = boosted_speed;
        OnMaxSpeedChanged.Broadcast(char_movement->MaxWalkSpeed);
    } else {
        log_warning(TEXT("Couldn't get character movement component."));
    }
}
