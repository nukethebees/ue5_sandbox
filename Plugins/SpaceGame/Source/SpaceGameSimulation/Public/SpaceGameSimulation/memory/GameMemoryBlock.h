#pragma once

#include "CoreMinimal.h"

#include <cstddef>

class FGameMemory;

class SPACEGAMESIMULATION_API FGameMemoryBlock {
  public:
    FGameMemoryBlock() = default;
    ~FGameMemoryBlock();
    FGameMemoryBlock(FGameMemoryBlock const&) = delete;
    auto operator=(FGameMemoryBlock const&) -> FGameMemoryBlock& = delete;
    FGameMemoryBlock(FGameMemoryBlock&& other) noexcept;
    auto operator=(FGameMemoryBlock&& other) noexcept -> FGameMemoryBlock&;

    auto data() const noexcept -> std::byte* { return data_; }
    auto size_bytes() const noexcept -> SIZE_T { return size_bytes_; }
    auto alignment() const noexcept -> SIZE_T { return alignment_; }
    explicit operator bool() const noexcept { return data_ != nullptr; }
  private:
    friend class FGameMemory;

    FGameMemoryBlock(FGameMemory& owner,
                     std::byte* data,
                     SIZE_T size_bytes,
                     SIZE_T alignment) noexcept;
    void reset();

    FGameMemory* owner_{};
    std::byte* data_{};
    SIZE_T size_bytes_{};
    SIZE_T alignment_{};
};
