#include "MyCharacter.h"
#include "Sandbox/MyHud.h"

AMyCharacter::AMyCharacter() {
    PrimaryActorTick.bCanEverTick = true;

    warp_component = CreateDefaultSubobject<UWarpComponent>(TEXT("WarpComponent"));
}

// Called when the game starts or when spawned
void AMyCharacter::BeginPlay() {
    Super::BeginPlay();

    check(GEngine != nullptr);

    torch_component = Cast<USpotLightComponent>(GetDefaultSubobjectByName(TEXT("torch")));

    jetpack_fuel = jetpack_fuel_max;
    jetpack_fuel_previous = jetpack_fuel;

    auto& char_movement{*GetCharacterMovement()};
    char_movement.MaxWalkSpeed = this->move_speed;
    char_movement.MaxAcceleration = this->acceleration;

    this->JumpMaxCount = 2;

    if (auto* pc{Cast<APlayerController>(this->Controller)}) {
        using EIPS = UEnhancedInputLocalPlayerSubsystem;
        auto const* local_player{pc->GetLocalPlayer()};

        if (auto* subsystem{ULocalPlayer::GetSubsystem<EIPS>(local_player)}) {
            subsystem->AddMappingContext(this->first_person_context, 0);
        }
    }
}

// Called every frame
void AMyCharacter::Tick(float DeltaTime) {
    Super::Tick(DeltaTime);

    if (is_jetpacking) {
        if (jetpack_fuel > 0.0f) {
            auto const jp_launch{FVector(0, 0, jetpack_force)};
            LaunchCharacter(jp_launch, false, true);
            jetpack_fuel -= jetpack_fuel_consumption_rate * DeltaTime;
            jetpack_fuel = FMath::Clamp(jetpack_fuel, 0.0f, jetpack_fuel_max);
        }
    } else {
        jetpack_fuel += jetpack_fuel_recharge_rate * DeltaTime;
        jetpack_fuel = FMath::Clamp(jetpack_fuel, 0.0f, jetpack_fuel_max);
    }

    if (auto* pc{Cast<APlayerController>(GetController())}) {
        if (auto* hud{Cast<AMyHUD>(pc->GetHUD())}) {
            hud->update_fuel(jetpack_fuel);
            hud->update_jump(JumpCurrentCount);
        } else {
            print_msg(TEXT("No HUD when updating HUD."));
        }
    } else {
        print_msg(TEXT("No PC when updating HUD."));
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
    auto const movement_value{value.Get<FVector2D>()};

    if (this->Controller) {
        auto const right{this->GetActorRightVector()};
        AddMovementInput(right, movement_value.X);

        auto const fwd{this->GetActorForwardVector()};
        AddMovementInput(fwd, movement_value.Y);
    }
}
void AMyCharacter::look(FInputActionValue const& value) {
    auto const look_axis_value{value.Get<FVector2D>()};

    if (Controller) {
        AddControllerYawInput(look_axis_value.X);
        AddControllerPitchInput(look_axis_value.Y);
    }
}
void AMyCharacter::start_jetpack(FInputActionValue const& value) {
    is_jetpacking = true;
}
void AMyCharacter::stop_jetpack(FInputActionValue const& value) {
    is_jetpacking = false;
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
void AMyCharacter::reset_torch() {
    if (!torch_component) {
        print_msg("No torch");
        return;
    }

    auto const fwd_rot{GetActorForwardVector().Rotation()};
    torch_component->SetWorldRotation(fwd_rot);
}
void AMyCharacter::toggle_torch() {
    if (!torch_component) {
        print_msg("No torch");
        return;
    }

    torch_on = !torch_on;
    torch_component->SetVisibility(torch_on);
}
