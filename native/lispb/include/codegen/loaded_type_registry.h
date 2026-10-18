#pragma once

#include <codegen/schema/type_registry.h>
#include <codegen/sexpr/indices.h>
#include <lispb/indices.h>

#include <map>
#include <vector>
namespace lispb::codegen {

struct LoadedTypeRegistry {
    TypeRegistry types;
    std::vector<sexpr::SourceFileId> sources;
    std::map<lispb::IdentifierId, sexpr::FormId> declarations;
};

}
