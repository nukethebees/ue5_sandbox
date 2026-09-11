#pragma once

#include "SpaceGameSimulation/memory/GameMemoryBacking.h"
#include "SpaceGameSimulation/memory/GameMemoryBlock.h"
#include "SpaceGameSimulation/memory/GameMemoryConfig.h"

#include <memory_resource>

struct FGameMemoryAllocationFailure {
    SIZE_T requested_bytes{};
    SIZE_T alignment{};
    SIZE_T claimed_bytes{};
    SIZE_T total_capacity_bytes{};
};

struct FGameMemoryStats {
    SIZE_T total_capacity_bytes{};
    SIZE_T claimed_bytes{};
    SIZE_T live_block_bytes{};
    int32 live_block_count{};
    int32 peak_live_block_count{};
    int32 reusable_range_count{};
};

class SPACEGAMESIMULATION_API FGameMemory {
  public:
    explicit FGameMemory(FGameMemoryConfig config = {});
    FGameMemory(FGameMemoryBackingLease lease, FGameMemoryConfig config);
    ~FGameMemory() = default;
    FGameMemory(FGameMemory const&) = delete;
    FGameMemory(FGameMemory&&) = delete;
    auto operator=(FGameMemory const&) -> FGameMemory& = delete;
    auto operator=(FGameMemory&&) -> FGameMemory& = delete;

    auto try_acquire_block(SIZE_T size_bytes, SIZE_T alignment) -> TOptional<FGameMemoryBlock>;
    auto acquire_block(SIZE_T size_bytes, SIZE_T alignment) -> FGameMemoryBlock;
    auto memory_resource() noexcept -> std::pmr::memory_resource& { return resource_; }
    auto get_stats() const noexcept -> FGameMemoryStats;
    auto get_last_allocation_failure() const noexcept
        -> TOptional<FGameMemoryAllocationFailure> const& {
        return last_allocation_failure_;
    }
    auto owns_backing_locally() const noexcept -> bool { return local_backing_.IsValid(); }
    auto backing_address() const noexcept -> std::byte* { return backing_->data(); }
  private:
    friend class FGameMemoryBlock;

    struct FReusableRange {
        std::byte* data{};
        SIZE_T size_bytes{};
        SIZE_T alignment{};
    };

    class FRootMemoryResource final : public std::pmr::memory_resource {
      public:
        explicit FRootMemoryResource(FGameMemory& owner) noexcept
            : owner_{owner} {}
      private:
        virtual auto do_allocate(size_t bytes, size_t alignment) -> void* override;
        virtual void do_deallocate(void* pointer, size_t bytes, size_t alignment) override;
        virtual auto do_is_equal(std::pmr::memory_resource const& other) const noexcept
            -> bool override {
            return this == &other;
        }

        FGameMemory& owner_;
    };

    auto try_acquire_range(SIZE_T size_bytes, SIZE_T alignment) -> std::byte*;
    void release_range(std::byte* data, SIZE_T size_bytes, SIZE_T alignment);

    TUniquePtr<FGameMemoryBacking> local_backing_{};
    FGameMemoryBackingLease external_lease_{};
    FGameMemoryBacking* backing_{};
    SIZE_T capacity_bytes_{};
    FRootMemoryResource resource_{*this};
    TArray<FReusableRange> reusable_ranges_{};
    TOptional<FGameMemoryAllocationFailure> last_allocation_failure_{};
    SIZE_T claimed_bytes_{};
    SIZE_T live_block_bytes_{};
    int32 live_block_count_{};
    int32 peak_live_block_count_{};
};
