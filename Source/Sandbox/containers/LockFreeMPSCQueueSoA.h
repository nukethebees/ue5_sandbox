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

namespace ml {

enum class ELockFreeMPSCQueueSoAInitResult : std::uint8_t {
    Success,
    AlreadyInitialised,
    AllocationFailed
};

enum class ELockFreeMPSCQueueSoAEnqueueResult : std::uint8_t { Success, Full, Uninitialised };

// Lock-free multi-producer single-consumer queue with Structure of Arrays layout
// Contract: swap_and_consume() must only be called when all enqueue() operations are complete
template <typename View = void, typename... Ts>
    requires (sizeof...(Ts) > 0) && (std::is_nothrow_move_constructible_v<Ts> && ...)
class LockFreeMPSCQueueSoA {
  public:
    using size_type = std::size_t;

    template <typename V = View>
    using view_type = std::conditional_t<std::is_void_v<V>, std::tuple<std::span<Ts>...>, V>;

    explicit LockFreeMPSCQueueSoA(
        std::pmr::memory_resource* mr = std::pmr::get_default_resource()) noexcept
        : alloc_{mr} {}

    ~LockFreeMPSCQueueSoA() {
        if (buffer_ != nullptr) {
            // Destroy elements in read buffer
            auto const read_buffer_idx{1 - write_buffer_index_.load()};
            destroy_all_buffers(read_buffer_idx, read_size_);

            // Destroy elements in write buffer
            auto const write_buffer_idx{write_buffer_index_.load()};
            auto const write_count{write_index_.load()};
            destroy_all_buffers(write_buffer_idx, write_count);

            // Deallocate memory
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

    [[nodiscard]] auto init(size_type n) -> ELockFreeMPSCQueueSoAInitResult {
        if (n == 0) {
            return ELockFreeMPSCQueueSoAInitResult::Success;
        }

        if (is_initialised()) {
            return ELockFreeMPSCQueueSoAInitResult::AlreadyInitialised;
        }

        layout_ = BufferLayout::compute(n);
        capacity_per_buffer_ = n;

        try {
            buffer_ =
                static_cast<std::byte*>(alloc_.allocate_bytes(layout_.total_bytes, max_alignment));
        } catch (...) {
            capacity_per_buffer_ = 0;
            return ELockFreeMPSCQueueSoAInitResult::AllocationFailed;
        }

        return ELockFreeMPSCQueueSoAInitResult::Success;
    }

    template <typename... Args>
        requires (sizeof...(Args) == sizeof...(Ts)) &&
                 (std::is_same_v<std::remove_cvref_t<Args>, Ts> && ...)
    [[nodiscard]] auto enqueue(Args&&... args) noexcept -> ELockFreeMPSCQueueSoAEnqueueResult {
        auto address_result{get_next_write_index()};
        if (!address_result) {
            return address_result.error();
        }

        auto const idx{*address_result};
        auto const buffer_idx{write_buffer_index_.load(std::memory_order_relaxed)};

        // Construct each element in its respective buffer
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            (construct_element<Is>(buffer_idx, idx, std::move(args)), ...);
        }(std::index_sequence_for<Ts...>{});

        return ELockFreeMPSCQueueSoAEnqueueResult::Success;
    }

    template <typename V = View>
        requires std::is_void_v<V> || std::constructible_from<V, std::span<Ts>...>
    [[nodiscard]] auto swap_and_consume() noexcept -> view_type<V> {
        // Swap buffers
        auto const new_read_size{write_index_.exchange(0, std::memory_order_acquire)};
        auto const old_read_size{read_size_};
        read_size_ = new_read_size;

        auto const old_write_buffer{write_buffer_index_.load(std::memory_order_acquire)};
        write_buffer_index_.store(1 - old_write_buffer, std::memory_order_release);

        // Destroy old read buffer (which becomes the new write buffer)
        destroy_all_buffers(1 - old_write_buffer, old_read_size);

        // Create spans for the new read buffer
        auto spans{make_spans(old_write_buffer, new_read_size)};

        if constexpr (std::is_void_v<V>) {
            return spans;
        } else {
            return std::apply([](auto... s) { return V{s...}; }, spans);
        }
    }

    template <typename Callable>
        requires std::invocable<Callable, view_type<View>>
    [[nodiscard]] decltype(auto) swap_and_visit(Callable&& callable) {
        return std::forward<Callable>(callable)(swap_and_consume<View>());
    }
  private:
    struct BufferLayout {
        size_type total_bytes{0};
        std::array<size_type, sizeof...(Ts)> offsets{};

        static consteval auto compute(size_type capacity_per_buffer) -> BufferLayout {
            BufferLayout layout{};
            size_type offset{0};
            size_type idx{0};

            auto align_and_add = [&]<typename T>() {
                // Align offset to T's alignment requirement
                auto const alignment{alignof(T)};
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

    template <std::size_t I>
    [[nodiscard]] auto get_buffer_ptr(size_type buffer_idx) const noexcept
        -> std::tuple_element_t<I, std::tuple<Ts*, Ts*...>> {
        using T = std::tuple_element_t<I, std::tuple<Ts...>>;
        auto const buffer_start{layout_.offsets[I] + buffer_idx * capacity_per_buffer_ * sizeof(T)};
        return std::launder(reinterpret_cast<T*>(buffer_ + buffer_start));
    }

    template <std::size_t I>
    void construct_element(size_type buffer_idx, size_type item_idx, auto&& arg) noexcept {
        using T = std::tuple_element_t<I, std::tuple<Ts...>>;
        auto* ptr{get_buffer_ptr<I>(buffer_idx) + item_idx};
        new (ptr) T{std::forward<decltype(arg)>(arg)};
    }

    template <std::size_t I>
    void destroy_buffer(size_type buffer_idx, size_type count) noexcept {
        using T = std::tuple_element_t<I, std::tuple<Ts...>>;
        if constexpr (!std::is_trivially_destructible_v<T>) {
            auto* ptr{get_buffer_ptr<I>(buffer_idx)};
            std::destroy_n(ptr, count);
        }
    }

    void destroy_all_buffers(size_type buffer_idx, size_type count) noexcept {
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            (destroy_buffer<Is>(buffer_idx, count), ...);
        }(std::index_sequence_for<Ts...>{});
    }

    [[nodiscard]] auto make_spans(size_type buffer_idx, size_type count) const noexcept
        -> std::tuple<std::span<Ts>...> {
        return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            return std::tuple{std::span<Ts>{get_buffer_ptr<Is>(buffer_idx), count}...};
        }(std::index_sequence_for<Ts...>{});
    }

    [[nodiscard]] auto get_next_write_index() noexcept
        -> std::expected<size_type, ELockFreeMPSCQueueSoAEnqueueResult> {
        if (!is_initialised()) {
            return std::unexpected(ELockFreeMPSCQueueSoAEnqueueResult::Uninitialised);
        }

        // Compare-and-swap loop to reserve an index
        auto i{write_index_.load(std::memory_order_relaxed)};
        for (;;) {
            if (i >= buffer_capacity()) {
                return std::unexpected(ELockFreeMPSCQueueSoAEnqueueResult::Full);
            }

            if (write_index_.compare_exchange_weak(
                    i, i + 1, std::memory_order_release, std::memory_order_relaxed)) {
                break;
            }
        }

        return i;
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

} // namespace ml
