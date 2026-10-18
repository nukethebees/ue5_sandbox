#pragma once

#include <lispb/schema/schema_edit_command.h>

namespace lispb::schema {

struct SchemaEditHistoryEntry {
    SchemaEditCommand forward;
    SchemaEditCommand inverse;
};

} // namespace lispb::schema
