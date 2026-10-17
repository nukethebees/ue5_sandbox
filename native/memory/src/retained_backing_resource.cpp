#include <native/memory/retained_backing_resource.h>

#include <cassert>
#include <new>

namespace ioj::memory {

RetainedBackingResource::RetainedBackingResource(std::size_t const capacity_bytes)
    : backing_{capacity_bytes} {}
RetainedBackingResource::~RetainedBackingResource() {
    assert(!is_in_use());
}
auto RetainedBackingResource::can_allocate(std::size_t const bytes,
                                           std::size_t const alignment) const noexcept -> bool {
    return !is_in_use() && bytes != 0 && bytes <= backing_.capacity_bytes() && alignment != 0 &&
           alignment <= Backing::alignment && (alignment & (alignment - 1)) == 0;
}
auto RetainedBackingResource::do_allocate(std::size_t const bytes, std::size_t const alignment)
    -> void* {
    if (!can_allocate(bytes, alignment)) {
        throw std::bad_alloc{};
    }

    active_bytes_ = bytes;
    active_alignment_ = alignment;
    return backing_.data();
}
void RetainedBackingResource::do_deallocate(void* const allocation,
                                            std::size_t const bytes,
                                            std::size_t const alignment) {
    assert(is_in_use());
    assert(allocation == backing_.data());
    assert(bytes == active_bytes_ && alignment == active_alignment_);
    active_bytes_ = 0;
    active_alignment_ = 0;
}
auto RetainedBackingResource::do_is_equal(std::pmr::memory_resource const& other) const noexcept
    -> bool {
    return this == &other;
}
}
