#pragma once

struct FTracked {
    inline static int alive{};

    FTracked() { ++alive; }
    explicit FTracked(int const value)
        : value{value} {
        ++alive;
    }
    FTracked(FTracked const& other)
        : value{other.value} {
        ++alive;
    }
    FTracked(FTracked&& other) noexcept
        : value{other.value} {
        other.value = -1;
        ++alive;
    }
    auto operator=(FTracked const&) -> FTracked& = default;
    auto operator=(FTracked&&) noexcept -> FTracked& = default;
    ~FTracked() { --alive; }

    int value{};
};
