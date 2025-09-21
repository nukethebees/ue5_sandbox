// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Sandbox/data/health/HealthChange.h"
#include "Sandbox/data/health/HealthData.h"
#include "Sandbox/interfaces/DeathHandler.h"

#include "HealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHealthPercentChanged, FHealthData, health_data);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDeath);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API UHealthComponent : public UActorComponent {
    GENERATED_BODY()
  public:
    UHealthComponent();
  protected:
    virtual void BeginPlay() override;
  public:
    static constexpr float NO_INITIAL_HEALTH{-1.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
    float max_health{100.0f};
    UPROPERTY(EditAnywhere, Category = "Health")
    float initial_health{NO_INITIAL_HEALTH};
    UPROPERTY(BlueprintAssignable, Category = "Health")
    FOnHealthPercentChanged on_health_percent_changed;
    UPROPERTY(BlueprintAssignable, Category = "Health")
    FOnDeath on_death;

    auto health() const { return health_; }
    bool is_dead() const { return health_ <= 0.0f; }
    bool is_alive() const { return health_ > 0.0f; }
    bool at_max_health() const { return FMath::IsNearlyEqual(health_, max_health); }
    UFUNCTION(BlueprintCallable, Category = "Health")
    void modify_health(FHealthChange change) {
        float new_health = health_;

        switch (change.type) {
            case EHealthChangeType::Damage:
                new_health -= change.value;
                break;
            case EHealthChangeType::Healing:
                new_health += change.value;
                break;
            default: {
                auto const* const enum_ptr{StaticEnum<EHealthChangeType>()};
                auto const display_text{
                    enum_ptr ? enum_ptr->GetNameStringByValue(static_cast<int64>(change.type))
                             : FString("Unknown")};
                UE_LOGFMT(LogTemp,
                          Warning,
                          "UHealthComponent::modify_health: Unhandled EHealthChangeType: {type}",
                          ("type", display_text));
                return;
            }
        }

        set_health(new_health);
    }
    UFUNCTION(BlueprintCallable, Category = "Health")
    float health_percent() const { return (max_health > 0.0f) ? health_ / max_health : 0.0f; }
  protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health")
    float health_{0.0f};
  private:
    void kill() { set_health(0.0f); }
    void set_health(float new_health) {
        auto const clamped_health{FMath::Clamp(new_health, 0.0f, max_health)};
        if (FMath::IsNearlyEqual(clamped_health, health_)) {
            return;
        }

        health_ = clamped_health;
        on_health_percent_changed.Broadcast({health_, max_health, health_percent()});
        if (is_dead()) {
            on_death.Broadcast();

            auto* owner{GetOwner()};
            if (!owner) {
                return;
            }

            // Trigger death handlers on components first then the actor itself
            TArray<UActorComponent*> components;
            owner->GetComponents(components);
            for (auto* comp : components) {
                IDeathHandler::try_kill(comp);
            }

            IDeathHandler::try_kill(owner);
        }
    }
};
