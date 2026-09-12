#pragma once

#include "CoreMinimal.h"

#include <sandbox/core/monitored_lock_free_mpsc_queue.h>

#include <string_view>

namespace ml {
template <typename Queue>
[[nodiscard]] auto logged_init(Queue& queue,
                               typename Queue::size_type const capacity,
                               std::string_view const queue_name = "") {
    auto const result{queue.init(capacity)};
    auto const name_prefix{queue_name.empty() ? "" : ": "};
    auto const name{queue_name.empty() ? "" : queue_name.data()};

    switch (result) {
        using enum ELockFreeMPSCQueueInitResult;
        case AlreadyInitialised: {
            UE_LOG(LogTemp,
                   Warning,
                   TEXT("LockFreeMPSCQueue%hs%hs init failed: Already initialised"),
                   name_prefix,
                   name);
            break;
        }
        case AllocationFailed: {
            UE_LOG(LogTemp,
                   Error,
                   TEXT("LockFreeMPSCQueue%hs%hs init failed: Allocation failed for capacity %zu"),
                   name_prefix,
                   name,
                   capacity);
            break;
        }
        default: {
            break;
        }
    }

    return result;
}

template <typename ConsumeResult>
void log_results(ConsumeResult const& result, FString const& queue_name) {
    if (result.full_count > 0) {
        UE_LOG(LogTemp, Error, TEXT("%s queue was full %zu times"), *queue_name, result.full_count);
    }
    if (result.uninitialised_count > 0) {
        UE_LOG(LogTemp,
               Error,
               TEXT("%s queue was uninitialised %zu times"),
               *queue_name,
               result.uninitialised_count);
    }
}
}
