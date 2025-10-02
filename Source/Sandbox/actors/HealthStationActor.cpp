#include "Sandbox/actors/HealthStationActor.h"

#include "Components/WidgetComponent.h"
#include "Sandbox/subsystems/world/TriggerSubsystem.h"
#include "Sandbox/widgets/HealthStationWidget.h"

AHealthStationActor::AHealthStationActor() {
    PrimaryActorTick.bCanEverTick = false;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

    widget_pivot = CreateDefaultSubobject<USceneComponent>(TEXT("WidgetPivot"));
    widget_pivot->SetupAttachment(RootComponent);

    widget_component = CreateDefaultSubobject<UWidgetComponent>(TEXT("WidgetComponent"));
    widget_component->SetupAttachment(widget_pivot);
}

void AHealthStationActor::BeginPlay() {
    Super::BeginPlay();

    health_station_payload.reset_capacity();

    health_station_payload.on_station_state_changed.AddDynamic(
        this, &AHealthStationActor::handle_station_state_changed);
    health_station_payload.broadcast_state();

    auto* subsystem{GetWorld()->GetSubsystem<UTriggerSubsystem>()};
    if (subsystem) {
        if (auto const id{
                subsystem->register_triggerable(*this, std::move(health_station_payload))}) {
            my_trigger_id = *id;
        }
    }
}

void AHealthStationActor::EndPlay(EEndPlayReason::Type reason) {
    if (auto* subsystem{GetWorld()->GetSubsystem<UTriggerSubsystem>()}) {
        subsystem->deregister_triggerable(*this);
    }
    Super::EndPlay(reason);
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
