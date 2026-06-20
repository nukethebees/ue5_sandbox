#include "TraceHits.h"

#include <SandboxCore/array_checks.h>
#include <SandboxCore/array_utils.h>

void TraceHits::reset() {
    ml::reset(actors, actor_components, hit_items);
}
auto TraceHits::num() const -> int32 {
    return actors.Num();
}
void TraceHits::remove_at_swap(int32 const index,
                               int32 const count,
                               EAllowShrinking const allow_shrinking) {
    actors.RemoveAtSwap(index, count, allow_shrinking);
    actor_components.RemoveAtSwap(index, count, allow_shrinking);
    hit_items.RemoveAtSwap(index, count, allow_shrinking);
}
void TraceHits::validate_array_sizes() const {
    ml::fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(actors),
        SANDBOX_NAMED_NUM(actor_components),
        SANDBOX_NAMED_NUM(hit_items),
    });
}
