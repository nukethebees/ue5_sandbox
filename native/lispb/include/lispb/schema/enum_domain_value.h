#pragma once

#include <lispb/schema/enum_code.h>

#include <optional>
#include <string>

namespace lispb::schema {
struct EnumDomainValue {
    std::string name;
    bool reserved{};
    std::optional<EnumCode> code;
};
}
