#include "MyHUD.h"

#include "GameFramework/PlayerController.h"
#include "Sandbox/actor_components/HealthComponent.h"
#include "Sandbox/game_states/PlatformerGameState.h"
#include "UObject/ConstructorHelpers.h"

template <typename T>
using CFinder = ConstructorHelpers::FClassFinder<T>;

#define MAIN_WIDGET_NULL_CHECK()                                          \
    do {                                                                  \
        if (!main_widget) {                                               \
            UE_LOGFMT(LogTemp, Error, "AMyHUD: main_widget not present"); \
            return;                                                       \
        }                                                                 \
    } while (0)

void AMyHUD::BeginPlay() {
    Super::BeginPlay();

    if (!main_widget_class) {
        UE_LOG(LogTemp, Error, TEXT("MyHUD: No main_widget_class."));
        return;
    }

    if (!main_widget) {
        main_widget = CreateWidget<UMainHUDWidget>(GetWorld(), main_widget_class);
        if (!main_widget) {
            return;
        }
        main_widget->AddToViewport();
    }

    if (auto* game_state{GetWorld()->GetGameState<APlatformerGameState>()}) {
        game_state->on_coin_count_changed.AddDynamic(this, &AMyHUD::update_coin);
        update_coin(0);
    }

    auto* player_controller{GetOwningPlayerController()};
    if (!player_controller) {
        return;
    }

    auto player_pawn{player_controller->GetPawn()};
    if (!player_pawn) {
        return;
    }

    auto* health_component{player_pawn->FindComponentByClass<UHealthComponent>()};
    if (!health_component) {
        UE_LOG(LogTemp, Warning, TEXT("AMyHUD: HealthComponent not found on player pawn."));
        return;
    }

    health_component->on_health_percent_changed.AddDynamic(this, &AMyHUD::update_health);
}

void AMyHUD::update_fuel(float new_fuel) {
    MAIN_WIDGET_NULL_CHECK();
    main_widget->update_fuel(new_fuel);
}
void AMyHUD::update_jump(int32 new_jump) {
    MAIN_WIDGET_NULL_CHECK();
    main_widget->update_jump(new_jump);
}
void AMyHUD::update_health(FHealthData health_data) {
    MAIN_WIDGET_NULL_CHECK();
    main_widget->update_health(health_data);
}
