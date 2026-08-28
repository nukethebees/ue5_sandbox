#pragma once

#include <codegen/generator.h>

namespace codegen::detail {

auto lower_soa_module(SoaModuleSchema const& module,
                      std::map<std::string, CppType> const& types) -> Module;
auto lower_homogeneous_module(HomogeneousModuleSchema const& module,
                              std::map<std::string, CppType> const& types) -> Module;
auto lower_vector_module(VectorModuleSchema const& module,
                         std::map<std::string, CppType> const& types) -> Module;
auto lower_facade_module(FacadeModuleSchema const& module,
                         std::map<std::string, CppType> const& types) -> Module;

} // namespace codegen::detail
