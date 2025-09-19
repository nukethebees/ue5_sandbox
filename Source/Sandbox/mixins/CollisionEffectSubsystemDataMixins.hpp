#pragma once

#include <concepts>
#include <optional>
#include <tuple>
#include <utility>

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Sandbox/interfaces/CollisionOwner.h"
#include "Sandbox/subsystems/DestructionManagerSubsystem.h"
#include "Sandbox/utilities/tuple.h"

namespace ml {
template <typename T>
concept IsCollisionPayload = requires(T t, AActor* actor) {
    { t.execute(actor) } -> std::same_as<void>;
};

template <typename T>
using CollisionArray = TArray<T>;

template <typename... Args>
    requires (IsCollisionPayload<Args> && ...)
using ArrayTuple = std::tuple<CollisionArray<Args>...>;

template <typename T, typename Tuple>
decltype(auto) ArrayGet(Tuple&& tup) {
    return std::get<CollisionArray<T>>(std::forward<Tuple>(tup));
}

template <typename T, typename Subsystem>
constexpr auto tuple_array_index_v =
    tuple_type_index<CollisionArray<T>, typename std::remove_cvref_t<Subsystem>::PayloadsT>::value;

}

// Macros used to generate the switch statement
// Adapted from MSVC's STL variant visitor
// Don't forget to undefine these after!
// ----------------------------------------------
#define COLLISION_STAMP4(n) \
    COLLISION_CASE(n);      \
    COLLISION_CASE(n + 1);  \
    COLLISION_CASE(n + 2);  \
    COLLISION_CASE(n + 3)

#define COLLISION_STAMP16(n) \
    COLLISION_STAMP4(n);     \
    COLLISION_STAMP4(n + 4); \
    COLLISION_STAMP4(n + 8); \
    COLLISION_STAMP4(n + 12)

#define COLLISION_STAMP64(n)   \
    COLLISION_STAMP16(n);      \
    COLLISION_STAMP16(n + 16); \
    COLLISION_STAMP16(n + 32); \
    COLLISION_STAMP16(n + 48)

#define COLLISION_STAMP256(n)   \
    COLLISION_STAMP64(n);       \
    COLLISION_STAMP64(n + 64);  \
    COLLISION_STAMP64(n + 128); \
    COLLISION_STAMP64(n + 192)

#define COLLISION_CASE(i)                                                              \
    case i: {                                                                          \
        if constexpr (i < N_TYPES) {                                                   \
            std::get<i>(self.payloads)[payload_index.array_index].execute(OtherActor); \
        }                                                                              \
        break;                                                                         \
    }

#define COLLISION_VISIT_STAMP(stamper, N_CASES)                                            \
    do {                                                                                   \
        static_assert(N_TYPES < (N_CASES), "n is too large for this expansion.");          \
        switch (payload_index.type_tag) {                                                  \
            stamper(0);                                                                    \
            default: {                                                                     \
                self.log_warning(TEXT("Unhandled tag type: %d."), payload_index.type_tag); \
                break;                                                                     \
            }                                                                              \
        }                                                                                  \
    } while (0)

#define COLLISION_STAMP(N_CASES) COLLISION_VISIT_STAMP(COLLISION_STAMP##N_CASES, N_CASES)

class UCollisionEffectSubsystemDataMixins {
  public:
    template <typename Self, typename Payload>
    void add_payload(this Self&& self, AActor* actor, Payload&& payload, auto* top_subsystem) {
        auto const actor_index{self.register_actor(actor, top_subsystem)};
        if (!actor_index) {
            return;
        }

        // auto& payload_array{ml::ArrayGet<Payload>(std::forward<Self>(self).payloads)};
        auto& payload_array{ml::ArrayGet<Payload>(self.payloads)};
        auto const payload_element_index{payload_array.Add(std::forward<Payload>(payload))};
        constexpr auto payload_array_index{ml::tuple_array_index_v<Payload, Self>};

        auto& payload_indexes{std::forward<Self>(self).actor_payload_indexes[*actor_index].indexes};
        payload_indexes.Add(FPayloadIndex(static_cast<uint8>(payload_array_index),
                                          static_cast<uint8>(payload_element_index)));
    }

    template <typename Self>
    std::optional<int32> register_actor(this Self&& self, AActor* actor, auto* top_subsystem) {
        if (!actor) {
            return std::nullopt;
        }

        if (auto const* i{self.actor_ids.Find(actor)}) {
            return *i;
        }

        auto* collision_owner{Cast<ICollisionOwner>(actor)};
        if (!collision_owner) {
            return std::nullopt;
        }

        auto* collision_comp{collision_owner->get_collision_component()};
        if (!collision_comp) {
            return std::nullopt;
        }

        // collision_comp->OnComponentBeginOverlap.AddDynamic(&self,
        // &std::remove_cvref_t<Self>::handle_collision_event);
        collision_comp->OnComponentBeginOverlap.AddDynamic(
            top_subsystem, &std::remove_cvref_t<decltype(*top_subsystem)>::handle_collision_event);

        auto const actor_i{self.actors.Add(actor)};
        auto const collision_i{self.collision_boxes.Add(collision_comp)};

        check(actor_i == collision_i);

        self.actor_ids.Add(actor, actor_i);
        self.collision_ids.Add(collision_comp, actor_i);

        std::forward<Self>(self).actor_payload_indexes.AddDefaulted();

        return actor_i;
    }

    template <typename Self>
    void handle_collision_event_(this Self&& self,
                                 UPrimitiveComponent* OverlappedComponent,
                                 AActor* OtherActor,
                                 UPrimitiveComponent* OtherComp,
                                 int32 OtherBodyIndex,
                                 bool bFromSweep,
                                 FHitResult const& SweepResult) {
        self.log_verbose(TEXT("handle_collision_event"));

        if (!OtherActor || !OverlappedComponent) {
            self.log_warning(TEXT("No OtherActor or OverlappedComponent."));
            return;
        }

        auto const* index_ptr{self.collision_ids.Find(OverlappedComponent)};
        if (!index_ptr) {
            self.log_warning(TEXT("No collision entry."));
            return;
        }

        int32 const index{*index_ptr};
        auto* owner{self.actors[index].Get()};
        if (!owner) {
            self.log_warning(TEXT("No owner."));
            return;
        }

        // Should have already been validated
        auto& collision_owner{*Cast<ICollisionOwner>(owner)};

        // Execute the effects
        collision_owner.on_pre_collision_effect(OtherActor);

        auto& payload_indexes{self.actor_payload_indexes[index]};

        self.log_verbose(TEXT("Handling %d indexes"), payload_indexes.indexes.Num());

        constexpr auto N_TYPES = std::remove_cvref_t<Self>::N_TYPES;

        for (auto const& payload_index : payload_indexes.indexes) {
            if constexpr (N_TYPES <= 4) {
                COLLISION_STAMP(4);
            } else if constexpr (N_TYPES <= 16) {
                COLLISION_STAMP(16);
            } else if constexpr (N_TYPES <= 64) {
                COLLISION_STAMP(64);
            } else if constexpr (N_TYPES <= 256) {
                COLLISION_STAMP(256);
            } else {
                static_assert(
                    N_TYPES > 256,
                    "Cannot support this many collision types. The macros must be extended.");
            }
        }

        collision_owner.on_collision_effect(OtherActor);
        collision_owner.on_post_collision_effect(OtherActor);

        if (collision_owner.should_destroy_after_collision()) {
            if (auto* destruction_manager{
                    owner->GetWorld()->GetSubsystem<UDestructionManagerSubsystem>()}) {
                destruction_manager->queue_actor_destruction(owner);
            }
        }
    }
};

#undef COLLISION_STAMP4
#undef COLLISION_STAMP16
#undef COLLISION_STAMP64
#undef COLLISION_STAMP256
#undef COLLISION_CASE
#undef COLLISION_VISIT_STAMP
#undef COLLISION_STAMP
