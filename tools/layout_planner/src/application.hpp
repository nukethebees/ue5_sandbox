#pragma once

#include <ioj/layout/lispb_adapter.hpp>

namespace ioj::layout_planner {

auto run_application(layout::CatalogLoadResult loaded) -> int;

} // namespace ioj::layout_planner
