#include "SpaceGameSimulation/memory/GameMemoryBlock.h"

#include <utility>

FGameMemoryBlock::FGameMemoryBlock(ml::memory::Block block) noexcept
    : block_{std::move(block)} {}

FGameMemoryBlock::~FGameMemoryBlock() = default;

FGameMemoryBlock::FGameMemoryBlock(FGameMemoryBlock&& other) noexcept = default;

auto FGameMemoryBlock::operator=(FGameMemoryBlock&& other) noexcept -> FGameMemoryBlock& = default;
