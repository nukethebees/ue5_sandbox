#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <memory_resource>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace ml {
// Contiguous frame-local array with fixed identity. The memory resource is borrowed and must
// outlive the array. Views, references, and iterators are invalidated by storage reallocation.
// Destroy the array before reclaiming its resource; retain no views beyond that lifetime.
// Require non-throwing relocation and destruction. Construct elements normally; pass any
// resource needed by their internal allocations explicitly to their constructors.
template <typename T>
    requires (std::is_nothrow_move_constructible_v<T> && std::is_nothrow_destructible_v<T>)
class FrameArray {
  public:
    explicit FrameArray(std::pmr::memory_resource* const resource)
        : resource_{resource} {
        assert(resource != nullptr);
    }

    FrameArray(FrameArray const&) = delete;
    FrameArray(FrameArray&&) = delete;
    auto operator=(FrameArray const&) -> FrameArray& = delete;
    auto operator=(FrameArray&&) -> FrameArray& = delete;
    ~FrameArray() {
        clear();
        deallocate(data_, capacity_);
    }

    operator std::span<T>() noexcept { return view(); }
    operator std::span<T const>() const noexcept { return view(); }

    auto num() const noexcept -> std::uint32_t { return size_; }
    auto is_empty() const noexcept -> bool { return size_ == 0; }

    void reserve(std::uint32_t const count) {
        if (count <= capacity_) {
            return;
        }

        auto* const new_data{allocate(count)};
        replace_storage(new_data, count);
    }

    void set_num(std::uint32_t const count)
        requires std::is_default_constructible_v<T>
    {
        if (count <= size_) {
            destroy_from(count);
            return;
        }

        if (count <= capacity_) {
            std::uninitialized_value_construct_n(data_ + size_, count - size_);
        } else {
            auto const new_capacity{growth_capacity(count)};
            auto* const new_data{allocate(new_capacity)};
            try {
                // Finish potentially throwing construction before moving existing values.
                std::uninitialized_value_construct_n(new_data + size_, count - size_);
            } catch (...) {
                deallocate(new_data, new_capacity);
                throw;
            }
            replace_storage(new_data, new_capacity);
        }

        size_ = count;
    }

    // Construct every added element before any operation that reads, moves, or destroys it.
    void set_num_uninitialised(std::uint32_t const count) {
        if (count <= size_) {
            destroy_from(count);
            return;
        }

        if (count > capacity_) {
            reserve(growth_capacity(count));
        }

        size_ = count;
    }

    void clear() noexcept { destroy_from(0); }

    void remove_at_swap(std::uint32_t const index)
        requires std::is_nothrow_move_assignable_v<T>
    {
        assert(index < size_);
        if (index + 1 != size_) {
            data_[index] = std::move(data_[size_ - 1]);
        }
        destroy_from(size_ - 1);
    }

    auto add(T const& value) -> T&
        requires std::is_copy_constructible_v<T>
    {
        return emplace(value);
    }

    auto add(T&& value) -> T& { return emplace(std::move(value)); }

    // Require source and indices to be disjoint from this array's storage.
    void add(std::span<T const> const source, std::span<std::uint32_t const> const indices)
        requires std::is_nothrow_copy_constructible_v<T>
    {
        assert(indices.size() <= max_supported_size - size_);
        if (indices.empty()) {
            return;
        }

        assert(!overlaps_storage(std::as_bytes(source)));
        assert(!overlaps_storage(std::as_bytes(indices)));

        auto const count{static_cast<std::uint32_t>(indices.size())};
        auto const first{size_};
        set_num_uninitialised(first + count);

        for (std::uint32_t index{}; index < count; ++index) {
            auto const source_index{indices[index]};
            assert(source_index < source.size());
            std::construct_at(data_ + first + index, source[source_index]);
        }
    }

    template <typename... Args>
        requires std::is_constructible_v<T, Args...>
    auto emplace(Args&&... args) -> T& {
        if (size_ == max_supported_size) {
            throw std::length_error{"FrameArray size exceeds its supported range"};
        }

        if (size_ < capacity_) {
            std::construct_at(data_ + size_, std::forward<Args>(args)...);
        } else {
            auto const new_capacity{growth_capacity(size_ + 1)};
            auto* const new_data{allocate(new_capacity)};
            try {
                // Consume arguments before relocation invalidates references into the array.
                std::construct_at(new_data + size_, std::forward<Args>(args)...);
            } catch (...) {
                deallocate(new_data, new_capacity);
                throw;
            }
            replace_storage(new_data, new_capacity);
        }

        return data_[size_++];
    }

    auto operator[](std::uint32_t const index) noexcept -> T& {
        assert(index < size_);
        return data_[index];
    }
    auto operator[](std::uint32_t const index) const noexcept -> T const& {
        assert(index < size_);
        return data_[index];
    }

    auto data() noexcept -> T* { return data_; }
    auto data() const noexcept -> T const* { return data_; }

    auto view() noexcept -> std::span<T> { return {data(), static_cast<std::size_t>(num())}; }
    auto view() const noexcept -> std::span<T const> {
        return {data(), static_cast<std::size_t>(num())};
    }

    auto begin() noexcept -> T* { return data_; }
    auto begin() const noexcept -> T const* { return data_; }
    auto end() noexcept -> T* { return size_ == 0 ? data_ : data_ + size_; }
    auto end() const noexcept -> T const* { return size_ == 0 ? data_ : data_ + size_; }
  private:
    static constexpr auto max_supported_size{static_cast<std::uint32_t>(std::min(
        {static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()),
         std::numeric_limits<std::size_t>::max() / sizeof(T),
         static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max()) / sizeof(T)}))};

    auto overlaps_storage(std::span<std::byte const> const bytes) const noexcept -> bool {
        if (bytes.empty() || capacity_ == 0) {
            return false;
        }

        auto const storage_begin{reinterpret_cast<std::uintptr_t>(data_)};
        auto const storage_end{storage_begin + static_cast<std::size_t>(capacity_) * sizeof(T)};
        auto const source_begin{reinterpret_cast<std::uintptr_t>(bytes.data())};
        auto const source_end{source_begin + bytes.size()};
        return source_begin < storage_end && storage_begin < source_end;
    }

    static void check_size(std::uint32_t const count) {
        if (count > max_supported_size) {
            throw std::length_error{"FrameArray size exceeds its supported range"};
        }
    }

    auto growth_capacity(std::uint32_t const count) const -> std::uint32_t {
        check_size(count);
        auto const doubled{capacity_ > max_supported_size / 2 ? max_supported_size : capacity_ * 2};
        return std::max(count, doubled);
    }

    auto allocate(std::uint32_t const count) -> T* {
        check_size(count);
        return static_cast<T*>(
            resource_->allocate(static_cast<std::size_t>(count) * sizeof(T), alignof(T)));
    }

    void deallocate(T* const data, std::uint32_t const capacity) noexcept {
        if (data != nullptr) {
            resource_->deallocate(data, static_cast<std::size_t>(capacity) * sizeof(T), alignof(T));
        }
    }

    void destroy_from(std::uint32_t const first) noexcept {
        while (size_ > first) {
            std::destroy_at(data_ + --size_);
        }
    }

    void replace_storage(T* const new_data, std::uint32_t const new_capacity) noexcept {
        auto const count{size_};
        for (std::uint32_t index{}; index < count; ++index) {
            std::construct_at(new_data + index, std::move(data_[index]));
        }
        clear();
        deallocate(data_, capacity_);

        data_ = new_data;
        capacity_ = new_capacity;
        size_ = count;
    }

    std::pmr::memory_resource* resource_;
    T* data_{};
    std::uint32_t size_{};
    std::uint32_t capacity_{};
};
}
