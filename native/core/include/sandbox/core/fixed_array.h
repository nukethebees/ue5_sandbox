#pragma once

#include <sandbox/core/fixed_storage.h>

#include <cassert>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>

namespace ml {
// Fixed-capacity, contiguous array with inline storage for up to N elements.
// Adding beyond capacity is a programming error and triggers a check.
template <typename T, std::int32_t N>
    requires (N >= 0)
class FixedArray {
  public:
    using value_type = T;
    using size_type = std::int32_t;
    using iterator = value_type*;
    using const_iterator = value_type const*;

    static constexpr size_type capacity_value{N};

    FixedArray() = default;

    FixedArray(std::initializer_list<value_type> const values) {
        auto const value_count{static_cast<size_type>(values.size())};
        check_has_sufficient_capacity(value_count);

        for (value_type const& value : values) {
            add(value);
        }
    }

    FixedArray(FixedArray const& other)
        requires std::is_copy_constructible_v<value_type>
    {
        for (value_type const& value : other) {
            add(value);
        }
    }

    FixedArray(FixedArray const& other)
        requires (!std::is_copy_constructible_v<value_type>)
    = delete;

    FixedArray(FixedArray&& other) noexcept(std::is_nothrow_move_constructible_v<value_type>)
        requires std::is_move_constructible_v<value_type>
    {
        for (value_type& value : other) {
            add(std::move(value));
        }

        other.reset();
    }

    FixedArray(FixedArray&& other)
        requires (!std::is_move_constructible_v<value_type>)
    = delete;

    ~FixedArray() { reset(); }

    auto operator=(FixedArray const& other) -> FixedArray&
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

    auto operator=(FixedArray const& other) -> FixedArray&
        requires (!std::is_copy_constructible_v<value_type>)
    = delete;

    auto operator=(FixedArray&& other) noexcept(std::is_nothrow_move_constructible_v<value_type>)
        -> FixedArray&
        requires std::is_move_constructible_v<value_type>
    {
        if (this != std::addressof(other)) {
            reset();

            for (value_type& value : other) {
                add(std::move(value));
            }

            other.reset();
        }

        return *this;
    }

    auto operator=(FixedArray&& other) -> FixedArray&
        requires (!std::is_move_constructible_v<value_type>)
    = delete;

    operator std::span<T>() { return {data(), static_cast<std::size_t>(size_)}; }
    operator std::span<T const>() const { return {data(), static_cast<std::size_t>(size_)}; }

    auto num() const noexcept -> size_type { return size_; }
    static constexpr auto capacity() noexcept -> size_type { return capacity_value; }
    auto is_empty() const noexcept -> bool { return size_ == 0; }
    auto is_full() const noexcept -> bool { return size_ == capacity(); }

    auto data() noexcept -> value_type* { return storage_.data(); }
    auto data() const noexcept -> value_type const* { return storage_.data(); }
    auto capacity_view() noexcept -> std::span<T> {
        return {data(), static_cast<std::size_t>(capacity())};
    }

    auto begin() noexcept -> iterator { return data(); }
    auto begin() const noexcept -> const_iterator { return data(); }
    auto cbegin() const noexcept -> const_iterator { return data(); }
    auto end() noexcept -> iterator { return data() + size_; }
    auto end() const noexcept -> const_iterator { return data() + size_; }
    auto cend() const noexcept -> const_iterator { return data() + size_; }

    auto operator[](size_type const index) noexcept -> value_type& {
        check_index(index);
        return storage_[index];
    }

    auto operator[](size_type const index) const noexcept -> value_type const& {
        check_index(index);
        return storage_[index];
    }

    auto first() noexcept -> value_type& { return (*this)[0]; }
    auto first() const noexcept -> value_type const& { return (*this)[0]; }
    auto last() noexcept -> value_type& { return (*this)[size_ - 1]; }
    auto last() const noexcept -> value_type const& { return (*this)[size_ - 1]; }

    void reserve(size_type const requested_capacity) const {
        (void)requested_capacity;
        assert(requested_capacity >= 0);
        assert(requested_capacity <= capacity());
    }

    auto add(value_type const& value) -> size_type {
        auto const index{size_};
        emplace_back(value);
        return index;
    }

    auto add(value_type&& value) -> size_type {
        auto const index{size_};
        emplace_back(std::move(value));
        return index;
    }

    template <typename... Args>
    auto emplace_back(Args&&... args) -> value_type& {
        check_has_sufficient_capacity(1);

        auto& value{storage_.construct_at(size_, std::forward<Args>(args)...)};
        ++size_;
        return value;
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
        assert(new_size >= 0);

        if (new_size < size_) {
            destroy_from(new_size);
            return;
        }

        add_defaulted(new_size - size_);
    }
    void set_num_uninitialised(size_type const new_size) {
        assert(new_size >= 0);
        assert(new_size <= capacity());

        if (new_size < size_) {
            destroy_from(new_size);
            return;
        }

        size_ = new_size;
    }

    void pop() {
        assert(!is_empty());
        --size_;
        storage_.destroy_at(size_);
    }

    void reset() noexcept { destroy_from(0); }
  private:
    void check_index(size_type const index) const {
        (void)index;
        assert(index >= 0);
        assert(index < size_);
    }

    void check_has_sufficient_capacity(size_type const count) const {
        (void)count;
        assert(count >= 0);
        assert(count <= capacity() - size_);
    }

    void destroy_from(size_type const first_index) noexcept {
        for (size_type i{size_}; i > first_index; --i) {
            storage_.destroy_at(i - 1);
        }

        size_ = first_index;
    }

    TFixedStorage<value_type, N> storage_;
    size_type size_{0};
};
}
