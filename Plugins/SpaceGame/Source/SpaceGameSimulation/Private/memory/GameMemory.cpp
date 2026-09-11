#include "SpaceGameSimulation/memory/GameMemory.h"

FGameMemory::FGameMemory(FGameMemoryConfig const config)
    : local_backing_{FGameMemoryBacking::create(config.root_capacity_bytes)}
    , backing_{local_backing_.Get()}
    , capacity_bytes_{config.root_capacity_bytes} {
    check(local_backing_.IsValid());
    check(!external_lease_);
    check(backing_ != nullptr);
}

FGameMemory::FGameMemory(FGameMemoryBackingLease lease, FGameMemoryConfig const config)
    : external_lease_{MoveTemp(lease)} {
    check(!local_backing_.IsValid());
    check(external_lease_);
    checkf(external_lease_.get().capacity_bytes() >= config.root_capacity_bytes,
           TEXT("The leased game-memory backing has %llu bytes but %llu bytes were requested."),
           static_cast<uint64>(external_lease_.get().capacity_bytes()),
           static_cast<uint64>(config.root_capacity_bytes));
    backing_ = &external_lease_.get();
    check(backing_ != nullptr);
    capacity_bytes_ = config.root_capacity_bytes;
}

auto FGameMemory::try_acquire_block(SIZE_T const size_bytes, SIZE_T const alignment)
    -> TOptional<FGameMemoryBlock> {
    auto* const data{try_acquire_range(size_bytes, alignment)};
    if (data == nullptr) {
        return NullOpt;
    }

    live_block_bytes_ += size_bytes;
    ++live_block_count_;
    peak_live_block_count_ = FMath::Max(peak_live_block_count_, live_block_count_);
    return FGameMemoryBlock{*this, data, size_bytes, alignment};
}

auto FGameMemory::acquire_block(SIZE_T const size_bytes, SIZE_T const alignment)
    -> FGameMemoryBlock {
    auto block{try_acquire_block(size_bytes, alignment)};
    checkf(block.IsSet(),
           TEXT("Game memory exhausted: requested=%llu alignment=%llu claimed=%llu capacity=%llu"),
           static_cast<uint64>(size_bytes),
           static_cast<uint64>(alignment),
           static_cast<uint64>(claimed_bytes_),
           static_cast<uint64>(capacity_bytes_));
    return MoveTemp(block.GetValue());
}

auto FGameMemory::get_stats() const noexcept -> FGameMemoryStats {
    return {.total_capacity_bytes = capacity_bytes_,
            .claimed_bytes = claimed_bytes_,
            .live_block_bytes = live_block_bytes_,
            .live_block_count = live_block_count_,
            .peak_live_block_count = peak_live_block_count_,
            .reusable_range_count = reusable_ranges_.Num()};
}

auto FGameMemory::try_acquire_range(SIZE_T const size_bytes, SIZE_T const alignment) -> std::byte* {
    if (size_bytes == 0 || alignment == 0 || !FMath::IsPowerOfTwo(alignment)) {
        last_allocation_failure_ =
            FGameMemoryAllocationFailure{.requested_bytes = size_bytes,
                                         .alignment = alignment,
                                         .claimed_bytes = claimed_bytes_,
                                         .total_capacity_bytes = capacity_bytes_};
        return nullptr;
    }

    auto const reusable_count{reusable_ranges_.Num()};
    for (int32 index{}; index < reusable_count; ++index) {
        auto const range{reusable_ranges_[index]};
        auto const begin{reinterpret_cast<UPTRINT>(range.data)};
        auto const aligned_begin{Align(begin, alignment)};
        auto const padding{static_cast<SIZE_T>(aligned_begin - begin)};
        if (padding <= range.size_bytes && size_bytes <= range.size_bytes - padding) {
            reusable_ranges_.RemoveAtSwap(index, EAllowShrinking::No);
            if (padding > 0) {
                reusable_ranges_.Add(
                    {.data = range.data, .size_bytes = padding, .alignment = range.alignment});
            }
            auto const remaining{range.size_bytes - padding - size_bytes};
            if (remaining > 0) {
                reusable_ranges_.Add(
                    {.data = reinterpret_cast<std::byte*>(aligned_begin) + size_bytes,
                     .size_bytes = remaining,
                     .alignment = 1});
            }
            last_allocation_failure_.Reset();
            return reinterpret_cast<std::byte*>(aligned_begin);
        }
    }

    auto const base_address{reinterpret_cast<UPTRINT>(backing_->data())};
    auto const current_address{base_address + claimed_bytes_};
    auto const aligned_address{Align(current_address, alignment)};
    auto const padding{static_cast<SIZE_T>(aligned_address - current_address)};
    auto const capacity{capacity_bytes_};
    if (padding > capacity - FMath::Min(claimed_bytes_, capacity) ||
        size_bytes > capacity - claimed_bytes_ - padding) {
        last_allocation_failure_ = FGameMemoryAllocationFailure{.requested_bytes = size_bytes,
                                                                .alignment = alignment,
                                                                .claimed_bytes = claimed_bytes_,
                                                                .total_capacity_bytes = capacity};
        return nullptr;
    }

    claimed_bytes_ += padding + size_bytes;
    last_allocation_failure_.Reset();
    return reinterpret_cast<std::byte*>(aligned_address);
}

void FGameMemory::release_range(std::byte* const data,
                                SIZE_T const size_bytes,
                                SIZE_T const alignment) {
    reusable_ranges_.Add({.data = data, .size_bytes = size_bytes, .alignment = alignment});
}

auto FGameMemory::FRootMemoryResource::do_allocate(size_t const bytes, size_t const alignment)
    -> void* {
    auto* const data{owner_.try_acquire_range(bytes, alignment)};
    if (data == nullptr) {
        throw std::bad_alloc{};
    }
    return data;
}

void FGameMemory::FRootMemoryResource::do_deallocate(void* const pointer,
                                                     size_t const bytes,
                                                     size_t const alignment) {
    owner_.release_range(static_cast<std::byte*>(pointer), bytes, alignment);
}
