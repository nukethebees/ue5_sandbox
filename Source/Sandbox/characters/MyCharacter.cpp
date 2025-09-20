#include "MyCharacter.h"

#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sandbox/actor_components/HealthComponent.h"
#include "Sandbox/actor_components/InteractorComponent.h"
#include "Sandbox/actor_components/JetpackComponent.h"
#include "Sandbox/actor_components/SpeedBoostComponent.h"
#include "Sandbox/actor_components/CoinCollectorActorComponent.h"
#include "Sandbox/huds/MyHud.h"
#include "Sandbox/interfaces/Interactable.h"
#include "UObject/ScriptInterface.h"

AMyCharacter::AMyCharacter() {
    PrimaryActorTick.bCanEverTick = true;

    // Initialize arrays
    cameras.Init(nullptr, camera_count);
    spring_arms.Init(nullptr, spring_arm_count);

    // Create cameras using configurations
    int32 spring_arm_index{0};
    for (auto const& config : ml::AMyCharacter::camera_configs) {
        auto& cc{cameras[config.camera_index]};
        cc = CreateDefaultSubobject<UCameraComponent>(ANSI_TO_TCHAR(config.component_name));
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

    coins = CreateDefaultSubobject<UCoinCollectorActorComponent>(TEXT("Coins"));
    interactor = CreateDefaultSubobject<UInteractorComponent>(TEXT("Interactor"));
    health = CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));
    speed_boost = CreateDefaultSubobject<USpeedBoostComponent>(TEXT("SpeedBoost"));
    jetpack = CreateDefaultSubobject<UJetpackComponent>(TEXT("Jetpack"));
    torch = CreateDefaultSubobject<USpotLightComponent>(TEXT("Torch"));
    torch->SetupAttachment(RootComponent);
    warp = CreateDefaultSubobject<UWarpComponent>(TEXT("WarpComponent"));
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
    char_movement.MaxWalkSpeed = this->move_speed;
    char_movement.MaxAcceleration = this->acceleration;
    OnMaxSpeedChanged.Broadcast(char_movement.MaxWalkSpeed);

    reset_max_jump_count();

    using PC = std::remove_pointer<decltype(player_controller)>::type;
    player_controller = Cast<PC>(this->Controller);

    if (player_controller) {
        using EIPS = UEnhancedInputLocalPlayerSubsystem;
        auto const* local_player{player_controller->GetLocalPlayer()};

        if (auto* subsystem{ULocalPlayer::GetSubsystem<EIPS>(local_player)}) {
            subsystem->AddMappingContext(this->input_actions.first_person_context, 0);
        }

        using HUD = std::remove_pointer<decltype(hud)>::type;
        hud = Cast<HUD>(player_controller->GetHUD());

        if (hud) {
            hud->update_jump(JumpCurrentCount);
        }
    }
}

void AMyCharacter::Tick(float DeltaTime) {
    Super::Tick(DeltaTime);

    if (cached_jump_count != JumpCurrentCount) {
        if (player_controller) {
            if (hud) {
                hud->update_jump(JumpCurrentCount);
            } else {
                print_msg(TEXT("No HUD when updating HUD."));
            }
        } else {
            print_msg(TEXT("No PC when updating HUD."));
        }

        cached_jump_count = JumpCurrentCount;
    }
}

void AMyCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) {
    if (auto* eic{CastChecked<UEnhancedInputComponent>(PlayerInputComponent)}) {
        eic->BindAction(
            this->input_actions.move_action, ETriggerEvent::Triggered, this, &AMyCharacter::move);

        // Bind Jump Actions
        eic->BindAction(input_actions.jump_action, ETriggerEvent::Started, this, &ACharacter::Jump);
        eic->BindAction(
            input_actions.jump_action, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

        eic->BindAction(input_actions.jetpack_action,
                        ETriggerEvent::Triggered,
                        this,
                        &AMyCharacter::start_jetpack);
        eic->BindAction(input_actions.jetpack_action,
                        ETriggerEvent::Completed,
                        this,
                        &AMyCharacter::stop_jetpack);

        eic->BindAction(input_actions.cycle_camera_action,
                        ETriggerEvent::Started,
                        this,
                        &AMyCharacter::cycle_camera);
    }
}

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
void AMyCharacter::look(FInputActionValue const& value) {
    auto const look_axis_value{value.Get<FVector2D>()};

    if (Controller) {
        AddControllerYawInput(look_axis_value.X);
        AddControllerPitchInput(look_axis_value.Y);
    }
}
void AMyCharacter::start_jetpack(FInputActionValue const&) {
    if (jetpack != nullptr) {
        jetpack->start_jetpack();
    }
}
void AMyCharacter::stop_jetpack(FInputActionValue const&) {
    if (jetpack != nullptr) {
        jetpack->stop_jetpack();
    }
}
void AMyCharacter::cycle_camera(FInputActionValue const&) {
    camera_mode = get_next(camera_mode);
    change_camera_to(camera_mode);
}
UCameraComponent const* AMyCharacter::get_active_camera() const {
    auto const camera_index{static_cast<int32>(camera_mode)};
    if (cameras.IsValidIndex(camera_index)) {
        return cameras[camera_index];
    }

    log_warning(TEXT("Invalid camera mode. Returning first person."));
    return cameras[static_cast<int32>(ECharacterCameraMode::FirstPerson)];
}

void AMyCharacter::aim_torch(FVector const& world_location) {
    if (!torch) {
        print_msg("No torch");
        return;
    }

    auto const location{torch->GetComponentLocation()};
    auto const direction{(world_location - location).GetSafeNormal()};
    auto const look_at_rotation{direction.Rotation()};
    torch->SetWorldRotation(look_at_rotation);
}
void AMyCharacter::reset_torch_position() {
    if (!torch) {
        print_msg("No torch");
        return;
    }

    auto const fwd_rot{GetActorForwardVector().Rotation()};
    torch->SetWorldRotation(fwd_rot);
}
void AMyCharacter::toggle_torch() {
    set_torch(!torch_on);
}
void AMyCharacter::set_torch(bool on) {
    if (!torch) {
        print_msg("No torch");
        return;
    }

    torch_on = on;
    torch->SetVisibility(torch_on);
}

void AMyCharacter::handle_death() {
    if (auto const* world{GetWorld()}) {
        UGameplayStatics::OpenLevel(world, "MainMenu");
    }
}

void AMyCharacter::on_speed_changed(float new_speed) {
    OnMaxSpeedChanged.Broadcast(new_speed);
}
void AMyCharacter::disable_all_cameras() {
    for (auto* camera : cameras) {
        if (camera) {
            camera->SetActive(false);
        }
    }
}
void AMyCharacter::change_camera_to(ECharacterCameraMode mode) {
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
void AMyCharacter::reset_max_jump_count() {
    this->JumpMaxCount = default_max_jump_count;
}
void AMyCharacter::increase_max_jump_count(int32 jumps) {
    this->JumpMaxCount += jumps;
}
