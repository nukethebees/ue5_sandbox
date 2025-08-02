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

    if (hud_widget_class) {
        auto* hud_widget = CreateWidget<UUserWidget>(GetWorld(), hud_widget_class);
        if (hud_widget) {
            hud_widget->AddToViewport();
        }
    }

    auto& char_movement{*GetCharacterMovement()};
    char_movement.MaxWalkSpeed = this->move_speed;
    char_movement.MaxAcceleration = this->acceleration;

    if (auto* pc{Cast<APlayerController>(this->Controller)}) {
        using EIPS = UEnhancedInputLocalPlayerSubsystem;
        auto local_player{pc->GetLocalPlayer()};

        if (auto* subsystem{ULocalPlayer::GetSubsystem<EIPS>(local_player)}) {
            subsystem->AddMappingContext(this->first_person_context, 0);
        }
    }
}

// Called every frame
void AMyCharacter::Tick(float DeltaTime) {
    Super::Tick(DeltaTime);
}

// Called to bind functionality to input
void AMyCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) {
    if (auto* eic{CastChecked<UEnhancedInputComponent>(PlayerInputComponent)}) {
        eic->BindAction(this->move_action, ETriggerEvent::Triggered, this, &AMyCharacter::move);

        // Bind Jump Actions
        eic->BindAction(jump_action, ETriggerEvent::Started, this, &ACharacter::Jump);
        eic->BindAction(jump_action, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
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
