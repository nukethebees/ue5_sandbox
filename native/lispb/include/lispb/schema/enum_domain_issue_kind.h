#pragma once

#include <cstdint>

namespace lispb::schema {

enum class EnumDomainIssueKind : std::uint8_t {
    non_literal,
    implicit_overflow,
    implicit_unknown,
    negative_unsigned,
    width_overflow
};

}
