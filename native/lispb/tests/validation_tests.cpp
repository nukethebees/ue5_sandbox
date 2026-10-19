#include "test_utilities.h"

#include <codegen/ast/cpp_files.h>
#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/modules.h>
#include <codegen/ast/type_operation.h>
#include <codegen/generator.h>
#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/enum_conversion.h>
#include <codegen/schema/enum_reflection.h>
#include <codegen/schema/enum_schemas.h>
#include <codegen/schema/enum_unreal_projections.h>
#include <codegen/schema/enumerator_schemas.h>
#include <codegen/schema/facade_method_schemas.h>
#include <codegen/schema/facade_schemas.h>
#include <codegen/schema/fixed_point_rounding.h>
#include <codegen/schema/fixed_point_schemas.h>
#include <codegen/schema/fixed_soa_schemas.h>
#include <codegen/schema/function_schemas.h>
#include <codegen/schema/homogeneous_layout_schemas.h>
#include <codegen/schema/homogeneous_value_schemas.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/integer_scalar_cpp_emission.h>
#include <codegen/schema/integer_scalar_schemas.h>
#include <codegen/schema/integer_varint_encoding.h>
#include <codegen/schema/integer_varint_schemas.h>
#include <codegen/schema/linear_quantized_schemas.h>
#include <codegen/schema/manifest.h>
#include <codegen/schema/mini_float_schemas.h>
#include <codegen/schema/module_settings_store.h>
#include <codegen/schema/native_enum_helper.h>
#include <codegen/schema/normal_module_schemas.h>
#include <codegen/schema/optional_presence_bit_schemas.h>
#include <codegen/schema/optional_sentinel_schemas.h>
#include <codegen/schema/packed_named_code_schemas.h>
#include <codegen/schema/parameter_schemas.h>
#include <codegen/schema/quantization_clipping.h>
#include <codegen/schema/record_member_schemas.h>
#include <codegen/schema/record_schemas.h>
#include <codegen/schema/schema_version.h>
#include <codegen/schema/semantic_relation_kind.h>
#include <codegen/schema/semantic_relation_schemas.h>
#include <codegen/schema/semantic_relation_unit.h>
#include <codegen/schema/setting_apply_mode.h>
#include <codegen/schema/setting_category_schemas.h>
#include <codegen/schema/setting_control_kind.h>
#include <codegen/schema/setting_control_schemas.h>
#include <codegen/schema/setting_schemas.h>
#include <codegen/schema/settings_module_schemas.h>
#include <codegen/schema/soa_allocators.h>
#include <codegen/schema/soa_backend.h>
#include <codegen/schema/soa_mask_dimension_schemas.h>
#include <codegen/schema/soa_member_kind.h>
#include <codegen/schema/soa_member_schemas.h>
#include <codegen/schema/soa_schemas.h>
#include <codegen/schema/static_table_column_schemas.h>
#include <codegen/schema/static_table_group_schemas.h>
#include <codegen/schema/static_table_row_schemas.h>
#include <codegen/schema/static_table_schemas.h>
#include <codegen/schema/type_refs.h>
#include <codegen/schema/umbrella_module_schemas.h>
#include <codegen/schema/vector_soa_schemas.h>
#include <codegen/sexpr/source_error.h>
#include <lispb/indices.h>
#include <lispb/project.h>
#include <lispb/schema/enum_signedness.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lispb::codegen {
namespace {

template <typename T>
auto manifest_with(CompilerTestContext& context,
                   T module,
                   std::vector<std::pair<std::string_view, CppTypeId>> const& types = {})
    -> Manifest {
    return lispb::codegen::Manifest{manifest_schema_version,
                                    make_test_registry(context, types),
                                    context.storage.declarations,
                                    {std::move(module)}};
}

auto valid_soa_module(CompilerTestContext& context) -> lispb::codegen::ModuleHandle {
    return [&] {
        auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
        context.storage.declarations.normal_modules
            .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] = [&] {
            auto const new_module_settings_store{
                context.storage.declarations.module_settings_store.append()};
            context.storage.declarations.module_settings_store
                .name[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.intern("soa");
            context.storage.declarations.module_settings_store
                .header[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.add("Soa.h");
            context.storage.declarations.module_settings_store
                .source[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.add("Soa.cpp");
            return new_module_settings_store;
        }();
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(new_normal_modules)] =
            context.storage.declarations.range<lispb::codegen::DeclarationHandle>({[&] {
                auto const new_soas{context.storage.declarations.soas.append()};
                context.storage.declarations.soas
                    .name[context.storage.declarations.soas.index(new_soas)] =
                    context.storage.strings.intern("FData");
                context.storage.declarations.soas
                    .members[context.storage.declarations.soas.index(new_soas)] =
                    context.storage.declarations.range<lispb::codegen::SoaMemberId>({[&] {
                        auto const new_soa_members{
                            context.storage.declarations.soa_members.append()};
                        context.storage.declarations.soa_members
                            .name[context.storage.declarations.soa_members.index(new_soa_members)] =
                            context.storage.strings.intern("values");
                        context.storage.declarations.soa_members
                            .kind[context.storage.declarations.soa_members.index(new_soa_members)] =
                            SoaMemberKind::array;
                        context.storage.declarations.soa_members
                            .type[context.storage.declarations.soa_members.index(
                                new_soa_members)] = [&] {
                            auto const new_type_refs{
                                context.storage.declarations.type_refs.append()};
                            context.storage.declarations.type_refs
                                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                                context.storage.strings.intern("int32");
                            return new_type_refs;
                        }();
                        return new_soa_members;
                    }()});
                return new_soas;
            }()});
        return new_normal_modules;
    }();
}

TEST(Validation, FunctionQualifiersAreSharedByRecordsAndAllSoaReceivers) {
    CompilerTestContext context;
    auto const valid{context.storage.declarations.functions.append()};
    context.storage.declarations.functions
        .name[context.storage.declarations.functions.index(valid)] =
        context.storage.strings.intern("read");
    context.storage.declarations.functions
        .return_type[context.storage.declarations.functions.index(valid)] = [&] {
        auto const new_type_refs{context.storage.declarations.type_refs.append()};
        context.storage.declarations.type_refs
            .name[context.storage.declarations.type_refs.index(new_type_refs)] =
            context.storage.strings.intern("int");
        return new_type_refs;
    }();
    context.storage.declarations.functions
        .body_lines[context.storage.declarations.functions.index(valid)] =
        context.storage.declarations.range<lispb::StringId>(
            {context.storage.strings.add("return 7;")});
    context.storage.declarations.functions
        .is_const[context.storage.declarations.functions.index(valid)] = true;
    context.storage.declarations.functions
        .is_constexpr[context.storage.declarations.functions.index(valid)] = true;
    context.storage.declarations.functions
        .is_nodiscard[context.storage.declarations.functions.index(valid)] = true;
    auto check = [&](lispb::codegen::FunctionSchemaId const& function,
                     std::string_view const diagnostic) {
        for (int receiver{}; receiver < 4; ++receiver) {
            SCOPED_TRACE(receiver);
            SCOPED_TRACE(diagnostic);
            auto module{valid_soa_module(context)};
            if (receiver == 0) {
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(module)] =
                    context.storage.declarations.range<lispb::codegen::DeclarationHandle>({[&] {
                        auto const new_records{context.storage.declarations.records.append()};
                        context.storage.declarations.records
                            .name[context.storage.declarations.records.index(new_records)] =
                            context.storage.strings.intern("Record");
                        context.storage.declarations.records
                            .functions[context.storage.declarations.records.index(new_records)] =
                            context.storage.declarations.range<lispb::codegen::FunctionSchemaId>(
                                {function});
                        return new_records;
                    }()});
            } else {

                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.normal_modules
                            .declarations[context.storage.declarations.normal_modules.index(
                                module)])};
                    auto rows{std::vector(original_items.begin(), original_items.end())};
                    rows.front() = [&] {
                        auto soa{context.storage.declarations.soas.clone(rows.front())};
                        auto& functions{
                            receiver == 1
                                ? context.storage.declarations.soas
                                      .functions[context.storage.declarations.soas.index(soa)]
                            : receiver == 2
                                ? context.storage.declarations.soas
                                      .const_view_functions[context.storage.declarations.soas.index(
                                          soa)]
                                : context.storage.declarations.soas.mutable_view_functions
                                      [context.storage.declarations.soas.index(soa)]};
                        functions =
                            context.storage.declarations.range<FunctionSchemaId>({function});
                        return soa;
                    }();
                    return context.storage.declarations.range(rows);
                }();
            }
            auto const manifest{manifest_with(context, module)};
            if (diagnostic.empty()) {
                EXPECT_NO_THROW(lispb::schema::resolve_type_graph(manifest));
            } else {
                try {
                    lispb::schema::resolve_type_graph(manifest);
                    FAIL() << "Expected " << diagnostic;
                } catch (std::invalid_argument const& error) {
                    EXPECT_TRUE(std::string{error.what()}.contains(diagnostic)) << error.what();
                }
            }
        }
    };
    check(valid, "");
    auto function{context.storage.declarations.functions.clone(valid)};
    context.storage.declarations.functions
        .body_lines[context.storage.declarations.functions.index(function)] = {};
    check(function, "constexpr function requires a generated header body");
    function = context.storage.declarations.functions.clone(valid);
    context.storage.declarations.functions
        .definition_in_source[context.storage.declarations.functions.index(function)] = true;
    check(function, "constexpr function requires a generated header body");
    function = context.storage.declarations.functions.clone(valid);
    context.storage.declarations.functions
        .return_type[context.storage.declarations.functions.index(function)] = [&] {
        auto const new_type_refs{context.storage.declarations.type_refs.append()};
        context.storage.declarations.type_refs
            .name[context.storage.declarations.type_refs.index(new_type_refs)] =
            context.storage.strings.intern("void");
        return new_type_refs;
    }();
    context.storage.declarations.functions
        .body_lines[context.storage.declarations.functions.index(function)] =
        context.storage.declarations.range<lispb::StringId>(
            {context.storage.strings.add("return;")});
    check(function, "nodiscard function must not return void");
    context.storage.declarations.functions
        .return_type[context.storage.declarations.functions.index(function)] = [&] {
        auto const new_type_refs{context.storage.declarations.type_refs.append()};
        context.storage.declarations.type_refs
            .name[context.storage.declarations.type_refs.index(new_type_refs)] =
            context.storage.strings.intern("auto");
        return new_type_refs;
    }();
    context.storage.declarations.functions
        .trailing_return_type[context.storage.declarations.functions.index(function)] = [&] {
        auto const new_type_refs{context.storage.declarations.type_refs.append()};
        context.storage.declarations.type_refs
            .name[context.storage.declarations.type_refs.index(new_type_refs)] =
            context.storage.strings.intern("void");
        return new_type_refs;
    }();
    check(function, "nodiscard function must not return void");
    function = context.storage.declarations.functions.clone(valid);
    context.storage.declarations.functions
        .is_static[context.storage.declarations.functions.index(function)] = true;
    check(function, "must not be both static and const");
    function = context.storage.declarations.functions.clone(valid);
    context.storage.declarations.functions
        .is_constexpr[context.storage.declarations.functions.index(function)] = false;
    context.storage.declarations.functions
        .is_inline[context.storage.declarations.functions.index(function)] = true;
    context.storage.declarations.functions
        .definition_in_source[context.storage.declarations.functions.index(function)] = true;
    check(function, "must not be both inline and defined in the source");
    context.storage.declarations.functions
        .is_inline[context.storage.declarations.functions.index(function)] = false;
    context.storage.declarations.functions
        .template_parameters[context.storage.declarations.functions.index(function)] =
        context.storage.strings.add("typename T");
    check(function, "function templates must be defined in the header");

    function = context.storage.declarations.functions.clone(valid);
    context.storage.declarations.functions
        .return_type[context.storage.declarations.functions.index(function)] = [&] {
        auto const new_type_refs{context.storage.declarations.type_refs.append()};
        context.storage.declarations.type_refs
            .name[context.storage.declarations.type_refs.index(new_type_refs)] =
            context.storage.strings.intern("void");
        context.storage.declarations.type_refs
            .suffix[context.storage.declarations.type_refs.index(new_type_refs)] =
            context.storage.strings.add("*");
        return new_type_refs;
    }();
    context.storage.declarations.functions
        .body_lines[context.storage.declarations.functions.index(function)] =
        context.storage.declarations.range<lispb::StringId>(
            {context.storage.strings.add("return nullptr;")});
    check(function, "");
    context.storage.declarations.functions
        .return_type[context.storage.declarations.functions.index(function)] = [&] {
        auto const new_type_refs{context.storage.declarations.type_refs.append()};
        context.storage.declarations.type_refs
            .name[context.storage.declarations.type_refs.index(new_type_refs)] =
            context.storage.strings.intern("auto");
        return new_type_refs;
    }();
    context.storage.declarations.functions
        .trailing_return_type[context.storage.declarations.functions.index(function)] = [&] {
        auto const new_type_refs{context.storage.declarations.type_refs.append()};
        context.storage.declarations.type_refs
            .name[context.storage.declarations.type_refs.index(new_type_refs)] =
            context.storage.strings.intern("void");
        context.storage.declarations.type_refs
            .suffix[context.storage.declarations.type_refs.index(new_type_refs)] =
            context.storage.strings.add("*");
        return new_type_refs;
    }();
    check(function, "");
    context.storage.declarations.functions
        .is_constexpr[context.storage.declarations.functions.index(function)] = false;
    context.storage.declarations.functions
        .body_lines[context.storage.declarations.functions.index(function)] = {};
    check(function, "");
}

auto valid_mask_soa_module(CompilerTestContext& context) -> lispb::codegen::ModuleHandle {
    auto module{valid_soa_module(context)};

    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.soas.clone(rows_1.front())};
            context.storage.declarations.soas
                .field_mask_name[context.storage.declarations.soas.index(row_0)] =
                context.storage.strings.intern("FFieldMask");
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.soas.clone(rows_1.front())};
            context.storage.declarations.soas
                .field_enum_name[context.storage.declarations.soas.index(row_0)] =
                context.storage.strings.intern("EField");
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.soas.clone(rows_1.front())};
            context.storage.declarations.soas
                .members[context.storage.declarations.soas.index(row_0)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.soas
                        .members[context.storage.declarations.soas.index(row_0)])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.insert(rows.begin(), [&] {
                    auto const new_soa_members{context.storage.declarations.soa_members.append()};
                    context.storage.declarations.soa_members
                        .name[context.storage.declarations.soa_members.index(new_soa_members)] =
                        context.storage.strings.intern("masks");
                    context.storage.declarations.soa_members
                        .kind[context.storage.declarations.soa_members.index(new_soa_members)] =
                        SoaMemberKind::array;
                    context.storage.declarations.soa_members
                        .type[context.storage.declarations.soa_members.index(new_soa_members)] =
                        [&] {
                            auto const new_type_refs{
                                context.storage.declarations.type_refs.append()};
                            context.storage.declarations.type_refs
                                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                                context.storage.strings.intern("FFieldMask");
                            return new_type_refs;
                        }();
                    return new_soa_members;
                }());
                return context.storage.declarations.range(rows);
            }();
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.soas.clone(rows_3.front())};
            context.storage.declarations.soas
                .members[context.storage.declarations.soas.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.soas
                        .members[context.storage.declarations.soas.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.back() = [&] {
                    auto row_0{context.storage.declarations.soa_members.clone(rows_1.back())};
                    context.storage.declarations.soa_members
                        .mask_field[context.storage.declarations.soa_members.index(row_0)] = true;
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    return module;
}

auto valid_vector_module(CompilerTestContext& context) -> lispb::codegen::ModuleHandle {
    return [&] {
        auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
        context.storage.declarations.normal_modules
            .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] = [&] {
            auto const new_module_settings_store{
                context.storage.declarations.module_settings_store.append()};
            context.storage.declarations.module_settings_store
                .name[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.intern("vectors");
            context.storage.declarations.module_settings_store
                .header[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.add("Vectors.h");
            context.storage.declarations.module_settings_store
                .source[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.add("Vectors.cpp");
            return new_module_settings_store;
        }();
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(new_normal_modules)] =
            context.storage.declarations.range<lispb::codegen::DeclarationHandle>({[&] {
                auto const new_vector_soas{context.storage.declarations.vector_soas.append()};
                context.storage.declarations.vector_soas
                    .name[context.storage.declarations.vector_soas.index(new_vector_soas)] =
                    context.storage.strings.intern("FVectors");
                context.storage.declarations.vector_soas
                    .value_type[context.storage.declarations.vector_soas.index(new_vector_soas)] =
                    [&] {
                        auto const new_type_refs{context.storage.declarations.type_refs.append()};
                        context.storage.declarations.type_refs
                            .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                            context.storage.strings.intern("float");
                        return new_type_refs;
                    }();
                context.storage.declarations.vector_soas
                    .components[context.storage.declarations.vector_soas.index(new_vector_soas)] =
                    context.storage.declarations.range<lispb::IdentifierId>(
                        {context.storage.strings.intern("xs"),
                         context.storage.strings.intern("ys")});
                context.storage.declarations.vector_soas
                    .equivalent_type[context.storage.declarations.vector_soas.index(
                        new_vector_soas)] = [&] {
                    auto const new_type_refs{context.storage.declarations.type_refs.append()};
                    context.storage.declarations.type_refs
                        .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                        context.storage.strings.intern("FVector2f");
                    return new_type_refs;
                }();
                return new_vector_soas;
            }()});
        return new_normal_modules;
    }();
}

auto valid_facade_module(CompilerTestContext& context) -> lispb::codegen::ModuleHandle {
    return [&] {
        auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
        context.storage.declarations.normal_modules
            .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] = [&] {
            auto const new_module_settings_store{
                context.storage.declarations.module_settings_store.append()};
            context.storage.declarations.module_settings_store
                .name[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.intern("facade");
            context.storage.declarations.module_settings_store
                .header[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.add("Facade.h");
            return new_module_settings_store;
        }();
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(new_normal_modules)] =
            context.storage.declarations.range<lispb::codegen::DeclarationHandle>({[&] {
                auto const new_facades{context.storage.declarations.facades.append()};
                context.storage.declarations.facades
                    .name[context.storage.declarations.facades.index(new_facades)] =
                    context.storage.strings.intern("FFacade");
                context.storage.declarations.facades
                    .target_type[context.storage.declarations.facades.index(new_facades)] = [&] {
                    auto const new_type_refs{context.storage.declarations.type_refs.append()};
                    context.storage.declarations.type_refs
                        .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                        context.storage.strings.intern("FTarget");
                    return new_type_refs;
                }();
                context.storage.declarations.facades
                    .target_member_name[context.storage.declarations.facades.index(new_facades)] =
                    context.storage.strings.intern("target");
                context.storage.declarations.facades
                    .methods[context.storage.declarations.facades.index(new_facades)] =
                    context.storage.declarations.range<lispb::codegen::FacadeMethodId>({[&] {
                        auto const new_facade_methods{
                            context.storage.declarations.facade_methods.append()};
                        context.storage.declarations.facade_methods
                            .name[context.storage.declarations.facade_methods.index(
                                new_facade_methods)] = context.storage.strings.intern("reset");
                        context.storage.declarations.facade_methods
                            .return_type[context.storage.declarations.facade_methods.index(
                                new_facade_methods)] = [&] {
                            auto const new_type_refs{
                                context.storage.declarations.type_refs.append()};
                            context.storage.declarations.type_refs
                                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                                context.storage.strings.intern("void");
                            return new_type_refs;
                        }();
                        return new_facade_methods;
                    }()});
                return new_facades;
            }()});
        return new_normal_modules;
    }();
}

auto valid_homogeneous_module(CompilerTestContext& context) -> lispb::codegen::ModuleHandle {
    return [&] {
        auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
        context.storage.declarations.normal_modules
            .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] = [&] {
            auto const new_module_settings_store{
                context.storage.declarations.module_settings_store.append()};
            context.storage.declarations.module_settings_store
                .name[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.intern("homogeneous");
            context.storage.declarations.module_settings_store
                .header[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.add("Values.h");
            context.storage.declarations.module_settings_store
                .source[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.add("Values.cpp");
            return new_module_settings_store;
        }();
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(new_normal_modules)] =
            context.storage.declarations.range<lispb::codegen::DeclarationHandle>({[&] {
                auto const new_homogeneous_layouts{
                    context.storage.declarations.homogeneous_layouts.append()};
                context.storage.declarations.homogeneous_layouts
                    .name[context.storage.declarations.homogeneous_layouts.index(
                        new_homogeneous_layouts)] = context.storage.strings.intern("Values");
                context.storage.declarations.homogeneous_layouts
                    .components[context.storage.declarations.homogeneous_layouts.index(
                        new_homogeneous_layouts)] =
                    context.storage.declarations.range<lispb::IdentifierId>(
                        {context.storage.strings.intern("xs"),
                         context.storage.strings.intern("ys")});
                context.storage.declarations.homogeneous_layouts
                    .value_types[context.storage.declarations.homogeneous_layouts.index(
                        new_homogeneous_layouts)] =
                    context.storage.declarations.range<lispb::codegen::HomogeneousValueId>({[&] {
                        auto const new_homogeneous_values{
                            context.storage.declarations.homogeneous_values.append()};
                        context.storage.declarations.homogeneous_values
                            .type[context.storage.declarations.homogeneous_values.index(
                                new_homogeneous_values)] = [&] {
                            auto const new_type_refs{
                                context.storage.declarations.type_refs.append()};
                            context.storage.declarations.type_refs
                                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                                context.storage.strings.intern("float");
                            return new_type_refs;
                        }();
                        context.storage.declarations.homogeneous_values
                            .suffix[context.storage.declarations.homogeneous_values.index(
                                new_homogeneous_values)] = context.storage.strings.add("f");
                        return new_homogeneous_values;
                    }()});
                return new_homogeneous_layouts;
            }()});
        return new_normal_modules;
    }();
}

auto normal_module(CompilerTestContext& context,
                   std::vector<DeclarationHandle> declarations,
                   std::optional<std::string_view> source = std::nullopt)
    -> lispb::codegen::ModuleHandle {
    return [&] {
        auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
        context.storage.declarations.normal_modules
            .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] = [&] {
            auto const new_module_settings_store{
                context.storage.declarations.module_settings_store.append()};
            context.storage.declarations.module_settings_store
                .name[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.intern("mixed");
            context.storage.declarations.module_settings_store
                .header[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.add("Mixed.h");
            context.storage.declarations.module_settings_store
                .source[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] =
                source ? std::optional{context.storage.strings.add(*source)} : std::nullopt;
            return new_module_settings_store;
        }();
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(new_normal_modules)] =
            context.storage.declarations.range<lispb::codegen::DeclarationHandle>(declarations);
        return new_normal_modules;
    }();
}

auto normal_soa_module(CompilerTestContext& context,
                       lispb::codegen::DeclarationHandle schema,
                       std::vector<lispb::codegen::SoaAllocatorVariantId> const& allocators = {},
                       SoaBackend backend = SoaBackend::unreal) -> lispb::codegen::ModuleHandle {
    std::vector<SoaAllocatorVariantId> allocator_handles;
    for (auto const& allocator : allocators) {
        allocator_handles.push_back(allocator);
    }
    auto const allocator_ids{
        context.storage.declarations.range<SoaAllocatorVariantId>(allocator_handles)};
    return [&] {
        auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
        context.storage.declarations.normal_modules
            .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] = [&] {
            auto const new_module_settings_store{
                context.storage.declarations.module_settings_store.append()};
            context.storage.declarations.module_settings_store
                .name[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.intern("mixed");
            context.storage.declarations.module_settings_store
                .header[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.add("Mixed.h");
            context.storage.declarations.module_settings_store
                .source[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.add("Mixed.cpp");
            return new_module_settings_store;
        }();
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(new_normal_modules)] =
            context.storage.declarations.range<lispb::codegen::DeclarationHandle>({schema});
        context.storage.declarations.normal_modules
            .soa_backend[context.storage.declarations.normal_modules.index(new_normal_modules)] =
            backend;
        context.storage.declarations.normal_modules
            .soa_array_allocators[context.storage.declarations.normal_modules.index(
                new_normal_modules)] = allocator_ids;
        return new_normal_modules;
    }();
}

auto plain_soa(CompilerTestContext& context, std::string name = "Data")
    -> lispb::codegen::DeclarationHandle {
    return [&] {
        auto const new_soas{context.storage.declarations.soas.append()};
        context.storage.declarations.soas.name[context.storage.declarations.soas.index(new_soas)] =
            context.storage.strings.intern(std::move(name));
        context.storage.declarations.soas
            .members[context.storage.declarations.soas.index(new_soas)] =
            context.storage.declarations.range<lispb::codegen::SoaMemberId>({[&] {
                auto const new_soa_members{context.storage.declarations.soa_members.append()};
                context.storage.declarations.soa_members
                    .name[context.storage.declarations.soa_members.index(new_soa_members)] =
                    context.storage.strings.intern("values");
                context.storage.declarations.soa_members
                    .kind[context.storage.declarations.soa_members.index(new_soa_members)] =
                    SoaMemberKind::array;
                context.storage.declarations.soa_members
                    .type[context.storage.declarations.soa_members.index(new_soa_members)] = [&] {
                    auto const new_type_refs{context.storage.declarations.type_refs.append()};
                    context.storage.declarations.type_refs
                        .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                        context.storage.strings.intern("int32");
                    return new_type_refs;
                }();
                return new_soa_members;
            }()});
        return new_soas;
    }();
}

auto valid_enum_module(CompilerTestContext& context) -> lispb::codegen::ModuleHandle {
    return [&] {
        auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
        context.storage.declarations.normal_modules
            .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] = [&] {
            auto const new_module_settings_store{
                context.storage.declarations.module_settings_store.append()};
            context.storage.declarations.module_settings_store
                .name[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.intern("enums");
            context.storage.declarations.module_settings_store
                .header[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.add("Enums.h");
            context.storage.declarations.module_settings_store
                .source[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.add("Enums.cpp");
            return new_module_settings_store;
        }();
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(new_normal_modules)] =
            context.storage.declarations.range<lispb::codegen::DeclarationHandle>({[&] {
                auto const new_enums{context.storage.declarations.enums.append()};
                context.storage.declarations.enums
                    .name[context.storage.declarations.enums.index(new_enums)] =
                    context.storage.strings.intern("EMode");
                context.storage.declarations.enums
                    .underlying_type[context.storage.declarations.enums.index(new_enums)] = [&] {
                    auto const new_type_refs{context.storage.declarations.type_refs.append()};
                    context.storage.declarations.type_refs
                        .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                        context.storage.strings.intern("uint8");
                    return new_type_refs;
                }();
                context.storage.declarations.enums
                    .values[context.storage.declarations.enums.index(new_enums)] =
                    context.storage.declarations.range<lispb::codegen::EnumeratorId>({[&] {
                        auto const new_enumerators{
                            context.storage.declarations.enumerators.append()};
                        context.storage.declarations.enumerators
                            .name[context.storage.declarations.enumerators.index(new_enumerators)] =
                            context.storage.strings.intern("Value");
                        return new_enumerators;
                    }()});
                context.storage.declarations.enums
                    .conversions[context.storage.declarations.enums.index(new_enums)] =
                    context.storage.declarations.range<lispb::codegen::EnumConversion>(
                        {EnumConversion::string_view});
                context.storage.declarations.enums
                    .reflection[context.storage.declarations.enums.index(new_enums)] =
                    EnumReflection::uenum;
                return new_enums;
            }()});
        context.storage.declarations.normal_modules
            .enum_helper_namespace[context.storage.declarations.normal_modules.index(
                new_normal_modules)] = context.storage.strings.intern("project");
        return new_normal_modules;
    }();
}

auto valid_native_enum_module(CompilerTestContext& context) -> lispb::codegen::ModuleHandle {
    return [&] {
        auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
        context.storage.declarations.normal_modules
            .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] = [&] {
            auto const new_module_settings_store{
                context.storage.declarations.module_settings_store.append()};
            context.storage.declarations.module_settings_store
                .name[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.intern("native_enums");
            context.storage.declarations.module_settings_store
                .header[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.add("NativeEnums.h");
            context.storage.declarations.module_settings_store
                .namespace_name[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.intern("project");
            return new_module_settings_store;
        }();
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(new_normal_modules)] =
            context.storage.declarations.range<lispb::codegen::DeclarationHandle>({[&] {
                auto const new_enums{context.storage.declarations.enums.append()};
                context.storage.declarations.enums
                    .name[context.storage.declarations.enums.index(new_enums)] =
                    context.storage.strings.intern("NativeMode");
                context.storage.declarations.enums
                    .underlying_type[context.storage.declarations.enums.index(new_enums)] = [&] {
                    auto const new_type_refs{context.storage.declarations.type_refs.append()};
                    context.storage.declarations.type_refs
                        .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                        context.storage.strings.intern("std::uint8_t");
                    return new_type_refs;
                }();
                context.storage.declarations.enums
                    .values[context.storage.declarations.enums.index(new_enums)] =
                    context.storage.declarations.range<lispb::codegen::EnumeratorId>({[&] {
                        auto const new_enumerators{
                            context.storage.declarations.enumerators.append()};
                        context.storage.declarations.enumerators
                            .name[context.storage.declarations.enumerators.index(new_enumerators)] =
                            context.storage.strings.intern("Idle");
                        context.storage.declarations.enumerators
                            .initializer[context.storage.declarations.enumerators.index(
                                new_enumerators)] = context.storage.strings.add("0");
                        context.storage.declarations.enumerators
                            .display_name[context.storage.declarations.enumerators.index(
                                new_enumerators)] = std::nullopt;
                        context.storage.declarations.enumerators
                            .hidden[context.storage.declarations.enumerators.index(
                                new_enumerators)] = false;
                        context.storage.declarations.enumerators
                            .serialized_name[context.storage.declarations.enumerators.index(
                                new_enumerators)] = context.storage.strings.add("idle");
                        return new_enumerators;
                    }()});
                context.storage.declarations.enums
                    .native_api[context.storage.declarations.enums.index(new_enums)] = true;
                return new_enums;
            }()});
        return new_normal_modules;
    }();
}

auto valid_integer_scalar_module(CompilerTestContext& context) -> lispb::codegen::ModuleHandle {
    return [&] {
        auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
        context.storage.declarations.normal_modules
            .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] = [&] {
            auto const new_module_settings_store{
                context.storage.declarations.module_settings_store.append()};
            context.storage.declarations.module_settings_store
                .name[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.intern("scalars");
            context.storage.declarations.module_settings_store
                .header[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.add("Scalars.h");
            return new_module_settings_store;
        }();
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(new_normal_modules)] =
            context.storage.declarations.range<lispb::codegen::DeclarationHandle>({[&] {
                auto const new_integer_scalars{
                    context.storage.declarations.integer_scalars.append()};
                context.storage.declarations.integer_scalars
                    .name[context.storage.declarations.integer_scalars.index(new_integer_scalars)] =
                    context.storage.strings.intern("DamageReason");
                context.storage.declarations.integer_scalars
                    .signedness[context.storage.declarations.integer_scalars.index(
                        new_integer_scalars)] = false;
                context.storage.declarations.integer_scalars
                    .minimum_value[context.storage.declarations.integer_scalars.index(
                        new_integer_scalars)] = 0;
                context.storage.declarations.integer_scalars
                    .maximum_value[context.storage.declarations.integer_scalars.index(
                        new_integer_scalars)] = 10;
                context.storage.declarations.integer_scalars
                    .bit_width[context.storage.declarations.integer_scalars.index(
                        new_integer_scalars)] = std::nullopt;
                context.storage.declarations.integer_scalars
                    .named_codes[context.storage.declarations.integer_scalars.index(
                        new_integer_scalars)] =
                    context.storage.declarations.range<lispb::codegen::PackedNamedCodeId>(
                        {[&] {
                             auto const new_packed_named_codes{
                                 context.storage.declarations.packed_named_codes.append()};
                             context.storage.declarations.packed_named_codes
                                 .name[context.storage.declarations.packed_named_codes.index(
                                     new_packed_named_codes)] =
                                 context.storage.strings.intern("Unknown");
                             context.storage.declarations.packed_named_codes
                                 .value[context.storage.declarations.packed_named_codes.index(
                                     new_packed_named_codes)] = 0;
                             context.storage.declarations.packed_named_codes
                                 .sentinel[context.storage.declarations.packed_named_codes.index(
                                     new_packed_named_codes)] = false;
                             return new_packed_named_codes;
                         }(),
                         [&] {
                             auto const new_packed_named_codes{
                                 context.storage.declarations.packed_named_codes.append()};
                             context.storage.declarations.packed_named_codes
                                 .name[context.storage.declarations.packed_named_codes.index(
                                     new_packed_named_codes)] =
                                 context.storage.strings.intern("Invalid");
                             context.storage.declarations.packed_named_codes
                                 .value[context.storage.declarations.packed_named_codes.index(
                                     new_packed_named_codes)] = 15;
                             context.storage.declarations.packed_named_codes
                                 .sentinel[context.storage.declarations.packed_named_codes.index(
                                     new_packed_named_codes)] = true;
                             return new_packed_named_codes;
                         }()});
                return new_integer_scalars;
            }()});
        return new_normal_modules;
    }();
}

auto valid_linear_quantized_manifest(CompilerTestContext& context) -> Manifest {
    auto scalar{valid_integer_scalar_module(context)};
    auto const representations{context.storage.declarations.normal_modules.append()};
    context.storage.declarations.normal_modules
        .settings[context.storage.declarations.normal_modules.index(representations)] = [&] {
        auto const new_module_settings_store{
            context.storage.declarations.module_settings_store.append()};
        context.storage.declarations.module_settings_store
            .name[context.storage.declarations.module_settings_store.index(
                new_module_settings_store)] = context.storage.strings.intern("representations");
        context.storage.declarations.module_settings_store
            .header[context.storage.declarations.module_settings_store.index(
                new_module_settings_store)] = context.storage.strings.add("Representations.h");
        return new_module_settings_store;
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(representations)] =
        context.storage.declarations.range<lispb::codegen::DeclarationHandle>({[&] {
            auto const new_linear_quantizeds{
                context.storage.declarations.linear_quantizeds.append()};
            context.storage.declarations.linear_quantizeds
                .name[context.storage.declarations.linear_quantizeds.index(new_linear_quantizeds)] =
                context.storage.strings.intern("DamageReasonQ4");
            context.storage.declarations.linear_quantizeds
                .source[context.storage.declarations.linear_quantizeds.index(
                    new_linear_quantizeds)] = [&] {
                auto const new_type_refs{context.storage.declarations.type_refs.append()};
                context.storage.declarations.type_refs
                    .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                    context.storage.strings.intern("DamageReason");
                return new_type_refs;
            }();
            context.storage.declarations.linear_quantizeds
                .bit_width[context.storage.declarations.linear_quantizeds.index(
                    new_linear_quantizeds)] = 4;
            context.storage.declarations.linear_quantizeds
                .reserved_codes[context.storage.declarations.linear_quantizeds.index(
                    new_linear_quantizeds)] = 1;
            context.storage.declarations.linear_quantizeds
                .clipping[context.storage.declarations.linear_quantizeds.index(
                    new_linear_quantizeds)] = QuantizationClipping::reject;
            return new_linear_quantizeds;
        }()});
    return lispb::codegen::Manifest{manifest_schema_version,
                                    make_test_registry(context),
                                    context.storage.declarations,
                                    {scalar, representations}};
}

auto valid_integer_varint_manifest(
    CompilerTestContext& context,
    bool const signedness = false,
    IntegerVarintEncoding const encoding = IntegerVarintEncoding::unsigned_varint) -> Manifest {
    auto scalar{valid_integer_scalar_module(context)};
    if (signedness) {
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(scalar)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(scalar)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{context.storage.declarations.integer_scalars.clone(rows_1.front())};
                context.storage.declarations.integer_scalars
                    .signedness[context.storage.declarations.integer_scalars.index(row_0)] = true;
                return row_0;
            }();
            return context.storage.declarations.range(rows_1);
        }();
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(scalar)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(scalar)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{context.storage.declarations.integer_scalars.clone(rows_1.front())};
                context.storage.declarations.integer_scalars
                    .minimum_value[context.storage.declarations.integer_scalars.index(row_0)] =
                    -100;
                return row_0;
            }();
            return context.storage.declarations.range(rows_1);
        }();
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(scalar)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(scalar)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{context.storage.declarations.integer_scalars.clone(rows_1.front())};
                context.storage.declarations.integer_scalars
                    .maximum_value[context.storage.declarations.integer_scalars.index(row_0)] = 100;
                return row_0;
            }();
            return context.storage.declarations.range(rows_1);
        }();
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(scalar)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(scalar)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{context.storage.declarations.integer_scalars.clone(rows_1.front())};
                context.storage.declarations.integer_scalars
                    .named_codes[context.storage.declarations.integer_scalars.index(row_0)] = {};
                return row_0;
            }();
            return context.storage.declarations.range(rows_1);
        }();
    }
    auto const representations{context.storage.declarations.normal_modules.append()};
    context.storage.declarations.normal_modules
        .settings[context.storage.declarations.normal_modules.index(representations)] = [&] {
        auto const new_module_settings_store{
            context.storage.declarations.module_settings_store.append()};
        context.storage.declarations.module_settings_store
            .name[context.storage.declarations.module_settings_store.index(
                new_module_settings_store)] = context.storage.strings.intern("representations");
        context.storage.declarations.module_settings_store
            .header[context.storage.declarations.module_settings_store.index(
                new_module_settings_store)] = context.storage.strings.add("Representations.h");
        return new_module_settings_store;
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(representations)] =
        context.storage.declarations.range<lispb::codegen::DeclarationHandle>({[&] {
            auto const new_integer_varints{context.storage.declarations.integer_varints.append()};
            context.storage.declarations.integer_varints
                .name[context.storage.declarations.integer_varints.index(new_integer_varints)] =
                context.storage.strings.intern("DamageReasonVarint");
            context.storage.declarations.integer_varints
                .source[context.storage.declarations.integer_varints.index(new_integer_varints)] =
                [&] {
                    auto const new_type_refs{context.storage.declarations.type_refs.append()};
                    context.storage.declarations.type_refs
                        .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                        context.storage.strings.intern("DamageReason");
                    return new_type_refs;
                }();
            context.storage.declarations.integer_varints
                .encoding[context.storage.declarations.integer_varints.index(new_integer_varints)] =
                encoding;
            return new_integer_varints;
        }()});
    return lispb::codegen::Manifest{manifest_schema_version,
                                    make_test_registry(context),
                                    context.storage.declarations,
                                    {scalar, representations}};
}

auto valid_fixed_point_manifest(CompilerTestContext& context, bool const signedness = true)
    -> Manifest {
    auto const representations{context.storage.declarations.normal_modules.append()};
    context.storage.declarations.normal_modules
        .settings[context.storage.declarations.normal_modules.index(representations)] = [&] {
        auto const new_module_settings_store{
            context.storage.declarations.module_settings_store.append()};
        context.storage.declarations.module_settings_store
            .name[context.storage.declarations.module_settings_store.index(
                new_module_settings_store)] = context.storage.strings.intern("representations");
        context.storage.declarations.module_settings_store
            .header[context.storage.declarations.module_settings_store.index(
                new_module_settings_store)] = context.storage.strings.add("Representations.h");
        return new_module_settings_store;
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(representations)] =
        context.storage.declarations.range<lispb::codegen::DeclarationHandle>({[&] {
            auto const new_fixed_points{context.storage.declarations.fixed_points.append()};
            context.storage.declarations.fixed_points
                .name[context.storage.declarations.fixed_points.index(new_fixed_points)] =
                context.storage.strings.intern("VelocityQ12_4");
            context.storage.declarations.fixed_points
                .signedness[context.storage.declarations.fixed_points.index(new_fixed_points)] =
                signedness;
            context.storage.declarations.fixed_points
                .total_bits[context.storage.declarations.fixed_points.index(new_fixed_points)] = 16;
            context.storage.declarations.fixed_points
                .fractional_bits[context.storage.declarations.fixed_points.index(
                    new_fixed_points)] = 4;
            context.storage.declarations.fixed_points
                .rounding[context.storage.declarations.fixed_points.index(new_fixed_points)] =
                FixedPointRounding::nearest_even;
            return new_fixed_points;
        }()});
    return lispb::codegen::Manifest{manifest_schema_version,
                                    make_test_registry(context),
                                    context.storage.declarations,
                                    {representations}};
}

auto valid_mini_float_manifest(CompilerTestContext& context) -> Manifest {
    auto const representations{context.storage.declarations.normal_modules.append()};
    context.storage.declarations.normal_modules
        .settings[context.storage.declarations.normal_modules.index(representations)] = [&] {
        auto const new_module_settings_store{
            context.storage.declarations.module_settings_store.append()};
        context.storage.declarations.module_settings_store
            .name[context.storage.declarations.module_settings_store.index(
                new_module_settings_store)] = context.storage.strings.intern("representations");
        context.storage.declarations.module_settings_store
            .header[context.storage.declarations.module_settings_store.index(
                new_module_settings_store)] = context.storage.strings.add("Representations.h");
        return new_module_settings_store;
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(representations)] =
        context.storage.declarations.range<lispb::codegen::DeclarationHandle>({[&] {
            auto const new_mini_floats{context.storage.declarations.mini_floats.append()};
            context.storage.declarations.mini_floats
                .name[context.storage.declarations.mini_floats.index(new_mini_floats)] =
                context.storage.strings.intern("CompactFloat");
            context.storage.declarations.mini_floats
                .sign_bits[context.storage.declarations.mini_floats.index(new_mini_floats)] = 1;
            context.storage.declarations.mini_floats
                .exponent_bits[context.storage.declarations.mini_floats.index(new_mini_floats)] = 5;
            context.storage.declarations.mini_floats
                .significand_bits[context.storage.declarations.mini_floats.index(new_mini_floats)] =
                10;
            context.storage.declarations.mini_floats
                .exponent_bias[context.storage.declarations.mini_floats.index(new_mini_floats)] =
                15;
            return new_mini_floats;
        }()});
    return lispb::codegen::Manifest{manifest_schema_version,
                                    make_test_registry(context),
                                    context.storage.declarations,
                                    {representations}};
}

auto valid_optional_sentinel_manifest(CompilerTestContext& context) -> Manifest {
    auto scalar{valid_integer_scalar_module(context)};
    auto const representations{context.storage.declarations.normal_modules.append()};
    context.storage.declarations.normal_modules
        .settings[context.storage.declarations.normal_modules.index(representations)] = [&] {
        auto const new_module_settings_store{
            context.storage.declarations.module_settings_store.append()};
        context.storage.declarations.module_settings_store
            .name[context.storage.declarations.module_settings_store.index(
                new_module_settings_store)] = context.storage.strings.intern("representations");
        context.storage.declarations.module_settings_store
            .header[context.storage.declarations.module_settings_store.index(
                new_module_settings_store)] = context.storage.strings.add("Representations.h");
        return new_module_settings_store;
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(representations)] =
        context.storage.declarations.range<lispb::codegen::DeclarationHandle>({[&] {
            auto const new_optional_sentinels{
                context.storage.declarations.optional_sentinels.append()};
            context.storage.declarations.optional_sentinels
                .name[context.storage.declarations.optional_sentinels.index(
                    new_optional_sentinels)] =
                context.storage.strings.intern("OptionalDamageReason");
            context.storage.declarations.optional_sentinels
                .source[context.storage.declarations.optional_sentinels.index(
                    new_optional_sentinels)] = [&] {
                auto const new_type_refs{context.storage.declarations.type_refs.append()};
                context.storage.declarations.type_refs
                    .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                    context.storage.strings.intern("DamageReason");
                return new_type_refs;
            }();
            context.storage.declarations.optional_sentinels
                .sentinel[context.storage.declarations.optional_sentinels.index(
                    new_optional_sentinels)] = context.storage.strings.intern("Invalid");
            return new_optional_sentinels;
        }()});
    return lispb::codegen::Manifest{manifest_schema_version,
                                    make_test_registry(context),
                                    context.storage.declarations,
                                    {scalar, representations}};
}

auto valid_optional_presence_bit_manifest(CompilerTestContext& context) -> Manifest {
    auto scalar{valid_integer_scalar_module(context)};
    auto const representations{context.storage.declarations.normal_modules.append()};
    context.storage.declarations.normal_modules
        .settings[context.storage.declarations.normal_modules.index(representations)] = [&] {
        auto const new_module_settings_store{
            context.storage.declarations.module_settings_store.append()};
        context.storage.declarations.module_settings_store
            .name[context.storage.declarations.module_settings_store.index(
                new_module_settings_store)] = context.storage.strings.intern("representations");
        context.storage.declarations.module_settings_store
            .header[context.storage.declarations.module_settings_store.index(
                new_module_settings_store)] = context.storage.strings.add("Representations.h");
        return new_module_settings_store;
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(representations)] =
        context.storage.declarations.range<lispb::codegen::DeclarationHandle>({[&] {
            auto const new_optional_presence_bits{
                context.storage.declarations.optional_presence_bits.append()};
            context.storage.declarations.optional_presence_bits
                .name[context.storage.declarations.optional_presence_bits.index(
                    new_optional_presence_bits)] =
                context.storage.strings.intern("PresentDamageReason");
            context.storage.declarations.optional_presence_bits
                .source[context.storage.declarations.optional_presence_bits.index(
                    new_optional_presence_bits)] = [&] {
                auto const new_type_refs{context.storage.declarations.type_refs.append()};
                context.storage.declarations.type_refs
                    .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                    context.storage.strings.intern("DamageReason");
                return new_type_refs;
            }();
            return new_optional_presence_bits;
        }()});
    return lispb::codegen::Manifest{manifest_schema_version,
                                    make_test_registry(context),
                                    context.storage.declarations,
                                    {scalar, representations}};
}

auto valid_static_table_module(CompilerTestContext& context) -> lispb::codegen::ModuleHandle {
    return [&] {
        auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
        context.storage.declarations.normal_modules
            .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] = [&] {
            auto const new_module_settings_store{
                context.storage.declarations.module_settings_store.append()};
            context.storage.declarations.module_settings_store
                .name[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.intern("tables");
            context.storage.declarations.module_settings_store
                .header[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.add("Tables.h");
            return new_module_settings_store;
        }();
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(new_normal_modules)] =
            context.storage.declarations.range<lispb::codegen::DeclarationHandle>({[&] {
                auto const new_static_tables{context.storage.declarations.static_tables.append()};
                context.storage.declarations.static_tables
                    .name[context.storage.declarations.static_tables.index(new_static_tables)] =
                    context.storage.strings.intern("FValues");
                context.storage.declarations.static_tables
                    .rows[context.storage.declarations.static_tables.index(new_static_tables)] =
                    context.storage.declarations.range<lispb::codegen::StaticTableRowId>(
                        {[&] {
                             auto const new_static_table_rows{
                                 context.storage.declarations.static_table_rows.append()};
                             context.storage.declarations.static_table_rows
                                 .name[context.storage.declarations.static_table_rows.index(
                                     new_static_table_rows)] =
                                 context.storage.strings.intern("first");
                             return new_static_table_rows;
                         }(),
                         [&] {
                             auto const new_static_table_rows{
                                 context.storage.declarations.static_table_rows.append()};
                             context.storage.declarations.static_table_rows
                                 .name[context.storage.declarations.static_table_rows.index(
                                     new_static_table_rows)] =
                                 context.storage.strings.intern("second");
                             return new_static_table_rows;
                         }()});
                context.storage.declarations.static_tables
                    .columns[context.storage.declarations.static_tables.index(new_static_tables)] =
                    context.storage.declarations.range<lispb::codegen::StaticTableColumnId>({[&] {
                        auto const new_static_table_columns{
                            context.storage.declarations.static_table_columns.append()};
                        context.storage.declarations.static_table_columns
                            .name[context.storage.declarations.static_table_columns.index(
                                new_static_table_columns)] = context.storage.strings.intern("ids");
                        context.storage.declarations.static_table_columns
                            .type[context.storage.declarations.static_table_columns.index(
                                new_static_table_columns)] = [&] {
                            auto const new_type_refs{
                                context.storage.declarations.type_refs.append()};
                            context.storage.declarations.type_refs
                                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                                context.storage.strings.intern("int32");
                            return new_type_refs;
                        }();
                        return new_static_table_columns;
                    }()});
                return new_static_tables;
            }()});
        return new_normal_modules;
    }();
}

TEST(Validation, IntegerAliasRequiresAUsableRepresentationButNoNamedCodes) {
    CompilerTestContext context;
    auto const scalar{context.storage.declarations.integer_scalars.append()};
    context.storage.declarations.integer_scalars
        .name[context.storage.declarations.integer_scalars.index(scalar)] =
        context.storage.strings.intern("Health");
    context.storage.declarations.integer_scalars
        .minimum_value[context.storage.declarations.integer_scalars.index(scalar)] = 0;
    context.storage.declarations.integer_scalars
        .maximum_value[context.storage.declarations.integer_scalars.index(scalar)] = 65535;
    context.storage.declarations.integer_scalars
        .bit_width[context.storage.declarations.integer_scalars.index(scalar)] = 16;
    context.storage.declarations.integer_scalars
        .cpp_emission[context.storage.declarations.integer_scalars.index(scalar)] =
        IntegerScalarCppEmission::alias;
    context.storage.declarations.integer_scalars
        .cpp_type[context.storage.declarations.integer_scalars.index(scalar)] = [&] {
        auto const new_type_refs{context.storage.declarations.type_refs.append()};
        context.storage.declarations.type_refs
            .name[context.storage.declarations.type_refs.index(new_type_refs)] =
            context.storage.strings.intern("std::uint16_t");
        return new_type_refs;
    }();
    auto make_manifest = [&] {
        return manifest_with(context, [&] {
            auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
            context.storage.declarations.normal_modules
                .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] =
                [&] {
                    auto const new_module_settings_store{
                        context.storage.declarations.module_settings_store.append()};
                    context.storage.declarations.module_settings_store
                        .name[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] = context.storage.strings.intern("health");
                    context.storage.declarations.module_settings_store
                        .header[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] = context.storage.strings.add("Health.h");
                    return new_module_settings_store;
                }();
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(
                    new_normal_modules)] =
                context.storage.declarations.range<lispb::codegen::DeclarationHandle>({scalar});
            return new_normal_modules;
        }());
    };
    EXPECT_NO_THROW(lispb::schema::resolve_type_graph(make_manifest()));
    for (auto const* representation : {"", "@missing", "float", "std::uint8_t", "std::uint16_t*"}) {
        context.storage.declarations.integer_scalars
            .cpp_type[context.storage.declarations.integer_scalars.index(scalar)] = [&] {
            auto const new_type_refs{context.storage.declarations.type_refs.append()};
            context.storage.declarations.type_refs
                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                context.storage.strings.intern(representation);
            return new_type_refs;
        }();
        EXPECT_THROW(lispb::schema::resolve_type_graph(make_manifest()), std::invalid_argument)
            << representation;
    }
    context.storage.declarations.integer_scalars
        .cpp_type[context.storage.declarations.integer_scalars.index(scalar)]
        .reset();
    EXPECT_THROW(lispb::schema::resolve_type_graph(make_manifest()), std::invalid_argument);
    context.storage.declarations.integer_scalars
        .cpp_type[context.storage.declarations.integer_scalars.index(scalar)] = [&] {
        auto const new_type_refs{context.storage.declarations.type_refs.append()};
        context.storage.declarations.type_refs
            .name[context.storage.declarations.type_refs.index(new_type_refs)] =
            context.storage.strings.intern("std::uint16_t");
        return new_type_refs;
    }();
    context.storage.declarations.integer_scalars
        .minimum_value[context.storage.declarations.integer_scalars.index(scalar)] = -1;
    EXPECT_THROW(lispb::schema::resolve_type_graph(make_manifest()), std::invalid_argument);
    context.storage.declarations.integer_scalars
        .minimum_value[context.storage.declarations.integer_scalars.index(scalar)] = 0;
    context.storage.declarations.integer_scalars
        .bit_width[context.storage.declarations.integer_scalars.index(scalar)] = 8;
    EXPECT_THROW(lispb::schema::resolve_type_graph(make_manifest()), std::invalid_argument);
}

TEST(Validation, RejectsEmptyModuleNames) {
    CompilerTestContext context;
    auto module{valid_soa_module(context)};
    context.storage.declarations.normal_modules
        .settings[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto row_0{context.storage.declarations.module_settings_store.clone(
            context.storage.declarations.normal_modules
                .settings[context.storage.declarations.normal_modules.index(module)])};
        context.storage.declarations.module_settings_store
            .name[context.storage.declarations.module_settings_store.index(row_0)] = {};
        return row_0;
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsInvalidEnumDefinitions) {
    CompilerTestContext context;
    auto module{valid_enum_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .values[context.storage.declarations.enums.index(row_0)] = {};
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_enum_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .values[context.storage.declarations.enums.index(row_0)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.enums
                        .values[context.storage.declarations.enums.index(row_0)])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.push_back([&] {
                    auto const new_enumerators{context.storage.declarations.enumerators.append()};
                    context.storage.declarations.enumerators
                        .name[context.storage.declarations.enumerators.index(new_enumerators)] =
                        context.storage.strings.intern("Value");
                    return new_enumerators;
                }());
                return context.storage.declarations.range(rows);
            }();
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_enum_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.enums.clone(rows_3.front())};
            context.storage.declarations.enums
                .values[context.storage.declarations.enums.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.enums
                        .values[context.storage.declarations.enums.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{context.storage.declarations.enumerators.clone(rows_1.front())};
                    context.storage.declarations.enumerators
                        .name[context.storage.declarations.enumerators.index(row_0)] =
                        context.storage.strings.intern("bad-name");
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_enum_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .underlying_type[context.storage.declarations.enums.index(row_0)] = [&] {
                auto const new_type_refs{context.storage.declarations.type_refs.append()};
                context.storage.declarations.type_refs
                    .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                    context.storage.strings.intern("@missing");
                return new_type_refs;
            }();
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsEnumSemanticWidthsThatCannotRepresentKnownValues) {
    CompilerTestContext context;
    auto module{valid_enum_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .reflection[context.storage.declarations.enums.index(row_0)] = EnumReflection::none;
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .bit_width[context.storage.declarations.enums.index(row_0)] = 0;
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_enum_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .reflection[context.storage.declarations.enums.index(row_0)] = EnumReflection::none;
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .bit_width[context.storage.declarations.enums.index(row_0)] = 65;
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_enum_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .reflection[context.storage.declarations.enums.index(row_0)] = EnumReflection::none;
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .values[context.storage.declarations.enums.index(row_0)] =
                context.storage.declarations.range<lispb::codegen::EnumeratorId>(
                    {[&] {
                         auto const new_enumerators{
                             context.storage.declarations.enumerators.append()};
                         context.storage.declarations.enumerators
                             .name[context.storage.declarations.enumerators.index(
                                 new_enumerators)] = context.storage.strings.intern("Zero");
                         context.storage.declarations.enumerators
                             .initializer[context.storage.declarations.enumerators.index(
                                 new_enumerators)] = context.storage.strings.add("0");
                         return new_enumerators;
                     }(),
                     [&] {
                         auto const new_enumerators{
                             context.storage.declarations.enumerators.append()};
                         context.storage.declarations.enumerators
                             .name[context.storage.declarations.enumerators.index(
                                 new_enumerators)] = context.storage.strings.intern("Seven");
                         context.storage.declarations.enumerators
                             .initializer[context.storage.declarations.enumerators.index(
                                 new_enumerators)] = context.storage.strings.add("7");
                         return new_enumerators;
                     }()});
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .bit_width[context.storage.declarations.enums.index(row_0)] = 2;
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_enum_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .reflection[context.storage.declarations.enums.index(row_0)] = EnumReflection::none;
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .values[context.storage.declarations.enums.index(row_0)] =
                context.storage.declarations.range<lispb::codegen::EnumeratorId>(
                    {[&] {
                         auto const new_enumerators{
                             context.storage.declarations.enumerators.append()};
                         context.storage.declarations.enumerators
                             .name[context.storage.declarations.enumerators.index(
                                 new_enumerators)] = context.storage.strings.intern("Zero");
                         context.storage.declarations.enumerators
                             .initializer[context.storage.declarations.enumerators.index(
                                 new_enumerators)] = context.storage.strings.add("0");
                         return new_enumerators;
                     }(),
                     [&] {
                         auto const new_enumerators{
                             context.storage.declarations.enumerators.append()};
                         context.storage.declarations.enumerators
                             .name[context.storage.declarations.enumerators.index(
                                 new_enumerators)] = context.storage.strings.intern("Seven");
                         context.storage.declarations.enumerators
                             .initializer[context.storage.declarations.enumerators.index(
                                 new_enumerators)] = context.storage.strings.add("7");
                         return new_enumerators;
                     }()});
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .bit_width[context.storage.declarations.enums.index(row_0)] = 3;
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));
}

TEST(Validation, AppliesExplicitEnumSignednessToTheSemanticDomain) {
    CompilerTestContext context;
    auto module{valid_enum_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .reflection[context.storage.declarations.enums.index(row_0)] = EnumReflection::none;
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .values[context.storage.declarations.enums.index(row_0)] =
                context.storage.declarations.range<lispb::codegen::EnumeratorId>(
                    {[&] {
                         auto const new_enumerators{
                             context.storage.declarations.enumerators.append()};
                         context.storage.declarations.enumerators
                             .name[context.storage.declarations.enumerators.index(
                                 new_enumerators)] = context.storage.strings.intern("Negative");
                         context.storage.declarations.enumerators
                             .initializer[context.storage.declarations.enumerators.index(
                                 new_enumerators)] = context.storage.strings.add("-1");
                         return new_enumerators;
                     }(),
                     [&] {
                         auto const new_enumerators{
                             context.storage.declarations.enumerators.append()};
                         context.storage.declarations.enumerators
                             .name[context.storage.declarations.enumerators.index(
                                 new_enumerators)] = context.storage.strings.intern("Positive");
                         context.storage.declarations.enumerators
                             .initializer[context.storage.declarations.enumerators.index(
                                 new_enumerators)] = context.storage.strings.add("1");
                         return new_enumerators;
                     }()});
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .signedness[context.storage.declarations.enums.index(row_0)] =
                schema::EnumSignedness::unsigned_;
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_enum_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .reflection[context.storage.declarations.enums.index(row_0)] = EnumReflection::none;
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .values[context.storage.declarations.enums.index(row_0)] =
                context.storage.declarations.range<lispb::codegen::EnumeratorId>(
                    {[&] {
                         auto const new_enumerators{
                             context.storage.declarations.enumerators.append()};
                         context.storage.declarations.enumerators
                             .name[context.storage.declarations.enumerators.index(
                                 new_enumerators)] = context.storage.strings.intern("Zero");
                         context.storage.declarations.enumerators
                             .initializer[context.storage.declarations.enumerators.index(
                                 new_enumerators)] = context.storage.strings.add("0");
                         return new_enumerators;
                     }(),
                     [&] {
                         auto const new_enumerators{
                             context.storage.declarations.enumerators.append()};
                         context.storage.declarations.enumerators
                             .name[context.storage.declarations.enumerators.index(
                                 new_enumerators)] = context.storage.strings.intern("Seven");
                         context.storage.declarations.enumerators
                             .initializer[context.storage.declarations.enumerators.index(
                                 new_enumerators)] = context.storage.strings.add("7");
                         return new_enumerators;
                     }()});
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .signedness[context.storage.declarations.enums.index(row_0)] =
                schema::EnumSignedness::signed_;
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .bit_width[context.storage.declarations.enums.index(row_0)] = 3;
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_enum_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .reflection[context.storage.declarations.enums.index(row_0)] = EnumReflection::none;
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .values[context.storage.declarations.enums.index(row_0)] =
                context.storage.declarations.range<lispb::codegen::EnumeratorId>(
                    {[&] {
                         auto const new_enumerators{
                             context.storage.declarations.enumerators.append()};
                         context.storage.declarations.enumerators
                             .name[context.storage.declarations.enumerators.index(
                                 new_enumerators)] = context.storage.strings.intern("Zero");
                         context.storage.declarations.enumerators
                             .initializer[context.storage.declarations.enumerators.index(
                                 new_enumerators)] = context.storage.strings.add("0");
                         return new_enumerators;
                     }(),
                     [&] {
                         auto const new_enumerators{
                             context.storage.declarations.enumerators.append()};
                         context.storage.declarations.enumerators
                             .name[context.storage.declarations.enumerators.index(
                                 new_enumerators)] = context.storage.strings.intern("Seven");
                         context.storage.declarations.enumerators
                             .initializer[context.storage.declarations.enumerators.index(
                                 new_enumerators)] = context.storage.strings.add("7");
                         return new_enumerators;
                     }()});
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .signedness[context.storage.declarations.enums.index(row_0)] =
                schema::EnumSignedness::signed_;
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .bit_width[context.storage.declarations.enums.index(row_0)] = 4;
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));
}

TEST(Validation, RejectsInvalidIntegerScalarDomainsAndNamedCodes) {
    CompilerTestContext context;
    auto module{valid_integer_scalar_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.integer_scalars.clone(rows_1.front())};
            context.storage.declarations.integer_scalars
                .bit_width[context.storage.declarations.integer_scalars.index(row_0)] = 3;
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_integer_scalar_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.integer_scalars.clone(rows_1.front())};
            context.storage.declarations.integer_scalars
                .minimum_value[context.storage.declarations.integer_scalars.index(row_0)] = -1;
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_integer_scalar_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.integer_scalars.clone(rows_3.front())};
            context.storage.declarations.integer_scalars
                .named_codes[context.storage.declarations.integer_scalars.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.integer_scalars
                        .named_codes[context.storage.declarations.integer_scalars.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{context.storage.declarations.packed_named_codes.clone(rows_1[1])};
                    context.storage.declarations.packed_named_codes
                        .value[context.storage.declarations.packed_named_codes.index(row_0)] = 10;
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_integer_scalar_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.integer_scalars.clone(rows_3.front())};
            context.storage.declarations.integer_scalars
                .named_codes[context.storage.declarations.integer_scalars.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.integer_scalars
                        .named_codes[context.storage.declarations.integer_scalars.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_named_codes.clone(rows_1[0])};
                    context.storage.declarations.packed_named_codes
                        .value[context.storage.declarations.packed_named_codes.index(row_0)] = 11;
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_integer_scalar_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.integer_scalars.clone(rows_3.front())};
            context.storage.declarations.integer_scalars
                .named_codes[context.storage.declarations.integer_scalars.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.integer_scalars
                        .named_codes[context.storage.declarations.integer_scalars.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{context.storage.declarations.packed_named_codes.clone(rows_1[1])};
                    context.storage.declarations.packed_named_codes
                        .name[context.storage.declarations.packed_named_codes.index(row_0)] =
                        context.storage.strings.intern("Unknown");
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_integer_scalar_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.integer_scalars.clone(rows_3.front())};
            context.storage.declarations.integer_scalars
                .named_codes[context.storage.declarations.integer_scalars.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.integer_scalars
                        .named_codes[context.storage.declarations.integer_scalars.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{context.storage.declarations.packed_named_codes.clone(rows_1[1])};
                    context.storage.declarations.packed_named_codes
                        .value[context.storage.declarations.packed_named_codes.index(row_0)] = 0;
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_integer_scalar_module(context);
    context.storage.declarations.normal_modules
        .settings[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto row_0{context.storage.declarations.module_settings_store.clone(
            context.storage.declarations.normal_modules
                .settings[context.storage.declarations.normal_modules.index(module)])};
        context.storage.declarations.module_settings_store
            .source[context.storage.declarations.module_settings_store.index(row_0)] =
            context.storage.strings.add("Scalars.cpp");
        return row_0;
    }();
    EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));

    module = valid_integer_scalar_module(context);
    auto const lowered{lower_test_manifest(context, manifest_with(context, module))};
    ASSERT_EQ(lowered.size(), 1U);
    auto const header{context.ir.modules.header[context.ir.modules.index(lowered.front())]};
    ASSERT_NE(header.value, 0U);
    EXPECT_EQ(context.storage.strings.view(context.ir.files.path[context.ir.files.index(header)]),
              "Scalars.h");
    EXPECT_EQ(render_modules(context.ir, lowered).front().content.find("struct "),
              std::string::npos);
}

TEST(Validation, ValidatesIntegerScalarCppConstantsPolicy) {
    CompilerTestContext context;
    auto module{valid_integer_scalar_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.integer_scalars.clone(rows_1.front())};
            context.storage.declarations.integer_scalars
                .cpp_type[context.storage.declarations.integer_scalars.index(row_0)] = [&] {
                auto const new_type_refs{context.storage.declarations.type_refs.append()};
                context.storage.declarations.type_refs
                    .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                    context.storage.strings.intern("std::uint8_t");
                return new_type_refs;
            }();
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_integer_scalar_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.integer_scalars.clone(rows_1.front())};
            context.storage.declarations.integer_scalars
                .cpp_emission[context.storage.declarations.integer_scalars.index(row_0)] =
                IntegerScalarCppEmission::constants;
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_integer_scalar_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.integer_scalars.clone(rows_1.front())};
            context.storage.declarations.integer_scalars
                .cpp_emission[context.storage.declarations.integer_scalars.index(row_0)] =
                IntegerScalarCppEmission::constants;
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.integer_scalars.clone(rows_1.front())};
            context.storage.declarations.integer_scalars
                .cpp_type[context.storage.declarations.integer_scalars.index(row_0)] = [&] {
                auto const new_type_refs{context.storage.declarations.type_refs.append()};
                context.storage.declarations.type_refs
                    .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                    context.storage.strings.intern("std::uint8_t");
                return new_type_refs;
            }();
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));

    module = valid_integer_scalar_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.integer_scalars.clone(rows_1.front())};
            context.storage.declarations.integer_scalars
                .cpp_emission[context.storage.declarations.integer_scalars.index(row_0)] =
                IntegerScalarCppEmission::constants;
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.integer_scalars.clone(rows_1.front())};
            context.storage.declarations.integer_scalars
                .cpp_type[context.storage.declarations.integer_scalars.index(row_0)] = [&] {
                auto const new_type_refs{context.storage.declarations.type_refs.append()};
                context.storage.declarations.type_refs
                    .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                    context.storage.strings.intern("std::int8_t");
                return new_type_refs;
            }();
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.integer_scalars.clone(rows_1.front())};
            context.storage.declarations.integer_scalars
                .maximum_value[context.storage.declarations.integer_scalars.index(row_0)] = 127;
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.integer_scalars.clone(rows_3.front())};
            context.storage.declarations.integer_scalars
                .named_codes[context.storage.declarations.integer_scalars.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.integer_scalars
                        .named_codes[context.storage.declarations.integer_scalars.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{context.storage.declarations.packed_named_codes.clone(rows_1[1])};
                    context.storage.declarations.packed_named_codes
                        .value[context.storage.declarations.packed_named_codes.index(row_0)] = 128;
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_integer_scalar_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.integer_scalars.clone(rows_1.front())};
            context.storage.declarations.integer_scalars
                .cpp_emission[context.storage.declarations.integer_scalars.index(row_0)] =
                IntegerScalarCppEmission::constants;
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.integer_scalars.clone(rows_1.front())};
            context.storage.declarations.integer_scalars
                .cpp_type[context.storage.declarations.integer_scalars.index(row_0)] = [&] {
                auto const new_type_refs{context.storage.declarations.type_refs.append()};
                context.storage.declarations.type_refs
                    .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                    context.storage.strings.intern("float");
                return new_type_refs;
            }();
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_integer_scalar_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.integer_scalars.clone(rows_1.front())};
            context.storage.declarations.integer_scalars
                .cpp_emission[context.storage.declarations.integer_scalars.index(row_0)] =
                IntegerScalarCppEmission::constants;
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.integer_scalars.clone(rows_1.front())};
            context.storage.declarations.integer_scalars
                .cpp_type[context.storage.declarations.integer_scalars.index(row_0)] = [&] {
                auto const new_type_refs{context.storage.declarations.type_refs.append()};
                context.storage.declarations.type_refs
                    .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                    context.storage.strings.intern("std::uint8_t");
                return new_type_refs;
            }();
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.integer_scalars.clone(rows_1.front())};
            context.storage.declarations.integer_scalars
                .named_codes[context.storage.declarations.integer_scalars.index(row_0)] = {};
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_integer_scalar_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.integer_scalars.clone(rows_1.front())};
            context.storage.declarations.integer_scalars
                .cpp_emission[context.storage.declarations.integer_scalars.index(row_0)] =
                IntegerScalarCppEmission::constants_with_names;
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.integer_scalars.clone(rows_1.front())};
            context.storage.declarations.integer_scalars
                .cpp_type[context.storage.declarations.integer_scalars.index(row_0)] = [&] {
                auto const new_type_refs{context.storage.declarations.type_refs.append()};
                context.storage.declarations.type_refs
                    .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                    context.storage.strings.intern("std::uint8_t");
                return new_type_refs;
            }();
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.integer_scalars.clone(rows_3.front())};
            context.storage.declarations.integer_scalars
                .named_codes[context.storage.declarations.integer_scalars.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.integer_scalars
                        .named_codes[context.storage.declarations.integer_scalars.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{
                        context.storage.declarations.packed_named_codes.clone(rows_1.front())};
                    context.storage.declarations.packed_named_codes
                        .name[context.storage.declarations.packed_named_codes.index(row_0)] =
                        context.storage.strings.intern("name");
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, AllowsEmptyEditableModulesToRemainGenerationDestinations) {
    CompilerTestContext context;
    auto settings = [&](std::string const& name) {
        return [&] {
            auto const new_module_settings_store{
                context.storage.declarations.module_settings_store.append()};
            context.storage.declarations.module_settings_store
                .name[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.intern(name);
            context.storage.declarations.module_settings_store
                .header[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.add(name + ".h");
            return new_module_settings_store;
        }();
    };
    auto module{[&] {
        auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
        context.storage.declarations.normal_modules
            .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] =
            settings("empty");
        return new_normal_modules;
    }()};
    EXPECT_NO_THROW(
        render_modules(context.ir, lower_test_manifest(context, manifest_with(context, module))));
    context.storage.declarations.normal_modules
        .settings[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto row_0{context.storage.declarations.module_settings_store.clone(
            context.storage.declarations.normal_modules
                .settings[context.storage.declarations.normal_modules.index(module)])};
        context.storage.declarations.module_settings_store
            .source[context.storage.declarations.module_settings_store.index(row_0)] =
            context.storage.strings.add("empty.cpp");
        return row_0;
    }();
    EXPECT_EQ(
        render_modules(context.ir, lower_test_manifest(context, manifest_with(context, module)))
            .size(),
        2U);
}

TEST(Validation, ValidatesRecordMemberRelationshipUnits) {
    CompilerTestContext context;
    auto module{[&] {
        auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
        context.storage.declarations.normal_modules
            .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] = [&] {
            auto const new_module_settings_store{
                context.storage.declarations.module_settings_store.append()};
            context.storage.declarations.module_settings_store
                .name[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.intern("records");
            context.storage.declarations.module_settings_store
                .header[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.add("Records.h");
            return new_module_settings_store;
        }();
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(new_normal_modules)] =
            context.storage.declarations.range<lispb::codegen::DeclarationHandle>(
                {[&] {
                     auto const new_records{context.storage.declarations.records.append()};
                     context.storage.declarations.records
                         .name[context.storage.declarations.records.index(new_records)] =
                         context.storage.strings.intern("Target");
                     context.storage.declarations.records
                         .members[context.storage.declarations.records.index(new_records)] =
                         context.storage.declarations.range<lispb::codegen::RecordMemberId>({[&] {
                             auto const new_record_members{
                                 context.storage.declarations.record_members.append()};
                             context.storage.declarations.record_members
                                 .name[context.storage.declarations.record_members.index(
                                     new_record_members)] = context.storage.strings.intern("value");
                             context.storage.declarations.record_members
                                 .type[context.storage.declarations.record_members.index(
                                     new_record_members)] = [&] {
                                 auto const new_type_refs{
                                     context.storage.declarations.type_refs.append()};
                                 context.storage.declarations.type_refs
                                     .name[context.storage.declarations.type_refs.index(
                                         new_type_refs)] =
                                     context.storage.strings.intern("std::uint32_t");
                                 return new_type_refs;
                             }();
                             return new_record_members;
                         }()});
                     return new_records;
                 }(),
                 [&] {
                     auto const new_records{context.storage.declarations.records.append()};
                     context.storage.declarations.records
                         .name[context.storage.declarations.records.index(new_records)] =
                         context.storage.strings.intern("User");
                     context.storage.declarations.records
                         .members[context.storage.declarations.records.index(new_records)] =
                         context.storage.declarations.range<lispb::codegen::RecordMemberId>({[&] {
                             auto const new_record_members{
                                 context.storage.declarations.record_members.append()};
                             context.storage.declarations.record_members
                                 .name[context.storage.declarations.record_members.index(
                                     new_record_members)] = context.storage.strings.intern("value");
                             context.storage.declarations.record_members
                                 .type[context.storage.declarations.record_members.index(
                                     new_record_members)] = [&] {
                                 auto const new_type_refs{
                                     context.storage.declarations.type_refs.append()};
                                 context.storage.declarations.type_refs
                                     .name[context.storage.declarations.type_refs.index(
                                         new_type_refs)] =
                                     context.storage.strings.intern("std::uint32_t");
                                 return new_type_refs;
                             }();
                             context.storage.declarations.record_members
                                 .relationship[context.storage.declarations.record_members.index(
                                     new_record_members)] = [&] {
                                 auto const new_semantic_relations{
                                     context.storage.declarations.semantic_relations.append()};
                                 context.storage.declarations.semantic_relations
                                     .kind[context.storage.declarations.semantic_relations.index(
                                         new_semantic_relations)] =
                                     SemanticRelationKind::references;
                                 context.storage.declarations.semantic_relations
                                     .target[context.storage.declarations.semantic_relations.index(
                                         new_semantic_relations)] = [&] {
                                     auto const new_type_refs{
                                         context.storage.declarations.type_refs.append()};
                                     context.storage.declarations.type_refs
                                         .name[context.storage.declarations.type_refs.index(
                                             new_type_refs)] =
                                         context.storage.strings.intern("Target");
                                     return new_type_refs;
                                 }();
                                 context.storage.declarations.semantic_relations
                                     .unit[context.storage.declarations.semantic_relations.index(
                                         new_semantic_relations)] = std::nullopt;
                                 return new_semantic_relations;
                             }();
                             return new_record_members;
                         }()});
                     return new_records;
                 }()});
        return new_normal_modules;
    }()};
    EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));

    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3[1] = [&] {
            auto row_2{context.storage.declarations.records.clone(rows_3[1])};
            context.storage.declarations.records
                .members[context.storage.declarations.records.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.records
                        .members[context.storage.declarations.records.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.record_members.clone(rows_1[0])};
                    context.storage.declarations.record_members
                        .relationship[context.storage.declarations.record_members.index(
                            row_0)] = [&] {
                        auto const new_semantic_relations{
                            context.storage.declarations.semantic_relations.append()};
                        context.storage.declarations.semantic_relations
                            .kind[context.storage.declarations.semantic_relations.index(
                                new_semantic_relations)] = SemanticRelationKind::offset_into;
                        context.storage.declarations.semantic_relations
                            .target[context.storage.declarations.semantic_relations.index(
                                new_semantic_relations)] = [&] {
                            auto const new_type_refs{
                                context.storage.declarations.type_refs.append()};
                            context.storage.declarations.type_refs
                                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                                context.storage.strings.intern("Target");
                            return new_type_refs;
                        }();
                        context.storage.declarations.semantic_relations
                            .unit[context.storage.declarations.semantic_relations.index(
                                new_semantic_relations)] = std::nullopt;
                        return new_semantic_relations;
                    }();
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3[1] = [&] {
            auto row_2{context.storage.declarations.records.clone(rows_3[1])};
            context.storage.declarations.records
                .members[context.storage.declarations.records.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.records
                        .members[context.storage.declarations.records.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.record_members.clone(rows_1[0])};
                    context.storage.declarations.record_members
                        .relationship[context.storage.declarations.record_members.index(
                            row_0)] = [&] {
                        auto const new_semantic_relations{
                            context.storage.declarations.semantic_relations.append()};
                        context.storage.declarations.semantic_relations
                            .kind[context.storage.declarations.semantic_relations.index(
                                new_semantic_relations)] = SemanticRelationKind::references;
                        context.storage.declarations.semantic_relations
                            .target[context.storage.declarations.semantic_relations.index(
                                new_semantic_relations)] = [&] {
                            auto const new_type_refs{
                                context.storage.declarations.type_refs.append()};
                            context.storage.declarations.type_refs
                                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                                context.storage.strings.intern("Target");
                            return new_type_refs;
                        }();
                        context.storage.declarations.semantic_relations
                            .unit[context.storage.declarations.semantic_relations.index(
                                new_semantic_relations)] = SemanticRelationUnit::elements;
                        return new_semantic_relations;
                    }();
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3[1] = [&] {
            auto row_2{context.storage.declarations.records.clone(rows_3[1])};
            context.storage.declarations.records
                .members[context.storage.declarations.records.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.records
                        .members[context.storage.declarations.records.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.record_members.clone(rows_1[0])};
                    context.storage.declarations.record_members
                        .relationship[context.storage.declarations.record_members.index(
                            row_0)] = [&] {
                        auto const new_semantic_relations{
                            context.storage.declarations.semantic_relations.append()};
                        context.storage.declarations.semantic_relations
                            .kind[context.storage.declarations.semantic_relations.index(
                                new_semantic_relations)] = SemanticRelationKind::offset_into;
                        context.storage.declarations.semantic_relations
                            .target[context.storage.declarations.semantic_relations.index(
                                new_semantic_relations)] = [&] {
                            auto const new_type_refs{
                                context.storage.declarations.type_refs.append()};
                            context.storage.declarations.type_refs
                                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                                context.storage.strings.intern("Target");
                            return new_type_refs;
                        }();
                        context.storage.declarations.semantic_relations
                            .unit[context.storage.declarations.semantic_relations.index(
                                new_semantic_relations)] = SemanticRelationUnit::bytes;
                        return new_semantic_relations;
                    }();
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));
}

TEST(Validation, ValidatesSoaMemberRelationshipUnits) {
    CompilerTestContext context;
    auto module{valid_soa_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows{std::vector(original_items.begin(), original_items.end())};
        rows.push_back([&] {
            auto const new_soas{context.storage.declarations.soas.append()};
            context.storage.declarations.soas
                .name[context.storage.declarations.soas.index(new_soas)] =
                context.storage.strings.intern("Target");
            context.storage.declarations.soas.members[context.storage.declarations.soas.index(
                new_soas)] = context.storage.declarations.range<lispb::codegen::SoaMemberId>({[&] {
                auto const new_soa_members{context.storage.declarations.soa_members.append()};
                context.storage.declarations.soa_members
                    .name[context.storage.declarations.soa_members.index(new_soa_members)] =
                    context.storage.strings.intern("values");
                context.storage.declarations.soa_members
                    .kind[context.storage.declarations.soa_members.index(new_soa_members)] =
                    SoaMemberKind::array;
                context.storage.declarations.soa_members
                    .type[context.storage.declarations.soa_members.index(new_soa_members)] = [&] {
                    auto const new_type_refs{context.storage.declarations.type_refs.append()};
                    context.storage.declarations.type_refs
                        .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                        context.storage.strings.intern("std::uint32_t");
                    return new_type_refs;
                }();
                context.storage.declarations.soa_members
                    .relationship[context.storage.declarations.soa_members.index(new_soa_members)] =
                    std::nullopt;
                return new_soa_members;
            }()});
            return new_soas;
        }());
        return context.storage.declarations.range(rows);
    }();

    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.soas.clone(rows_3.front())};
            context.storage.declarations.soas
                .members[context.storage.declarations.soas.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.soas
                        .members[context.storage.declarations.soas.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{context.storage.declarations.soa_members.clone(rows_1.front())};
                    context.storage.declarations.soa_members
                        .relationship[context.storage.declarations.soa_members.index(row_0)] = [&] {
                        auto const new_semantic_relations{
                            context.storage.declarations.semantic_relations.append()};
                        context.storage.declarations.semantic_relations
                            .kind[context.storage.declarations.semantic_relations.index(
                                new_semantic_relations)] = SemanticRelationKind::references;
                        context.storage.declarations.semantic_relations
                            .target[context.storage.declarations.semantic_relations.index(
                                new_semantic_relations)] = [&] {
                            auto const new_type_refs{
                                context.storage.declarations.type_refs.append()};
                            context.storage.declarations.type_refs
                                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                                context.storage.strings.intern("Target");
                            return new_type_refs;
                        }();
                        context.storage.declarations.semantic_relations
                            .unit[context.storage.declarations.semantic_relations.index(
                                new_semantic_relations)] = std::nullopt;
                        return new_semantic_relations;
                    }();
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));

    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.soas.clone(rows_3.front())};
            context.storage.declarations.soas
                .members[context.storage.declarations.soas.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.soas
                        .members[context.storage.declarations.soas.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{context.storage.declarations.soa_members.clone(rows_1.front())};
                    context.storage.declarations.soa_members
                        .relationship[context.storage.declarations.soa_members.index(row_0)] = [&] {
                        auto const new_semantic_relations{
                            context.storage.declarations.semantic_relations.append()};
                        context.storage.declarations.semantic_relations
                            .kind[context.storage.declarations.semantic_relations.index(
                                new_semantic_relations)] = SemanticRelationKind::offset_into;
                        context.storage.declarations.semantic_relations
                            .target[context.storage.declarations.semantic_relations.index(
                                new_semantic_relations)] = [&] {
                            auto const new_type_refs{
                                context.storage.declarations.type_refs.append()};
                            context.storage.declarations.type_refs
                                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                                context.storage.strings.intern("Target");
                            return new_type_refs;
                        }();
                        context.storage.declarations.semantic_relations
                            .unit[context.storage.declarations.semantic_relations.index(
                                new_semantic_relations)] = std::nullopt;
                        return new_semantic_relations;
                    }();
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.soas.clone(rows_3.front())};
            context.storage.declarations.soas
                .members[context.storage.declarations.soas.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.soas
                        .members[context.storage.declarations.soas.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{context.storage.declarations.soa_members.clone(rows_1.front())};
                    context.storage.declarations.soa_members
                        .relationship[context.storage.declarations.soa_members.index(row_0)] = [&] {
                        auto const new_semantic_relations{
                            context.storage.declarations.semantic_relations.append()};
                        context.storage.declarations.semantic_relations
                            .kind[context.storage.declarations.semantic_relations.index(
                                new_semantic_relations)] = SemanticRelationKind::references;
                        context.storage.declarations.semantic_relations
                            .target[context.storage.declarations.semantic_relations.index(
                                new_semantic_relations)] = [&] {
                            auto const new_type_refs{
                                context.storage.declarations.type_refs.append()};
                            context.storage.declarations.type_refs
                                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                                context.storage.strings.intern("Target");
                            return new_type_refs;
                        }();
                        context.storage.declarations.semantic_relations
                            .unit[context.storage.declarations.semantic_relations.index(
                                new_semantic_relations)] = SemanticRelationUnit::elements;
                        return new_semantic_relations;
                    }();
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.soas.clone(rows_3.front())};
            context.storage.declarations.soas
                .members[context.storage.declarations.soas.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.soas
                        .members[context.storage.declarations.soas.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{context.storage.declarations.soa_members.clone(rows_1.front())};
                    context.storage.declarations.soa_members
                        .relationship[context.storage.declarations.soa_members.index(row_0)] = [&] {
                        auto const new_semantic_relations{
                            context.storage.declarations.semantic_relations.append()};
                        context.storage.declarations.semantic_relations
                            .kind[context.storage.declarations.semantic_relations.index(
                                new_semantic_relations)] = SemanticRelationKind::offset_into;
                        context.storage.declarations.semantic_relations
                            .target[context.storage.declarations.semantic_relations.index(
                                new_semantic_relations)] = [&] {
                            auto const new_type_refs{
                                context.storage.declarations.type_refs.append()};
                            context.storage.declarations.type_refs
                                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                                context.storage.strings.intern("Target");
                            return new_type_refs;
                        }();
                        context.storage.declarations.semantic_relations
                            .unit[context.storage.declarations.semantic_relations.index(
                                new_semantic_relations)] = SemanticRelationUnit::bytes;
                        return new_semantic_relations;
                    }();
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));
}

TEST(Validation, RequiresUnsignedIndexCountAndOffsetIntegerScalars) {
    CompilerTestContext context;
    for (auto const kind : {SemanticRelationKind::index_into,
                            SemanticRelationKind::count_of,
                            SemanticRelationKind::offset_into}) {
        auto module{valid_integer_scalar_module(context)};

        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(module)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{context.storage.declarations.integer_scalars.clone(rows_1.front())};
                context.storage.declarations.integer_scalars
                    .signedness[context.storage.declarations.integer_scalars.index(row_0)] = true;
                return row_0;
            }();
            return context.storage.declarations.range(rows_1);
        }();
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(module)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{context.storage.declarations.integer_scalars.clone(rows_1.front())};
                context.storage.declarations.integer_scalars
                    .minimum_value[context.storage.declarations.integer_scalars.index(row_0)] = -10;
                return row_0;
            }();
            return context.storage.declarations.range(rows_1);
        }();
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(module)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{context.storage.declarations.integer_scalars.clone(rows_1.front())};
                context.storage.declarations.integer_scalars
                    .maximum_value[context.storage.declarations.integer_scalars.index(row_0)] = 10;
                return row_0;
            }();
            return context.storage.declarations.range(rows_1);
        }();
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(module)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{context.storage.declarations.integer_scalars.clone(rows_1.front())};
                context.storage.declarations.integer_scalars
                    .named_codes[context.storage.declarations.integer_scalars.index(row_0)] = {};
                return row_0;
            }();
            return context.storage.declarations.range(rows_1);
        }();
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(module)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{context.storage.declarations.integer_scalars.clone(rows_1.front())};
                context.storage.declarations.integer_scalars
                    .relationship[context.storage.declarations.integer_scalars.index(row_0)] = [&] {
                    auto const new_semantic_relations{
                        context.storage.declarations.semantic_relations.append()};
                    context.storage.declarations.semantic_relations
                        .kind[context.storage.declarations.semantic_relations.index(
                            new_semantic_relations)] = kind;
                    context.storage.declarations.semantic_relations
                        .target[context.storage.declarations.semantic_relations.index(
                            new_semantic_relations)] = [&] {
                        auto const new_type_refs{context.storage.declarations.type_refs.append()};
                        context.storage.declarations.type_refs
                            .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                            context.storage.strings.intern("DamageReason");
                        return new_type_refs;
                    }();
                    context.storage.declarations.semantic_relations
                        .unit[context.storage.declarations.semantic_relations.index(
                            new_semantic_relations)] =
                        kind == SemanticRelationKind::offset_into
                            ? std::optional{SemanticRelationUnit::bytes}
                            : std::nullopt;
                    return new_semantic_relations;
                }();
                return row_0;
            }();
            return context.storage.declarations.range(rows_1);
        }();
        EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                     std::invalid_argument);
    }

    auto module{valid_integer_scalar_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.integer_scalars.clone(rows_1.front())};
            context.storage.declarations.integer_scalars
                .relationship[context.storage.declarations.integer_scalars.index(row_0)] = [&] {
                auto const new_semantic_relations{
                    context.storage.declarations.semantic_relations.append()};
                context.storage.declarations.semantic_relations
                    .kind[context.storage.declarations.semantic_relations.index(
                        new_semantic_relations)] = SemanticRelationKind::references;
                context.storage.declarations.semantic_relations
                    .target[context.storage.declarations.semantic_relations.index(
                        new_semantic_relations)] = [&] {
                    auto const new_type_refs{context.storage.declarations.type_refs.append()};
                    context.storage.declarations.type_refs
                        .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                        context.storage.strings.intern("DamageReason");
                    return new_type_refs;
                }();
                context.storage.declarations.semantic_relations
                    .unit[context.storage.declarations.semantic_relations.index(
                        new_semantic_relations)] = std::nullopt;
                return new_semantic_relations;
            }();
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));

    module = valid_integer_scalar_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.integer_scalars.clone(rows_1.front())};
            context.storage.declarations.integer_scalars
                .relationship[context.storage.declarations.integer_scalars.index(row_0)] = [&] {
                auto const new_semantic_relations{
                    context.storage.declarations.semantic_relations.append()};
                context.storage.declarations.semantic_relations
                    .kind[context.storage.declarations.semantic_relations.index(
                        new_semantic_relations)] = SemanticRelationKind::offset_into;
                context.storage.declarations.semantic_relations
                    .target[context.storage.declarations.semantic_relations.index(
                        new_semantic_relations)] = [&] {
                    auto const new_type_refs{context.storage.declarations.type_refs.append()};
                    context.storage.declarations.type_refs
                        .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                        context.storage.strings.intern("DamageReason");
                    return new_type_refs;
                }();
                context.storage.declarations.semantic_relations
                    .unit[context.storage.declarations.semantic_relations.index(
                        new_semantic_relations)] = std::nullopt;
                return new_semantic_relations;
            }();
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_integer_scalar_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.integer_scalars.clone(rows_1.front())};
            context.storage.declarations.integer_scalars
                .relationship[context.storage.declarations.integer_scalars.index(row_0)] = [&] {
                auto const new_semantic_relations{
                    context.storage.declarations.semantic_relations.append()};
                context.storage.declarations.semantic_relations
                    .kind[context.storage.declarations.semantic_relations.index(
                        new_semantic_relations)] = SemanticRelationKind::index_into;
                context.storage.declarations.semantic_relations
                    .target[context.storage.declarations.semantic_relations.index(
                        new_semantic_relations)] = [&] {
                    auto const new_type_refs{context.storage.declarations.type_refs.append()};
                    context.storage.declarations.type_refs
                        .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                        context.storage.strings.intern("DamageReason");
                    return new_type_refs;
                }();
                context.storage.declarations.semantic_relations
                    .unit[context.storage.declarations.semantic_relations.index(
                        new_semantic_relations)] = SemanticRelationUnit::elements;
                return new_semantic_relations;
            }();
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_integer_scalar_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.integer_scalars.clone(rows_1.front())};
            context.storage.declarations.integer_scalars
                .relationship[context.storage.declarations.integer_scalars.index(row_0)] = [&] {
                auto const new_semantic_relations{
                    context.storage.declarations.semantic_relations.append()};
                context.storage.declarations.semantic_relations
                    .kind[context.storage.declarations.semantic_relations.index(
                        new_semantic_relations)] = SemanticRelationKind::offset_into;
                context.storage.declarations.semantic_relations
                    .target[context.storage.declarations.semantic_relations.index(
                        new_semantic_relations)] = [&] {
                    auto const new_type_refs{context.storage.declarations.type_refs.append()};
                    context.storage.declarations.type_refs
                        .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                        context.storage.strings.intern("DamageReason");
                    return new_type_refs;
                }();
                context.storage.declarations.semantic_relations
                    .unit[context.storage.declarations.semantic_relations.index(
                        new_semantic_relations)] = SemanticRelationUnit::bytes;
                return new_semantic_relations;
            }();
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));
}

TEST(Validation, ValidatesLinearQuantizedRepresentationsAndEmitsOnlyConfiguredHeader) {
    CompilerTestContext context;
    auto manifest{valid_linear_quantized_manifest(context)};
    auto lowered{lower_test_manifest(context, manifest)};
    ASSERT_EQ(lowered.size(), 2U);
    auto const header{context.ir.modules.header[context.ir.modules.index(lowered[1])]};
    ASSERT_NE(header.value, 0U);
    EXPECT_EQ(context.storage.strings.view(context.ir.files.path[context.ir.files.index(header)]),
              "Representations.h");
    EXPECT_EQ(render_modules(context.ir, lowered)[1].content.find("struct "), std::string::npos);

    manifest = valid_linear_quantized_manifest(context);

    manifest.modules[0] = [&] {
        auto row_2{context.storage.declarations.normal_modules.clone(manifest.modules[0])};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_2)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_2)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{context.storage.declarations.integer_scalars.clone(rows_1.front())};
                context.storage.declarations.integer_scalars
                    .signedness[context.storage.declarations.integer_scalars.index(row_0)] = true;
                return row_0;
            }();
            return context.storage.declarations.range(rows_1);
        }();
        return row_2;
    }();
    manifest.modules[0] = [&] {
        auto row_2{context.storage.declarations.normal_modules.clone(manifest.modules[0])};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_2)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_2)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{context.storage.declarations.integer_scalars.clone(rows_1.front())};
                context.storage.declarations.integer_scalars
                    .minimum_value[context.storage.declarations.integer_scalars.index(row_0)] =
                    -100;
                return row_0;
            }();
            return context.storage.declarations.range(rows_1);
        }();
        return row_2;
    }();
    manifest.modules[0] = [&] {
        auto row_2{context.storage.declarations.normal_modules.clone(manifest.modules[0])};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_2)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_2)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{context.storage.declarations.integer_scalars.clone(rows_1.front())};
                context.storage.declarations.integer_scalars
                    .maximum_value[context.storage.declarations.integer_scalars.index(row_0)] = 100;
                return row_0;
            }();
            return context.storage.declarations.range(rows_1);
        }();
        return row_2;
    }();
    manifest.modules[0] = [&] {
        auto row_2{context.storage.declarations.normal_modules.clone(manifest.modules[0])};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_2)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_2)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{context.storage.declarations.integer_scalars.clone(rows_1.front())};
                context.storage.declarations.integer_scalars
                    .named_codes[context.storage.declarations.integer_scalars.index(row_0)] = {};
                return row_0;
            }();
            return context.storage.declarations.range(rows_1);
        }();
        return row_2;
    }();
    EXPECT_NO_THROW(lower_test_manifest(context, manifest));

    manifest = valid_linear_quantized_manifest(context);
    manifest.modules[1] = [&] {
        auto row_2{context.storage.declarations.normal_modules.clone(manifest.modules[1])};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_2)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_2)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{context.storage.declarations.linear_quantizeds.clone(rows_1.front())};
                context.storage.declarations.linear_quantizeds
                    .bit_width[context.storage.declarations.linear_quantizeds.index(row_0)] = 1;
                return row_0;
            }();
            return context.storage.declarations.range(rows_1);
        }();
        return row_2;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);

    manifest = valid_linear_quantized_manifest(context);

    manifest.modules[1] = [&] {
        auto row_2{context.storage.declarations.normal_modules.clone(manifest.modules[1])};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_2)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_2)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{context.storage.declarations.linear_quantizeds.clone(rows_1.front())};
                context.storage.declarations.linear_quantizeds
                    .bit_width[context.storage.declarations.linear_quantizeds.index(row_0)] = 2;
                return row_0;
            }();
            return context.storage.declarations.range(rows_1);
        }();
        return row_2;
    }();
    manifest.modules[1] = [&] {
        auto row_2{context.storage.declarations.normal_modules.clone(manifest.modules[1])};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_2)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_2)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{context.storage.declarations.linear_quantizeds.clone(rows_1.front())};
                context.storage.declarations.linear_quantizeds
                    .reserved_codes[context.storage.declarations.linear_quantizeds.index(row_0)] =
                    3;
                return row_0;
            }();
            return context.storage.declarations.range(rows_1);
        }();
        return row_2;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);

    manifest = valid_linear_quantized_manifest(context);
    manifest.modules[0] = [&] {
        auto row_2{context.storage.declarations.normal_modules.clone(manifest.modules[0])};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_2)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_2)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{context.storage.declarations.integer_scalars.clone(rows_1.front())};
                context.storage.declarations.integer_scalars
                    .maximum_value[context.storage.declarations.integer_scalars.index(row_0)] = 0;
                return row_0;
            }();
            return context.storage.declarations.range(rows_1);
        }();
        return row_2;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);

    manifest = valid_linear_quantized_manifest(context);
    manifest.modules[1] = [&] {
        auto row_2{context.storage.declarations.normal_modules.clone(manifest.modules[1])};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_2)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_2)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{context.storage.declarations.linear_quantizeds.clone(rows_1.front())};
                context.storage.declarations.linear_quantizeds
                    .source[context.storage.declarations.linear_quantizeds.index(row_0)] = [&] {
                    auto const new_type_refs{context.storage.declarations.type_refs.append()};
                    context.storage.declarations.type_refs
                        .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                        context.storage.strings.intern("std::uint32_t");
                    return new_type_refs;
                }();
                return row_0;
            }();
            return context.storage.declarations.range(rows_1);
        }();
        return row_2;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);

    manifest = valid_linear_quantized_manifest(context);
    manifest.modules[1] = [&] {
        auto row_1{context.storage.declarations.normal_modules.clone(manifest.modules[1])};
        context.storage.declarations.normal_modules
            .settings[context.storage.declarations.normal_modules.index(row_1)] = [&] {
            auto row_0{context.storage.declarations.module_settings_store.clone(
                context.storage.declarations.normal_modules
                    .settings[context.storage.declarations.normal_modules.index(row_1)])};
            context.storage.declarations.module_settings_store
                .source[context.storage.declarations.module_settings_store.index(row_0)] =
                context.storage.strings.add("Representations.cpp");
            return row_0;
        }();
        return row_1;
    }();
    EXPECT_EQ(render_modules(context.ir, lower_test_manifest(context, manifest)).size(), 3U);
}

TEST(Validation, ValidatesIntegerVarintSourceAndSignedness) {
    CompilerTestContext context;
    EXPECT_NO_THROW(lower_test_manifest(context, valid_integer_varint_manifest(context)));
    EXPECT_NO_THROW(lower_test_manifest(
        context,
        valid_integer_varint_manifest(context, true, IntegerVarintEncoding::signed_varint)));
    EXPECT_NO_THROW(lower_test_manifest(
        context,
        valid_integer_varint_manifest(context, true, IntegerVarintEncoding::zigzag_varint)));

    EXPECT_THROW(lower_test_manifest(context,
                                     valid_integer_varint_manifest(
                                         context, true, IntegerVarintEncoding::unsigned_varint)),
                 std::invalid_argument);
    EXPECT_THROW(lower_test_manifest(context,
                                     valid_integer_varint_manifest(
                                         context, false, IntegerVarintEncoding::signed_varint)),
                 std::invalid_argument);
    EXPECT_THROW(lower_test_manifest(context,
                                     valid_integer_varint_manifest(
                                         context, false, IntegerVarintEncoding::zigzag_varint)),
                 std::invalid_argument);

    auto manifest{valid_integer_varint_manifest(context)};
    manifest.modules[1] = [&] {
        auto row_2{context.storage.declarations.normal_modules.clone(manifest.modules[1])};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_2)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_2)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{context.storage.declarations.integer_varints.clone(rows_1.front())};
                context.storage.declarations.integer_varints
                    .source[context.storage.declarations.integer_varints.index(row_0)] = [&] {
                    auto const new_type_refs{context.storage.declarations.type_refs.append()};
                    context.storage.declarations.type_refs
                        .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                        context.storage.strings.intern("std::uint32_t");
                    return new_type_refs;
                }();
                return row_0;
            }();
            return context.storage.declarations.range(rows_1);
        }();
        return row_2;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);

    manifest = valid_integer_varint_manifest(context);

    manifest.modules[1] = [&] {
        auto row_0{context.storage.declarations.normal_modules.clone(manifest.modules[1])};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_0)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_0)])};
            auto rows{std::vector(original_items.begin(), original_items.end())};
            rows.push_back([&] {
                auto const new_linear_quantizeds{
                    context.storage.declarations.linear_quantizeds.append()};
                context.storage.declarations.linear_quantizeds
                    .name[context.storage.declarations.linear_quantizeds.index(
                        new_linear_quantizeds)] =
                    context.storage.declarations.integer_varints
                        .name[context.storage.declarations.integer_varints.index(
                            context.storage.declarations
                                .items(context.storage.declarations.normal_modules
                                           .declarations[context.storage.declarations.normal_modules
                                                             .index(manifest.modules[1])])
                                .front())];
                context.storage.declarations.linear_quantizeds
                    .source[context.storage.declarations.linear_quantizeds.index(
                        new_linear_quantizeds)] = [&] {
                    auto const new_type_refs{context.storage.declarations.type_refs.append()};
                    context.storage.declarations.type_refs
                        .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                        context.storage.strings.intern("DamageReason");
                    return new_type_refs;
                }();
                context.storage.declarations.linear_quantizeds
                    .bit_width[context.storage.declarations.linear_quantizeds.index(
                        new_linear_quantizeds)] = 4;
                context.storage.declarations.linear_quantizeds
                    .reserved_codes[context.storage.declarations.linear_quantizeds.index(
                        new_linear_quantizeds)] = 0;
                context.storage.declarations.linear_quantizeds
                    .clipping[context.storage.declarations.linear_quantizeds.index(
                        new_linear_quantizeds)] = QuantizationClipping::reject;
                return new_linear_quantizeds;
            }());
            return context.storage.declarations.range(rows);
        }();
        return row_0;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);
}

TEST(Validation, ValidatesFixedPointWidthsAndEmitsOnlyConfiguredHeader) {
    CompilerTestContext context;
    auto manifest{valid_fixed_point_manifest(context)};
    auto lowered{lower_test_manifest(context, manifest)};
    ASSERT_EQ(lowered.size(), 1U);
    auto const header{context.ir.modules.header[context.ir.modules.index(lowered.front())]};
    ASSERT_NE(header.value, 0U);
    EXPECT_EQ(context.storage.strings.view(context.ir.files.path[context.ir.files.index(header)]),
              "Representations.h");
    EXPECT_EQ(render_modules(context.ir, lowered).front().content.find("struct "),
              std::string::npos);

    manifest = valid_fixed_point_manifest(context, false);

    manifest.modules.front() = [&] {
        auto row_2{context.storage.declarations.normal_modules.clone(manifest.modules.front())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_2)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_2)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{context.storage.declarations.fixed_points.clone(rows_1.front())};
                context.storage.declarations.fixed_points
                    .total_bits[context.storage.declarations.fixed_points.index(row_0)] = 8;
                return row_0;
            }();
            return context.storage.declarations.range(rows_1);
        }();
        return row_2;
    }();
    manifest.modules.front() = [&] {
        auto row_2{context.storage.declarations.normal_modules.clone(manifest.modules.front())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_2)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_2)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{context.storage.declarations.fixed_points.clone(rows_1.front())};
                context.storage.declarations.fixed_points
                    .fractional_bits[context.storage.declarations.fixed_points.index(row_0)] = 8;
                return row_0;
            }();
            return context.storage.declarations.range(rows_1);
        }();
        return row_2;
    }();
    EXPECT_NO_THROW(lower_test_manifest(context, manifest));

    manifest = valid_fixed_point_manifest(context);
    manifest.modules.front() = [&] {
        auto row_2{context.storage.declarations.normal_modules.clone(manifest.modules.front())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_2)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_2)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{context.storage.declarations.fixed_points.clone(rows_1.front())};
                context.storage.declarations.fixed_points
                    .fractional_bits[context.storage.declarations.fixed_points.index(row_0)] = 16;
                return row_0;
            }();
            return context.storage.declarations.range(rows_1);
        }();
        return row_2;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);

    manifest = valid_fixed_point_manifest(context);
    manifest.modules.front() = [&] {
        auto row_2{context.storage.declarations.normal_modules.clone(manifest.modules.front())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_2)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_2)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{context.storage.declarations.fixed_points.clone(rows_1.front())};
                context.storage.declarations.fixed_points
                    .total_bits[context.storage.declarations.fixed_points.index(row_0)] = 0;
                return row_0;
            }();
            return context.storage.declarations.range(rows_1);
        }();
        return row_2;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);

    manifest = valid_fixed_point_manifest(context);

    manifest.modules.front() = [&] {
        auto row_0{context.storage.declarations.normal_modules.clone(manifest.modules.front())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_0)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_0)])};
            auto rows{std::vector(original_items.begin(), original_items.end())};
            rows.push_back(
                context.storage.declarations
                    .items(context.storage.declarations.normal_modules
                               .declarations[context.storage.declarations.normal_modules.index(
                                   manifest.modules.front())])
                    .front());
            return context.storage.declarations.range(rows);
        }();
        return row_0;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);
}

TEST(Validation, ValidatesMiniFloatEncodingAndEmitsNoPhysicalType) {
    CompilerTestContext context;
    auto manifest{valid_mini_float_manifest(context)};
    auto lowered{lower_test_manifest(context, manifest)};
    ASSERT_EQ(lowered.size(), 1U);
    auto const header{context.ir.modules.header[context.ir.modules.index(lowered.front())]};
    ASSERT_NE(header.value, 0U);
    EXPECT_EQ(context.storage.strings.view(context.ir.files.path[context.ir.files.index(header)]),
              "Representations.h");
    EXPECT_EQ(render_modules(context.ir, lowered).front().content.find("struct "),
              std::string::npos);

    auto invalidate = [&](auto edit) {
        auto invalid{valid_mini_float_manifest(context)};
        auto& declarations{context.storage.declarations};
        auto const module{declarations.normal_modules.clone(invalid.modules.front())};
        auto const original{declarations.items(
            declarations.normal_modules.declarations[declarations.normal_modules.index(module)])};
        auto rows{std::vector(original.begin(), original.end())};
        auto value{declarations.mini_floats.clone(rows.front())};
        edit(value);
        rows.front() = value;
        declarations.normal_modules.declarations[declarations.normal_modules.index(module)] =
            declarations.range(rows);
        invalid.modules.front() = module;
        EXPECT_THROW(lower_test_manifest(context, invalid), std::invalid_argument);
    };
    invalidate([&](lispb::codegen::DeclarationHandle& value) {
        context.storage.declarations.mini_floats
            .sign_bits[context.storage.declarations.mini_floats.index(value)] = 2;
    });
    invalidate([&](lispb::codegen::DeclarationHandle& value) {
        context.storage.declarations.mini_floats
            .exponent_bits[context.storage.declarations.mini_floats.index(value)] = 1;
    });
    invalidate([&](lispb::codegen::DeclarationHandle& value) {
        context.storage.declarations.mini_floats
            .exponent_bits[context.storage.declarations.mini_floats.index(value)] = 16;
    });
    invalidate([&](lispb::codegen::DeclarationHandle& value) {
        context.storage.declarations.mini_floats
            .significand_bits[context.storage.declarations.mini_floats.index(value)] = 63;
    });
    invalidate([&](lispb::codegen::DeclarationHandle& value) {
        context.storage.declarations.mini_floats
            .exponent_bits[context.storage.declarations.mini_floats.index(value)] = 15;
        context.storage.declarations.mini_floats
            .significand_bits[context.storage.declarations.mini_floats.index(value)] = 49;
    });
    invalidate([&](lispb::codegen::DeclarationHandle& value) {
        context.storage.declarations.mini_floats
            .exponent_bias[context.storage.declarations.mini_floats.index(value)] = -32'769;
    });
    invalidate([&](lispb::codegen::DeclarationHandle& value) {
        context.storage.declarations.mini_floats
            .exponent_bias[context.storage.declarations.mini_floats.index(value)] = 32'768;
    });

    manifest = valid_mini_float_manifest(context);

    manifest.modules.front() = [&] {
        auto row_0{context.storage.declarations.normal_modules.clone(manifest.modules.front())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_0)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_0)])};
            auto rows{std::vector(original_items.begin(), original_items.end())};
            rows.push_back([&] {
                auto const new_fixed_points{context.storage.declarations.fixed_points.append()};
                context.storage.declarations.fixed_points
                    .name[context.storage.declarations.fixed_points.index(new_fixed_points)] =
                    context.storage.declarations.mini_floats
                        .name[context.storage.declarations.mini_floats.index(
                            context.storage.declarations
                                .items(context.storage.declarations.normal_modules
                                           .declarations[context.storage.declarations.normal_modules
                                                             .index(manifest.modules.front())])
                                .front())];
                context.storage.declarations.fixed_points
                    .signedness[context.storage.declarations.fixed_points.index(new_fixed_points)] =
                    true;
                context.storage.declarations.fixed_points
                    .total_bits[context.storage.declarations.fixed_points.index(new_fixed_points)] =
                    16;
                context.storage.declarations.fixed_points
                    .fractional_bits[context.storage.declarations.fixed_points.index(
                        new_fixed_points)] = 4;
                context.storage.declarations.fixed_points
                    .rounding[context.storage.declarations.fixed_points.index(new_fixed_points)] =
                    FixedPointRounding::nearest_even;
                return new_fixed_points;
            }());
            return context.storage.declarations.range(rows);
        }();
        return row_0;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);
}

TEST(Validation, ValidatesOptionalSentinelSourceAndCodeRole) {
    CompilerTestContext context;
    EXPECT_NO_THROW(lower_test_manifest(context, valid_optional_sentinel_manifest(context)));

    auto manifest{valid_optional_sentinel_manifest(context)};

    manifest.modules[1] = [&] {
        auto row_2{context.storage.declarations.normal_modules.clone(manifest.modules[1])};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_2)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_2)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{context.storage.declarations.optional_sentinels.clone(rows_1.front())};
                context.storage.declarations.optional_sentinels
                    .source[context.storage.declarations.optional_sentinels.index(row_0)] = [&] {
                    auto const new_type_refs{context.storage.declarations.type_refs.append()};
                    context.storage.declarations.type_refs
                        .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                        context.storage.strings.intern("std::uint32_t");
                    return new_type_refs;
                }();
                return row_0;
            }();
            return context.storage.declarations.range(rows_1);
        }();
        return row_2;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);

    manifest = valid_optional_sentinel_manifest(context);
    manifest.modules[1] = [&] {
        auto row_2{context.storage.declarations.normal_modules.clone(manifest.modules[1])};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_2)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_2)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{context.storage.declarations.optional_sentinels.clone(rows_1.front())};
                context.storage.declarations.optional_sentinels
                    .sentinel[context.storage.declarations.optional_sentinels.index(row_0)] =
                    context.storage.strings.intern("Missing");
                return row_0;
            }();
            return context.storage.declarations.range(rows_1);
        }();
        return row_2;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);

    manifest = valid_optional_sentinel_manifest(context);
    manifest.modules[1] = [&] {
        auto row_2{context.storage.declarations.normal_modules.clone(manifest.modules[1])};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_2)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_2)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{context.storage.declarations.optional_sentinels.clone(rows_1.front())};
                context.storage.declarations.optional_sentinels
                    .sentinel[context.storage.declarations.optional_sentinels.index(row_0)] =
                    context.storage.strings.intern("Unknown");
                return row_0;
            }();
            return context.storage.declarations.range(rows_1);
        }();
        return row_2;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);

    manifest = valid_optional_sentinel_manifest(context);
    manifest.modules[0] = [&] {
        auto row_2{context.storage.declarations.normal_modules.clone(manifest.modules[0])};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_2)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_2)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{context.storage.declarations.integer_scalars.clone(rows_1.front())};
                context.storage.declarations.integer_scalars
                    .named_codes[context.storage.declarations.integer_scalars.index(row_0)] = {};
                return row_0;
            }();
            return context.storage.declarations.range(rows_1);
        }();
        return row_2;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);

    manifest = valid_optional_sentinel_manifest(context);

    manifest.modules[1] = [&] {
        auto row_0{context.storage.declarations.normal_modules.clone(manifest.modules[1])};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_0)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_0)])};
            auto rows{std::vector(original_items.begin(), original_items.end())};
            rows.push_back([&] {
                auto const new_fixed_points{context.storage.declarations.fixed_points.append()};
                context.storage.declarations.fixed_points
                    .name[context.storage.declarations.fixed_points.index(new_fixed_points)] =
                    context.storage.declarations.optional_sentinels
                        .name[context.storage.declarations.optional_sentinels.index(
                            context.storage.declarations
                                .items(context.storage.declarations.normal_modules
                                           .declarations[context.storage.declarations.normal_modules
                                                             .index(manifest.modules[1])])
                                .front())];
                context.storage.declarations.fixed_points
                    .signedness[context.storage.declarations.fixed_points.index(new_fixed_points)] =
                    false;
                context.storage.declarations.fixed_points
                    .total_bits[context.storage.declarations.fixed_points.index(new_fixed_points)] =
                    8;
                context.storage.declarations.fixed_points
                    .fractional_bits[context.storage.declarations.fixed_points.index(
                        new_fixed_points)] = 0;
                context.storage.declarations.fixed_points
                    .rounding[context.storage.declarations.fixed_points.index(new_fixed_points)] =
                    FixedPointRounding::nearest_even;
                return new_fixed_points;
            }());
            return context.storage.declarations.range(rows);
        }();
        return row_0;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);
}

TEST(Validation, ValidatesOptionalPresenceBitSourceAndSharedNames) {
    CompilerTestContext context;
    EXPECT_NO_THROW(lower_test_manifest(context, valid_optional_presence_bit_manifest(context)));

    auto manifest{valid_optional_presence_bit_manifest(context)};
    manifest.modules[1] = [&] {
        auto row_2{context.storage.declarations.normal_modules.clone(manifest.modules[1])};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_2)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_2)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{
                    context.storage.declarations.optional_presence_bits.clone(rows_1.front())};
                context.storage.declarations.optional_presence_bits
                    .source[context.storage.declarations.optional_presence_bits.index(row_0)] =
                    [&] {
                        auto const new_type_refs{context.storage.declarations.type_refs.append()};
                        context.storage.declarations.type_refs
                            .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                            context.storage.strings.intern("std::uint32_t");
                        return new_type_refs;
                    }();
                return row_0;
            }();
            return context.storage.declarations.range(rows_1);
        }();
        return row_2;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);

    manifest = valid_optional_presence_bit_manifest(context);

    manifest.modules[1] = [&] {
        auto row_0{context.storage.declarations.normal_modules.clone(manifest.modules[1])};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_0)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_0)])};
            auto rows{std::vector(original_items.begin(), original_items.end())};
            rows.push_back([&] {
                auto const new_optional_sentinels{
                    context.storage.declarations.optional_sentinels.append()};
                context.storage.declarations.optional_sentinels
                    .name[context.storage.declarations.optional_sentinels.index(
                        new_optional_sentinels)] =
                    context.storage.declarations.optional_presence_bits
                        .name[context.storage.declarations.optional_presence_bits.index(
                            context.storage.declarations
                                .items(context.storage.declarations.normal_modules
                                           .declarations[context.storage.declarations.normal_modules
                                                             .index(manifest.modules[1])])
                                .front())];
                context.storage.declarations.optional_sentinels
                    .source[context.storage.declarations.optional_sentinels.index(
                        new_optional_sentinels)] = [&] {
                    auto const new_type_refs{context.storage.declarations.type_refs.append()};
                    context.storage.declarations.type_refs
                        .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                        context.storage.strings.intern("DamageReason");
                    return new_type_refs;
                }();
                context.storage.declarations.optional_sentinels
                    .sentinel[context.storage.declarations.optional_sentinels.index(
                        new_optional_sentinels)] = context.storage.strings.intern("Invalid");
                return new_optional_sentinels;
            }());
            return context.storage.declarations.range(rows);
        }();
        return row_0;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);
}

TEST(Validation, RejectsInvalidEnumModuleConfiguration) {
    CompilerTestContext context;
    auto module{valid_enum_module(context)};
    context.storage.declarations.normal_modules
        .settings[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto row_0{context.storage.declarations.module_settings_store.clone(
            context.storage.declarations.normal_modules
                .settings[context.storage.declarations.normal_modules.index(module)])};
        context.storage.declarations.module_settings_store
            .namespace_name[context.storage.declarations.module_settings_store.index(row_0)] =
            context.storage.strings.intern("project");
        return row_0;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_enum_module(context);
    context.storage.declarations.normal_modules
        .settings[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto row_0{context.storage.declarations.module_settings_store.clone(
            context.storage.declarations.normal_modules
                .settings[context.storage.declarations.normal_modules.index(module)])};
        context.storage.declarations.module_settings_store
            .source[context.storage.declarations.module_settings_store.index(row_0)]
            .reset();
        return row_0;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_enum_module(context);
    context.storage.declarations.normal_modules
        .enum_helper_namespace[context.storage.declarations.normal_modules.index(module)] =
        context.storage.strings.intern("bad-name");
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_enum_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .conversions[context.storage.declarations.enums.index(row_0)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.enums
                        .conversions[context.storage.declarations.enums.index(row_0)])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.push_back(EnumConversion::string_view);
                return context.storage.declarations.range(rows);
            }();
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_enum_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .reflection[context.storage.declarations.enums.index(row_0)] = EnumReflection::none;
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.enums.clone(rows_3.front())};
            context.storage.declarations.enums
                .values[context.storage.declarations.enums.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.enums
                        .values[context.storage.declarations.enums.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{context.storage.declarations.enumerators.clone(rows_1.front())};
                    context.storage.declarations.enumerators
                        .hidden[context.storage.declarations.enumerators.index(row_0)] = true;
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsUnsupportedNativeEnumCombinations) {
    CompilerTestContext context;
    for (auto const& helpers : {std::vector{NativeEnumHelper::traits, NativeEnumHelper::traits},
                                std::vector{static_cast<NativeEnumHelper>(999)},
                                std::vector{NativeEnumHelper::to_serialized_string},
                                std::vector{NativeEnumHelper::try_parse_serialized}}) {
        auto invalid{valid_native_enum_module(context)};

        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(invalid)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(invalid)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
                context.storage.declarations.enums
                    .helpers[context.storage.declarations.enums.index(row_0)] =
                    context.storage.declarations.range<NativeEnumHelper>(helpers);
                return row_0;
            }();
            return context.storage.declarations.range(rows_1);
        }();
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(invalid)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(invalid)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{context.storage.declarations.enums.clone(rows_3.front())};
                context.storage.declarations.enums
                    .values[context.storage.declarations.enums.index(row_2)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.enums
                            .values[context.storage.declarations.enums.index(row_2)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1.front() = [&] {
                        auto row_0{context.storage.declarations.enumerators.clone(rows_1.front())};
                        context.storage.declarations.enumerators
                            .serialized_name[context.storage.declarations.enumerators.index(row_0)]
                            .reset();
                        return row_0;
                    }();
                    return context.storage.declarations.range(rows_1);
                }();
                return row_2;
            }();
            return context.storage.declarations.range(rows_3);
        }();
        EXPECT_THROW(lower_test_manifest(context, manifest_with(context, invalid)),
                     std::invalid_argument);
    }
    auto unreal{valid_enum_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(unreal)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(unreal)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .helpers[context.storage.declarations.enums.index(row_0)] =
                context.storage.declarations.range<lispb::codegen::NativeEnumHelper>(
                    {NativeEnumHelper::traits});
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, unreal)),
                 std::invalid_argument);

    auto module{valid_native_enum_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows{std::vector(original_items.begin(), original_items.end())};
        rows.push_back(
            context.storage.declarations
                .items(context.storage.declarations.normal_modules
                           .declarations[context.storage.declarations.normal_modules.index(
                               valid_enum_module(context))])
                .front());
        return context.storage.declarations.range(rows);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_native_enum_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .reflection[context.storage.declarations.enums.index(row_0)] =
                EnumReflection::uenum;
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_native_enum_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .enum_array[context.storage.declarations.enums.index(row_0)] = true;
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));

    module = valid_native_enum_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .conversions[context.storage.declarations.enums.index(row_0)] =
                context.storage.declarations.range<lispb::codegen::EnumConversion>(
                    {EnumConversion::string_view});
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_native_enum_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .export_specifier[context.storage.declarations.enums.index(row_0)] =
                context.storage.strings.intern("PROJECT_API");
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_native_enum_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .unreal_projection[context.storage.declarations.enums.index(row_0)] = [&] {
                auto const new_enum_unreal_projections{
                    context.storage.declarations.enum_unreal_projections.append()};
                context.storage.declarations.enum_unreal_projections
                    .name[context.storage.declarations.enum_unreal_projections.index(
                        new_enum_unreal_projections)] =
                    context.storage.strings.intern("ENativeMode");
                context.storage.declarations.enum_unreal_projections
                    .header[context.storage.declarations.enum_unreal_projections.index(
                        new_enum_unreal_projections)] =
                    context.storage.strings.add("Project/NativeMode.h");
                context.storage.declarations.enum_unreal_projections
                    .header_include[context.storage.declarations.enum_unreal_projections.index(
                        new_enum_unreal_projections)] =
                    context.storage.strings.add("Project/NativeMode.h");
                context.storage.declarations.enum_unreal_projections
                    .conversion_header[context.storage.declarations.enum_unreal_projections.index(
                        new_enum_unreal_projections)] =
                    context.storage.strings.add("Project/NativeModeConversion.h");
                context.storage.declarations.enum_unreal_projections
                    .native_header_include[context.storage.declarations.enum_unreal_projections
                                               .index(new_enum_unreal_projections)] =
                    context.storage.strings.add("project/NativeEnums.h");
                context.storage.declarations.enum_unreal_projections
                    .reflection[context.storage.declarations.enum_unreal_projections.index(
                        new_enum_unreal_projections)] = EnumReflection::none;
                return new_enum_unreal_projections;
            }();
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_enum_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .unreal_projection[context.storage.declarations.enums.index(row_0)] = [&] {
                auto const new_enum_unreal_projections{
                    context.storage.declarations.enum_unreal_projections.append()};
                context.storage.declarations.enum_unreal_projections
                    .name[context.storage.declarations.enum_unreal_projections.index(
                        new_enum_unreal_projections)] =
                    context.storage.strings.intern("ENativeMode");
                context.storage.declarations.enum_unreal_projections
                    .header[context.storage.declarations.enum_unreal_projections.index(
                        new_enum_unreal_projections)] =
                    context.storage.strings.add("Project/NativeMode.h");
                context.storage.declarations.enum_unreal_projections
                    .header_include[context.storage.declarations.enum_unreal_projections.index(
                        new_enum_unreal_projections)] =
                    context.storage.strings.add("Project/NativeMode.h");
                context.storage.declarations.enum_unreal_projections
                    .conversion_header[context.storage.declarations.enum_unreal_projections.index(
                        new_enum_unreal_projections)] =
                    context.storage.strings.add("Project/NativeModeConversion.h");
                context.storage.declarations.enum_unreal_projections
                    .native_header_include[context.storage.declarations.enum_unreal_projections
                                               .index(new_enum_unreal_projections)] =
                    context.storage.strings.add("project/NativeEnums.h");
                return new_enum_unreal_projections;
            }();
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsIncompleteOrAmbiguousSerializedEnumNames) {
    CompilerTestContext context;
    auto module{valid_enum_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .conversions[context.storage.declarations.enums.index(row_0)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.enums
                        .conversions[context.storage.declarations.enums.index(row_0)])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.push_back(EnumConversion::try_parse_serialized);
                return context.storage.declarations.range(rows);
            }();
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_enum_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.enums.clone(rows_3.front())};
            context.storage.declarations.enums
                .values[context.storage.declarations.enums.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.enums
                        .values[context.storage.declarations.enums.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{context.storage.declarations.enumerators.clone(rows_1.front())};
                    context.storage.declarations.enumerators
                        .serialized_name[context.storage.declarations.enumerators.index(row_0)] =
                        context.storage.strings.add("value");
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .values[context.storage.declarations.enums.index(row_0)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.enums
                        .values[context.storage.declarations.enums.index(row_0)])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.push_back([&] {
                    auto const new_enumerators{context.storage.declarations.enumerators.append()};
                    context.storage.declarations.enumerators
                        .name[context.storage.declarations.enumerators.index(new_enumerators)] =
                        context.storage.strings.intern("Other");
                    context.storage.declarations.enumerators
                        .initializer[context.storage.declarations.enumerators.index(
                            new_enumerators)] = std::nullopt;
                    context.storage.declarations.enumerators
                        .display_name[context.storage.declarations.enumerators.index(
                            new_enumerators)] = std::nullopt;
                    context.storage.declarations.enumerators
                        .hidden[context.storage.declarations.enumerators.index(new_enumerators)] =
                        false;
                    context.storage.declarations.enumerators
                        .serialized_name[context.storage.declarations.enumerators.index(
                            new_enumerators)] = context.storage.strings.add("value");
                    return new_enumerators;
                }());
                return context.storage.declarations.range(rows);
            }();
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .conversions[context.storage.declarations.enums.index(row_0)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.enums
                        .conversions[context.storage.declarations.enums.index(row_0)])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.push_back(EnumConversion::try_parse_serialized);
                return context.storage.declarations.range(rows);
            }();
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsInvalidEnumArrayDefinitions) {
    CompilerTestContext context;
    auto module{valid_enum_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .enum_array[context.storage.declarations.enums.index(row_0)] = true;
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.enums.clone(rows_3.front())};
            context.storage.declarations.enums
                .values[context.storage.declarations.enums.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.enums
                        .values[context.storage.declarations.enums.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{context.storage.declarations.enumerators.clone(rows_1.front())};
                    context.storage.declarations.enumerators
                        .initializer[context.storage.declarations.enumerators.index(row_0)] =
                        context.storage.strings.add("0");
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));

    for (auto const* code : {"2", "0", "-1", "unknown_code", "0x2"}) {
        SCOPED_TRACE(code);
        module = valid_enum_module(context);

        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(module)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
                context.storage.declarations.enums
                    .enum_array[context.storage.declarations.enums.index(row_0)] = true;
                return row_0;
            }();
            return context.storage.declarations.range(rows_1);
        }();
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(module)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
                context.storage.declarations.enums
                    .values[context.storage.declarations.enums.index(row_0)] =
                    context.storage.declarations.range<lispb::codegen::EnumeratorId>(
                        {[&] {
                             auto const new_enumerators{
                                 context.storage.declarations.enumerators.append()};
                             context.storage.declarations.enumerators
                                 .name[context.storage.declarations.enumerators.index(
                                     new_enumerators)] = context.storage.strings.intern("First");
                             context.storage.declarations.enumerators
                                 .initializer[context.storage.declarations.enumerators.index(
                                     new_enumerators)] = context.storage.strings.add("0");
                             return new_enumerators;
                         }(),
                         [&] {
                             auto const new_enumerators{
                                 context.storage.declarations.enumerators.append()};
                             context.storage.declarations.enumerators
                                 .name[context.storage.declarations.enumerators.index(
                                     new_enumerators)] = context.storage.strings.intern("Second");
                             context.storage.declarations.enumerators
                                 .initializer[context.storage.declarations.enumerators.index(
                                     new_enumerators)] = context.storage.strings.add(code);
                             return new_enumerators;
                         }()});
                return row_0;
            }();
            return context.storage.declarations.range(rows_1);
        }();
        EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                     std::invalid_argument);
    }

    module = valid_native_enum_module(context);

    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .enum_array[context.storage.declarations.enums.index(row_0)] = true;
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .values[context.storage.declarations.enums.index(row_0)] =
                context.storage.declarations.range<lispb::codegen::EnumeratorId>(
                    {[&] {
                         auto const new_enumerators{
                             context.storage.declarations.enumerators.append()};
                         context.storage.declarations.enumerators
                             .name[context.storage.declarations.enumerators.index(
                                 new_enumerators)] = context.storage.strings.intern("First");
                         context.storage.declarations.enumerators
                             .initializer[context.storage.declarations.enumerators.index(
                                 new_enumerators)] = context.storage.strings.add("0x0");
                         return new_enumerators;
                     }(),
                     [&] {
                         auto const new_enumerators{
                             context.storage.declarations.enumerators.append()};
                         context.storage.declarations.enumerators
                             .name[context.storage.declarations.enumerators.index(
                                 new_enumerators)] = context.storage.strings.intern("Second");
                         context.storage.declarations.enumerators
                             .initializer[context.storage.declarations.enumerators.index(
                                 new_enumerators)] = context.storage.strings.add("1");
                         return new_enumerators;
                     }()});
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));

    module = valid_enum_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .enum_array[context.storage.declarations.enums.index(row_0)] = true;
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.enums.clone(rows_3.front())};
            context.storage.declarations.enums
                .values[context.storage.declarations.enums.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.enums
                        .values[context.storage.declarations.enums.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{context.storage.declarations.enumerators.clone(rows_1.front())};
                    context.storage.declarations.enumerators
                        .hidden[context.storage.declarations.enumerators.index(row_0)] = true;
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_enum_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .count[context.storage.declarations.enums.index(row_0)] =
                context.storage.strings.intern("Value");
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_enum_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .enum_array[context.storage.declarations.enums.index(row_0)] = true;
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .count[context.storage.declarations.enums.index(row_0)] =
                context.storage.strings.intern("Missing");
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_enum_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .enum_array[context.storage.declarations.enums.index(row_0)] = true;
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .values[context.storage.declarations.enums.index(row_0)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.enums
                        .values[context.storage.declarations.enums.index(row_0)])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.push_back([&] {
                    auto const new_enumerators{context.storage.declarations.enumerators.append()};
                    context.storage.declarations.enumerators
                        .name[context.storage.declarations.enumerators.index(new_enumerators)] =
                        context.storage.strings.intern("COUNT");
                    context.storage.declarations.enumerators
                        .initializer[context.storage.declarations.enumerators.index(
                            new_enumerators)] = std::nullopt;
                    context.storage.declarations.enumerators
                        .display_name[context.storage.declarations.enumerators.index(
                            new_enumerators)] = std::nullopt;
                    context.storage.declarations.enumerators
                        .hidden[context.storage.declarations.enumerators.index(new_enumerators)] =
                        true;
                    return new_enumerators;
                }());
                return context.storage.declarations.range(rows);
            }();
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .values[context.storage.declarations.enums.index(row_0)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.enums
                        .values[context.storage.declarations.enums.index(row_0)])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.push_back([&] {
                    auto const new_enumerators{context.storage.declarations.enumerators.append()};
                    context.storage.declarations.enumerators
                        .name[context.storage.declarations.enumerators.index(new_enumerators)] =
                        context.storage.strings.intern("After");
                    return new_enumerators;
                }());
                return context.storage.declarations.range(rows);
            }();
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .count[context.storage.declarations.enums.index(row_0)] =
                context.storage.strings.intern("COUNT");
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_enum_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .enum_array[context.storage.declarations.enums.index(row_0)] = true;
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .values[context.storage.declarations.enums.index(row_0)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.enums
                        .values[context.storage.declarations.enums.index(row_0)])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.push_back([&] {
                    auto const new_enumerators{context.storage.declarations.enumerators.append()};
                    context.storage.declarations.enumerators
                        .name[context.storage.declarations.enumerators.index(new_enumerators)] =
                        context.storage.strings.intern("COUNT");
                    return new_enumerators;
                }());
                return context.storage.declarations.range(rows);
            }();
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .count[context.storage.declarations.enums.index(row_0)] =
                context.storage.strings.intern("COUNT");
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_enum_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .enum_array[context.storage.declarations.enums.index(row_0)] = true;
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums.values[context.storage.declarations.enums.index(
                row_0)] = context.storage.declarations.range<lispb::codegen::EnumeratorId>({[&] {
                auto const new_enumerators{context.storage.declarations.enumerators.append()};
                context.storage.declarations.enumerators
                    .name[context.storage.declarations.enumerators.index(new_enumerators)] =
                    context.storage.strings.intern("COUNT");
                context.storage.declarations.enumerators
                    .initializer[context.storage.declarations.enumerators.index(new_enumerators)] =
                    std::nullopt;
                context.storage.declarations.enumerators
                    .display_name[context.storage.declarations.enumerators.index(new_enumerators)] =
                    std::nullopt;
                context.storage.declarations.enumerators
                    .hidden[context.storage.declarations.enumerators.index(new_enumerators)] = true;
                return new_enumerators;
            }()});
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .count[context.storage.declarations.enums.index(row_0)] =
                context.storage.strings.intern("COUNT");
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, SupportsSignedEnumValuesWhenTheyFitTheUnderlyingType) {
    CompilerTestContext context;
    auto module{valid_native_enum_module(context)};

    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .underlying_type[context.storage.declarations.enums.index(row_0)] = [&] {
                auto const new_type_refs{context.storage.declarations.type_refs.append()};
                context.storage.declarations.type_refs
                    .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                    context.storage.strings.intern("@native_int8");
                return new_type_refs;
            }();
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .values[context.storage.declarations.enums.index(row_0)] =
                context.storage.declarations.range<lispb::codegen::EnumeratorId>(
                    {[&] {
                         auto const new_enumerators{
                             context.storage.declarations.enumerators.append()};
                         context.storage.declarations.enumerators
                             .name[context.storage.declarations.enumerators.index(
                                 new_enumerators)] = context.storage.strings.intern("Minimum");
                         context.storage.declarations.enumerators
                             .initializer[context.storage.declarations.enumerators.index(
                                 new_enumerators)] = context.storage.strings.add("-128");
                         return new_enumerators;
                     }(),
                     [&] {
                         auto const new_enumerators{
                             context.storage.declarations.enumerators.append()};
                         context.storage.declarations.enumerators
                             .name[context.storage.declarations.enumerators.index(
                                 new_enumerators)] = context.storage.strings.intern("Maximum");
                         context.storage.declarations.enumerators
                             .initializer[context.storage.declarations.enumerators.index(
                                 new_enumerators)] = context.storage.strings.add("127");
                         return new_enumerators;
                     }()});
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .count[context.storage.declarations.enums.index(row_0)]
                .reset();
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    auto const types{std::vector<std::pair<std::string_view, CppTypeId>>{
        {"native_int8",
         context.storage.cpp_types.add(
             "std::int8_t", {context.storage.cpp_types.dependency("std::int8_t", "cstdint")})},
    }};

    EXPECT_NO_THROW(
        static_cast<void>(lower_test_manifest(context, manifest_with(context, module, types))));

    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.enums.clone(rows_3.front())};
            context.storage.declarations.enums
                .values[context.storage.declarations.enums.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.enums
                        .values[context.storage.declarations.enums.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.back() = [&] {
                    auto row_0{context.storage.declarations.enumerators.clone(rows_1.back())};
                    context.storage.declarations.enumerators
                        .initializer[context.storage.declarations.enumerators.index(row_0)] =
                        context.storage.strings.add("128");
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(
        static_cast<void>(lower_test_manifest(context, manifest_with(context, module, types))),
        std::invalid_argument);
}

TEST(Validation, RequiresKnownSemanticFactsWhenEnumBackingIsDerived) {
    CompilerTestContext context;
    auto module{valid_native_enum_module(context)};

    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .values[context.storage.declarations.enums.index(row_0)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.enums
                        .values[context.storage.declarations.enums.index(row_0)])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.push_back([&] {
                    auto const new_enumerators{context.storage.declarations.enumerators.append()};
                    context.storage.declarations.enumerators
                        .name[context.storage.declarations.enumerators.index(new_enumerators)] =
                        context.storage.strings.intern("Active");
                    return new_enumerators;
                }());
                return context.storage.declarations.range(rows);
            }();
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .underlying_type[context.storage.declarations.enums.index(row_0)]
                .reset();
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .bit_width[context.storage.declarations.enums.index(row_0)] = 12;
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .signedness[context.storage.declarations.enums.index(row_0)] =
                schema::EnumSignedness::unsigned_;
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();

    EXPECT_NO_THROW(
        static_cast<void>(lower_test_manifest(context, manifest_with(context, module))));

    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.enums.clone(rows_3.front())};
            context.storage.declarations.enums
                .values[context.storage.declarations.enums.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.enums
                        .values[context.storage.declarations.enums.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{context.storage.declarations.enumerators.clone(rows_1.front())};
                    context.storage.declarations.enumerators
                        .initializer[context.storage.declarations.enumerators.index(row_0)] =
                        context.storage.strings.add("calculate_state()");
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.enums.clone(rows_3.front())};
            context.storage.declarations.enums
                .values[context.storage.declarations.enums.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.enums
                        .values[context.storage.declarations.enums.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.back() = [&] {
                    auto row_0{context.storage.declarations.enumerators.clone(rows_1.back())};
                    context.storage.declarations.enumerators
                        .initializer[context.storage.declarations.enumerators.index(row_0)]
                        .reset();
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .bit_width[context.storage.declarations.enums.index(row_0)]
                .reset();
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.enums.clone(rows_1.front())};
            context.storage.declarations.enums
                .signedness[context.storage.declarations.enums.index(row_0)] =
                schema::EnumSignedness::unspecified;
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(static_cast<void>(lower_test_manifest(context, manifest_with(context, module))),
                 std::invalid_argument);
}

TEST(Validation, RejectsInvalidStaticTableModuleConfiguration) {
    CompilerTestContext context;
    auto module{valid_static_table_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = {};
    EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));

    module = valid_static_table_module(context);
    context.storage.declarations.normal_modules
        .settings[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto row_0{context.storage.declarations.module_settings_store.clone(
            context.storage.declarations.normal_modules
                .settings[context.storage.declarations.normal_modules.index(module)])};
        context.storage.declarations.module_settings_store
            .source[context.storage.declarations.module_settings_store.index(row_0)] =
            context.storage.strings.add("Tables.cpp");
        return row_0;
    }();
    EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));

    module = valid_static_table_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.static_tables.clone(rows_1.front())};
            context.storage.declarations.static_tables
                .rows[context.storage.declarations.static_tables.index(row_0)] = {};
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_static_table_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.static_tables.clone(rows_1.front())};
            context.storage.declarations.static_tables
                .columns[context.storage.declarations.static_tables.index(row_0)] = {};
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsInvalidStaticTableNamesAndTypes) {
    CompilerTestContext context;
    auto module{valid_static_table_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows{std::vector(original_items.begin(), original_items.end())};
        rows.push_back(
            context.storage.declarations
                .items(context.storage.declarations.normal_modules
                           .declarations[context.storage.declarations.normal_modules.index(module)])
                .front());
        return context.storage.declarations.range(rows);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_static_table_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.static_tables.clone(rows_1.front())};
            context.storage.declarations.static_tables
                .rows[context.storage.declarations.static_tables.index(row_0)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.static_tables
                        .rows[context.storage.declarations.static_tables.index(row_0)])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.push_back([&] {
                    auto const new_static_table_rows{
                        context.storage.declarations.static_table_rows.append()};
                    context.storage.declarations.static_table_rows
                        .name[context.storage.declarations.static_table_rows.index(
                            new_static_table_rows)] = context.storage.strings.intern("first");
                    return new_static_table_rows;
                }());
                return context.storage.declarations.range(rows);
            }();
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_static_table_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.static_tables.clone(rows_3.front())};
            context.storage.declarations.static_tables
                .rows[context.storage.declarations.static_tables.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.static_tables
                        .rows[context.storage.declarations.static_tables.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{
                        context.storage.declarations.static_table_rows.clone(rows_1.front())};
                    context.storage.declarations.static_table_rows
                        .name[context.storage.declarations.static_table_rows.index(row_0)] =
                        context.storage.strings.intern("bad-name");
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_static_table_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.static_tables.clone(rows_1.front())};
            context.storage.declarations.static_tables
                .columns[context.storage.declarations.static_tables.index(row_0)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.static_tables
                        .columns[context.storage.declarations.static_tables.index(row_0)])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.push_back(
                    context.storage.declarations
                        .items(
                            context.storage.declarations.static_tables
                                .columns[context.storage.declarations.static_tables.index(
                                    context.storage.declarations
                                        .items(context.storage.declarations.normal_modules
                                                   .declarations[context.storage.declarations
                                                                     .normal_modules.index(module)])
                                        .front())])
                        .front());
                return context.storage.declarations.range(rows);
            }();
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_static_table_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.static_tables.clone(rows_3.front())};
            context.storage.declarations.static_tables
                .columns[context.storage.declarations.static_tables.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.static_tables
                        .columns[context.storage.declarations.static_tables.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{
                        context.storage.declarations.static_table_columns.clone(rows_1.front())};
                    context.storage.declarations.static_table_columns
                        .type[context.storage.declarations.static_table_columns.index(row_0)] =
                        [&] {
                            auto const new_type_refs{
                                context.storage.declarations.type_refs.append()};
                            context.storage.declarations.type_refs
                                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                                context.storage.strings.intern("@missing");
                            return new_type_refs;
                        }();
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsStaticTableColumnsCollidingWithGeneratedApis) {
    CompilerTestContext context;
    for (auto const& name :
         {"FValues", "num_rows", "num", "apply_arrays", "apply_array_pairs", "first_index"}) {
        SCOPED_TRACE(name);
        auto module{valid_static_table_module(context)};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(module)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{context.storage.declarations.static_tables.clone(rows_3.front())};
                context.storage.declarations.static_tables
                    .columns[context.storage.declarations.static_tables.index(row_2)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.static_tables
                            .columns[context.storage.declarations.static_tables.index(row_2)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1.front() = [&] {
                        auto row_0{context.storage.declarations.static_table_columns.clone(
                            rows_1.front())};
                        context.storage.declarations.static_table_columns
                            .name[context.storage.declarations.static_table_columns.index(row_0)] =
                            context.storage.strings.intern(name);
                        return row_0;
                    }();
                    return context.storage.declarations.range(rows_1);
                }();
                return row_2;
            }();
            return context.storage.declarations.range(rows_3);
        }();
        EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                     std::invalid_argument);
    }
}

TEST(Validation, RejectsInvalidStaticTableGroups) {
    CompilerTestContext context;
    auto module{valid_static_table_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.static_tables.clone(rows_1.front())};
            context.storage.declarations.static_tables
                .groups[context.storage.declarations.static_tables.index(row_0)] =
                context.storage.declarations.range<lispb::codegen::StaticTableGroupId>({[&] {
                    auto const new_static_table_groups{
                        context.storage.declarations.static_table_groups.append()};
                    context.storage.declarations.static_table_groups
                        .name[context.storage.declarations.static_table_groups.index(
                            new_static_table_groups)] = context.storage.strings.intern("point");
                    context.storage.declarations.static_table_groups
                        .type[context.storage.declarations.static_table_groups.index(
                            new_static_table_groups)] = [&] {
                        auto const new_type_refs{context.storage.declarations.type_refs.append()};
                        context.storage.declarations.type_refs
                            .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                            context.storage.strings.intern("FPoint");
                        return new_type_refs;
                    }();
                    context.storage.declarations.static_table_groups
                        .columns[context.storage.declarations.static_table_groups.index(
                            new_static_table_groups)] =
                        context.storage.declarations.range<lispb::IdentifierId>(
                            {context.storage.strings.intern("ids")});
                    return new_static_table_groups;
                }()});
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();

    auto duplicate_group{module};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(duplicate_group)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(duplicate_group)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.static_tables.clone(rows_1.front())};
            context.storage.declarations.static_tables
                .groups[context.storage.declarations.static_tables.index(row_0)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.static_tables
                        .groups[context.storage.declarations.static_tables.index(row_0)])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.push_back(
                    context.storage.declarations
                        .items(context.storage.declarations.static_tables
                                   .groups[context.storage.declarations.static_tables.index(
                                       context.storage.declarations
                                           .items(context.storage.declarations.normal_modules
                                                      .declarations[context.storage.declarations
                                                                        .normal_modules.index(
                                                                            duplicate_group)])
                                           .front())])
                        .front());
                return context.storage.declarations.range(rows);
            }();
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, duplicate_group)),
                 std::invalid_argument);

    auto invalid_name{module};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(invalid_name)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(invalid_name)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.static_tables.clone(rows_3.front())};
            context.storage.declarations.static_tables
                .groups[context.storage.declarations.static_tables.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.static_tables
                        .groups[context.storage.declarations.static_tables.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{
                        context.storage.declarations.static_table_groups.clone(rows_1.front())};
                    context.storage.declarations.static_table_groups
                        .name[context.storage.declarations.static_table_groups.index(row_0)] =
                        context.storage.strings.intern("bad-name");
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, invalid_name)),
                 std::invalid_argument);

    auto unknown_type{module};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(unknown_type)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(unknown_type)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.static_tables.clone(rows_3.front())};
            context.storage.declarations.static_tables
                .groups[context.storage.declarations.static_tables.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.static_tables
                        .groups[context.storage.declarations.static_tables.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{
                        context.storage.declarations.static_table_groups.clone(rows_1.front())};
                    context.storage.declarations.static_table_groups
                        .type[context.storage.declarations.static_table_groups.index(row_0)] = [&] {
                        auto const new_type_refs{context.storage.declarations.type_refs.append()};
                        context.storage.declarations.type_refs
                            .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                            context.storage.strings.intern("@missing");
                        return new_type_refs;
                    }();
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, unknown_type)),
                 std::invalid_argument);

    auto empty_columns{module};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(empty_columns)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(empty_columns)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.static_tables.clone(rows_3.front())};
            context.storage.declarations.static_tables
                .groups[context.storage.declarations.static_tables.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.static_tables
                        .groups[context.storage.declarations.static_tables.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{
                        context.storage.declarations.static_table_groups.clone(rows_1.front())};
                    context.storage.declarations.static_table_groups
                        .columns[context.storage.declarations.static_table_groups.index(row_0)] =
                        {};
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, empty_columns)),
                 std::invalid_argument);

    auto unknown_column{module};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(unknown_column)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(unknown_column)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.static_tables.clone(rows_3.front())};
            context.storage.declarations.static_tables
                .groups[context.storage.declarations.static_tables.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.static_tables
                        .groups[context.storage.declarations.static_tables.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{
                        context.storage.declarations.static_table_groups.clone(rows_1.front())};
                    context.storage.declarations.static_table_groups
                        .columns[context.storage.declarations.static_table_groups.index(row_0)] =
                        context.storage.declarations.range<lispb::IdentifierId>(
                            {context.storage.strings.intern("missing")});
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, unknown_column)),
                 std::invalid_argument);

    auto duplicate_column{module};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(duplicate_column)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(
                    duplicate_column)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.static_tables.clone(rows_3.front())};
            context.storage.declarations.static_tables
                .groups[context.storage.declarations.static_tables.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.static_tables
                        .groups[context.storage.declarations.static_tables.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{
                        context.storage.declarations.static_table_groups.clone(rows_1.front())};
                    context.storage.declarations.static_table_groups
                        .columns[context.storage.declarations.static_table_groups.index(row_0)] =
                        context.storage.declarations.range<lispb::IdentifierId>(
                            {context.storage.strings.intern("ids"),
                             context.storage.strings.intern("ids")});
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, duplicate_column)),
                 std::invalid_argument);
}

TEST(Validation, RejectsStaticTableGroupGetterCollisions) {
    CompilerTestContext context;
    auto module{valid_static_table_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.static_tables.clone(rows_1.front())};
            context.storage.declarations.static_tables
                .columns[context.storage.declarations.static_tables.index(row_0)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.static_tables
                        .columns[context.storage.declarations.static_tables.index(row_0)])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.push_back([&] {
                    auto const new_static_table_columns{
                        context.storage.declarations.static_table_columns.append()};
                    context.storage.declarations.static_table_columns
                        .name[context.storage.declarations.static_table_columns.index(
                            new_static_table_columns)] =
                        context.storage.strings.intern("get_point");
                    context.storage.declarations.static_table_columns
                        .type[context.storage.declarations.static_table_columns.index(
                            new_static_table_columns)] = [&] {
                        auto const new_type_refs{context.storage.declarations.type_refs.append()};
                        context.storage.declarations.type_refs
                            .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                            context.storage.strings.intern("float");
                        return new_type_refs;
                    }();
                    return new_static_table_columns;
                }());
                return context.storage.declarations.range(rows);
            }();
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.static_tables.clone(rows_1.front())};
            context.storage.declarations.static_tables
                .groups[context.storage.declarations.static_tables.index(row_0)] =
                context.storage.declarations.range<lispb::codegen::StaticTableGroupId>({[&] {
                    auto const new_static_table_groups{
                        context.storage.declarations.static_table_groups.append()};
                    context.storage.declarations.static_table_groups
                        .name[context.storage.declarations.static_table_groups.index(
                            new_static_table_groups)] = context.storage.strings.intern("point");
                    context.storage.declarations.static_table_groups
                        .type[context.storage.declarations.static_table_groups.index(
                            new_static_table_groups)] = [&] {
                        auto const new_type_refs{context.storage.declarations.type_refs.append()};
                        context.storage.declarations.type_refs
                            .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                            context.storage.strings.intern("FPoint");
                        return new_type_refs;
                    }();
                    context.storage.declarations.static_table_groups
                        .columns[context.storage.declarations.static_table_groups.index(
                            new_static_table_groups)] =
                        context.storage.declarations.range<lispb::IdentifierId>(
                            {context.storage.strings.intern("ids")});
                    return new_static_table_groups;
                }()});
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsEmptyModuleOutputs) {
    CompilerTestContext context;
    auto module{valid_soa_module(context)};
    context.storage.declarations.normal_modules
        .settings[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto row_0{context.storage.declarations.module_settings_store.clone(
            context.storage.declarations.normal_modules
                .settings[context.storage.declarations.normal_modules.index(module)])};
        context.storage.declarations.module_settings_store
            .header[context.storage.declarations.module_settings_store.index(row_0)] = {};
        return row_0;
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsEmptyOptionalModuleSettings) {
    CompilerTestContext context;
    auto module{valid_soa_module(context)};
    context.storage.declarations.normal_modules
        .settings[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto row_0{context.storage.declarations.module_settings_store.clone(
            context.storage.declarations.normal_modules
                .settings[context.storage.declarations.normal_modules.index(module)])};
        context.storage.declarations.module_settings_store
            .header_include[context.storage.declarations.module_settings_store.index(row_0)] =
            context.storage.strings.add("");
        return row_0;
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_soa_module(context);
    context.storage.declarations.normal_modules
        .settings[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto row_0{context.storage.declarations.module_settings_store.clone(
            context.storage.declarations.normal_modules
                .settings[context.storage.declarations.normal_modules.index(module)])};
        context.storage.declarations.module_settings_store
            .namespace_name[context.storage.declarations.module_settings_store.index(row_0)] =
            context.storage.strings.intern("");
        return row_0;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsDuplicateAndEmptyIncludeOrderPrefixes) {
    CompilerTestContext context;
    auto module{valid_soa_module(context)};
    context.storage.declarations.normal_modules
        .settings[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto row_0{context.storage.declarations.module_settings_store.clone(
            context.storage.declarations.normal_modules
                .settings[context.storage.declarations.normal_modules.index(module)])};
        context.storage.declarations.module_settings_store
            .include_order[context.storage.declarations.module_settings_store.index(row_0)] =
            context.storage.declarations.range<lispb::StringId>(
                {context.storage.strings.add("Project/"), context.storage.strings.add("Project/")});
        return row_0;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_soa_module(context);
    context.storage.declarations.normal_modules
        .settings[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto row_0{context.storage.declarations.module_settings_store.clone(
            context.storage.declarations.normal_modules
                .settings[context.storage.declarations.normal_modules.index(module)])};
        context.storage.declarations.module_settings_store
            .include_order[context.storage.declarations.module_settings_store.index(row_0)] =
            context.storage.declarations.range<lispb::StringId>({context.storage.strings.add("")});
        return row_0;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsMalformedQualifiedNamespaces) {
    CompilerTestContext context;
    auto module{valid_soa_module(context)};
    context.storage.declarations.normal_modules
        .settings[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto row_0{context.storage.declarations.module_settings_store.clone(
            context.storage.declarations.normal_modules
                .settings[context.storage.declarations.normal_modules.index(module)])};
        context.storage.declarations.module_settings_store
            .namespace_name[context.storage.declarations.module_settings_store.index(row_0)] =
            context.storage.strings.intern("project:::invalid");
        return row_0;
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsDuplicateModuleNames) {
    CompilerTestContext context;
    auto first{valid_soa_module(context)};
    auto second{valid_soa_module(context)};
    context.storage.declarations.normal_modules
        .settings[context.storage.declarations.normal_modules.index(second)] = [&] {
        auto row_0{context.storage.declarations.module_settings_store.clone(
            context.storage.declarations.normal_modules
                .settings[context.storage.declarations.normal_modules.index(second)])};
        context.storage.declarations.module_settings_store
            .header[context.storage.declarations.module_settings_store.index(row_0)] =
            context.storage.strings.add("Other.h");
        return row_0;
    }();
    context.storage.declarations.normal_modules
        .settings[context.storage.declarations.normal_modules.index(second)] = [&] {
        auto row_0{context.storage.declarations.module_settings_store.clone(
            context.storage.declarations.normal_modules
                .settings[context.storage.declarations.normal_modules.index(second)])};
        context.storage.declarations.module_settings_store
            .source[context.storage.declarations.module_settings_store.index(row_0)] =
            context.storage.strings.add("Other.cpp");
        return row_0;
    }();
    lispb::codegen::Manifest const manifest{manifest_schema_version,
                                            make_test_registry(context),
                                            context.storage.declarations,
                                            {first, second}};

    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);
}

TEST(Validation, RejectsUnsupportedProgrammaticSchemaVersions) {
    CompilerTestContext context;
    auto manifest{manifest_with(context, valid_soa_module(context))};
    manifest.schema_version = manifest_schema_version + 1;

    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);
}

TEST(Validation, AllowsManifestsWithoutModules) {
    CompilerTestContext context;
    lispb::codegen::Manifest const manifest{
        manifest_schema_version, make_test_registry(context), context.storage.declarations, {}};

    EXPECT_NO_THROW(lower_test_manifest(context, manifest));
}

TEST(Validation, RejectsKeywordsAndReservedIdentifiers) {
    CompilerTestContext context;
    for (auto const& invalid : {"class", "__generated", "_Reserved"}) {
        SCOPED_TRACE(invalid);
        auto module{valid_soa_module(context)};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(module)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{context.storage.declarations.soas.clone(rows_3.front())};
                context.storage.declarations.soas
                    .members[context.storage.declarations.soas.index(row_2)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.soas
                            .members[context.storage.declarations.soas.index(row_2)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1.front() = [&] {
                        auto row_0{context.storage.declarations.soa_members.clone(rows_1.front())};
                        context.storage.declarations.soa_members
                            .name[context.storage.declarations.soa_members.index(row_0)] =
                            context.storage.strings.intern(invalid);
                        return row_0;
                    }();
                    return context.storage.declarations.range(rows_1);
                }();
                return row_2;
            }();
            return context.storage.declarations.range(rows_3);
        }();
        EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                     std::invalid_argument);
    }

    auto module{valid_soa_module(context)};
    context.storage.declarations.normal_modules
        .settings[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto row_0{context.storage.declarations.module_settings_store.clone(
            context.storage.declarations.normal_modules
                .settings[context.storage.declarations.normal_modules.index(module)])};
        context.storage.declarations.module_settings_store
            .namespace_name[context.storage.declarations.module_settings_store.index(row_0)] =
            context.storage.strings.intern("project::namespace");
        return row_0;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsEmptyTypeSpellings) {
    CompilerTestContext context;
    auto module{valid_soa_module(context)};
    EXPECT_THROW(
        lower_test_manifest(
            context,
            manifest_with(context, module, {{"empty", context.storage.cpp_types.add("")}})),
        std::invalid_argument);
}

TEST(Validation, RejectsInvalidTypeDependencyAndOperationSpellings) {
    CompilerTestContext context;
    auto empty_header{context.storage.cpp_types.add(
        "FValue", {context.storage.cpp_types.dependency("FValue", "")})};
    EXPECT_THROW(
        lower_test_manifest(
            context, manifest_with(context, valid_soa_module(context), {{"value", empty_header}})),
        std::invalid_argument);

    auto const invalid_operation{context.storage.cpp_types.add("FValue")};
    context.storage.cpp_types.set_operation(
        invalid_operation, TypeOperation::remove_at_swap, "bad-name");
    EXPECT_THROW(
        lower_test_manifest(
            context,
            manifest_with(context, valid_soa_module(context), {{"value", invalid_operation}})),
        std::invalid_argument);
}

TEST(Validation, RejectsEmptySoaNames) {
    CompilerTestContext context;
    auto module{valid_soa_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.soas.clone(rows_1.front())};
            context.storage.declarations.soas
                .name[context.storage.declarations.soas.index(row_0)] = {};
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsDuplicateSoaMembers) {
    CompilerTestContext context;
    auto module{valid_soa_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.soas.clone(rows_1.front())};
            context.storage.declarations.soas
                .members[context.storage.declarations.soas.index(row_0)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.soas
                        .members[context.storage.declarations.soas.index(row_0)])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.push_back([&] {
                    auto const new_soa_members{context.storage.declarations.soa_members.append()};
                    context.storage.declarations.soa_members
                        .name[context.storage.declarations.soa_members.index(new_soa_members)] =
                        context.storage.strings.intern("values");
                    context.storage.declarations.soa_members
                        .kind[context.storage.declarations.soa_members.index(new_soa_members)] =
                        SoaMemberKind::array;
                    context.storage.declarations.soa_members
                        .type[context.storage.declarations.soa_members.index(new_soa_members)] =
                        [&] {
                            auto const new_type_refs{
                                context.storage.declarations.type_refs.append()};
                            context.storage.declarations.type_refs
                                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                                context.storage.strings.intern("float");
                            return new_type_refs;
                        }();
                    return new_soa_members;
                }());
                return context.storage.declarations.range(rows);
            }();
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsMalformedSoaMemberIdentifiers) {
    CompilerTestContext context;
    auto module{valid_soa_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.soas.clone(rows_3.front())};
            context.storage.declarations.soas
                .members[context.storage.declarations.soas.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.soas
                        .members[context.storage.declarations.soas.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{context.storage.declarations.soa_members.clone(rows_1.front())};
                    context.storage.declarations.soa_members
                        .name[context.storage.declarations.soa_members.index(row_0)] =
                        context.storage.strings.intern("bad-name");
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsIncompleteSoaFieldMaskDeclarations) {
    CompilerTestContext context;
    auto module{valid_mask_soa_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.soas.clone(rows_1.front())};
            context.storage.declarations.soas
                .field_enum_name[context.storage.declarations.soas.index(row_0)]
                .reset();
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_mask_soa_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.soas.clone(rows_1.front())};
            context.storage.declarations.soas
                .field_mask_name[context.storage.declarations.soas.index(row_0)]
                .reset();
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsInvalidSoaFieldMaskMembers) {
    CompilerTestContext context;
    auto module{valid_mask_soa_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.soas.clone(rows_3.front())};
            context.storage.declarations.soas
                .members[context.storage.declarations.soas.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.soas
                        .members[context.storage.declarations.soas.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{context.storage.declarations.soa_members.clone(rows_1.front())};
                    context.storage.declarations.soa_members
                        .type[context.storage.declarations.soa_members.index(row_0)] = [&] {
                        auto const new_type_refs{context.storage.declarations.type_refs.append()};
                        context.storage.declarations.type_refs
                            .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                            context.storage.strings.intern("uint8");
                        return new_type_refs;
                    }();
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_mask_soa_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.soas.clone(rows_3.front())};
            context.storage.declarations.soas
                .members[context.storage.declarations.soas.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.soas
                        .members[context.storage.declarations.soas.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.back() = [&] {
                    auto row_0{context.storage.declarations.soa_members.clone(rows_1.back())};
                    context.storage.declarations.soa_members
                        .mask_field[context.storage.declarations.soa_members.index(row_0)] = false;
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_soa_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.soas.clone(rows_3.front())};
            context.storage.declarations.soas
                .members[context.storage.declarations.soas.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.soas
                        .members[context.storage.declarations.soas.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{context.storage.declarations.soa_members.clone(rows_1.front())};
                    context.storage.declarations.soa_members
                        .mask_field[context.storage.declarations.soa_members.index(row_0)] = true;
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsInvalidSoaMaskDimensions) {
    CompilerTestContext context;
    auto module{valid_mask_soa_module(context)};

    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.soas.clone(rows_3.front())};
            context.storage.declarations.soas
                .members[context.storage.declarations.soas.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.soas
                        .members[context.storage.declarations.soas.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.back() = [&] {
                    auto row_0{context.storage.declarations.soa_members.clone(rows_1.back())};
                    context.storage.declarations.soa_members
                        .mask_dimensions[context.storage.declarations.soa_members.index(row_0)] =
                        [&] {
                            auto const original_items{context.storage.declarations.items(
                                context.storage.declarations.soa_members
                                    .mask_dimensions[context.storage.declarations.soa_members.index(
                                        row_0)])};
                            auto rows{std::vector(original_items.begin(), original_items.end())};
                            rows.push_back([&] {
                                auto const new_soa_mask_dimensions{
                                    context.storage.declarations.soa_mask_dimensions.append()};
                                context.storage.declarations.soa_mask_dimensions
                                    .index_name[context.storage.declarations.soa_mask_dimensions
                                                    .index(new_soa_mask_dimensions)] =
                                    context.storage.strings.intern("field_index");
                                context.storage.declarations.soa_mask_dimensions
                                    .extent[context.storage.declarations.soa_mask_dimensions.index(
                                        new_soa_mask_dimensions)] =
                                    context.storage.strings.add("4");
                                return new_soa_mask_dimensions;
                            }());
                            return context.storage.declarations.range(rows);
                        }();
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.soas.clone(rows_3.front())};
            context.storage.declarations.soas
                .members[context.storage.declarations.soas.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.soas
                        .members[context.storage.declarations.soas.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.back() = [&] {
                    auto row_0{context.storage.declarations.soa_members.clone(rows_1.back())};
                    context.storage.declarations.soa_members
                        .mask_dimensions[context.storage.declarations.soa_members.index(row_0)] =
                        [&] {
                            auto const original_items{context.storage.declarations.items(
                                context.storage.declarations.soa_members
                                    .mask_dimensions[context.storage.declarations.soa_members.index(
                                        row_0)])};
                            auto rows{std::vector(original_items.begin(), original_items.end())};
                            rows.push_back([&] {
                                auto const new_soa_mask_dimensions{
                                    context.storage.declarations.soa_mask_dimensions.append()};
                                context.storage.declarations.soa_mask_dimensions
                                    .index_name[context.storage.declarations.soa_mask_dimensions
                                                    .index(new_soa_mask_dimensions)] =
                                    context.storage.strings.intern("field_index");
                                context.storage.declarations.soa_mask_dimensions
                                    .extent[context.storage.declarations.soa_mask_dimensions.index(
                                        new_soa_mask_dimensions)] =
                                    context.storage.strings.add("2");
                                return new_soa_mask_dimensions;
                            }());
                            return context.storage.declarations.range(rows);
                        }();
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_mask_soa_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.soas.clone(rows_3.front())};
            context.storage.declarations.soas
                .members[context.storage.declarations.soas.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.soas
                        .members[context.storage.declarations.soas.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.back() = [&] {
                    auto row_0{context.storage.declarations.soa_members.clone(rows_1.back())};
                    context.storage.declarations.soa_members
                        .mask_dimensions[context.storage.declarations.soa_members.index(row_0)] =
                        [&] {
                            auto const original_items{context.storage.declarations.items(
                                context.storage.declarations.soa_members
                                    .mask_dimensions[context.storage.declarations.soa_members.index(
                                        row_0)])};
                            auto rows{std::vector(original_items.begin(), original_items.end())};
                            rows.push_back([&] {
                                auto const new_soa_mask_dimensions{
                                    context.storage.declarations.soa_mask_dimensions.append()};
                                context.storage.declarations.soa_mask_dimensions
                                    .index_name[context.storage.declarations.soa_mask_dimensions
                                                    .index(new_soa_mask_dimensions)] =
                                    context.storage.strings.intern("bad-index");
                                context.storage.declarations.soa_mask_dimensions
                                    .extent[context.storage.declarations.soa_mask_dimensions.index(
                                        new_soa_mask_dimensions)] =
                                    context.storage.strings.add("4");
                                return new_soa_mask_dimensions;
                            }());
                            return context.storage.declarations.range(rows);
                        }();
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsMembersCollidingWithGeneratedStorageApis) {
    CompilerTestContext context;
    for (auto const& name : {"num", "reset", "copy_element"}) {
        SCOPED_TRACE(name);
        auto module{valid_soa_module(context)};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(module)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{context.storage.declarations.soas.clone(rows_3.front())};
                context.storage.declarations.soas
                    .members[context.storage.declarations.soas.index(row_2)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.soas
                            .members[context.storage.declarations.soas.index(row_2)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1.front() = [&] {
                        auto row_0{context.storage.declarations.soa_members.clone(rows_1.front())};
                        context.storage.declarations.soa_members
                            .name[context.storage.declarations.soa_members.index(row_0)] =
                            context.storage.strings.intern(name);
                        return row_0;
                    }();
                    return context.storage.declarations.range(rows_1);
                }();
                return row_2;
            }();
            return context.storage.declarations.range(rows_3);
        }();
        EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                     std::invalid_argument);
    }

    auto homogeneous{valid_homogeneous_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(homogeneous)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(homogeneous)])};
        auto rows_2{std::vector(original_items.begin(), original_items.end())};
        rows_2.front() = [&] {
            auto row_1{context.storage.declarations.homogeneous_layouts.clone(rows_2.front())};
            context.storage.declarations.homogeneous_layouts
                .components[context.storage.declarations.homogeneous_layouts.index(row_1)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.homogeneous_layouts
                        .components[context.storage.declarations.homogeneous_layouts.index(
                            row_1)])};
                auto rows_0{std::vector(original_items.begin(), original_items.end())};
                rows_0.front() = context.storage.strings.intern("add");
                return context.storage.declarations.range(rows_0);
            }();
            return row_1;
        }();
        return context.storage.declarations.range(rows_2);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, homogeneous)),
                 std::invalid_argument);

    auto vector{valid_vector_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(vector)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(vector)])};
        auto rows_2{std::vector(original_items.begin(), original_items.end())};
        rows_2.front() = [&] {
            auto row_1{context.storage.declarations.vector_soas.clone(rows_2.front())};
            context.storage.declarations.vector_soas
                .components[context.storage.declarations.vector_soas.index(row_1)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.vector_soas
                        .components[context.storage.declarations.vector_soas.index(row_1)])};
                auto rows_0{std::vector(original_items.begin(), original_items.end())};
                rows_0.front() = context.storage.strings.intern("get_view");
                return context.storage.declarations.range(rows_0);
            }();
            return row_1;
        }();
        return context.storage.declarations.range(rows_2);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, vector)),
                 std::invalid_argument);
}

TEST(Validation, RejectsUnknownSoaTypeReferences) {
    CompilerTestContext context;
    auto module{valid_soa_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.soas.clone(rows_3.front())};
            context.storage.declarations.soas
                .members[context.storage.declarations.soas.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.soas
                        .members[context.storage.declarations.soas.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{context.storage.declarations.soa_members.clone(rows_1.front())};
                    context.storage.declarations.soa_members
                        .type[context.storage.declarations.soa_members.index(row_0)] = [&] {
                        auto const new_type_refs{context.storage.declarations.type_refs.append()};
                        context.storage.declarations.type_refs
                            .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                            context.storage.strings.intern("@missing");
                        return new_type_refs;
                    }();
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsFixedSchemasOnArrayMembers) {
    CompilerTestContext context;
    auto module{valid_soa_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.soas.clone(rows_3.front())};
            context.storage.declarations.soas
                .members[context.storage.declarations.soas.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.soas
                        .members[context.storage.declarations.soas.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{context.storage.declarations.soa_members.clone(rows_1.front())};
                    context.storage.declarations.soa_members
                        .fixed_schema[context.storage.declarations.soa_members.index(row_0)] =
                        context.storage.strings.intern("FChild");
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsOpaqueNestedMembersInFixedSoas) {
    CompilerTestContext context;
    auto module{valid_soa_module(context)};

    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.soas.clone(rows_1.front())};
            context.storage.declarations.soas
                .fixed[context.storage.declarations.soas.index(row_0)] = [&] {
                auto const new_fixed_soas{context.storage.declarations.fixed_soas.append()};
                context.storage.declarations.fixed_soas
                    .storage_name[context.storage.declarations.fixed_soas.index(new_fixed_soas)] =
                    context.storage.strings.intern("TDataStorage");
                context.storage.declarations.fixed_soas
                    .containers[context.storage.declarations.fixed_soas.index(new_fixed_soas)] = {};
                return new_fixed_soas;
            }();
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.soas.clone(rows_1.front())};
            context.storage.declarations.soas.members[context.storage.declarations.soas.index(
                row_0)] = context.storage.declarations.range<lispb::codegen::SoaMemberId>({[&] {
                auto const new_soa_members{context.storage.declarations.soa_members.append()};
                context.storage.declarations.soa_members
                    .name[context.storage.declarations.soa_members.index(new_soa_members)] =
                    context.storage.strings.intern("child");
                context.storage.declarations.soa_members
                    .kind[context.storage.declarations.soa_members.index(new_soa_members)] =
                    SoaMemberKind::nested;
                context.storage.declarations.soa_members
                    .type[context.storage.declarations.soa_members.index(new_soa_members)] = [&] {
                    auto const new_type_refs{context.storage.declarations.type_refs.append()};
                    context.storage.declarations.type_refs
                        .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                        context.storage.strings.intern("FChild");
                    return new_type_refs;
                }();
                return new_soa_members;
            }()});
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsUnknownNestedFixedSchemas) {
    CompilerTestContext context;
    auto module{valid_soa_module(context)};

    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.soas.clone(rows_1.front())};
            context.storage.declarations.soas
                .fixed[context.storage.declarations.soas.index(row_0)] = [&] {
                auto const new_fixed_soas{context.storage.declarations.fixed_soas.append()};
                context.storage.declarations.fixed_soas
                    .storage_name[context.storage.declarations.fixed_soas.index(new_fixed_soas)] =
                    context.storage.strings.intern("TDataStorage");
                context.storage.declarations.fixed_soas
                    .containers[context.storage.declarations.fixed_soas.index(new_fixed_soas)] = {};
                return new_fixed_soas;
            }();
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.soas.clone(rows_1.front())};
            context.storage.declarations.soas.members[context.storage.declarations.soas.index(
                row_0)] = context.storage.declarations.range<lispb::codegen::SoaMemberId>({[&] {
                auto const new_soa_members{context.storage.declarations.soa_members.append()};
                context.storage.declarations.soa_members
                    .name[context.storage.declarations.soa_members.index(new_soa_members)] =
                    context.storage.strings.intern("child");
                context.storage.declarations.soa_members
                    .kind[context.storage.declarations.soa_members.index(new_soa_members)] =
                    SoaMemberKind::nested;
                context.storage.declarations.soa_members
                    .type[context.storage.declarations.soa_members.index(new_soa_members)] = [&] {
                    auto const new_type_refs{context.storage.declarations.type_refs.append()};
                    context.storage.declarations.type_refs
                        .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                        context.storage.strings.intern("FChild");
                    return new_type_refs;
                }();
                context.storage.declarations.soa_members
                    .fixed_schema[context.storage.declarations.soa_members.index(new_soa_members)] =
                    context.storage.strings.intern("FChild");
                return new_soa_members;
            }()});
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsFixedSchemaCycles) {
    CompilerTestContext context;
    auto module{valid_soa_module(context)};

    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.soas.clone(rows_1.front())};
            context.storage.declarations.soas
                .fixed[context.storage.declarations.soas.index(row_0)] = [&] {
                auto const new_fixed_soas{context.storage.declarations.fixed_soas.append()};
                context.storage.declarations.fixed_soas
                    .storage_name[context.storage.declarations.fixed_soas.index(new_fixed_soas)] =
                    context.storage.strings.intern("TDataStorage");
                context.storage.declarations.fixed_soas
                    .containers[context.storage.declarations.fixed_soas.index(new_fixed_soas)] = {};
                return new_fixed_soas;
            }();
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.soas.clone(rows_1.front())};
            context.storage.declarations.soas.members[context.storage.declarations.soas.index(
                row_0)] = context.storage.declarations.range<lispb::codegen::SoaMemberId>({[&] {
                auto const new_soa_members{context.storage.declarations.soa_members.append()};
                context.storage.declarations.soa_members
                    .name[context.storage.declarations.soa_members.index(new_soa_members)] =
                    context.storage.strings.intern("children");
                context.storage.declarations.soa_members
                    .kind[context.storage.declarations.soa_members.index(new_soa_members)] =
                    SoaMemberKind::nested;
                context.storage.declarations.soa_members
                    .type[context.storage.declarations.soa_members.index(new_soa_members)] = [&] {
                    auto const new_type_refs{context.storage.declarations.type_refs.append()};
                    context.storage.declarations.type_refs
                        .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                        context.storage.strings.intern("FData");
                    return new_type_refs;
                }();
                context.storage.declarations.soa_members
                    .fixed_schema[context.storage.declarations.soa_members.index(new_soa_members)] =
                    context.storage.strings.intern("FData");
                return new_soa_members;
            }()});
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsCollidingGeneratedSoaTypeNames) {
    CompilerTestContext context;
    auto module{valid_soa_module(context)};
    auto second{context.storage.declarations.soas.clone(
        context.storage.declarations
            .items(context.storage.declarations.normal_modules
                       .declarations[context.storage.declarations.normal_modules.index(module)])
            .front())};
    context.storage.declarations.soas.name[context.storage.declarations.soas.index(second)] =
        context.storage.strings.intern("FDataView");
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows{std::vector(original_items.begin(), original_items.end())};
        rows.push_back(second);
        return context.storage.declarations.range(rows);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsDuplicateFixedStorageAndContainerNames) {
    CompilerTestContext context;
    auto module{valid_soa_module(context)};

    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.soas.clone(rows_1.front())};
            context.storage.declarations.soas
                .fixed[context.storage.declarations.soas.index(row_0)] = [&] {
                auto const new_fixed_soas{context.storage.declarations.fixed_soas.append()};
                context.storage.declarations.fixed_soas
                    .storage_name[context.storage.declarations.fixed_soas.index(new_fixed_soas)] =
                    context.storage.strings.intern("TShared");
                context.storage.declarations.fixed_soas
                    .containers[context.storage.declarations.fixed_soas.index(new_fixed_soas)] =
                    context.storage.declarations.range<lispb::IdentifierId>(
                        {context.storage.strings.intern("TFixedData")});
                return new_fixed_soas;
            }();
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    auto second{context.storage.declarations.soas.clone(
        context.storage.declarations
            .items(context.storage.declarations.normal_modules
                       .declarations[context.storage.declarations.normal_modules.index(module)])
            .front())};
    context.storage.declarations.soas.name[context.storage.declarations.soas.index(second)] =
        context.storage.strings.intern("FOther");
    context.storage.declarations.soas.fixed[context.storage.declarations.soas.index(second)] = [&] {
        auto const new_fixed_soas{context.storage.declarations.fixed_soas.append()};
        context.storage.declarations.fixed_soas
            .storage_name[context.storage.declarations.fixed_soas.index(new_fixed_soas)] =
            context.storage.strings.intern("TShared");
        context.storage.declarations.fixed_soas
            .containers[context.storage.declarations.fixed_soas.index(new_fixed_soas)] =
            context.storage.declarations.range<lispb::IdentifierId>(
                {context.storage.strings.intern("TFixedOther")});
        return new_fixed_soas;
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows{std::vector(original_items.begin(), original_items.end())};
        rows.push_back(second);
        return context.storage.declarations.range(rows);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_soa_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.soas.clone(rows_1.front())};
            context.storage.declarations.soas
                .fixed[context.storage.declarations.soas.index(row_0)] = [&] {
                auto const new_fixed_soas{context.storage.declarations.fixed_soas.append()};
                context.storage.declarations.fixed_soas
                    .storage_name[context.storage.declarations.fixed_soas.index(new_fixed_soas)] =
                    context.storage.strings.intern("TShared");
                context.storage.declarations.fixed_soas
                    .containers[context.storage.declarations.fixed_soas.index(new_fixed_soas)] =
                    context.storage.declarations.range<lispb::IdentifierId>(
                        {context.storage.strings.intern("TShared")});
                return new_fixed_soas;
            }();
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsSoaModulesWithoutSourceOutput) {
    CompilerTestContext context;
    auto module{valid_soa_module(context)};
    context.storage.declarations.normal_modules
        .settings[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto row_0{context.storage.declarations.module_settings_store.clone(
            context.storage.declarations.normal_modules
                .settings[context.storage.declarations.normal_modules.index(module)])};
        context.storage.declarations.module_settings_store
            .source[context.storage.declarations.module_settings_store.index(row_0)]
            .reset();
        return row_0;
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsEmptyOptionalSoaViewNames) {
    CompilerTestContext context;
    auto module{valid_soa_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.soas.clone(rows_1.front())};
            context.storage.declarations.soas
                .view_name[context.storage.declarations.soas.index(row_0)] =
                context.storage.strings.intern("");
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsBlankSoaUsingDeclarations) {
    CompilerTestContext context;
    for (auto const* declaration : {"", " \t\r\n"}) {
        SCOPED_TRACE(declaration);
        auto module{valid_soa_module(context)};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(module)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{context.storage.declarations.soas.clone(rows_1.front())};
                context.storage.declarations.soas
                    .using_declarations[context.storage.declarations.soas.index(row_0)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.soas
                            .using_declarations[context.storage.declarations.soas.index(row_0)])};
                    auto rows{std::vector(original_items.begin(), original_items.end())};
                    rows.push_back(context.storage.strings.add(declaration));
                    return context.storage.declarations.range(rows);
                }();
                return row_0;
            }();
            return context.storage.declarations.range(rows_1);
        }();

        EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                     std::invalid_argument);
    }
}

TEST(Validation, RejectsDuplicateCustomSoaFunctionParameters) {
    CompilerTestContext context;
    auto module{valid_soa_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.soas.clone(rows_1.front())};
            context.storage.declarations.soas
                .functions[context.storage.declarations.soas.index(row_0)] =
                context.storage.declarations.range<lispb::codegen::FunctionSchemaId>({[&] {
                    auto const new_functions{context.storage.declarations.functions.append()};
                    context.storage.declarations.functions
                        .name[context.storage.declarations.functions.index(new_functions)] =
                        context.storage.strings.intern("set");
                    context.storage.declarations.functions
                        .return_type[context.storage.declarations.functions.index(new_functions)] =
                        [&] {
                            auto const new_type_refs{
                                context.storage.declarations.type_refs.append()};
                            context.storage.declarations.type_refs
                                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                                context.storage.strings.intern("void");
                            return new_type_refs;
                        }();
                    context.storage.declarations.functions
                        .parameters[context.storage.declarations.functions.index(new_functions)] =
                        context.storage.declarations.range<lispb::codegen::ParameterId>(
                            {[&] {
                                 auto const new_parameters{
                                     context.storage.declarations.parameters.append()};
                                 context.storage.declarations.parameters
                                     .type[context.storage.declarations.parameters.index(
                                         new_parameters)] = [&] {
                                     auto const new_type_refs{
                                         context.storage.declarations.type_refs.append()};
                                     context.storage.declarations.type_refs
                                         .name[context.storage.declarations.type_refs.index(
                                             new_type_refs)] =
                                         context.storage.strings.intern("int32");
                                     return new_type_refs;
                                 }();
                                 context.storage.declarations.parameters
                                     .name[context.storage.declarations.parameters.index(
                                         new_parameters)] = context.storage.strings.intern("value");
                                 return new_parameters;
                             }(),
                             [&] {
                                 auto const new_parameters{
                                     context.storage.declarations.parameters.append()};
                                 context.storage.declarations.parameters
                                     .type[context.storage.declarations.parameters.index(
                                         new_parameters)] = [&] {
                                     auto const new_type_refs{
                                         context.storage.declarations.type_refs.append()};
                                     context.storage.declarations.type_refs
                                         .name[context.storage.declarations.type_refs.index(
                                             new_type_refs)] =
                                         context.storage.strings.intern("float");
                                     return new_type_refs;
                                 }();
                                 context.storage.declarations.parameters
                                     .name[context.storage.declarations.parameters.index(
                                         new_parameters)] = context.storage.strings.intern("value");
                                 return new_parameters;
                             }()});
                    return new_functions;
                }()});
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsNonTrailingCustomSoaDefaultArguments) {
    CompilerTestContext context;
    auto module{valid_soa_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.soas.clone(rows_1.front())};
            context.storage.declarations.soas
                .functions[context.storage.declarations.soas.index(row_0)] =
                context.storage.declarations.range<lispb::codegen::FunctionSchemaId>({[&] {
                    auto const new_functions{context.storage.declarations.functions.append()};
                    context.storage.declarations.functions
                        .name[context.storage.declarations.functions.index(new_functions)] =
                        context.storage.strings.intern("set");
                    context.storage.declarations.functions
                        .return_type[context.storage.declarations.functions.index(new_functions)] =
                        [&] {
                            auto const new_type_refs{
                                context.storage.declarations.type_refs.append()};
                            context.storage.declarations.type_refs
                                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                                context.storage.strings.intern("void");
                            return new_type_refs;
                        }();
                    context.storage.declarations.functions
                        .parameters[context.storage.declarations.functions.index(new_functions)] =
                        context.storage.declarations.range<lispb::codegen::ParameterId>(
                            {[&] {
                                 auto const new_parameters{
                                     context.storage.declarations.parameters.append()};
                                 context.storage.declarations.parameters
                                     .type[context.storage.declarations.parameters.index(
                                         new_parameters)] = [&] {
                                     auto const new_type_refs{
                                         context.storage.declarations.type_refs.append()};
                                     context.storage.declarations.type_refs
                                         .name[context.storage.declarations.type_refs.index(
                                             new_type_refs)] =
                                         context.storage.strings.intern("int32");
                                     return new_type_refs;
                                 }();
                                 context.storage.declarations.parameters
                                     .name[context.storage.declarations.parameters.index(
                                         new_parameters)] = context.storage.strings.intern("first");
                                 context.storage.declarations.parameters
                                     .default_value[context.storage.declarations.parameters.index(
                                         new_parameters)] = context.storage.strings.add("0");
                                 return new_parameters;
                             }(),
                             [&] {
                                 auto const new_parameters{
                                     context.storage.declarations.parameters.append()};
                                 context.storage.declarations.parameters
                                     .type[context.storage.declarations.parameters.index(
                                         new_parameters)] = [&] {
                                     auto const new_type_refs{
                                         context.storage.declarations.type_refs.append()};
                                     context.storage.declarations.type_refs
                                         .name[context.storage.declarations.type_refs.index(
                                             new_type_refs)] =
                                         context.storage.strings.intern("int32");
                                     return new_type_refs;
                                 }();
                                 context.storage.declarations.parameters
                                     .name[context.storage.declarations.parameters.index(
                                         new_parameters)] =
                                     context.storage.strings.intern("second");
                                 return new_parameters;
                             }()});
                    return new_functions;
                }()});
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_soa_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.soas.clone(rows_1.front())};
            context.storage.declarations.soas
                .functions[context.storage.declarations.soas.index(row_0)] =
                context.storage.declarations.range<lispb::codegen::FunctionSchemaId>({[&] {
                    auto const new_functions{context.storage.declarations.functions.append()};
                    context.storage.declarations.functions
                        .name[context.storage.declarations.functions.index(new_functions)] =
                        context.storage.strings.intern("set");
                    context.storage.declarations.functions
                        .return_type[context.storage.declarations.functions.index(new_functions)] =
                        [&] {
                            auto const new_type_refs{
                                context.storage.declarations.type_refs.append()};
                            context.storage.declarations.type_refs
                                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                                context.storage.strings.intern("void");
                            return new_type_refs;
                        }();
                    context.storage.declarations.functions
                        .parameters[context.storage.declarations.functions.index(new_functions)] =
                        context.storage.declarations.range<lispb::codegen::ParameterId>({[&] {
                            auto const new_parameters{
                                context.storage.declarations.parameters.append()};
                            context.storage.declarations.parameters
                                .type[context.storage.declarations.parameters.index(
                                    new_parameters)] = [&] {
                                auto const new_type_refs{
                                    context.storage.declarations.type_refs.append()};
                                context.storage.declarations.type_refs
                                    .name[context.storage.declarations.type_refs.index(
                                        new_type_refs)] = context.storage.strings.intern("int32");
                                return new_type_refs;
                            }();
                            context.storage.declarations.parameters
                                .name[context.storage.declarations.parameters.index(
                                    new_parameters)] = context.storage.strings.intern("value");
                            context.storage.declarations.parameters
                                .default_value[context.storage.declarations.parameters.index(
                                    new_parameters)] = context.storage.strings.add(" \t");
                            return new_parameters;
                        }()});
                    return new_functions;
                }()});
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsUnknownCustomSoaDependencies) {
    CompilerTestContext context;
    auto module{valid_soa_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.soas.clone(rows_1.front())};
            context.storage.declarations.soas
                .functions[context.storage.declarations.soas.index(row_0)] =
                context.storage.declarations.range<lispb::codegen::FunctionSchemaId>({[&] {
                    auto const new_functions{context.storage.declarations.functions.append()};
                    context.storage.declarations.functions
                        .name[context.storage.declarations.functions.index(new_functions)] =
                        context.storage.strings.intern("reset_values");
                    context.storage.declarations.functions
                        .return_type[context.storage.declarations.functions.index(new_functions)] =
                        [&] {
                            auto const new_type_refs{
                                context.storage.declarations.type_refs.append()};
                            context.storage.declarations.type_refs
                                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                                context.storage.strings.intern("void");
                            return new_type_refs;
                        }();
                    context.storage.declarations.functions
                        .dependencies[context.storage.declarations.functions.index(new_functions)] =
                        context.storage.declarations.range<lispb::IdentifierId>(
                            {context.storage.strings.intern("missing")});
                    return new_functions;
                }()});
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_soa_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.soas.clone(rows_1.front())};
            context.storage.declarations.soas
                .functions[context.storage.declarations.soas.index(row_0)] =
                context.storage.declarations.range<lispb::codegen::FunctionSchemaId>({[&] {
                    auto const new_functions{context.storage.declarations.functions.append()};
                    context.storage.declarations.functions
                        .name[context.storage.declarations.functions.index(new_functions)] =
                        context.storage.strings.intern("reset_values");
                    context.storage.declarations.functions
                        .return_type[context.storage.declarations.functions.index(new_functions)] =
                        [&] {
                            auto const new_type_refs{
                                context.storage.declarations.type_refs.append()};
                            context.storage.declarations.type_refs
                                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                                context.storage.strings.intern("void");
                            return new_type_refs;
                        }();
                    context.storage.declarations.functions
                        .dependencies[context.storage.declarations.functions.index(new_functions)] =
                        context.storage.declarations.range<lispb::IdentifierId>(
                            {context.storage.strings.intern(" \t")});
                    return new_functions;
                }()});
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsInvalidCustomSoaFunctionDefinitions) {
    CompilerTestContext context;
    auto module{valid_soa_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.soas.clone(rows_1.front())};
            context.storage.declarations.soas
                .functions[context.storage.declarations.soas.index(row_0)] =
                context.storage.declarations.range<lispb::codegen::FunctionSchemaId>({[&] {
                    auto const new_functions{context.storage.declarations.functions.append()};
                    context.storage.declarations.functions
                        .name[context.storage.declarations.functions.index(new_functions)] =
                        context.storage.strings.intern("class");
                    context.storage.declarations.functions
                        .return_type[context.storage.declarations.functions.index(new_functions)] =
                        [&] {
                            auto const new_type_refs{
                                context.storage.declarations.type_refs.append()};
                            context.storage.declarations.type_refs
                                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                                context.storage.strings.intern("void");
                            return new_type_refs;
                        }();
                    context.storage.declarations.functions
                        .is_inline[context.storage.declarations.functions.index(new_functions)] =
                        true;
                    return new_functions;
                }()});
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_soa_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.soas.clone(rows_1.front())};
            context.storage.declarations.soas
                .functions[context.storage.declarations.soas.index(row_0)] =
                context.storage.declarations.range<lispb::codegen::FunctionSchemaId>({[&] {
                    auto const new_functions{context.storage.declarations.functions.append()};
                    context.storage.declarations.functions
                        .name[context.storage.declarations.functions.index(new_functions)] =
                        context.storage.strings.intern("update");
                    context.storage.declarations.functions
                        .return_type[context.storage.declarations.functions.index(new_functions)] =
                        [&] {
                            auto const new_type_refs{
                                context.storage.declarations.type_refs.append()};
                            context.storage.declarations.type_refs
                                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                                context.storage.strings.intern("void");
                            return new_type_refs;
                        }();
                    context.storage.declarations.functions
                        .is_inline[context.storage.declarations.functions.index(new_functions)] =
                        true;
                    context.storage.declarations.functions
                        .definition_in_source[context.storage.declarations.functions.index(
                            new_functions)] = true;
                    return new_functions;
                }()});
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_soa_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.soas.clone(rows_1.front())};
            context.storage.declarations.soas
                .functions[context.storage.declarations.soas.index(row_0)] =
                context.storage.declarations.range<lispb::codegen::FunctionSchemaId>({[&] {
                    auto const new_functions{context.storage.declarations.functions.append()};
                    context.storage.declarations.functions
                        .name[context.storage.declarations.functions.index(new_functions)] =
                        context.storage.strings.intern("invalid_static_const");
                    context.storage.declarations.functions
                        .return_type[context.storage.declarations.functions.index(new_functions)] =
                        [&] {
                            auto const new_type_refs{
                                context.storage.declarations.type_refs.append()};
                            context.storage.declarations.type_refs
                                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                                context.storage.strings.intern("void");
                            return new_type_refs;
                        }();
                    context.storage.declarations.functions
                        .is_const[context.storage.declarations.functions.index(new_functions)] =
                        true;
                    context.storage.declarations.functions
                        .is_static[context.storage.declarations.functions.index(new_functions)] =
                        true;
                    return new_functions;
                }()});
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_soa_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.soas.clone(rows_1.front())};
            context.storage.declarations.soas
                .functions[context.storage.declarations.soas.index(row_0)] =
                context.storage.declarations.range<lispb::codegen::FunctionSchemaId>({[&] {
                    auto const new_functions{context.storage.declarations.functions.append()};
                    context.storage.declarations.functions
                        .name[context.storage.declarations.functions.index(new_functions)] =
                        context.storage.strings.intern("invalid_trailing_return");
                    context.storage.declarations.functions
                        .return_type[context.storage.declarations.functions.index(new_functions)] =
                        [&] {
                            auto const new_type_refs{
                                context.storage.declarations.type_refs.append()};
                            context.storage.declarations.type_refs
                                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                                context.storage.strings.intern("void");
                            return new_type_refs;
                        }();
                    context.storage.declarations.functions
                        .trailing_return_type[context.storage.declarations.functions.index(
                            new_functions)] = [&] {
                        auto const new_type_refs{context.storage.declarations.type_refs.append()};
                        context.storage.declarations.type_refs
                            .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                            context.storage.strings.intern("int32");
                        return new_type_refs;
                    }();
                    return new_functions;
                }()});
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_soa_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.soas.clone(rows_1.front())};
            context.storage.declarations.soas
                .functions[context.storage.declarations.soas.index(row_0)] =
                context.storage.declarations.range<lispb::codegen::FunctionSchemaId>({[&] {
                    auto const new_functions{context.storage.declarations.functions.append()};
                    context.storage.declarations.functions
                        .name[context.storage.declarations.functions.index(new_functions)] =
                        context.storage.strings.intern("invalid_template");
                    context.storage.declarations.functions
                        .return_type[context.storage.declarations.functions.index(new_functions)] =
                        [&] {
                            auto const new_type_refs{
                                context.storage.declarations.type_refs.append()};
                            context.storage.declarations.type_refs
                                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                                context.storage.strings.intern("void");
                            return new_type_refs;
                        }();
                    context.storage.declarations.functions
                        .template_parameters[context.storage.declarations.functions.index(
                            new_functions)] = context.storage.strings.add(" \t");
                    return new_functions;
                }()});
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_soa_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.soas.clone(rows_1.front())};
            context.storage.declarations.soas
                .functions[context.storage.declarations.soas.index(row_0)] =
                context.storage.declarations.range<lispb::codegen::FunctionSchemaId>({[&] {
                    auto const new_functions{context.storage.declarations.functions.append()};
                    context.storage.declarations.functions
                        .name[context.storage.declarations.functions.index(new_functions)] =
                        context.storage.strings.intern("invalid_requires");
                    context.storage.declarations.functions
                        .return_type[context.storage.declarations.functions.index(new_functions)] =
                        [&] {
                            auto const new_type_refs{
                                context.storage.declarations.type_refs.append()};
                            context.storage.declarations.type_refs
                                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                                context.storage.strings.intern("void");
                            return new_type_refs;
                        }();
                    context.storage.declarations.functions
                        .requires_clause[context.storage.declarations.functions.index(
                            new_functions)] = context.storage.strings.add(" \t");
                    return new_functions;
                }()});
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsCustomSoaFunctionsCollidingWithGeneratedApisAndTypeNames) {
    CompilerTestContext context;
    for (auto const& name : {"sort", "get_view", "FData"}) {
        SCOPED_TRACE(name);
        auto module{valid_soa_module(context)};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(module)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{context.storage.declarations.soas.clone(rows_1.front())};
                context.storage.declarations.soas
                    .functions[context.storage.declarations.soas.index(row_0)] =
                    context.storage.declarations.range<lispb::codegen::FunctionSchemaId>({[&] {
                        auto const new_functions{context.storage.declarations.functions.append()};
                        context.storage.declarations.functions
                            .name[context.storage.declarations.functions.index(new_functions)] =
                            context.storage.strings.intern(name);
                        context.storage.declarations.functions
                            .return_type[context.storage.declarations.functions.index(
                                new_functions)] = [&] {
                            auto const new_type_refs{
                                context.storage.declarations.type_refs.append()};
                            context.storage.declarations.type_refs
                                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                                context.storage.strings.intern("void");
                            return new_type_refs;
                        }();
                        context.storage.declarations.functions
                            .is_inline[context.storage.declarations.functions.index(
                                new_functions)] = true;
                        return new_functions;
                    }()});
                return row_0;
            }();
            return context.storage.declarations.range(rows_1);
        }();
        EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                     std::invalid_argument);
    }
}

TEST(Validation, RejectsMalformedExportSpecifiers) {
    CompilerTestContext context;
    auto soa{valid_soa_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(soa)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(soa)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.soas.clone(rows_1.front())};
            context.storage.declarations.soas
                .export_specifier[context.storage.declarations.soas.index(row_0)] =
                context.storage.strings.intern("bad-specifier");
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, soa)), std::invalid_argument);

    auto homogeneous{valid_homogeneous_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(homogeneous)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(homogeneous)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.homogeneous_layouts.clone(rows_1.front())};
            context.storage.declarations.homogeneous_layouts
                .export_specifier[context.storage.declarations.homogeneous_layouts.index(row_0)] =
                context.storage.strings.intern("class");
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, homogeneous)),
                 std::invalid_argument);

    auto facade{valid_facade_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(facade)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(facade)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.facades.clone(rows_1.front())};
            context.storage.declarations.facades
                .export_specifier[context.storage.declarations.facades.index(row_0)] =
                context.storage.strings.intern("two words");
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, facade)),
                 std::invalid_argument);

    auto table{valid_static_table_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(table)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(table)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.static_tables.clone(rows_1.front())};
            context.storage.declarations.static_tables
                .export_specifier[context.storage.declarations.static_tables.index(row_0)] =
                context.storage.strings.intern("bad-specifier");
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, table)),
                 std::invalid_argument);
}

TEST(Validation, RejectsDuplicateHomogeneousComponents) {
    CompilerTestContext context;
    auto const module{context.storage.declarations.normal_modules.append()};
    context.storage.declarations.normal_modules
        .settings[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const new_module_settings_store{
            context.storage.declarations.module_settings_store.append()};
        context.storage.declarations.module_settings_store
            .name[context.storage.declarations.module_settings_store.index(
                new_module_settings_store)] = context.storage.strings.intern("homogeneous");
        context.storage.declarations.module_settings_store
            .header[context.storage.declarations.module_settings_store.index(
                new_module_settings_store)] = context.storage.strings.add("Values.h");
        context.storage.declarations.module_settings_store
            .source[context.storage.declarations.module_settings_store.index(
                new_module_settings_store)] = context.storage.strings.add("Values.cpp");
        return new_module_settings_store;
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] =
        context.storage.declarations.range<lispb::codegen::DeclarationHandle>({[&] {
            auto const new_homogeneous_layouts{
                context.storage.declarations.homogeneous_layouts.append()};
            context.storage.declarations.homogeneous_layouts
                .name[context.storage.declarations.homogeneous_layouts.index(
                    new_homogeneous_layouts)] = context.storage.strings.intern("Values");
            context.storage.declarations.homogeneous_layouts
                .components[context.storage.declarations.homogeneous_layouts.index(
                    new_homogeneous_layouts)] =
                context.storage.declarations.range<lispb::IdentifierId>(
                    {context.storage.strings.intern("xs"), context.storage.strings.intern("xs")});
            context.storage.declarations.homogeneous_layouts
                .value_types[context.storage.declarations.homogeneous_layouts.index(
                    new_homogeneous_layouts)] =
                context.storage.declarations.range<lispb::codegen::HomogeneousValueId>({[&] {
                    auto const new_homogeneous_values{
                        context.storage.declarations.homogeneous_values.append()};
                    context.storage.declarations.homogeneous_values
                        .type[context.storage.declarations.homogeneous_values.index(
                            new_homogeneous_values)] = [&] {
                        auto const new_type_refs{context.storage.declarations.type_refs.append()};
                        context.storage.declarations.type_refs
                            .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                            context.storage.strings.intern("float");
                        return new_type_refs;
                    }();
                    context.storage.declarations.homogeneous_values
                        .suffix[context.storage.declarations.homogeneous_values.index(
                            new_homogeneous_values)] = context.storage.strings.add("f");
                    return new_homogeneous_values;
                }()});
            return new_homogeneous_layouts;
        }()});

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsHomogeneousLayoutsWithoutValueTypes) {
    CompilerTestContext context;
    auto const module{context.storage.declarations.normal_modules.append()};
    context.storage.declarations.normal_modules
        .settings[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const new_module_settings_store{
            context.storage.declarations.module_settings_store.append()};
        context.storage.declarations.module_settings_store
            .name[context.storage.declarations.module_settings_store.index(
                new_module_settings_store)] = context.storage.strings.intern("homogeneous");
        context.storage.declarations.module_settings_store
            .header[context.storage.declarations.module_settings_store.index(
                new_module_settings_store)] = context.storage.strings.add("Values.h");
        context.storage.declarations.module_settings_store
            .source[context.storage.declarations.module_settings_store.index(
                new_module_settings_store)] = context.storage.strings.add("Values.cpp");
        return new_module_settings_store;
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] =
        context.storage.declarations.range<lispb::codegen::DeclarationHandle>({[&] {
            auto const new_homogeneous_layouts{
                context.storage.declarations.homogeneous_layouts.append()};
            context.storage.declarations.homogeneous_layouts
                .name[context.storage.declarations.homogeneous_layouts.index(
                    new_homogeneous_layouts)] = context.storage.strings.intern("Values");
            context.storage.declarations.homogeneous_layouts
                .components[context.storage.declarations.homogeneous_layouts.index(
                    new_homogeneous_layouts)] =
                context.storage.declarations.range<lispb::IdentifierId>(
                    {context.storage.strings.intern("xs")});
            return new_homogeneous_layouts;
        }()});

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsHomogeneousModulesWithoutSourceOutput) {
    CompilerTestContext context;
    auto module{valid_homogeneous_module(context)};
    context.storage.declarations.normal_modules
        .settings[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto row_0{context.storage.declarations.module_settings_store.clone(
            context.storage.declarations.normal_modules
                .settings[context.storage.declarations.normal_modules.index(module)])};
        context.storage.declarations.module_settings_store
            .source[context.storage.declarations.module_settings_store.index(row_0)]
            .reset();
        return row_0;
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsDuplicateHomogeneousLayoutNames) {
    CompilerTestContext context;
    auto module{valid_homogeneous_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows{std::vector(original_items.begin(), original_items.end())};
        rows.push_back(
            context.storage.declarations
                .items(context.storage.declarations.normal_modules
                           .declarations[context.storage.declarations.normal_modules.index(module)])
                .front());
        return context.storage.declarations.range(rows);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsDuplicateHomogeneousValueSuffixes) {
    CompilerTestContext context;
    auto module{valid_homogeneous_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.homogeneous_layouts.clone(rows_1.front())};
            context.storage.declarations.homogeneous_layouts
                .value_types[context.storage.declarations.homogeneous_layouts.index(row_0)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.homogeneous_layouts
                        .value_types[context.storage.declarations.homogeneous_layouts.index(
                            row_0)])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.push_back([&] {
                    auto const new_homogeneous_values{
                        context.storage.declarations.homogeneous_values.append()};
                    context.storage.declarations.homogeneous_values
                        .type[context.storage.declarations.homogeneous_values.index(
                            new_homogeneous_values)] = [&] {
                        auto const new_type_refs{context.storage.declarations.type_refs.append()};
                        context.storage.declarations.type_refs
                            .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                            context.storage.strings.intern("double");
                        return new_type_refs;
                    }();
                    context.storage.declarations.homogeneous_values
                        .suffix[context.storage.declarations.homogeneous_values.index(
                            new_homogeneous_values)] = context.storage.strings.add("f");
                    return new_homogeneous_values;
                }());
                return context.storage.declarations.range(rows);
            }();
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsMalformedHomogeneousSuffixes) {
    CompilerTestContext context;
    auto module{valid_homogeneous_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.homogeneous_layouts.clone(rows_3.front())};
            context.storage.declarations.homogeneous_layouts
                .value_types[context.storage.declarations.homogeneous_layouts.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.homogeneous_layouts
                        .value_types[context.storage.declarations.homogeneous_layouts.index(
                            row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{
                        context.storage.declarations.homogeneous_values.clone(rows_1.front())};
                    context.storage.declarations.homogeneous_values
                        .suffix[context.storage.declarations.homogeneous_values.index(row_0)] =
                        context.storage.strings.add("-float");
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsCollidingHomogeneousStorageNames) {
    CompilerTestContext context;
    auto module{valid_homogeneous_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] =
        context.storage.declarations.range<lispb::codegen::DeclarationHandle>(
            {[&] {
                 auto const new_homogeneous_layouts{
                     context.storage.declarations.homogeneous_layouts.append()};
                 context.storage.declarations.homogeneous_layouts
                     .name[context.storage.declarations.homogeneous_layouts.index(
                         new_homogeneous_layouts)] = context.storage.strings.intern("Value");
                 context.storage.declarations.homogeneous_layouts
                     .components[context.storage.declarations.homogeneous_layouts.index(
                         new_homogeneous_layouts)] =
                     context.storage.declarations.range<lispb::IdentifierId>(
                         {context.storage.strings.intern("xs")});
                 context.storage.declarations.homogeneous_layouts
                     .value_types[context.storage.declarations.homogeneous_layouts.index(
                         new_homogeneous_layouts)] =
                     context.storage.declarations.range<lispb::codegen::HomogeneousValueId>({[&] {
                         auto const new_homogeneous_values{
                             context.storage.declarations.homogeneous_values.append()};
                         context.storage.declarations.homogeneous_values
                             .type[context.storage.declarations.homogeneous_values.index(
                                 new_homogeneous_values)] = [&] {
                             auto const new_type_refs{
                                 context.storage.declarations.type_refs.append()};
                             context.storage.declarations.type_refs
                                 .name[context.storage.declarations.type_refs.index(
                                     new_type_refs)] = context.storage.strings.intern("float");
                             return new_type_refs;
                         }();
                         context.storage.declarations.homogeneous_values
                             .suffix[context.storage.declarations.homogeneous_values.index(
                                 new_homogeneous_values)] = context.storage.strings.add("sf");
                         return new_homogeneous_values;
                     }()});
                 return new_homogeneous_layouts;
             }(),
             [&] {
                 auto const new_homogeneous_layouts{
                     context.storage.declarations.homogeneous_layouts.append()};
                 context.storage.declarations.homogeneous_layouts
                     .name[context.storage.declarations.homogeneous_layouts.index(
                         new_homogeneous_layouts)] = context.storage.strings.intern("Values");
                 context.storage.declarations.homogeneous_layouts
                     .components[context.storage.declarations.homogeneous_layouts.index(
                         new_homogeneous_layouts)] =
                     context.storage.declarations.range<lispb::IdentifierId>(
                         {context.storage.strings.intern("xs")});
                 context.storage.declarations.homogeneous_layouts
                     .value_types[context.storage.declarations.homogeneous_layouts.index(
                         new_homogeneous_layouts)] =
                     context.storage.declarations.range<lispb::codegen::HomogeneousValueId>({[&] {
                         auto const new_homogeneous_values{
                             context.storage.declarations.homogeneous_values.append()};
                         context.storage.declarations.homogeneous_values
                             .type[context.storage.declarations.homogeneous_values.index(
                                 new_homogeneous_values)] = [&] {
                             auto const new_type_refs{
                                 context.storage.declarations.type_refs.append()};
                             context.storage.declarations.type_refs
                                 .name[context.storage.declarations.type_refs.index(
                                     new_type_refs)] = context.storage.strings.intern("float");
                             return new_type_refs;
                         }();
                         context.storage.declarations.homogeneous_values
                             .suffix[context.storage.declarations.homogeneous_values.index(
                                 new_homogeneous_values)] = context.storage.strings.add("f");
                         return new_homogeneous_values;
                     }()});
                 return new_homogeneous_layouts;
             }()});

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsDuplicateHomogeneousEquivalentSpecialisations) {
    CompilerTestContext context;
    auto module{valid_homogeneous_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.homogeneous_layouts.clone(rows_1.front())};
            context.storage.declarations.homogeneous_layouts
                .value_types[context.storage.declarations.homogeneous_layouts.index(
                    row_0)] = context.storage.declarations.range<lispb::codegen::
                                                                     HomogeneousValueId>(
                {[&] {
                     auto const new_homogeneous_values{
                         context.storage.declarations.homogeneous_values.append()};
                     context.storage.declarations.homogeneous_values
                         .type[context.storage.declarations.homogeneous_values.index(
                             new_homogeneous_values)] = [&] {
                         auto const new_type_refs{context.storage.declarations.type_refs.append()};
                         context.storage.declarations.type_refs
                             .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                             context.storage.strings.intern("float");
                         return new_type_refs;
                     }();
                     context.storage.declarations.homogeneous_values
                         .suffix[context.storage.declarations.homogeneous_values.index(
                             new_homogeneous_values)] = context.storage.strings.add("f");
                     context.storage.declarations.homogeneous_values
                         .equivalent_type[context.storage.declarations.homogeneous_values.index(
                             new_homogeneous_values)] = [&] {
                         auto const new_type_refs{context.storage.declarations.type_refs.append()};
                         context.storage.declarations.type_refs
                             .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                             context.storage.strings.intern("FVector2f");
                         return new_type_refs;
                     }();
                     return new_homogeneous_values;
                 }(),
                 [&] {
                     auto const new_homogeneous_values{
                         context.storage.declarations.homogeneous_values.append()};
                     context.storage.declarations.homogeneous_values
                         .type[context.storage.declarations.homogeneous_values.index(
                             new_homogeneous_values)] = [&] {
                         auto const new_type_refs{context.storage.declarations.type_refs.append()};
                         context.storage.declarations.type_refs
                             .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                             context.storage.strings.intern("float");
                         return new_type_refs;
                     }();
                     context.storage.declarations.homogeneous_values
                         .suffix[context.storage.declarations.homogeneous_values.index(
                             new_homogeneous_values)] = context.storage.strings.add("other");
                     context.storage.declarations.homogeneous_values
                         .equivalent_type[context.storage.declarations.homogeneous_values.index(
                             new_homogeneous_values)] = [&] {
                         auto const new_type_refs{context.storage.declarations.type_refs.append()};
                         context.storage.declarations.type_refs
                             .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                             context.storage.strings.intern("FOtherVector2f");
                         return new_type_refs;
                     }();
                     return new_homogeneous_values;
                 }()});
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsDuplicateHomogeneousInputTypes) {
    CompilerTestContext context;
    auto module{valid_homogeneous_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.homogeneous_layouts.clone(rows_3.front())};
            context.storage.declarations.homogeneous_layouts
                .value_types[context.storage.declarations.homogeneous_layouts.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.homogeneous_layouts
                        .value_types[context.storage.declarations.homogeneous_layouts.index(
                            row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{
                        context.storage.declarations.homogeneous_values.clone(rows_1.front())};
                    context.storage.declarations.homogeneous_values
                        .input_types[context.storage.declarations.homogeneous_values.index(row_0)] =
                        context.storage.declarations.range<lispb::codegen::TypeRefId>(
                            {[&] {
                                 auto const new_type_refs{
                                     context.storage.declarations.type_refs.append()};
                                 context.storage.declarations.type_refs
                                     .name[context.storage.declarations.type_refs.index(
                                         new_type_refs)] =
                                     context.storage.strings.intern("FVector2f");
                                 return new_type_refs;
                             }(),
                             [&] {
                                 auto const new_type_refs{
                                     context.storage.declarations.type_refs.append()};
                                 context.storage.declarations.type_refs
                                     .name[context.storage.declarations.type_refs.index(
                                         new_type_refs)] =
                                     context.storage.strings.intern("FVector2f");
                                 return new_type_refs;
                             }()});
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsHomogeneousInputOverloadsWithMoreThanThreeComponents) {
    CompilerTestContext context;
    auto module{valid_homogeneous_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.homogeneous_layouts.clone(rows_1.front())};
            context.storage.declarations.homogeneous_layouts
                .input_members[context.storage.declarations.homogeneous_layouts.index(row_0)] =
                context.storage.declarations.range<lispb::IdentifierId>(
                    {context.storage.strings.intern("X")});
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.homogeneous_layouts.clone(rows_1.front())};
            context.storage.declarations.homogeneous_layouts
                .input_members[context.storage.declarations.homogeneous_layouts.index(row_0)] = {};
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.homogeneous_layouts.clone(rows_1.front())};
            context.storage.declarations.homogeneous_layouts
                .components[context.storage.declarations.homogeneous_layouts.index(row_0)] =
                context.storage.declarations.range<lispb::IdentifierId>(
                    {context.storage.strings.intern("xs"),
                     context.storage.strings.intern("ys"),
                     context.storage.strings.intern("zs"),
                     context.storage.strings.intern("ws")});
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.homogeneous_layouts.clone(rows_3.front())};
            context.storage.declarations.homogeneous_layouts
                .value_types[context.storage.declarations.homogeneous_layouts.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.homogeneous_layouts
                        .value_types[context.storage.declarations.homogeneous_layouts.index(
                            row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{
                        context.storage.declarations.homogeneous_values.clone(rows_1.front())};
                    context.storage.declarations.homogeneous_values
                        .input_types[context.storage.declarations.homogeneous_values.index(row_0)] =
                        context.storage.declarations.range<lispb::codegen::TypeRefId>({[&] {
                            auto const new_type_refs{
                                context.storage.declarations.type_refs.append()};
                            context.storage.declarations.type_refs
                                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                                context.storage.strings.intern("FVector4f");
                            return new_type_refs;
                        }()});
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsHomogeneousInputComponentsWithDuplicateInitials) {
    CompilerTestContext context;
    auto module{valid_homogeneous_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.homogeneous_layouts.clone(rows_1.front())};
            context.storage.declarations.homogeneous_layouts
                .components[context.storage.declarations.homogeneous_layouts.index(row_0)] =
                context.storage.declarations.range<lispb::IdentifierId>(
                    {context.storage.strings.intern("x_values"),
                     context.storage.strings.intern("x_weights")});
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.homogeneous_layouts.clone(rows_3.front())};
            context.storage.declarations.homogeneous_layouts
                .value_types[context.storage.declarations.homogeneous_layouts.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.homogeneous_layouts
                        .value_types[context.storage.declarations.homogeneous_layouts.index(
                            row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{
                        context.storage.declarations.homogeneous_values.clone(rows_1.front())};
                    context.storage.declarations.homogeneous_values
                        .input_types[context.storage.declarations.homogeneous_values.index(row_0)] =
                        context.storage.declarations.range<lispb::codegen::TypeRefId>({[&] {
                            auto const new_type_refs{
                                context.storage.declarations.type_refs.append()};
                            context.storage.declarations.type_refs
                                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                                context.storage.strings.intern("FInput");
                            return new_type_refs;
                        }()});
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsHomogeneousComponentParameterCollisionsWithoutInputTypes) {
    CompilerTestContext context;
    auto module{valid_homogeneous_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.homogeneous_layouts.clone(rows_1.front())};
            context.storage.declarations.homogeneous_layouts
                .components[context.storage.declarations.homogeneous_layouts.index(row_0)] =
                context.storage.declarations.range<lispb::IdentifierId>(
                    {context.storage.strings.intern("x_values"),
                     context.storage.strings.intern("x_weights")});
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsVectorsWithMoreThanThreeComponents) {
    CompilerTestContext context;
    auto module{valid_vector_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.vector_soas.clone(rows_1.front())};
            context.storage.declarations.vector_soas
                .components[context.storage.declarations.vector_soas.index(row_0)] =
                context.storage.declarations.range<lispb::IdentifierId>(
                    {context.storage.strings.intern("xs"),
                     context.storage.strings.intern("ys"),
                     context.storage.strings.intern("zs"),
                     context.storage.strings.intern("ws")});
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.vector_soas.clone(rows_1.front())};
            context.storage.declarations.vector_soas
                .equivalent_type[context.storage.declarations.vector_soas.index(row_0)] = [&] {
                auto const new_type_refs{context.storage.declarations.type_refs.append()};
                context.storage.declarations.type_refs
                    .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                    context.storage.strings.intern("FVector4f");
                return new_type_refs;
            }();
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsDuplicateVectorComponents) {
    CompilerTestContext context;
    auto module{valid_vector_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.vector_soas.clone(rows_1.front())};
            context.storage.declarations.vector_soas
                .components[context.storage.declarations.vector_soas.index(row_0)] =
                context.storage.declarations.range<lispb::IdentifierId>(
                    {context.storage.strings.intern("xs"), context.storage.strings.intern("xs")});
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsVectorModulesWithoutSourceOutput) {
    CompilerTestContext context;
    auto module{valid_vector_module(context)};
    context.storage.declarations.normal_modules
        .settings[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto row_0{context.storage.declarations.module_settings_store.clone(
            context.storage.declarations.normal_modules
                .settings[context.storage.declarations.normal_modules.index(module)])};
        context.storage.declarations.module_settings_store
            .source[context.storage.declarations.module_settings_store.index(row_0)]
            .reset();
        return row_0;
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsMalformedVectorStorageIdentifiers) {
    CompilerTestContext context;
    auto module{valid_vector_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.vector_soas.clone(rows_1.front())};
            context.storage.declarations.vector_soas
                .name[context.storage.declarations.vector_soas.index(row_0)] =
                context.storage.strings.intern("F Vectors");
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsVectorComponentsWithDuplicateParameterInitials) {
    CompilerTestContext context;
    auto module{valid_vector_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.vector_soas.clone(rows_1.front())};
            context.storage.declarations.vector_soas
                .components[context.storage.declarations.vector_soas.index(row_0)] =
                context.storage.declarations.range<lispb::IdentifierId>(
                    {context.storage.strings.intern("x_values"),
                     context.storage.strings.intern("x_weights")});
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsFacadesWithoutMethods) {
    CompilerTestContext context;
    auto module{valid_facade_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.facades.clone(rows_1.front())};
            context.storage.declarations.facades
                .methods[context.storage.declarations.facades.index(row_0)] = {};
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsUnknownFacadeAccess) {
    CompilerTestContext context;
    EXPECT_THROW(parse_test_manifest(context, R"((module facade :header "Facade.h"
      (facade FFacade :target FTarget :method-access protected (method reset void))))"),
                 lispb::codegen::sexpr::SourceError);
}

TEST(Validation, RejectsMalformedFacadeMethodIdentifiers) {
    CompilerTestContext context;
    auto module{valid_facade_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.facades.clone(rows_3.front())};
            context.storage.declarations.facades
                .methods[context.storage.declarations.facades.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.facades
                        .methods[context.storage.declarations.facades.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{context.storage.declarations.facade_methods.clone(rows_1.front())};
                    context.storage.declarations.facade_methods
                        .name[context.storage.declarations.facade_methods.index(row_0)] =
                        context.storage.strings.intern("bad-name");
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsOutOfLineFacadesWithoutSourceOutput) {
    CompilerTestContext context;
    auto module{valid_facade_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.facades.clone(rows_1.front())};
            context.storage.declarations.facades
                .definitions_in_source[context.storage.declarations.facades.index(row_0)] = true;
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, AllowsInlineFacadesInModulesWithSourceOutput) {
    CompilerTestContext context;
    auto module{valid_facade_module(context)};
    context.storage.declarations.normal_modules
        .settings[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto row_0{context.storage.declarations.module_settings_store.clone(
            context.storage.declarations.normal_modules
                .settings[context.storage.declarations.normal_modules.index(module)])};
        context.storage.declarations.module_settings_store
            .source[context.storage.declarations.module_settings_store.index(row_0)] =
            context.storage.strings.add("Facade.cpp");
        return row_0;
    }();

    EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));
}

TEST(Validation, RejectsDuplicateFacadeMethodParameters) {
    CompilerTestContext context;
    auto module{valid_facade_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.facades.clone(rows_3.front())};
            context.storage.declarations.facades
                .methods[context.storage.declarations.facades.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.facades
                        .methods[context.storage.declarations.facades.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{context.storage.declarations.facade_methods.clone(rows_1.front())};
                    context.storage.declarations.facade_methods
                        .parameters[context.storage.declarations.facade_methods.index(row_0)] =
                        context.storage.declarations.range<lispb::codegen::ParameterId>(
                            {[&] {
                                 auto const new_parameters{
                                     context.storage.declarations.parameters.append()};
                                 context.storage.declarations.parameters
                                     .type[context.storage.declarations.parameters.index(
                                         new_parameters)] = [&] {
                                     auto const new_type_refs{
                                         context.storage.declarations.type_refs.append()};
                                     context.storage.declarations.type_refs
                                         .name[context.storage.declarations.type_refs.index(
                                             new_type_refs)] =
                                         context.storage.strings.intern("int32");
                                     return new_type_refs;
                                 }();
                                 context.storage.declarations.parameters
                                     .name[context.storage.declarations.parameters.index(
                                         new_parameters)] = context.storage.strings.intern("value");
                                 return new_parameters;
                             }(),
                             [&] {
                                 auto const new_parameters{
                                     context.storage.declarations.parameters.append()};
                                 context.storage.declarations.parameters
                                     .type[context.storage.declarations.parameters.index(
                                         new_parameters)] = [&] {
                                     auto const new_type_refs{
                                         context.storage.declarations.type_refs.append()};
                                     context.storage.declarations.type_refs
                                         .name[context.storage.declarations.type_refs.index(
                                             new_type_refs)] =
                                         context.storage.strings.intern("float");
                                     return new_type_refs;
                                 }();
                                 context.storage.declarations.parameters
                                     .name[context.storage.declarations.parameters.index(
                                         new_parameters)] = context.storage.strings.intern("value");
                                 return new_parameters;
                             }()});
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsDuplicateFacadeMethodSignatures) {
    CompilerTestContext context;
    auto module{valid_facade_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.facades.clone(rows_1.front())};
            context.storage.declarations.facades
                .methods[context.storage.declarations.facades.index(row_0)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.facades
                        .methods[context.storage.declarations.facades.index(row_0)])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.push_back(
                    context.storage.declarations
                        .items(
                            context.storage.declarations.facades
                                .methods[context.storage.declarations.facades.index(
                                    context.storage.declarations
                                        .items(context.storage.declarations.normal_modules
                                                   .declarations[context.storage.declarations
                                                                     .normal_modules.index(module)])
                                        .front())])
                        .front());
                return context.storage.declarations.range(rows);
            }();
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsFacadeOverloadsDifferingOnlyByNoexcept) {
    CompilerTestContext context;
    auto module{valid_facade_module(context)};
    auto second{context.storage.declarations.facade_methods.clone(
        context.storage.declarations
            .items(
                context.storage.declarations.facades
                    .methods[context.storage.declarations.facades.index(
                        context.storage.declarations
                            .items(context.storage.declarations.normal_modules.declarations
                                       [context.storage.declarations.normal_modules.index(module)])
                            .front())])
            .front())};
    context.storage.declarations.facade_methods
        .is_noexcept[context.storage.declarations.facade_methods.index(second)] = true;
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.facades.clone(rows_1.front())};
            context.storage.declarations.facades
                .methods[context.storage.declarations.facades.index(row_0)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.facades
                        .methods[context.storage.declarations.facades.index(row_0)])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.push_back(second);
                return context.storage.declarations.range(rows);
            }();
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsFacadeMethodsCollidingWithBind) {
    CompilerTestContext context;
    auto module{valid_facade_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.facades.clone(rows_1.front())};
            context.storage.declarations.facades.methods[context.storage.declarations.facades.index(
                row_0)] = context.storage.declarations.range<lispb::codegen::FacadeMethodId>({[&] {
                auto const new_facade_methods{context.storage.declarations.facade_methods.append()};
                context.storage.declarations.facade_methods
                    .name[context.storage.declarations.facade_methods.index(new_facade_methods)] =
                    context.storage.strings.intern("bind");
                context.storage.declarations.facade_methods
                    .return_type[context.storage.declarations.facade_methods.index(
                        new_facade_methods)] = [&] {
                    auto const new_type_refs{context.storage.declarations.type_refs.append()};
                    context.storage.declarations.type_refs
                        .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                        context.storage.strings.intern("void");
                    return new_type_refs;
                }();
                context.storage.declarations.facade_methods
                    .parameters[context.storage.declarations.facade_methods.index(
                        new_facade_methods)] =
                    context.storage.declarations.range<lispb::codegen::ParameterId>({[&] {
                        auto const new_parameters{context.storage.declarations.parameters.append()};
                        context.storage.declarations.parameters
                            .type[context.storage.declarations.parameters.index(
                                new_parameters)] = [&] {
                            auto const new_type_refs{
                                context.storage.declarations.type_refs.append()};
                            context.storage.declarations.type_refs
                                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                                context.storage.strings.intern("FTarget&");
                            return new_type_refs;
                        }();
                        context.storage.declarations.parameters
                            .name[context.storage.declarations.parameters.index(new_parameters)] =
                            context.storage.strings.intern("target");
                        return new_parameters;
                    }()});
                return new_facade_methods;
            }()});
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsNonTrailingFacadeDefaultArguments) {
    CompilerTestContext context;
    auto module{valid_facade_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.facades.clone(rows_3.front())};
            context.storage.declarations.facades
                .methods[context.storage.declarations.facades.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.facades
                        .methods[context.storage.declarations.facades.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{context.storage.declarations.facade_methods.clone(rows_1.front())};
                    context.storage.declarations.facade_methods
                        .parameters[context.storage.declarations.facade_methods.index(row_0)] =
                        context.storage.declarations.range<lispb::codegen::ParameterId>(
                            {[&] {
                                 auto const new_parameters{
                                     context.storage.declarations.parameters.append()};
                                 context.storage.declarations.parameters
                                     .type[context.storage.declarations.parameters.index(
                                         new_parameters)] = [&] {
                                     auto const new_type_refs{
                                         context.storage.declarations.type_refs.append()};
                                     context.storage.declarations.type_refs
                                         .name[context.storage.declarations.type_refs.index(
                                             new_type_refs)] =
                                         context.storage.strings.intern("int32");
                                     return new_type_refs;
                                 }();
                                 context.storage.declarations.parameters
                                     .name[context.storage.declarations.parameters.index(
                                         new_parameters)] = context.storage.strings.intern("first");
                                 context.storage.declarations.parameters
                                     .default_value[context.storage.declarations.parameters.index(
                                         new_parameters)] = context.storage.strings.add("0");
                                 return new_parameters;
                             }(),
                             [&] {
                                 auto const new_parameters{
                                     context.storage.declarations.parameters.append()};
                                 context.storage.declarations.parameters
                                     .type[context.storage.declarations.parameters.index(
                                         new_parameters)] = [&] {
                                     auto const new_type_refs{
                                         context.storage.declarations.type_refs.append()};
                                     context.storage.declarations.type_refs
                                         .name[context.storage.declarations.type_refs.index(
                                             new_type_refs)] =
                                         context.storage.strings.intern("int32");
                                     return new_type_refs;
                                 }();
                                 context.storage.declarations.parameters
                                     .name[context.storage.declarations.parameters.index(
                                         new_parameters)] =
                                     context.storage.strings.intern("second");
                                 return new_parameters;
                             }()});
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsUnknownFacadeValidationDependencies) {
    CompilerTestContext context;
    auto module{valid_facade_module(context)};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.facades.clone(rows_1.front())};
            context.storage.declarations.facades
                .validation_dependencies[context.storage.declarations.facades.index(row_0)] =
                context.storage.declarations.range<lispb::IdentifierId>(
                    {context.storage.strings.intern("missing")});
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsInvalidFacadeFriendAndMemberCollisions) {
    CompilerTestContext context;
    auto module{valid_facade_module(context)};
    EXPECT_THROW(parse_test_manifest(context, R"((module facade :header "Facade.h"
      (facade FFacade :target FTarget :friend-kind union (method reset void))))"),
                 lispb::codegen::sexpr::SourceError);

    module = valid_facade_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.facades.clone(rows_1.front())};
            context.storage.declarations.facades
                .friends[context.storage.declarations.facades.index(row_0)] =
                context.storage.declarations.range<lispb::StringId>(
                    {context.storage.strings.add("class")});
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_facade_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.facades.clone(rows_1.front())};
            context.storage.declarations.facades
                .target_member_name[context.storage.declarations.facades.index(row_0)] =
                context.storage.strings.intern("bind");
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_facade_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.facades.clone(rows_3.front())};
            context.storage.declarations.facades
                .methods[context.storage.declarations.facades.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.facades
                        .methods[context.storage.declarations.facades.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{context.storage.declarations.facade_methods.clone(rows_1.front())};
                    context.storage.declarations.facade_methods
                        .name[context.storage.declarations.facade_methods.index(row_0)] =
                        context.storage.declarations.facades
                            .target_member_name[context.storage.declarations.facades.index(
                                context.storage.declarations
                                    .items(context.storage.declarations.normal_modules
                                               .declarations[context.storage.declarations
                                                                 .normal_modules.index(module)])
                                    .front())];
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsDuplicateUmbrellaHeaders) {
    CompilerTestContext context;
    auto const module{context.storage.declarations.umbrella_modules.append()};
    context.storage.declarations.umbrella_modules
        .settings[context.storage.declarations.umbrella_modules.index(module)] = [&] {
        auto const new_module_settings_store{
            context.storage.declarations.module_settings_store.append()};
        context.storage.declarations.module_settings_store
            .name[context.storage.declarations.module_settings_store.index(
                new_module_settings_store)] = context.storage.strings.intern("all");
        context.storage.declarations.module_settings_store
            .header[context.storage.declarations.module_settings_store.index(
                new_module_settings_store)] = context.storage.strings.add("All.h");
        return new_module_settings_store;
    }();
    context.storage.declarations.umbrella_modules
        .headers[context.storage.declarations.umbrella_modules.index(module)] =
        context.storage.declarations.range<lispb::StringId>(
            {context.storage.strings.add("Value.h"), context.storage.strings.add("Value.h")});

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsUmbrellaModulesWithoutHeaders) {
    CompilerTestContext context;
    auto const module{context.storage.declarations.umbrella_modules.append()};
    context.storage.declarations.umbrella_modules
        .settings[context.storage.declarations.umbrella_modules.index(module)] = [&] {
        auto const new_module_settings_store{
            context.storage.declarations.module_settings_store.append()};
        context.storage.declarations.module_settings_store
            .name[context.storage.declarations.module_settings_store.index(
                new_module_settings_store)] = context.storage.strings.intern("all");
        context.storage.declarations.module_settings_store
            .header[context.storage.declarations.module_settings_store.index(
                new_module_settings_store)] = context.storage.strings.add("All.h");
        return new_module_settings_store;
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsUnsupportedUmbrellaSourceAndNamespaceSettings) {
    CompilerTestContext context;
    auto const module{context.storage.declarations.umbrella_modules.append()};
    context.storage.declarations.umbrella_modules
        .settings[context.storage.declarations.umbrella_modules.index(module)] = [&] {
        auto const new_module_settings_store{
            context.storage.declarations.module_settings_store.append()};
        context.storage.declarations.module_settings_store
            .name[context.storage.declarations.module_settings_store.index(
                new_module_settings_store)] = context.storage.strings.intern("all");
        context.storage.declarations.module_settings_store
            .header[context.storage.declarations.module_settings_store.index(
                new_module_settings_store)] = context.storage.strings.add("All.h");
        context.storage.declarations.module_settings_store
            .source[context.storage.declarations.module_settings_store.index(
                new_module_settings_store)] = context.storage.strings.add("All.cpp");
        return new_module_settings_store;
    }();
    context.storage.declarations.umbrella_modules
        .headers[context.storage.declarations.umbrella_modules.index(module)] =
        context.storage.declarations.range<lispb::StringId>(
            {context.storage.strings.add("Value.h")});
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    context.storage.declarations.umbrella_modules
        .settings[context.storage.declarations.umbrella_modules.index(module)] = [&] {
        auto row_0{context.storage.declarations.module_settings_store.clone(
            context.storage.declarations.umbrella_modules
                .settings[context.storage.declarations.umbrella_modules.index(module)])};
        context.storage.declarations.module_settings_store
            .source[context.storage.declarations.module_settings_store.index(row_0)]
            .reset();
        return row_0;
    }();
    context.storage.declarations.umbrella_modules
        .settings[context.storage.declarations.umbrella_modules.index(module)] = [&] {
        auto row_0{context.storage.declarations.module_settings_store.clone(
            context.storage.declarations.umbrella_modules
                .settings[context.storage.declarations.umbrella_modules.index(module)])};
        context.storage.declarations.module_settings_store
            .namespace_name[context.storage.declarations.module_settings_store.index(row_0)] =
            context.storage.strings.intern("project");
        return row_0;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsInvalidSettingsDefinitions) {
    CompilerTestContext context;
    auto module = [&] {
        auto const new_settings_modules{context.storage.declarations.settings_modules.append()};
        context.storage.declarations.settings_modules
            .settings[context.storage.declarations.settings_modules.index(new_settings_modules)] =
            [&] {
                auto const new_module_settings_store{
                    context.storage.declarations.module_settings_store.append()};
                context.storage.declarations.module_settings_store
                    .name[context.storage.declarations.module_settings_store.index(
                        new_module_settings_store)] = context.storage.strings.intern("settings");
                context.storage.declarations.module_settings_store
                    .header[context.storage.declarations.module_settings_store.index(
                        new_module_settings_store)] = context.storage.strings.add("Settings.h");
                context.storage.declarations.module_settings_store
                    .source[context.storage.declarations.module_settings_store.index(
                        new_module_settings_store)] = context.storage.strings.add("Settings.cpp");
                return new_module_settings_store;
            }();
        context.storage.declarations.settings_modules
            .api_name[context.storage.declarations.settings_modules.index(new_settings_modules)] =
            context.storage.strings.intern("TSettingsAccess");
        context.storage.declarations.settings_modules
            .state_name[context.storage.declarations.settings_modules.index(new_settings_modules)] =
            context.storage.strings.intern("FSettingsState");
        context.storage.declarations.settings_modules
            .categories[context.storage.declarations.settings_modules.index(new_settings_modules)] =
            context.storage.declarations.range<lispb::codegen::SettingCategoryId>({[&] {
                auto const new_setting_categories{
                    context.storage.declarations.setting_categories.append()};
                context.storage.declarations.setting_categories
                    .name[context.storage.declarations.setting_categories.index(
                        new_setting_categories)] = context.storage.strings.intern("video");
                context.storage.declarations.setting_categories
                    .label[context.storage.declarations.setting_categories.index(
                        new_setting_categories)] = context.storage.strings.add("Video");
                return new_setting_categories;
            }()});
        context.storage.declarations.settings_modules
            .settings_list[context.storage.declarations.settings_modules.index(
                new_settings_modules)] =
            context.storage.declarations.range<lispb::codegen::SettingId>({[&] {
                auto const new_settings{context.storage.declarations.settings.append()};
                context.storage.declarations.settings
                    .name[context.storage.declarations.settings.index(new_settings)] =
                    context.storage.strings.intern("vsync");
                context.storage.declarations.settings
                    .label[context.storage.declarations.settings.index(new_settings)] =
                    context.storage.strings.add("VSync");
                context.storage.declarations.settings
                    .category[context.storage.declarations.settings.index(new_settings)] =
                    context.storage.strings.intern("missing");
                context.storage.declarations.settings
                    .value_type[context.storage.declarations.settings.index(new_settings)] = [&] {
                    auto const new_type_refs{context.storage.declarations.type_refs.append()};
                    context.storage.declarations.type_refs
                        .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                        context.storage.strings.intern("bool");
                    return new_type_refs;
                }();
                context.storage.declarations.settings
                    .backend[context.storage.declarations.settings.index(new_settings)] =
                    context.storage.strings.intern("engine");
                context.storage.declarations.settings
                    .apply_mode[context.storage.declarations.settings.index(new_settings)] =
                    SettingApplyMode::deferred;
                context.storage.declarations.settings
                    .control[context.storage.declarations.settings.index(new_settings)] = [&] {
                    auto const new_setting_controls{
                        context.storage.declarations.setting_controls.append()};
                    context.storage.declarations.setting_controls
                        .kind[context.storage.declarations.setting_controls.index(
                            new_setting_controls)] = SettingControlKind::toggle;
                    return new_setting_controls;
                }();
                return new_settings;
            }()});
        return new_settings_modules;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    context.storage.declarations.settings_modules
        .settings_list[context.storage.declarations.settings_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.settings_modules
                .settings_list[context.storage.declarations.settings_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.settings.clone(rows_1.front())};
            context.storage.declarations.settings
                .category[context.storage.declarations.settings.index(row_0)] =
                context.storage.strings.intern("video");
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    context.storage.declarations.settings_modules
        .settings_list[context.storage.declarations.settings_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.settings_modules
                .settings_list[context.storage.declarations.settings_modules.index(module)])};
        auto rows_2{std::vector(original_items.begin(), original_items.end())};
        rows_2.front() = [&] {
            auto row_1{context.storage.declarations.settings.clone(rows_2.front())};
            context.storage.declarations.settings
                .control[context.storage.declarations.settings.index(row_1)] = [&] {
                auto row_0{context.storage.declarations.setting_controls.clone(
                    context.storage.declarations.settings
                        .control[context.storage.declarations.settings.index(row_1)])};
                context.storage.declarations.setting_controls
                    .kind[context.storage.declarations.setting_controls.index(row_0)] =
                    SettingControlKind::choice;
                return row_0;
            }();
            return row_1;
        }();
        return context.storage.declarations.range(rows_2);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    context.storage.declarations.settings_modules
        .settings_list[context.storage.declarations.settings_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.settings_modules
                .settings_list[context.storage.declarations.settings_modules.index(module)])};
        auto rows_2{std::vector(original_items.begin(), original_items.end())};
        rows_2.front() = [&] {
            auto row_1{context.storage.declarations.settings.clone(rows_2.front())};
            context.storage.declarations.settings
                .control[context.storage.declarations.settings.index(row_1)] = [&] {
                auto row_0{context.storage.declarations.setting_controls.clone(
                    context.storage.declarations.settings
                        .control[context.storage.declarations.settings.index(row_1)])};
                context.storage.declarations.setting_controls
                    .kind[context.storage.declarations.setting_controls.index(row_0)] =
                    SettingControlKind::custom;
                return row_0;
            }();
            return row_1;
        }();
        return context.storage.declarations.range(rows_2);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, NormalModuleReservesGeneratedNamesAcrossDeclarationKinds) {
    CompilerTestContext context;
    auto vector = [&] {
        auto const new_vector_soas{context.storage.declarations.vector_soas.append()};
        context.storage.declarations.vector_soas
            .name[context.storage.declarations.vector_soas.index(new_vector_soas)] =
            context.storage.strings.intern("Foo");
        context.storage.declarations.vector_soas
            .value_type[context.storage.declarations.vector_soas.index(new_vector_soas)] = [&] {
            auto const new_type_refs{context.storage.declarations.type_refs.append()};
            context.storage.declarations.type_refs
                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                context.storage.strings.intern("float");
            return new_type_refs;
        }();
        context.storage.declarations.vector_soas
            .components[context.storage.declarations.vector_soas.index(new_vector_soas)] =
            context.storage.declarations.range<lispb::IdentifierId>(
                {context.storage.strings.intern("xs"), context.storage.strings.intern("ys")});
        context.storage.declarations.vector_soas
            .equivalent_type[context.storage.declarations.vector_soas.index(new_vector_soas)] =
            [&] {
                auto const new_type_refs{context.storage.declarations.type_refs.append()};
                context.storage.declarations.type_refs
                    .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                    context.storage.strings.intern("int32");
                return new_type_refs;
            }();
        return new_vector_soas;
    }();
    auto module = normal_module(
        context,
        {vector,
         [&] {
             auto const new_enums{context.storage.declarations.enums.append()};
             context.storage.declarations.enums
                 .name[context.storage.declarations.enums.index(new_enums)] =
                 context.storage.strings.intern("FooView");
             context.storage.declarations.enums
                 .underlying_type[context.storage.declarations.enums.index(new_enums)] = [&] {
                 auto const new_type_refs{context.storage.declarations.type_refs.append()};
                 context.storage.declarations.type_refs
                     .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                     context.storage.strings.intern("uint8");
                 return new_type_refs;
             }();
             context.storage.declarations.enums
                 .values[context.storage.declarations.enums.index(new_enums)] =
                 context.storage.declarations.range<lispb::codegen::EnumeratorId>({[&] {
                     auto const new_enumerators{context.storage.declarations.enumerators.append()};
                     context.storage.declarations.enumerators
                         .name[context.storage.declarations.enumerators.index(new_enumerators)] =
                         context.storage.strings.intern("One");
                     return new_enumerators;
                 }()});
             return new_enums;
         }()},
        "Mixed.cpp");
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = normal_module(
        context,
        {vector,
         [&] {
             auto const new_records{context.storage.declarations.records.append()};
             context.storage.declarations.records
                 .name[context.storage.declarations.records.index(new_records)] =
                 context.storage.strings.intern("FooConstView");
             context.storage.declarations.records
                 .members[context.storage.declarations.records.index(new_records)] =
                 context.storage.declarations.range<lispb::codegen::RecordMemberId>({[&] {
                     auto const new_record_members{
                         context.storage.declarations.record_members.append()};
                     context.storage.declarations.record_members
                         .name[context.storage.declarations.record_members.index(
                             new_record_members)] = context.storage.strings.intern("value");
                     context.storage.declarations.record_members
                         .type[context.storage.declarations.record_members.index(
                             new_record_members)] = [&] {
                         auto const new_type_refs{context.storage.declarations.type_refs.append()};
                         context.storage.declarations.type_refs
                             .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                             context.storage.strings.intern("int32");
                         return new_type_refs;
                     }();
                     return new_record_members;
                 }()});
             return new_records;
         }()},
        "Mixed.cpp");
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    auto layout = [&] {
        auto const new_homogeneous_layouts{
            context.storage.declarations.homogeneous_layouts.append()};
        context.storage.declarations.homogeneous_layouts
            .name[context.storage.declarations.homogeneous_layouts.index(new_homogeneous_layouts)] =
            context.storage.strings.intern("Values");
        context.storage.declarations.homogeneous_layouts
            .components[context.storage.declarations.homogeneous_layouts.index(
                new_homogeneous_layouts)] =
            context.storage.declarations.range<lispb::IdentifierId>(
                {context.storage.strings.intern("xs"), context.storage.strings.intern("ys")});
        context.storage.declarations.homogeneous_layouts
            .value_types[context.storage.declarations.homogeneous_layouts.index(
                new_homogeneous_layouts)] =
            context.storage.declarations.range<lispb::codegen::HomogeneousValueId>({[&] {
                auto const new_homogeneous_values{
                    context.storage.declarations.homogeneous_values.append()};
                context.storage.declarations.homogeneous_values
                    .type[context.storage.declarations.homogeneous_values.index(
                        new_homogeneous_values)] = [&] {
                    auto const new_type_refs{context.storage.declarations.type_refs.append()};
                    context.storage.declarations.type_refs
                        .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                        context.storage.strings.intern("float");
                    return new_type_refs;
                }();
                context.storage.declarations.homogeneous_values
                    .suffix[context.storage.declarations.homogeneous_values.index(
                        new_homogeneous_values)] = context.storage.strings.add("f");
                context.storage.declarations.homogeneous_values
                    .equivalent_type[context.storage.declarations.homogeneous_values.index(
                        new_homogeneous_values)] = [&] {
                    auto const new_type_refs{context.storage.declarations.type_refs.append()};
                    context.storage.declarations.type_refs
                        .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                        context.storage.strings.intern("int32");
                    return new_type_refs;
                }();
                return new_homogeneous_values;
            }()});
        return new_homogeneous_layouts;
    }();
    module = normal_module(
        context,
        {layout,
         [&] {
             auto const new_records{context.storage.declarations.records.append()};
             context.storage.declarations.records
                 .name[context.storage.declarations.records.index(new_records)] =
                 context.storage.strings.intern("TValuesView");
             context.storage.declarations.records
                 .members[context.storage.declarations.records.index(new_records)] =
                 context.storage.declarations.range<lispb::codegen::RecordMemberId>({[&] {
                     auto const new_record_members{
                         context.storage.declarations.record_members.append()};
                     context.storage.declarations.record_members
                         .name[context.storage.declarations.record_members.index(
                             new_record_members)] = context.storage.strings.intern("value");
                     context.storage.declarations.record_members
                         .type[context.storage.declarations.record_members.index(
                             new_record_members)] = [&] {
                         auto const new_type_refs{context.storage.declarations.type_refs.append()};
                         context.storage.declarations.type_refs
                             .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                             context.storage.strings.intern("int32");
                         return new_type_refs;
                     }();
                     return new_record_members;
                 }()});
             return new_records;
         }()},
        "Mixed.cpp");
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    layout = [&] {
        auto const new_homogeneous_layouts{
            context.storage.declarations.homogeneous_layouts.append()};
        context.storage.declarations.homogeneous_layouts
            .name[context.storage.declarations.homogeneous_layouts.index(new_homogeneous_layouts)] =
            context.storage.strings.intern("Values");
        context.storage.declarations.homogeneous_layouts
            .components[context.storage.declarations.homogeneous_layouts.index(
                new_homogeneous_layouts)] =
            context.storage.declarations.range<lispb::IdentifierId>(
                {context.storage.strings.intern("xs"), context.storage.strings.intern("ys")});
        context.storage.declarations.homogeneous_layouts
            .value_types[context.storage.declarations.homogeneous_layouts.index(
                new_homogeneous_layouts)] =
            context.storage.declarations.range<lispb::codegen::HomogeneousValueId>({[&] {
                auto const new_homogeneous_values{
                    context.storage.declarations.homogeneous_values.append()};
                context.storage.declarations.homogeneous_values
                    .type[context.storage.declarations.homogeneous_values.index(
                        new_homogeneous_values)] = [&] {
                    auto const new_type_refs{context.storage.declarations.type_refs.append()};
                    context.storage.declarations.type_refs
                        .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                        context.storage.strings.intern("float");
                    return new_type_refs;
                }();
                context.storage.declarations.homogeneous_values
                    .suffix[context.storage.declarations.homogeneous_values.index(
                        new_homogeneous_values)] = context.storage.strings.add("f");
                context.storage.declarations.homogeneous_values
                    .equivalent_type[context.storage.declarations.homogeneous_values.index(
                        new_homogeneous_values)] = [&] {
                    auto const new_type_refs{context.storage.declarations.type_refs.append()};
                    context.storage.declarations.type_refs
                        .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                        context.storage.strings.intern("int32");
                    return new_type_refs;
                }();
                return new_homogeneous_values;
            }()});
        return new_homogeneous_layouts;
    }();
    module = normal_module(
        context,
        {layout,
         [&] {
             auto const new_records{context.storage.declarations.records.append()};
             context.storage.declarations.records
                 .name[context.storage.declarations.records.index(new_records)] =
                 context.storage.strings.intern("TValuesEquivalentType");
             context.storage.declarations.records
                 .members[context.storage.declarations.records.index(new_records)] =
                 context.storage.declarations.range<lispb::codegen::RecordMemberId>({[&] {
                     auto const new_record_members{
                         context.storage.declarations.record_members.append()};
                     context.storage.declarations.record_members
                         .name[context.storage.declarations.record_members.index(
                             new_record_members)] = context.storage.strings.intern("value");
                     context.storage.declarations.record_members
                         .type[context.storage.declarations.record_members.index(
                             new_record_members)] = [&] {
                         auto const new_type_refs{context.storage.declarations.type_refs.append()};
                         context.storage.declarations.type_refs
                             .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                             context.storage.strings.intern("int32");
                         return new_type_refs;
                     }();
                     return new_record_members;
                 }()});
             return new_records;
         }()},
        "Mixed.cpp");
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    auto soa = [&] {
        auto const new_soas{context.storage.declarations.soas.append()};
        context.storage.declarations.soas.name[context.storage.declarations.soas.index(new_soas)] =
            context.storage.strings.intern("Data");
        context.storage.declarations.soas
            .members[context.storage.declarations.soas.index(new_soas)] =
            context.storage.declarations.range<lispb::codegen::SoaMemberId>({[&] {
                auto const new_soa_members{context.storage.declarations.soa_members.append()};
                context.storage.declarations.soa_members
                    .name[context.storage.declarations.soa_members.index(new_soa_members)] =
                    context.storage.strings.intern("values");
                context.storage.declarations.soa_members
                    .kind[context.storage.declarations.soa_members.index(new_soa_members)] =
                    SoaMemberKind::array;
                context.storage.declarations.soa_members
                    .type[context.storage.declarations.soa_members.index(new_soa_members)] = [&] {
                    auto const new_type_refs{context.storage.declarations.type_refs.append()};
                    context.storage.declarations.type_refs
                        .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                        context.storage.strings.intern("int32");
                    return new_type_refs;
                }();
                return new_soa_members;
            }()});
        return new_soas;
    }();
    module = normal_module(
        context,
        {soa,
         [&] {
             auto const new_enums{context.storage.declarations.enums.append()};
             context.storage.declarations.enums
                 .name[context.storage.declarations.enums.index(new_enums)] =
                 context.storage.strings.intern("DataView");
             context.storage.declarations.enums
                 .underlying_type[context.storage.declarations.enums.index(new_enums)] = [&] {
                 auto const new_type_refs{context.storage.declarations.type_refs.append()};
                 context.storage.declarations.type_refs
                     .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                     context.storage.strings.intern("uint8");
                 return new_type_refs;
             }();
             context.storage.declarations.enums
                 .values[context.storage.declarations.enums.index(new_enums)] =
                 context.storage.declarations.range<lispb::codegen::EnumeratorId>({[&] {
                     auto const new_enumerators{context.storage.declarations.enumerators.append()};
                     context.storage.declarations.enumerators
                         .name[context.storage.declarations.enumerators.index(new_enumerators)] =
                         context.storage.strings.intern("One");
                     return new_enumerators;
                 }()});
             return new_enums;
         }()},
        "Mixed.cpp");
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = normal_module(
        context,
        {[&] {
             auto const new_enums{context.storage.declarations.enums.append()};
             context.storage.declarations.enums
                 .name[context.storage.declarations.enums.index(new_enums)] =
                 context.storage.strings.intern("State");
             context.storage.declarations.enums
                 .underlying_type[context.storage.declarations.enums.index(new_enums)] = [&] {
                 auto const new_type_refs{context.storage.declarations.type_refs.append()};
                 context.storage.declarations.type_refs
                     .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                     context.storage.strings.intern("uint8");
                 return new_type_refs;
             }();
             context.storage.declarations.enums
                 .values[context.storage.declarations.enums.index(new_enums)] =
                 context.storage.declarations.range<lispb::codegen::EnumeratorId>({[&] {
                     auto const new_enumerators{context.storage.declarations.enumerators.append()};
                     context.storage.declarations.enumerators
                         .name[context.storage.declarations.enumerators.index(new_enumerators)] =
                         context.storage.strings.intern("One");
                     return new_enumerators;
                 }()});
             return new_enums;
         }(),
         [&] {
             auto const new_records{context.storage.declarations.records.append()};
             context.storage.declarations.records
                 .name[context.storage.declarations.records.index(new_records)] =
                 context.storage.strings.intern("Snapshot");
             context.storage.declarations.records
                 .members[context.storage.declarations.records.index(new_records)] =
                 context.storage.declarations.range<lispb::codegen::RecordMemberId>({[&] {
                     auto const new_record_members{
                         context.storage.declarations.record_members.append()};
                     context.storage.declarations.record_members
                         .name[context.storage.declarations.record_members.index(
                             new_record_members)] = context.storage.strings.intern("value");
                     context.storage.declarations.record_members
                         .type[context.storage.declarations.record_members.index(
                             new_record_members)] = [&] {
                         auto const new_type_refs{context.storage.declarations.type_refs.append()};
                         context.storage.declarations.type_refs
                             .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                             context.storage.strings.intern("int32");
                         return new_type_refs;
                     }();
                     return new_record_members;
                 }()});
             return new_records;
         }()});
    EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));
}

TEST(Validation, NormalModuleAppliesSourceRequirementsPerDeclaration) {
    CompilerTestContext context;
    auto facade = [&] {
        auto const new_facades{context.storage.declarations.facades.append()};
        context.storage.declarations.facades
            .name[context.storage.declarations.facades.index(new_facades)] =
            context.storage.strings.intern("InlineFacade");
        context.storage.declarations.facades
            .target_type[context.storage.declarations.facades.index(new_facades)] = [&] {
            auto const new_type_refs{context.storage.declarations.type_refs.append()};
            context.storage.declarations.type_refs
                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                context.storage.strings.intern("int32");
            return new_type_refs;
        }();
        context.storage.declarations.facades
            .target_member_name[context.storage.declarations.facades.index(new_facades)] =
            context.storage.strings.intern("target");
        context.storage.declarations.facades
            .methods[context.storage.declarations.facades.index(new_facades)] =
            context.storage.declarations.range<lispb::codegen::FacadeMethodId>({[&] {
                auto const new_facade_methods{context.storage.declarations.facade_methods.append()};
                context.storage.declarations.facade_methods
                    .name[context.storage.declarations.facade_methods.index(new_facade_methods)] =
                    context.storage.strings.intern("reset");
                context.storage.declarations.facade_methods
                    .return_type[context.storage.declarations.facade_methods.index(
                        new_facade_methods)] = [&] {
                    auto const new_type_refs{context.storage.declarations.type_refs.append()};
                    context.storage.declarations.type_refs
                        .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                        context.storage.strings.intern("void");
                    return new_type_refs;
                }();
                return new_facade_methods;
            }()});
        return new_facades;
    }();
    auto layout = [&] {
        auto const new_homogeneous_layouts{
            context.storage.declarations.homogeneous_layouts.append()};
        context.storage.declarations.homogeneous_layouts
            .name[context.storage.declarations.homogeneous_layouts.index(new_homogeneous_layouts)] =
            context.storage.strings.intern("Values");
        context.storage.declarations.homogeneous_layouts
            .components[context.storage.declarations.homogeneous_layouts.index(
                new_homogeneous_layouts)] =
            context.storage.declarations.range<lispb::IdentifierId>(
                {context.storage.strings.intern("xs"), context.storage.strings.intern("ys")});
        context.storage.declarations.homogeneous_layouts
            .value_types[context.storage.declarations.homogeneous_layouts.index(
                new_homogeneous_layouts)] =
            context.storage.declarations.range<lispb::codegen::HomogeneousValueId>({[&] {
                auto const new_homogeneous_values{
                    context.storage.declarations.homogeneous_values.append()};
                context.storage.declarations.homogeneous_values
                    .type[context.storage.declarations.homogeneous_values.index(
                        new_homogeneous_values)] = [&] {
                    auto const new_type_refs{context.storage.declarations.type_refs.append()};
                    context.storage.declarations.type_refs
                        .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                        context.storage.strings.intern("float");
                    return new_type_refs;
                }();
                context.storage.declarations.homogeneous_values
                    .suffix[context.storage.declarations.homogeneous_values.index(
                        new_homogeneous_values)] = context.storage.strings.add("f");
                return new_homogeneous_values;
            }()});
        return new_homogeneous_layouts;
    }();
    auto module = normal_module(context, {facade, layout}, "Mixed.cpp");
    EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));

    module = normal_module(context, {layout});
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    auto const source_facade{context.storage.declarations.facades.clone(facade)};
    context.storage.declarations.facades
        .definitions_in_source[context.storage.declarations.facades.index(source_facade)] = true;
    module = normal_module(context, {source_facade});
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, NormalModuleCanMixIndependentEnumApis) {
    CompilerTestContext context;
    auto module = normal_module(
        context,
        {[&] {
             auto const new_enums{context.storage.declarations.enums.append()};
             context.storage.declarations.enums
                 .name[context.storage.declarations.enums.index(new_enums)] =
                 context.storage.strings.intern("NativeMode");
             context.storage.declarations.enums
                 .underlying_type[context.storage.declarations.enums.index(new_enums)] = [&] {
                 auto const new_type_refs{context.storage.declarations.type_refs.append()};
                 context.storage.declarations.type_refs
                     .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                     context.storage.strings.intern("uint8");
                 return new_type_refs;
             }();
             context.storage.declarations.enums
                 .values[context.storage.declarations.enums.index(new_enums)] =
                 context.storage.declarations.range<lispb::codegen::EnumeratorId>({[&] {
                     auto const new_enumerators{context.storage.declarations.enumerators.append()};
                     context.storage.declarations.enumerators
                         .name[context.storage.declarations.enumerators.index(new_enumerators)] =
                         context.storage.strings.intern("One");
                     return new_enumerators;
                 }()});
             context.storage.declarations.enums
                 .native_api[context.storage.declarations.enums.index(new_enums)] = true;
             return new_enums;
         }(),
         [&] {
             auto const new_enums{context.storage.declarations.enums.append()};
             context.storage.declarations.enums
                 .name[context.storage.declarations.enums.index(new_enums)] =
                 context.storage.strings.intern("UnrealMode");
             context.storage.declarations.enums
                 .underlying_type[context.storage.declarations.enums.index(new_enums)] = [&] {
                 auto const new_type_refs{context.storage.declarations.type_refs.append()};
                 context.storage.declarations.type_refs
                     .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                     context.storage.strings.intern("uint8");
                 return new_type_refs;
             }();
             context.storage.declarations.enums
                 .values[context.storage.declarations.enums.index(new_enums)] =
                 context.storage.declarations.range<lispb::codegen::EnumeratorId>({[&] {
                     auto const new_enumerators{context.storage.declarations.enumerators.append()};
                     context.storage.declarations.enumerators
                         .name[context.storage.declarations.enumerators.index(new_enumerators)] =
                         context.storage.strings.intern("One");
                     return new_enumerators;
                 }()});
             return new_enums;
         }()});
    EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));
}

TEST(Validation, NormalModuleMatchesSoaAllocatorVariantRestrictions) {
    CompilerTestContext context;
    auto const allocator{[&] {
        auto const new_soa_allocator_variants{
            context.storage.declarations.soa_allocator_variants.append()};
        context.storage.declarations.soa_allocator_variants
            .prefix[context.storage.declarations.soa_allocator_variants.index(
                new_soa_allocator_variants)] = context.storage.strings.intern("Custom");
        context.storage.declarations.soa_allocator_variants
            .allocator[context.storage.declarations.soa_allocator_variants.index(
                new_soa_allocator_variants)] = [&] {
            auto const new_type_refs{context.storage.declarations.type_refs.append()};
            context.storage.declarations.type_refs
                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                context.storage.strings.intern("FAllocator");
            return new_type_refs;
        }();
        return new_soa_allocator_variants;
    }()};
    auto module{
        normal_soa_module(context, plain_soa(context), {allocator}, SoaBackend::standard_library)};
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = normal_soa_module(
        context, plain_soa(context), {[&] {
            auto const new_soa_allocator_variants{
                context.storage.declarations.soa_allocator_variants.append()};
            context.storage.declarations.soa_allocator_variants
                .prefix[context.storage.declarations.soa_allocator_variants.index(
                    new_soa_allocator_variants)] = context.storage.strings.intern("1Bad");
            context.storage.declarations.soa_allocator_variants
                .allocator[context.storage.declarations.soa_allocator_variants.index(
                    new_soa_allocator_variants)] = [&] {
                auto const new_type_refs{context.storage.declarations.type_refs.append()};
                context.storage.declarations.type_refs
                    .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                    context.storage.strings.intern("FAllocator");
                return new_type_refs;
            }();
            return new_soa_allocator_variants;
        }()});
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    auto fixed{plain_soa(context)};
    context.storage.declarations.soas.fixed[context.storage.declarations.soas.index(fixed)] = [&] {
        auto const new_fixed_soas{context.storage.declarations.fixed_soas.append()};
        context.storage.declarations.fixed_soas
            .storage_name[context.storage.declarations.fixed_soas.index(new_fixed_soas)] =
            context.storage.strings.intern("DataStorage");
        context.storage.declarations.fixed_soas
            .containers[context.storage.declarations.fixed_soas.index(new_fixed_soas)] = {};
        return new_fixed_soas;
    }();
    module = normal_soa_module(context, fixed, {allocator});
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    auto equivalent{plain_soa(context)};
    context.storage.declarations.soas
        .equivalent_type[context.storage.declarations.soas.index(equivalent)] = [&] {
        auto const new_type_refs{context.storage.declarations.type_refs.append()};
        context.storage.declarations.type_refs
            .name[context.storage.declarations.type_refs.index(new_type_refs)] =
            context.storage.strings.intern("int32");
        return new_type_refs;
    }();
    module = normal_soa_module(context, equivalent, {allocator});
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    auto functions{plain_soa(context)};
    context.storage.declarations.soas
        .functions[context.storage.declarations.soas.index(functions)] =
        context.storage.declarations.range<lispb::codegen::FunctionSchemaId>({[&] {
            auto const new_functions{context.storage.declarations.functions.append()};
            context.storage.declarations.functions
                .name[context.storage.declarations.functions.index(new_functions)] =
                context.storage.strings.intern("custom");
            context.storage.declarations.functions
                .return_type[context.storage.declarations.functions.index(new_functions)] = [&] {
                auto const new_type_refs{context.storage.declarations.type_refs.append()};
                context.storage.declarations.type_refs
                    .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                    context.storage.strings.intern("void");
                return new_type_refs;
            }();
            return new_functions;
        }()});
    module = normal_soa_module(context, functions, {allocator});
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    auto mutable_functions{plain_soa(context)};
    context.storage.declarations.soas
        .mutable_view_functions[context.storage.declarations.soas.index(mutable_functions)] =
        context.storage.declarations.range<lispb::codegen::FunctionSchemaId>({[&] {
            auto const new_functions{context.storage.declarations.functions.append()};
            context.storage.declarations.functions
                .name[context.storage.declarations.functions.index(new_functions)] =
                context.storage.strings.intern("custom");
            context.storage.declarations.functions
                .return_type[context.storage.declarations.functions.index(new_functions)] = [&] {
                auto const new_type_refs{context.storage.declarations.type_refs.append()};
                context.storage.declarations.type_refs
                    .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                    context.storage.strings.intern("void");
                return new_type_refs;
            }();
            return new_functions;
        }()});
    module = normal_soa_module(context, mutable_functions, {allocator});
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    auto declarations{plain_soa(context)};
    context.storage.declarations.soas
        .using_declarations[context.storage.declarations.soas.index(declarations)] =
        context.storage.declarations.range<lispb::StringId>(
            {context.storage.strings.add("value_type = int32")});
    module = normal_soa_module(context, declarations, {allocator});
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    auto nested{plain_soa(context)};
    context.storage.declarations.soas.members[context.storage.declarations.soas.index(nested)] =
        context.storage.declarations.range<lispb::codegen::SoaMemberId>({[&] {
            auto const new_soa_members{context.storage.declarations.soa_members.append()};
            context.storage.declarations.soa_members
                .name[context.storage.declarations.soa_members.index(new_soa_members)] =
                context.storage.strings.intern("child");
            context.storage.declarations.soa_members
                .kind[context.storage.declarations.soa_members.index(new_soa_members)] =
                SoaMemberKind::nested;
            context.storage.declarations.soa_members
                .type[context.storage.declarations.soa_members.index(new_soa_members)] = [&] {
                auto const new_type_refs{context.storage.declarations.type_refs.append()};
                context.storage.declarations.type_refs
                    .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                    context.storage.strings.intern("int32");
                return new_type_refs;
            }();
            context.storage.declarations.soa_members
                .nested_schema[context.storage.declarations.soa_members.index(new_soa_members)] =
                context.storage.strings.intern("Missing");
            return new_soa_members;
        }()});
    module = normal_soa_module(context, nested, {allocator});
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = [&] {
        auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
        context.storage.declarations.normal_modules
            .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] = [&] {
            auto const new_module_settings_store{
                context.storage.declarations.module_settings_store.append()};
            context.storage.declarations.module_settings_store
                .name[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.intern("mixed");
            context.storage.declarations.module_settings_store
                .header[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.add("Mixed.h");
            context.storage.declarations.module_settings_store
                .source[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.add("Mixed.cpp");
            return new_module_settings_store;
        }();
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(new_normal_modules)] =
            context.storage.declarations.range<lispb::codegen::DeclarationHandle>(
                {plain_soa(context, "Data"), plain_soa(context, "CustomData")});
        context.storage.declarations.normal_modules
            .soa_array_allocators[context.storage.declarations.normal_modules.index(
                new_normal_modules)] =
            context.storage.declarations.range<lispb::codegen::SoaAllocatorVariantId>({allocator});
        return new_normal_modules;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = normal_soa_module(context, plain_soa(context), {allocator});
    EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));
    auto const files{
        render_modules(context.ir, lower_test_manifest(context, manifest_with(context, module)))};
    ASSERT_EQ(files.size(), 2U);
    EXPECT_NE(files.front().content.find("struct CustomData"), std::string::npos);
}

TEST(Validation, FieldMaskSoaCannotUseArrayAllocatorVariants) {
    CompilerTestContext context;
    auto const allocator{[&] {
        auto const new_soa_allocator_variants{
            context.storage.declarations.soa_allocator_variants.append()};
        context.storage.declarations.soa_allocator_variants
            .prefix[context.storage.declarations.soa_allocator_variants.index(
                new_soa_allocator_variants)] = context.storage.strings.intern("Custom");
        context.storage.declarations.soa_allocator_variants
            .allocator[context.storage.declarations.soa_allocator_variants.index(
                new_soa_allocator_variants)] = [&] {
            auto const new_type_refs{context.storage.declarations.type_refs.append()};
            context.storage.declarations.type_refs
                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                context.storage.strings.intern("FAllocator");
            return new_type_refs;
        }();
        return new_soa_allocator_variants;
    }()};
    auto const masked{
        context.storage.declarations
            .items(context.storage.declarations.normal_modules
                       .declarations[context.storage.declarations.normal_modules.index(
                           valid_mask_soa_module(context))])
            .front()};
    auto normal{normal_soa_module(context, masked, {allocator})};
    auto normal_manifest{manifest_with(context, normal)};
    EXPECT_THROW(lispb::schema::resolve_type_graph(normal_manifest), std::invalid_argument);
    EXPECT_THROW(lower_test_manifest(context, normal_manifest), std::invalid_argument);

    auto legacy{valid_mask_soa_module(context)};
    context.storage.declarations.normal_modules
        .soa_array_allocators[context.storage.declarations.normal_modules.index(legacy)] =
        context.storage.declarations.range<lispb::codegen::SoaAllocatorVariantId>({allocator});
    auto legacy_manifest{manifest_with(context, legacy)};
    EXPECT_THROW(lispb::schema::resolve_type_graph(legacy_manifest), std::invalid_argument);
}

TEST(Validation, NormalModuleReservesFixedSoaStorageAndContainerNames) {
    CompilerTestContext context;
    auto fixed{plain_soa(context)};
    context.storage.declarations.soas.fixed[context.storage.declarations.soas.index(fixed)] = [&] {
        auto const new_fixed_soas{context.storage.declarations.fixed_soas.append()};
        context.storage.declarations.fixed_soas
            .storage_name[context.storage.declarations.fixed_soas.index(new_fixed_soas)] =
            context.storage.strings.intern("DataStorage");
        context.storage.declarations.fixed_soas
            .containers[context.storage.declarations.fixed_soas.index(new_fixed_soas)] =
            context.storage.declarations.range<lispb::IdentifierId>(
                {context.storage.strings.intern("FixedData")});
        return new_fixed_soas;
    }();
    auto module = normal_module(
        context,
        {fixed,
         [&] {
             auto const new_records{context.storage.declarations.records.append()};
             context.storage.declarations.records
                 .name[context.storage.declarations.records.index(new_records)] =
                 context.storage.strings.intern("DataStorage");
             context.storage.declarations.records
                 .members[context.storage.declarations.records.index(new_records)] =
                 context.storage.declarations.range<lispb::codegen::RecordMemberId>({[&] {
                     auto const new_record_members{
                         context.storage.declarations.record_members.append()};
                     context.storage.declarations.record_members
                         .name[context.storage.declarations.record_members.index(
                             new_record_members)] = context.storage.strings.intern("value");
                     context.storage.declarations.record_members
                         .type[context.storage.declarations.record_members.index(
                             new_record_members)] = [&] {
                         auto const new_type_refs{context.storage.declarations.type_refs.append()};
                         context.storage.declarations.type_refs
                             .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                             context.storage.strings.intern("int32");
                         return new_type_refs;
                     }();
                     return new_record_members;
                 }()});
             return new_records;
         }()},
        "Mixed.cpp");
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = normal_module(
        context,
        {fixed,
         [&] {
             auto const new_records{context.storage.declarations.records.append()};
             context.storage.declarations.records
                 .name[context.storage.declarations.records.index(new_records)] =
                 context.storage.strings.intern("FixedData");
             context.storage.declarations.records
                 .members[context.storage.declarations.records.index(new_records)] =
                 context.storage.declarations.range<lispb::codegen::RecordMemberId>({[&] {
                     auto const new_record_members{
                         context.storage.declarations.record_members.append()};
                     context.storage.declarations.record_members
                         .name[context.storage.declarations.record_members.index(
                             new_record_members)] = context.storage.strings.intern("value");
                     context.storage.declarations.record_members
                         .type[context.storage.declarations.record_members.index(
                             new_record_members)] = [&] {
                         auto const new_type_refs{context.storage.declarations.type_refs.append()};
                         context.storage.declarations.type_refs
                             .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                             context.storage.strings.intern("int32");
                         return new_type_refs;
                     }();
                     return new_record_members;
                 }()});
             return new_records;
         }()},
        "Mixed.cpp");
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    auto vector = [&] {
        auto const new_vector_soas{context.storage.declarations.vector_soas.append()};
        context.storage.declarations.vector_soas
            .name[context.storage.declarations.vector_soas.index(new_vector_soas)] =
            context.storage.strings.intern("Vectors");
        context.storage.declarations.vector_soas
            .value_type[context.storage.declarations.vector_soas.index(new_vector_soas)] = [&] {
            auto const new_type_refs{context.storage.declarations.type_refs.append()};
            context.storage.declarations.type_refs
                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                context.storage.strings.intern("float");
            return new_type_refs;
        }();
        context.storage.declarations.vector_soas
            .components[context.storage.declarations.vector_soas.index(new_vector_soas)] =
            context.storage.declarations.range<lispb::IdentifierId>(
                {context.storage.strings.intern("xs"), context.storage.strings.intern("ys")});
        context.storage.declarations.vector_soas
            .equivalent_type[context.storage.declarations.vector_soas.index(new_vector_soas)] =
            [&] {
                auto const new_type_refs{context.storage.declarations.type_refs.append()};
                context.storage.declarations.type_refs
                    .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                    context.storage.strings.intern("int32");
                return new_type_refs;
            }();
        context.storage.declarations.vector_soas
            .fixed[context.storage.declarations.vector_soas.index(new_vector_soas)] = [&] {
            auto const new_fixed_soas{context.storage.declarations.fixed_soas.append()};
            context.storage.declarations.fixed_soas
                .storage_name[context.storage.declarations.fixed_soas.index(new_fixed_soas)] =
                context.storage.strings.intern("VectorStorage");
            context.storage.declarations.fixed_soas
                .containers[context.storage.declarations.fixed_soas.index(new_fixed_soas)] =
                context.storage.declarations.range<lispb::IdentifierId>(
                    {context.storage.strings.intern("FixedVectors")});
            return new_fixed_soas;
        }();
        return new_vector_soas;
    }();
    module = normal_module(
        context,
        {vector,
         [&] {
             auto const new_records{context.storage.declarations.records.append()};
             context.storage.declarations.records
                 .name[context.storage.declarations.records.index(new_records)] =
                 context.storage.strings.intern("FixedVectors");
             context.storage.declarations.records
                 .members[context.storage.declarations.records.index(new_records)] =
                 context.storage.declarations.range<lispb::codegen::RecordMemberId>({[&] {
                     auto const new_record_members{
                         context.storage.declarations.record_members.append()};
                     context.storage.declarations.record_members
                         .name[context.storage.declarations.record_members.index(
                             new_record_members)] = context.storage.strings.intern("value");
                     context.storage.declarations.record_members
                         .type[context.storage.declarations.record_members.index(
                             new_record_members)] = [&] {
                         auto const new_type_refs{context.storage.declarations.type_refs.append()};
                         context.storage.declarations.type_refs
                             .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                             context.storage.strings.intern("int32");
                         return new_type_refs;
                     }();
                     return new_record_members;
                 }()});
             return new_records;
         }()},
        "Mixed.cpp");
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    auto valid{plain_soa(context)};
    context.storage.declarations.soas.fixed[context.storage.declarations.soas.index(valid)] = [&] {
        auto const new_fixed_soas{context.storage.declarations.fixed_soas.append()};
        context.storage.declarations.fixed_soas
            .storage_name[context.storage.declarations.fixed_soas.index(new_fixed_soas)] =
            context.storage.strings.intern("DataStorage");
        context.storage.declarations.fixed_soas
            .containers[context.storage.declarations.fixed_soas.index(new_fixed_soas)] =
            context.storage.declarations.range<lispb::IdentifierId>(
                {context.storage.strings.intern("FixedData")});
        return new_fixed_soas;
    }();
    module = normal_module(
        context,
        {valid,
         [&] {
             auto const new_records{context.storage.declarations.records.append()};
             context.storage.declarations.records
                 .name[context.storage.declarations.records.index(new_records)] =
                 context.storage.strings.intern("Snapshot");
             context.storage.declarations.records
                 .members[context.storage.declarations.records.index(new_records)] =
                 context.storage.declarations.range<lispb::codegen::RecordMemberId>({[&] {
                     auto const new_record_members{
                         context.storage.declarations.record_members.append()};
                     context.storage.declarations.record_members
                         .name[context.storage.declarations.record_members.index(
                             new_record_members)] = context.storage.strings.intern("value");
                     context.storage.declarations.record_members
                         .type[context.storage.declarations.record_members.index(
                             new_record_members)] = [&] {
                         auto const new_type_refs{context.storage.declarations.type_refs.append()};
                         context.storage.declarations.type_refs
                             .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                             context.storage.strings.intern("int32");
                         return new_type_refs;
                     }();
                     return new_record_members;
                 }()});
             return new_records;
         }()},
        "Mixed.cpp");
    EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));
}

TEST(Validation, RejectsEmptySettingsSchema) {
    CompilerTestContext context;
    auto manifest{parse_test_manifest(context, R"(
(module settings :header "Settings.h"
  (settings-schema Empty
    :target Empty
    :target-header "Empty.h")))")};

    EXPECT_THROW(lower_test_manifest(context, std::move(manifest)), std::invalid_argument);
}

} // namespace
} // namespace lispb::codegen
