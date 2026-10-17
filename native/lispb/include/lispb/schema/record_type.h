#pragma once

#include <lispb/schema/record_member.h>

#include <vector>

namespace lispb::schema {
struct RecordType {
    std::vector<RecordMember> members;
};
}
