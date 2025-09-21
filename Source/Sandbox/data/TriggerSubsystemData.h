#pragma once

#include <optional>
#include <tuple>
#include <utility>

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Sandbox/data/TriggerableId.h"
#include "Sandbox/data/TriggerContext.h"
#include "Sandbox/data/TriggerPayloadConcept.h"
#include "Sandbox/data/TriggerResult.h"
#include "Sandbox/data/TriggerResults.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"
#include "Sandbox/utilities/tuple.h"
#include "Sandbox/macros/switch_stamping.hpp"

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
#define TRIGGER_CASE(i)                                                                     \
    case i: {                                                                               \
        if constexpr (i < N_TYPES) {                                                        \
            self.log_verbose(TEXT("Handling trigger case %d."), i);                         \
            auto result{                                                                    \
                std::get<i>(self.triggerables)[id.array_index()].trigger(trigger_context)}; \
            self.handle_trigger_result(id, result);                                         \
        }                                                                                   \
        break;                                                                              \
    }

#define TRIGGER_VISIT_STAMP(stamper, N_CASES)                                            \
    do {                                                                                 \
        static_assert(N_TYPES <= (N_CASES), "n is too large for this expansion.");       \
        switch (id.tuple_index()) {                                                      \
            stamper(0, TRIGGER_CASE);                                                    \
            default: {                                                                   \
                self.log_warning(TEXT("Unhandled trigger type: %d."), id.tuple_index()); \
                break;                                                                   \
            }                                                                            \
        }                                                                                \
    } while (0)

#define TICK_CASE(i)                                                                  \
    case i: {                                                                         \
        if constexpr (i < N_TYPES) {                                                  \
            return std::get<i>(self.triggerables)[id.array_index()].tick(delta_time); \
        }                                                                             \
        break;                                                                        \
    }

#define TICK_VISIT_STAMP(stamper, N_CASES)                                            \
    do {                                                                              \
        static_assert(N_TYPES <= (N_CASES), "n is too large for this expansion.");    \
        switch (id.tuple_index()) {                                                   \
            stamper(0, TICK_CASE);                                                    \
            default: {                                                                \
                self.log_warning(TEXT("Unhandled tick type: %d."), id.tuple_index()); \
                return false;                                                         \
            }                                                                         \
        }                                                                             \
    } while (0)

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
            SWITCH_STAMP(4, TRIGGER_VISIT_STAMP);
        } else if constexpr (N_TYPES <= 16) {
            SWITCH_STAMP(16, TRIGGER_VISIT_STAMP);
        } else if constexpr (N_TYPES <= 64) {
            SWITCH_STAMP(64, TRIGGER_VISIT_STAMP);
        } else if constexpr (N_TYPES <= 256) {
            SWITCH_STAMP(256, TRIGGER_VISIT_STAMP);
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

    template <typename Self>
    ETriggerOccurred trigger(this Self&& self, AActor* actor, FTriggeringSource source) {
        auto id_opt{self.get_triggerable_id(actor)};
        if (!id_opt || !id_opt->is_valid()) {
            return ETriggerOccurred::no;
        }

        self.trigger(*id_opt, source);
        return ETriggerOccurred::yes;
    }

    template <typename Self>
    FTriggerResults
        trigger(this Self&& self, TArrayView<AActor*> actors, FTriggeringSource source) {
        // Fill results array from both sides
        // Triggered actors from the left, non-triggered from the right
        FTriggerResults results{};
        results.actors.Init(nullptr, actors.Num());
        int32 triggered_index{0};
        int32 not_triggered_index{actors.Num() - 1};

        for (auto* actor : actors) {
            if (self.trigger(actor, source) == ETriggerOccurred::yes) {
                results.actors[triggered_index++] = actor;
            } else {
                results.actors[not_triggered_index--] = actor;
            }
        }

        results.n_triggered = triggered_index;

        self.log_verbose(TEXT("Batch trigger: %d triggered, %d not triggered"),
                         results.n_triggered,
                         results.num_not_triggered());

        return results;
    }

    template <typename Self>
    void handle_trigger_result(this Self&& self, TriggerableId id, FTriggerResult result) {
        if (result.enable_ticking) {
            self.ticking_payloads.AddUnique(id);
            self.log_verbose(
                TEXT("Enabled ticking for ID (%d, %d)"), id.tuple_index(), id.array_index());
        }
    }

    template <typename Self>
    bool call_payload_tick(this Self&& self, TriggerableId id, float delta_time) {
        static_assert(N_TYPES <= 256, "Cannot support this many trigger types for ticking.");

        if constexpr (N_TYPES <= 4) {
            SWITCH_STAMP(4, TICK_VISIT_STAMP);
        } else if constexpr (N_TYPES <= 16) {
            SWITCH_STAMP(16, TICK_VISIT_STAMP);
        } else if constexpr (N_TYPES <= 64) {
            SWITCH_STAMP(64, TICK_VISIT_STAMP);
        } else if constexpr (N_TYPES <= 256) {
            SWITCH_STAMP(256, TICK_VISIT_STAMP);
        } else {
            self.log_warning(TEXT("Too many types for tick branch."));
            return false;
        }
        return false; // Should never reach here
    }

    template <typename Self>
    void tick_payloads(this Self&& self, float delta_time) {
        if (self.ticking_payloads.IsEmpty()) {
            return;
        }

        int32 write_index{0}; // Where to write next still-ticking ID

        for (int32 read_index{0}; read_index < self.ticking_payloads.Num(); ++read_index) {
            auto const id{self.ticking_payloads[read_index]};
            auto const continue_ticking{self.call_payload_tick(id, delta_time)};

            if (continue_ticking) {
                self.ticking_payloads[write_index++] = id; // Keep this one
            }
        }

        // Shrink array to only the still-ticking payloads
        self.ticking_payloads.SetNum(write_index);

        self.log_verbose(TEXT("Ticking %d payloads"), self.ticking_payloads.Num());
    }

    template <typename Self>
    bool has_ticking_payloads(this Self&& self) {
        return !self.ticking_payloads.IsEmpty();
    }
  private:
    TriggerableTupleT triggerables;
    TMap<AActor*, TriggerableId> actor_to_id;
    TMap<uint64, AActor*> id_to_actor;
    TArray<TriggerableId> ticking_payloads;
};

#undef TRIGGER_CASE
#undef TRIGGER_VISIT_STAMP
#undef TICK_CASE
#undef TICK_VISIT_STAMP

#include "Sandbox/macros/switch_stamping_undef.hpp"
