#include <ioj/sim/collision/cell_coord.h>

namespace ioj::sim::collision {
auto CellCoord::operator[](std::size_t const index) noexcept -> int& {
    if (index == 0) {
        return x;
    }
    if (index == 1) {
        return y;
    }
    return z;
}
auto CellCoord::operator[](std::size_t const index) const noexcept -> int {
    if (index == 0) {
        return x;
    }
    if (index == 1) {
        return y;
    }
    return z;
}
}
