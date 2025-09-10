// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Sandbox/data/JetpackState.h"

#include "JetpackComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFuelChanged, FJetpackState const&, NewFuel);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API UJetpackComponent : public UActorComponent {
    GENERATED_BODY()
  public:
    UJetpackComponent();

    void start_jetpack();
    void stop_jetpack();
    float get_fuel() const { return fuel; }
    void broadcast_fuel_state() const;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jetpack");
    float fuel_max{0.5f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jetpack");
    float fuel_recharge_rate{0.1f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jetpack");
    float fuel_consumption_rate{1.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jetpack");
    float lift_force{15000.0f};
    UPROPERTY(BlueprintAssignable, Category = "Jetpack")
    FOnFuelChanged on_fuel_changed;
  protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float delta_time,
                               ELevelTick tick_type,
                               FActorComponentTickFunction* this_tick_function) override;
  private:
    void apply_jetpack_force(float delta_time);
    void recharge_fuel();
    void end_timers();

    FTimerHandle fuel_recharge_timer;
    FTimerHandle fuel_broadcast_timer;

    float fuel{0.0f};
    bool is_jetpacking{false};
};
