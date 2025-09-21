#pragma once

#include <optional>
#include <tuple>
#include <utility>

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Sandbox/data/TriggerableId.h"
#include "Sandbox/data/TriggerContext.h"
#include "Sandbox/data/TriggerPayloadConcept.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"
#include "Sandbox/utilities/tuple.h"

namespace ml {
template <typename T>
using TriggerArray = TArray<T>;

template <typename... Args>
    requires (IsTriggerPayload<Args> && ...)
using TriggerTuple = std::tuple<TriggerArray<Args>...>;

template <typename T, typename Tuple>
decltype(auto) TriggerArrayGet(Tuple&& tup) {
    return std::get<TriggerArray<T>>(std::forward<Tuple>(tup));
}

template <typename T, typename Subsystem>
constexpr auto trigger_array_index_v =
    tuple_type_index<TriggerArray<T>,
                     typename std::remove_cvref_t<Subsystem>::TriggerableTupleT>::value;
}

// Macros for generating switch statement
#define TRIGGER_STAMP4(n) \
    TRIGGER_CASE(n);      \
    TRIGGER_CASE(n + 1);  \
    TRIGGER_CASE(n + 2);  \
    TRIGGER_CASE(n + 3)

#define TRIGGER_STAMP16(n) \
    TRIGGER_STAMP4(n);     \
    TRIGGER_STAMP4(n + 4); \
    TRIGGER_STAMP4(n + 8); \
    TRIGGER_STAMP4(n + 12)

#define TRIGGER_STAMP64(n)   \
    TRIGGER_STAMP16(n);      \
    TRIGGER_STAMP16(n + 16); \
    TRIGGER_STAMP16(n + 32); \
    TRIGGER_STAMP16(n + 48)

#define TRIGGER_STAMP256(n)   \
    TRIGGER_STAMP64(n);       \
    TRIGGER_STAMP64(n + 64);  \
    TRIGGER_STAMP64(n + 128); \
    TRIGGER_STAMP64(n + 192)

#define TRIGGER_CASE(i)                                                                \
    case i: {                                                                          \
        if constexpr (i < N_TYPES) {                                                   \
            self.log_verbose(TEXT("Handling trigger case %d."), i);                    \
            std::get<i>(self.triggerables)[id.array_index()].trigger(trigger_context); \
        }                                                                              \
        break;                                                                         \
    }

#define TRIGGER_VISIT_STAMP(stamper, N_CASES)                                            \
    do {                                                                                 \
        static_assert(N_TYPES <= (N_CASES), "n is too large for this expansion.");       \
        switch (id.tuple_index()) {                                                      \
            stamper(0);                                                                  \
            default: {                                                                   \
                self.log_warning(TEXT("Unhandled trigger type: %d."), id.tuple_index()); \
                break;                                                                   \
            }                                                                            \
        }                                                                                \
    } while (0)

#define TRIGGER_STAMP(N_CASES) TRIGGER_VISIT_STAMP(TRIGGER_STAMP##N_CASES, N_CASES)

template <typename... Types>
class UTriggerSubsystemData : public ml::LogMsgMixin<"UTriggerSubsystemData"> {
  public:
    using TriggerableTupleT = ml::TriggerTuple<Types...>;
    static constexpr std::size_t N_TYPES{sizeof...(Types)};

    template <typename Self, typename Payload>
    TriggerableId register_triggerable(this Self&& self,
                                       AActor* actor,
                                       Payload&& payload,
                                       auto* top_subsystem) {
        static_assert(ml::IsTriggerPayload<std::remove_cvref_t<Payload>>,
                      "Payload must satisfy IsTriggerPayload concept");

        if (!actor) {
            self.log_warning(TEXT("Cannot register null actor"));
            return TriggerableId::invalid();
        }

        auto& payload_array{ml::TriggerArrayGet<std::remove_cvref_t<Payload>>(self.triggerables)};
        auto const payload_element_index{payload_array.Add(std::forward<Payload>(payload))};
        constexpr auto payload_array_index{
            ml::trigger_array_index_v<std::remove_cvref_t<Payload>, Self>};

        TriggerableId id{static_cast<int32>(payload_array_index), payload_element_index};

        self.actor_to_id.Add(actor, id);
        self.id_to_actor.Add(id.as_combined_id(), actor);

        self.log_verbose(TEXT("Registered triggerable for actor %s with ID (%d, %d)"),
                         *actor->GetName(),
                         id.tuple_index(),
                         id.array_index());

        return id;
    }

    template <typename Self>
    void trigger(this Self&& self, TriggerableId id, FTriggeringSource source) {
        if (!id.is_valid()) {
            self.log_warning(TEXT("Cannot trigger invalid ID"));
            return;
        }

        auto const combined_id{id.as_combined_id()};
        auto const* actor_ptr{self.id_to_actor.Find(combined_id)};
        if (!actor_ptr || !*actor_ptr) {
            self.log_warning(TEXT("Cannot find actor for trigger ID (%d, %d)"),
                             id.tuple_index(),
                             id.array_index());
            return;
        }

        auto* actor{*actor_ptr};
        auto* world{actor->GetWorld()};
        if (!world) {
            self.log_warning(TEXT("Actor has no world"));
            return;
        }

        FTriggerContext trigger_context{.world = *world,
                                        .triggered_actor = *actor,
                                        .source = source,
                                        .trigger_strength = 1.0f,
                                        .trigger_location = actor->GetActorLocation(),
                                        .delta_time = world->GetDeltaSeconds()};

        self.log_verbose(TEXT("Triggering ID (%d, %d) for actor %s"),
                         id.tuple_index(),
                         id.array_index(),
                         *actor->GetName());

        static_assert(N_TYPES <= 256,
                      "Cannot support this many trigger types. The macros must be extended.");

        if constexpr (N_TYPES <= 4) {
            TRIGGER_STAMP(4);
        } else if constexpr (N_TYPES <= 16) {
            TRIGGER_STAMP(16);
        } else if constexpr (N_TYPES <= 64) {
            TRIGGER_STAMP(64);
        } else if constexpr (N_TYPES <= 256) {
            TRIGGER_STAMP(256);
        } else {
            self.log_warning(TEXT("Too many types for branch. This should never hit."));
            return;
        }
    }

    template <typename Self>
    void deregister_triggerable(this Self&& self, AActor* actor) {
        if (!actor) {
            return;
        }

        auto const* id_ptr{self.actor_to_id.Find(actor)};
        if (!id_ptr) {
            return;
        }

        auto const id{*id_ptr};
        auto const combined_id{id.as_combined_id()};

        self.actor_to_id.Remove(actor);
        self.id_to_actor.Remove(combined_id);

        self.log_verbose(TEXT("Deregistered triggerable for actor %s"), *actor->GetName());
    }

    template <typename Self>
    std::optional<TriggerableId> get_triggerable_id(this Self&& self, AActor* actor) {
        if (!actor) {
            return std::nullopt;
        }

        auto const* id_ptr{self.actor_to_id.Find(actor)};
        if (!id_ptr) {
            return std::nullopt;
        }

        return *id_ptr;
    }
  private:
    TriggerableTupleT triggerables;
    TMap<AActor*, TriggerableId> actor_to_id;
    TMap<uint64, AActor*> id_to_actor;
};

#undef TRIGGER_STAMP4
#undef TRIGGER_STAMP16
#undef TRIGGER_STAMP64
#undef TRIGGER_STAMP256
#undef TRIGGER_CASE
#undef TRIGGER_VISIT_STAMP
#undef TRIGGER_STAMP
