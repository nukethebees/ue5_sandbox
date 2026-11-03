#include <ioj/memory/bounded_resource.h>

#include <algorithm>
#include <bit>
#include <cassert>
#include <exception>
#include <memory>
#include <new>

namespace ioj::memory {
BoundedResource::BoundedResource(std::size_t const capacity_bytes,
                                 std::pmr::memory_resource& upstream)
    : upstream_{upstream}
    , buffer_{nullptr, capacity_bytes, 64} {
    if (capacity_bytes != 0) {
        buffer_.data =
            static_cast<std::byte*>(upstream_.allocate(capacity_bytes, buffer_.alignment));
    }
}
BoundedResource::~BoundedResource() {
    if (buffer_.data != nullptr) {
        upstream_.deallocate(buffer_.data, buffer_.size_bytes, buffer_.alignment);
    }
}
auto BoundedResource::try_allocate(std::size_t const bytes, std::size_t const alignment) noexcept
    -> void* {
    assert(std::has_single_bit(alignment));
    if (buffer_.data == nullptr) {
        return nullptr;
    }
    auto* cursor{static_cast<void*>(buffer_.data + offset_)};
    auto available{buffer_.size_bytes - offset_};
    auto const size{std::max(bytes, std::size_t{1})};
    if (std::align(alignment, size, cursor, available) == nullptr) {
        return nullptr;
    }
    offset_ = static_cast<std::byte*>(cursor) - buffer_.data + size;
    return cursor;
}
auto BoundedResource::do_allocate(std::size_t const bytes, std::size_t const alignment) -> void* {
    if (auto* allocation{try_allocate(bytes, alignment)}) {
        return allocation;
    }
#if defined(__cpp_exceptions)
    throw std::bad_alloc{};
#else
    std::terminate();
#endif
}
}
