#pragma once

#include <cstddef>

namespace lispb::schema {
struct DeleteModule {
    std::size_t module_index{};
};
}
