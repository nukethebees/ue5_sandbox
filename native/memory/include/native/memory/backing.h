#pragma once

#include <cstddef>
#include <memory>
#include <optional>

namespace ml::memory {
class Backing;

class BackingLease {
  public:
    BackingLease() = default;
    ~BackingLease();
    BackingLease(BackingLease const&) = delete;
    auto operator=(BackingLease const&) -> BackingLease& = delete;
    BackingLease(BackingLease&& other) noexcept;
    auto operator=(BackingLease&& other) noexcept -> BackingLease&;

    [[nodiscard]] auto get() noexcept -> Backing&;
    [[nodiscard]] auto get() const noexcept -> Backing const&;
    explicit operator bool() const noexcept { return backing_ != nullptr; }
  private:
    friend class Backing;

    explicit BackingLease(Backing& backing) noexcept;
    void reset() noexcept;

    Backing* backing_{};
};

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
