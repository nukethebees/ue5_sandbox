#pragma once

#include <native/memory/backing_lease.h>

#include <cstddef>
#include <memory>
#include <optional>

namespace ml::memory {

class Backing {
  public:
    static constexpr std::size_t alignment{64};

    [[nodiscard]] static auto create(std::size_t capacity_bytes) -> std::unique_ptr<Backing>;

    ~Backing();
    Backing(Backing const&) = delete;
    Backing(Backing&&) = delete;
    auto operator=(Backing const&) -> Backing& = delete;
    auto operator=(Backing&&) -> Backing& = delete;

    [[nodiscard]] auto try_acquire_lease() -> std::optional<BackingLease>;
    [[nodiscard]] auto data() const noexcept -> std::byte* { return data_; }
    [[nodiscard]] auto capacity_bytes() const noexcept -> std::size_t { return capacity_bytes_; }
    [[nodiscard]] auto is_leased() const noexcept -> bool { return leased_; }
  private:
    friend class BackingLease;

    explicit Backing(std::size_t capacity_bytes);
    void release_lease() noexcept;

    std::byte* data_{};
    std::size_t capacity_bytes_{};
    bool leased_{};
};
}
