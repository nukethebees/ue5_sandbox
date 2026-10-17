#pragma once

#include <optional>
#include <string>

namespace lispb::schema {
struct EnumDomainInput {
    std::string name;
    std::optional<std::string> explicit_value;
    bool reserved{};
};
}
