#pragma once

#include <Containers/Array.h>

struct FScoreRecord;

namespace ioj::detail {
auto make_sample_profile_records() -> TArray<FScoreRecord>;
}
