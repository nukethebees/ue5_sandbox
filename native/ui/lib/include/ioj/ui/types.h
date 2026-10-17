#pragma once

#include <ioj/point2f.h>
#include <ioj/point3f.h>

namespace ioj::ui {
using Vector2f = Point2f;
using Vector3f = Point3f;

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
