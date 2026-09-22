#pragma once

#include <codegen/generator.h>
#include <lispb/schema/type_graph.h>

namespace codegen::detail {

auto lower_enum_module(EnumModuleSchema const& module, std::map<std::string, CppType> const& types)
    -> std::vector<Module>;
auto lower_packed_value_module(PackedValueModuleSchema const& module,
                               std::map<std::string, CppType> const& types,
                               lispb::schema::TypeGraph const& type_graph) -> Module;
auto lower_record_module(RecordModuleSchema const& module,
                         std::map<std::string, CppType> const& types) -> Module;
auto lower_union_module(UnionModuleSchema const& module,
                        std::map<std::string, CppType> const& types) -> Module;
auto lower_soa_module(SoaModuleSchema const& module,
                      std::map<std::string, CppType> const& types,
                      lispb::schema::TypeGraph const& type_graph) -> Module;
auto lower_static_table_module(StaticTableModuleSchema const& module,
                               std::map<std::string, CppType> const& types) -> Module;
auto lower_homogeneous_module(HomogeneousModuleSchema const& module,
                              std::map<std::string, CppType> const& types) -> Module;
auto lower_vector_module(VectorModuleSchema const& module,
                         std::map<std::string, CppType> const& types) -> Module;
auto lower_facade_module(FacadeModuleSchema const& module,
                         std::map<std::string, CppType> const& types) -> Module;
auto lower_settings_module(SettingsModuleSchema const& module,
                           std::map<std::string, CppType> const& types) -> Module;

} // namespace codegen::detail
