// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "JetpackComponent.generated.h"

USTRUCT(BlueprintType)
struct FJetpackState {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float fuel_remaining{0.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float fuel_capacity{0.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool is_jetpacking{false};

    FJetpackState() = default;
    FJetpackState(float fuel_remaining, float fuel_capacity, bool is_jetpacking)
        : fuel_remaining{fuel_remaining}
        , fuel_capacity{fuel_capacity}
        , is_jetpacking{is_jetpacking} {}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFuelChanged, FJetpackState const&, NewFuel);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API UJetpackComponent : public UActorComponent {
    GENERATED_BODY()
  public:
    UJetpackComponent();

    void start_jetpack();
    void stop_jetpack();
    float get_fuel() const { return fuel; }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jetpack");
    float fuel_max{2.0f};
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

    FTimerHandle fuel_recharge_timer;

    float fuel{0.0f};
    bool is_jetpacking{false};
};
