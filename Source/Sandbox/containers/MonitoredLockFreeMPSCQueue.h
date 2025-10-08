#pragma once

#include <atomic>
#include <concepts>
#include <cstddef>
#include <string_view>
#include <utility>

#include "CoreMinimal.h"

#include "Sandbox/containers/LockFreeMPSCQueueEnums.h"

namespace ml {

template <typename T>
concept LockFreeMPSCQueueLike = requires(T queue) {
    typename T::view_type;
    { queue.init(std::declval<std::size_t>()) } -> std::same_as<ELockFreeMPSCQueueInitResult>;
    { queue.is_initialised() } -> std::convertible_to<bool>;
    { queue.swap_and_consume() } -> std::same_as<typename T::view_type>;
};

template <LockFreeMPSCQueueLike QueueType>
class MonitoredLockFreeMPSCQueue : public QueueType {
  public:
    using QueueType::QueueType;
    using size_type = typename QueueType::size_type;

    struct ConsumeResult {
        typename QueueType::view_type view;
        size_type success_count;
        size_type full_count;
        size_type uninitialised_count;

        void log_results(FString const& queue_name) const {
            if (full_count > 0) {
                UE_LOG(
                    LogTemp, Error, TEXT("%s queue was full %zu times"), *queue_name, full_count);
            }
            if (uninitialised_count > 0) {
                UE_LOG(LogTemp,
                       Error,
                       TEXT("%s queue was uninitialised %zu times"),
                       *queue_name,
                       uninitialised_count);
            }
        }
    };

    [[nodiscard]] auto logged_init(size_type n, std::string_view queue_name = "") {
        auto const result{QueueType::init(n)};

        if (result != ELockFreeMPSCQueueInitResult::Success) {
            log_init_error(result, queue_name, n);
        }

        return result;
    }

    template <typename... Args>
    [[nodiscard]] auto enqueue(Args&&... args) noexcept(
        noexcept(QueueType::enqueue(std::forward<Args>(args)...))) {
        auto const result{QueueType::enqueue(std::forward<Args>(args)...)};

        switch (result) {
            using enum ELockFreeMPSCQueueEnqueueResult;
            case Success: {
                ++success_count_;
                break;
            }
            case Full: {
                ++full_count_;
                break;
            }
            case Uninitialised: {
                ++uninitialised_count_;
                break;
            }
        }

        return result;
    }

    [[nodiscard]] auto swap_and_consume() noexcept(noexcept(QueueType::swap_and_consume()))
        -> ConsumeResult {
        auto view{QueueType::swap_and_consume()};

        return ConsumeResult{
            .view = std::move(view),
            .success_count = success_count_.exchange(0, std::memory_order_relaxed),
            .full_count = full_count_.exchange(0, std::memory_order_relaxed),
            .uninitialised_count = uninitialised_count_.exchange(0, std::memory_order_relaxed),
        };
    }

    template <typename Callable>
        requires std::invocable<Callable, ConsumeResult const&>
    [[nodiscard]] decltype(auto) swap_and_visit(Callable&& callable) {
        auto result{swap_and_consume()};
        return std::forward<Callable>(callable)(result);
    }

    [[nodiscard]] size_type get_success_count() const noexcept {
        return success_count_.load(std::memory_order_relaxed);
    }
    [[nodiscard]] size_type get_full_count() const noexcept {
        return full_count_.load(std::memory_order_relaxed);
    }
    [[nodiscard]] size_type get_uninitialised_count() const noexcept {
        return uninitialised_count_.load(std::memory_order_relaxed);
    }

    void reset_counters() noexcept {
        success_count_.store(0, std::memory_order_relaxed);
        full_count_.store(0, std::memory_order_relaxed);
        uninitialised_count_.store(0, std::memory_order_relaxed);
    }
  private:
    void log_init_error(ELockFreeMPSCQueueInitResult result,
                        std::string_view queue_name,
                        size_type requested_capacity) const {
        auto const name_prefix{queue_name.empty() ? "" : ": "};
        auto const name_str{queue_name.empty() ? "" : queue_name.data()};

        switch (result) {
            using enum ELockFreeMPSCQueueInitResult;
            case AlreadyInitialised: {
                UE_LOG(LogTemp,
                       Warning,
                       TEXT("LockFreeMPSCQueue%hs%hs init failed: Already initialised"),
                       name_prefix,
                       name_str);
                break;
            }
            case AllocationFailed: {
                UE_LOG(
                    LogTemp,
                    Error,
                    TEXT("LockFreeMPSCQueue%hs%hs init failed: Allocation failed for capacity %zu"),
                    name_prefix,
                    name_str,
                    requested_capacity);
                break;
            }
            default: {
                break;
            }
        }
    }

    std::atomic<size_type> success_count_{0};
    std::atomic<size_type> full_count_{0};
    std::atomic<size_type> uninitialised_count_{0};
};

}
