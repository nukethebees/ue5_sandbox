#pragma once
#include <cstdint>
namespace lispb::codegen {
enum class TemplateKind : std::uint8_t {
    invalid,
    parameters,
    explicit_specialization,
    authored_raw
};
}
