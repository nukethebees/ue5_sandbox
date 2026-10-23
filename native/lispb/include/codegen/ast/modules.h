#pragma once

#include <codegen/ast/indices.h>
#include <lispb/indices.h>

#include <cstddef>
#include <vector>

namespace lispb::codegen {

struct Modules {
    auto append(StringId name, CppFileId header = {}, CppFileId source = {}) -> ModuleId;
    // Require a module belonging to this store. Zero denotes an absent module.
    auto index(ModuleId module) const -> std::size_t;
    void clear();

    std::vector<StringId> name;
    std::vector<CppFileId> header;
    std::vector<CppFileId> source;
};

}
