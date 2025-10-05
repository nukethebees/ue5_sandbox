#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory_resource>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>

#include "Sandbox/containers/LockFreeMPSCQueueEnums.h"

template <typename View, typename... Ts>
concept is_soa_queue_view = std::is_void_v<View> || std::constructible_from<View, std::span<Ts>...>;

namespace ml {
// Lock-free multi-producer single-consumer queue with Structure of Arrays layout
// Contract: swap_and_consume() must only be called when all enqueue() operations are complete
template <typename View = void, typename... Ts>
    requires (((sizeof...(Ts) > 0) && (std::is_nothrow_move_constructible_v<Ts> && ...)) &&
              is_soa_queue_view<View, Ts...>)
class LockFreeMPSCQueueSoA {
  public:
    static constexpr bool is_void_view{std::is_void_v<View>};
    static constexpr auto type_indexes{std::index_sequence_for<Ts...>{}};

    using size_type = std::size_t;
    using view_type = std::conditional_t<is_void_view, std::tuple<std::span<Ts>...>, View>;
    template <size_type I>
    using value_type = std::tuple_element_t<I, std::tuple<Ts...>>;
    using view_tuple = std::tuple<std::span<Ts>...>;

    explicit LockFreeMPSCQueueSoA(
        std::pmr::memory_resource* mr = std::pmr::get_default_resource()) noexcept
        : alloc_{mr} {}

    ~LockFreeMPSCQueueSoA() {
        if (buffer_ != nullptr) {
            destroy_all_buffers(active_read_buffer(), read_size_);

            auto const write_count{write_index_.load()};
            destroy_all_buffers(active_write_buffer(), write_count);

            alloc_.deallocate_bytes(buffer_, layout_.total_bytes, max_alignment);
        }
    }

    LockFreeMPSCQueueSoA(LockFreeMPSCQueueSoA const&) = delete;
    LockFreeMPSCQueueSoA& operator=(LockFreeMPSCQueueSoA const&) = delete;
    LockFreeMPSCQueueSoA(LockFreeMPSCQueueSoA&&) = delete;
    LockFreeMPSCQueueSoA& operator=(LockFreeMPSCQueueSoA&&) = delete;

    [[nodiscard]] auto is_initialised() const noexcept -> bool { return capacity_per_buffer_ > 0; }
    [[nodiscard]] auto buffer_capacity() const noexcept -> size_type {
        return capacity_per_buffer_;
    }
    [[nodiscard]] auto full_capacity() const noexcept -> size_type {
        return capacity_per_buffer_ * 2;
    }

    [[nodiscard]] auto init(size_type n) -> ELockFreeMPSCQueueInitResult {
        if (n == 0) {
            return ELockFreeMPSCQueueInitResult::Success;
        }

        if (is_initialised()) {
            return ELockFreeMPSCQueueInitResult::AlreadyInitialised;
        }

        layout_ = BufferLayout::compute(n);
        capacity_per_buffer_ = n;

        try {
            buffer_ =
                static_cast<std::byte*>(alloc_.allocate_bytes(layout_.total_bytes, max_alignment));
        } catch (...) {
            capacity_per_buffer_ = 0;
            return ELockFreeMPSCQueueInitResult::AllocationFailed;
        }

        return ELockFreeMPSCQueueInitResult::Success;
    }

    template <typename... Args>
        requires (sizeof...(Args) == sizeof...(Ts)) &&
                 (std::is_nothrow_constructible_v<Ts, Args &&> && ...)
    [[nodiscard]] auto enqueue(Args&&... args) noexcept -> ELockFreeMPSCQueueEnqueueResult {
        auto const address_result{get_next_write_index()};
        if (!address_result) {
            return address_result.error();
        }

        auto const buffer_idx{write_buffer_index_.load(std::memory_order_relaxed)};

        [&, idx = *address_result]<std::size_t... Is>(std::index_sequence<Is...>) {
            (construct_element<Is>(buffer_idx, idx, std::forward<Args>(args)), ...);
        }(type_indexes);

        return ELockFreeMPSCQueueEnqueueResult::Success;
    }

    [[nodiscard]] auto swap_and_consume() noexcept -> view_type {
        // Swap buffers
        auto const new_read_size{write_index_.exchange(0, std::memory_order_acquire)};
        auto const old_read_size{read_size_};
        read_size_ = new_read_size;

        auto const old_write_buffer{write_buffer_index_.load(std::memory_order_acquire)};
        write_buffer_index_.store(1 - old_write_buffer, std::memory_order_release);

        // Destroy old read buffer (which becomes the new write buffer)
        destroy_all_buffers(1 - old_write_buffer, old_read_size);

        return map_index<view_type>([&]<std::size_t I>() {
            return std::span<value_type<I>>(get_buffer_ptr<I>(old_write_buffer), new_read_size);
        });
    }

    template <typename Callable>
        requires std::invocable<Callable, view_type>
    [[nodiscard]] decltype(auto) swap_and_visit(Callable&& callable) {
        return std::forward<Callable>(callable)(swap_and_consume());
    }
  private:
    struct BufferLayout {
        size_type total_bytes{0};
        std::array<size_type, sizeof...(Ts)> offsets{};

        static auto compute(size_type capacity_per_buffer) -> BufferLayout {
            BufferLayout layout{};
            size_type offset{0};
            size_type idx{0};

            auto align_and_add = [&]<typename T>() {
                // Align offset to T's alignment requirement
                constexpr auto alignment{alignof(T)};
                offset = (offset + alignment - 1) & ~(alignment - 1);

                layout.offsets[idx++] = offset;

                // Add space for 2 buffers of T
                offset += 2 * capacity_per_buffer * sizeof(T);
            };

            (align_and_add.template operator()<Ts>(), ...);

            layout.total_bytes = offset;
            return layout;
        }
    };

    template <size_type I>
    [[nodiscard]] auto get_buffer_ptr(size_type buffer_idx) noexcept -> auto* {
        using T = value_type<I>;
        auto const buffer_start{layout_.offsets[I] + buffer_idx * capacity_per_buffer_ * sizeof(T)};
        return std::launder(reinterpret_cast<T*>(buffer_ + buffer_start));
    }

    template <size_type I>
    void construct_element(size_type active_write_buffer_idx,
                           size_type element_index,
                           auto&& value) noexcept {
        auto* ptr{get_buffer_ptr<I>(active_write_buffer_idx) + element_index};
        new (ptr) value_type<I>{std::forward<decltype(value)>(value)};
    }

    template <size_type I>
    void destroy_buffer(size_type buffer_idx, size_type count) noexcept {
        if constexpr (!std::is_trivially_destructible_v<value_type<I>>) {
            auto* ptr{get_buffer_ptr<I>(buffer_idx)};
            std::destroy_n(ptr, count);
        }
    }

    void destroy_all_buffers(size_type buffer_idx, size_type count) noexcept {
        for_each_index([&]<std::size_t I>() { destroy_buffer<I>(buffer_idx, count); });
    }

    [[nodiscard]] auto get_next_write_index() noexcept
        -> std::expected<size_type, ELockFreeMPSCQueueEnqueueResult> {
        if (!is_initialised()) {
            return std::unexpected(ELockFreeMPSCQueueEnqueueResult::Uninitialised);
        }

        // Compare-and-swap loop to reserve an index
        auto i{write_index_.load(std::memory_order_relaxed)};
        for (;;) {
            if (i >= buffer_capacity()) {
                return std::unexpected(ELockFreeMPSCQueueEnqueueResult::Full);
            }

            if (write_index_.compare_exchange_weak(
                    i, i + 1, std::memory_order_release, std::memory_order_relaxed)) {
                break;
            }
        }

        return i;
    }

    // Index helpers
    [[nodiscard]] size_type active_write_buffer() const noexcept {
        return write_buffer_index_.load(std::memory_order_relaxed);
    }
    [[nodiscard]] size_type active_read_buffer() const noexcept {
        return 1 - write_buffer_index_.load(std::memory_order_relaxed);
    }

    // Tuple iteration
    template <typename Fn, std::size_t... Is>
    constexpr void for_each_index_impl(Fn&& fn, std::index_sequence<Is...>) {
        (fn.template operator()<Is>(), ...);
    }

    template <typename Fn>
    constexpr void for_each_index(Fn&& fn) {
        for_each_index_impl(std::forward<Fn>(fn), type_indexes);
    }

    template <typename Container, typename Fn, std::size_t... Is>
    constexpr auto map_index_impl(Fn&& fn, std::index_sequence<Is...>) {
        return Container { fn.template operator()<Is>()... };
    }

    template <typename Container, typename Fn>
    constexpr auto map_index(Fn&& fn) {
        return map_index_impl<Container>(std::forward<Fn>(fn), type_indexes);
    }

    static constexpr size_type max_alignment{std::max({alignof(Ts)...})};
    static constexpr size_type cache_line_size_bytes{64};

    std::pmr::polymorphic_allocator<std::byte> alloc_;
    std::byte* buffer_{nullptr};
    BufferLayout layout_{};
    size_type capacity_per_buffer_{0};
    size_type read_size_{0};

    alignas(cache_line_size_bytes) std::atomic<size_type> write_buffer_index_{0};
    alignas(cache_line_size_bytes) std::atomic<size_type> write_index_{0};
};

}
