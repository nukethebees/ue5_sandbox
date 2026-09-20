#pragma once

namespace ml::ui {
struct Vector2f {
    float x{};
    float y{};

    auto operator==(Vector2f const&) const -> bool = default;
};

struct Vector3f {
    float x{};
    float y{};
    float z{};

    auto operator==(Vector3f const&) const -> bool = default;
};

struct Color4f {
    float r{};
    float g{};
    float b{};
    float a{1.0f};

    auto operator==(Color4f const&) const -> bool = default;
};

struct Insets {
    float left{};
    float top{};
    float right{};
    float bottom{};
};
}
