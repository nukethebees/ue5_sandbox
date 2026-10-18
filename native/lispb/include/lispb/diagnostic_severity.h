#pragma once

#include <cstdint>

namespace lispb {

enum class DiagnosticSeverity : std::uint8_t { note, warning, error };

}
