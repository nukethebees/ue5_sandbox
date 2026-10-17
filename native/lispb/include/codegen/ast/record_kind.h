#pragma once

#include <stdexcept>
#include <string_view>

namespace codegen {
enum class RecordKind { struct_, class_, union_ };

constexpr auto record_keyword(RecordKind kind) -> std::string_view {
    switch (kind) {
        case RecordKind::struct_:
            return "struct";
        case RecordKind::class_:
            return "class";
        case RecordKind::union_:
            return "union";
    }
    throw std::logic_error{"Invalid C++ record kind"};
}
}
