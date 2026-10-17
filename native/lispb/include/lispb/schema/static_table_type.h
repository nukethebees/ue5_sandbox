#pragma once

#include <lispb/schema/record_member.h>
#include <lispb/schema/static_table_group.h>

#include <string>
#include <vector>

namespace lispb::schema {
struct StaticTableType {
    std::vector<std::string> rows;
    std::vector<RecordMember> columns;
    std::vector<StaticTableGroup> groups;
};
}
