#pragma once

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

}
