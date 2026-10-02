#pragma once

#include <gtest/gtest.h>

#include <cstddef>
#include <map>
#include <memory_resource>

namespace pmr_test_support {

struct Allocation {
    std::size_t bytes{};
    std::size_t alignment{};
};

struct AllocationState {
    int allocations{};
    int frees{};
    std::size_t last_bytes{};
    std::size_t last_alignment{};
    bool reject_allocation{};
    std::map<void*, Allocation> live;
};

class CountingResource final : public std::pmr::memory_resource {
  public:
    explicit CountingResource(AllocationState& state)
        : state_{state} {}
    ~CountingResource() override = default;
  private:
    auto do_allocate(std::size_t const bytes, std::size_t const alignment) -> void* override {
        state_.last_bytes = bytes;
        state_.last_alignment = alignment;
        if (state_.reject_allocation) {
            throw std::bad_alloc{};
        }
        auto* const data{std::pmr::new_delete_resource()->allocate(bytes, alignment)};
        state_.live.emplace(data, Allocation{bytes, alignment});
        ++state_.allocations;
        return data;
    }
    void do_deallocate(void* const data,
                       std::size_t const bytes,
                       std::size_t const alignment) override {
        auto const found{state_.live.find(data)};
        ASSERT_NE(found, state_.live.end());
        EXPECT_EQ(found->second.bytes, bytes);
        EXPECT_EQ(found->second.alignment, alignment);
        std::pmr::new_delete_resource()->deallocate(
            data, found->second.bytes, found->second.alignment);
        state_.live.erase(found);
        ++state_.frees;
    }
    auto do_is_equal(std::pmr::memory_resource const& other) const noexcept -> bool override {
        // Tests compare only counting resources or the resource itself.
        return this == &other || equal_resource_ == &other;
    }
  public:
    void set_equal_resource(CountingResource& other) {
        EXPECT_EQ(&state_, &other.state_);
        equal_resource_ = &other;
        other.equal_resource_ = this;
    }
  private:
    AllocationState& state_;
    CountingResource* equal_resource_{};
};

class DefaultResourceScope {
  public:
    explicit DefaultResourceScope(std::pmr::memory_resource* resource)
        : previous_{std::pmr::set_default_resource(resource)} {}
    ~DefaultResourceScope() { std::pmr::set_default_resource(previous_); }
    DefaultResourceScope(DefaultResourceScope const&) = delete;
    auto operator=(DefaultResourceScope const&) -> DefaultResourceScope& = delete;
  private:
    std::pmr::memory_resource* previous_;
};

}
