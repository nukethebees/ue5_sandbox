#pragma once

#include "Sandbox/core/destruction/subsystems/DestructionManagerSubsystem.h"
#include "Sandbox/interaction/CollisionContext.h"
#include "Sandbox/interaction/CollisionOwner.h"
#include "Sandbox/interaction/PayloadIndex.h"
#include "Sandbox/logging/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/utilities/tuple.h"

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#include <concepts>
#include <optional>
#include <tuple>
#include <utility>

#include "Sandbox/utilities/macros/null_checks.hpp"
#include "Sandbox/utilities/macros/switch_stamping.hpp"

namespace ml {
template <typename T>
concept IsCollisionPayload = requires(T t, FCollisionContext context) {
    { t.execute(context) } -> std::same_as<void>;
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
#define COLLISION_CASE(i)                                                                     \
    case i: {                                                                                 \
        if constexpr (i < N_TYPES) {                                                          \
            self.log_verbose(TEXT("Handling case %d."), i);                                   \
            std::get<i>(self.payloads)[payload_index.array_index].execute(collision_context); \
        }                                                                                     \
        break;                                                                                \
    }

#define COLLISION_VISIT_STAMP(stamper, N_CASES)                                            \
    do {                                                                                   \
        switch (payload_index.type_tag) {                                                  \
            stamper(0, COLLISION_CASE);                                                    \
            default: {                                                                     \
                self.log_warning(TEXT("Unhandled tag type: %d."), payload_index.type_tag); \
                break;                                                                     \
            }                                                                              \
        }                                                                                  \
    } while (0)

template <typename... Types>
class UCollisionEffectSubsystemCore
    : public ml::LogMsgMixin<"UCollisionEffectSubsystemCore", LogSandboxSubsystem> {
  public:
    using PayloadsT = ml::ArrayTuple<Types...>;
    static constexpr std::size_t N_TYPES = sizeof...(Types);

    template <typename Self, typename Payload>
    void add_payload(this Self& self, AActor& actor, Payload&& payload, auto* top_subsystem) {
        using PayloadT = std::remove_cvref_t<Payload>;

        auto const actor_index{self.register_actor(actor, top_subsystem)};
        RETURN_IF_FALSE(actor_index);

        auto& payload_array{ml::ArrayGet<PayloadT>(self.payloads)};
        auto const payload_element_index{payload_array.Add(std::forward<Payload>(payload))};
        constexpr auto payload_array_index{ml::tuple_array_index_v<PayloadT, Self>};

        auto& payload_indexes{self.actor_payload_indexes[*actor_index].indexes};
        payload_indexes.Add(FPayloadIndex(static_cast<uint8>(payload_array_index),
                                          static_cast<uint8>(payload_element_index)));
    }

    template <typename Self>
    std::optional<int32> register_actor(this Self& self, AActor& actor, auto* top_subsystem) {
        TRACE_CPUPROFILER_EVENT_SCOPE(
            TEXT("Sandbox::UCollisionEffectSubsystemCore::register_actor"))

        if (auto const* i{self.actor_ids.Find(&actor)}) {
            return *i;
        }

        INIT_OR_RETURN_VALUE_IF_FALSE(auto*, collision_owner, Cast<ICollisionOwner>(&actor), {});
        INIT_OR_RETURN_VALUE_IF_FALSE(
            auto*, collision_comp, collision_owner->get_collision_component(), {});

        collision_comp->OnComponentBeginOverlap.AddDynamic(
            top_subsystem, &std::remove_cvref_t<decltype(*top_subsystem)>::handle_collision_event);

        auto const actor_i{self.actors.Add(&actor)};
        auto const collision_i{self.collision_boxes.Add(collision_comp)};

        check(actor_i == collision_i);

        self.actor_ids.Add(&actor, actor_i);
        self.collision_ids.Add(collision_comp, actor_i);

        self.actor_payload_indexes.AddDefaulted();

        return actor_i;
    }

    template <typename Self>
    void handle_collision_event_(this Self&& self,
                                 UPrimitiveComponent* overlapped_component,
                                 AActor* other_actor,
                                 UPrimitiveComponent* OtherComp,
                                 int32 other_body_index,
                                 bool from_sweep,
                                 FHitResult const& sweep_result) {
        static constexpr auto logger{NestedLogger<"handle_collision_event_">()};

        logger.log_verbose(TEXT("handle_collision_event"));

        RETURN_IF_NULLPTR(other_actor);
        RETURN_IF_NULLPTR(overlapped_component);

        auto const* index_ptr{self.collision_ids.Find(overlapped_component)};
        RETURN_IF_NULLPTR(index_ptr);

        int32 const index{*index_ptr};
        auto* owner{self.actors[index].Get()};
        RETURN_IF_NULLPTR(owner);

        auto* world{owner->GetWorld()};
        RETURN_IF_NULLPTR(world);

        auto collision_context{FCollisionContext(*world, *other_actor)};

        // Should have already been validated
        auto& collision_owner{*Cast<ICollisionOwner>(owner)};

        // Execute the effects
        collision_owner.on_pre_collision_effect(*other_actor);

        auto& payload_indexes{self.actor_payload_indexes[index]};

        logger.log_verbose(TEXT("Handling %d indexes"), payload_indexes.indexes.Num());

        static_assert(N_TYPES <= 256,
                      "Cannot support this many collision types. The macros must be extended.");
        for (auto const& payload_index : payload_indexes.indexes) {
            if constexpr (N_TYPES <= 4) {
                SWITCH_STAMP(4, COLLISION_VISIT_STAMP);
            } else if constexpr (N_TYPES <= 16) {
                SWITCH_STAMP(16, COLLISION_VISIT_STAMP);
            } else if constexpr (N_TYPES <= 64) {
                SWITCH_STAMP(64, COLLISION_VISIT_STAMP);
            } else if constexpr (N_TYPES <= 256) {
                SWITCH_STAMP(256, COLLISION_VISIT_STAMP);
            } else {
                self.log_warning(TEXT("Too many types for branch. This should never hit."));
                return;
            }
        }

        collision_owner.on_post_collision_effect(*other_actor);

        if (collision_owner.should_destroy_after_collision()) {
            TRY_INIT_PTR(destruction_manager, world->GetSubsystem<UDestructionManagerSubsystem>());

            auto const delay{collision_owner.get_destruction_delay()};
            if (delay > 0.0f) {
                destruction_manager->queue_destruction_with_delay(owner, delay);
            } else {
                destruction_manager->queue_destruction(owner);
            }
        }
    }
  private:
    PayloadsT payloads;
    TArray<FActorPayloadIndexes> actor_payload_indexes;
    TArray<TWeakObjectPtr<AActor>> actors;
    TArray<TWeakObjectPtr<UPrimitiveComponent>> collision_boxes;

    TMap<AActor*, int32> actor_ids;
    TMap<UPrimitiveComponent*, int32> collision_ids{};
};

#undef COLLISION_CASE
#undef COLLISION_VISIT_STAMP

#include "Sandbox/utilities/macros/null_checks_undef.hpp"
#include "Sandbox/utilities/macros/switch_stamping_undef.hpp"
