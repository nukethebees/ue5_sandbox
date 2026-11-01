#include "native/memory/block.h"

#include <utility>

namespace ioj::memory {
Block::Block(std::pmr::memory_resource& resource,
             std::size_t const size_bytes,
             std::size_t const alignment)
    : owner_{&resource}
    , data_{static_cast<std::byte*>(resource.allocate(size_bytes, alignment))}
    , size_bytes_{size_bytes}
    , alignment_{alignment} {}

Block::~Block() {
    reset();
}

Block::Block(Block&& other) noexcept
    : owner_{std::exchange(other.owner_, nullptr)}
    , data_{std::exchange(other.data_, nullptr)}
    , size_bytes_{std::exchange(other.size_bytes_, 0)}
    , alignment_{std::exchange(other.alignment_, 0)} {}

auto Block::operator=(Block&& other) noexcept -> Block& {
    if (this != &other) {
        reset();
        owner_ = std::exchange(other.owner_, nullptr);
        data_ = std::exchange(other.data_, nullptr);
        size_bytes_ = std::exchange(other.size_bytes_, 0);
        alignment_ = std::exchange(other.alignment_, 0);
    }
    return *this;
}

void Block::reset() {
    if (owner_ != nullptr) {
        owner_->deallocate(data_, size_bytes_, alignment_);
        owner_ = nullptr;
        data_ = nullptr;
        size_bytes_ = 0;
        alignment_ = 0;
    }
}
}
