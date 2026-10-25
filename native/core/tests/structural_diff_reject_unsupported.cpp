#include <ioj/structural_diff.h>

struct Unsupported {};

void reject_unsupported() {
    ioj::DiffResult result;
    ioj::DiffContext{result}.diff("unsupported", Unsupported{}, Unsupported{});
}
