#include "MyCharacter.h"

// Sets default values
AMyCharacter::AMyCharacter() {
    // Set this character to call Tick() every frame.  You can turn this off to improve performance
    // if you don't need it.
    PrimaryActorTick.bCanEverTick = true;
}

void AMyCharacter::print_msg(FString const& msg) {
    auto const fmsg{FString::Printf(TEXT("%s: %s"), *this->GetName(), *msg)};
    GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, fmsg);
}

// Called when the game starts or when spawned
void AMyCharacter::BeginPlay() {
    Super::BeginPlay();

    check(GEngine != nullptr);

    jetpack_fuel = jetpack_fuel_max;
    jetpack_fuel_previous = jetpack_fuel;

    if (hud_widget_class) {
        hud_widget = CreateWidget<UFuelWidget>(GetWorld(), hud_widget_class);
        if (hud_widget) {
            hud_widget->AddToViewport();
            hud_widget->update_fuel(this->jetpack_fuel);
        }
    }

    auto& char_movement{*GetCharacterMovement()};
    char_movement.MaxWalkSpeed = this->move_speed;
    char_movement.MaxAcceleration = this->acceleration;

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

    if (!FMath::IsNearlyEqual(jetpack_fuel, jetpack_fuel_previous, 0.01f)) {
        hud_widget->update_fuel(this->jetpack_fuel);
        jetpack_fuel_previous = jetpack_fuel;
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
