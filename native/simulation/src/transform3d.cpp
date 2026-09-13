#include <sandbox/simulation/transform3d.h>

#include <array>
#include <cmath>

namespace ml::simulation {
auto Transform3d::operator*(Transform3d const& parent) const -> Transform3d {
    Transform3d result{parent.rotation * rotation,
                       parent.rotation.rotate_vector(parent.scale * location) + parent.location,
                       scale * parent.scale};
    if (scale.x >= 0.0 && scale.y >= 0.0 && scale.z >= 0.0 && parent.scale.x >= 0.0 &&
        parent.scale.y >= 0.0 && parent.scale.z >= 0.0) {
        return result;
    }

    std::array<ml::Vector3d, 3> axes{ml::Vector3d{scale.x, 0.0, 0.0},
                                     ml::Vector3d{0.0, scale.y, 0.0},
                                     ml::Vector3d{0.0, 0.0, scale.z}};
    std::array const signs{result.scale.x < 0.0 ? -1.0 : 1.0,
                           result.scale.y < 0.0 ? -1.0 : 1.0,
                           result.scale.z < 0.0 ? -1.0 : 1.0};
    for (std::size_t i{}; i < axes.size(); ++i) {
        auto& axis{axes[i]};
        axis = parent.rotation.rotate_vector(parent.scale * rotation.rotate_vector(axis));
        auto const squared{axis.size_squared()};
        axis = axis * (squared > 1.e-8 ? signs[i] / std::sqrt(squared) : signs[i]);
        if (std::abs(axis.x) <= 1.e-4 && std::abs(axis.y) <= 1.e-4 && std::abs(axis.z) <= 1.e-4) {
            result.rotation = {};
            return result;
        }
    }

    // The columns are the transformed local axes, with scale and reflections removed.
    double const matrix[3][3]{{axes[0].x, axes[1].x, axes[2].x},
                              {axes[0].y, axes[1].y, axes[2].y},
                              {axes[0].z, axes[1].z, axes[2].z}};
    auto const trace{matrix[0][0] + matrix[1][1] + matrix[2][2]};
    if (trace > 0.0) {
        auto const inverse_root{1.0 / std::sqrt(trace + 1.0)};
        auto const half_inverse_root{0.5 * inverse_root};
        result.rotation = {(matrix[2][1] - matrix[1][2]) * half_inverse_root,
                           (matrix[0][2] - matrix[2][0]) * half_inverse_root,
                           (matrix[1][0] - matrix[0][1]) * half_inverse_root,
                           0.5 / inverse_root};
    } else {
        std::size_t i{matrix[1][1] > matrix[0][0] ? 1u : 0u};
        if (matrix[2][2] > matrix[i][i]) {
            i = 2;
        }
        auto const j{(i + 1) % 3};
        auto const k{(j + 1) % 3};
        auto const root{std::sqrt(matrix[i][i] - matrix[j][j] - matrix[k][k] + 1.0)};
        auto const factor{0.5 / root};
        std::array<double, 3> xyz{};
        xyz[i] = 0.5 * root;
        xyz[j] = (matrix[j][i] + matrix[i][j]) * factor;
        xyz[k] = (matrix[k][i] + matrix[i][k]) * factor;
        result.rotation = {xyz[0], xyz[1], xyz[2], (matrix[k][j] - matrix[j][k]) * factor};
    }
    result.rotation.normalize();
    return result;
}
}
