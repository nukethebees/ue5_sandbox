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
#include <codegen/schema/function_schemas.h>
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
    return parse_test_manifest(context, R"(
(module soa :header "Soa.h" :source "Soa.cpp"
  (struct FData (member values array int32)))
)")
        .modules.front();
}

TEST(Validation, FunctionQualifiersAreSharedByRecordsAndAllSoaReceivers) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& functions_schemas{declarations.functions};
    auto& type_refs{declarations.type_refs};
    auto& normal_modules{declarations.normal_modules};
    auto& records{declarations.records};
    auto& soas{declarations.soas};
    auto& strings{context.storage.strings};
    auto const valid{functions_schemas.append()};
    auto const valid_row{functions_schemas.index(valid)};
    functions_schemas.name[valid_row] = strings.intern("read");
    functions_schemas.return_type[valid_row] = make_test_type_ref(context, "int");
    functions_schemas.body_lines[valid_row] =
        declarations.range<lispb::StringId>({strings.intern("return 7;")});
    functions_schemas.is_const[valid_row] = true;
    functions_schemas.is_constexpr[valid_row] = true;
    functions_schemas.is_nodiscard[valid_row] = true;
    auto check = [&](lispb::codegen::FunctionSchemaId const& function,
                     std::string_view const diagnostic) {
        for (int receiver{}; receiver < 4; ++receiver) {
            SCOPED_TRACE(receiver);
            SCOPED_TRACE(diagnostic);
            auto module{valid_soa_module(context)};
            if (receiver == 0) {
                normal_modules.declarations[normal_modules.index(module)] =
                    declarations.range<lispb::codegen::DeclarationHandle>({[&] {
                        auto const new_records{records.append()};
                        auto const new_records_row{records.index(new_records)};
                        records.name[new_records_row] = strings.intern("Record");
                        records.functions[new_records_row] =
                            declarations.range<lispb::codegen::FunctionSchemaId>({function});
                        return new_records;
                    }()});
            } else {

                normal_modules.declarations[normal_modules.index(module)] = [&] {
                    auto const original_items{declarations.items(
                        normal_modules.declarations[normal_modules.index(module)])};
                    auto rows{std::vector(original_items.begin(), original_items.end())};
                    rows.front() = [&] {
                        auto soa{soas.clone(rows.front())};
                        auto const soa_row{soas.index(soa)};
                        auto& functions{receiver == 1   ? soas.functions[soa_row]
                                        : receiver == 2 ? soas.const_view_functions[soa_row]
                                                        : soas.mutable_view_functions[soa_row]};
                        functions = declarations.range<FunctionSchemaId>({function});
                        return soa;
                    }();
                    return declarations.range(rows);
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
    auto function{functions_schemas.clone(valid)};
    functions_schemas.body_lines[functions_schemas.index(function)] = {};
    check(function, "constexpr function requires a generated header body");
    function = functions_schemas.clone(valid);
    functions_schemas.definition_in_source[functions_schemas.index(function)] = true;
    check(function, "constexpr function requires a generated header body");
    function = functions_schemas.clone(valid);
    functions_schemas.return_type[functions_schemas.index(function)] =
        make_test_type_ref(context, "void");
    functions_schemas.body_lines[functions_schemas.index(function)] =
        declarations.range<lispb::StringId>({strings.intern("return;")});
    check(function, "nodiscard function must not return void");
    functions_schemas.return_type[functions_schemas.index(function)] =
        make_test_type_ref(context, "auto");
    functions_schemas.trailing_return_type[functions_schemas.index(function)] =
        make_test_type_ref(context, "void");
    check(function, "nodiscard function must not return void");
    function = functions_schemas.clone(valid);
    functions_schemas.is_static[functions_schemas.index(function)] = true;
    check(function, "must not be both static and const");
    function = functions_schemas.clone(valid);
    functions_schemas.is_constexpr[functions_schemas.index(function)] = false;
    functions_schemas.is_inline[functions_schemas.index(function)] = true;
    functions_schemas.definition_in_source[functions_schemas.index(function)] = true;
    check(function, "must not be both inline and defined in the source");
    functions_schemas.is_inline[functions_schemas.index(function)] = false;
    functions_schemas.template_parameters[functions_schemas.index(function)] =
        strings.intern("typename T");
    check(function, "function templates must be defined in the header");

    function = functions_schemas.clone(valid);
    functions_schemas.return_type[functions_schemas.index(function)] = [&] {
        auto const new_type_refs{type_refs.append()};
        auto const new_type_refs_row{type_refs.index(new_type_refs)};
        type_refs.name[new_type_refs_row] = strings.intern("void");
        type_refs.suffix[new_type_refs_row] = strings.intern("*");
        return new_type_refs;
    }();
    functions_schemas.body_lines[functions_schemas.index(function)] =
        declarations.range<lispb::StringId>({strings.intern("return nullptr;")});
    check(function, "");
    functions_schemas.return_type[functions_schemas.index(function)] =
        make_test_type_ref(context, "auto");
    functions_schemas.trailing_return_type[functions_schemas.index(function)] = [&] {
        auto const new_type_refs{type_refs.append()};
        auto const new_type_refs_row{type_refs.index(new_type_refs)};
        type_refs.name[new_type_refs_row] = strings.intern("void");
        type_refs.suffix[new_type_refs_row] = strings.intern("*");
        return new_type_refs;
    }();
    check(function, "");
    functions_schemas.is_constexpr[functions_schemas.index(function)] = false;
    functions_schemas.body_lines[functions_schemas.index(function)] = {};
    check(function, "");
}

auto valid_mask_soa_module(CompilerTestContext& context) -> lispb::codegen::ModuleHandle {
    return parse_test_manifest(context, R"(
(module soa :header "Soa.h" :source "Soa.cpp"
  (struct FData :field-mask-name FFieldMask :field-enum-name EField
    (member masks array FFieldMask)
    (member values array int32 :mask-field true)))
)")
        .modules.front();
}

auto valid_facade_module(CompilerTestContext& context) -> lispb::codegen::ModuleHandle {
    return parse_test_manifest(context, R"(
(module facade :header "Facade.h"
  (facade FFacade FTarget target
    (method reset void)))
)")
        .modules.front();
}

auto normal_module(CompilerTestContext& context,
                   std::vector<DeclarationHandle> const& declarations,
                   std::optional<std::string_view> source = std::nullopt)
    -> lispb::codegen::ModuleHandle {
    auto& strings{context.storage.strings};
    auto& modules{context.storage.declarations.normal_modules};
    auto& settings{context.storage.declarations.module_settings_store};
    auto const module{modules.append()};
    auto const module_row{modules.index(module)};
    auto const config{settings.append()};
    auto const settings_row{settings.index(config)};
    settings.name[settings_row] = strings.intern("mixed");
    settings.header[settings_row] = strings.intern("Mixed.h");
    settings.source[settings_row] = source ? std::optional{strings.intern(*source)} : std::nullopt;
    modules.settings[module_row] = config;
    modules.declarations[module_row] = context.storage.declarations.range(declarations);
    return module;
}

auto plain_soa(CompilerTestContext& context, std::string name = "Data")
    -> lispb::codegen::DeclarationHandle {
    auto& declarations{context.storage.declarations};
    auto& soas{declarations.soas};
    auto& soa_members{declarations.soa_members};
    auto& strings{context.storage.strings};
    return [&] {
        auto const new_soas{soas.append()};
        auto const new_soas_row{soas.index(new_soas)};
        soas.name[new_soas_row] = strings.intern(std::move(name));
        soas.members[new_soas_row] = declarations.range<lispb::codegen::SoaMemberId>({[&] {
            auto const new_soa_members{soa_members.append()};
            auto const new_soa_members_row{soa_members.index(new_soa_members)};
            soa_members.name[new_soa_members_row] = strings.intern("values");
            soa_members.kind[new_soa_members_row] = SoaMemberKind::array;
            soa_members.type[new_soa_members_row] = make_test_type_ref(context, "int32");
            return new_soa_members;
        }()});
        return new_soas;
    }();
}

auto valid_enum_module(CompilerTestContext& context) -> lispb::codegen::ModuleHandle {
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& module_settings_store{declarations.module_settings_store};
    auto& enums{declarations.enums};
    auto& strings{context.storage.strings};
    return [&] {
        auto const new_normal_modules{normal_modules.append()};
        auto const new_normal_modules_row{normal_modules.index(new_normal_modules)};
        normal_modules.settings[new_normal_modules_row] = [&] {
            auto const new_module_settings_store{module_settings_store.append()};
            auto const new_module_settings_store_row{
                module_settings_store.index(new_module_settings_store)};
            module_settings_store.name[new_module_settings_store_row] = strings.intern("enums");
            module_settings_store.header[new_module_settings_store_row] = strings.intern("Enums.h");
            module_settings_store.source[new_module_settings_store_row] =
                strings.intern("Enums.cpp");
            return new_module_settings_store;
        }();
        normal_modules.declarations[new_normal_modules_row] =
            declarations.range<lispb::codegen::DeclarationHandle>({[&] {
                auto const new_enums{enums.append()};
                auto const new_enums_row{enums.index(new_enums)};
                enums.name[new_enums_row] = strings.intern("EMode");
                enums.underlying_type[new_enums_row] = make_test_type_ref(context, "uint8");
                enums
                    .values[new_enums_row] = declarations.range<lispb::codegen::EnumeratorId>({[&] {
                    auto const new_enumerators{declarations.enumerators.append()};
                    declarations.enumerators.name[declarations.enumerators.index(new_enumerators)] =
                        strings.intern("Value");
                    return new_enumerators;
                }()});
                enums.conversions[new_enums_row] =
                    declarations.range<lispb::codegen::EnumConversion>(
                        {EnumConversion::string_view});
                enums.reflection[new_enums_row] = EnumReflection::uenum;
                return new_enums;
            }()});
        normal_modules.enum_helper_namespace[new_normal_modules_row] = strings.intern("project");
        return new_normal_modules;
    }();
}

auto valid_native_enum_module(CompilerTestContext& context) -> lispb::codegen::ModuleHandle {
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& module_settings_store{declarations.module_settings_store};
    auto& enums{declarations.enums};
    auto& enumerators{declarations.enumerators};
    auto& strings{context.storage.strings};
    return [&] {
        auto const new_normal_modules{normal_modules.append()};
        auto const new_normal_modules_row{normal_modules.index(new_normal_modules)};
        normal_modules.settings[new_normal_modules_row] = [&] {
            auto const new_module_settings_store{module_settings_store.append()};
            auto const new_module_settings_store_row{
                module_settings_store.index(new_module_settings_store)};
            module_settings_store.name[new_module_settings_store_row] =
                strings.intern("native_enums");
            module_settings_store.header[new_module_settings_store_row] =
                strings.intern("NativeEnums.h");
            module_settings_store.namespace_name[new_module_settings_store_row] =
                strings.intern("project");
            return new_module_settings_store;
        }();
        normal_modules.declarations[new_normal_modules_row] =
            declarations.range<lispb::codegen::DeclarationHandle>({[&] {
                auto const new_enums{enums.append()};
                auto const new_enums_row{enums.index(new_enums)};
                enums.name[new_enums_row] = strings.intern("NativeMode");
                enums.underlying_type[new_enums_row] = make_test_type_ref(context, "std::uint8_t");
                enums.values[new_enums_row] =
                    declarations.range<lispb::codegen::EnumeratorId>({[&] {
                        auto const new_enumerators{enumerators.append()};
                        auto const new_enumerators_row{enumerators.index(new_enumerators)};
                        enumerators.name[new_enumerators_row] = strings.intern("Idle");
                        enumerators.initializer[new_enumerators_row] = strings.intern("0");
                        enumerators.display_name[new_enumerators_row] = std::nullopt;
                        enumerators.hidden[new_enumerators_row] = false;
                        enumerators.serialized_name[new_enumerators_row] = strings.intern("idle");
                        return new_enumerators;
                    }()});
                enums.native_api[new_enums_row] = true;
                return new_enums;
            }()});
        return new_normal_modules;
    }();
}

auto valid_integer_scalar_module(CompilerTestContext& context) -> lispb::codegen::ModuleHandle {
    return parse_test_manifest(context, R"(
(module scalars :header "Scalars.h"
  (integer-scalar DamageReason :signed false :minimum 0 :maximum 10
    (code Unknown :value 0)
    (code Invalid :value 15 :sentinel true)))
)")
        .modules.front();
}

auto valid_linear_quantized_manifest(CompilerTestContext& context) -> Manifest {
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& module_settings_store{declarations.module_settings_store};
    auto& linear_quantizeds{declarations.linear_quantizeds};
    auto& strings{context.storage.strings};
    auto scalar{valid_integer_scalar_module(context)};
    auto const representations{normal_modules.append()};
    auto const representations_row{normal_modules.index(representations)};
    normal_modules.settings[representations_row] = [&] {
        auto const new_module_settings_store{module_settings_store.append()};
        auto const new_module_settings_store_row{
            module_settings_store.index(new_module_settings_store)};
        module_settings_store.name[new_module_settings_store_row] =
            strings.intern("representations");
        module_settings_store.header[new_module_settings_store_row] =
            strings.intern("Representations.h");
        return new_module_settings_store;
    }();
    normal_modules.declarations[representations_row] =
        declarations.range<lispb::codegen::DeclarationHandle>({[&] {
            auto const new_linear_quantizeds{linear_quantizeds.append()};
            auto const new_linear_quantizeds_row{linear_quantizeds.index(new_linear_quantizeds)};
            linear_quantizeds.name[new_linear_quantizeds_row] = strings.intern("DamageReasonQ4");
            linear_quantizeds.source[new_linear_quantizeds_row] =
                make_test_type_ref(context, "DamageReason");
            linear_quantizeds.bit_width[new_linear_quantizeds_row] = 4;
            linear_quantizeds.reserved_codes[new_linear_quantizeds_row] = 1;
            linear_quantizeds.clipping[new_linear_quantizeds_row] = QuantizationClipping::reject;
            return new_linear_quantizeds;
        }()});
    return lispb::codegen::Manifest{manifest_schema_version,
                                    make_test_registry(context),
                                    declarations,
                                    {scalar, representations}};
}

auto valid_integer_varint_manifest(
    CompilerTestContext& context,
    bool const signedness = false,
    IntegerVarintEncoding const encoding = IntegerVarintEncoding::unsigned_varint) -> Manifest {
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& integer_scalars{declarations.integer_scalars};
    auto& module_settings_store{declarations.module_settings_store};
    auto& integer_varints{declarations.integer_varints};
    auto& strings{context.storage.strings};
    auto scalar{valid_integer_scalar_module(context)};
    if (signedness) {
        normal_modules.declarations[normal_modules.index(scalar)] = [&] {
            auto const original_items{
                declarations.items(normal_modules.declarations[normal_modules.index(scalar)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{integer_scalars.clone(rows_1.front())};
                integer_scalars.signedness[integer_scalars.index(row_0)] = true;
                return row_0;
            }();
            return declarations.range(rows_1);
        }();
        normal_modules.declarations[normal_modules.index(scalar)] = [&] {
            auto const original_items{
                declarations.items(normal_modules.declarations[normal_modules.index(scalar)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{integer_scalars.clone(rows_1.front())};
                integer_scalars.minimum_value[integer_scalars.index(row_0)] = -100;
                return row_0;
            }();
            return declarations.range(rows_1);
        }();
        normal_modules.declarations[normal_modules.index(scalar)] = [&] {
            auto const original_items{
                declarations.items(normal_modules.declarations[normal_modules.index(scalar)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{integer_scalars.clone(rows_1.front())};
                integer_scalars.maximum_value[integer_scalars.index(row_0)] = 100;
                return row_0;
            }();
            return declarations.range(rows_1);
        }();
        normal_modules.declarations[normal_modules.index(scalar)] = [&] {
            auto const original_items{
                declarations.items(normal_modules.declarations[normal_modules.index(scalar)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{integer_scalars.clone(rows_1.front())};
                integer_scalars.named_codes[integer_scalars.index(row_0)] = {};
                return row_0;
            }();
            return declarations.range(rows_1);
        }();
    }
    auto const representations{normal_modules.append()};
    auto const representations_row{normal_modules.index(representations)};
    normal_modules.settings[representations_row] = [&] {
        auto const new_module_settings_store{module_settings_store.append()};
        auto const new_module_settings_store_row{
            module_settings_store.index(new_module_settings_store)};
        module_settings_store.name[new_module_settings_store_row] =
            strings.intern("representations");
        module_settings_store.header[new_module_settings_store_row] =
            strings.intern("Representations.h");
        return new_module_settings_store;
    }();
    normal_modules.declarations[representations_row] =
        declarations.range<lispb::codegen::DeclarationHandle>({[&] {
            auto const new_integer_varints{integer_varints.append()};
            auto const new_integer_varints_row{integer_varints.index(new_integer_varints)};
            integer_varints.name[new_integer_varints_row] = strings.intern("DamageReasonVarint");
            integer_varints.source[new_integer_varints_row] =
                make_test_type_ref(context, "DamageReason");
            integer_varints.encoding[new_integer_varints_row] = encoding;
            return new_integer_varints;
        }()});
    return lispb::codegen::Manifest{manifest_schema_version,
                                    make_test_registry(context),
                                    declarations,
                                    {scalar, representations}};
}

auto valid_fixed_point_manifest(CompilerTestContext& context, bool const signedness = true)
    -> Manifest {
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& module_settings_store{declarations.module_settings_store};
    auto& fixed_points{declarations.fixed_points};
    auto& strings{context.storage.strings};
    auto const representations{normal_modules.append()};
    auto const representations_row{normal_modules.index(representations)};
    normal_modules.settings[representations_row] = [&] {
        auto const new_module_settings_store{module_settings_store.append()};
        auto const new_module_settings_store_row{
            module_settings_store.index(new_module_settings_store)};
        module_settings_store.name[new_module_settings_store_row] =
            strings.intern("representations");
        module_settings_store.header[new_module_settings_store_row] =
            strings.intern("Representations.h");
        return new_module_settings_store;
    }();
    normal_modules.declarations[representations_row] =
        declarations.range<lispb::codegen::DeclarationHandle>({[&] {
            auto const new_fixed_points{fixed_points.append()};
            auto const new_fixed_points_row{fixed_points.index(new_fixed_points)};
            fixed_points.name[new_fixed_points_row] = strings.intern("VelocityQ12_4");
            fixed_points.signedness[new_fixed_points_row] = signedness;
            fixed_points.total_bits[new_fixed_points_row] = 16;
            fixed_points.fractional_bits[new_fixed_points_row] = 4;
            fixed_points.rounding[new_fixed_points_row] = FixedPointRounding::nearest_even;
            return new_fixed_points;
        }()});
    return lispb::codegen::Manifest{
        manifest_schema_version, make_test_registry(context), declarations, {representations}};
}

auto valid_mini_float_manifest(CompilerTestContext& context) -> Manifest {
    return parse_test_manifest(context, R"(
(module representations :header "Representations.h"
  (mini-float CompactFloat :sign-bits 1 :exponent-bits 5 :significand-bits 10 :bias 15))
)");
}

auto valid_optional_sentinel_manifest(CompilerTestContext& context) -> Manifest {
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& module_settings_store{declarations.module_settings_store};
    auto& optional_sentinels{declarations.optional_sentinels};
    auto& strings{context.storage.strings};
    auto scalar{valid_integer_scalar_module(context)};
    auto const representations{normal_modules.append()};
    auto const representations_row{normal_modules.index(representations)};
    normal_modules.settings[representations_row] = [&] {
        auto const new_module_settings_store{module_settings_store.append()};
        auto const new_module_settings_store_row{
            module_settings_store.index(new_module_settings_store)};
        module_settings_store.name[new_module_settings_store_row] =
            strings.intern("representations");
        module_settings_store.header[new_module_settings_store_row] =
            strings.intern("Representations.h");
        return new_module_settings_store;
    }();
    normal_modules.declarations[representations_row] =
        declarations.range<lispb::codegen::DeclarationHandle>({[&] {
            auto const new_optional_sentinels{optional_sentinels.append()};
            auto const new_optional_sentinels_row{optional_sentinels.index(new_optional_sentinels)};
            optional_sentinels.name[new_optional_sentinels_row] =
                strings.intern("OptionalDamageReason");
            optional_sentinels.source[new_optional_sentinels_row] =
                make_test_type_ref(context, "DamageReason");
            optional_sentinels.sentinel[new_optional_sentinels_row] = strings.intern("Invalid");
            return new_optional_sentinels;
        }()});
    return lispb::codegen::Manifest{manifest_schema_version,
                                    make_test_registry(context),
                                    declarations,
                                    {scalar, representations}};
}

auto valid_optional_presence_bit_manifest(CompilerTestContext& context) -> Manifest {
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& module_settings_store{declarations.module_settings_store};
    auto& optional_presence_bits{declarations.optional_presence_bits};
    auto& strings{context.storage.strings};
    auto scalar{valid_integer_scalar_module(context)};
    auto const representations{normal_modules.append()};
    auto const representations_row{normal_modules.index(representations)};
    normal_modules.settings[representations_row] = [&] {
        auto const new_module_settings_store{module_settings_store.append()};
        auto const new_module_settings_store_row{
            module_settings_store.index(new_module_settings_store)};
        module_settings_store.name[new_module_settings_store_row] =
            strings.intern("representations");
        module_settings_store.header[new_module_settings_store_row] =
            strings.intern("Representations.h");
        return new_module_settings_store;
    }();
    normal_modules.declarations[representations_row] =
        declarations.range<lispb::codegen::DeclarationHandle>({[&] {
            auto const new_optional_presence_bits{optional_presence_bits.append()};
            auto const new_optional_presence_bits_row{
                optional_presence_bits.index(new_optional_presence_bits)};
            optional_presence_bits.name[new_optional_presence_bits_row] =
                strings.intern("PresentDamageReason");
            optional_presence_bits.source[new_optional_presence_bits_row] =
                make_test_type_ref(context, "DamageReason");
            return new_optional_presence_bits;
        }()});
    return lispb::codegen::Manifest{manifest_schema_version,
                                    make_test_registry(context),
                                    declarations,
                                    {scalar, representations}};
}

auto valid_static_table_module(CompilerTestContext& context) -> lispb::codegen::ModuleHandle {
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& module_settings_store{declarations.module_settings_store};
    auto& static_tables{declarations.static_tables};
    auto& static_table_rows{declarations.static_table_rows};
    auto& static_table_columns{declarations.static_table_columns};
    auto& strings{context.storage.strings};
    return [&] {
        auto const new_normal_modules{normal_modules.append()};
        auto const new_normal_modules_row{normal_modules.index(new_normal_modules)};
        normal_modules.settings[new_normal_modules_row] = [&] {
            auto const new_module_settings_store{module_settings_store.append()};
            auto const new_module_settings_store_row{
                module_settings_store.index(new_module_settings_store)};
            module_settings_store.name[new_module_settings_store_row] = strings.intern("tables");
            module_settings_store.header[new_module_settings_store_row] =
                strings.intern("Tables.h");
            return new_module_settings_store;
        }();
        normal_modules.declarations[new_normal_modules_row] = declarations.range<
            lispb::codegen::DeclarationHandle>({[&] {
            auto const new_static_tables{static_tables.append()};
            auto const new_static_tables_row{static_tables.index(new_static_tables)};
            static_tables.name[new_static_tables_row] = strings.intern("FValues");
            static_tables.rows[new_static_tables_row] =
                declarations.range<lispb::codegen::StaticTableRowId>(
                    {[&] {
                         auto const new_static_table_rows{static_table_rows.append()};
                         static_table_rows.name[static_table_rows.index(new_static_table_rows)] =
                             strings.intern("first");
                         return new_static_table_rows;
                     }(),
                     [&] {
                         auto const new_static_table_rows{static_table_rows.append()};
                         static_table_rows.name[static_table_rows.index(new_static_table_rows)] =
                             strings.intern("second");
                         return new_static_table_rows;
                     }()});
            static_tables.columns[new_static_tables_row] =
                declarations.range<lispb::codegen::StaticTableColumnId>({[&] {
                    auto const new_static_table_columns{static_table_columns.append()};
                    auto const new_static_table_columns_row{
                        static_table_columns.index(new_static_table_columns)};
                    static_table_columns.name[new_static_table_columns_row] = strings.intern("ids");
                    static_table_columns.type[new_static_table_columns_row] =
                        make_test_type_ref(context, "int32");
                    return new_static_table_columns;
                }()});
            return new_static_tables;
        }()});
        return new_normal_modules;
    }();
}

TEST(Validation, IntegerAliasRequiresAUsableRepresentationButNoNamedCodes) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& integer_scalars{declarations.integer_scalars};

    auto& normal_modules{declarations.normal_modules};
    auto& module_settings_store{declarations.module_settings_store};
    auto& strings{context.storage.strings};
    auto const scalar{integer_scalars.append()};
    auto const scalar_row{integer_scalars.index(scalar)};
    integer_scalars.name[scalar_row] = strings.intern("Health");
    integer_scalars.minimum_value[scalar_row] = 0;
    integer_scalars.maximum_value[scalar_row] = 65535;
    integer_scalars.bit_width[scalar_row] = 16;
    integer_scalars.cpp_emission[scalar_row] = IntegerScalarCppEmission::alias;
    integer_scalars.cpp_type[scalar_row] = make_test_type_ref(context, "std::uint16_t");
    auto make_manifest = [&] {
        return manifest_with(context, [&] {
            auto const new_normal_modules{normal_modules.append()};
            auto const new_normal_modules_row{normal_modules.index(new_normal_modules)};
            normal_modules.settings[new_normal_modules_row] = [&] {
                auto const new_module_settings_store{module_settings_store.append()};
                auto const new_module_settings_store_row{
                    module_settings_store.index(new_module_settings_store)};
                module_settings_store.name[new_module_settings_store_row] =
                    strings.intern("health");
                module_settings_store.header[new_module_settings_store_row] =
                    strings.intern("Health.h");
                return new_module_settings_store;
            }();
            normal_modules.declarations[new_normal_modules_row] =
                declarations.range<lispb::codegen::DeclarationHandle>({scalar});
            return new_normal_modules;
        }());
    };
    EXPECT_NO_THROW(lispb::schema::resolve_type_graph(make_manifest()));
    for (auto const* representation : {"", "@missing", "float", "std::uint8_t", "std::uint16_t*"}) {
        integer_scalars.cpp_type[scalar_row] = make_test_type_ref(context, representation);
        EXPECT_THROW(lispb::schema::resolve_type_graph(make_manifest()), std::invalid_argument)
            << representation;
    }
    integer_scalars.cpp_type[scalar_row].reset();
    EXPECT_THROW(lispb::schema::resolve_type_graph(make_manifest()), std::invalid_argument);
    integer_scalars.cpp_type[scalar_row] = make_test_type_ref(context, "std::uint16_t");
    integer_scalars.minimum_value[scalar_row] = -1;
    EXPECT_THROW(lispb::schema::resolve_type_graph(make_manifest()), std::invalid_argument);
    integer_scalars.minimum_value[scalar_row] = 0;
    integer_scalars.bit_width[scalar_row] = 8;
    EXPECT_THROW(lispb::schema::resolve_type_graph(make_manifest()), std::invalid_argument);
}

TEST(Validation, RejectsEmptyModuleNames) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto module{valid_soa_module(context)};
    normal_modules.settings[normal_modules.index(module)] = [&] {
        auto row_0{declarations.module_settings_store.clone(
            normal_modules.settings[normal_modules.index(module)])};
        declarations.module_settings_store
            .name[declarations.module_settings_store.index(row_0)] = {};
        return row_0;
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsInvalidEnumDefinitions) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& enums{declarations.enums};
    auto& enumerators{declarations.enumerators};
    auto& strings{context.storage.strings};
    auto module{valid_enum_module(context)};
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            enums.values[enums.index(row_0)] = {};
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_enum_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            auto const row_0_row{enums.index(row_0)};
            enums.values[row_0_row] = [&] {
                auto const original_items{declarations.items(enums.values[row_0_row])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.push_back([&] {
                    auto const new_enumerators{enumerators.append()};
                    enumerators.name[enumerators.index(new_enumerators)] = strings.intern("Value");
                    return new_enumerators;
                }());
                return declarations.range(rows);
            }();
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_enum_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{enums.clone(rows_3.front())};
            auto const row_2_row{enums.index(row_2)};
            enums.values[row_2_row] = [&] {
                auto const original_items{declarations.items(enums.values[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{enumerators.clone(rows_1.front())};
                    enumerators.name[enumerators.index(row_0)] = strings.intern("bad-name");
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_enum_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            enums.underlying_type[enums.index(row_0)] = make_test_type_ref(context, "@missing");
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsEnumSemanticWidthsThatCannotRepresentKnownValues) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& enums{declarations.enums};
    auto& enumerators{declarations.enumerators};
    auto& strings{context.storage.strings};
    auto module{valid_enum_module(context)};
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            enums.reflection[enums.index(row_0)] = EnumReflection::none;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            enums.bit_width[enums.index(row_0)] = 0;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_enum_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            enums.reflection[enums.index(row_0)] = EnumReflection::none;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            enums.bit_width[enums.index(row_0)] = 65;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_enum_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            enums.reflection[enums.index(row_0)] = EnumReflection::none;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            enums.values[enums.index(row_0)] = declarations.range<lispb::codegen::EnumeratorId>(
                {[&] {
                     auto const new_enumerators{enumerators.append()};
                     auto const new_enumerators_row{enumerators.index(new_enumerators)};
                     enumerators.name[new_enumerators_row] = strings.intern("Zero");
                     enumerators.initializer[new_enumerators_row] = strings.intern("0");
                     return new_enumerators;
                 }(),
                 [&] {
                     auto const new_enumerators{enumerators.append()};
                     auto const new_enumerators_row{enumerators.index(new_enumerators)};
                     enumerators.name[new_enumerators_row] = strings.intern("Seven");
                     enumerators.initializer[new_enumerators_row] = strings.intern("7");
                     return new_enumerators;
                 }()});
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            enums.bit_width[enums.index(row_0)] = 2;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_enum_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            enums.reflection[enums.index(row_0)] = EnumReflection::none;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            enums.values[enums.index(row_0)] = declarations.range<lispb::codegen::EnumeratorId>(
                {[&] {
                     auto const new_enumerators{enumerators.append()};
                     auto const new_enumerators_row{enumerators.index(new_enumerators)};
                     enumerators.name[new_enumerators_row] = strings.intern("Zero");
                     enumerators.initializer[new_enumerators_row] = strings.intern("0");
                     return new_enumerators;
                 }(),
                 [&] {
                     auto const new_enumerators{enumerators.append()};
                     auto const new_enumerators_row{enumerators.index(new_enumerators)};
                     enumerators.name[new_enumerators_row] = strings.intern("Seven");
                     enumerators.initializer[new_enumerators_row] = strings.intern("7");
                     return new_enumerators;
                 }()});
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            enums.bit_width[enums.index(row_0)] = 3;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));
}

TEST(Validation, AppliesExplicitEnumSignednessToTheSemanticDomain) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& enums{declarations.enums};
    auto& enumerators{declarations.enumerators};
    auto& strings{context.storage.strings};
    auto module{valid_enum_module(context)};
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            enums.reflection[enums.index(row_0)] = EnumReflection::none;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            enums.values[enums.index(row_0)] = declarations.range<lispb::codegen::EnumeratorId>(
                {[&] {
                     auto const new_enumerators{enumerators.append()};
                     auto const new_enumerators_row{enumerators.index(new_enumerators)};
                     enumerators.name[new_enumerators_row] = strings.intern("Negative");
                     enumerators.initializer[new_enumerators_row] = strings.intern("-1");
                     return new_enumerators;
                 }(),
                 [&] {
                     auto const new_enumerators{enumerators.append()};
                     auto const new_enumerators_row{enumerators.index(new_enumerators)};
                     enumerators.name[new_enumerators_row] = strings.intern("Positive");
                     enumerators.initializer[new_enumerators_row] = strings.intern("1");
                     return new_enumerators;
                 }()});
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            enums.signedness[enums.index(row_0)] = schema::EnumSignedness::unsigned_;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_enum_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            enums.reflection[enums.index(row_0)] = EnumReflection::none;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            enums.values[enums.index(row_0)] = declarations.range<lispb::codegen::EnumeratorId>(
                {[&] {
                     auto const new_enumerators{enumerators.append()};
                     auto const new_enumerators_row{enumerators.index(new_enumerators)};
                     enumerators.name[new_enumerators_row] = strings.intern("Zero");
                     enumerators.initializer[new_enumerators_row] = strings.intern("0");
                     return new_enumerators;
                 }(),
                 [&] {
                     auto const new_enumerators{enumerators.append()};
                     auto const new_enumerators_row{enumerators.index(new_enumerators)};
                     enumerators.name[new_enumerators_row] = strings.intern("Seven");
                     enumerators.initializer[new_enumerators_row] = strings.intern("7");
                     return new_enumerators;
                 }()});
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            enums.signedness[enums.index(row_0)] = schema::EnumSignedness::signed_;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            enums.bit_width[enums.index(row_0)] = 3;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_enum_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            enums.reflection[enums.index(row_0)] = EnumReflection::none;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            enums.values[enums.index(row_0)] = declarations.range<lispb::codegen::EnumeratorId>(
                {[&] {
                     auto const new_enumerators{enumerators.append()};
                     auto const new_enumerators_row{enumerators.index(new_enumerators)};
                     enumerators.name[new_enumerators_row] = strings.intern("Zero");
                     enumerators.initializer[new_enumerators_row] = strings.intern("0");
                     return new_enumerators;
                 }(),
                 [&] {
                     auto const new_enumerators{enumerators.append()};
                     auto const new_enumerators_row{enumerators.index(new_enumerators)};
                     enumerators.name[new_enumerators_row] = strings.intern("Seven");
                     enumerators.initializer[new_enumerators_row] = strings.intern("7");
                     return new_enumerators;
                 }()});
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            enums.signedness[enums.index(row_0)] = schema::EnumSignedness::signed_;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            enums.bit_width[enums.index(row_0)] = 4;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));
}

TEST(Validation, RejectsInvalidIntegerScalarDomainsAndNamedCodes) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& integer_scalars{declarations.integer_scalars};
    auto& packed_named_codes{declarations.packed_named_codes};
    auto& strings{context.storage.strings};
    auto module{valid_integer_scalar_module(context)};
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{integer_scalars.clone(rows_1.front())};
            integer_scalars.bit_width[integer_scalars.index(row_0)] = 3;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_integer_scalar_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{integer_scalars.clone(rows_1.front())};
            integer_scalars.minimum_value[integer_scalars.index(row_0)] = -1;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_integer_scalar_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{integer_scalars.clone(rows_3.front())};
            auto const row_2_row{integer_scalars.index(row_2)};
            integer_scalars.named_codes[row_2_row] = [&] {
                auto const original_items{
                    declarations.items(integer_scalars.named_codes[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{packed_named_codes.clone(rows_1[1])};
                    packed_named_codes.value[packed_named_codes.index(row_0)] = 10;
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_integer_scalar_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{integer_scalars.clone(rows_3.front())};
            auto const row_2_row{integer_scalars.index(row_2)};
            integer_scalars.named_codes[row_2_row] = [&] {
                auto const original_items{
                    declarations.items(integer_scalars.named_codes[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_named_codes.clone(rows_1[0])};
                    packed_named_codes.value[packed_named_codes.index(row_0)] = 11;
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_integer_scalar_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{integer_scalars.clone(rows_3.front())};
            auto const row_2_row{integer_scalars.index(row_2)};
            integer_scalars.named_codes[row_2_row] = [&] {
                auto const original_items{
                    declarations.items(integer_scalars.named_codes[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{packed_named_codes.clone(rows_1[1])};
                    packed_named_codes.name[packed_named_codes.index(row_0)] =
                        strings.intern("Unknown");
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_integer_scalar_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{integer_scalars.clone(rows_3.front())};
            auto const row_2_row{integer_scalars.index(row_2)};
            integer_scalars.named_codes[row_2_row] = [&] {
                auto const original_items{
                    declarations.items(integer_scalars.named_codes[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{packed_named_codes.clone(rows_1[1])};
                    packed_named_codes.value[packed_named_codes.index(row_0)] = 0;
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_integer_scalar_module(context);
    normal_modules.settings[normal_modules.index(module)] = [&] {
        auto row_0{declarations.module_settings_store.clone(
            normal_modules.settings[normal_modules.index(module)])};
        declarations.module_settings_store.source[declarations.module_settings_store.index(row_0)] =
            strings.intern("Scalars.cpp");
        return row_0;
    }();
    EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));

    module = valid_integer_scalar_module(context);
    auto const lowered{lower_test_manifest(context, manifest_with(context, module))};
    ASSERT_EQ(lowered.size(), 1U);
    auto const header{context.ir.modules.header[context.ir.modules.index(lowered.front())]};
    ASSERT_NE(header.value, 0U);
    EXPECT_EQ(strings.view(context.ir.files.path[context.ir.files.index(header)]), "Scalars.h");
    EXPECT_EQ(render_modules(context.ir, lowered).front().content.find("struct "),
              std::string::npos);
}

TEST(Validation, ValidatesIntegerScalarCppConstantsPolicy) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& integer_scalars{declarations.integer_scalars};

    auto& packed_named_codes{declarations.packed_named_codes};
    auto& strings{context.storage.strings};
    auto module{valid_integer_scalar_module(context)};
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{integer_scalars.clone(rows_1.front())};
            integer_scalars.cpp_type[integer_scalars.index(row_0)] =
                make_test_type_ref(context, "std::uint8_t");
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_integer_scalar_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{integer_scalars.clone(rows_1.front())};
            integer_scalars.cpp_emission[integer_scalars.index(row_0)] =
                IntegerScalarCppEmission::constants;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_integer_scalar_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{integer_scalars.clone(rows_1.front())};
            integer_scalars.cpp_emission[integer_scalars.index(row_0)] =
                IntegerScalarCppEmission::constants;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{integer_scalars.clone(rows_1.front())};
            integer_scalars.cpp_type[integer_scalars.index(row_0)] =
                make_test_type_ref(context, "std::uint8_t");
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));

    module = valid_integer_scalar_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{integer_scalars.clone(rows_1.front())};
            integer_scalars.cpp_emission[integer_scalars.index(row_0)] =
                IntegerScalarCppEmission::constants;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{integer_scalars.clone(rows_1.front())};
            integer_scalars.cpp_type[integer_scalars.index(row_0)] =
                make_test_type_ref(context, "std::int8_t");
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{integer_scalars.clone(rows_1.front())};
            integer_scalars.maximum_value[integer_scalars.index(row_0)] = 127;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{integer_scalars.clone(rows_3.front())};
            auto const row_2_row{integer_scalars.index(row_2)};
            integer_scalars.named_codes[row_2_row] = [&] {
                auto const original_items{
                    declarations.items(integer_scalars.named_codes[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{packed_named_codes.clone(rows_1[1])};
                    packed_named_codes.value[packed_named_codes.index(row_0)] = 128;
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_integer_scalar_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{integer_scalars.clone(rows_1.front())};
            integer_scalars.cpp_emission[integer_scalars.index(row_0)] =
                IntegerScalarCppEmission::constants;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{integer_scalars.clone(rows_1.front())};
            integer_scalars.cpp_type[integer_scalars.index(row_0)] =
                make_test_type_ref(context, "float");
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_integer_scalar_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{integer_scalars.clone(rows_1.front())};
            integer_scalars.cpp_emission[integer_scalars.index(row_0)] =
                IntegerScalarCppEmission::constants;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{integer_scalars.clone(rows_1.front())};
            integer_scalars.cpp_type[integer_scalars.index(row_0)] =
                make_test_type_ref(context, "std::uint8_t");
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{integer_scalars.clone(rows_1.front())};
            integer_scalars.named_codes[integer_scalars.index(row_0)] = {};
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_integer_scalar_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{integer_scalars.clone(rows_1.front())};
            integer_scalars.cpp_emission[integer_scalars.index(row_0)] =
                IntegerScalarCppEmission::constants_with_names;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{integer_scalars.clone(rows_1.front())};
            integer_scalars.cpp_type[integer_scalars.index(row_0)] =
                make_test_type_ref(context, "std::uint8_t");
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{integer_scalars.clone(rows_3.front())};
            auto const row_2_row{integer_scalars.index(row_2)};
            integer_scalars.named_codes[row_2_row] = [&] {
                auto const original_items{
                    declarations.items(integer_scalars.named_codes[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{packed_named_codes.clone(rows_1.front())};
                    packed_named_codes.name[packed_named_codes.index(row_0)] =
                        strings.intern("name");
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, AllowsEmptyEditableModulesToRemainGenerationDestinations) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& module_settings_store{declarations.module_settings_store};
    auto& normal_modules{declarations.normal_modules};
    auto& strings{context.storage.strings};
    auto settings = [&](std::string const& name) {
        return [&] {
            auto const new_module_settings_store{module_settings_store.append()};
            auto const new_module_settings_store_row{
                module_settings_store.index(new_module_settings_store)};
            module_settings_store.name[new_module_settings_store_row] = strings.intern(name);
            module_settings_store.header[new_module_settings_store_row] =
                strings.intern(name + ".h");
            return new_module_settings_store;
        }();
    };
    auto module{[&] {
        auto const new_normal_modules{normal_modules.append()};
        normal_modules.settings[normal_modules.index(new_normal_modules)] = settings("empty");
        return new_normal_modules;
    }()};
    EXPECT_NO_THROW(
        render_modules(context.ir, lower_test_manifest(context, manifest_with(context, module))));
    normal_modules.settings[normal_modules.index(module)] = [&] {
        auto row_0{
            module_settings_store.clone(normal_modules.settings[normal_modules.index(module)])};
        module_settings_store.source[module_settings_store.index(row_0)] =
            strings.intern("empty.cpp");
        return row_0;
    }();
    EXPECT_EQ(
        render_modules(context.ir, lower_test_manifest(context, manifest_with(context, module)))
            .size(),
        2U);
}

TEST(Validation, ValidatesRecordMemberRelationshipUnits) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& module_settings_store{declarations.module_settings_store};
    auto& records{declarations.records};
    auto& record_members{declarations.record_members};

    auto& semantic_relations{declarations.semantic_relations};
    auto& strings{context.storage.strings};
    auto module{[&] {
        auto const new_normal_modules{normal_modules.append()};
        auto const new_normal_modules_row{normal_modules.index(new_normal_modules)};
        normal_modules.settings[new_normal_modules_row] = [&] {
            auto const new_module_settings_store{module_settings_store.append()};
            auto const new_module_settings_store_row{
                module_settings_store.index(new_module_settings_store)};
            module_settings_store.name[new_module_settings_store_row] = strings.intern("records");
            module_settings_store.header[new_module_settings_store_row] =
                strings.intern("Records.h");
            return new_module_settings_store;
        }();
        normal_modules.declarations[new_normal_modules_row] =
            declarations.range<lispb::codegen::DeclarationHandle>(
                {[&] {
                     auto const new_records{records.append()};
                     auto const new_records_row{records.index(new_records)};
                     records.name[new_records_row] = strings.intern("Target");
                     records.members[new_records_row] =
                         declarations.range<lispb::codegen::RecordMemberId>({[&] {
                             auto const new_record_members{record_members.append()};
                             auto const new_record_members_row{
                                 record_members.index(new_record_members)};
                             record_members.name[new_record_members_row] = strings.intern("value");
                             record_members.type[new_record_members_row] =
                                 make_test_type_ref(context, "std::uint32_t");
                             return new_record_members;
                         }()});
                     return new_records;
                 }(),
                 [&] {
                     auto const new_records{records.append()};
                     auto const new_records_row{records.index(new_records)};
                     records.name[new_records_row] = strings.intern("User");
                     records.members[new_records_row] =
                         declarations.range<lispb::codegen::RecordMemberId>({[&] {
                             auto const new_record_members{record_members.append()};
                             auto const new_record_members_row{
                                 record_members.index(new_record_members)};
                             record_members.name[new_record_members_row] = strings.intern("value");
                             record_members.type[new_record_members_row] =
                                 make_test_type_ref(context, "std::uint32_t");
                             record_members.relationship[new_record_members_row] = [&] {
                                 auto const new_semantic_relations{semantic_relations.append()};
                                 auto const new_semantic_relations_row{
                                     semantic_relations.index(new_semantic_relations)};
                                 semantic_relations.kind[new_semantic_relations_row] =
                                     SemanticRelationKind::references;
                                 semantic_relations.target[new_semantic_relations_row] =
                                     make_test_type_ref(context, "Target");
                                 semantic_relations.unit[new_semantic_relations_row] = std::nullopt;
                                 return new_semantic_relations;
                             }();
                             return new_record_members;
                         }()});
                     return new_records;
                 }()});
        return new_normal_modules;
    }()};
    EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));

    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3[1] = [&] {
            auto row_2{records.clone(rows_3[1])};
            auto const row_2_row{records.index(row_2)};
            records.members[row_2_row] = [&] {
                auto const original_items{declarations.items(records.members[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{record_members.clone(rows_1[0])};
                    record_members.relationship[record_members.index(row_0)] = [&] {
                        auto const new_semantic_relations{semantic_relations.append()};
                        auto const new_semantic_relations_row{
                            semantic_relations.index(new_semantic_relations)};
                        semantic_relations.kind[new_semantic_relations_row] =
                            SemanticRelationKind::offset_into;
                        semantic_relations.target[new_semantic_relations_row] =
                            make_test_type_ref(context, "Target");
                        semantic_relations.unit[new_semantic_relations_row] = std::nullopt;
                        return new_semantic_relations;
                    }();
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3[1] = [&] {
            auto row_2{records.clone(rows_3[1])};
            auto const row_2_row{records.index(row_2)};
            records.members[row_2_row] = [&] {
                auto const original_items{declarations.items(records.members[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{record_members.clone(rows_1[0])};
                    record_members.relationship[record_members.index(row_0)] = [&] {
                        auto const new_semantic_relations{semantic_relations.append()};
                        auto const new_semantic_relations_row{
                            semantic_relations.index(new_semantic_relations)};
                        semantic_relations.kind[new_semantic_relations_row] =
                            SemanticRelationKind::references;
                        semantic_relations.target[new_semantic_relations_row] =
                            make_test_type_ref(context, "Target");
                        semantic_relations.unit[new_semantic_relations_row] =
                            SemanticRelationUnit::elements;
                        return new_semantic_relations;
                    }();
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3[1] = [&] {
            auto row_2{records.clone(rows_3[1])};
            auto const row_2_row{records.index(row_2)};
            records.members[row_2_row] = [&] {
                auto const original_items{declarations.items(records.members[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{record_members.clone(rows_1[0])};
                    record_members.relationship[record_members.index(row_0)] = [&] {
                        auto const new_semantic_relations{semantic_relations.append()};
                        auto const new_semantic_relations_row{
                            semantic_relations.index(new_semantic_relations)};
                        semantic_relations.kind[new_semantic_relations_row] =
                            SemanticRelationKind::offset_into;
                        semantic_relations.target[new_semantic_relations_row] =
                            make_test_type_ref(context, "Target");
                        semantic_relations.unit[new_semantic_relations_row] =
                            SemanticRelationUnit::bytes;
                        return new_semantic_relations;
                    }();
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));
}

TEST(Validation, ValidatesSoaMemberRelationshipUnits) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& soas{declarations.soas};
    auto& soa_members{declarations.soa_members};

    auto& semantic_relations{declarations.semantic_relations};
    auto& strings{context.storage.strings};
    auto module{valid_soa_module(context)};
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows{std::vector(original_items.begin(), original_items.end())};
        rows.push_back([&] {
            auto const new_soas{soas.append()};
            auto const new_soas_row{soas.index(new_soas)};
            soas.name[new_soas_row] = strings.intern("Target");
            soas.members[new_soas_row] = declarations.range<lispb::codegen::SoaMemberId>({[&] {
                auto const new_soa_members{soa_members.append()};
                auto const new_soa_members_row{soa_members.index(new_soa_members)};
                soa_members.name[new_soa_members_row] = strings.intern("values");
                soa_members.kind[new_soa_members_row] = SoaMemberKind::array;
                soa_members.type[new_soa_members_row] =
                    make_test_type_ref(context, "std::uint32_t");
                soa_members.relationship[new_soa_members_row] = std::nullopt;
                return new_soa_members;
            }()});
            return new_soas;
        }());
        return declarations.range(rows);
    }();

    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{soas.clone(rows_3.front())};
            auto const row_2_row{soas.index(row_2)};
            soas.members[row_2_row] = [&] {
                auto const original_items{declarations.items(soas.members[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{soa_members.clone(rows_1.front())};
                    soa_members.relationship[soa_members.index(row_0)] = [&] {
                        auto const new_semantic_relations{semantic_relations.append()};
                        auto const new_semantic_relations_row{
                            semantic_relations.index(new_semantic_relations)};
                        semantic_relations.kind[new_semantic_relations_row] =
                            SemanticRelationKind::references;
                        semantic_relations.target[new_semantic_relations_row] =
                            make_test_type_ref(context, "Target");
                        semantic_relations.unit[new_semantic_relations_row] = std::nullopt;
                        return new_semantic_relations;
                    }();
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));

    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{soas.clone(rows_3.front())};
            auto const row_2_row{soas.index(row_2)};
            soas.members[row_2_row] = [&] {
                auto const original_items{declarations.items(soas.members[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{soa_members.clone(rows_1.front())};
                    soa_members.relationship[soa_members.index(row_0)] = [&] {
                        auto const new_semantic_relations{semantic_relations.append()};
                        auto const new_semantic_relations_row{
                            semantic_relations.index(new_semantic_relations)};
                        semantic_relations.kind[new_semantic_relations_row] =
                            SemanticRelationKind::offset_into;
                        semantic_relations.target[new_semantic_relations_row] =
                            make_test_type_ref(context, "Target");
                        semantic_relations.unit[new_semantic_relations_row] = std::nullopt;
                        return new_semantic_relations;
                    }();
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{soas.clone(rows_3.front())};
            auto const row_2_row{soas.index(row_2)};
            soas.members[row_2_row] = [&] {
                auto const original_items{declarations.items(soas.members[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{soa_members.clone(rows_1.front())};
                    soa_members.relationship[soa_members.index(row_0)] = [&] {
                        auto const new_semantic_relations{semantic_relations.append()};
                        auto const new_semantic_relations_row{
                            semantic_relations.index(new_semantic_relations)};
                        semantic_relations.kind[new_semantic_relations_row] =
                            SemanticRelationKind::references;
                        semantic_relations.target[new_semantic_relations_row] =
                            make_test_type_ref(context, "Target");
                        semantic_relations.unit[new_semantic_relations_row] =
                            SemanticRelationUnit::elements;
                        return new_semantic_relations;
                    }();
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{soas.clone(rows_3.front())};
            auto const row_2_row{soas.index(row_2)};
            soas.members[row_2_row] = [&] {
                auto const original_items{declarations.items(soas.members[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{soa_members.clone(rows_1.front())};
                    soa_members.relationship[soa_members.index(row_0)] = [&] {
                        auto const new_semantic_relations{semantic_relations.append()};
                        auto const new_semantic_relations_row{
                            semantic_relations.index(new_semantic_relations)};
                        semantic_relations.kind[new_semantic_relations_row] =
                            SemanticRelationKind::offset_into;
                        semantic_relations.target[new_semantic_relations_row] =
                            make_test_type_ref(context, "Target");
                        semantic_relations.unit[new_semantic_relations_row] =
                            SemanticRelationUnit::bytes;
                        return new_semantic_relations;
                    }();
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));
}

TEST(Validation, RequiresUnsignedIndexCountAndOffsetIntegerScalars) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& integer_scalars{declarations.integer_scalars};
    auto& semantic_relations{declarations.semantic_relations};

    for (auto const kind : {SemanticRelationKind::index_into,
                            SemanticRelationKind::count_of,
                            SemanticRelationKind::offset_into}) {
        auto module{valid_integer_scalar_module(context)};

        normal_modules.declarations[normal_modules.index(module)] = [&] {
            auto const original_items{
                declarations.items(normal_modules.declarations[normal_modules.index(module)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{integer_scalars.clone(rows_1.front())};
                integer_scalars.signedness[integer_scalars.index(row_0)] = true;
                return row_0;
            }();
            return declarations.range(rows_1);
        }();
        normal_modules.declarations[normal_modules.index(module)] = [&] {
            auto const original_items{
                declarations.items(normal_modules.declarations[normal_modules.index(module)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{integer_scalars.clone(rows_1.front())};
                integer_scalars.minimum_value[integer_scalars.index(row_0)] = -10;
                return row_0;
            }();
            return declarations.range(rows_1);
        }();
        normal_modules.declarations[normal_modules.index(module)] = [&] {
            auto const original_items{
                declarations.items(normal_modules.declarations[normal_modules.index(module)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{integer_scalars.clone(rows_1.front())};
                integer_scalars.maximum_value[integer_scalars.index(row_0)] = 10;
                return row_0;
            }();
            return declarations.range(rows_1);
        }();
        normal_modules.declarations[normal_modules.index(module)] = [&] {
            auto const original_items{
                declarations.items(normal_modules.declarations[normal_modules.index(module)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{integer_scalars.clone(rows_1.front())};
                integer_scalars.named_codes[integer_scalars.index(row_0)] = {};
                return row_0;
            }();
            return declarations.range(rows_1);
        }();
        normal_modules.declarations[normal_modules.index(module)] = [&] {
            auto const original_items{
                declarations.items(normal_modules.declarations[normal_modules.index(module)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{integer_scalars.clone(rows_1.front())};
                integer_scalars.relationship[integer_scalars.index(row_0)] = [&] {
                    auto const new_semantic_relations{semantic_relations.append()};
                    auto const new_semantic_relations_row{
                        semantic_relations.index(new_semantic_relations)};
                    semantic_relations.kind[new_semantic_relations_row] = kind;
                    semantic_relations.target[new_semantic_relations_row] =
                        make_test_type_ref(context, "DamageReason");
                    semantic_relations.unit[new_semantic_relations_row] =
                        kind == SemanticRelationKind::offset_into
                            ? std::optional{SemanticRelationUnit::bytes}
                            : std::nullopt;
                    return new_semantic_relations;
                }();
                return row_0;
            }();
            return declarations.range(rows_1);
        }();
        EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                     std::invalid_argument);
    }

    auto module{valid_integer_scalar_module(context)};
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{integer_scalars.clone(rows_1.front())};
            integer_scalars.relationship[integer_scalars.index(row_0)] = [&] {
                auto const new_semantic_relations{semantic_relations.append()};
                auto const new_semantic_relations_row{
                    semantic_relations.index(new_semantic_relations)};
                semantic_relations.kind[new_semantic_relations_row] =
                    SemanticRelationKind::references;
                semantic_relations.target[new_semantic_relations_row] =
                    make_test_type_ref(context, "DamageReason");
                semantic_relations.unit[new_semantic_relations_row] = std::nullopt;
                return new_semantic_relations;
            }();
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));

    module = valid_integer_scalar_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{integer_scalars.clone(rows_1.front())};
            integer_scalars.relationship[integer_scalars.index(row_0)] = [&] {
                auto const new_semantic_relations{semantic_relations.append()};
                auto const new_semantic_relations_row{
                    semantic_relations.index(new_semantic_relations)};
                semantic_relations.kind[new_semantic_relations_row] =
                    SemanticRelationKind::offset_into;
                semantic_relations.target[new_semantic_relations_row] =
                    make_test_type_ref(context, "DamageReason");
                semantic_relations.unit[new_semantic_relations_row] = std::nullopt;
                return new_semantic_relations;
            }();
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_integer_scalar_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{integer_scalars.clone(rows_1.front())};
            integer_scalars.relationship[integer_scalars.index(row_0)] = [&] {
                auto const new_semantic_relations{semantic_relations.append()};
                auto const new_semantic_relations_row{
                    semantic_relations.index(new_semantic_relations)};
                semantic_relations.kind[new_semantic_relations_row] =
                    SemanticRelationKind::index_into;
                semantic_relations.target[new_semantic_relations_row] =
                    make_test_type_ref(context, "DamageReason");
                semantic_relations.unit[new_semantic_relations_row] =
                    SemanticRelationUnit::elements;
                return new_semantic_relations;
            }();
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_integer_scalar_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{integer_scalars.clone(rows_1.front())};
            integer_scalars.relationship[integer_scalars.index(row_0)] = [&] {
                auto const new_semantic_relations{semantic_relations.append()};
                auto const new_semantic_relations_row{
                    semantic_relations.index(new_semantic_relations)};
                semantic_relations.kind[new_semantic_relations_row] =
                    SemanticRelationKind::offset_into;
                semantic_relations.target[new_semantic_relations_row] =
                    make_test_type_ref(context, "DamageReason");
                semantic_relations.unit[new_semantic_relations_row] = SemanticRelationUnit::bytes;
                return new_semantic_relations;
            }();
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));
}

TEST(Validation, ValidatesLinearQuantizedRepresentationsAndEmitsOnlyConfiguredHeader) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& integer_scalars{declarations.integer_scalars};
    auto& linear_quantizeds{declarations.linear_quantizeds};
    auto& strings{context.storage.strings};
    auto manifest{valid_linear_quantized_manifest(context)};
    auto lowered{lower_test_manifest(context, manifest)};
    ASSERT_EQ(lowered.size(), 2U);
    auto const header{context.ir.modules.header[context.ir.modules.index(lowered[1])]};
    ASSERT_NE(header.value, 0U);
    EXPECT_EQ(strings.view(context.ir.files.path[context.ir.files.index(header)]),
              "Representations.h");
    EXPECT_EQ(render_modules(context.ir, lowered)[1].content.find("struct "), std::string::npos);

    manifest = valid_linear_quantized_manifest(context);

    manifest.modules[0] = [&] {
        auto row_2{normal_modules.clone(manifest.modules[0])};
        auto const row_2_row{normal_modules.index(row_2)};
        normal_modules.declarations[row_2_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_2_row])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{integer_scalars.clone(rows_1.front())};
                integer_scalars.signedness[integer_scalars.index(row_0)] = true;
                return row_0;
            }();
            return declarations.range(rows_1);
        }();
        return row_2;
    }();
    manifest.modules[0] = [&] {
        auto row_2{normal_modules.clone(manifest.modules[0])};
        auto const row_2_row{normal_modules.index(row_2)};
        normal_modules.declarations[row_2_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_2_row])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{integer_scalars.clone(rows_1.front())};
                integer_scalars.minimum_value[integer_scalars.index(row_0)] = -100;
                return row_0;
            }();
            return declarations.range(rows_1);
        }();
        return row_2;
    }();
    manifest.modules[0] = [&] {
        auto row_2{normal_modules.clone(manifest.modules[0])};
        auto const row_2_row{normal_modules.index(row_2)};
        normal_modules.declarations[row_2_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_2_row])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{integer_scalars.clone(rows_1.front())};
                integer_scalars.maximum_value[integer_scalars.index(row_0)] = 100;
                return row_0;
            }();
            return declarations.range(rows_1);
        }();
        return row_2;
    }();
    manifest.modules[0] = [&] {
        auto row_2{normal_modules.clone(manifest.modules[0])};
        auto const row_2_row{normal_modules.index(row_2)};
        normal_modules.declarations[row_2_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_2_row])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{integer_scalars.clone(rows_1.front())};
                integer_scalars.named_codes[integer_scalars.index(row_0)] = {};
                return row_0;
            }();
            return declarations.range(rows_1);
        }();
        return row_2;
    }();
    EXPECT_NO_THROW(lower_test_manifest(context, manifest));

    manifest = valid_linear_quantized_manifest(context);
    manifest.modules[1] = [&] {
        auto row_2{normal_modules.clone(manifest.modules[1])};
        auto const row_2_row{normal_modules.index(row_2)};
        normal_modules.declarations[row_2_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_2_row])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{linear_quantizeds.clone(rows_1.front())};
                linear_quantizeds.bit_width[linear_quantizeds.index(row_0)] = 1;
                return row_0;
            }();
            return declarations.range(rows_1);
        }();
        return row_2;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);

    manifest = valid_linear_quantized_manifest(context);

    manifest.modules[1] = [&] {
        auto row_2{normal_modules.clone(manifest.modules[1])};
        auto const row_2_row{normal_modules.index(row_2)};
        normal_modules.declarations[row_2_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_2_row])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{linear_quantizeds.clone(rows_1.front())};
                linear_quantizeds.bit_width[linear_quantizeds.index(row_0)] = 2;
                return row_0;
            }();
            return declarations.range(rows_1);
        }();
        return row_2;
    }();
    manifest.modules[1] = [&] {
        auto row_2{normal_modules.clone(manifest.modules[1])};
        auto const row_2_row{normal_modules.index(row_2)};
        normal_modules.declarations[row_2_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_2_row])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{linear_quantizeds.clone(rows_1.front())};
                linear_quantizeds.reserved_codes[linear_quantizeds.index(row_0)] = 3;
                return row_0;
            }();
            return declarations.range(rows_1);
        }();
        return row_2;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);

    manifest = valid_linear_quantized_manifest(context);
    manifest.modules[0] = [&] {
        auto row_2{normal_modules.clone(manifest.modules[0])};
        auto const row_2_row{normal_modules.index(row_2)};
        normal_modules.declarations[row_2_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_2_row])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{integer_scalars.clone(rows_1.front())};
                integer_scalars.maximum_value[integer_scalars.index(row_0)] = 0;
                return row_0;
            }();
            return declarations.range(rows_1);
        }();
        return row_2;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);

    manifest = valid_linear_quantized_manifest(context);
    manifest.modules[1] = [&] {
        auto row_2{normal_modules.clone(manifest.modules[1])};
        auto const row_2_row{normal_modules.index(row_2)};
        normal_modules.declarations[row_2_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_2_row])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{linear_quantizeds.clone(rows_1.front())};
                linear_quantizeds.source[linear_quantizeds.index(row_0)] =
                    make_test_type_ref(context, "std::uint32_t");
                return row_0;
            }();
            return declarations.range(rows_1);
        }();
        return row_2;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);

    manifest = valid_linear_quantized_manifest(context);
    manifest.modules[1] = [&] {
        auto row_1{normal_modules.clone(manifest.modules[1])};
        auto const row_1_row{normal_modules.index(row_1)};
        normal_modules.settings[row_1_row] = [&] {
            auto row_0{
                declarations.module_settings_store.clone(normal_modules.settings[row_1_row])};
            declarations.module_settings_store
                .source[declarations.module_settings_store.index(row_0)] =
                strings.intern("Representations.cpp");
            return row_0;
        }();
        return row_1;
    }();
    EXPECT_EQ(render_modules(context.ir, lower_test_manifest(context, manifest)).size(), 3U);
}

TEST(Validation, ValidatesIntegerVarintSourceAndSignedness) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& integer_varints{declarations.integer_varints};

    auto& linear_quantizeds{declarations.linear_quantizeds};

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
        auto row_2{normal_modules.clone(manifest.modules[1])};
        auto const row_2_row{normal_modules.index(row_2)};
        normal_modules.declarations[row_2_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_2_row])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{integer_varints.clone(rows_1.front())};
                integer_varints.source[integer_varints.index(row_0)] =
                    make_test_type_ref(context, "std::uint32_t");
                return row_0;
            }();
            return declarations.range(rows_1);
        }();
        return row_2;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);

    manifest = valid_integer_varint_manifest(context);

    manifest.modules[1] = [&] {
        auto row_0{normal_modules.clone(manifest.modules[1])};
        auto const row_0_row{normal_modules.index(row_0)};
        normal_modules.declarations[row_0_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_0_row])};
            auto rows{std::vector(original_items.begin(), original_items.end())};
            rows.push_back([&] {
                auto const new_linear_quantizeds{linear_quantizeds.append()};
                auto const new_linear_quantizeds_row{
                    linear_quantizeds.index(new_linear_quantizeds)};
                linear_quantizeds.name[new_linear_quantizeds_row] =
                    integer_varints.name[integer_varints.index(
                        declarations
                            .items(normal_modules
                                       .declarations[normal_modules.index(manifest.modules[1])])
                            .front())];
                linear_quantizeds.source[new_linear_quantizeds_row] =
                    make_test_type_ref(context, "DamageReason");
                linear_quantizeds.bit_width[new_linear_quantizeds_row] = 4;
                linear_quantizeds.reserved_codes[new_linear_quantizeds_row] = 0;
                linear_quantizeds.clipping[new_linear_quantizeds_row] =
                    QuantizationClipping::reject;
                return new_linear_quantizeds;
            }());
            return declarations.range(rows);
        }();
        return row_0;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);
}

TEST(Validation, ValidatesFixedPointWidthsAndEmitsOnlyConfiguredHeader) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& fixed_points{declarations.fixed_points};
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
        auto row_2{normal_modules.clone(manifest.modules.front())};
        auto const row_2_row{normal_modules.index(row_2)};
        normal_modules.declarations[row_2_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_2_row])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{fixed_points.clone(rows_1.front())};
                fixed_points.total_bits[fixed_points.index(row_0)] = 8;
                return row_0;
            }();
            return declarations.range(rows_1);
        }();
        return row_2;
    }();
    manifest.modules.front() = [&] {
        auto row_2{normal_modules.clone(manifest.modules.front())};
        auto const row_2_row{normal_modules.index(row_2)};
        normal_modules.declarations[row_2_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_2_row])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{fixed_points.clone(rows_1.front())};
                fixed_points.fractional_bits[fixed_points.index(row_0)] = 8;
                return row_0;
            }();
            return declarations.range(rows_1);
        }();
        return row_2;
    }();
    EXPECT_NO_THROW(lower_test_manifest(context, manifest));

    manifest = valid_fixed_point_manifest(context);
    manifest.modules.front() = [&] {
        auto row_2{normal_modules.clone(manifest.modules.front())};
        auto const row_2_row{normal_modules.index(row_2)};
        normal_modules.declarations[row_2_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_2_row])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{fixed_points.clone(rows_1.front())};
                fixed_points.fractional_bits[fixed_points.index(row_0)] = 16;
                return row_0;
            }();
            return declarations.range(rows_1);
        }();
        return row_2;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);

    manifest = valid_fixed_point_manifest(context);
    manifest.modules.front() = [&] {
        auto row_2{normal_modules.clone(manifest.modules.front())};
        auto const row_2_row{normal_modules.index(row_2)};
        normal_modules.declarations[row_2_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_2_row])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{fixed_points.clone(rows_1.front())};
                fixed_points.total_bits[fixed_points.index(row_0)] = 0;
                return row_0;
            }();
            return declarations.range(rows_1);
        }();
        return row_2;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);

    manifest = valid_fixed_point_manifest(context);

    manifest.modules.front() = [&] {
        auto row_0{normal_modules.clone(manifest.modules.front())};
        auto const row_0_row{normal_modules.index(row_0)};
        normal_modules.declarations[row_0_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_0_row])};
            auto rows{std::vector(original_items.begin(), original_items.end())};
            rows.push_back(
                declarations
                    .items(
                        normal_modules.declarations[normal_modules.index(manifest.modules.front())])
                    .front());
            return declarations.range(rows);
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
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& optional_sentinels{declarations.optional_sentinels};
    auto& fixed_points{declarations.fixed_points};
    auto& strings{context.storage.strings};
    EXPECT_NO_THROW(lower_test_manifest(context, valid_optional_sentinel_manifest(context)));

    auto manifest{valid_optional_sentinel_manifest(context)};

    manifest.modules[1] = [&] {
        auto row_2{normal_modules.clone(manifest.modules[1])};
        auto const row_2_row{normal_modules.index(row_2)};
        normal_modules.declarations[row_2_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_2_row])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{optional_sentinels.clone(rows_1.front())};
                optional_sentinels.source[optional_sentinels.index(row_0)] =
                    make_test_type_ref(context, "std::uint32_t");
                return row_0;
            }();
            return declarations.range(rows_1);
        }();
        return row_2;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);

    manifest = valid_optional_sentinel_manifest(context);
    manifest.modules[1] = [&] {
        auto row_2{normal_modules.clone(manifest.modules[1])};
        auto const row_2_row{normal_modules.index(row_2)};
        normal_modules.declarations[row_2_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_2_row])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{optional_sentinels.clone(rows_1.front())};
                optional_sentinels.sentinel[optional_sentinels.index(row_0)] =
                    strings.intern("Missing");
                return row_0;
            }();
            return declarations.range(rows_1);
        }();
        return row_2;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);

    manifest = valid_optional_sentinel_manifest(context);
    manifest.modules[1] = [&] {
        auto row_2{normal_modules.clone(manifest.modules[1])};
        auto const row_2_row{normal_modules.index(row_2)};
        normal_modules.declarations[row_2_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_2_row])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{optional_sentinels.clone(rows_1.front())};
                optional_sentinels.sentinel[optional_sentinels.index(row_0)] =
                    strings.intern("Unknown");
                return row_0;
            }();
            return declarations.range(rows_1);
        }();
        return row_2;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);

    manifest = valid_optional_sentinel_manifest(context);
    manifest.modules[0] = [&] {
        auto row_2{normal_modules.clone(manifest.modules[0])};
        auto const row_2_row{normal_modules.index(row_2)};
        normal_modules.declarations[row_2_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_2_row])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{declarations.integer_scalars.clone(rows_1.front())};
                declarations.integer_scalars
                    .named_codes[declarations.integer_scalars.index(row_0)] = {};
                return row_0;
            }();
            return declarations.range(rows_1);
        }();
        return row_2;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);

    manifest = valid_optional_sentinel_manifest(context);

    manifest.modules[1] = [&] {
        auto row_0{normal_modules.clone(manifest.modules[1])};
        auto const row_0_row{normal_modules.index(row_0)};
        normal_modules.declarations[row_0_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_0_row])};
            auto rows{std::vector(original_items.begin(), original_items.end())};
            rows.push_back([&] {
                auto const new_fixed_points{fixed_points.append()};
                auto const new_fixed_points_row{fixed_points.index(new_fixed_points)};
                fixed_points.name[new_fixed_points_row] =
                    optional_sentinels.name[optional_sentinels.index(
                        declarations
                            .items(normal_modules
                                       .declarations[normal_modules.index(manifest.modules[1])])
                            .front())];
                fixed_points.signedness[new_fixed_points_row] = false;
                fixed_points.total_bits[new_fixed_points_row] = 8;
                fixed_points.fractional_bits[new_fixed_points_row] = 0;
                fixed_points.rounding[new_fixed_points_row] = FixedPointRounding::nearest_even;
                return new_fixed_points;
            }());
            return declarations.range(rows);
        }();
        return row_0;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);
}

TEST(Validation, ValidatesOptionalPresenceBitSourceAndSharedNames) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& optional_presence_bits{declarations.optional_presence_bits};

    auto& optional_sentinels{declarations.optional_sentinels};
    auto& strings{context.storage.strings};
    EXPECT_NO_THROW(lower_test_manifest(context, valid_optional_presence_bit_manifest(context)));

    auto manifest{valid_optional_presence_bit_manifest(context)};
    manifest.modules[1] = [&] {
        auto row_2{normal_modules.clone(manifest.modules[1])};
        auto const row_2_row{normal_modules.index(row_2)};
        normal_modules.declarations[row_2_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_2_row])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{optional_presence_bits.clone(rows_1.front())};
                optional_presence_bits.source[optional_presence_bits.index(row_0)] =
                    make_test_type_ref(context, "std::uint32_t");
                return row_0;
            }();
            return declarations.range(rows_1);
        }();
        return row_2;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);

    manifest = valid_optional_presence_bit_manifest(context);

    manifest.modules[1] = [&] {
        auto row_0{normal_modules.clone(manifest.modules[1])};
        auto const row_0_row{normal_modules.index(row_0)};
        normal_modules.declarations[row_0_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_0_row])};
            auto rows{std::vector(original_items.begin(), original_items.end())};
            rows.push_back([&] {
                auto const new_optional_sentinels{optional_sentinels.append()};
                auto const new_optional_sentinels_row{
                    optional_sentinels.index(new_optional_sentinels)};
                optional_sentinels.name[new_optional_sentinels_row] =
                    optional_presence_bits.name[optional_presence_bits.index(
                        declarations
                            .items(normal_modules
                                       .declarations[normal_modules.index(manifest.modules[1])])
                            .front())];
                optional_sentinels.source[new_optional_sentinels_row] =
                    make_test_type_ref(context, "DamageReason");
                optional_sentinels.sentinel[new_optional_sentinels_row] = strings.intern("Invalid");
                return new_optional_sentinels;
            }());
            return declarations.range(rows);
        }();
        return row_0;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);
}

TEST(Validation, RejectsInvalidEnumModuleConfiguration) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& module_settings_store{declarations.module_settings_store};
    auto& enums{declarations.enums};
    auto& strings{context.storage.strings};
    auto module{valid_enum_module(context)};
    auto settings_row{
        module_settings_store.index(normal_modules.settings[normal_modules.index(module)])};
    module_settings_store.namespace_name[settings_row] = strings.intern("project");
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_enum_module(context);
    settings_row =
        module_settings_store.index(normal_modules.settings[normal_modules.index(module)]);
    module_settings_store.source[settings_row].reset();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_enum_module(context);
    normal_modules.enum_helper_namespace[normal_modules.index(module)] = strings.intern("bad-name");
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_enum_module(context);
    auto enum_row{enums.index(
        declarations.items(normal_modules.declarations[normal_modules.index(module)]).front())};
    auto const original_conversions{declarations.items(enums.conversions[enum_row])};
    auto conversions{std::vector(original_conversions.begin(), original_conversions.end())};
    conversions.push_back(EnumConversion::string_view);
    enums.conversions[enum_row] = declarations.range(conversions);
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_enum_module(context);
    enum_row = enums.index(
        declarations.items(normal_modules.declarations[normal_modules.index(module)]).front());
    enums.reflection[enum_row] = EnumReflection::none;
    auto const first_value{declarations.items(enums.values[enum_row]).front()};
    declarations.enumerators.hidden[declarations.enumerators.index(first_value)] = true;
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsUnsupportedNativeEnumCombinations) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& enums{declarations.enums};
    auto& enum_unreal_projections{declarations.enum_unreal_projections};
    auto& strings{context.storage.strings};
    for (auto const& helpers : {std::vector{NativeEnumHelper::traits, NativeEnumHelper::traits},
                                std::vector{static_cast<NativeEnumHelper>(999)},
                                std::vector{NativeEnumHelper::to_serialized_string},
                                std::vector{NativeEnumHelper::try_parse_serialized}}) {
        auto invalid{valid_native_enum_module(context)};

        normal_modules.declarations[normal_modules.index(invalid)] = [&] {
            auto const original_items{
                declarations.items(normal_modules.declarations[normal_modules.index(invalid)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{enums.clone(rows_1.front())};
                enums.helpers[enums.index(row_0)] = declarations.range<NativeEnumHelper>(helpers);
                return row_0;
            }();
            return declarations.range(rows_1);
        }();
        normal_modules.declarations[normal_modules.index(invalid)] = [&] {
            auto const original_items{
                declarations.items(normal_modules.declarations[normal_modules.index(invalid)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{enums.clone(rows_3.front())};
                auto const row_2_row{enums.index(row_2)};
                enums.values[row_2_row] = [&] {
                    auto const original_items{declarations.items(enums.values[row_2_row])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1.front() = [&] {
                        auto row_0{declarations.enumerators.clone(rows_1.front())};
                        declarations.enumerators
                            .serialized_name[declarations.enumerators.index(row_0)]
                            .reset();
                        return row_0;
                    }();
                    return declarations.range(rows_1);
                }();
                return row_2;
            }();
            return declarations.range(rows_3);
        }();
        EXPECT_THROW(lower_test_manifest(context, manifest_with(context, invalid)),
                     std::invalid_argument);
    }
    auto unreal{valid_enum_module(context)};
    normal_modules.declarations[normal_modules.index(unreal)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(unreal)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            enums.helpers[enums.index(row_0)] =
                declarations.range<lispb::codegen::NativeEnumHelper>({NativeEnumHelper::traits});
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, unreal)),
                 std::invalid_argument);

    auto module{valid_native_enum_module(context)};
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows{std::vector(original_items.begin(), original_items.end())};
        rows.push_back(
            declarations
                .items(
                    normal_modules.declarations[normal_modules.index(valid_enum_module(context))])
                .front());
        return declarations.range(rows);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_native_enum_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            enums.reflection[enums.index(row_0)] = EnumReflection::uenum;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_native_enum_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            enums.enum_array[enums.index(row_0)] = true;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));

    module = valid_native_enum_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            enums.conversions[enums.index(row_0)] =
                declarations.range<lispb::codegen::EnumConversion>({EnumConversion::string_view});
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_native_enum_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            enums.export_specifier[enums.index(row_0)] = strings.intern("PROJECT_API");
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_native_enum_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            enums.unreal_projection[enums.index(row_0)] = [&] {
                auto const new_enum_unreal_projections{enum_unreal_projections.append()};
                auto const new_enum_unreal_projections_row{
                    enum_unreal_projections.index(new_enum_unreal_projections)};
                enum_unreal_projections.name[new_enum_unreal_projections_row] =
                    strings.intern("ENativeMode");
                enum_unreal_projections.header[new_enum_unreal_projections_row] =
                    strings.intern("Project/NativeMode.h");
                enum_unreal_projections.header_include[new_enum_unreal_projections_row] =
                    strings.intern("Project/NativeMode.h");
                enum_unreal_projections.conversion_header[new_enum_unreal_projections_row] =
                    strings.intern("Project/NativeModeConversion.h");
                enum_unreal_projections.native_header_include[enum_unreal_projections.index(
                    new_enum_unreal_projections)] = strings.intern("project/NativeEnums.h");
                enum_unreal_projections.reflection[new_enum_unreal_projections_row] =
                    EnumReflection::none;
                return new_enum_unreal_projections;
            }();
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_enum_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            enums.unreal_projection[enums.index(row_0)] = [&] {
                auto const new_enum_unreal_projections{enum_unreal_projections.append()};
                auto const new_enum_unreal_projections_row{
                    enum_unreal_projections.index(new_enum_unreal_projections)};
                enum_unreal_projections.name[new_enum_unreal_projections_row] =
                    strings.intern("ENativeMode");
                enum_unreal_projections.header[new_enum_unreal_projections_row] =
                    strings.intern("Project/NativeMode.h");
                enum_unreal_projections.header_include[new_enum_unreal_projections_row] =
                    strings.intern("Project/NativeMode.h");
                enum_unreal_projections.conversion_header[new_enum_unreal_projections_row] =
                    strings.intern("Project/NativeModeConversion.h");
                enum_unreal_projections.native_header_include[enum_unreal_projections.index(
                    new_enum_unreal_projections)] = strings.intern("project/NativeEnums.h");
                return new_enum_unreal_projections;
            }();
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsIncompleteOrAmbiguousSerializedEnumNames) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& enums{declarations.enums};
    auto& enumerators{declarations.enumerators};
    auto& strings{context.storage.strings};
    auto module{valid_enum_module(context)};
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            auto const row_0_row{enums.index(row_0)};
            enums.conversions[row_0_row] = [&] {
                auto const original_items{declarations.items(enums.conversions[row_0_row])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.push_back(EnumConversion::try_parse_serialized);
                return declarations.range(rows);
            }();
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_enum_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{enums.clone(rows_3.front())};
            auto const row_2_row{enums.index(row_2)};
            enums.values[row_2_row] = [&] {
                auto const original_items{declarations.items(enums.values[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{enumerators.clone(rows_1.front())};
                    enumerators.serialized_name[enumerators.index(row_0)] = strings.intern("value");
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            auto const row_0_row{enums.index(row_0)};
            enums.values[row_0_row] = [&] {
                auto const original_items{declarations.items(enums.values[row_0_row])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.push_back([&] {
                    auto const new_enumerators{enumerators.append()};
                    auto const new_enumerators_row{enumerators.index(new_enumerators)};
                    enumerators.name[new_enumerators_row] = strings.intern("Other");
                    enumerators.initializer[new_enumerators_row] = std::nullopt;
                    enumerators.display_name[new_enumerators_row] = std::nullopt;
                    enumerators.hidden[new_enumerators_row] = false;
                    enumerators.serialized_name[new_enumerators_row] = strings.intern("value");
                    return new_enumerators;
                }());
                return declarations.range(rows);
            }();
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            auto const row_0_row{enums.index(row_0)};
            enums.conversions[row_0_row] = [&] {
                auto const original_items{declarations.items(enums.conversions[row_0_row])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.push_back(EnumConversion::try_parse_serialized);
                return declarations.range(rows);
            }();
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsInvalidEnumArrayDefinitions) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& enums{declarations.enums};
    auto& enumerators{declarations.enumerators};
    auto& strings{context.storage.strings};
    auto module{valid_enum_module(context)};
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            enums.enum_array[enums.index(row_0)] = true;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{enums.clone(rows_3.front())};
            auto const row_2_row{enums.index(row_2)};
            enums.values[row_2_row] = [&] {
                auto const original_items{declarations.items(enums.values[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{enumerators.clone(rows_1.front())};
                    enumerators.initializer[enumerators.index(row_0)] = strings.intern("0");
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));

    for (auto const* code : {"2", "0", "-1", "unknown_code", "0x2"}) {
        SCOPED_TRACE(code);
        module = valid_enum_module(context);

        normal_modules.declarations[normal_modules.index(module)] = [&] {
            auto const original_items{
                declarations.items(normal_modules.declarations[normal_modules.index(module)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{enums.clone(rows_1.front())};
                enums.enum_array[enums.index(row_0)] = true;
                return row_0;
            }();
            return declarations.range(rows_1);
        }();
        normal_modules.declarations[normal_modules.index(module)] = [&] {
            auto const original_items{
                declarations.items(normal_modules.declarations[normal_modules.index(module)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{enums.clone(rows_1.front())};
                enums.values[enums.index(row_0)] = declarations.range<lispb::codegen::EnumeratorId>(
                    {[&] {
                         auto const new_enumerators{enumerators.append()};
                         auto const new_enumerators_row{enumerators.index(new_enumerators)};
                         enumerators.name[new_enumerators_row] = strings.intern("First");
                         enumerators.initializer[new_enumerators_row] = strings.intern("0");
                         return new_enumerators;
                     }(),
                     [&] {
                         auto const new_enumerators{enumerators.append()};
                         auto const new_enumerators_row{enumerators.index(new_enumerators)};
                         enumerators.name[new_enumerators_row] = strings.intern("Second");
                         enumerators.initializer[new_enumerators_row] = strings.intern(code);
                         return new_enumerators;
                     }()});
                return row_0;
            }();
            return declarations.range(rows_1);
        }();
        EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                     std::invalid_argument);
    }

    module = valid_native_enum_module(context);

    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            enums.enum_array[enums.index(row_0)] = true;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            enums.values[enums.index(row_0)] = declarations.range<lispb::codegen::EnumeratorId>(
                {[&] {
                     auto const new_enumerators{enumerators.append()};
                     auto const new_enumerators_row{enumerators.index(new_enumerators)};
                     enumerators.name[new_enumerators_row] = strings.intern("First");
                     enumerators.initializer[new_enumerators_row] = strings.intern("0x0");
                     return new_enumerators;
                 }(),
                 [&] {
                     auto const new_enumerators{enumerators.append()};
                     auto const new_enumerators_row{enumerators.index(new_enumerators)};
                     enumerators.name[new_enumerators_row] = strings.intern("Second");
                     enumerators.initializer[new_enumerators_row] = strings.intern("1");
                     return new_enumerators;
                 }()});
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));

    module = valid_enum_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            enums.enum_array[enums.index(row_0)] = true;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{enums.clone(rows_3.front())};
            auto const row_2_row{enums.index(row_2)};
            enums.values[row_2_row] = [&] {
                auto const original_items{declarations.items(enums.values[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{enumerators.clone(rows_1.front())};
                    enumerators.hidden[enumerators.index(row_0)] = true;
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_enum_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            enums.count[enums.index(row_0)] = strings.intern("Value");
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_enum_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            enums.enum_array[enums.index(row_0)] = true;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            enums.count[enums.index(row_0)] = strings.intern("Missing");
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_enum_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            enums.enum_array[enums.index(row_0)] = true;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            auto const row_0_row{enums.index(row_0)};
            enums.values[row_0_row] = [&] {
                auto const original_items{declarations.items(enums.values[row_0_row])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.push_back([&] {
                    auto const new_enumerators{enumerators.append()};
                    auto const new_enumerators_row{enumerators.index(new_enumerators)};
                    enumerators.name[new_enumerators_row] = strings.intern("COUNT");
                    enumerators.initializer[new_enumerators_row] = std::nullopt;
                    enumerators.display_name[new_enumerators_row] = std::nullopt;
                    enumerators.hidden[new_enumerators_row] = true;
                    return new_enumerators;
                }());
                return declarations.range(rows);
            }();
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            auto const row_0_row{enums.index(row_0)};
            enums.values[row_0_row] = [&] {
                auto const original_items{declarations.items(enums.values[row_0_row])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.push_back([&] {
                    auto const new_enumerators{enumerators.append()};
                    enumerators.name[enumerators.index(new_enumerators)] = strings.intern("After");
                    return new_enumerators;
                }());
                return declarations.range(rows);
            }();
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            enums.count[enums.index(row_0)] = strings.intern("COUNT");
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_enum_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            enums.enum_array[enums.index(row_0)] = true;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            auto const row_0_row{enums.index(row_0)};
            enums.values[row_0_row] = [&] {
                auto const original_items{declarations.items(enums.values[row_0_row])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.push_back([&] {
                    auto const new_enumerators{enumerators.append()};
                    enumerators.name[enumerators.index(new_enumerators)] = strings.intern("COUNT");
                    return new_enumerators;
                }());
                return declarations.range(rows);
            }();
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            enums.count[enums.index(row_0)] = strings.intern("COUNT");
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_enum_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            enums.enum_array[enums.index(row_0)] = true;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            enums.values[enums.index(row_0)] =
                declarations.range<lispb::codegen::EnumeratorId>({[&] {
                    auto const new_enumerators{enumerators.append()};
                    auto const new_enumerators_row{enumerators.index(new_enumerators)};
                    enumerators.name[new_enumerators_row] = strings.intern("COUNT");
                    enumerators.initializer[new_enumerators_row] = std::nullopt;
                    enumerators.display_name[new_enumerators_row] = std::nullopt;
                    enumerators.hidden[new_enumerators_row] = true;
                    return new_enumerators;
                }()});
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            enums.count[enums.index(row_0)] = strings.intern("COUNT");
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, SupportsSignedEnumValuesWhenTheyFitTheUnderlyingType) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& enums{declarations.enums};
    auto& enumerators{declarations.enumerators};
    auto& strings{context.storage.strings};
    auto module{valid_native_enum_module(context)};

    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            enums.underlying_type[enums.index(row_0)] = make_test_type_ref(context, "@native_int8");
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            enums.values[enums.index(row_0)] = declarations.range<lispb::codegen::EnumeratorId>(
                {[&] {
                     auto const new_enumerators{enumerators.append()};
                     auto const new_enumerators_row{enumerators.index(new_enumerators)};
                     enumerators.name[new_enumerators_row] = strings.intern("Minimum");
                     enumerators.initializer[new_enumerators_row] = strings.intern("-128");
                     return new_enumerators;
                 }(),
                 [&] {
                     auto const new_enumerators{enumerators.append()};
                     auto const new_enumerators_row{enumerators.index(new_enumerators)};
                     enumerators.name[new_enumerators_row] = strings.intern("Maximum");
                     enumerators.initializer[new_enumerators_row] = strings.intern("127");
                     return new_enumerators;
                 }()});
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            enums.count[enums.index(row_0)].reset();
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    auto const types{std::vector<std::pair<std::string_view, CppTypeId>>{
        {"native_int8",
         context.storage.cpp_types.add(
             "std::int8_t", {context.storage.cpp_types.dependency("std::int8_t", "cstdint")})},
    }};

    EXPECT_NO_THROW(
        static_cast<void>(lower_test_manifest(context, manifest_with(context, module, types))));

    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{enums.clone(rows_3.front())};
            auto const row_2_row{enums.index(row_2)};
            enums.values[row_2_row] = [&] {
                auto const original_items{declarations.items(enums.values[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.back() = [&] {
                    auto row_0{enumerators.clone(rows_1.back())};
                    enumerators.initializer[enumerators.index(row_0)] = strings.intern("128");
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(
        static_cast<void>(lower_test_manifest(context, manifest_with(context, module, types))),
        std::invalid_argument);
}

TEST(Validation, RequiresKnownSemanticFactsWhenEnumBackingIsDerived) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& enums{declarations.enums};
    auto& enumerators{declarations.enumerators};
    auto& strings{context.storage.strings};
    auto module{valid_native_enum_module(context)};

    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            auto const row_0_row{enums.index(row_0)};
            enums.values[row_0_row] = [&] {
                auto const original_items{declarations.items(enums.values[row_0_row])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.push_back([&] {
                    auto const new_enumerators{enumerators.append()};
                    enumerators.name[enumerators.index(new_enumerators)] = strings.intern("Active");
                    return new_enumerators;
                }());
                return declarations.range(rows);
            }();
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            enums.underlying_type[enums.index(row_0)].reset();
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            enums.bit_width[enums.index(row_0)] = 12;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            enums.signedness[enums.index(row_0)] = schema::EnumSignedness::unsigned_;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();

    EXPECT_NO_THROW(
        static_cast<void>(lower_test_manifest(context, manifest_with(context, module))));

    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{enums.clone(rows_3.front())};
            auto const row_2_row{enums.index(row_2)};
            enums.values[row_2_row] = [&] {
                auto const original_items{declarations.items(enums.values[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{enumerators.clone(rows_1.front())};
                    enumerators.initializer[enumerators.index(row_0)] =
                        strings.intern("calculate_state()");
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{enums.clone(rows_3.front())};
            auto const row_2_row{enums.index(row_2)};
            enums.values[row_2_row] = [&] {
                auto const original_items{declarations.items(enums.values[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.back() = [&] {
                    auto row_0{enumerators.clone(rows_1.back())};
                    enumerators.initializer[enumerators.index(row_0)].reset();
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            enums.bit_width[enums.index(row_0)].reset();
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{enums.clone(rows_1.front())};
            enums.signedness[enums.index(row_0)] = schema::EnumSignedness::unspecified;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(static_cast<void>(lower_test_manifest(context, manifest_with(context, module))),
                 std::invalid_argument);
}

TEST(Validation, RejectsInvalidStaticTableModuleConfiguration) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& static_tables{declarations.static_tables};
    auto module{valid_static_table_module(context)};
    normal_modules.declarations[normal_modules.index(module)] = {};
    EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));

    module = valid_static_table_module(context);
    normal_modules.settings[normal_modules.index(module)] = [&] {
        auto row_0{declarations.module_settings_store.clone(
            normal_modules.settings[normal_modules.index(module)])};
        declarations.module_settings_store.source[declarations.module_settings_store.index(row_0)] =
            context.storage.strings.intern("Tables.cpp");
        return row_0;
    }();
    EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));

    module = valid_static_table_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{static_tables.clone(rows_1.front())};
            static_tables.rows[static_tables.index(row_0)] = {};
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_static_table_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{static_tables.clone(rows_1.front())};
            static_tables.columns[static_tables.index(row_0)] = {};
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsInvalidStaticTableNamesAndTypes) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules_schemas{declarations.normal_modules};
    auto& static_tables{declarations.static_tables};
    auto& static_table_rows{declarations.static_table_rows};
    auto& strings{context.storage.strings};
    auto module{valid_static_table_module(context)};
    normal_modules_schemas.declarations[normal_modules_schemas.index(module)] = [&] {
        auto const original_items{declarations.items(
            normal_modules_schemas.declarations[normal_modules_schemas.index(module)])};
        auto rows{std::vector(original_items.begin(), original_items.end())};
        rows.push_back(
            declarations
                .items(normal_modules_schemas.declarations[normal_modules_schemas.index(module)])
                .front());
        return declarations.range(rows);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_static_table_module(context);
    normal_modules_schemas.declarations[normal_modules_schemas.index(module)] = [&] {
        auto const original_items{declarations.items(
            normal_modules_schemas.declarations[normal_modules_schemas.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{static_tables.clone(rows_1.front())};
            auto const row_0_row{static_tables.index(row_0)};
            static_tables.rows[row_0_row] = [&] {
                auto const original_items{declarations.items(static_tables.rows[row_0_row])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.push_back([&] {
                    auto const new_static_table_rows{static_table_rows.append()};
                    static_table_rows.name[static_table_rows.index(new_static_table_rows)] =
                        strings.intern("first");
                    return new_static_table_rows;
                }());
                return declarations.range(rows);
            }();
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_static_table_module(context);
    normal_modules_schemas.declarations[normal_modules_schemas.index(module)] = [&] {
        auto const original_items{declarations.items(
            normal_modules_schemas.declarations[normal_modules_schemas.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{static_tables.clone(rows_3.front())};
            auto const row_2_row{static_tables.index(row_2)};
            static_tables.rows[row_2_row] = [&] {
                auto const original_items{declarations.items(static_tables.rows[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{static_table_rows.clone(rows_1.front())};
                    static_table_rows.name[static_table_rows.index(row_0)] =
                        strings.intern("bad-name");
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_static_table_module(context);
    normal_modules_schemas.declarations[normal_modules_schemas.index(module)] = [&] {
        auto const original_items{declarations.items(
            normal_modules_schemas.declarations[normal_modules_schemas.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{static_tables.clone(rows_1.front())};
            auto const row_0_row{static_tables.index(row_0)};
            static_tables.columns[row_0_row] = [&] {
                auto const original_items{declarations.items(static_tables.columns[row_0_row])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.push_back(
                    declarations
                        .items(static_tables.columns[static_tables.index(
                            declarations
                                .items(normal_modules_schemas
                                           .declarations[declarations.normal_modules.index(module)])
                                .front())])
                        .front());
                return declarations.range(rows);
            }();
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_static_table_module(context);
    normal_modules_schemas.declarations[normal_modules_schemas.index(module)] = [&] {
        auto const original_items{declarations.items(
            normal_modules_schemas.declarations[normal_modules_schemas.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{static_tables.clone(rows_3.front())};
            auto const row_2_row{static_tables.index(row_2)};
            static_tables.columns[row_2_row] = [&] {
                auto const original_items{declarations.items(static_tables.columns[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{declarations.static_table_columns.clone(rows_1.front())};
                    declarations.static_table_columns
                        .type[declarations.static_table_columns.index(row_0)] =
                        make_test_type_ref(context, "@missing");
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsStaticTableColumnsCollidingWithGeneratedApis) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& static_tables{declarations.static_tables};
    for (auto const& name :
         {"FValues", "num_rows", "num", "apply_arrays", "apply_array_pairs", "first_index"}) {
        SCOPED_TRACE(name);
        auto module{valid_static_table_module(context)};
        normal_modules.declarations[normal_modules.index(module)] = [&] {
            auto const original_items{
                declarations.items(normal_modules.declarations[normal_modules.index(module)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{static_tables.clone(rows_3.front())};
                auto const row_2_row{static_tables.index(row_2)};
                static_tables.columns[row_2_row] = [&] {
                    auto const original_items{declarations.items(static_tables.columns[row_2_row])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1.front() = [&] {
                        auto row_0{declarations.static_table_columns.clone(rows_1.front())};
                        declarations.static_table_columns
                            .name[declarations.static_table_columns.index(row_0)] =
                            context.storage.strings.intern(name);
                        return row_0;
                    }();
                    return declarations.range(rows_1);
                }();
                return row_2;
            }();
            return declarations.range(rows_3);
        }();
        EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                     std::invalid_argument);
    }
}

TEST(Validation, RejectsInvalidStaticTableGroups) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules_schemas{declarations.normal_modules};
    auto& static_tables{declarations.static_tables};
    auto& static_table_groups{declarations.static_table_groups};

    auto& strings{context.storage.strings};
    auto module{valid_static_table_module(context)};
    normal_modules_schemas.declarations[normal_modules_schemas.index(module)] = [&] {
        auto const original_items{declarations.items(
            normal_modules_schemas.declarations[normal_modules_schemas.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{static_tables.clone(rows_1.front())};
            static_tables.groups[static_tables.index(row_0)] =
                declarations.range<lispb::codegen::StaticTableGroupId>({[&] {
                    auto const new_static_table_groups{static_table_groups.append()};
                    auto const new_static_table_groups_row{
                        static_table_groups.index(new_static_table_groups)};
                    static_table_groups.name[new_static_table_groups_row] = strings.intern("point");
                    static_table_groups.type[new_static_table_groups_row] =
                        make_test_type_ref(context, "FPoint");
                    static_table_groups.columns[new_static_table_groups_row] =
                        declarations.range<lispb::StringId>({strings.intern("ids")});
                    return new_static_table_groups;
                }()});
            return row_0;
        }();
        return declarations.range(rows_1);
    }();

    auto duplicate_group{module};
    normal_modules_schemas.declarations[normal_modules_schemas.index(duplicate_group)] = [&] {
        auto const original_items{declarations.items(
            normal_modules_schemas.declarations[normal_modules_schemas.index(duplicate_group)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{static_tables.clone(rows_1.front())};
            auto const row_0_row{static_tables.index(row_0)};
            static_tables.groups[row_0_row] = [&] {
                auto const original_items{declarations.items(static_tables.groups[row_0_row])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.push_back(
                    declarations
                        .items(static_tables.groups[static_tables.index(
                            declarations
                                .items(normal_modules_schemas.declarations
                                           [declarations.normal_modules.index(duplicate_group)])
                                .front())])
                        .front());
                return declarations.range(rows);
            }();
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, duplicate_group)),
                 std::invalid_argument);

    auto invalid_name{module};
    normal_modules_schemas.declarations[normal_modules_schemas.index(invalid_name)] = [&] {
        auto const original_items{declarations.items(
            normal_modules_schemas.declarations[normal_modules_schemas.index(invalid_name)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{static_tables.clone(rows_3.front())};
            auto const row_2_row{static_tables.index(row_2)};
            static_tables.groups[row_2_row] = [&] {
                auto const original_items{declarations.items(static_tables.groups[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{static_table_groups.clone(rows_1.front())};
                    static_table_groups.name[static_table_groups.index(row_0)] =
                        strings.intern("bad-name");
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, invalid_name)),
                 std::invalid_argument);

    auto unknown_type{module};
    normal_modules_schemas.declarations[normal_modules_schemas.index(unknown_type)] = [&] {
        auto const original_items{declarations.items(
            normal_modules_schemas.declarations[normal_modules_schemas.index(unknown_type)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{static_tables.clone(rows_3.front())};
            auto const row_2_row{static_tables.index(row_2)};
            static_tables.groups[row_2_row] = [&] {
                auto const original_items{declarations.items(static_tables.groups[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{static_table_groups.clone(rows_1.front())};
                    static_table_groups.type[static_table_groups.index(row_0)] =
                        make_test_type_ref(context, "@missing");
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, unknown_type)),
                 std::invalid_argument);

    auto empty_columns{module};
    normal_modules_schemas.declarations[normal_modules_schemas.index(empty_columns)] = [&] {
        auto const original_items{declarations.items(
            normal_modules_schemas.declarations[normal_modules_schemas.index(empty_columns)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{static_tables.clone(rows_3.front())};
            auto const row_2_row{static_tables.index(row_2)};
            static_tables.groups[row_2_row] = [&] {
                auto const original_items{declarations.items(static_tables.groups[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{static_table_groups.clone(rows_1.front())};
                    static_table_groups.columns[static_table_groups.index(row_0)] = {};
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, empty_columns)),
                 std::invalid_argument);

    auto unknown_column{module};
    normal_modules_schemas.declarations[normal_modules_schemas.index(unknown_column)] = [&] {
        auto const original_items{declarations.items(
            normal_modules_schemas.declarations[normal_modules_schemas.index(unknown_column)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{static_tables.clone(rows_3.front())};
            auto const row_2_row{static_tables.index(row_2)};
            static_tables.groups[row_2_row] = [&] {
                auto const original_items{declarations.items(static_tables.groups[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{static_table_groups.clone(rows_1.front())};
                    static_table_groups.columns[static_table_groups.index(row_0)] =
                        declarations.range<lispb::StringId>({strings.intern("missing")});
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, unknown_column)),
                 std::invalid_argument);

    auto duplicate_column{module};
    normal_modules_schemas.declarations[normal_modules_schemas.index(duplicate_column)] = [&] {
        auto const original_items{declarations.items(
            normal_modules_schemas.declarations[normal_modules_schemas.index(duplicate_column)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{static_tables.clone(rows_3.front())};
            auto const row_2_row{static_tables.index(row_2)};
            static_tables.groups[row_2_row] = [&] {
                auto const original_items{declarations.items(static_tables.groups[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{static_table_groups.clone(rows_1.front())};
                    static_table_groups.columns[static_table_groups.index(row_0)] =
                        declarations.range<lispb::StringId>(
                            {strings.intern("ids"), strings.intern("ids")});
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, duplicate_column)),
                 std::invalid_argument);
}

TEST(Validation, RejectsStaticTableGroupGetterCollisions) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& static_tables{declarations.static_tables};
    auto& static_table_columns{declarations.static_table_columns};

    auto& static_table_groups{declarations.static_table_groups};
    auto& strings{context.storage.strings};
    auto module{valid_static_table_module(context)};
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{static_tables.clone(rows_1.front())};
            auto const row_0_row{static_tables.index(row_0)};
            static_tables.columns[row_0_row] = [&] {
                auto const original_items{declarations.items(static_tables.columns[row_0_row])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.push_back([&] {
                    auto const new_static_table_columns{static_table_columns.append()};
                    auto const new_static_table_columns_row{
                        static_table_columns.index(new_static_table_columns)};
                    static_table_columns.name[new_static_table_columns_row] =
                        strings.intern("get_point");
                    static_table_columns.type[new_static_table_columns_row] =
                        make_test_type_ref(context, "float");
                    return new_static_table_columns;
                }());
                return declarations.range(rows);
            }();
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{static_tables.clone(rows_1.front())};
            static_tables.groups[static_tables.index(row_0)] =
                declarations.range<lispb::codegen::StaticTableGroupId>({[&] {
                    auto const new_static_table_groups{static_table_groups.append()};
                    auto const new_static_table_groups_row{
                        static_table_groups.index(new_static_table_groups)};
                    static_table_groups.name[new_static_table_groups_row] = strings.intern("point");
                    static_table_groups.type[new_static_table_groups_row] =
                        make_test_type_ref(context, "FPoint");
                    static_table_groups.columns[new_static_table_groups_row] =
                        declarations.range<lispb::StringId>({strings.intern("ids")});
                    return new_static_table_groups;
                }()});
            return row_0;
        }();
        return declarations.range(rows_1);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsEmptyModuleOutputs) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& settings{declarations.module_settings_store};
    auto const module{valid_soa_module(context)};
    auto const row{settings.index(normal_modules.settings[normal_modules.index(module)])};
    settings.header[row] = {};

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsEmptyOptionalModuleSettings) {
    for (auto const& [name, column] :
         {std::pair{"header_include", &ModuleSettingsStore::header_include},
          std::pair{"namespace_name", &ModuleSettingsStore::namespace_name}}) {
        SCOPED_TRACE(name);
        CompilerTestContext context;
        auto& declarations{context.storage.declarations};
        auto& normal_modules{declarations.normal_modules};
        auto& settings{declarations.module_settings_store};
        auto const module{valid_soa_module(context)};
        auto const row{settings.index(normal_modules.settings[normal_modules.index(module)])};
        (settings.*column)[row] = context.storage.strings.intern("");

        EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                     std::invalid_argument);
    }
}

TEST(Validation, RejectsDuplicateAndEmptyIncludeOrderPrefixes) {
    for (auto const& prefixes : {std::vector<std::string_view>{"Project/", "Project/"},
                                 std::vector<std::string_view>{""}}) {
        SCOPED_TRACE(::testing::PrintToString(prefixes));
        CompilerTestContext context;
        auto& declarations{context.storage.declarations};
        auto& normal_modules{declarations.normal_modules};
        auto& settings{declarations.module_settings_store};
        auto const module{valid_soa_module(context)};
        auto const row{settings.index(normal_modules.settings[normal_modules.index(module)])};
        std::vector<StringId> values;
        for (auto const prefix : prefixes) {
            values.push_back(context.storage.strings.intern(prefix));
        }
        settings.include_order[row] = declarations.range(values);

        EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                     std::invalid_argument);
    }
}

TEST(Validation, RejectsMalformedQualifiedNamespaces) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& settings{declarations.module_settings_store};
    auto const module{valid_soa_module(context)};
    auto const row{settings.index(normal_modules.settings[normal_modules.index(module)])};
    settings.namespace_name[row] = context.storage.strings.intern("project:::invalid");

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsDuplicateModuleNames) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& settings{declarations.module_settings_store};
    auto& strings{context.storage.strings};
    auto const first{valid_soa_module(context)};
    auto const second{valid_soa_module(context)};
    auto const row{settings.index(normal_modules.settings[normal_modules.index(second)])};
    settings.header[row] = strings.intern("Other.h");
    settings.source[row] = strings.intern("Other.cpp");
    lispb::codegen::Manifest const manifest{
        manifest_schema_version, make_test_registry(context), declarations, {first, second}};

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
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& soas{declarations.soas};
    auto& strings{context.storage.strings};
    for (auto const& invalid : {"class", "__generated", "_Reserved"}) {
        SCOPED_TRACE(invalid);
        auto module{valid_soa_module(context)};
        normal_modules.declarations[normal_modules.index(module)] = [&] {
            auto const original_items{
                declarations.items(normal_modules.declarations[normal_modules.index(module)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{soas.clone(rows_3.front())};
                auto const row_2_row{soas.index(row_2)};
                soas.members[row_2_row] = [&] {
                    auto const original_items{declarations.items(soas.members[row_2_row])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1.front() = [&] {
                        auto row_0{declarations.soa_members.clone(rows_1.front())};
                        declarations.soa_members.name[declarations.soa_members.index(row_0)] =
                            strings.intern(invalid);
                        return row_0;
                    }();
                    return declarations.range(rows_1);
                }();
                return row_2;
            }();
            return declarations.range(rows_3);
        }();
        EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                     std::invalid_argument);
    }

    auto module{valid_soa_module(context)};
    normal_modules.settings[normal_modules.index(module)] = [&] {
        auto row_0{declarations.module_settings_store.clone(
            normal_modules.settings[normal_modules.index(module)])};
        declarations.module_settings_store
            .namespace_name[declarations.module_settings_store.index(row_0)] =
            strings.intern("project::namespace");
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
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto module{valid_soa_module(context)};
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{declarations.soas.clone(rows_1.front())};
            declarations.soas.name[declarations.soas.index(row_0)] = {};
            return row_0;
        }();
        return declarations.range(rows_1);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsDuplicateSoaMembers) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& soas{declarations.soas};
    auto& soa_members{declarations.soa_members};
    auto& strings{context.storage.strings};
    auto module{valid_soa_module(context)};
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{soas.clone(rows_1.front())};
            auto const row_0_row{soas.index(row_0)};
            soas.members[row_0_row] = [&] {
                auto const original_items{declarations.items(soas.members[row_0_row])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.push_back([&] {
                    auto const new_soa_members{soa_members.append()};
                    auto const new_soa_members_row{soa_members.index(new_soa_members)};
                    soa_members.name[new_soa_members_row] = strings.intern("values");
                    soa_members.kind[new_soa_members_row] = SoaMemberKind::array;
                    soa_members.type[new_soa_members_row] = make_test_type_ref(context, "float");
                    return new_soa_members;
                }());
                return declarations.range(rows);
            }();
            return row_0;
        }();
        return declarations.range(rows_1);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsMalformedSoaMemberIdentifiers) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& soas{declarations.soas};
    auto module{valid_soa_module(context)};
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{soas.clone(rows_3.front())};
            auto const row_2_row{soas.index(row_2)};
            soas.members[row_2_row] = [&] {
                auto const original_items{declarations.items(soas.members[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{declarations.soa_members.clone(rows_1.front())};
                    declarations.soa_members.name[declarations.soa_members.index(row_0)] =
                        context.storage.strings.intern("bad-name");
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsIncompleteSoaFieldMaskDeclarations) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& soas{declarations.soas};
    auto module{valid_mask_soa_module(context)};
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{soas.clone(rows_1.front())};
            soas.field_enum_name[soas.index(row_0)].reset();
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_mask_soa_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{soas.clone(rows_1.front())};
            soas.field_mask_name[soas.index(row_0)].reset();
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsInvalidSoaFieldMaskMembers) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& soas{declarations.soas};
    auto& soa_members{declarations.soa_members};
    auto module{valid_mask_soa_module(context)};
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{soas.clone(rows_3.front())};
            auto const row_2_row{soas.index(row_2)};
            soas.members[row_2_row] = [&] {
                auto const original_items{declarations.items(soas.members[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{soa_members.clone(rows_1.front())};
                    soa_members.type[soa_members.index(row_0)] =
                        make_test_type_ref(context, "uint8");
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_mask_soa_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{soas.clone(rows_3.front())};
            auto const row_2_row{soas.index(row_2)};
            soas.members[row_2_row] = [&] {
                auto const original_items{declarations.items(soas.members[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.back() = [&] {
                    auto row_0{soa_members.clone(rows_1.back())};
                    soa_members.mask_field[soa_members.index(row_0)] = false;
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_soa_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{soas.clone(rows_3.front())};
            auto const row_2_row{soas.index(row_2)};
            soas.members[row_2_row] = [&] {
                auto const original_items{declarations.items(soas.members[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{soa_members.clone(rows_1.front())};
                    soa_members.mask_field[soa_members.index(row_0)] = true;
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsInvalidSoaMaskDimensions) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& soas{declarations.soas};
    auto& soa_members{declarations.soa_members};
    auto& soa_mask_dimensions{declarations.soa_mask_dimensions};
    auto& strings{context.storage.strings};
    auto module{valid_mask_soa_module(context)};

    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{soas.clone(rows_3.front())};
            auto const row_2_row{soas.index(row_2)};
            soas.members[row_2_row] = [&] {
                auto const original_items{declarations.items(soas.members[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.back() = [&] {
                    auto row_0{soa_members.clone(rows_1.back())};
                    auto const row_0_row{soa_members.index(row_0)};
                    soa_members.mask_dimensions[row_0_row] = [&] {
                        auto const original_items{
                            declarations.items(soa_members.mask_dimensions[row_0_row])};
                        auto rows{std::vector(original_items.begin(), original_items.end())};
                        rows.push_back([&] {
                            auto const new_soa_mask_dimensions{soa_mask_dimensions.append()};
                            soa_mask_dimensions
                                .index_name[soa_mask_dimensions.index(new_soa_mask_dimensions)] =
                                strings.intern("field_index");
                            soa_mask_dimensions
                                .extent[soa_mask_dimensions.index(new_soa_mask_dimensions)] =
                                strings.intern("4");
                            return new_soa_mask_dimensions;
                        }());
                        return declarations.range(rows);
                    }();
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{soas.clone(rows_3.front())};
            auto const row_2_row{soas.index(row_2)};
            soas.members[row_2_row] = [&] {
                auto const original_items{declarations.items(soas.members[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.back() = [&] {
                    auto row_0{soa_members.clone(rows_1.back())};
                    auto const row_0_row{soa_members.index(row_0)};
                    soa_members.mask_dimensions[row_0_row] = [&] {
                        auto const original_items{
                            declarations.items(soa_members.mask_dimensions[row_0_row])};
                        auto rows{std::vector(original_items.begin(), original_items.end())};
                        rows.push_back([&] {
                            auto const new_soa_mask_dimensions{soa_mask_dimensions.append()};
                            soa_mask_dimensions
                                .index_name[soa_mask_dimensions.index(new_soa_mask_dimensions)] =
                                strings.intern("field_index");
                            soa_mask_dimensions
                                .extent[soa_mask_dimensions.index(new_soa_mask_dimensions)] =
                                strings.intern("2");
                            return new_soa_mask_dimensions;
                        }());
                        return declarations.range(rows);
                    }();
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_mask_soa_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{soas.clone(rows_3.front())};
            auto const row_2_row{soas.index(row_2)};
            soas.members[row_2_row] = [&] {
                auto const original_items{declarations.items(soas.members[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.back() = [&] {
                    auto row_0{soa_members.clone(rows_1.back())};
                    auto const row_0_row{soa_members.index(row_0)};
                    soa_members.mask_dimensions[row_0_row] = [&] {
                        auto const original_items{
                            declarations.items(soa_members.mask_dimensions[row_0_row])};
                        auto rows{std::vector(original_items.begin(), original_items.end())};
                        rows.push_back([&] {
                            auto const new_soa_mask_dimensions{soa_mask_dimensions.append()};
                            soa_mask_dimensions
                                .index_name[soa_mask_dimensions.index(new_soa_mask_dimensions)] =
                                strings.intern("bad-index");
                            soa_mask_dimensions
                                .extent[soa_mask_dimensions.index(new_soa_mask_dimensions)] =
                                strings.intern("4");
                            return new_soa_mask_dimensions;
                        }());
                        return declarations.range(rows);
                    }();
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsMembersCollidingWithGeneratedStorageApis) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& soas{declarations.soas};
    for (auto const& name : {"num", "reset", "copy_element"}) {
        SCOPED_TRACE(name);
        auto module{valid_soa_module(context)};
        normal_modules.declarations[normal_modules.index(module)] = [&] {
            auto const original_items{
                declarations.items(normal_modules.declarations[normal_modules.index(module)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{soas.clone(rows_3.front())};
                auto const row_2_row{soas.index(row_2)};
                soas.members[row_2_row] = [&] {
                    auto const original_items{declarations.items(soas.members[row_2_row])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1.front() = [&] {
                        auto row_0{declarations.soa_members.clone(rows_1.front())};
                        declarations.soa_members.name[declarations.soa_members.index(row_0)] =
                            context.storage.strings.intern(name);
                        return row_0;
                    }();
                    return declarations.range(rows_1);
                }();
                return row_2;
            }();
            return declarations.range(rows_3);
        }();
        EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                     std::invalid_argument);
    }
}

TEST(Validation, RejectsUnknownSoaTypeReferences) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& soas{declarations.soas};
    auto module{valid_soa_module(context)};
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{soas.clone(rows_3.front())};
            auto const row_2_row{soas.index(row_2)};
            soas.members[row_2_row] = [&] {
                auto const original_items{declarations.items(soas.members[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{declarations.soa_members.clone(rows_1.front())};
                    declarations.soa_members.type[declarations.soa_members.index(row_0)] =
                        make_test_type_ref(context, "@missing");
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsCollidingGeneratedSoaTypeNames) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto module{valid_soa_module(context)};
    auto second{declarations.soas.clone(
        declarations.items(normal_modules.declarations[normal_modules.index(module)]).front())};
    declarations.soas.name[declarations.soas.index(second)] =
        context.storage.strings.intern("F_Data");
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows{std::vector(original_items.begin(), original_items.end())};
        rows.push_back(second);
        return declarations.range(rows);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsReservedSoaNamespaceNames) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto module{valid_soa_module(context)};
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{declarations.soas.clone(rows_1.front())};
            declarations.soas.name[declarations.soas.index(row_0)] =
                context.storage.strings.intern("Class");
            return row_0;
        }();
        return declarations.range(rows_1);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsBlankSoaUsingDeclarations) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& soas{declarations.soas};
    for (auto const* declaration : {"", " \t\r\n"}) {
        SCOPED_TRACE(declaration);
        auto module{valid_soa_module(context)};
        normal_modules.declarations[normal_modules.index(module)] = [&] {
            auto const original_items{
                declarations.items(normal_modules.declarations[normal_modules.index(module)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{soas.clone(rows_1.front())};
                auto const row_0_row{soas.index(row_0)};
                soas.using_declarations[row_0_row] = [&] {
                    auto const original_items{
                        declarations.items(soas.using_declarations[row_0_row])};
                    auto rows{std::vector(original_items.begin(), original_items.end())};
                    rows.push_back(context.storage.strings.intern(declaration));
                    return declarations.range(rows);
                }();
                return row_0;
            }();
            return declarations.range(rows_1);
        }();

        EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                     std::invalid_argument);
    }
}

TEST(Validation, RejectsDuplicateCustomSoaFunctionParameters) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& functions_schemas{declarations.functions};

    auto& parameters_schemas{declarations.parameters};
    auto& strings{context.storage.strings};
    auto module{valid_soa_module(context)};
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{declarations.soas.clone(rows_1.front())};
            declarations.soas.functions[declarations.soas.index(row_0)] =
                declarations.range<lispb::codegen::FunctionSchemaId>({[&] {
                    auto const new_functions{functions_schemas.append()};
                    auto const new_functions_row{functions_schemas.index(new_functions)};
                    functions_schemas.name[new_functions_row] = strings.intern("set");
                    functions_schemas.return_type[new_functions_row] =
                        make_test_type_ref(context, "void");
                    functions_schemas.parameters[new_functions_row] =
                        declarations.range<lispb::codegen::ParameterId>(
                            {[&] {
                                 auto const new_parameters{parameters_schemas.append()};
                                 auto const new_parameters_row{
                                     parameters_schemas.index(new_parameters)};
                                 parameters_schemas.type[new_parameters_row] =
                                     make_test_type_ref(context, "int32");
                                 parameters_schemas.name[new_parameters_row] =
                                     strings.intern("value");
                                 return new_parameters;
                             }(),
                             [&] {
                                 auto const new_parameters{parameters_schemas.append()};
                                 auto const new_parameters_row{
                                     parameters_schemas.index(new_parameters)};
                                 parameters_schemas.type[new_parameters_row] =
                                     make_test_type_ref(context, "float");
                                 parameters_schemas.name[new_parameters_row] =
                                     strings.intern("value");
                                 return new_parameters;
                             }()});
                    return new_functions;
                }()});
            return row_0;
        }();
        return declarations.range(rows_1);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsNonTrailingCustomSoaDefaultArguments) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& soas{declarations.soas};
    auto& functions_schemas{declarations.functions};

    auto& parameters_schemas{declarations.parameters};
    auto& strings{context.storage.strings};
    auto module{valid_soa_module(context)};
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{soas.clone(rows_1.front())};
            soas.functions[soas.index(row_0)] =
                declarations.range<lispb::codegen::FunctionSchemaId>({[&] {
                    auto const new_functions{functions_schemas.append()};
                    auto const new_functions_row{functions_schemas.index(new_functions)};
                    functions_schemas.name[new_functions_row] = strings.intern("set");
                    functions_schemas.return_type[new_functions_row] =
                        make_test_type_ref(context, "void");
                    functions_schemas.parameters[new_functions_row] =
                        declarations.range<lispb::codegen::ParameterId>(
                            {[&] {
                                 auto const new_parameters{parameters_schemas.append()};
                                 auto const new_parameters_row{
                                     parameters_schemas.index(new_parameters)};
                                 parameters_schemas.type[new_parameters_row] =
                                     make_test_type_ref(context, "int32");
                                 parameters_schemas.name[new_parameters_row] =
                                     strings.intern("first");
                                 parameters_schemas.default_value[new_parameters_row] =
                                     strings.intern("0");
                                 return new_parameters;
                             }(),
                             [&] {
                                 auto const new_parameters{parameters_schemas.append()};
                                 auto const new_parameters_row{
                                     parameters_schemas.index(new_parameters)};
                                 parameters_schemas.type[new_parameters_row] =
                                     make_test_type_ref(context, "int32");
                                 parameters_schemas.name[new_parameters_row] =
                                     strings.intern("second");
                                 return new_parameters;
                             }()});
                    return new_functions;
                }()});
            return row_0;
        }();
        return declarations.range(rows_1);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_soa_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{soas.clone(rows_1.front())};
            soas.functions[soas.index(row_0)] =
                declarations.range<lispb::codegen::FunctionSchemaId>({[&] {
                    auto const new_functions{functions_schemas.append()};
                    auto const new_functions_row{functions_schemas.index(new_functions)};
                    functions_schemas.name[new_functions_row] = strings.intern("set");
                    functions_schemas.return_type[new_functions_row] =
                        make_test_type_ref(context, "void");
                    functions_schemas.parameters[new_functions_row] =
                        declarations.range<lispb::codegen::ParameterId>({[&] {
                            auto const new_parameters{parameters_schemas.append()};
                            auto const new_parameters_row{parameters_schemas.index(new_parameters)};
                            parameters_schemas.type[new_parameters_row] =
                                make_test_type_ref(context, "int32");
                            parameters_schemas.name[new_parameters_row] = strings.intern("value");
                            parameters_schemas.default_value[new_parameters_row] =
                                strings.intern(" \t");
                            return new_parameters;
                        }()});
                    return new_functions;
                }()});
            return row_0;
        }();
        return declarations.range(rows_1);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsUnknownCustomSoaDependencies) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& soas{declarations.soas};
    auto& functions_schemas{declarations.functions};

    auto& strings{context.storage.strings};
    auto module{valid_soa_module(context)};
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{soas.clone(rows_1.front())};
            soas.functions[soas.index(row_0)] =
                declarations.range<lispb::codegen::FunctionSchemaId>({[&] {
                    auto const new_functions{functions_schemas.append()};
                    auto const new_functions_row{functions_schemas.index(new_functions)};
                    functions_schemas.name[new_functions_row] = strings.intern("reset_values");
                    functions_schemas.return_type[new_functions_row] =
                        make_test_type_ref(context, "void");
                    functions_schemas.dependencies[new_functions_row] =
                        declarations.range<lispb::StringId>({strings.intern("missing")});
                    return new_functions;
                }()});
            return row_0;
        }();
        return declarations.range(rows_1);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_soa_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{soas.clone(rows_1.front())};
            soas.functions[soas.index(row_0)] =
                declarations.range<lispb::codegen::FunctionSchemaId>({[&] {
                    auto const new_functions{functions_schemas.append()};
                    auto const new_functions_row{functions_schemas.index(new_functions)};
                    functions_schemas.name[new_functions_row] = strings.intern("reset_values");
                    functions_schemas.return_type[new_functions_row] =
                        make_test_type_ref(context, "void");
                    functions_schemas.dependencies[new_functions_row] =
                        declarations.range<lispb::StringId>({strings.intern(" \t")});
                    return new_functions;
                }()});
            return row_0;
        }();
        return declarations.range(rows_1);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsInvalidCustomSoaFunctionDefinitions) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& soas{declarations.soas};
    auto& functions_schemas{declarations.functions};

    auto& strings{context.storage.strings};
    auto module{valid_soa_module(context)};
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{soas.clone(rows_1.front())};
            soas.functions[soas.index(row_0)] =
                declarations.range<lispb::codegen::FunctionSchemaId>({[&] {
                    auto const new_functions{functions_schemas.append()};
                    auto const new_functions_row{functions_schemas.index(new_functions)};
                    functions_schemas.name[new_functions_row] = strings.intern("class");
                    functions_schemas.return_type[new_functions_row] =
                        make_test_type_ref(context, "void");
                    functions_schemas.is_inline[new_functions_row] = true;
                    return new_functions;
                }()});
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_soa_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{soas.clone(rows_1.front())};
            soas.functions[soas.index(row_0)] =
                declarations.range<lispb::codegen::FunctionSchemaId>({[&] {
                    auto const new_functions{functions_schemas.append()};
                    auto const new_functions_row{functions_schemas.index(new_functions)};
                    functions_schemas.name[new_functions_row] = strings.intern("update");
                    functions_schemas.return_type[new_functions_row] =
                        make_test_type_ref(context, "void");
                    functions_schemas.is_inline[new_functions_row] = true;
                    functions_schemas.definition_in_source[new_functions_row] = true;
                    return new_functions;
                }()});
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_soa_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{soas.clone(rows_1.front())};
            soas.functions[soas.index(row_0)] =
                declarations.range<lispb::codegen::FunctionSchemaId>({[&] {
                    auto const new_functions{functions_schemas.append()};
                    auto const new_functions_row{functions_schemas.index(new_functions)};
                    functions_schemas.name[new_functions_row] =
                        strings.intern("invalid_static_const");
                    functions_schemas.return_type[new_functions_row] =
                        make_test_type_ref(context, "void");
                    functions_schemas.is_const[new_functions_row] = true;
                    functions_schemas.is_static[new_functions_row] = true;
                    return new_functions;
                }()});
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_soa_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{soas.clone(rows_1.front())};
            soas.functions[soas.index(row_0)] =
                declarations.range<lispb::codegen::FunctionSchemaId>({[&] {
                    auto const new_functions{functions_schemas.append()};
                    auto const new_functions_row{functions_schemas.index(new_functions)};
                    functions_schemas.name[new_functions_row] =
                        strings.intern("invalid_trailing_return");
                    functions_schemas.return_type[new_functions_row] =
                        make_test_type_ref(context, "void");
                    functions_schemas.trailing_return_type[new_functions_row] =
                        make_test_type_ref(context, "int32");
                    return new_functions;
                }()});
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_soa_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{soas.clone(rows_1.front())};
            soas.functions[soas.index(row_0)] =
                declarations.range<lispb::codegen::FunctionSchemaId>({[&] {
                    auto const new_functions{functions_schemas.append()};
                    auto const new_functions_row{functions_schemas.index(new_functions)};
                    functions_schemas.name[new_functions_row] = strings.intern("invalid_template");
                    functions_schemas.return_type[new_functions_row] =
                        make_test_type_ref(context, "void");
                    functions_schemas.template_parameters[new_functions_row] =
                        strings.intern(" \t");
                    return new_functions;
                }()});
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_soa_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{soas.clone(rows_1.front())};
            soas.functions[soas.index(row_0)] =
                declarations.range<lispb::codegen::FunctionSchemaId>({[&] {
                    auto const new_functions{functions_schemas.append()};
                    auto const new_functions_row{functions_schemas.index(new_functions)};
                    functions_schemas.name[new_functions_row] = strings.intern("invalid_requires");
                    functions_schemas.return_type[new_functions_row] =
                        make_test_type_ref(context, "void");
                    functions_schemas.requires_clause[new_functions_row] = strings.intern(" \t");
                    return new_functions;
                }()});
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsCustomSoaFunctionsCollidingWithGeneratedApisAndTypeNames) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& functions_schemas{declarations.functions};
    auto& strings{context.storage.strings};
    for (auto const& name : {"sort", "get_view", "FData"}) {
        SCOPED_TRACE(name);
        auto module{valid_soa_module(context)};
        normal_modules.declarations[normal_modules.index(module)] = [&] {
            auto const original_items{
                declarations.items(normal_modules.declarations[normal_modules.index(module)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{declarations.soas.clone(rows_1.front())};
                declarations.soas.functions[declarations.soas.index(row_0)] =
                    declarations.range<lispb::codegen::FunctionSchemaId>({[&] {
                        auto const new_functions{functions_schemas.append()};
                        auto const new_functions_row{functions_schemas.index(new_functions)};
                        functions_schemas.name[new_functions_row] = strings.intern(name);
                        functions_schemas.return_type[new_functions_row] =
                            make_test_type_ref(context, "void");
                        functions_schemas.is_inline[new_functions_row] = true;
                        return new_functions;
                    }()});
                return row_0;
            }();
            return declarations.range(rows_1);
        }();
        EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                     std::invalid_argument);
    }
}

TEST(Validation, RejectsMalformedExportSpecifiers) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& strings{context.storage.strings};
    auto soa{valid_soa_module(context)};
    normal_modules.declarations[normal_modules.index(soa)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(soa)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{declarations.soas.clone(rows_1.front())};
            declarations.soas.export_specifier[declarations.soas.index(row_0)] =
                strings.intern("bad-specifier");
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, soa)), std::invalid_argument);

    auto facade{valid_facade_module(context)};
    normal_modules.declarations[normal_modules.index(facade)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(facade)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{declarations.facades.clone(rows_1.front())};
            declarations.facades.export_specifier[declarations.facades.index(row_0)] =
                strings.intern("two words");
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, facade)),
                 std::invalid_argument);

    auto table{valid_static_table_module(context)};
    normal_modules.declarations[normal_modules.index(table)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(table)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{declarations.static_tables.clone(rows_1.front())};
            declarations.static_tables.export_specifier[declarations.static_tables.index(row_0)] =
                strings.intern("bad-specifier");
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, table)),
                 std::invalid_argument);
}

TEST(Validation, RejectsFacadesWithoutMethods) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto module{valid_facade_module(context)};
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{declarations.facades.clone(rows_1.front())};
            declarations.facades.methods[declarations.facades.index(row_0)] = {};
            return row_0;
        }();
        return declarations.range(rows_1);
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
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& facades{declarations.facades};
    auto module{valid_facade_module(context)};
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{facades.clone(rows_3.front())};
            auto const row_2_row{facades.index(row_2)};
            facades.methods[row_2_row] = [&] {
                auto const original_items{declarations.items(facades.methods[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{declarations.facade_methods.clone(rows_1.front())};
                    declarations.facade_methods.name[declarations.facade_methods.index(row_0)] =
                        context.storage.strings.intern("bad-name");
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsOutOfLineFacadesWithoutSourceOutput) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto module{valid_facade_module(context)};
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{declarations.facades.clone(rows_1.front())};
            declarations.facades.definitions_in_source[declarations.facades.index(row_0)] = true;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, AllowsInlineFacadesInModulesWithSourceOutput) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto module{valid_facade_module(context)};
    normal_modules.settings[normal_modules.index(module)] = [&] {
        auto row_0{declarations.module_settings_store.clone(
            normal_modules.settings[normal_modules.index(module)])};
        declarations.module_settings_store.source[declarations.module_settings_store.index(row_0)] =
            context.storage.strings.intern("Facade.cpp");
        return row_0;
    }();

    EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));
}

TEST(Validation, RejectsDuplicateFacadeMethodParameters) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& facades{declarations.facades};
    auto& parameters_schemas{declarations.parameters};

    auto& strings{context.storage.strings};
    auto module{valid_facade_module(context)};
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{facades.clone(rows_3.front())};
            auto const row_2_row{facades.index(row_2)};
            facades.methods[row_2_row] = [&] {
                auto const original_items{declarations.items(facades.methods[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{declarations.facade_methods.clone(rows_1.front())};
                    declarations.facade_methods
                        .parameters[declarations.facade_methods.index(row_0)] =
                        declarations.range<lispb::codegen::ParameterId>(
                            {[&] {
                                 auto const new_parameters{parameters_schemas.append()};
                                 auto const new_parameters_row{
                                     parameters_schemas.index(new_parameters)};
                                 parameters_schemas.type[new_parameters_row] =
                                     make_test_type_ref(context, "int32");
                                 parameters_schemas.name[new_parameters_row] =
                                     strings.intern("value");
                                 return new_parameters;
                             }(),
                             [&] {
                                 auto const new_parameters{parameters_schemas.append()};
                                 auto const new_parameters_row{
                                     parameters_schemas.index(new_parameters)};
                                 parameters_schemas.type[new_parameters_row] =
                                     make_test_type_ref(context, "float");
                                 parameters_schemas.name[new_parameters_row] =
                                     strings.intern("value");
                                 return new_parameters;
                             }()});
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsDuplicateFacadeMethodSignatures) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules_schemas{declarations.normal_modules};
    auto& facades{declarations.facades};
    auto module{valid_facade_module(context)};
    normal_modules_schemas.declarations[normal_modules_schemas.index(module)] = [&] {
        auto const original_items{declarations.items(
            normal_modules_schemas.declarations[normal_modules_schemas.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{facades.clone(rows_1.front())};
            auto const row_0_row{facades.index(row_0)};
            facades.methods[row_0_row] = [&] {
                auto const original_items{declarations.items(facades.methods[row_0_row])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.push_back(
                    declarations
                        .items(facades.methods[facades.index(
                            declarations
                                .items(normal_modules_schemas
                                           .declarations[declarations.normal_modules.index(module)])
                                .front())])
                        .front());
                return declarations.range(rows);
            }();
            return row_0;
        }();
        return declarations.range(rows_1);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsFacadeOverloadsDifferingOnlyByNoexcept) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& facades{declarations.facades};
    auto& normal_modules{declarations.normal_modules};
    auto module{valid_facade_module(context)};
    auto second{declarations.facade_methods.clone(
        declarations
            .items(facades.methods[facades.index(
                declarations.items(normal_modules.declarations[normal_modules.index(module)])
                    .front())])
            .front())};
    declarations.facade_methods.is_noexcept[declarations.facade_methods.index(second)] = true;
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{facades.clone(rows_1.front())};
            auto const row_0_row{facades.index(row_0)};
            facades.methods[row_0_row] = [&] {
                auto const original_items{declarations.items(facades.methods[row_0_row])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.push_back(second);
                return declarations.range(rows);
            }();
            return row_0;
        }();
        return declarations.range(rows_1);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsFacadeMethodsCollidingWithBind) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& facade_methods{declarations.facade_methods};

    auto& parameters_schemas{declarations.parameters};
    auto& strings{context.storage.strings};
    auto module{valid_facade_module(context)};
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{declarations.facades.clone(rows_1.front())};
            declarations.facades.methods[declarations.facades.index(row_0)] =
                declarations.range<lispb::codegen::FacadeMethodId>({[&] {
                    auto const new_facade_methods{facade_methods.append()};
                    auto const new_facade_methods_row{facade_methods.index(new_facade_methods)};
                    facade_methods.name[new_facade_methods_row] = strings.intern("bind");
                    facade_methods.return_type[new_facade_methods_row] =
                        make_test_type_ref(context, "void");
                    facade_methods.parameters[new_facade_methods_row] =
                        declarations.range<lispb::codegen::ParameterId>({[&] {
                            auto const new_parameters{parameters_schemas.append()};
                            auto const new_parameters_row{parameters_schemas.index(new_parameters)};
                            parameters_schemas.type[new_parameters_row] =
                                make_test_type_ref(context, "FTarget&");
                            parameters_schemas.name[new_parameters_row] = strings.intern("target");
                            return new_parameters;
                        }()});
                    return new_facade_methods;
                }()});
            return row_0;
        }();
        return declarations.range(rows_1);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsNonTrailingFacadeDefaultArguments) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& facades{declarations.facades};
    auto& parameters_schemas{declarations.parameters};

    auto& strings{context.storage.strings};
    auto module{valid_facade_module(context)};
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{facades.clone(rows_3.front())};
            auto const row_2_row{facades.index(row_2)};
            facades.methods[row_2_row] = [&] {
                auto const original_items{declarations.items(facades.methods[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{declarations.facade_methods.clone(rows_1.front())};
                    declarations.facade_methods
                        .parameters[declarations.facade_methods.index(row_0)] =
                        declarations.range<lispb::codegen::ParameterId>(
                            {[&] {
                                 auto const new_parameters{parameters_schemas.append()};
                                 auto const new_parameters_row{
                                     parameters_schemas.index(new_parameters)};
                                 parameters_schemas.type[new_parameters_row] =
                                     make_test_type_ref(context, "int32");
                                 parameters_schemas.name[new_parameters_row] =
                                     strings.intern("first");
                                 parameters_schemas.default_value[new_parameters_row] =
                                     strings.intern("0");
                                 return new_parameters;
                             }(),
                             [&] {
                                 auto const new_parameters{parameters_schemas.append()};
                                 auto const new_parameters_row{
                                     parameters_schemas.index(new_parameters)};
                                 parameters_schemas.type[new_parameters_row] =
                                     make_test_type_ref(context, "int32");
                                 parameters_schemas.name[new_parameters_row] =
                                     strings.intern("second");
                                 return new_parameters;
                             }()});
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsUnknownFacadeValidationDependencies) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto module{valid_facade_module(context)};
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{declarations.facades.clone(rows_1.front())};
            declarations.facades.validation_dependencies[declarations.facades.index(row_0)] =
                declarations.range<lispb::StringId>({context.storage.strings.intern("missing")});
            return row_0;
        }();
        return declarations.range(rows_1);
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsInvalidFacadeFriendAndMemberCollisions) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules_schemas{declarations.normal_modules};
    auto& facades{declarations.facades};
    auto& strings{context.storage.strings};
    auto module{valid_facade_module(context)};
    EXPECT_THROW(parse_test_manifest(context, R"((module facade :header "Facade.h"
      (facade FFacade :target FTarget :friend-kind union (method reset void))))"),
                 lispb::codegen::sexpr::SourceError);

    module = valid_facade_module(context);
    normal_modules_schemas.declarations[normal_modules_schemas.index(module)] = [&] {
        auto const original_items{declarations.items(
            normal_modules_schemas.declarations[normal_modules_schemas.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{facades.clone(rows_1.front())};
            facades.friends[facades.index(row_0)] =
                declarations.range<lispb::StringId>({strings.intern("class")});
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_facade_module(context);
    normal_modules_schemas.declarations[normal_modules_schemas.index(module)] = [&] {
        auto const original_items{declarations.items(
            normal_modules_schemas.declarations[normal_modules_schemas.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{facades.clone(rows_1.front())};
            facades.target_member_name[facades.index(row_0)] = strings.intern("bind");
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_facade_module(context);
    normal_modules_schemas.declarations[normal_modules_schemas.index(module)] = [&] {
        auto const original_items{declarations.items(
            normal_modules_schemas.declarations[normal_modules_schemas.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{facades.clone(rows_3.front())};
            auto const row_2_row{facades.index(row_2)};
            facades.methods[row_2_row] = [&] {
                auto const original_items{declarations.items(facades.methods[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.front() = [&] {
                    auto row_0{declarations.facade_methods.clone(rows_1.front())};
                    declarations.facade_methods.name[declarations.facade_methods.index(row_0)] =
                        facades.target_member_name[facades.index(
                            declarations
                                .items(normal_modules_schemas
                                           .declarations[declarations.normal_modules.index(module)])
                                .front())];
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsDuplicateUmbrellaHeaders) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& umbrella_modules{declarations.umbrella_modules};
    auto& module_settings_store{declarations.module_settings_store};
    auto& strings{context.storage.strings};
    auto const module{umbrella_modules.append()};
    auto const module_row{umbrella_modules.index(module)};
    umbrella_modules.settings[module_row] = [&] {
        auto const new_module_settings_store{module_settings_store.append()};
        auto const new_module_settings_store_row{
            module_settings_store.index(new_module_settings_store)};
        module_settings_store.name[new_module_settings_store_row] = strings.intern("all");
        module_settings_store.header[new_module_settings_store_row] = strings.intern("All.h");
        return new_module_settings_store;
    }();
    umbrella_modules.headers[module_row] =
        declarations.range<lispb::StringId>({strings.intern("Value.h"), strings.intern("Value.h")});

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsUmbrellaModulesWithoutHeaders) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& module_settings_store{declarations.module_settings_store};
    auto& strings{context.storage.strings};
    auto const module{declarations.umbrella_modules.append()};
    declarations.umbrella_modules.settings[declarations.umbrella_modules.index(module)] = [&] {
        auto const new_module_settings_store{module_settings_store.append()};
        auto const new_module_settings_store_row{
            module_settings_store.index(new_module_settings_store)};
        module_settings_store.name[new_module_settings_store_row] = strings.intern("all");
        module_settings_store.header[new_module_settings_store_row] = strings.intern("All.h");
        return new_module_settings_store;
    }();

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsUnsupportedUmbrellaSourceAndNamespaceSettings) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& umbrella_modules{declarations.umbrella_modules};
    auto& module_settings_store{declarations.module_settings_store};
    auto& strings{context.storage.strings};
    auto const module{umbrella_modules.append()};
    auto const module_row{umbrella_modules.index(module)};
    umbrella_modules.settings[module_row] = [&] {
        auto const new_module_settings_store{module_settings_store.append()};
        auto const new_module_settings_store_row{
            module_settings_store.index(new_module_settings_store)};
        module_settings_store.name[new_module_settings_store_row] = strings.intern("all");
        module_settings_store.header[new_module_settings_store_row] = strings.intern("All.h");
        module_settings_store.source[new_module_settings_store_row] = strings.intern("All.cpp");
        return new_module_settings_store;
    }();
    umbrella_modules.headers[module_row] =
        declarations.range<lispb::StringId>({strings.intern("Value.h")});
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    umbrella_modules.settings[module_row] = [&] {
        auto row_0{module_settings_store.clone(umbrella_modules.settings[module_row])};
        module_settings_store.source[module_settings_store.index(row_0)].reset();
        return row_0;
    }();
    umbrella_modules.settings[module_row] = [&] {
        auto row_0{module_settings_store.clone(umbrella_modules.settings[module_row])};
        module_settings_store.namespace_name[module_settings_store.index(row_0)] =
            strings.intern("project");
        return row_0;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsInvalidSettingsDefinitions) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& settings_modules{declarations.settings_modules};
    auto& module_settings_store{declarations.module_settings_store};
    auto& setting_categories{declarations.setting_categories};
    auto& settings_schemas{declarations.settings};
    auto& setting_controls{declarations.setting_controls};
    auto& strings{context.storage.strings};
    auto module = [&] {
        auto const new_settings_modules{settings_modules.append()};
        auto const new_settings_modules_row{settings_modules.index(new_settings_modules)};
        settings_modules.settings[new_settings_modules_row] = [&] {
            auto const new_module_settings_store{module_settings_store.append()};
            auto const new_module_settings_store_row{
                module_settings_store.index(new_module_settings_store)};
            module_settings_store.name[new_module_settings_store_row] = strings.intern("settings");
            module_settings_store.header[new_module_settings_store_row] =
                strings.intern("Settings.h");
            module_settings_store.source[new_module_settings_store_row] =
                strings.intern("Settings.cpp");
            return new_module_settings_store;
        }();
        settings_modules.api_name[new_settings_modules_row] = strings.intern("TSettingsAccess");
        settings_modules.state_name[new_settings_modules_row] = strings.intern("FSettingsState");
        settings_modules.categories[new_settings_modules_row] =
            declarations.range<lispb::codegen::SettingCategoryId>({[&] {
                auto const new_setting_categories{setting_categories.append()};
                auto const new_setting_categories_row{
                    setting_categories.index(new_setting_categories)};
                setting_categories.name[new_setting_categories_row] = strings.intern("video");
                setting_categories.label[new_setting_categories_row] = strings.intern("Video");
                return new_setting_categories;
            }()});
        settings_modules.settings_list[new_settings_modules_row] =
            declarations.range<lispb::codegen::SettingId>({[&] {
                auto const new_settings{settings_schemas.append()};
                auto const new_settings_row{settings_schemas.index(new_settings)};
                settings_schemas.name[new_settings_row] = strings.intern("vsync");
                settings_schemas.label[new_settings_row] = strings.intern("VSync");
                settings_schemas.category[new_settings_row] = strings.intern("missing");
                settings_schemas.value_type[new_settings_row] = make_test_type_ref(context, "bool");
                settings_schemas.backend[new_settings_row] = strings.intern("engine");
                settings_schemas.apply_mode[new_settings_row] = SettingApplyMode::deferred;
                settings_schemas.control[new_settings_row] = [&] {
                    auto const new_setting_controls{setting_controls.append()};
                    setting_controls.kind[setting_controls.index(new_setting_controls)] =
                        SettingControlKind::toggle;
                    return new_setting_controls;
                }();
                return new_settings;
            }()});
        return new_settings_modules;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    settings_modules.settings_list[settings_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(settings_modules.settings_list[settings_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{settings_schemas.clone(rows_1.front())};
            settings_schemas.category[settings_schemas.index(row_0)] = strings.intern("video");
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    settings_modules.settings_list[settings_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(settings_modules.settings_list[settings_modules.index(module)])};
        auto rows_2{std::vector(original_items.begin(), original_items.end())};
        rows_2.front() = [&] {
            auto row_1{settings_schemas.clone(rows_2.front())};
            auto const row_1_row{settings_schemas.index(row_1)};
            settings_schemas.control[row_1_row] = [&] {
                auto row_0{setting_controls.clone(settings_schemas.control[row_1_row])};
                setting_controls.kind[setting_controls.index(row_0)] = SettingControlKind::choice;
                return row_0;
            }();
            return row_1;
        }();
        return declarations.range(rows_2);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    settings_modules.settings_list[settings_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(settings_modules.settings_list[settings_modules.index(module)])};
        auto rows_2{std::vector(original_items.begin(), original_items.end())};
        rows_2.front() = [&] {
            auto row_1{settings_schemas.clone(rows_2.front())};
            auto const row_1_row{settings_schemas.index(row_1)};
            settings_schemas.control[row_1_row] = [&] {
                auto row_0{setting_controls.clone(settings_schemas.control[row_1_row])};
                setting_controls.kind[setting_controls.index(row_0)] = SettingControlKind::custom;
                return row_0;
            }();
            return row_1;
        }();
        return declarations.range(rows_2);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, NormalModuleReservesGeneratedNamesAcrossDeclarationKinds) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& enums{declarations.enums};

    auto& enumerators{declarations.enumerators};
    auto& records{declarations.records};
    auto& record_members{declarations.record_members};
    auto& soas{declarations.soas};
    auto& soa_members{declarations.soa_members};
    auto& strings{context.storage.strings};
    auto const vector{plain_soa(context, "Foo")};
    auto module = normal_module(
        context,
        {vector,
         [&] {
             auto const new_enums{enums.append()};
             auto const new_enums_row{enums.index(new_enums)};
             enums.name[new_enums_row] = strings.intern("foo");
             enums.underlying_type[new_enums_row] = make_test_type_ref(context, "uint8");
             enums.values[new_enums_row] = declarations.range<lispb::codegen::EnumeratorId>({[&] {
                 auto const new_enumerators{enumerators.append()};
                 enumerators.name[enumerators.index(new_enumerators)] = strings.intern("One");
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
             auto const new_records{records.append()};
             auto const new_records_row{records.index(new_records)};
             records.name[new_records_row] = strings.intern("foo");
             records.members[new_records_row] =
                 declarations.range<lispb::codegen::RecordMemberId>({[&] {
                     auto const new_record_members{record_members.append()};
                     auto const new_record_members_row{record_members.index(new_record_members)};
                     record_members.name[new_record_members_row] = strings.intern("value");
                     record_members.type[new_record_members_row] =
                         make_test_type_ref(context, "int32");
                     return new_record_members;
                 }()});
             return new_records;
         }()},
        "Mixed.cpp");
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    auto soa = [&] {
        auto const new_soas{soas.append()};
        auto const new_soas_row{soas.index(new_soas)};
        soas.name[new_soas_row] = strings.intern("Data");
        soas.members[new_soas_row] = declarations.range<lispb::codegen::SoaMemberId>({[&] {
            auto const new_soa_members{soa_members.append()};
            auto const new_soa_members_row{soa_members.index(new_soa_members)};
            soa_members.name[new_soa_members_row] = strings.intern("values");
            soa_members.kind[new_soa_members_row] = SoaMemberKind::array;
            soa_members.type[new_soa_members_row] = make_test_type_ref(context, "int32");
            return new_soa_members;
        }()});
        return new_soas;
    }();
    module = normal_module(
        context,
        {soa,
         [&] {
             auto const new_enums{enums.append()};
             auto const new_enums_row{enums.index(new_enums)};
             enums.name[new_enums_row] = strings.intern("data");
             enums.underlying_type[new_enums_row] = make_test_type_ref(context, "uint8");
             enums.values[new_enums_row] = declarations.range<lispb::codegen::EnumeratorId>({[&] {
                 auto const new_enumerators{enumerators.append()};
                 enumerators.name[enumerators.index(new_enumerators)] = strings.intern("One");
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
             auto const new_enums{enums.append()};
             auto const new_enums_row{enums.index(new_enums)};
             enums.name[new_enums_row] = strings.intern("State");
             enums.underlying_type[new_enums_row] = make_test_type_ref(context, "uint8");
             enums.values[new_enums_row] = declarations.range<lispb::codegen::EnumeratorId>({[&] {
                 auto const new_enumerators{enumerators.append()};
                 enumerators.name[enumerators.index(new_enumerators)] = strings.intern("One");
                 return new_enumerators;
             }()});
             return new_enums;
         }(),
         [&] {
             auto const new_records{records.append()};
             auto const new_records_row{records.index(new_records)};
             records.name[new_records_row] = strings.intern("Snapshot");
             records.members[new_records_row] =
                 declarations.range<lispb::codegen::RecordMemberId>({[&] {
                     auto const new_record_members{record_members.append()};
                     auto const new_record_members_row{record_members.index(new_record_members)};
                     record_members.name[new_record_members_row] = strings.intern("value");
                     record_members.type[new_record_members_row] =
                         make_test_type_ref(context, "int32");
                     return new_record_members;
                 }()});
             return new_records;
         }()});
    EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));
}

TEST(Validation, NormalModuleAppliesSourceRequirementsPerDeclaration) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& facades{declarations.facades};

    auto& facade_methods{declarations.facade_methods};
    auto& strings{context.storage.strings};
    auto facade = [&] {
        auto const new_facades{facades.append()};
        auto const new_facades_row{facades.index(new_facades)};
        facades.name[new_facades_row] = strings.intern("InlineFacade");
        facades.target_type[new_facades_row] = make_test_type_ref(context, "int32");
        facades.target_member_name[new_facades_row] = strings.intern("target");
        facades.methods[new_facades_row] = declarations.range<lispb::codegen::FacadeMethodId>({[&] {
            auto const new_facade_methods{facade_methods.append()};
            auto const new_facade_methods_row{facade_methods.index(new_facade_methods)};
            facade_methods.name[new_facade_methods_row] = strings.intern("reset");
            facade_methods.return_type[new_facade_methods_row] =
                make_test_type_ref(context, "void");
            return new_facade_methods;
        }()});
        return new_facades;
    }();
    auto module = normal_module(context, {facade}, "Mixed.cpp");
    EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));

    auto const source_facade{facades.clone(facade)};
    facades.definitions_in_source[facades.index(source_facade)] = true;
    module = normal_module(context, {source_facade});
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, NormalModuleCanMixIndependentEnumApis) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& enums{declarations.enums};

    auto& enumerators{declarations.enumerators};
    auto& strings{context.storage.strings};
    auto module = normal_module(
        context,
        {[&] {
             auto const new_enums{enums.append()};
             auto const new_enums_row{enums.index(new_enums)};
             enums.name[new_enums_row] = strings.intern("NativeMode");
             enums.underlying_type[new_enums_row] = make_test_type_ref(context, "uint8");
             enums.values[new_enums_row] = declarations.range<lispb::codegen::EnumeratorId>({[&] {
                 auto const new_enumerators{enumerators.append()};
                 enumerators.name[enumerators.index(new_enumerators)] = strings.intern("One");
                 return new_enumerators;
             }()});
             enums.native_api[new_enums_row] = true;
             return new_enums;
         }(),
         [&] {
             auto const new_enums{enums.append()};
             auto const new_enums_row{enums.index(new_enums)};
             enums.name[new_enums_row] = strings.intern("UnrealMode");
             enums.underlying_type[new_enums_row] = make_test_type_ref(context, "uint8");
             enums.values[new_enums_row] = declarations.range<lispb::codegen::EnumeratorId>({[&] {
                 auto const new_enumerators{enumerators.append()};
                 enumerators.name[enumerators.index(new_enumerators)] = strings.intern("One");
                 return new_enumerators;
             }()});
             return new_enums;
         }()});
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

TEST(Validation, EquivalentMemberMappingsRequireACompleteEquivalentType) {
    for (auto const declaration :
         {"(struct Rows (member xs array float :equivalent-member x))",
          "(struct Rows :equivalent-type Pair (member xs array float :equivalent-accessor true))",
          "(struct Rows :equivalent-type Pair (member xs array float :equivalent-member x) (member "
          "ys array float))",
          "(struct Rows :equivalent-constructor make_pair (member xs array float))"}) {
        CompilerTestContext context;
        auto manifest{parse_test_manifest(
            context, std::string{"(module test :header \"Test.h\" "} + declaration + ")")};
        EXPECT_THROW(lower_test_manifest(context, std::move(manifest)), std::invalid_argument);
    }
}

} // namespace
} // namespace lispb::codegen
