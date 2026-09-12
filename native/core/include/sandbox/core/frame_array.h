#pragma once

#include <cassert>
#include <cstdint>
#include <limits>
#include <memory_resource>
#include <span>
#include <utility>
#include <vector>

namespace ml {
// Contiguous frame-local array with fixed identity. The memory resource is borrowed and must
// outlive the array. Views, references, and iterators are invalidated by storage reallocation.
template <typename T>
class FrameArray {
  public:
    FrameArray()
        : FrameArray{std::pmr::new_delete_resource()} {}

    explicit FrameArray(std::pmr::memory_resource* const resource)
        : values_{checked_resource(resource)} {}

    FrameArray(FrameArray const&) = delete;
    FrameArray(FrameArray&&) = delete;
    auto operator=(FrameArray const&) -> FrameArray& = delete;
    auto operator=(FrameArray&&) -> FrameArray& = delete;
    ~FrameArray() = default;

    operator std::span<T>() noexcept { return view(); }
    operator std::span<T const>() const noexcept { return view(); }

    auto num() const noexcept -> std::int32_t { return to_public_size(values_.size()); }
    auto is_empty() const noexcept -> bool { return values_.empty(); }

    void reserve(std::int32_t const count) { values_.reserve(to_storage_size(count)); }
    void set_num(std::int32_t const count) { values_.resize(to_storage_size(count)); }
    void clear() noexcept { values_.clear(); }

    void remove_at_swap(std::int32_t const index) {
        auto const storage_index{checked_index(index)};
        if (storage_index + 1 != values_.size()) {
            values_[storage_index] = std::move(values_.back());
        }
        values_.pop_back();
    }

    auto add(T const& value) -> T& {
        check_can_add();
        return values_.emplace_back(value);
    }

    auto add(T&& value) -> T& {
        check_can_add();
        return values_.emplace_back(std::move(value));
    }

    template <typename... Args>
    auto emplace(Args&&... args) -> T& {
        check_can_add();
        return values_.emplace_back(std::forward<Args>(args)...);
    }

    auto operator[](std::int32_t const index) noexcept -> T& {
        return values_[checked_index(index)];
    }
    auto operator[](std::int32_t const index) const noexcept -> T const& {
        return values_[checked_index(index)];
    }

    auto data() noexcept -> T* { return values_.data(); }
    auto data() const noexcept -> T const* { return values_.data(); }

    auto view() noexcept -> std::span<T> { return {data(), static_cast<std::size_t>(num())}; }
    auto view() const noexcept -> std::span<T const> {
        return {data(), static_cast<std::size_t>(num())};
    }

    auto begin() noexcept { return values_.begin(); }
    auto begin() const noexcept { return values_.begin(); }
    auto end() noexcept { return values_.end(); }
    auto end() const noexcept { return values_.end(); }
  private:
    using storage_type = std::pmr::vector<T>;
    using storage_size_type = typename storage_type::size_type;

    static constexpr storage_size_type max_supported_size{
        static_cast<storage_size_type>(std::numeric_limits<std::int32_t>::max())};

    static auto checked_resource(std::pmr::memory_resource* const resource)
        -> std::pmr::memory_resource* {
        assert(resource != nullptr);
        return resource;
    }

    static auto to_storage_size(std::int32_t const count) -> storage_size_type {
        assert(count >= 0);
        return static_cast<storage_size_type>(count);
    }

    static auto to_public_size(storage_size_type const count) noexcept -> std::int32_t {
        assert(count <= max_supported_size);
        return static_cast<std::int32_t>(count);
    }

    auto checked_index(std::int32_t const index) const noexcept -> storage_size_type {
        assert(index >= 0);
        auto const storage_index{static_cast<storage_size_type>(index)};
        assert(storage_index < values_.size());
        return storage_index;
    }

    void check_can_add() const noexcept { assert(values_.size() < max_supported_size); }

    storage_type values_;
};
}
