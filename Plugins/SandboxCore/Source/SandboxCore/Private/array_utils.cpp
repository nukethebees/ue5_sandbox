#include "SandboxCore/array_utils.h"

namespace ioj {
auto is_sorted_desc(TConstArrayView<int32> const xs) -> bool {
    return kernel::is_sorted_desc(xs.GetData(), xs.Num());
}
}
