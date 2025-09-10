#include "Sandbox/actors/HealthStationActor.h"

#include "Components/WidgetComponent.h"
#include "Sandbox/actor_components/HealthStationComponent.h"
#include "Sandbox/widgets/HealthStationWidget.h"

AHealthStationActor::AHealthStationActor() {
    PrimaryActorTick.bCanEverTick = false;

    health_station_component =
        CreateDefaultSubobject<UHealthStationComponent>(TEXT("HealthStationComponent"));

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

    widget_pivot = CreateDefaultSubobject<USceneComponent>(TEXT("WidgetPivot"));
    widget_pivot->SetupAttachment(RootComponent);

    widget_component = CreateDefaultSubobject<UWidgetComponent>(TEXT("WidgetComponent"));
    widget_component->SetupAttachment(widget_pivot);
}
void AHealthStationActor::BeginPlay() {
    Super::BeginPlay();

    if (health_station_component) {
        health_station_component->on_station_state_changed.AddDynamic(
            this, &AHealthStationActor::handle_station_state_changed);
        health_station_component->broadcast_state();
    }
}

void AHealthStationActor::handle_station_state_changed(FStationStateData const& state_data) {
    if (!widget_component) {
        UE_LOGFMT(LogTemp, Warning, "No widget component.");
        return;
    }

    if (auto* widget{Cast<UHealthStationWidget>(widget_component->GetUserWidgetObject())}) {
        widget->update(state_data);
    }
}
