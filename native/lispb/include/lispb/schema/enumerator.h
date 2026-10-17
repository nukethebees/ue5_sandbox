#pragma once

#include <optional>
#include <string>

namespace lispb::schema {
struct Enumerator {
    std::string name;
    std::optional<std::string> explicit_value;
    std::optional<std::string> display_name;
    std::optional<std::string> serialized_name;
    bool hidden{};
    bool sentinel{};
    bool count_sentinel{};
};
}
