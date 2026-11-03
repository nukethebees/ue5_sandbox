#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <exception>
#include <iterator>
#include <limits>
#include <memory>
#include <memory_resource>
#include <new>
#include <type_traits>
#include <utility>

namespace ioj {
// Segmented storage: growth preserves element addresses. Clear retains segments for reuse.
// Borrow the resource for the container's lifetime. Moves transfer storage and its resource;
// copying is disabled. Clear, pop_back and moves invalidate affected iterators/end iterators.
template <typename T>
class LinkedVector {
    struct Segment {
        Segment* previous{};
        Segment* next{};
        std::size_t size{};
        std::size_t capacity{};
    };
    static constexpr std::size_t data_offset{(sizeof(Segment) + alignof(T) - 1) / alignof(T) *
                                             alignof(T)};
    static constexpr std::size_t allocation_alignment{std::max(alignof(Segment), alignof(T))};
    static constexpr std::size_t max_capacity{
        (std::numeric_limits<std::size_t>::max() - data_offset) / sizeof(T)};

    static auto elements(Segment* segment) noexcept -> T* {
        return reinterpret_cast<T*>(reinterpret_cast<std::byte*>(segment) + data_offset);
    }

  public:
    template <bool Const>
    class Iterator {
      public:
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using reference = std::conditional_t<Const, T const&, T&>;
        using pointer = std::conditional_t<Const, T const*, T*>;
        using iterator_category = std::bidirectional_iterator_tag;
        using iterator_concept = std::bidirectional_iterator_tag;

        Iterator() = default;
        template <bool Other>
            requires (Const && !Other)
        Iterator(Iterator<Other> const& other)
            : owner_{other.owner_}
            , segment_{other.segment_}
            , index_{other.index_} {}

        auto operator*() const -> reference {
            assert(segment_ != nullptr && index_ < segment_->size);
            return elements(segment_)[index_];
        }
        auto operator->() const -> pointer { return &**this; }
        auto operator++() -> Iterator& {
            assert(segment_ != nullptr);
            if (++index_ == segment_->size) {
                segment_ = segment_->next;
                while (segment_ != nullptr && segment_->size == 0) {
                    segment_ = segment_->next;
                }
                index_ = 0;
            }
            return *this;
        }
        auto operator++(int) -> Iterator {
            auto previous{*this};
            ++*this;
            return previous;
        }
        auto operator--() -> Iterator& {
            assert(owner_ != nullptr);
            if (segment_ == nullptr) {
                segment_ = owner_->active_;
                index_ = segment_ == nullptr ? 0 : segment_->size;
            }
            while (segment_ != nullptr && index_ == 0) {
                segment_ = segment_->previous;
                index_ = segment_ == nullptr ? 0 : segment_->size;
            }
            assert(segment_ != nullptr && index_ != 0);
            --index_;
            return *this;
        }
        auto operator--(int) -> Iterator {
            auto previous{*this};
            --*this;
            return previous;
        }
        template <bool Other>
        auto operator==(Iterator<Other> const& other) const -> bool {
            return owner_ == other.owner_ && segment_ == other.segment_ && index_ == other.index_;
        }

      private:
        friend class LinkedVector;
        template <bool>
        friend class Iterator;
        Iterator(LinkedVector const* owner, Segment* segment)
            : owner_{owner}
            , segment_{segment} {}
        LinkedVector const* owner_{};
        Segment* segment_{};
        std::size_t index_{};
    };

    using value_type = T;
    using iterator = Iterator<false>;
    using const_iterator = Iterator<true>;

    explicit LinkedVector(
        std::pmr::memory_resource& resource = *std::pmr::get_default_resource()) noexcept
        : resource_{&resource} {}
    ~LinkedVector() { release(); }
    LinkedVector(LinkedVector const&) = delete;
    LinkedVector(LinkedVector&& other) noexcept { take_storage(other); }
    auto operator=(LinkedVector const&) -> LinkedVector& = delete;
    auto operator=(LinkedVector&& other) noexcept -> LinkedVector& {
        if (this != &other) {
            release();
            take_storage(other);
        }
        return *this;
    }

    auto size() const noexcept -> std::size_t { return size_; }
    auto capacity() const noexcept -> std::size_t { return capacity_; }
    auto empty() const noexcept -> bool { return size_ == 0; }
    auto begin() noexcept -> iterator { return {this, empty() ? nullptr : head_}; }
    auto end() noexcept -> iterator { return {this, nullptr}; }
    auto begin() const noexcept -> const_iterator { return {this, empty() ? nullptr : head_}; }
    auto end() const noexcept -> const_iterator { return {this, nullptr}; }
    auto back() -> T& {
        auto it{end()};
        return *--it;
    }
    auto back() const -> T const& {
        auto it{end()};
        return *--it;
    }

    void reserve(std::size_t const count) {
        if (count <= capacity_) {
            return;
        }
        if (count > max_capacity) {
            allocation_failure();
        }

        auto const added{count - capacity_};
        auto const bytes{data_offset + added * sizeof(T)};
        auto* const allocation{resource_->allocate(bytes, allocation_alignment)};
        auto* const segment{std::construct_at(static_cast<Segment*>(allocation))};
        segment->previous = tail_;
        segment->capacity = added;
        if (tail_ != nullptr) {
            tail_->next = segment;
        } else {
            head_ = segment;
        }
        tail_ = segment;
        capacity_ = count;
    }
    template <typename... Args>
    auto emplace_back(Args&&... args) -> T& {
        if (size_ == capacity_) {
            if (capacity_ == max_capacity) {
                allocation_failure();
            }
            auto const growth{capacity_ == 0 ? 1 : std::min(capacity_, max_capacity - capacity_)};
            reserve(capacity_ + growth);
        }

        auto* segment{size_ == 0 ? head_ : active_};
        while (segment->size == segment->capacity) {
            segment = segment->next;
        }
        auto* const result{
            std::construct_at(elements(segment) + segment->size, std::forward<Args>(args)...)};
        ++segment->size;
        ++size_;
        active_ = segment;
        return *result;
    }
    void pop_back() {
        assert(size_ != 0);
        while (active_->size == 0) {
            active_ = active_->previous;
        }
        std::destroy_at(elements(active_) + --active_->size);
        --size_;
    }
    void clear() noexcept {
        for (auto* segment{head_}; segment != nullptr; segment = segment->next) {
            std::destroy_n(elements(segment), segment->size);
            segment->size = 0;
        }
        size_ = 0;
        active_ = head_;
    }
    void release() noexcept {
        clear();
        while (head_ != nullptr) {
            auto* segment{head_};
            head_ = segment->next;
            auto const bytes{data_offset + segment->capacity * sizeof(T)};
            std::destroy_at(segment);
            resource_->deallocate(segment, bytes, allocation_alignment);
        }
        tail_ = nullptr;
        active_ = nullptr;
        capacity_ = 0;
    }

  private:
    [[noreturn]] static void allocation_failure() {
#if defined(__cpp_exceptions)
        throw std::bad_alloc{};
#else
        std::terminate();
#endif
    }
    void take_storage(LinkedVector& other) noexcept {
        resource_ = other.resource_;
        head_ = std::exchange(other.head_, nullptr);
        tail_ = std::exchange(other.tail_, nullptr);
        active_ = std::exchange(other.active_, nullptr);
        size_ = std::exchange(other.size_, 0);
        capacity_ = std::exchange(other.capacity_, 0);
    }

    std::pmr::memory_resource* resource_{};
    Segment* head_{};
    Segment* tail_{};
    Segment* active_{};
    std::size_t size_{};
    std::size_t capacity_{};
};
}
