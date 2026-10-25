#include <ioj/structural_diff.h>

struct Unsupported {};

void reject_unsupported() {
    ioj::DiffReport result;
    result.diff("unsupported", Unsupported{}, Unsupported{});
}
