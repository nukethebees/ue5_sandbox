#include "Sandbox/players/npcs/actor_components/DamageAuraComponent.h"

#include "Components/BoxComponent.h"
#include "GameFramework/Actor.h"
#include "GenericTeamAgentInterface.h"
#include "TimerManager.h"

#include "Sandbox/health/actor_components/HealthComponent.h"
#include "Sandbox/health/data/HealthChange.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

UDamageAuraComponent::UDamageAuraComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}

void UDamageAuraComponent::BeginPlay() {
    Super::BeginPlay();

    TRY_INIT_PTR(owner, GetOwner());

    collision_box = owner->FindComponentByClass<UBoxComponent>();
    RETURN_IF_NULLPTR(collision_box);

    collision_box->OnComponentBeginOverlap.AddDynamic(this,
                                                      &UDamageAuraComponent::on_overlap_begin);
    collision_box->OnComponentEndOverlap.AddDynamic(this, &UDamageAuraComponent::on_overlap_end);

    collision_box->IgnoreActorWhenMoving(owner, true);
}

void UDamageAuraComponent::on_overlap_begin(UPrimitiveComponent* overlapped_comp,
                                            AActor* other_actor,
                                            UPrimitiveComponent* other_comp,
                                            int32 other_body_index,
                                            bool b_from_sweep,
                                            FHitResult const& sweep_result) {
    RETURN_IF_NULLPTR(other_actor);
    if (other_actor == GetOwner()) {
        return;
    }

    TRY_INIT_PTR(health_component, other_actor->FindComponentByClass<UHealthComponent>());
    actors_in_aura.Add(other_actor);
    update_timer();
}

void UDamageAuraComponent::on_overlap_end(UPrimitiveComponent* overlapped_comp,
                                          AActor* other_actor,
                                          UPrimitiveComponent* other_comp,
                                          int32 other_body_index) {
    actors_in_aura.Remove(other_actor);
    update_timer();
}

void UDamageAuraComponent::apply_damage() {
    TArray<TWeakObjectPtr<AActor>> actors_to_remove{};

    TRY_INIT_PTR(owner, GetOwner());

    for (auto const& weak_actor : actors_in_aura) {
        auto* actor{weak_actor.Get()};
        if (!actor) {
            actors_to_remove.Add(weak_actor);
            continue;
        }

        auto* health_component{actor->FindComponentByClass<UHealthComponent>()};
        if (!health_component) {
            actors_to_remove.Add(weak_actor);
            continue;
        }

        // Check team attitude if only_damage_enemies is enabled
        if (only_damage_enemies) {
            auto const owner_attitude{FGenericTeamId::GetAttitude(owner, actor)};
            if (owner_attitude != ETeamAttitude::Hostile) {
                continue;
            }
        }

        auto const damage_amount{damage_per_second * tick_interval};
        health_component->modify_health(FHealthChange{damage_amount, EHealthChangeType::Damage});
    }

    for (auto const& actor_to_remove : actors_to_remove) {
        actors_in_aura.Remove(actor_to_remove);
    }

    if (actors_in_aura.Num() == 0) {
        update_timer();
    }
}

void UDamageAuraComponent::update_timer() {
    TRY_INIT_PTR(world, GetWorld());

    if (actors_in_aura.Num() == 0) {
        world->GetTimerManager().ClearTimer(damage_timer);
    } else {
        if (!world->GetTimerManager().IsTimerActive(damage_timer)) {
            world->GetTimerManager().SetTimer(
                damage_timer, this, &UDamageAuraComponent::apply_damage, tick_interval, true);
        }
    }
}
