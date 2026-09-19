#pragma once

#include <ioj/layout/schema_loader.hpp>

namespace ioj::layout_planner {

auto run_application(layout::SchemaLoadResult loaded) -> int;

} // namespace ioj::layout_planner
