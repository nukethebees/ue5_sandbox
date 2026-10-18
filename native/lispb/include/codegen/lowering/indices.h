#pragma once

#include <lispb/index.h>

namespace lispb::codegen {

struct CompactVectors;
struct FixedLeaves;
struct FixedLayouts;
struct FixedMemberLayouts;
class LoweringPaths;
struct ResolvedMembers;
struct RowParameters;
struct SingleAllocationColumns;
struct SingleAllocationModels;
struct SingleAllocationViews;

using CompactVectorId = Index<CompactVectors>;
using FixedLeafId = Index<FixedLeaves>;
using FixedLayoutId = Index<FixedLayouts>;
using FixedMemberLayoutId = Index<FixedMemberLayouts>;
using LoweringPathId = Index<LoweringPaths>;
using ResolvedMemberId = Index<ResolvedMembers>;
using RowParameterId = Index<RowParameters>;
using SingleAllocationColumnId = Index<SingleAllocationColumns>;
using SingleAllocationModelId = Index<SingleAllocationModels>;
using SingleAllocationViewId = Index<SingleAllocationViews>;

}
