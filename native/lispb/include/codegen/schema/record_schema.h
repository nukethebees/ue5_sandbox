#pragma once

#include <codegen/schema/function_schema.h>
#include <codegen/schema/record_member_schema.h>

#include <optional>
#include <string>
#include <vector>

namespace codegen {

enum class RecordComparison { none, equality, three_way };

struct RecordSchema {
    std::string name;
    std::vector<RecordMemberSchema> members;
    std::optional<std::string> export_specifier;
    RecordComparison comparison{RecordComparison::none};
    bool comparison_noexcept{false};
    std::vector<FunctionSchema> functions{};
};

} // namespace codegen
