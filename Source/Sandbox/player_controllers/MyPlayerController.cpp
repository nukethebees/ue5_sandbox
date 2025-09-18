// Fill out your copyright notice in the Description page of Project Settings.

#include "MyPlayerController.h"

#include "Sandbox/actors/TalkingPillar.h"
#include "Sandbox/huds/MyHud.h"

void AMyPlayerController::BeginPlay() {
    Super::BeginPlay();

    bEnableClickEvents = true;
    bEnableMouseOverEvents = false;

    set_game_input_mode();

    if (auto* local_player{GetLocalPlayer()}) {
        using SS = UEnhancedInputLocalPlayerSubsystem;
        if (auto* subsystem{ULocalPlayer::GetSubsystem<SS>(local_player)}) {
            subsystem->AddMappingContext(default_mapping_context.LoadSynchronous(), 0);
        }
    } else {
        UE_LOGFMT(LogTemp, Warning, "AMyPlayerController: Could not get local player.");
    }

    if (auto* character{Cast<AMyCharacter>(GetPawn())}) {
        if (auto* hud{Cast<AMyHUD>(GetHUD())}) {
            character->OnMaxSpeedChanged.AddDynamic(hud, &AMyHUD::update_max_speed);
        }
    } else {
        UE_LOGFMT(LogTemp, Warning, "AMyPlayerController: Could not cast to AMyCharacter.");
    }
}
void AMyPlayerController::OnPossess(APawn* InPawn) {
    Super::OnPossess(InPawn);

    controlled_character = Cast<AMyCharacter>(InPawn);
    if (controlled_character) {
        controlled_character->my_player_controller = this;
    } else {
        UE_LOGFMT(LogTemp, Warning, "AMyPlayerController: Could not possess a AMyCharacter.");
    }
}
void AMyPlayerController::Tick(float DeltaSeconds) {
    Super::Tick(DeltaSeconds);

    if (!controlled_character) {
        print_msg(TEXT("No controlled character"));
        return;
    }

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
        eic->BindAction(look_action.LoadSynchronous(),
                        ETriggerEvent::Triggered,
                        this,
                        &AMyPlayerController::look);
        eic->BindAction(toggle_mouse_action.LoadSynchronous(),
                        ETriggerEvent::Started,
                        this,
                        &AMyPlayerController::toggle_mouse);
        eic->BindAction(mouse_click_action.LoadSynchronous(),
                        ETriggerEvent::Started,
                        this,
                        &AMyPlayerController::mouse_click);

        eic->BindAction(toggle_torch_action.LoadSynchronous(),
                        ETriggerEvent::Started,
                        this,
                        &AMyPlayerController::toggle_torch);

        eic->BindAction(scroll_torch_cone_action.LoadSynchronous(),
                        ETriggerEvent::Triggered,
                        this,
                        &AMyPlayerController::scroll_torch_cone);

        eic->BindAction(warp_to_cursor_action.LoadSynchronous(),
                        ETriggerEvent::Completed,
                        this,
                        &AMyPlayerController::warp_to_cursor);
    } else {
        print_msg(TEXT("AMyPlayerController: Did not get the UEnhancedInputComponent."));
    }
}

void AMyPlayerController::look(FInputActionValue const& value) {
    if (controlled_character) {
        controlled_character->look(value);
    }
}
void AMyPlayerController::toggle_mouse(FInputActionValue const& value) {
    auto const mouse_value{value.Get<bool>()};

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
    print_msg("Shooting a ray.");

    FVector world_location;
    FVector world_direction;
    if (DeprojectMousePositionToWorld(world_location, world_direction)) {
        auto const end{world_location + (world_direction * 10000)};
        FHitResult hit_result;
        GetWorld()->LineTraceSingleByChannel(hit_result, world_location, end, ECC_Visibility);

        DrawDebugLine(GetWorld(),
                      world_location - FVector(0.0f, 0.0f, 10.0f),
                      end,
                      FColor::Green,
                      false, // Not persistent
                      2.0f,  // Duration in seconds
                      0,     // Depth priority
                      1.0f   // Thickness
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
void AMyPlayerController::toggle_torch(FInputActionValue const& value) {
    if (controlled_character) {
        controlled_character->toggle_torch();
    }
}
void AMyPlayerController::scroll_torch_cone(FInputActionValue const& value) {
    if (!controlled_character || !controlled_character->torch_component) {
        print_msg("No char or torch");
    }

    auto const scroll_delta{value.Get<float>()};
    auto* const torch{controlled_character->torch_component};

    static constexpr auto min_cone{5.0f};
    static constexpr auto max_cone{15.0f};

    auto const new_outer{
        FMath::Clamp(torch->OuterConeAngle + scroll_delta * 2.0f, min_cone, max_cone)};
    torch->SetOuterConeAngle(new_outer);

    auto const new_inner{FMath::Clamp(new_outer * 0.7f, 2.0f, new_outer)};
    torch->SetInnerConeAngle(new_inner);
}
void AMyPlayerController::warp_to_cursor(FInputActionValue const& value) {
    if (!controlled_character) {
        print_msg(TEXT("No char to warp"));
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
            controlled_character->warp_component->warp_to_location(controlled_character,
                                                                   hit.Location);
        }
    }
}

void AMyPlayerController::set_game_input_mode() {
    auto input_mode{FInputModeGameOnly()};
    SetInputMode(input_mode);
    bShowMouseCursor = false;
    controlled_character->reset_torch_position();
}
void AMyPlayerController::set_mouse_input_mode() {
    auto input_mode{FInputModeGameAndUI()};
    SetInputMode(input_mode);
    bShowMouseCursor = true;
}
