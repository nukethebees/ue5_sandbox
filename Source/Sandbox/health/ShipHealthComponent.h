// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Sandbox/health/ShipHealth.h"

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "ShipHealthComponent.generated.h"

DECLARE_DELEGATE_OneParam(FOnShipHealthChanged, FShipHealth);

UCLASS()
class SANDBOX_API UShipHealthComponent : public UActorComponent {
    GENERATED_BODY()
  public:
    UShipHealthComponent();

    auto get_health() const { return health.health; }
    void set_health(int32 new_health);
    void add_health(int32 new_health);
    void apply_damage(int32 damage) { add_health(-damage); }
    void upgrade_max_health();
    auto get_max_health() const { return health.max_health; }
    auto get_health_info() const { return health; }
    bool is_dead() const { return health.health <= 0; }

    FOnShipHealthChanged on_health_changed;
  protected:
    void BeginPlay() override;

    UPROPERTY(EditAnywhere, Category = "Health")
    FShipHealth health{};
};
