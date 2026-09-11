#include "SpaceGameSimulation/memory/GameMemoryBlock.h"

#include "SpaceGameSimulation/memory/GameMemory.h"

#include <utility>

FGameMemoryBlock::FGameMemoryBlock(FGameMemory& owner,
                                   std::byte* const data,
                                   SIZE_T const size_bytes,
                                   SIZE_T const alignment) noexcept
    : owner_{&owner}
    , data_{data}
    , size_bytes_{size_bytes}
    , alignment_{alignment} {}

FGameMemoryBlock::~FGameMemoryBlock() {
    reset();
}

FGameMemoryBlock::FGameMemoryBlock(FGameMemoryBlock&& other) noexcept
    : owner_{std::exchange(other.owner_, nullptr)}
    , data_{std::exchange(other.data_, nullptr)}
    , size_bytes_{std::exchange(other.size_bytes_, 0)}
    , alignment_{std::exchange(other.alignment_, 0)} {}

auto FGameMemoryBlock::operator=(FGameMemoryBlock&& other) noexcept -> FGameMemoryBlock& {
    if (this != &other) {
        reset();
        owner_ = std::exchange(other.owner_, nullptr);
        data_ = std::exchange(other.data_, nullptr);
        size_bytes_ = std::exchange(other.size_bytes_, 0);
        alignment_ = std::exchange(other.alignment_, 0);
    }
    return *this;
}

void FGameMemoryBlock::reset() {
    if (owner_ != nullptr) {
        owner_->live_block_bytes_ -= size_bytes_;
        --owner_->live_block_count_;
        owner_->release_range(data_, size_bytes_, alignment_);
        owner_ = nullptr;
        data_ = nullptr;
        size_bytes_ = 0;
        alignment_ = 0;
    }
}
