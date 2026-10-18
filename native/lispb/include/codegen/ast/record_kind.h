#pragma once

#include <cstdint>
#include <string_view>
#include <utility>

namespace lispb::codegen {

enum class RecordKind : std::uint8_t { struct_, class_, union_ };

constexpr auto record_keyword(RecordKind kind) -> std::string_view {
    switch (kind) {
        case RecordKind::struct_:
            return "struct";
        case RecordKind::class_:
            return "class";
        case RecordKind::union_:
            return "union";
    }
    std::unreachable();
}

} // namespace lispb::codegen
