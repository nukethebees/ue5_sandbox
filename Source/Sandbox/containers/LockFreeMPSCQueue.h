#pragma once

#include <atomic>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

enum class ELockFreeMPSCQueueInitResult : std::uint8_t { Success, AlreadyInitialised };
enum class ELockFreeMPSCQueueEnqueueResult : std::uint8_t { Success, Full, Uninitialised };

// Lock-free multi-producer single-consumer queue
// Contract: consume() must only be called when all enqueue() operations are complete
// (e.g., at end of frame after all producers have finished)
template <typename T, typename Allocator = std::allocator<T>>
class LockFreeMPSCQueue {
  public:
    using AllocTraits = std::allocator_traits<Allocator>;
    using value_type = typename AllocTraits::value_type;
    using allocator_type = Allocator;
    using size_type = typename AllocTraits::size_type;
    using difference_type = typename AllocTraits::difference_type;
    using pointer = typename AllocTraits::pointer;
    using const_pointer = typename AllocTraits::const_pointer;
    using reference = value_type&;
    using const_reference = value_type const&;

    explicit LockFreeMPSCQueue(Allocator alloc = Allocator{})
        : allocator_{std::move(alloc)} {}

    ~LockFreeMPSCQueue() {
        // Destroy objects in read buffer
        auto* read_buffer_start{get_address(1 - active_buffer_.load(), 0)};
        for (std::size_t i{0}; i < read_size_; ++i) {
            AllocTraits::destroy(allocator_, read_buffer_start + i);
        }

        // Destroy objects in write buffer
        auto write_count{write_index_.load()};
        auto* write_buffer_start{get_address(active_buffer_.load(), 0)};
        for (std::size_t i{0}; i < write_count; ++i) {
            AllocTraits::destroy(allocator_, write_buffer_start + i);
        }

        AllocTraits::deallocate(allocator_, data_, 2 * capacity_per_buffer_);
    }

    LockFreeMPSCQueue(LockFreeMPSCQueue const&) = delete;
    LockFreeMPSCQueue& operator=(LockFreeMPSCQueue const&) = delete;
    LockFreeMPSCQueue(LockFreeMPSCQueue&&) = delete;
    LockFreeMPSCQueue& operator=(LockFreeMPSCQueue&&) = delete;

    auto full_capacity() const { return buffer_capacity() * 2; }
    auto buffer_capacity() const { return capacity_per_buffer_; }
    auto is_initialised() const { return capacity_per_buffer_ > 0; }

    [[nodiscard]] auto init(size_type n) -> ELockFreeMPSCQueueInitResult {
        if (n == 0) {
            return ELockFreeMPSCQueueInitResult::Success;
        }

        if (is_initialised()) {
            return ELockFreeMPSCQueueInitResult::AlreadyInitialised;
        }

        data_ = AllocTraits::allocate(allocator_, full_capacity());
        return ELockFreeMPSCQueueInitResult::Success;
    }

    template <typename U>
        requires std::same_as<U, T>
    [[nodiscard]] auto enqueue(U&& value) -> ELockFreeMPSCQueueEnqueueResult {
        if (!is_initialised()) {
            return ELockFreeMPSCQueueEnqueueResult::Uninitialised;
        }

        auto const i{write_index_.fetch_add(1, std::memory_order_acquire)};
        if (i >= capacity_per_buffer_) {
            return ELockFreeMPSCQueueEnqueueResult::Full;
        }

        auto* const address{get_address(active_buffer_.load(std::memory_order_acquire), i)};
        new (address) T{std::forward<U>(value)};
        return ELockFreeMPSCQueueEnqueueResult::Success;
    }

    [[nodiscard]] std::span<T> consume() {
        auto const new_read_size{write_index_.load(std::memory_order_acquire)};
        auto const old_read_size{read_size_};
        read_size_ = new_read_size;

        auto const old_active_buffer{active_buffer_.load(std::memory_order_acquire)};
        active_buffer_.store(1 - old_active_buffer, std::memory_order_release);
        write_index_.store(0, std::memory_order_release);

        auto const* const new_write_buffer{get_address(1 - old_active_buffer, 0)};
        auto const* const new_read_buffer{get_address(old_active_buffer, 0)};

        destroy_buffer(new_write_buffer, old_read_size);

        return std::span<T>{new_read_buffer, new_read_size};
    }
  private:
    inline T* get_address(std::size_t buffer_index, std::size_t item_index) const noexcept {
        return data_ + (buffer_index * buffer_capacity() + item_index);
    }
    void destroy_buffer(pointer ptr, size_type n) {
        for (std::size_t i{0}; i < n; ++i) {
            AllocTraits::destroy(allocator_, ptr + i);
        }
    }

    T* data_{nullptr};
    std::size_t const capacity_per_buffer_{0};
    std::atomic_size_t write_index_{0};
    std::size_t read_size_{0};
    std::atomic_size_t active_buffer_{0};
    [[no_unique_address]] Allocator allocator_{};
};
