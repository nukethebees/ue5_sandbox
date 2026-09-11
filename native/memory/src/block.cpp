#include "native/memory/block.h"

#include "native/memory/root_arena.h"

#include <utility>

namespace ml::memory {
Block::Block(RootArena& owner,
             std::byte* const data,
             std::size_t const size_bytes,
             std::size_t const alignment) noexcept
    : owner_{&owner}
    , data_{data}
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
        owner_->release_block(data_, size_bytes_);
        owner_ = nullptr;
        data_ = nullptr;
        size_bytes_ = 0;
        alignment_ = 0;
    }
}
}
