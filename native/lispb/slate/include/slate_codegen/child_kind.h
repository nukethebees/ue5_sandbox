#pragma once

#include <cstdint>
namespace slate_codegen::detail {

enum class ChildKind : std::uint8_t { none, widget, box, existing, call };

}
