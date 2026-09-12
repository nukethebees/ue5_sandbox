#pragma once

#include "sandbox/core/lock_free_mpsc_queue_enums.h"

#include <atomic>
#include <concepts>
#include <cstddef>
#include <utility>

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

        [[nodiscard]] auto is_empty() const noexcept -> bool { return success_count == 0; }
    };

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

    [[nodiscard]] auto get_success_count() const noexcept -> size_type {
        return success_count_.load(std::memory_order_relaxed);
    }
    [[nodiscard]] auto get_full_count() const noexcept -> size_type {
        return full_count_.load(std::memory_order_relaxed);
    }
    [[nodiscard]] auto get_uninitialised_count() const noexcept -> size_type {
        return uninitialised_count_.load(std::memory_order_relaxed);
    }

    void reset_counters() noexcept {
        success_count_.store(0, std::memory_order_relaxed);
        full_count_.store(0, std::memory_order_relaxed);
        uninitialised_count_.store(0, std::memory_order_relaxed);
    }
  private:
    std::atomic<size_type> success_count_{0};
    std::atomic<size_type> full_count_{0};
    std::atomic<size_type> uninitialised_count_{0};
};
}
