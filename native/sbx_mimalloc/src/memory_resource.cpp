#include <sbx/memory_resource.h>

#include <sbx/memory.h>

#include <new>

namespace sbx::memory {

class MimallocResource final : public std::pmr::memory_resource {
    auto do_allocate(std::size_t const bytes, std::size_t const alignment) -> void* override {
        auto* const allocation{allocate_aligned(bytes, alignment)};
        if (allocation == nullptr) {
            throw std::bad_alloc{};
        }
        return allocation;
    }

    void do_deallocate(void* const data, std::size_t, std::size_t) override { free(data); }

    auto do_is_equal(std::pmr::memory_resource const& other) const noexcept -> bool override {
        return this == &other;
    }
};

auto mimalloc_resource() noexcept -> std::pmr::memory_resource* {
    static MimallocResource resource;
    return &resource;
}

}
