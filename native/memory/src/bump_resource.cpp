#include <native/memory/bump_resource.h>

#include <sbx/memory_resource.h>

#include <algorithm>
#include <cstdint>
#include <memory>

namespace ioj::memory {

BumpResource::BumpResource(std::size_t const capacity_bytes)
    : BumpResource{capacity_bytes, *sbx::memory::mimalloc_resource()} {}
BumpResource::BumpResource(std::size_t const capacity_bytes, std::pmr::memory_resource& upstream)
    : upstream_{upstream}
    , buffer_{capacity_bytes != 0 ? Block{upstream, capacity_bytes, backing_alignment} : Block{}} {}

auto BumpResource::do_allocate(std::size_t const bytes, std::size_t const alignment) -> void* {
    if (buffer_) {
        auto* cursor{static_cast<void*>(buffer_.data() + offset_)};
        auto available{buffer_.size_bytes() - offset_};
        auto const size{std::max(bytes, std::size_t{1})};
        if (std::align(alignment, size, cursor, available)) {
            offset_ = static_cast<std::byte*>(cursor) - buffer_.data() + size;
            return cursor;
        }
    }
    return upstream_.allocate(bytes, alignment);
}
void BumpResource::do_deallocate(void* const allocation,
                                 std::size_t const bytes,
                                 std::size_t const alignment) {
    auto const address{reinterpret_cast<std::uintptr_t>(allocation)};
    auto const begin{reinterpret_cast<std::uintptr_t>(buffer_.data())};
    if (address - begin >= buffer_.size_bytes()) {
        upstream_.deallocate(allocation, bytes, alignment);
    }
}
auto BumpResource::do_is_equal(std::pmr::memory_resource const& other) const noexcept -> bool {
    return this == &other;
}
}
