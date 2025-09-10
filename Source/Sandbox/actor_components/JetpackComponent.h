// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "JetpackComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API UJetpackComponent : public UActorComponent {
    GENERATED_BODY()
  public:
    UJetpackComponent();

    void start_jetpack();
    void stop_jetpack();
    float get_fuel() const { return jetpack_fuel; }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement");
    float jetpack_fuel_max{2.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement");
    float jetpack_fuel_recharge_rate{1.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement");
    float jetpack_fuel_consumption_rate{1.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement");
    float jetpack_force{5000.0f};
  protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float delta_time,
                               ELevelTick tick_type,
                               FActorComponentTickFunction* this_tick_function) override;
  private:
    void apply_jetpack_force(float delta_time);
    void recharge_fuel();

    FTimerHandle fuel_recharge_timer;

    float jetpack_fuel{0.0f};
    bool is_jetpacking{false};
};
