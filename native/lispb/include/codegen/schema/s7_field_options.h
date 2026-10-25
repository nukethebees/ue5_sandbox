#pragma once

#include <lispb/indices.h>

#include <optional>

namespace lispb::codegen {
struct S7FieldOptions {
    std::optional<StringId> name;
    std::optional<StringId> minimum;
    std::optional<StringId> maximum;
    bool required{};
    bool exclude{};
    bool nonzero{};
    bool positive{};
    bool nonnegative{};
    bool finite{};
    bool nonempty{};
};
}
