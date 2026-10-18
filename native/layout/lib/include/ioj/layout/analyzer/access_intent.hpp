#pragma once

#include <ioj/layout/analyzer/access_operation.hpp>

#include <string>

namespace ioj::layout {

struct AccessIntent {
    auto operator==(AccessIntent const&) const -> bool = default;

    std::string name{};
    AccessOperation operation{AccessOperation::read};
};

}
