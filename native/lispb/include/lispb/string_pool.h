#pragma once

#include <lispb/indices.h>
#include <lispb/string_slice.h>

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>
namespace lispb {

struct StringPool {
    std::string bytes;
    std::vector<StringSlice> slices{{}};
    std::unordered_multimap<std::size_t, StringId> identifiers;
};

}
