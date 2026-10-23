#pragma once

#include <lispb/index.h>

namespace lispb::codegen {

class LoweringPaths;
struct SingleAllocationColumns;
struct SingleAllocationModels;
struct SingleAllocationViews;

using LoweringPathId = Index<LoweringPaths>;
using SingleAllocationColumnId = Index<SingleAllocationColumns>;
using SingleAllocationModelId = Index<SingleAllocationModels>;
using SingleAllocationViewId = Index<SingleAllocationViews>;

}
