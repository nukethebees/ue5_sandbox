#pragma once

#include "CoreMinimal.h"

#include <native/memory/block.h>

class FGameMemory;

class SPACEGAMESIMULATION_API FGameMemoryBlock {
  public:
    FGameMemoryBlock() = default;
    ~FGameMemoryBlock();
    FGameMemoryBlock(FGameMemoryBlock const&) = delete;
    auto operator=(FGameMemoryBlock const&) -> FGameMemoryBlock& = delete;
    FGameMemoryBlock(FGameMemoryBlock&& other) noexcept;
    auto operator=(FGameMemoryBlock&& other) noexcept -> FGameMemoryBlock&;

    auto data() const noexcept -> std::byte* { return block_.data(); }
    auto size_bytes() const noexcept -> SIZE_T { return block_.size_bytes(); }
    auto alignment() const noexcept -> SIZE_T { return block_.alignment(); }
    explicit operator bool() const noexcept { return static_cast<bool>(block_); }
  private:
    friend class FGameMemory;

    explicit FGameMemoryBlock(ml::memory::Block block) noexcept;

    ml::memory::Block block_{};
};
