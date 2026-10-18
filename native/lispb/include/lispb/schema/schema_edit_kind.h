#pragma once

#include <cstdint>

namespace lispb::schema {

enum class SchemaEditKind : std::uint8_t {
    create_declaration,
    replace_declaration,
    delete_declaration,
    create_module,
    delete_module,
    restore_module,
    move_declaration,
    set_enumerator_display_name,
    set_enumerator_name,
    rename_declaration,
};

} // namespace lispb::schema
