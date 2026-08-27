#pragma once

#include <Containers/ArrayView.h>
#include <HAL/Platform.h>
#include <Misc/AssertionMacros.h>

#include <limits>
#include <memory_resource>
#include <utility>
#include <vector>

namespace ml {
// Contiguous frame-local array with fixed identity. The memory resource is borrowed and must
// outlive the array. Views, references, and iterators are invalidated by storage reallocation.
template <typename T>
class TFrameArray {
  public:
    TFrameArray()
        : TFrameArray{std::pmr::new_delete_resource()} {}

    explicit TFrameArray(std::pmr::memory_resource* const resource)
        : values_{checked_resource(resource)} {}

    TFrameArray(TFrameArray const&) = delete;
    TFrameArray(TFrameArray&&) = delete;
    auto operator=(TFrameArray const&) -> TFrameArray& = delete;
    auto operator=(TFrameArray&&) -> TFrameArray& = delete;
    ~TFrameArray() = default;

    operator TArrayView<T>() noexcept { return view(); }
    operator TConstArrayView<T>() const noexcept { return view(); }

    auto num() const noexcept -> int32 { return to_public_size(values_.size()); }
    auto is_empty() const noexcept -> bool { return values_.empty(); }

    void reserve(int32 const count) { values_.reserve(to_storage_size(count)); }
    void clear() noexcept { values_.clear(); }

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

    auto operator[](int32 const index) noexcept -> T& { return values_[checked_index(index)]; }
    auto operator[](int32 const index) const noexcept -> T const& {
        return values_[checked_index(index)];
    }

    auto data() noexcept -> T* { return values_.data(); }
    auto data() const noexcept -> T const* { return values_.data(); }

    auto view() noexcept -> TArrayView<T> { return {data(), num()}; }
    auto view() const noexcept -> TConstArrayView<T> { return {data(), num()}; }

    auto begin() noexcept { return values_.begin(); }
    auto begin() const noexcept { return values_.begin(); }
    auto end() noexcept { return values_.end(); }
    auto end() const noexcept { return values_.end(); }
  private:
    using storage_type = std::pmr::vector<T>;
    using storage_size_type = typename storage_type::size_type;

    static constexpr storage_size_type max_supported_size{
        static_cast<storage_size_type>(std::numeric_limits<int32>::max())};

    static auto checked_resource(std::pmr::memory_resource* const resource)
        -> std::pmr::memory_resource* {
        check(resource != nullptr);
        return resource;
    }

    static auto to_storage_size(int32 const count) -> storage_size_type {
        check(count >= 0);
        return static_cast<storage_size_type>(count);
    }

    static auto to_public_size(storage_size_type const count) noexcept -> int32 {
        check(count <= max_supported_size);
        return static_cast<int32>(count);
    }

    auto checked_index(int32 const index) const noexcept -> storage_size_type {
        check(index >= 0);
        auto const storage_index{static_cast<storage_size_type>(index)};
        check(storage_index < values_.size());
        return storage_index;
    }

    void check_can_add() const noexcept { check(values_.size() < max_supported_size); }

    storage_type values_;
};
}
