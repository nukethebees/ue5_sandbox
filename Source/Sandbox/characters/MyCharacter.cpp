#include "MyCharacter.h"

#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sandbox/actor_components/InteractorComponent.h"
#include "Sandbox/actor_components/JetpackComponent.h"
#include "Sandbox/huds/MyHud.h"
#include "Sandbox/interfaces/Interactable.h"
#include "UObject/ScriptInterface.h"

AMyCharacter::AMyCharacter() {
    PrimaryActorTick.bCanEverTick = true;

    warp_component = CreateDefaultSubobject<UWarpComponent>(TEXT("WarpComponent"));
}

// Called when the game starts or when spawned
void AMyCharacter::BeginPlay() {
    Super::BeginPlay();

    check(GEngine != nullptr);

    is_forced_movement = false;

    first_person_camera_component = FindComponentByClass<UCameraComponent>();
    first_person_camera_component->bUsePawnControlRotation = true;

    jetpack_component = FindComponentByClass<UJetpackComponent>();

    torch_component = Cast<USpotLightComponent>(GetDefaultSubobjectByName(TEXT("torch")));
    torch_component->bCastVolumetricShadow = true;
    torch_component->VolumetricScatteringIntensity = 1.0f;
    torch_component->AttenuationRadius = 2000.0f;
    set_torch(false);

    auto& char_movement{*GetCharacterMovement()};
    char_movement.MaxWalkSpeed = this->move_speed;
    char_movement.MaxAcceleration = this->acceleration;
    OnMaxSpeedChanged.Broadcast(char_movement.MaxWalkSpeed);

    this->JumpMaxCount = 2;

    using PC = std::remove_pointer<decltype(player_controller)>::type;
    player_controller = Cast<PC>(this->Controller);

    if (player_controller) {
        using EIPS = UEnhancedInputLocalPlayerSubsystem;
        auto const* local_player{player_controller->GetLocalPlayer()};

        if (auto* subsystem{ULocalPlayer::GetSubsystem<EIPS>(local_player)}) {
            subsystem->AddMappingContext(this->first_person_context, 0);
        }

        using HUD = std::remove_pointer<decltype(hud)>::type;
        hud = Cast<HUD>(player_controller->GetHUD());

        if (hud) {
            hud->update_jump(JumpCurrentCount);
        }
    }
}

// Called every frame
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

// Called to bind functionality to input
void AMyCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) {
    if (auto* eic{CastChecked<UEnhancedInputComponent>(PlayerInputComponent)}) {
        eic->BindAction(this->move_action, ETriggerEvent::Triggered, this, &AMyCharacter::move);

        // Bind Jump Actions
        eic->BindAction(jump_action, ETriggerEvent::Started, this, &ACharacter::Jump);
        eic->BindAction(jump_action, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

        eic->BindAction(
            jetpack_action, ETriggerEvent::Triggered, this, &AMyCharacter::start_jetpack);
        eic->BindAction(
            jetpack_action, ETriggerEvent::Completed, this, &AMyCharacter::stop_jetpack);
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
    if (jetpack_component != nullptr) {
        jetpack_component->start_jetpack();
    }
}
void AMyCharacter::stop_jetpack(FInputActionValue const&) {
    if (jetpack_component != nullptr) {
        jetpack_component->stop_jetpack();
    }
}

void AMyCharacter::aim_torch(FVector const& world_location) {
    if (!torch_component) {
        print_msg("No torch");
        return;
    }

    auto const location{torch_component->GetComponentLocation()};
    auto const direction{(world_location - location).GetSafeNormal()};
    auto const look_at_rotation{direction.Rotation()};
    torch_component->SetWorldRotation(look_at_rotation);
}
void AMyCharacter::reset_torch_position() {
    if (!torch_component) {
        print_msg("No torch");
        return;
    }

    auto const fwd_rot{GetActorForwardVector().Rotation()};
    torch_component->SetWorldRotation(fwd_rot);
}
void AMyCharacter::toggle_torch() {
    set_torch(!torch_on);
}
void AMyCharacter::set_torch(bool on) {
    if (!torch_component) {
        print_msg("No torch");
        return;
    }

    torch_on = on;
    torch_component->SetVisibility(torch_on);
}

void AMyCharacter::handle_death() {
    if (auto const* world{GetWorld()}) {
        UGameplayStatics::OpenLevel(world, "MainMenu");
    }
}

void AMyCharacter::on_speed_changed(float new_speed) {
    OnMaxSpeedChanged.Broadcast(new_speed);
}
