#pragma once

#include <codegen/schema/record_member_schema.h>

#include <optional>
#include <string>
#include <vector>

namespace codegen {

struct RecordSchema {
    std::string name;
    std::vector<RecordMemberSchema> members;
    std::optional<std::string> export_specifier;
};

} // namespace codegen
