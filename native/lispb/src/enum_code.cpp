#include <lispb/schema/enum_code.h>

namespace lispb::schema {
auto format_enum_code(EnumCode const value) -> std::string {
    return (value.negative ? "-" : "") + std::to_string(value.magnitude);
}
}
