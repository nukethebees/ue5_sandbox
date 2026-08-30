#pragma once

#include <codegen/schema/static_table_column_schema.h>
#include <codegen/schema/static_table_row_schema.h>

#include <optional>
#include <string>
#include <vector>

namespace codegen {

struct StaticTableSchema {
    std::string name;
    std::vector<StaticTableRowSchema> rows;
    std::vector<StaticTableColumnSchema> columns;
    std::optional<std::string> export_specifier;
};

} // namespace codegen
