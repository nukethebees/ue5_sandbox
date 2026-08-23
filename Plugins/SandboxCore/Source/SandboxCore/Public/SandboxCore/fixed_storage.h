#pragma once

#include <HAL/Platform.h>
#include <Misc/AssertionMacros.h>

#include <memory>
#include <new>
#include <utility>

namespace ml {
// Raw fixed-capacity storage. The owner is responsible for tracking which elements are alive.
template <typename T, int32 Capacity>
    requires (Capacity >= 0)
class TFixedStorage {
  public:
    using value_type = T;
    using size_type = int32;

    static constexpr size_type capacity_value{Capacity};

    TFixedStorage() noexcept = default;
    TFixedStorage(TFixedStorage const&) = delete;
    TFixedStorage(TFixedStorage&&) = delete;
    auto operator=(TFixedStorage const&) -> TFixedStorage& = delete;
    auto operator=(TFixedStorage&&) -> TFixedStorage& = delete;
    ~TFixedStorage() noexcept = default;

    static constexpr auto capacity() noexcept -> size_type { return capacity_value; }

    auto data() noexcept -> value_type* {
        return reinterpret_cast<value_type*>(std::addressof(storage_));
    }
    auto data() const noexcept -> value_type const* {
        return reinterpret_cast<value_type const*>(std::addressof(storage_));
    }

    auto operator[](size_type const index) noexcept -> value_type& {
        check_capacity_index(index);
        return data()[index];
    }
    auto operator[](size_type const index) const noexcept -> value_type const& {
        check_capacity_index(index);
        return data()[index];
    }

    template <typename... Args>
    auto construct_at(size_type const index, Args&&... args) -> value_type& {
        check_capacity_index(index);
        return *std::construct_at(data() + index, std::forward<Args>(args)...);
    }

    void destroy_at(size_type const index) noexcept {
        check_capacity_index(index);
        std::destroy_at(data() + index);
    }
  private:
    static constexpr size_type storage_count{Capacity > 0 ? Capacity : 1};

    union FStorage {
        FStorage() noexcept {}
        ~FStorage() noexcept {}

        value_type values[storage_count];
    };

    static void check_capacity_index(size_type const index) {
        check(index >= 0);
        check(index < capacity());
    }

    FStorage storage_;
};
}
