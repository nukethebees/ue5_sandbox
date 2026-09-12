#include "SandboxCore/array_utils.h"

namespace ml {
auto is_sorted_desc(TConstArrayView<int32> const xs) -> bool {
    return ml::kernel::is_sorted_desc(xs.GetData(), xs.Num());
}
}
