#pragma once

#include <ioj/s7/detail/load_status.h>
#include <ioj/s7/detail/load_token.h>

namespace ioj::s7::detail {
struct LoadDecision {
    LoadStatus status{LoadStatus::already_loaded};
    // Only admitted loads have a token to complete or abort.
    LoadToken token{};
};
}
