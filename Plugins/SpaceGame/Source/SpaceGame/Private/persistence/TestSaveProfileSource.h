#pragma once

#include <Containers/Array.h>

struct FScoreRecord;

namespace ml::ioj::detail {
auto make_test_profile_records() -> TArray<FScoreRecord>;
}
