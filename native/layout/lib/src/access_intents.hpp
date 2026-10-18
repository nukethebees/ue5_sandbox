#pragma once
#include <ioj/layout/analyzer/access_operation.hpp>
namespace ioj::layout::analysis_detail {
auto access_includes_read(AccessOperation operation) -> bool;
auto access_includes_write(AccessOperation operation) -> bool;
}
