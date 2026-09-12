#pragma once

#include "CoreMinimal.h"

#include <cstddef>
#include <memory>
#include <new>
#include <utility>

namespace ml {

template <typename T, int32 Capacity>
class TFixedStorage {
  public:
    auto data() noexcept -> T* { return reinterpret_cast<T*>(storage_); }
    auto data() const noexcept -> T const* { return reinterpret_cast<T const*>(storage_); }

    template <typename... Args>
    void construct_at(int32 const index, Args&&... args) {
        std::construct_at(data() + index, std::forward<Args>(args)...);
    }

    void destroy_at(int32 const index) noexcept { std::destroy_at(data() + index); }

    auto operator[](int32 const index) noexcept -> T& { return data()[index]; }
    auto operator[](int32 const index) const noexcept -> T const& { return data()[index]; }
  private:
    static constexpr auto byte_count = Capacity > 0 ? sizeof(T) * static_cast<std::size_t>(Capacity)
                                                    : sizeof(T);
    alignas(T) std::byte storage_[byte_count];
};

} // namespace ml
