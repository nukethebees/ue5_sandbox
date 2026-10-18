#pragma once

#include <cstdint>

namespace lispb::schema {
enum class TypeOrigin : std::uint8_t { declaration, registered_external, cpp_spelling };
}
