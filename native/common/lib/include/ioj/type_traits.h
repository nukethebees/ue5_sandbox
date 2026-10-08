#pragma once

#include <type_traits>

namespace ioj {
template <typename T, typename... Candidates>
concept SameAsAny = (std::is_same_v<T, Candidates> || ...);
}
