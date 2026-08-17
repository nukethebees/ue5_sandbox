#pragma once

#include <HAL/Platform.h>
#include <Misc/AssertionMacros.h>
#include <Templates/UnrealTemplate.h>

#include <cstddef>
#include <initializer_list>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace ml {
// Fixed-capacity, contiguous array with inline storage for up to N elements.
// Adding beyond capacity is a programming error and triggers a check.
template <typename T, int32 N>
    requires (N >= 0)
class TFixedArray {
  public:
    using value_type = T;
    using size_type = int32;
    using iterator = value_type*;
    using const_iterator = value_type const*;

    static constexpr size_type capacity_value{N};

    TFixedArray() = default;

    TFixedArray(std::initializer_list<value_type> const values) {
        auto const value_count{static_cast<size_type>(values.size())};
        check_has_sufficient_capacity(value_count);

        for (value_type const& value : values) {
            add(value);
        }
    }

    TFixedArray(TFixedArray const& other)
        requires std::is_copy_constructible_v<value_type>
    {
        for (value_type const& value : other) {
            add(value);
        }
    }

    TFixedArray(TFixedArray const& other)
        requires (!std::is_copy_constructible_v<value_type>)
    = delete;

    TFixedArray(TFixedArray&& other) noexcept(std::is_nothrow_move_constructible_v<value_type>)
        requires std::is_move_constructible_v<value_type>
    {
        for (value_type& value : other) {
            add(MoveTemp(value));
        }

        other.reset();
    }

    TFixedArray(TFixedArray&& other)
        requires (!std::is_move_constructible_v<value_type>)
    = delete;

    ~TFixedArray() { reset(); }

    auto operator=(TFixedArray const& other) -> TFixedArray&
        requires std::is_copy_constructible_v<value_type>
    {
        if (this != std::addressof(other)) {
            reset();

            for (value_type const& value : other) {
                add(value);
            }
        }

        return *this;
    }

    auto operator=(TFixedArray const& other) -> TFixedArray&
        requires (!std::is_copy_constructible_v<value_type>)
    = delete;

    auto operator=(TFixedArray&& other) noexcept(std::is_nothrow_move_constructible_v<value_type>)
        -> TFixedArray&
        requires std::is_move_constructible_v<value_type>
    {
        if (this != std::addressof(other)) {
            reset();

            for (value_type& value : other) {
                add(MoveTemp(value));
            }

            other.reset();
        }

        return *this;
    }

    auto operator=(TFixedArray&& other) -> TFixedArray&
        requires (!std::is_move_constructible_v<value_type>)
    = delete;

    operator TArrayView<T>() { return {data(), size_}; }

    operator TArrayView<T const>() const { return {data(), size_}; }

    auto num() const noexcept -> size_type { return size_; }
    static constexpr auto capacity() noexcept -> size_type { return capacity_value; }
    auto is_empty() const noexcept -> bool { return size_ == 0; }
    auto is_full() const noexcept -> bool { return size_ == capacity(); }

    auto data() noexcept -> value_type* { return value_at_storage(0); }
    auto data() const noexcept -> value_type const* { return value_at_storage(0); }

    auto begin() noexcept -> iterator { return data(); }
    auto begin() const noexcept -> const_iterator { return data(); }
    auto cbegin() const noexcept -> const_iterator { return data(); }
    auto end() noexcept -> iterator { return data() + size_; }
    auto end() const noexcept -> const_iterator { return data() + size_; }
    auto cend() const noexcept -> const_iterator { return data() + size_; }

    auto operator[](size_type const index) noexcept -> value_type& {
        check_index(index);
        return *value_at_storage(index);
    }

    auto operator[](size_type const index) const noexcept -> value_type const& {
        check_index(index);
        return *value_at_storage(index);
    }

    auto first() noexcept -> value_type& { return (*this)[0]; }
    auto first() const noexcept -> value_type const& { return (*this)[0]; }
    auto last() noexcept -> value_type& { return (*this)[size_ - 1]; }
    auto last() const noexcept -> value_type const& { return (*this)[size_ - 1]; }

    void reserve(size_type const requested_capacity) const {
        check(requested_capacity >= 0);
        check(requested_capacity <= capacity());
    }

    auto add(value_type const& value) -> size_type {
        auto const index{size_};
        emplace_back(value);
        return index;
    }

    auto add(value_type&& value) -> size_type {
        auto const index{size_};
        emplace_back(MoveTemp(value));
        return index;
    }

    template <typename... TArgs>
    auto emplace_back(TArgs&&... args) -> value_type& {
        check_has_sufficient_capacity(1);

        value_type* const value{value_at_storage(size_)};
        std::construct_at(value, std::forward<TArgs>(args)...);
        ++size_;
        return *value;
    }

    void add_defaulted(size_type const count = 1)
        requires std::is_default_constructible_v<value_type>
    {
        check_has_sufficient_capacity(count);

        for (size_type i{0}; i < count; ++i) {
            emplace_back();
        }
    }

    void set_num(size_type const new_size)
        requires std::is_default_constructible_v<value_type>
    {
        check(new_size >= 0);

        if (new_size < size_) {
            destroy_from(new_size);
            return;
        }

        add_defaulted(new_size - size_);
    }

    void pop() {
        check(!is_empty());
        --size_;
        std::destroy_at(value_at_storage(size_));
    }

    void reset() noexcept { destroy_from(0); }
  private:
    static constexpr size_type storage_count{N > 0 ? N : 1};

    auto value_at_storage(size_type const index) noexcept -> value_type* {
        return reinterpret_cast<value_type*>(storage_ + sizeof(value_type) * index);
    }

    auto value_at_storage(size_type const index) const noexcept -> value_type const* {
        return reinterpret_cast<value_type const*>(storage_ + sizeof(value_type) * index);
    }

    void check_index(size_type const index) const {
        check(index >= 0);
        check(index < size_);
    }

    void check_has_sufficient_capacity(size_type const count) const {
        check(count >= 0);
        check(count <= capacity() - size_);
    }

    void destroy_from(size_type const first_index) noexcept {
        for (size_type i{size_}; i > first_index; --i) {
            std::destroy_at(value_at_storage(i - 1));
        }

        size_ = first_index;
    }

    alignas(value_type) std::byte storage_[sizeof(value_type) * storage_count];
    size_type size_{0};
};
}
