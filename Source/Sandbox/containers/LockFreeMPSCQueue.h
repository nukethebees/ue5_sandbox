#pragma once

#include <atomic>
#include <concepts>
#include <cstddef>
#include <memory>
#include <span>

// Lock-free multi-producer single-consumer queue
// Contract: consume() must only be called when all enqueue() operations are complete
// (e.g., at end of frame after all producers have finished)
template <typename T, typename Allocator = std::allocator<T>>
class LockFreeMPSCQueue {
  public:
    explicit LockFreeMPSCQueue(std::size_t capacity, Allocator alloc = Allocator{})
        : capacity_per_buffer_{capacity}
        , allocator_{std::move(alloc)} {
        data_ = allocator_.allocate(2 * capacity_per_buffer_);
    }

    ~LockFreeMPSCQueue() {
        // Destroy objects in read buffer
        auto* read_buffer_start{get_address(1 - active_buffer_.load(), 0)};
        for (std::size_t i{0}; i < read_size_; ++i) {
            std::destroy_at(read_buffer_start + i);
        }

        // Destroy objects in write buffer
        auto write_count{write_index_.load()};
        auto* write_buffer_start{get_address(active_buffer_.load(), 0)};
        for (std::size_t i{0}; i < write_count; ++i) {
            std::destroy_at(write_buffer_start + i);
        }

        allocator_.deallocate(data_, 2 * capacity_per_buffer_);
    }

    LockFreeMPSCQueue(LockFreeMPSCQueue const&) = delete;
    LockFreeMPSCQueue& operator=(LockFreeMPSCQueue const&) = delete;
    LockFreeMPSCQueue(LockFreeMPSCQueue&&) = delete;
    LockFreeMPSCQueue& operator=(LockFreeMPSCQueue&&) = delete;

    template <typename U>
        requires std::same_as<U, T>
    [[nodiscard]] bool enqueue(U&& value) {
        auto index{write_index_.fetch_add(1, std::memory_order_acquire)};
        if (index >= capacity_per_buffer_) {
            return false;
        }

        auto* address{get_address(active_buffer_.load(std::memory_order_acquire), index)};
        new (address) T(std::forward<U>(value));
        return true;
    }

    [[nodiscard]] std::span<T> consume() {
        auto new_read_size{write_index_.load(std::memory_order_acquire)};
        auto old_read_size{read_size_};
        read_size_ = new_read_size;

        auto old_active_buffer{active_buffer_.load(std::memory_order_acquire)};
        active_buffer_.store(1 - old_active_buffer, std::memory_order_release);
        write_index_.store(0, std::memory_order_release);

        // Destroy objects in the new write buffer (was the old read buffer)
        auto* new_write_buffer{get_address(1 - old_active_buffer, 0)};
        for (std::size_t i{0}; i < old_read_size; ++i) {
            std::destroy_at(new_write_buffer + i);
        }

        // Return span of old write buffer (now the read buffer)
        return std::span<T>{get_address(old_active_buffer, 0), new_read_size};
    }
  private:
    inline T* get_address(std::size_t buffer_index, std::size_t item_index) const noexcept {
        return data_ + (buffer_index * capacity_per_buffer_ + item_index);
    }

    T* data_{nullptr};
    std::size_t const capacity_per_buffer_;
    std::atomic_size_t write_index_{0};
    std::size_t read_size_{0};
    std::atomic_size_t active_buffer_{0};
    [[no_unique_address]] Allocator allocator_;
};
