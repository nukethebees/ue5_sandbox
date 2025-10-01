#pragma once

#include <optional>
#include <span>
#include <tuple>
#include <utility>

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Sandbox/concepts/TriggerPayloadConcept.h"
#include "Sandbox/data/trigger/StrongIds.h"
#include "Sandbox/data/trigger/TriggerableId.h"
#include "Sandbox/data/trigger/TriggerContext.h"
#include "Sandbox/data/trigger/TriggerResult.h"
#include "Sandbox/macros/switch_stamping.hpp"
#include "Sandbox/mixins/log_msg_mixin.hpp"
#include "Sandbox/utilities/tuple.h"
#include "Sandbox/utilities/actor_utils.h"

struct FTriggerableRange {
    uint32 offset{0};
    uint16 length{0};

    bool is_empty() const { return length == 0; }
    uint32 end() const { return offset + length; }
};

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
#define TRIGGER_CASE(i)                                   \
    case i: {                                             \
        self.handle_trigger_case<i>(id, trigger_context); \
        break;                                            \
    }

#define TRIGGER_VISIT_STAMP(stamper, N_CASES)                                           \
    do {                                                                                \
        static_assert(N_TYPES <= (N_CASES), "n is too large for this expansion.");      \
        switch (id.tuple_index()) {                                                     \
            stamper(0, TRIGGER_CASE);                                                   \
            default: {                                                                  \
                LOG.log_warning(TEXT("Unhandled trigger type: %d."), id.tuple_index()); \
                break;                                                                  \
            }                                                                           \
        }                                                                               \
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
class TriggerSubsystemCore : public ml::LogMsgMixin<"TriggerSubsystemCore"> {
  public:
    using TriggerableTupleT = ml::TriggerTuple<Types...>;
    static constexpr std::size_t N_TYPES{sizeof...(Types)};

    template <typename Self, typename Payload>
    std::optional<TriggerableId>
        register_triggerable(this Self&& self, AActor& actor, Payload&& payload) {
        TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::TriggerSubsystemCore::register_triggerable"))

        static_assert(ml::IsTriggerPayload<std::remove_cvref_t<Payload>>,
                      "Payload must satisfy IsTriggerPayload concept");

        // Actor triggerables must be stored contiguously
        // The actor may be added separately from its triggerables and this must be handled
        if (self.current_registering_actor != &actor) {
            // Check if actor already has triggerables
            if (auto actor_id_opt{self.get_actor_id(actor)}) {
                if (auto* range{self.actor_id_to_range.Find(*actor_id_opt)}) {
                    if (range->length > 0) {
                        self.log_error(TEXT("Actor %s already finished registration - cannot add "
                                            "more triggerables"),
                                       *ActorUtils::GetBestDisplayName(&actor));
                        return std::nullopt;
                    }
                }
            }
            self.current_registering_actor = &actor;
        }

        auto& payload_array{ml::TriggerArrayGet<std::remove_cvref_t<Payload>>(self.triggerables)};
        auto const payload_element_index{payload_array.Add(std::forward<Payload>(payload))};
        static constexpr auto payload_array_index{
            ml::trigger_array_index_v<std::remove_cvref_t<Payload>, Self>};

        TriggerableId const id{static_cast<int32>(payload_array_index), payload_element_index};
        auto const triggerable_id_index{self.triggerable_ids.Add(id)};
        auto const actor_id{self.get_or_create_actor_id(actor)};

        if (auto* range{self.actor_id_to_range.Find(actor_id)}) {
            if (range->is_empty()) {
                range->offset = triggerable_id_index;
            }
            range->length++;
        }

        self.id_to_actor.Add(id.as_combined_id(), &actor);
        self.log_verbose(TEXT("Registered triggerable for actor %s with ID (%d, %d)"),
                         *ActorUtils::GetBestDisplayName(&actor),
                         id.tuple_index(),
                         id.array_index());

        return id;
    }

    template <typename Self>
    void deregister_triggerable(this Self&& self, AActor& actor) {
        TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::TriggerSubsystemCore::deregister_triggerable"))

        auto triggerable_ids{self.get_triggerable_ids(actor)};
        if (triggerable_ids.empty()) {
            return;
        }

        // Remove all TriggerableIds for this actor from id_to_actor map
        for (auto const& id : triggerable_ids) {
            self.id_to_actor.Remove(id.as_combined_id());
        }

        // Remove actor from actor ID system
        if (auto actor_id_opt{self.get_actor_id(actor)}) {
            self.actor_id_to_range.Remove(*actor_id_opt);
            self.actor_to_actor_id.Remove(&actor);
        }

        self.log_verbose(TEXT("Deregistered triggerable for actor %s"),
                         *ActorUtils::GetBestDisplayName(&actor));
    }

    template <typename Self>
    std::span<TriggerableId const> get_triggerable_ids(this Self&& self, AActor& actor) {
        auto const actor_id_opt{self.get_actor_id(actor)};
        if (!actor_id_opt) {
            return {};
        }

        auto const* range{self.actor_id_to_range.Find(*actor_id_opt)};
        if (!range || range->is_empty()) {
            return {};
        }

        return std::span{&self.triggerable_ids[range->offset], range->length};
    }

    template <typename Self>
    ETriggerOccurred trigger(this Self&& self, AActor& actor, FTriggeringSource source) {
        TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::TriggerSubsystemCore::trigger"))
        static constexpr auto LOG{NestedLogger<"trigger">()};

        if (auto* const id{self.actor_to_actor_id.Find(&actor)}) {
            return self.trigger(*id, source);
        } else {
            LOG.log_warning(TEXT("Actor is not registered."));
            return ETriggerOccurred::no;
        }
    }

    template <typename Self>
    ETriggerOccurred trigger(this Self&& self, ActorId actor_id, FTriggeringSource source) {
        TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::TriggerSubsystemCore::trigger"))

        static constexpr auto LOG{NestedLogger<"trigger">()};

        auto const* range{self.actor_id_to_range.Find(actor_id)};
        if (!range || range->is_empty()) {
            LOG.log_verbose(TEXT("Actor ID %llu has no triggerables"), actor_id.get());
            return ETriggerOccurred::no;
        }

        // Get first triggerable to obtain actor and world for context creation
        auto const first_triggerable_id{self.triggerable_ids[range->offset]};
        auto* actor{self.get_actor(first_triggerable_id)};
        if (!actor) {
            return ETriggerOccurred::no;
        }

        auto* world{actor->GetWorld()};
        if (!world) {
            LOG.log_warning(TEXT("Actor has no world"));
            return ETriggerOccurred::no;
        }

        // Create context once for all triggerables of this actor
        constexpr float trigger_strength{1.0f};
        FTriggerContext const trigger_context{*world,
                                              *actor,
                                              source,
                                              trigger_strength,
                                              actor->GetActorLocation(),
                                              world->GetDeltaSeconds()};

        LOG.log_verbose(
            TEXT("Triggering %d triggerables for actor ID %llu"), range->length, actor_id.get());

        // Trigger all triggerables for this actor with shared context
        std::span<TriggerableId const> triggerables{&self.triggerable_ids[range->offset],
                                                    range->length};
        for (auto const& id : triggerables) {
            LOG.log_verbose(
                TEXT("Triggering TriggerableId (%d, %d)"), id.tuple_index(), id.array_index());

            // Inline switch statement for performance (no function call overhead)
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
                LOG.log_warning(TEXT("Too many types for branch. This should never hit."));
            }
        }

        return ETriggerOccurred::yes;
    }

    template <typename Self>
    bool call_payload_tick(this Self&& self, TriggerableId id, float delta_time) {
        TRACE_CPUPROFILER_EVENT_SCOPE(
            TEXT("Sandbox::UCollisionEffectSubsystemCore::call_payload_tick"))
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
    }

    template <typename Self>
    void tick_payloads(this Self&& self, float delta_time) {
        TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::UCollisionEffectSubsystemCore::tick_payloads"))
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

        self.log_very_verbose(TEXT("Ticking %d payloads"), self.ticking_payloads.Num());
    }

    template <typename Self>
    bool has_ticking_payloads(this Self&& self) {
        return !self.ticking_payloads.IsEmpty();
    }

    // Actor ID management
    template <typename Self>
    ActorId get_or_create_actor_id(this Self&& self, AActor& actor) {
        TRACE_CPUPROFILER_EVENT_SCOPE(
            TEXT("Sandbox::UCollisionEffectSubsystemCore::get_or_create_actor_id"))
        static constexpr auto LOG{self.NestedLogger<"get_or_create_actor_id">()};

        if (auto* existing_id{self.actor_to_actor_id.Find(&actor)}) {
            return *existing_id;
        }

        ActorId const new_id{self.next_actor_id++};
        self.actor_to_actor_id.Add(&actor, new_id);
        self.actor_id_to_range.Add(new_id, FTriggerableRange{});

        LOG.log_verbose(TEXT("Created actor ID %llu for actor %s"),
                        new_id.get(),
                        *ActorUtils::GetBestDisplayName(&actor));
        return new_id;
    }

    template <typename Self>
    std::optional<ActorId> get_actor_id(this Self&& self, AActor& actor) {
        TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::UCollisionEffectSubsystemCore::get_actor_id"))
        if (auto* id{self.actor_to_actor_id.Find(&actor)}) {
            return *id;
        }

        return std::nullopt;
    }

    template <typename Self>
    auto* get_actor(this Self&& self, TriggerableId id) {
        TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::UCollisionEffectSubsystemCore::get_actor"))

        auto const combined_id{id.as_combined_id()};
        auto const* actor_ptr{std::forward<Self>(self).id_to_actor.Find(combined_id)};
        using Return = decltype(*actor_ptr);

        if (!actor_ptr || !*actor_ptr) {
            self.log_warning(TEXT("Cannot find actor for trigger ID (%d, %d)"),
                             id.tuple_index(),
                             id.array_index());
            return Return{nullptr};
        }

        return *actor_ptr;
    }
  private:
    template <std::size_t TupleIndex, typename Self>
    void handle_trigger_case(this Self&& self,
                             TriggerableId id,
                             FTriggerContext const& trigger_context) {
        TRACE_CPUPROFILER_EVENT_SCOPE(
            TEXT("Sandbox::UCollisionEffectSubsystemCore::handle_trigger_case"))

        static constexpr auto LOG{NestedLogger<"handle_trigger_case">()};

        if constexpr (TupleIndex < N_TYPES) {
            LOG.log_verbose(TEXT("Handling trigger case %d."), static_cast<int32>(TupleIndex));
            auto const result{
                std::get<TupleIndex>(self.triggerables)[id.array_index()].trigger(trigger_context)};

            if (result.enable_ticking) {
                self.ticking_payloads.AddUnique(id);
                self.log_verbose(
                    TEXT("Enabled ticking for ID (%d, %d)"), id.tuple_index(), id.array_index());
            }
        }
    }
    TriggerableTupleT triggerables;
    TArray<TriggerableId> triggerable_ids; // Parallel storage for contiguous ID access
    TArray<TriggerableId> ticking_payloads;

    TMap<TriggerableId::CombinedId, AActor*> id_to_actor;
    TMap<AActor*, ActorId> actor_to_actor_id;
    TMap<ActorId, FTriggerableRange> actor_id_to_range;

    ActorId next_actor_id{ActorId{0}};
    AActor* current_registering_actor{nullptr};
};

#undef TRIGGER_CASE
#undef TRIGGER_VISIT_STAMP
#undef TICK_CASE
#undef TICK_VISIT_STAMP

#include "Sandbox/macros/switch_stamping_undef.hpp"
