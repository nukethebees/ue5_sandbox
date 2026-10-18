#pragma once

#include <codegen/schema/module_handle.h>
#include <codegen/sexpr/handle_range.h>
#include <codegen/sexpr/indices.h>

#include <vector>
namespace lispb::codegen {

struct LoadedModuleSource {
    sexpr::SourceFileId file;
    sexpr::HandleRange forms;
    std::vector<ModuleHandle> modules;
};

}
