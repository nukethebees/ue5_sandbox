#include "test_utilities.h"

#include <codegen/ast/cpp_files.h>
#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/modules.h>
#include <codegen/ast/type_operation.h>
#include <codegen/generator.h>
#include <codegen/schema/child_occurrences.h>
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

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <format>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lispb::codegen {
namespace {

template <typename Id, typename Store, typename Edit>
auto edit_first_test_item(Declarations& declarations,
                          SchemaRange<Id> const range,
                          Store& store,
                          Edit edit) -> SchemaRange<Id> {
    auto const items{declarations.items(range)};
    auto rows{std::vector(items.begin(), items.end())};
    assert(!rows.empty());
    auto const replacement{store.clone(rows.front())};
    edit(replacement);
    rows.front() = replacement;
    return declarations.range(rows);
}

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

TEST(Validation, EnumStringTablesRequireKnownDenseZeroBasedCodes) {
    for (auto const values : {"(value First :value -1) (value Second :value 0)",
                              "(value First :value 0) (value Second :value 2)",
                              "(value First :value 1) (value Second :value 2)",
                              "(value First :value 0) (value Second :value 0)",
                              "(value First :value \"external_code\") (value Second :value 1)"}) {
        for (auto const options :
             {":native-api true :helpers (to-string)", ":conversions (lex-to-string)"}) {
            SCOPED_TRACE(values);
            SCOPED_TRACE(options);
            CompilerTestContext context;
            auto const source{std::format("(module test :header \"Test.h\" :source \"Test.cpp\" "
                                          "(enum State int {} {} (projections (cpp))))",
                                          options,
                                          values)};
            try {
                render_test_manifest(context, parse_test_manifest(context, source));
                FAIL() << "Expected dense enum diagnostic";
            } catch (std::invalid_argument const& error) {
                EXPECT_NE(std::string_view{error.what()}.find("zero-based contiguous codes"),
                          std::string_view::npos)
                    << error.what();
            }
        }
    }
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
                normal_modules.declarations[module] =
                    declarations.range<lispb::codegen::DeclarationHandle>(
                        {append_test_row(records, [&](auto const new_records) {
                            records.cpp_projection[new_records] = CppProjection{};
                            records.name[new_records] = strings.intern("Record");
                            records.functions[new_records] =
                                declarations.range<lispb::codegen::FunctionSchemaId>({function});
                        })});
            } else {

                normal_modules.declarations[module] = edit_first_test_item(
                    declarations, normal_modules.declarations[module], soas, [&](auto const soa) {
                        auto const soa_row{soas.index(soa)};
                        auto& functions{receiver == 1   ? soas.functions[soa_row]
                                        : receiver == 2 ? soas.const_view_functions[soa_row]
                                                        : soas.mutable_view_functions[soa_row]};
                        functions = declarations.range<FunctionSchemaId>({function});
                    });
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
    functions_schemas.return_type[functions_schemas.index(function)] =
        append_test_row(type_refs, [&](auto const new_type_refs) {
            type_refs.name[new_type_refs] = strings.intern("void");
            type_refs.suffix[new_type_refs] = strings.intern("*");
        });
    functions_schemas.body_lines[functions_schemas.index(function)] =
        declarations.range<lispb::StringId>({strings.intern("return nullptr;")});
    check(function, "");
    functions_schemas.return_type[functions_schemas.index(function)] =
        make_test_type_ref(context, "auto");
    functions_schemas.trailing_return_type[functions_schemas.index(function)] =
        append_test_row(type_refs, [&](auto const new_type_refs) {
            type_refs.name[new_type_refs] = strings.intern("void");
            type_refs.suffix[new_type_refs] = strings.intern("*");
        });
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
    return append_test_row(soas, [&](auto const new_soas) {
        soas.name[new_soas] = strings.intern(std::move(name));
        soas.members[new_soas] = declarations.range<lispb::codegen::SoaMemberId>(
            {append_test_row(soa_members, [&](auto const new_soa_members) {
                soa_members.name[new_soa_members] = strings.intern("values");
                soa_members.kind[new_soa_members] = SoaMemberKind::array;
                soa_members.type[new_soa_members] = make_test_type_ref(context, "int32");
            })});
    });
}

auto valid_enum_module(CompilerTestContext& context) -> lispb::codegen::ModuleHandle {
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& module_settings_store{declarations.module_settings_store};
    auto& enums{declarations.enums};
    auto& strings{context.storage.strings};
    return append_test_row(normal_modules, [&](auto const new_normal_modules) {
        normal_modules.settings[new_normal_modules] =
            append_test_row(module_settings_store, [&](auto const new_module_settings_store) {
                module_settings_store.name[new_module_settings_store] = strings.intern("enums");
                module_settings_store.header[new_module_settings_store] = strings.intern("Enums.h");
                module_settings_store.source[new_module_settings_store] =
                    strings.intern("Enums.cpp");
            });
        normal_modules.declarations[new_normal_modules] =
            declarations.range<lispb::codegen::DeclarationHandle>(
                {append_test_row(enums, [&](auto const new_enums) {
                    enums.cpp_projection[new_enums] = CppProjection{};
                    enums.name[new_enums] = strings.intern("EMode");
                    enums.underlying_type[new_enums] = make_test_type_ref(context, "uint8");
                    enums.values[new_enums] = declarations.range<lispb::codegen::EnumeratorId>(
                        {append_test_row(declarations.enumerators, [&](auto const new_enumerators) {
                            declarations.enumerators.name[new_enumerators] =
                                strings.intern("Value");
                        })});
                    enums.conversions[new_enums] =
                        declarations.range<lispb::codegen::EnumConversion>(
                            {EnumConversion::string_view});
                    enums.reflection[new_enums] = EnumReflection::uenum;
                })});
        normal_modules.enum_helper_namespace[new_normal_modules] = strings.intern("project");
    });
}

auto valid_native_enum_module(CompilerTestContext& context) -> lispb::codegen::ModuleHandle {
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& module_settings_store{declarations.module_settings_store};
    auto& enums{declarations.enums};
    auto& enumerators{declarations.enumerators};
    auto& strings{context.storage.strings};
    return append_test_row(normal_modules, [&](auto const new_normal_modules) {
        normal_modules.settings[new_normal_modules] =
            append_test_row(module_settings_store, [&](auto const new_module_settings_store) {
                module_settings_store.name[new_module_settings_store] =
                    strings.intern("native_enums");
                module_settings_store.header[new_module_settings_store] =
                    strings.intern("NativeEnums.h");
                module_settings_store.namespace_name[new_module_settings_store] =
                    strings.intern("project");
            });
        normal_modules.declarations[new_normal_modules] =
            declarations.range<lispb::codegen::DeclarationHandle>(
                {append_test_row(enums, [&](auto const new_enums) {
                    enums.cpp_projection[new_enums] = CppProjection{};
                    enums.name[new_enums] = strings.intern("NativeMode");
                    enums.underlying_type[new_enums] = make_test_type_ref(context, "std::uint8_t");
                    enums.values[new_enums] = declarations.range<lispb::codegen::EnumeratorId>(
                        {append_test_row(enumerators, [&](auto const new_enumerators) {
                            enumerators.name[new_enumerators] = strings.intern("Idle");
                            enumerators.initializer[new_enumerators] = strings.intern("0");
                            enumerators.display_name[new_enumerators] = std::nullopt;
                            enumerators.hidden[new_enumerators] = false;
                            enumerators.serialized_name[new_enumerators] = strings.intern("idle");
                        })});
                    enums.native_api[new_enums] = true;
                })});
    });
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
    normal_modules.settings[representations_row] =
        append_test_row(module_settings_store, [&](auto const new_module_settings_store) {
            module_settings_store.name[new_module_settings_store] =
                strings.intern("representations");
            module_settings_store.header[new_module_settings_store] =
                strings.intern("Representations.h");
        });
    normal_modules.declarations[representations_row] =
        declarations.range<lispb::codegen::DeclarationHandle>(
            {append_test_row(linear_quantizeds, [&](auto const new_linear_quantizeds) {
                linear_quantizeds.name[new_linear_quantizeds] = strings.intern("DamageReasonQ4");
                linear_quantizeds.source[new_linear_quantizeds] =
                    make_test_type_ref(context, "@scalars/DamageReason");
                linear_quantizeds.bit_width[new_linear_quantizeds] = 4;
                linear_quantizeds.reserved_codes[new_linear_quantizeds] = 1;
                linear_quantizeds.clipping[new_linear_quantizeds] = QuantizationClipping::reject;
            })});
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
        normal_modules.declarations[scalar] = edit_first_test_item(
            declarations,
            normal_modules.declarations[scalar],
            integer_scalars,
            [&](auto const row_0) { integer_scalars.signedness[row_0] = true; });
        normal_modules.declarations[scalar] = edit_first_test_item(
            declarations,
            normal_modules.declarations[scalar],
            integer_scalars,
            [&](auto const row_0) { integer_scalars.minimum_value[row_0] = -100; });
        normal_modules.declarations[scalar] = edit_first_test_item(
            declarations,
            normal_modules.declarations[scalar],
            integer_scalars,
            [&](auto const row_0) { integer_scalars.maximum_value[row_0] = 100; });
        normal_modules.declarations[scalar] = edit_first_test_item(
            declarations,
            normal_modules.declarations[scalar],
            integer_scalars,
            [&](auto const row_0) { integer_scalars.named_codes[row_0] = {}; });
    }
    auto const representations{normal_modules.append()};
    auto const representations_row{normal_modules.index(representations)};
    normal_modules.settings[representations_row] =
        append_test_row(module_settings_store, [&](auto const new_module_settings_store) {
            module_settings_store.name[new_module_settings_store] =
                strings.intern("representations");
            module_settings_store.header[new_module_settings_store] =
                strings.intern("Representations.h");
        });
    normal_modules.declarations[representations_row] =
        declarations.range<lispb::codegen::DeclarationHandle>(
            {append_test_row(integer_varints, [&](auto const new_integer_varints) {
                integer_varints.name[new_integer_varints] = strings.intern("DamageReasonVarint");
                integer_varints.source[new_integer_varints] =
                    make_test_type_ref(context, "@scalars/DamageReason");
                integer_varints.encoding[new_integer_varints] = encoding;
            })});
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
    normal_modules.settings[representations_row] =
        append_test_row(module_settings_store, [&](auto const new_module_settings_store) {
            module_settings_store.name[new_module_settings_store] =
                strings.intern("representations");
            module_settings_store.header[new_module_settings_store] =
                strings.intern("Representations.h");
        });
    normal_modules.declarations[representations_row] =
        declarations.range<lispb::codegen::DeclarationHandle>(
            {append_test_row(fixed_points, [&](auto const new_fixed_points) {
                fixed_points.name[new_fixed_points] = strings.intern("VelocityQ12_4");
                fixed_points.signedness[new_fixed_points] = signedness;
                fixed_points.total_bits[new_fixed_points] = 16;
                fixed_points.fractional_bits[new_fixed_points] = 4;
                fixed_points.rounding[new_fixed_points] = FixedPointRounding::nearest_even;
            })});
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
    normal_modules.settings[representations_row] =
        append_test_row(module_settings_store, [&](auto const new_module_settings_store) {
            module_settings_store.name[new_module_settings_store] =
                strings.intern("representations");
            module_settings_store.header[new_module_settings_store] =
                strings.intern("Representations.h");
        });
    normal_modules.declarations[representations_row] =
        declarations.range<lispb::codegen::DeclarationHandle>(
            {append_test_row(optional_sentinels, [&](auto const new_optional_sentinels) {
                optional_sentinels.name[new_optional_sentinels] =
                    strings.intern("OptionalDamageReason");
                optional_sentinels.source[new_optional_sentinels] =
                    make_test_type_ref(context, "@scalars/DamageReason");
                optional_sentinels.sentinel[new_optional_sentinels] = strings.intern("Invalid");
            })});
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
    normal_modules.settings[representations_row] =
        append_test_row(module_settings_store, [&](auto const new_module_settings_store) {
            module_settings_store.name[new_module_settings_store] =
                strings.intern("representations");
            module_settings_store.header[new_module_settings_store] =
                strings.intern("Representations.h");
        });
    normal_modules.declarations[representations_row] =
        declarations.range<lispb::codegen::DeclarationHandle>(
            {append_test_row(optional_presence_bits, [&](auto const new_optional_presence_bits) {
                optional_presence_bits.name[new_optional_presence_bits] =
                    strings.intern("PresentDamageReason");
                optional_presence_bits.source[new_optional_presence_bits] =
                    make_test_type_ref(context, "@scalars/DamageReason");
            })});
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
    return append_test_row(normal_modules, [&](auto const new_normal_modules) {
        normal_modules.settings[new_normal_modules] =
            append_test_row(module_settings_store, [&](auto const new_module_settings_store) {
                module_settings_store.name[new_module_settings_store] = strings.intern("tables");
                module_settings_store.header[new_module_settings_store] =
                    strings.intern("Tables.h");
            });
        normal_modules.declarations[new_normal_modules] =
            declarations.range<lispb::codegen::DeclarationHandle>(
                {append_test_row(static_tables, [&](auto const new_static_tables) {
                    static_tables.name[new_static_tables] = strings.intern("FValues");
                    static_tables.rows[new_static_tables] =
                        declarations.range<lispb::codegen::StaticTableRowId>(
                            {append_test_row(static_table_rows,
                                             [&](auto const new_static_table_rows) {
                                                 static_table_rows.name[new_static_table_rows] =
                                                     strings.intern("first");
                                             }),
                             append_test_row(static_table_rows,
                                             [&](auto const new_static_table_rows) {
                                                 static_table_rows.name[new_static_table_rows] =
                                                     strings.intern("second");
                                             })});
                    static_tables.columns[new_static_tables] =
                        declarations.range<lispb::codegen::StaticTableColumnId>({append_test_row(
                            static_table_columns, [&](auto const new_static_table_columns) {
                                static_table_columns.name[new_static_table_columns] =
                                    strings.intern("ids");
                                static_table_columns.type[new_static_table_columns] =
                                    make_test_type_ref(context, "int32");
                            })});
                })});
    });
}

TEST(Validation, IntegerProjectionRequiresUnsignedSufficientStorage) {
    for (auto const storage : {"std::uint8_t", "std::int32_t", "float"}) {
        CompilerTestContext context;
        auto const source{
            std::format("(module values :header \"Values.h\" (integer-scalar Index :bit-width 24 "
                        "(projections (cpp :storage {}))))",
                        storage)};
        EXPECT_THROW(render_test_manifest(context, parse_test_manifest(context, source)),
                     std::invalid_argument);
    }
    CompilerTestContext context;
    EXPECT_NO_THROW(render_test_manifest(context, parse_test_manifest(context, R"(
      (module values :header "Values.h"
        (integer-scalar Index :bit-width 24 (projections (cpp :storage std::uint64_t))))
    )")));
}

TEST(Validation, RejectsEmptyModuleNames) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto module{valid_soa_module(context)};
    normal_modules.settings[module] = [&] {
        auto row_0{declarations.module_settings_store.clone(normal_modules.settings[module])};
        declarations.module_settings_store.name[row_0] = {};
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
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], enums, [&](auto const row_0) {
            enums.values[row_0] = {};
        });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_enum_module(context);
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], enums, [&](auto const row_0) {
            auto const row_0_row{enums.index(row_0)};
            enums.values[row_0_row] = [&] {
                auto const original_items{declarations.items(enums.values[row_0_row])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.push_back(append_test_row(enumerators, [&](auto const new_enumerators) {
                    enumerators.name[new_enumerators] = strings.intern("Value");
                }));
                return declarations.range(rows);
            }();
        });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_enum_module(context);
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], enums, [&](auto const row_2) {
            auto const row_2_row{enums.index(row_2)};
            enums.values[row_2_row] = edit_first_test_item(
                declarations, enums.values[row_2_row], enumerators, [&](auto const row_0) {
                    enumerators.name[row_0] = strings.intern("bad-name");
                });
        });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_enum_module(context);
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], enums, [&](auto const row_0) {
            enums.underlying_type[row_0] = make_test_type_ref(context, "@missing");
        });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

struct EnumDomainCase {
    char const* name;
    std::optional<std::uint8_t> bit_width;
    schema::EnumSignedness signedness;
    std::vector<std::pair<std::string_view, std::string_view>> values;
    bool valid;
};

using EnumDomainValidation = testing::TestWithParam<EnumDomainCase>;

TEST_P(EnumDomainValidation, AppliesWidthAndSignednessToKnownValues) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& modules{declarations.normal_modules};
    auto& enums{declarations.enums};
    auto& enumerators{declarations.enumerators};
    auto& strings{context.storage.strings};
    auto const& test_case{GetParam()};
    auto const module{valid_enum_module(context)};
    modules.declarations[module] = edit_first_test_item(
        declarations, modules.declarations[module], enums, [&](auto const enumeration) {
            enums.reflection[enumeration] = EnumReflection::none;
            enums.conversions[enumeration] = {};
            enums.bit_width[enumeration] = test_case.bit_width;
            enums.signedness[enumeration] = test_case.signedness;
            if (!test_case.values.empty()) {
                std::vector<EnumeratorId> values;
                for (auto const& [name, initializer] : test_case.values) {
                    auto const value{enumerators.append()};
                    enumerators.name[value] = strings.intern(name);
                    enumerators.initializer[value] = strings.intern(initializer);
                    values.push_back(value);
                }
                enums.values[enumeration] = declarations.range(values);
            }
        });

    if (test_case.valid) {
        EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));
    } else {
        EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                     std::invalid_argument);
    }
}

INSTANTIATE_TEST_SUITE_P(
    Validation,
    EnumDomainValidation,
    testing::Values(EnumDomainCase{"ZeroWidth", 0, schema::EnumSignedness::unspecified, {}, false},
                    EnumDomainCase{
                        "WidthAbove64", 65, schema::EnumSignedness::unspecified, {}, false},
                    EnumDomainCase{"InsufficientUnsignedWidth",
                                   2,
                                   schema::EnumSignedness::unspecified,
                                   {{"Zero", "0"}, {"Seven", "7"}},
                                   false},
                    EnumDomainCase{"SufficientUnsignedWidth",
                                   3,
                                   schema::EnumSignedness::unspecified,
                                   {{"Zero", "0"}, {"Seven", "7"}},
                                   true},
                    EnumDomainCase{"NegativeUnsignedValue",
                                   std::nullopt,
                                   schema::EnumSignedness::unsigned_,
                                   {{"Negative", "-1"}, {"Positive", "1"}},
                                   false},
                    EnumDomainCase{"InsufficientSignedWidth",
                                   3,
                                   schema::EnumSignedness::signed_,
                                   {{"Zero", "0"}, {"Seven", "7"}},
                                   false},
                    EnumDomainCase{"SufficientSignedWidth",
                                   4,
                                   schema::EnumSignedness::signed_,
                                   {{"Zero", "0"}, {"Seven", "7"}},
                                   true}),
    [](testing::TestParamInfo<EnumDomainCase> const& info) { return info.param.name; });

TEST(Validation, RejectsInvalidIntegerScalarDomainsAndNamedCodes) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& integer_scalars{declarations.integer_scalars};
    auto& packed_named_codes{declarations.packed_named_codes};
    auto& strings{context.storage.strings};
    auto module{valid_integer_scalar_module(context)};
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], integer_scalars, [&](auto const row_0) {
            integer_scalars.bit_width[row_0] = 3;
        });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_integer_scalar_module(context);
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], integer_scalars, [&](auto const row_0) {
            integer_scalars.minimum_value[row_0] = -1;
        });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_integer_scalar_module(context);
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], integer_scalars, [&](auto const row_2) {
            auto const row_2_row{integer_scalars.index(row_2)};
            integer_scalars.named_codes[row_2_row] = [&] {
                auto const original_items{
                    declarations.items(integer_scalars.named_codes[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{packed_named_codes.clone(rows_1[1])};
                    packed_named_codes.value[row_0] = 10;
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
        });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_integer_scalar_module(context);
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], integer_scalars, [&](auto const row_2) {
            auto const row_2_row{integer_scalars.index(row_2)};
            integer_scalars.named_codes[row_2_row] = [&] {
                auto const original_items{
                    declarations.items(integer_scalars.named_codes[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_named_codes.clone(rows_1[0])};
                    packed_named_codes.value[row_0] = 11;
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
        });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_integer_scalar_module(context);
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], integer_scalars, [&](auto const row_2) {
            auto const row_2_row{integer_scalars.index(row_2)};
            integer_scalars.named_codes[row_2_row] = [&] {
                auto const original_items{
                    declarations.items(integer_scalars.named_codes[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{packed_named_codes.clone(rows_1[1])};
                    packed_named_codes.name[row_0] = strings.intern("Unknown");
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
        });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_integer_scalar_module(context);
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], integer_scalars, [&](auto const row_2) {
            auto const row_2_row{integer_scalars.index(row_2)};
            integer_scalars.named_codes[row_2_row] = [&] {
                auto const original_items{
                    declarations.items(integer_scalars.named_codes[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{packed_named_codes.clone(rows_1[1])};
                    packed_named_codes.value[row_0] = 0;
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
        });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_integer_scalar_module(context);
    normal_modules.settings[module] = [&] {
        auto row_0{declarations.module_settings_store.clone(normal_modules.settings[module])};
        declarations.module_settings_store.source[row_0] = strings.intern("Scalars.cpp");
        return row_0;
    }();
    EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));

    module = valid_integer_scalar_module(context);
    auto const lowered{lower_test_manifest(context, manifest_with(context, module))};
    EXPECT_TRUE(lowered.empty());
    EXPECT_TRUE(render_modules(context.ir, lowered).empty());
}

TEST(Validation, LogicalScalarNeedsNoCppProjectionOrNamedCodes) {
    CompilerTestContext context;
    EXPECT_NO_THROW(schema::resolve_type_graph(parse_test_manifest(context, R"(
      (module values
        (integer-scalar Index :bit-width 24)
        (integer-scalar Delta :bit-width 16 :signed true)
        (integer-scalar Health :bit-width 8 :minimum 0 :maximum 100))
    )")));
}

TEST(Validation, EmptyEditableModulesDoNotEmitCppWithoutDeclarations) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& module_settings_store{declarations.module_settings_store};
    auto& normal_modules{declarations.normal_modules};
    auto& strings{context.storage.strings};
    auto settings = [&](std::string const& name) {
        return append_test_row(module_settings_store, [&](auto const new_module_settings_store) {
            module_settings_store.name[new_module_settings_store] = strings.intern(name);
            module_settings_store.header[new_module_settings_store] = strings.intern(name + ".h");
        });
    };
    auto module{append_test_row(normal_modules, [&](auto const new_normal_modules) {
        normal_modules.settings[new_normal_modules] = settings("empty");
    })};
    EXPECT_NO_THROW(
        render_modules(context.ir, lower_test_manifest(context, manifest_with(context, module))));
    normal_modules.settings[module] = [&] {
        auto row_0{module_settings_store.clone(normal_modules.settings[module])};
        module_settings_store.source[row_0] = strings.intern("empty.cpp");
        return row_0;
    }();
    EXPECT_EQ(
        render_modules(context.ir, lower_test_manifest(context, manifest_with(context, module)))
            .size(),
        0U);
}

TEST(Validation, ValidatesRecordMemberRelationshipUnits) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& module_settings_store{declarations.module_settings_store};
    auto& records{declarations.records};
    auto& record_members{declarations.child_occurrences};

    auto& semantic_relations{declarations.semantic_relations};
    auto& strings{context.storage.strings};
    auto module{append_test_row(normal_modules, [&](auto const new_normal_modules) {
        normal_modules.settings[new_normal_modules] =
            append_test_row(module_settings_store, [&](auto const new_module_settings_store) {
                module_settings_store.name[new_module_settings_store] = strings.intern("records");
                module_settings_store.header[new_module_settings_store] =
                    strings.intern("Records.h");
            });
        normal_modules.declarations[new_normal_modules] =
            declarations.range<lispb::codegen::DeclarationHandle>(
                {append_test_row(
                     records,
                     [&](auto const new_records) {
                         records.cpp_projection[new_records] = CppProjection{};
                         records.name[new_records] = strings.intern("Target");
                         records.members[new_records] =
                             declarations.range<lispb::codegen::ChildOccurrenceId>({append_test_row(
                                 record_members, [&](auto const new_record_members) {
                                     record_members.name[new_record_members] =
                                         strings.intern("value");
                                     record_members.type[new_record_members] =
                                         make_test_type_ref(context, "std::uint32_t");
                                 })});
                     }),
                 append_test_row(records, [&](auto const new_records) {
                     records.cpp_projection[new_records] = CppProjection{};
                     records.name[new_records] = strings.intern("User");
                     records.members[new_records] =
                         declarations.range<lispb::codegen::ChildOccurrenceId>(
                             {append_test_row(record_members, [&](auto const new_record_members) {
                                 record_members.name[new_record_members] = strings.intern("value");
                                 record_members.type[new_record_members] =
                                     make_test_type_ref(context, "std::uint32_t");
                                 record_members.relationship[new_record_members] = append_test_row(
                                     semantic_relations, [&](auto const new_semantic_relations) {
                                         semantic_relations.kind[new_semantic_relations] =
                                             SemanticRelationKind::references;
                                         semantic_relations.target[new_semantic_relations] =
                                             make_test_type_ref(context, "Target");
                                         semantic_relations.unit[new_semantic_relations] =
                                             std::nullopt;
                                     });
                             })});
                 })});
    })};
    EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));

    normal_modules.declarations[module] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3[1] = [&] {
            auto row_2{records.clone(rows_3[1])};
            auto const row_2_row{records.index(row_2)};
            records.members[row_2_row] = [&] {
                auto const original_items{declarations.items(records.members[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{record_members.clone(rows_1[0])};
                    record_members.relationship[row_0] =
                        append_test_row(semantic_relations, [&](auto const new_semantic_relations) {
                            semantic_relations.kind[new_semantic_relations] =
                                SemanticRelationKind::offset_into;
                            semantic_relations.target[new_semantic_relations] =
                                make_test_type_ref(context, "Target");
                            semantic_relations.unit[new_semantic_relations] = std::nullopt;
                        });
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

    normal_modules.declarations[module] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3[1] = [&] {
            auto row_2{records.clone(rows_3[1])};
            auto const row_2_row{records.index(row_2)};
            records.members[row_2_row] = [&] {
                auto const original_items{declarations.items(records.members[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{record_members.clone(rows_1[0])};
                    record_members.relationship[row_0] =
                        append_test_row(semantic_relations, [&](auto const new_semantic_relations) {
                            semantic_relations.kind[new_semantic_relations] =
                                SemanticRelationKind::references;
                            semantic_relations.target[new_semantic_relations] =
                                make_test_type_ref(context, "Target");
                            semantic_relations.unit[new_semantic_relations] =
                                SemanticRelationUnit::elements;
                        });
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

    normal_modules.declarations[module] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3[1] = [&] {
            auto row_2{records.clone(rows_3[1])};
            auto const row_2_row{records.index(row_2)};
            records.members[row_2_row] = [&] {
                auto const original_items{declarations.items(records.members[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{record_members.clone(rows_1[0])};
                    record_members.relationship[row_0] =
                        append_test_row(semantic_relations, [&](auto const new_semantic_relations) {
                            semantic_relations.kind[new_semantic_relations] =
                                SemanticRelationKind::offset_into;
                            semantic_relations.target[new_semantic_relations] =
                                make_test_type_ref(context, "Target");
                            semantic_relations.unit[new_semantic_relations] =
                                SemanticRelationUnit::bytes;
                        });
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
    normal_modules.declarations[module] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module])};
        auto rows{std::vector(original_items.begin(), original_items.end())};
        rows.push_back(append_test_row(soas, [&](auto const new_soas) {
            soas.name[new_soas] = strings.intern("Target");
            soas.members[new_soas] = declarations.range<lispb::codegen::SoaMemberId>(
                {append_test_row(soa_members, [&](auto const new_soa_members) {
                    soa_members.name[new_soa_members] = strings.intern("values");
                    soa_members.kind[new_soa_members] = SoaMemberKind::array;
                    soa_members.type[new_soa_members] =
                        make_test_type_ref(context, "std::uint32_t");
                    soa_members.relationship[new_soa_members] = std::nullopt;
                })});
        }));
        return declarations.range(rows);
    }();

    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], soas, [&](auto const row_2) {
            auto const row_2_row{soas.index(row_2)};
            soas.members[row_2_row] = edit_first_test_item(
                declarations, soas.members[row_2_row], soa_members, [&](auto const row_0) {
                    soa_members.relationship[row_0] =
                        append_test_row(semantic_relations, [&](auto const new_semantic_relations) {
                            semantic_relations.kind[new_semantic_relations] =
                                SemanticRelationKind::references;
                            semantic_relations.target[new_semantic_relations] =
                                make_test_type_ref(context, "Target");
                            semantic_relations.unit[new_semantic_relations] = std::nullopt;
                        });
                });
        });
    EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));

    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], soas, [&](auto const row_2) {
            auto const row_2_row{soas.index(row_2)};
            soas.members[row_2_row] = edit_first_test_item(
                declarations, soas.members[row_2_row], soa_members, [&](auto const row_0) {
                    soa_members.relationship[row_0] =
                        append_test_row(semantic_relations, [&](auto const new_semantic_relations) {
                            semantic_relations.kind[new_semantic_relations] =
                                SemanticRelationKind::offset_into;
                            semantic_relations.target[new_semantic_relations] =
                                make_test_type_ref(context, "Target");
                            semantic_relations.unit[new_semantic_relations] = std::nullopt;
                        });
                });
        });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], soas, [&](auto const row_2) {
            auto const row_2_row{soas.index(row_2)};
            soas.members[row_2_row] = edit_first_test_item(
                declarations, soas.members[row_2_row], soa_members, [&](auto const row_0) {
                    soa_members.relationship[row_0] =
                        append_test_row(semantic_relations, [&](auto const new_semantic_relations) {
                            semantic_relations.kind[new_semantic_relations] =
                                SemanticRelationKind::references;
                            semantic_relations.target[new_semantic_relations] =
                                make_test_type_ref(context, "Target");
                            semantic_relations.unit[new_semantic_relations] =
                                SemanticRelationUnit::elements;
                        });
                });
        });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], soas, [&](auto const row_2) {
            auto const row_2_row{soas.index(row_2)};
            soas.members[row_2_row] = edit_first_test_item(
                declarations, soas.members[row_2_row], soa_members, [&](auto const row_0) {
                    soa_members.relationship[row_0] =
                        append_test_row(semantic_relations, [&](auto const new_semantic_relations) {
                            semantic_relations.kind[new_semantic_relations] =
                                SemanticRelationKind::offset_into;
                            semantic_relations.target[new_semantic_relations] =
                                make_test_type_ref(context, "Target");
                            semantic_relations.unit[new_semantic_relations] =
                                SemanticRelationUnit::bytes;
                        });
                });
        });
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

        normal_modules.declarations[module] = edit_first_test_item(
            declarations,
            normal_modules.declarations[module],
            integer_scalars,
            [&](auto const row_0) { integer_scalars.signedness[row_0] = true; });
        normal_modules.declarations[module] = edit_first_test_item(
            declarations,
            normal_modules.declarations[module],
            integer_scalars,
            [&](auto const row_0) { integer_scalars.minimum_value[row_0] = -10; });
        normal_modules.declarations[module] = edit_first_test_item(
            declarations,
            normal_modules.declarations[module],
            integer_scalars,
            [&](auto const row_0) { integer_scalars.maximum_value[row_0] = 10; });
        normal_modules.declarations[module] = edit_first_test_item(
            declarations,
            normal_modules.declarations[module],
            integer_scalars,
            [&](auto const row_0) { integer_scalars.named_codes[row_0] = {}; });
        normal_modules.declarations[module] = edit_first_test_item(
            declarations,
            normal_modules.declarations[module],
            integer_scalars,
            [&](auto const row_0) {
                integer_scalars.relationship[row_0] =
                    append_test_row(semantic_relations, [&](auto const new_semantic_relations) {
                        semantic_relations.kind[new_semantic_relations] = kind;
                        semantic_relations.target[new_semantic_relations] =
                            make_test_type_ref(context, "@scalars/DamageReason");
                        semantic_relations.unit[new_semantic_relations] =
                            kind == SemanticRelationKind::offset_into
                                ? std::optional{SemanticRelationUnit::bytes}
                                : std::nullopt;
                    });
            });
        EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                     std::invalid_argument);
    }

    auto module{valid_integer_scalar_module(context)};
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], integer_scalars, [&](auto const row_0) {
            integer_scalars.relationship[row_0] =
                append_test_row(semantic_relations, [&](auto const new_semantic_relations) {
                    semantic_relations.kind[new_semantic_relations] =
                        SemanticRelationKind::references;
                    semantic_relations.target[new_semantic_relations] =
                        make_test_type_ref(context, "@scalars/DamageReason");
                    semantic_relations.unit[new_semantic_relations] = std::nullopt;
                });
        });
    EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));

    module = valid_integer_scalar_module(context);
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], integer_scalars, [&](auto const row_0) {
            integer_scalars.relationship[row_0] =
                append_test_row(semantic_relations, [&](auto const new_semantic_relations) {
                    semantic_relations.kind[new_semantic_relations] =
                        SemanticRelationKind::offset_into;
                    semantic_relations.target[new_semantic_relations] =
                        make_test_type_ref(context, "@scalars/DamageReason");
                    semantic_relations.unit[new_semantic_relations] = std::nullopt;
                });
        });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_integer_scalar_module(context);
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], integer_scalars, [&](auto const row_0) {
            integer_scalars.relationship[row_0] =
                append_test_row(semantic_relations, [&](auto const new_semantic_relations) {
                    semantic_relations.kind[new_semantic_relations] =
                        SemanticRelationKind::index_into;
                    semantic_relations.target[new_semantic_relations] =
                        make_test_type_ref(context, "@scalars/DamageReason");
                    semantic_relations.unit[new_semantic_relations] =
                        SemanticRelationUnit::elements;
                });
        });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_integer_scalar_module(context);
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], integer_scalars, [&](auto const row_0) {
            integer_scalars.relationship[row_0] =
                append_test_row(semantic_relations, [&](auto const new_semantic_relations) {
                    semantic_relations.kind[new_semantic_relations] =
                        SemanticRelationKind::offset_into;
                    semantic_relations.target[new_semantic_relations] =
                        make_test_type_ref(context, "@scalars/DamageReason");
                    semantic_relations.unit[new_semantic_relations] = SemanticRelationUnit::bytes;
                });
        });
    EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));
}

TEST(Validation, ValidatesLogicalLinearQuantizedRepresentationsWithoutCppOutput) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& integer_scalars{declarations.integer_scalars};
    auto& linear_quantizeds{declarations.linear_quantizeds};
    auto& strings{context.storage.strings};
    auto manifest{valid_linear_quantized_manifest(context)};
    auto lowered{lower_test_manifest(context, manifest)};
    EXPECT_TRUE(lowered.empty());

    manifest = valid_linear_quantized_manifest(context);

    manifest.modules[0] = [&] {
        auto row_2{normal_modules.clone(manifest.modules[0])};
        auto const row_2_row{normal_modules.index(row_2)};
        normal_modules.declarations[row_2_row] = edit_first_test_item(
            declarations,
            normal_modules.declarations[row_2_row],
            integer_scalars,
            [&](auto const row_0) { integer_scalars.signedness[row_0] = true; });
        return row_2;
    }();
    manifest.modules[0] = [&] {
        auto row_2{normal_modules.clone(manifest.modules[0])};
        auto const row_2_row{normal_modules.index(row_2)};
        normal_modules.declarations[row_2_row] = edit_first_test_item(
            declarations,
            normal_modules.declarations[row_2_row],
            integer_scalars,
            [&](auto const row_0) { integer_scalars.minimum_value[row_0] = -100; });
        return row_2;
    }();
    manifest.modules[0] = [&] {
        auto row_2{normal_modules.clone(manifest.modules[0])};
        auto const row_2_row{normal_modules.index(row_2)};
        normal_modules.declarations[row_2_row] = edit_first_test_item(
            declarations,
            normal_modules.declarations[row_2_row],
            integer_scalars,
            [&](auto const row_0) { integer_scalars.maximum_value[row_0] = 100; });
        return row_2;
    }();
    manifest.modules[0] = [&] {
        auto row_2{normal_modules.clone(manifest.modules[0])};
        auto const row_2_row{normal_modules.index(row_2)};
        normal_modules.declarations[row_2_row] = edit_first_test_item(
            declarations,
            normal_modules.declarations[row_2_row],
            integer_scalars,
            [&](auto const row_0) { integer_scalars.named_codes[row_0] = {}; });
        return row_2;
    }();
    EXPECT_NO_THROW(lower_test_manifest(context, manifest));

    manifest = valid_linear_quantized_manifest(context);
    manifest.modules[1] = [&] {
        auto row_2{normal_modules.clone(manifest.modules[1])};
        auto const row_2_row{normal_modules.index(row_2)};
        normal_modules.declarations[row_2_row] =
            edit_first_test_item(declarations,
                                 normal_modules.declarations[row_2_row],
                                 linear_quantizeds,
                                 [&](auto const row_0) { linear_quantizeds.bit_width[row_0] = 1; });
        return row_2;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);

    manifest = valid_linear_quantized_manifest(context);

    manifest.modules[1] = [&] {
        auto row_2{normal_modules.clone(manifest.modules[1])};
        auto const row_2_row{normal_modules.index(row_2)};
        normal_modules.declarations[row_2_row] =
            edit_first_test_item(declarations,
                                 normal_modules.declarations[row_2_row],
                                 linear_quantizeds,
                                 [&](auto const row_0) { linear_quantizeds.bit_width[row_0] = 2; });
        return row_2;
    }();
    manifest.modules[1] = [&] {
        auto row_2{normal_modules.clone(manifest.modules[1])};
        auto const row_2_row{normal_modules.index(row_2)};
        normal_modules.declarations[row_2_row] = edit_first_test_item(
            declarations,
            normal_modules.declarations[row_2_row],
            linear_quantizeds,
            [&](auto const row_0) { linear_quantizeds.reserved_codes[row_0] = 3; });
        return row_2;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);

    manifest = valid_linear_quantized_manifest(context);
    manifest.modules[0] = [&] {
        auto row_2{normal_modules.clone(manifest.modules[0])};
        auto const row_2_row{normal_modules.index(row_2)};
        normal_modules.declarations[row_2_row] = edit_first_test_item(
            declarations,
            normal_modules.declarations[row_2_row],
            integer_scalars,
            [&](auto const row_0) { integer_scalars.maximum_value[row_0] = 0; });
        return row_2;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);

    manifest = valid_linear_quantized_manifest(context);
    manifest.modules[1] = [&] {
        auto row_2{normal_modules.clone(manifest.modules[1])};
        auto const row_2_row{normal_modules.index(row_2)};
        normal_modules.declarations[row_2_row] =
            edit_first_test_item(declarations,
                                 normal_modules.declarations[row_2_row],
                                 linear_quantizeds,
                                 [&](auto const row_0) {
                                     linear_quantizeds.source[row_0] =
                                         make_test_type_ref(context, "std::uint32_t");
                                 });
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
            declarations.module_settings_store.source[row_0] =
                strings.intern("Representations.cpp");
            return row_0;
        }();
        return row_1;
    }();
    EXPECT_TRUE(render_modules(context.ir, lower_test_manifest(context, manifest)).empty());
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
        normal_modules.declarations[row_2_row] =
            edit_first_test_item(declarations,
                                 normal_modules.declarations[row_2_row],
                                 integer_varints,
                                 [&](auto const row_0) {
                                     integer_varints.source[row_0] =
                                         make_test_type_ref(context, "std::uint32_t");
                                 });
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
            rows.push_back(
                append_test_row(linear_quantizeds, [&](auto const new_linear_quantizeds) {
                    linear_quantizeds.name[new_linear_quantizeds] =
                        integer_varints
                            .name[declarations
                                      .items(normal_modules.declarations[manifest.modules[1]])
                                      .front()];
                    linear_quantizeds.source[new_linear_quantizeds] =
                        make_test_type_ref(context, "@scalars/DamageReason");
                    linear_quantizeds.bit_width[new_linear_quantizeds] = 4;
                    linear_quantizeds.reserved_codes[new_linear_quantizeds] = 0;
                    linear_quantizeds.clipping[new_linear_quantizeds] =
                        QuantizationClipping::reject;
                }));
            return declarations.range(rows);
        }();
        return row_0;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);
}

TEST(Validation, ValidatesLogicalFixedPointWidthsWithoutCppOutput) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& fixed_points{declarations.fixed_points};
    auto manifest{valid_fixed_point_manifest(context)};
    auto lowered{lower_test_manifest(context, manifest)};
    EXPECT_TRUE(lowered.empty());

    manifest = valid_fixed_point_manifest(context, false);

    manifest.modules.front() = [&] {
        auto row_2{normal_modules.clone(manifest.modules.front())};
        auto const row_2_row{normal_modules.index(row_2)};
        normal_modules.declarations[row_2_row] =
            edit_first_test_item(declarations,
                                 normal_modules.declarations[row_2_row],
                                 fixed_points,
                                 [&](auto const row_0) { fixed_points.total_bits[row_0] = 8; });
        return row_2;
    }();
    manifest.modules.front() = [&] {
        auto row_2{normal_modules.clone(manifest.modules.front())};
        auto const row_2_row{normal_modules.index(row_2)};
        normal_modules.declarations[row_2_row] = edit_first_test_item(
            declarations,
            normal_modules.declarations[row_2_row],
            fixed_points,
            [&](auto const row_0) { fixed_points.fractional_bits[row_0] = 8; });
        return row_2;
    }();
    EXPECT_NO_THROW(lower_test_manifest(context, manifest));

    manifest = valid_fixed_point_manifest(context);
    manifest.modules.front() = [&] {
        auto row_2{normal_modules.clone(manifest.modules.front())};
        auto const row_2_row{normal_modules.index(row_2)};
        normal_modules.declarations[row_2_row] = edit_first_test_item(
            declarations,
            normal_modules.declarations[row_2_row],
            fixed_points,
            [&](auto const row_0) { fixed_points.fractional_bits[row_0] = 16; });
        return row_2;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);

    manifest = valid_fixed_point_manifest(context);
    manifest.modules.front() = [&] {
        auto row_2{normal_modules.clone(manifest.modules.front())};
        auto const row_2_row{normal_modules.index(row_2)};
        normal_modules.declarations[row_2_row] =
            edit_first_test_item(declarations,
                                 normal_modules.declarations[row_2_row],
                                 fixed_points,
                                 [&](auto const row_0) { fixed_points.total_bits[row_0] = 0; });
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
                declarations.items(normal_modules.declarations[manifest.modules.front()]).front());
            return declarations.range(rows);
        }();
        return row_0;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);
}

TEST(Validation, ValidatesMiniFloatEncodingAndEmitsNoPhysicalType) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& floats{declarations.mini_floats};
    auto& points{declarations.fixed_points};
    auto& modules{declarations.normal_modules};
    auto manifest{valid_mini_float_manifest(context)};
    auto lowered{lower_test_manifest(context, manifest)};
    EXPECT_TRUE(lowered.empty());

    auto invalidate = [&](auto edit) {
        auto invalid{valid_mini_float_manifest(context)};
        auto const module{modules.clone(invalid.modules.front())};
        auto const original{declarations.items(modules.declarations[module])};
        auto rows{std::vector(original.begin(), original.end())};
        auto value{floats.clone(rows.front())};
        edit(value);
        rows.front() = value;
        modules.declarations[module] = declarations.range(rows);
        invalid.modules.front() = module;
        EXPECT_THROW(lower_test_manifest(context, invalid), std::invalid_argument);
    };
    invalidate([&](lispb::codegen::DeclarationHandle& value) { floats.sign_bits[value] = 2; });
    invalidate([&](lispb::codegen::DeclarationHandle& value) { floats.exponent_bits[value] = 1; });
    invalidate([&](lispb::codegen::DeclarationHandle& value) { floats.exponent_bits[value] = 16; });
    invalidate(
        [&](lispb::codegen::DeclarationHandle& value) { floats.significand_bits[value] = 63; });
    invalidate([&](lispb::codegen::DeclarationHandle& value) {
        floats.exponent_bits[value] = 15;
        floats.significand_bits[value] = 49;
    });
    invalidate(
        [&](lispb::codegen::DeclarationHandle& value) { floats.exponent_bias[value] = -32'769; });
    invalidate(
        [&](lispb::codegen::DeclarationHandle& value) { floats.exponent_bias[value] = 32'768; });

    manifest = valid_mini_float_manifest(context);

    manifest.modules.front() = [&] {
        auto row_0{modules.clone(manifest.modules.front())};
        modules.declarations[row_0] = [&] {
            auto const original_items{declarations.items(modules.declarations[row_0])};
            auto rows{std::vector(original_items.begin(), original_items.end())};
            rows.push_back(append_test_row(points, [&](auto const new_fixed_points) {
                points.name[new_fixed_points] =
                    floats.name[declarations.items(modules.declarations[manifest.modules.front()])
                                    .front()];
                points.signedness[new_fixed_points] = true;
                points.total_bits[new_fixed_points] = 16;
                points.fractional_bits[new_fixed_points] = 4;
                points.rounding[new_fixed_points] = FixedPointRounding::nearest_even;
            }));
            return declarations.range(rows);
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
        normal_modules.declarations[row_2_row] =
            edit_first_test_item(declarations,
                                 normal_modules.declarations[row_2_row],
                                 optional_sentinels,
                                 [&](auto const row_0) {
                                     optional_sentinels.source[row_0] =
                                         make_test_type_ref(context, "std::uint32_t");
                                 });
        return row_2;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);

    manifest = valid_optional_sentinel_manifest(context);
    manifest.modules[1] = [&] {
        auto row_2{normal_modules.clone(manifest.modules[1])};
        auto const row_2_row{normal_modules.index(row_2)};
        normal_modules.declarations[row_2_row] =
            edit_first_test_item(declarations,
                                 normal_modules.declarations[row_2_row],
                                 optional_sentinels,
                                 [&](auto const row_0) {
                                     optional_sentinels.sentinel[row_0] = strings.intern("Missing");
                                 });
        return row_2;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);

    manifest = valid_optional_sentinel_manifest(context);
    manifest.modules[1] = [&] {
        auto row_2{normal_modules.clone(manifest.modules[1])};
        auto const row_2_row{normal_modules.index(row_2)};
        normal_modules.declarations[row_2_row] =
            edit_first_test_item(declarations,
                                 normal_modules.declarations[row_2_row],
                                 optional_sentinels,
                                 [&](auto const row_0) {
                                     optional_sentinels.sentinel[row_0] = strings.intern("Unknown");
                                 });
        return row_2;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);

    manifest = valid_optional_sentinel_manifest(context);
    manifest.modules[0] = [&] {
        auto row_2{normal_modules.clone(manifest.modules[0])};
        auto const row_2_row{normal_modules.index(row_2)};
        normal_modules.declarations[row_2_row] = edit_first_test_item(
            declarations,
            normal_modules.declarations[row_2_row],
            declarations.integer_scalars,
            [&](auto const row_0) { declarations.integer_scalars.named_codes[row_0] = {}; });
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
            rows.push_back(append_test_row(fixed_points, [&](auto const new_fixed_points) {
                fixed_points.name[new_fixed_points] =
                    optional_sentinels
                        .name[declarations.items(normal_modules.declarations[manifest.modules[1]])
                                  .front()];
                fixed_points.signedness[new_fixed_points] = false;
                fixed_points.total_bits[new_fixed_points] = 8;
                fixed_points.fractional_bits[new_fixed_points] = 0;
                fixed_points.rounding[new_fixed_points] = FixedPointRounding::nearest_even;
            }));
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
        normal_modules.declarations[row_2_row] =
            edit_first_test_item(declarations,
                                 normal_modules.declarations[row_2_row],
                                 optional_presence_bits,
                                 [&](auto const row_0) {
                                     optional_presence_bits.source[row_0] =
                                         make_test_type_ref(context, "std::uint32_t");
                                 });
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
            rows.push_back(
                append_test_row(optional_sentinels, [&](auto const new_optional_sentinels) {
                    optional_sentinels.name[new_optional_sentinels] =
                        optional_presence_bits
                            .name[declarations
                                      .items(normal_modules.declarations[manifest.modules[1]])
                                      .front()];
                    optional_sentinels.source[new_optional_sentinels] =
                        make_test_type_ref(context, "@scalars/DamageReason");
                    optional_sentinels.sentinel[new_optional_sentinels] = strings.intern("Invalid");
                }));
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
    auto settings_row{module_settings_store.index(normal_modules.settings[module])};
    module_settings_store.namespace_name[settings_row] = strings.intern("project");
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_enum_module(context);
    settings_row = module_settings_store.index(normal_modules.settings[module]);
    module_settings_store.source[settings_row].reset();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_enum_module(context);
    normal_modules.enum_helper_namespace[module] = strings.intern("bad-name");
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_enum_module(context);
    auto enum_row{enums.index(declarations.items(normal_modules.declarations[module]).front())};
    auto const original_conversions{declarations.items(enums.conversions[enum_row])};
    auto conversions{std::vector(original_conversions.begin(), original_conversions.end())};
    conversions.push_back(EnumConversion::string_view);
    enums.conversions[enum_row] = declarations.range(conversions);
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_enum_module(context);
    enum_row = enums.index(declarations.items(normal_modules.declarations[module]).front());
    enums.reflection[enum_row] = EnumReflection::none;
    auto const first_value{declarations.items(enums.values[enum_row]).front()};
    declarations.enumerators.hidden[first_value] = true;
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

        normal_modules.declarations[invalid] = edit_first_test_item(
            declarations, normal_modules.declarations[invalid], enums, [&](auto const row_0) {
                enums.helpers[row_0] = declarations.range<NativeEnumHelper>(helpers);
            });
        normal_modules.declarations[invalid] = edit_first_test_item(
            declarations, normal_modules.declarations[invalid], enums, [&](auto const row_2) {
                auto const row_2_row{enums.index(row_2)};
                enums.values[row_2_row] = edit_first_test_item(
                    declarations,
                    enums.values[row_2_row],
                    declarations.enumerators,
                    [&](auto const row_0) {
                        declarations.enumerators.serialized_name[row_0].reset();
                    });
            });
        EXPECT_THROW(lower_test_manifest(context, manifest_with(context, invalid)),
                     std::invalid_argument);
    }
    auto unreal{valid_enum_module(context)};
    normal_modules.declarations[unreal] = edit_first_test_item(
        declarations, normal_modules.declarations[unreal], enums, [&](auto const row_0) {
            enums.helpers[row_0] =
                declarations.range<lispb::codegen::NativeEnumHelper>({NativeEnumHelper::traits});
        });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, unreal)),
                 std::invalid_argument);

    auto module{valid_native_enum_module(context)};
    normal_modules.declarations[module] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module])};
        auto rows{std::vector(original_items.begin(), original_items.end())};
        rows.push_back(
            declarations.items(normal_modules.declarations[valid_enum_module(context)]).front());
        return declarations.range(rows);
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_native_enum_module(context);
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], enums, [&](auto const row_0) {
            enums.reflection[row_0] = EnumReflection::uenum;
        });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_native_enum_module(context);
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], enums, [&](auto const row_0) {
            enums.enum_array[row_0] = true;
        });
    EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));

    module = valid_native_enum_module(context);
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], enums, [&](auto const row_0) {
            enums.conversions[row_0] =
                declarations.range<lispb::codegen::EnumConversion>({EnumConversion::string_view});
        });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_native_enum_module(context);
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], enums, [&](auto const row_0) {
            enums.export_specifier[row_0] = strings.intern("PROJECT_API");
        });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_native_enum_module(context);
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], enums, [&](auto const row_0) {
            enums.unreal_projection[row_0] = append_test_row(
                enum_unreal_projections, [&](auto const new_enum_unreal_projections) {
                    enum_unreal_projections.name[new_enum_unreal_projections] =
                        strings.intern("ENativeMode");
                    enum_unreal_projections.header[new_enum_unreal_projections] =
                        strings.intern("Project/NativeMode.h");
                    enum_unreal_projections.header_include[new_enum_unreal_projections] =
                        strings.intern("Project/NativeMode.h");
                    enum_unreal_projections.conversion_header[new_enum_unreal_projections] =
                        strings.intern("Project/NativeModeConversion.h");
                    enum_unreal_projections.native_header_include[new_enum_unreal_projections] =
                        strings.intern("project/NativeEnums.h");
                    enum_unreal_projections.reflection[new_enum_unreal_projections] =
                        EnumReflection::none;
                });
        });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_enum_module(context);
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], enums, [&](auto const row_0) {
            enums.unreal_projection[row_0] = append_test_row(
                enum_unreal_projections, [&](auto const new_enum_unreal_projections) {
                    enum_unreal_projections.name[new_enum_unreal_projections] =
                        strings.intern("ENativeMode");
                    enum_unreal_projections.header[new_enum_unreal_projections] =
                        strings.intern("Project/NativeMode.h");
                    enum_unreal_projections.header_include[new_enum_unreal_projections] =
                        strings.intern("Project/NativeMode.h");
                    enum_unreal_projections.conversion_header[new_enum_unreal_projections] =
                        strings.intern("Project/NativeModeConversion.h");
                    enum_unreal_projections.native_header_include[new_enum_unreal_projections] =
                        strings.intern("project/NativeEnums.h");
                });
        });
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
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], enums, [&](auto const row_0) {
            auto const row_0_row{enums.index(row_0)};
            enums.conversions[row_0_row] = [&] {
                auto const original_items{declarations.items(enums.conversions[row_0_row])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.push_back(EnumConversion::try_parse_serialized);
                return declarations.range(rows);
            }();
        });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_enum_module(context);
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], enums, [&](auto const row_2) {
            auto const row_2_row{enums.index(row_2)};
            enums.values[row_2_row] = edit_first_test_item(
                declarations, enums.values[row_2_row], enumerators, [&](auto const row_0) {
                    enumerators.serialized_name[row_0] = strings.intern("value");
                });
        });
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], enums, [&](auto const row_0) {
            auto const row_0_row{enums.index(row_0)};
            enums.values[row_0_row] = [&] {
                auto const original_items{declarations.items(enums.values[row_0_row])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.push_back(append_test_row(enumerators, [&](auto const new_enumerators) {
                    enumerators.name[new_enumerators] = strings.intern("Other");
                    enumerators.initializer[new_enumerators] = std::nullopt;
                    enumerators.display_name[new_enumerators] = std::nullopt;
                    enumerators.hidden[new_enumerators] = false;
                    enumerators.serialized_name[new_enumerators] = strings.intern("value");
                }));
                return declarations.range(rows);
            }();
        });
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], enums, [&](auto const row_0) {
            auto const row_0_row{enums.index(row_0)};
            enums.conversions[row_0_row] = [&] {
                auto const original_items{declarations.items(enums.conversions[row_0_row])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.push_back(EnumConversion::try_parse_serialized);
                return declarations.range(rows);
            }();
        });
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
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], enums, [&](auto const row_0) {
            enums.enum_array[row_0] = true;
        });
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], enums, [&](auto const row_2) {
            auto const row_2_row{enums.index(row_2)};
            enums.values[row_2_row] = edit_first_test_item(
                declarations, enums.values[row_2_row], enumerators, [&](auto const row_0) {
                    enumerators.initializer[row_0] = strings.intern("0");
                });
        });
    EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));

    for (auto const* code : {"2", "0", "-1", "unknown_code", "0x2"}) {
        SCOPED_TRACE(code);
        module = valid_enum_module(context);

        normal_modules.declarations[module] = edit_first_test_item(
            declarations, normal_modules.declarations[module], enums, [&](auto const row_0) {
                enums.enum_array[row_0] = true;
            });
        normal_modules.declarations[module] = edit_first_test_item(
            declarations, normal_modules.declarations[module], enums, [&](auto const row_0) {
                enums.values[row_0] = declarations.range<lispb::codegen::EnumeratorId>(
                    {append_test_row(enumerators,
                                     [&](auto const new_enumerators) {
                                         enumerators.name[new_enumerators] =
                                             strings.intern("First");
                                         enumerators.initializer[new_enumerators] =
                                             strings.intern("0");
                                     }),
                     append_test_row(enumerators, [&](auto const new_enumerators) {
                         enumerators.name[new_enumerators] = strings.intern("Second");
                         enumerators.initializer[new_enumerators] = strings.intern(code);
                     })});
            });
        EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                     std::invalid_argument);
    }

    module = valid_native_enum_module(context);

    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], enums, [&](auto const row_0) {
            enums.enum_array[row_0] = true;
        });
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], enums, [&](auto const row_0) {
            enums.values[row_0] = declarations.range<lispb::codegen::EnumeratorId>(
                {append_test_row(enumerators,
                                 [&](auto const new_enumerators) {
                                     enumerators.name[new_enumerators] = strings.intern("First");
                                     enumerators.initializer[new_enumerators] =
                                         strings.intern("0x0");
                                 }),
                 append_test_row(enumerators, [&](auto const new_enumerators) {
                     enumerators.name[new_enumerators] = strings.intern("Second");
                     enumerators.initializer[new_enumerators] = strings.intern("1");
                 })});
        });
    EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));

    module = valid_enum_module(context);
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], enums, [&](auto const row_0) {
            enums.enum_array[row_0] = true;
        });
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], enums, [&](auto const row_2) {
            auto const row_2_row{enums.index(row_2)};
            enums.values[row_2_row] = edit_first_test_item(
                declarations, enums.values[row_2_row], enumerators, [&](auto const row_0) {
                    enumerators.hidden[row_0] = true;
                });
        });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_enum_module(context);
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], enums, [&](auto const row_0) {
            enums.count[row_0] = strings.intern("Value");
        });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_enum_module(context);
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], enums, [&](auto const row_0) {
            enums.enum_array[row_0] = true;
        });
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], enums, [&](auto const row_0) {
            enums.count[row_0] = strings.intern("Missing");
        });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_enum_module(context);
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], enums, [&](auto const row_0) {
            enums.enum_array[row_0] = true;
        });
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], enums, [&](auto const row_0) {
            auto const row_0_row{enums.index(row_0)};
            enums.values[row_0_row] = [&] {
                auto const original_items{declarations.items(enums.values[row_0_row])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.push_back(append_test_row(enumerators, [&](auto const new_enumerators) {
                    enumerators.name[new_enumerators] = strings.intern("COUNT");
                    enumerators.initializer[new_enumerators] = std::nullopt;
                    enumerators.display_name[new_enumerators] = std::nullopt;
                    enumerators.hidden[new_enumerators] = true;
                }));
                return declarations.range(rows);
            }();
        });
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], enums, [&](auto const row_0) {
            auto const row_0_row{enums.index(row_0)};
            enums.values[row_0_row] = [&] {
                auto const original_items{declarations.items(enums.values[row_0_row])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.push_back(append_test_row(enumerators, [&](auto const new_enumerators) {
                    enumerators.name[new_enumerators] = strings.intern("After");
                }));
                return declarations.range(rows);
            }();
        });
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], enums, [&](auto const row_0) {
            enums.count[row_0] = strings.intern("COUNT");
        });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_enum_module(context);
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], enums, [&](auto const row_0) {
            enums.enum_array[row_0] = true;
        });
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], enums, [&](auto const row_0) {
            auto const row_0_row{enums.index(row_0)};
            enums.values[row_0_row] = [&] {
                auto const original_items{declarations.items(enums.values[row_0_row])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.push_back(append_test_row(enumerators, [&](auto const new_enumerators) {
                    enumerators.name[new_enumerators] = strings.intern("COUNT");
                }));
                return declarations.range(rows);
            }();
        });
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], enums, [&](auto const row_0) {
            enums.count[row_0] = strings.intern("COUNT");
        });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_enum_module(context);
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], enums, [&](auto const row_0) {
            enums.enum_array[row_0] = true;
        });
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], enums, [&](auto const row_0) {
            enums.values[row_0] = declarations.range<lispb::codegen::EnumeratorId>(
                {append_test_row(enumerators, [&](auto const new_enumerators) {
                    enumerators.name[new_enumerators] = strings.intern("COUNT");
                    enumerators.initializer[new_enumerators] = std::nullopt;
                    enumerators.display_name[new_enumerators] = std::nullopt;
                    enumerators.hidden[new_enumerators] = true;
                })});
        });
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], enums, [&](auto const row_0) {
            enums.count[row_0] = strings.intern("COUNT");
        });
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

    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], enums, [&](auto const row_0) {
            enums.underlying_type[row_0] = make_test_type_ref(context, "@native_int8");
        });
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], enums, [&](auto const row_0) {
            enums.values[row_0] = declarations.range<lispb::codegen::EnumeratorId>(
                {append_test_row(enumerators,
                                 [&](auto const new_enumerators) {
                                     enumerators.name[new_enumerators] = strings.intern("Minimum");
                                     enumerators.initializer[new_enumerators] =
                                         strings.intern("-128");
                                 }),
                 append_test_row(enumerators, [&](auto const new_enumerators) {
                     enumerators.name[new_enumerators] = strings.intern("Maximum");
                     enumerators.initializer[new_enumerators] = strings.intern("127");
                 })});
        });
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], enums, [&](auto const row_0) {
            enums.count[row_0].reset();
        });
    auto const types{std::vector<std::pair<std::string_view, CppTypeId>>{
        {"native_int8",
         context.storage.cpp_types.add(
             "std::int8_t", {context.storage.cpp_types.dependency("std::int8_t", "cstdint")})},
    }};

    EXPECT_NO_THROW(
        static_cast<void>(lower_test_manifest(context, manifest_with(context, module, types))));

    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], enums, [&](auto const row_2) {
            auto const row_2_row{enums.index(row_2)};
            enums.values[row_2_row] = [&] {
                auto const original_items{declarations.items(enums.values[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.back() = [&] {
                    auto row_0{enumerators.clone(rows_1.back())};
                    enumerators.initializer[row_0] = strings.intern("128");
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
        });
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

    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], enums, [&](auto const row_0) {
            auto const row_0_row{enums.index(row_0)};
            enums.values[row_0_row] = [&] {
                auto const original_items{declarations.items(enums.values[row_0_row])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.push_back(append_test_row(enumerators, [&](auto const new_enumerators) {
                    enumerators.name[new_enumerators] = strings.intern("Active");
                }));
                return declarations.range(rows);
            }();
        });
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], enums, [&](auto const row_0) {
            enums.underlying_type[row_0].reset();
        });
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], enums, [&](auto const row_0) {
            enums.bit_width[row_0] = 12;
        });
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], enums, [&](auto const row_0) {
            enums.signedness[row_0] = schema::EnumSignedness::unsigned_;
        });

    EXPECT_NO_THROW(
        static_cast<void>(lower_test_manifest(context, manifest_with(context, module))));

    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], enums, [&](auto const row_2) {
            auto const row_2_row{enums.index(row_2)};
            enums.values[row_2_row] = edit_first_test_item(
                declarations, enums.values[row_2_row], enumerators, [&](auto const row_0) {
                    enumerators.initializer[row_0] = strings.intern("calculate_state()");
                });
        });
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], enums, [&](auto const row_2) {
            auto const row_2_row{enums.index(row_2)};
            enums.values[row_2_row] = [&] {
                auto const original_items{declarations.items(enums.values[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.back() = [&] {
                    auto row_0{enumerators.clone(rows_1.back())};
                    enumerators.initializer[row_0].reset();
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
        });
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], enums, [&](auto const row_0) {
            enums.bit_width[row_0].reset();
        });
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], enums, [&](auto const row_0) {
            enums.signedness[row_0] = schema::EnumSignedness::unspecified;
        });
    EXPECT_THROW(static_cast<void>(lower_test_manifest(context, manifest_with(context, module))),
                 std::invalid_argument);
}

TEST(Validation, RejectsInvalidStaticTableModuleConfiguration) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& static_tables{declarations.static_tables};
    auto module{valid_static_table_module(context)};
    normal_modules.declarations[module] = {};
    EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));

    module = valid_static_table_module(context);
    normal_modules.settings[module] = [&] {
        auto row_0{declarations.module_settings_store.clone(normal_modules.settings[module])};
        declarations.module_settings_store.source[row_0] =
            context.storage.strings.intern("Tables.cpp");
        return row_0;
    }();
    EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));

    module = valid_static_table_module(context);
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], static_tables, [&](auto const row_0) {
            static_tables.rows[row_0] = {};
        });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_static_table_module(context);
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], static_tables, [&](auto const row_0) {
            static_tables.columns[row_0] = {};
        });
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
    normal_modules_schemas.declarations[normal_modules_schemas.index(module)] =
        edit_first_test_item(
            declarations,
            normal_modules_schemas.declarations[normal_modules_schemas.index(module)],
            static_tables,
            [&](auto const row_0) {
                auto const row_0_row{static_tables.index(row_0)};
                static_tables.rows[row_0_row] = [&] {
                    auto const original_items{declarations.items(static_tables.rows[row_0_row])};
                    auto rows{std::vector(original_items.begin(), original_items.end())};
                    rows.push_back(
                        append_test_row(static_table_rows, [&](auto const new_static_table_rows) {
                            static_table_rows.name[new_static_table_rows] = strings.intern("first");
                        }));
                    return declarations.range(rows);
                }();
            });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_static_table_module(context);
    normal_modules_schemas.declarations[normal_modules_schemas.index(module)] =
        edit_first_test_item(
            declarations,
            normal_modules_schemas.declarations[normal_modules_schemas.index(module)],
            static_tables,
            [&](auto const row_2) {
                auto const row_2_row{static_tables.index(row_2)};
                static_tables.rows[row_2_row] =
                    edit_first_test_item(declarations,
                                         static_tables.rows[row_2_row],
                                         static_table_rows,
                                         [&](auto const row_0) {
                                             static_table_rows.name[row_0] =
                                                 strings.intern("bad-name");
                                         });
            });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_static_table_module(context);
    normal_modules_schemas
        .declarations[normal_modules_schemas.index(module)] = edit_first_test_item(
        declarations,
        normal_modules_schemas.declarations[normal_modules_schemas.index(module)],
        static_tables,
        [&](auto const row_0) {
            auto const row_0_row{static_tables.index(row_0)};
            static_tables.columns[row_0_row] = [&] {
                auto const original_items{declarations.items(static_tables.columns[row_0_row])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.push_back(
                    declarations
                        .items(
                            static_tables
                                .columns[declarations
                                             .items(normal_modules_schemas.declarations
                                                        [declarations.normal_modules.index(module)])
                                             .front()])
                        .front());
                return declarations.range(rows);
            }();
        });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_static_table_module(context);
    normal_modules_schemas.declarations[normal_modules_schemas.index(module)] =
        edit_first_test_item(
            declarations,
            normal_modules_schemas.declarations[normal_modules_schemas.index(module)],
            static_tables,
            [&](auto const row_2) {
                auto const row_2_row{static_tables.index(row_2)};
                static_tables.columns[row_2_row] =
                    edit_first_test_item(declarations,
                                         static_tables.columns[row_2_row],
                                         declarations.static_table_columns,
                                         [&](auto const row_0) {
                                             declarations.static_table_columns.type[row_0] =
                                                 make_test_type_ref(context, "@missing");
                                         });
            });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

struct InvalidSchemaTextCase {
    char const* name;
    std::string_view value;
};

using StaticTableColumnCollision = testing::TestWithParam<InvalidSchemaTextCase>;

TEST_P(StaticTableColumnCollision, RejectsGeneratedApiNames) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto const module{valid_static_table_module(context)};
    auto const table{declarations.items(declarations.normal_modules.declarations[module]).front()};
    auto const column{declarations.items(declarations.static_tables.columns[table]).front()};
    declarations.static_table_columns.name[column] =
        context.storage.strings.intern(GetParam().value);

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

INSTANTIATE_TEST_SUITE_P(
    Validation,
    StaticTableColumnCollision,
    testing::Values(InvalidSchemaTextCase{"FValues", "FValues"},
                    InvalidSchemaTextCase{"num_rows", "num_rows"},
                    InvalidSchemaTextCase{"num", "num"},
                    InvalidSchemaTextCase{"apply_arrays", "apply_arrays"},
                    InvalidSchemaTextCase{"apply_array_pairs", "apply_array_pairs"},
                    InvalidSchemaTextCase{"first_index", "first_index"}),
    [](testing::TestParamInfo<InvalidSchemaTextCase> const& info) { return info.param.name; });

TEST(Validation, RejectsInvalidStaticTableGroups) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules_schemas{declarations.normal_modules};
    auto& static_tables{declarations.static_tables};
    auto& static_table_groups{declarations.static_table_groups};

    auto& strings{context.storage.strings};
    auto module{valid_static_table_module(context)};
    normal_modules_schemas.declarations[normal_modules_schemas.index(module)] =
        edit_first_test_item(
            declarations,
            normal_modules_schemas.declarations[normal_modules_schemas.index(module)],
            static_tables,
            [&](auto const row_0) {
                static_tables.groups[row_0] =
                    declarations.range<lispb::codegen::StaticTableGroupId>({append_test_row(
                        static_table_groups, [&](auto const new_static_table_groups) {
                            static_table_groups.name[new_static_table_groups] =
                                strings.intern("point");
                            static_table_groups.type[new_static_table_groups] =
                                make_test_type_ref(context, "FPoint");
                            static_table_groups.columns[new_static_table_groups] =
                                declarations.range<lispb::StringId>({strings.intern("ids")});
                        })});
            });

    auto duplicate_group{module};
    normal_modules_schemas.declarations[normal_modules_schemas.index(duplicate_group)] =
        edit_first_test_item(
            declarations,
            normal_modules_schemas.declarations[normal_modules_schemas.index(duplicate_group)],
            static_tables,
            [&](auto const row_0) {
                auto const row_0_row{static_tables.index(row_0)};
                static_tables.groups[row_0_row] = [&] {
                    auto const original_items{declarations.items(static_tables.groups[row_0_row])};
                    auto rows{std::vector(original_items.begin(), original_items.end())};
                    rows.push_back(
                        declarations
                            .items(
                                static_tables.groups[declarations
                                                         .items(normal_modules_schemas.declarations
                                                                    [declarations.normal_modules
                                                                         .index(duplicate_group)])
                                                         .front()])
                            .front());
                    return declarations.range(rows);
                }();
            });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, duplicate_group)),
                 std::invalid_argument);

    auto invalid_name{module};
    normal_modules_schemas.declarations[normal_modules_schemas.index(invalid_name)] =
        edit_first_test_item(
            declarations,
            normal_modules_schemas.declarations[normal_modules_schemas.index(invalid_name)],
            static_tables,
            [&](auto const row_2) {
                auto const row_2_row{static_tables.index(row_2)};
                static_tables.groups[row_2_row] =
                    edit_first_test_item(declarations,
                                         static_tables.groups[row_2_row],
                                         static_table_groups,
                                         [&](auto const row_0) {
                                             static_table_groups.name[row_0] =
                                                 strings.intern("bad-name");
                                         });
            });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, invalid_name)),
                 std::invalid_argument);

    auto unknown_type{module};
    normal_modules_schemas.declarations[normal_modules_schemas.index(unknown_type)] =
        edit_first_test_item(
            declarations,
            normal_modules_schemas.declarations[normal_modules_schemas.index(unknown_type)],
            static_tables,
            [&](auto const row_2) {
                auto const row_2_row{static_tables.index(row_2)};
                static_tables.groups[row_2_row] =
                    edit_first_test_item(declarations,
                                         static_tables.groups[row_2_row],
                                         static_table_groups,
                                         [&](auto const row_0) {
                                             static_table_groups.type[row_0] =
                                                 make_test_type_ref(context, "@missing");
                                         });
            });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, unknown_type)),
                 std::invalid_argument);

    auto empty_columns{module};
    normal_modules_schemas.declarations[normal_modules_schemas.index(empty_columns)] =
        edit_first_test_item(
            declarations,
            normal_modules_schemas.declarations[normal_modules_schemas.index(empty_columns)],
            static_tables,
            [&](auto const row_2) {
                auto const row_2_row{static_tables.index(row_2)};
                static_tables.groups[row_2_row] = edit_first_test_item(
                    declarations,
                    static_tables.groups[row_2_row],
                    static_table_groups,
                    [&](auto const row_0) { static_table_groups.columns[row_0] = {}; });
            });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, empty_columns)),
                 std::invalid_argument);

    auto unknown_column{module};
    normal_modules_schemas.declarations[normal_modules_schemas.index(unknown_column)] =
        edit_first_test_item(
            declarations,
            normal_modules_schemas.declarations[normal_modules_schemas.index(unknown_column)],
            static_tables,
            [&](auto const row_2) {
                auto const row_2_row{static_tables.index(row_2)};
                static_tables.groups[row_2_row] = edit_first_test_item(
                    declarations,
                    static_tables.groups[row_2_row],
                    static_table_groups,
                    [&](auto const row_0) {
                        static_table_groups.columns[row_0] =
                            declarations.range<lispb::StringId>({strings.intern("missing")});
                    });
            });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, unknown_column)),
                 std::invalid_argument);

    auto duplicate_column{module};
    normal_modules_schemas.declarations[normal_modules_schemas.index(duplicate_column)] =
        edit_first_test_item(
            declarations,
            normal_modules_schemas.declarations[normal_modules_schemas.index(duplicate_column)],
            static_tables,
            [&](auto const row_2) {
                auto const row_2_row{static_tables.index(row_2)};
                static_tables.groups[row_2_row] = edit_first_test_item(
                    declarations,
                    static_tables.groups[row_2_row],
                    static_table_groups,
                    [&](auto const row_0) {
                        static_table_groups.columns[row_0] = declarations.range<lispb::StringId>(
                            {strings.intern("ids"), strings.intern("ids")});
                    });
            });
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
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], static_tables, [&](auto const row_0) {
            auto const row_0_row{static_tables.index(row_0)};
            static_tables.columns[row_0_row] = [&] {
                auto const original_items{declarations.items(static_tables.columns[row_0_row])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.push_back(
                    append_test_row(static_table_columns, [&](auto const new_static_table_columns) {
                        static_table_columns.name[new_static_table_columns] =
                            strings.intern("get_point");
                        static_table_columns.type[new_static_table_columns] =
                            make_test_type_ref(context, "float");
                    }));
                return declarations.range(rows);
            }();
        });
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], static_tables, [&](auto const row_0) {
            static_tables.groups[row_0] = declarations.range<lispb::codegen::StaticTableGroupId>(
                {append_test_row(static_table_groups, [&](auto const new_static_table_groups) {
                    static_table_groups.name[new_static_table_groups] = strings.intern("point");
                    static_table_groups.type[new_static_table_groups] =
                        make_test_type_ref(context, "FPoint");
                    static_table_groups.columns[new_static_table_groups] =
                        declarations.range<lispb::StringId>({strings.intern("ids")});
                })});
        });

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsEmptyModuleOutputs) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& settings{declarations.module_settings_store};
    auto const module{valid_soa_module(context)};
    auto const row{settings.index(normal_modules.settings[module])};
    settings.header[row] = {};

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

struct EmptyModuleSettingCase {
    char const* name;
    decltype(&ModuleSettingsStore::header_include) column;
};

using EmptyModuleSetting = testing::TestWithParam<EmptyModuleSettingCase>;

TEST_P(EmptyModuleSetting, RejectsPresentButEmptyValues) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& settings{declarations.module_settings_store};
    auto const module{valid_soa_module(context)};
    auto const row{settings.index(declarations.normal_modules.settings[module])};
    (settings.*GetParam().column)[row] = context.storage.strings.intern("");

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

INSTANTIATE_TEST_SUITE_P(
    Validation,
    EmptyModuleSetting,
    testing::Values(EmptyModuleSettingCase{"HeaderInclude", &ModuleSettingsStore::header_include},
                    EmptyModuleSettingCase{"Namespace", &ModuleSettingsStore::namespace_name}),
    [](testing::TestParamInfo<EmptyModuleSettingCase> const& info) { return info.param.name; });

struct InvalidIncludeOrderCase {
    char const* name;
    std::vector<std::string_view> prefixes;
};

using InvalidIncludeOrder = testing::TestWithParam<InvalidIncludeOrderCase>;

TEST_P(InvalidIncludeOrder, RejectsDuplicateAndEmptyPrefixes) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto const module{valid_soa_module(context)};
    auto const settings{declarations.normal_modules.settings[module]};
    std::vector<StringId> prefixes;
    for (auto const prefix : GetParam().prefixes) {
        prefixes.push_back(context.storage.strings.intern(prefix));
    }
    declarations.module_settings_store.include_order[settings] = declarations.range(prefixes);

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

INSTANTIATE_TEST_SUITE_P(
    Validation,
    InvalidIncludeOrder,
    testing::Values(InvalidIncludeOrderCase{"Duplicate", {"Project/", "Project/"}},
                    InvalidIncludeOrderCase{"Empty", {""}}),
    [](testing::TestParamInfo<InvalidIncludeOrderCase> const& info) { return info.param.name; });

TEST(Validation, RejectsMalformedQualifiedNamespaces) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& settings{declarations.module_settings_store};
    auto const module{valid_soa_module(context)};
    auto const row{settings.index(normal_modules.settings[module])};
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
    auto const row{settings.index(normal_modules.settings[second])};
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

TEST(Validation, RejectsKeywordsInQualifiedNamespaces) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto const module{valid_soa_module(context)};
    auto const settings{declarations.normal_modules.settings[module]};
    declarations.module_settings_store.namespace_name[settings] =
        context.storage.strings.intern("project::namespace");

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
    normal_modules.declarations[module] =
        edit_first_test_item(declarations,
                             normal_modules.declarations[module],
                             declarations.soas,
                             [&](auto const row_0) { declarations.soas.name[row_0] = {}; });

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
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], soas, [&](auto const row_0) {
            auto const row_0_row{soas.index(row_0)};
            soas.members[row_0_row] = [&] {
                auto const original_items{declarations.items(soas.members[row_0_row])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.push_back(append_test_row(soa_members, [&](auto const new_soa_members) {
                    soa_members.name[new_soa_members] = strings.intern("values");
                    soa_members.kind[new_soa_members] = SoaMemberKind::array;
                    soa_members.type[new_soa_members] = make_test_type_ref(context, "float");
                }));
                return declarations.range(rows);
            }();
        });

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsMalformedSoaMemberIdentifiers) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& soas{declarations.soas};
    auto module{valid_soa_module(context)};
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], soas, [&](auto const row_2) {
            auto const row_2_row{soas.index(row_2)};
            soas.members[row_2_row] =
                edit_first_test_item(declarations,
                                     soas.members[row_2_row],
                                     declarations.soa_members,
                                     [&](auto const row_0) {
                                         declarations.soa_members.name[row_0] =
                                             context.storage.strings.intern("bad-name");
                                     });
        });

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsIncompleteSoaFieldMaskDeclarations) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& soas{declarations.soas};
    auto module{valid_mask_soa_module(context)};
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], soas, [&](auto const row_0) {
            soas.field_enum_name[row_0].reset();
        });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_mask_soa_module(context);
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], soas, [&](auto const row_0) {
            soas.field_mask_name[row_0].reset();
        });
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
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], soas, [&](auto const row_2) {
            auto const row_2_row{soas.index(row_2)};
            soas.members[row_2_row] = edit_first_test_item(
                declarations, soas.members[row_2_row], soa_members, [&](auto const row_0) {
                    soa_members.type[row_0] = make_test_type_ref(context, "uint8");
                });
        });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_mask_soa_module(context);
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], soas, [&](auto const row_2) {
            auto const row_2_row{soas.index(row_2)};
            soas.members[row_2_row] = [&] {
                auto const original_items{declarations.items(soas.members[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1.back() = [&] {
                    auto row_0{soa_members.clone(rows_1.back())};
                    soa_members.mask_field[row_0] = false;
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
        });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_soa_module(context);
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], soas, [&](auto const row_2) {
            auto const row_2_row{soas.index(row_2)};
            soas.members[row_2_row] = edit_first_test_item(
                declarations, soas.members[row_2_row], soa_members, [&](auto const row_0) {
                    soa_members.mask_field[row_0] = true;
                });
        });
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

    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], soas, [&](auto const row_2) {
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
                        rows.push_back(append_test_row(
                            soa_mask_dimensions, [&](auto const new_soa_mask_dimensions) {
                                soa_mask_dimensions.index_name[new_soa_mask_dimensions] =
                                    strings.intern("field_index");
                                soa_mask_dimensions.extent[new_soa_mask_dimensions] =
                                    strings.intern("4");
                            }));
                        return declarations.range(rows);
                    }();
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
        });
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], soas, [&](auto const row_2) {
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
                        rows.push_back(append_test_row(
                            soa_mask_dimensions, [&](auto const new_soa_mask_dimensions) {
                                soa_mask_dimensions.index_name[new_soa_mask_dimensions] =
                                    strings.intern("field_index");
                                soa_mask_dimensions.extent[new_soa_mask_dimensions] =
                                    strings.intern("2");
                            }));
                        return declarations.range(rows);
                    }();
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
        });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_mask_soa_module(context);
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], soas, [&](auto const row_2) {
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
                        rows.push_back(append_test_row(
                            soa_mask_dimensions, [&](auto const new_soa_mask_dimensions) {
                                soa_mask_dimensions.index_name[new_soa_mask_dimensions] =
                                    strings.intern("bad-index");
                                soa_mask_dimensions.extent[new_soa_mask_dimensions] =
                                    strings.intern("4");
                            }));
                        return declarations.range(rows);
                    }();
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
        });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

using InvalidSoaMemberName = testing::TestWithParam<InvalidSchemaTextCase>;

TEST_P(InvalidSoaMemberName, RejectsReservedNamesAndGeneratedApiCollisions) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto const module{valid_soa_module(context)};
    auto const soa{declarations.items(declarations.normal_modules.declarations[module]).front()};
    auto const member{declarations.items(declarations.soas.members[soa]).front()};
    declarations.soa_members.name[member] = context.storage.strings.intern(GetParam().value);

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

INSTANTIATE_TEST_SUITE_P(Validation,
                         InvalidSoaMemberName,
                         testing::Values(InvalidSchemaTextCase{"Keyword", "class"},
                                         InvalidSchemaTextCase{"DoubleUnderscore", "__generated"},
                                         InvalidSchemaTextCase{"ReservedPrefix", "_Reserved"},
                                         InvalidSchemaTextCase{"Num", "num"},
                                         InvalidSchemaTextCase{"Reset", "reset"},
                                         InvalidSchemaTextCase{"CopyElement", "copy_element"}),
                         [](testing::TestParamInfo<InvalidSchemaTextCase> const& info) {
                             return info.param.name;
                         });

TEST(Validation, RejectsUnknownSoaTypeReferences) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& soas{declarations.soas};
    auto module{valid_soa_module(context)};
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], soas, [&](auto const row_2) {
            auto const row_2_row{soas.index(row_2)};
            soas.members[row_2_row] =
                edit_first_test_item(declarations,
                                     soas.members[row_2_row],
                                     declarations.soa_members,
                                     [&](auto const row_0) {
                                         declarations.soa_members.type[row_0] =
                                             make_test_type_ref(context, "@missing");
                                     });
        });

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsCollidingGeneratedSoaTypeNames) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto module{valid_soa_module(context)};
    auto second{
        declarations.soas.clone(declarations.items(normal_modules.declarations[module]).front())};
    declarations.soas.name[second] = context.storage.strings.intern("F_Data");
    normal_modules.declarations[module] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module])};
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
    normal_modules.declarations[module] =
        edit_first_test_item(declarations,
                             normal_modules.declarations[module],
                             declarations.soas,
                             [&](auto const row_0) {
                                 declarations.soas.name[row_0] =
                                     context.storage.strings.intern("Class");
                             });

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

using BlankSoaUsingDeclaration = testing::TestWithParam<InvalidSchemaTextCase>;

TEST_P(BlankSoaUsingDeclaration, RejectsBlankDeclarations) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto const module{valid_soa_module(context)};
    auto const soa{declarations.items(declarations.normal_modules.declarations[module]).front()};
    declarations.soas.using_declarations[soa] =
        declarations.range<StringId>({context.storage.strings.intern(GetParam().value)});

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

INSTANTIATE_TEST_SUITE_P(Validation,
                         BlankSoaUsingDeclaration,
                         testing::Values(InvalidSchemaTextCase{"Empty", ""},
                                         InvalidSchemaTextCase{"Whitespace", " \t\r\n"}),
                         [](testing::TestParamInfo<InvalidSchemaTextCase> const& info) {
                             return info.param.name;
                         });

TEST(Validation, RejectsDuplicateCustomSoaFunctionParameters) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& functions_schemas{declarations.functions};

    auto& parameters_schemas{declarations.parameters};
    auto& strings{context.storage.strings};
    auto module{valid_soa_module(context)};
    normal_modules.declarations[module] = edit_first_test_item(
        declarations,
        normal_modules.declarations[module],
        declarations.soas,
        [&](auto const row_0) {
            declarations.soas.functions[row_0] =
                declarations.range<lispb::codegen::FunctionSchemaId>(
                    {append_test_row(functions_schemas, [&](auto const new_functions) {
                        functions_schemas.name[new_functions] = strings.intern("set");
                        functions_schemas.return_type[new_functions] =
                            make_test_type_ref(context, "void");
                        functions_schemas.parameters[new_functions] =
                            declarations.range<lispb::codegen::ParameterId>(
                                {append_test_row(parameters_schemas,
                                                 [&](auto const new_parameters) {
                                                     parameters_schemas.type[new_parameters] =
                                                         make_test_type_ref(context, "int32");
                                                     parameters_schemas.name[new_parameters] =
                                                         strings.intern("value");
                                                 }),
                                 append_test_row(parameters_schemas,
                                                 [&](auto const new_parameters) {
                                                     parameters_schemas.type[new_parameters] =
                                                         make_test_type_ref(context, "float");
                                                     parameters_schemas.name[new_parameters] =
                                                         strings.intern("value");
                                                 })});
                    })});
        });

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
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], soas, [&](auto const row_0) {
            soas.functions[row_0] = declarations.range<lispb::codegen::FunctionSchemaId>(
                {append_test_row(functions_schemas, [&](auto const new_functions) {
                    functions_schemas.name[new_functions] = strings.intern("set");
                    functions_schemas.return_type[new_functions] =
                        make_test_type_ref(context, "void");
                    functions_schemas.parameters[new_functions] =
                        declarations.range<lispb::codegen::ParameterId>(
                            {append_test_row(parameters_schemas,
                                             [&](auto const new_parameters) {
                                                 parameters_schemas.type[new_parameters] =
                                                     make_test_type_ref(context, "int32");
                                                 parameters_schemas.name[new_parameters] =
                                                     strings.intern("first");
                                                 parameters_schemas.default_value[new_parameters] =
                                                     strings.intern("0");
                                             }),
                             append_test_row(parameters_schemas, [&](auto const new_parameters) {
                                 parameters_schemas.type[new_parameters] =
                                     make_test_type_ref(context, "int32");
                                 parameters_schemas.name[new_parameters] = strings.intern("second");
                             })});
                })});
        });

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_soa_module(context);
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], soas, [&](auto const row_0) {
            soas.functions[row_0] = declarations.range<lispb::codegen::FunctionSchemaId>(
                {append_test_row(functions_schemas, [&](auto const new_functions) {
                    functions_schemas.name[new_functions] = strings.intern("set");
                    functions_schemas.return_type[new_functions] =
                        make_test_type_ref(context, "void");
                    functions_schemas.parameters[new_functions] =
                        declarations.range<lispb::codegen::ParameterId>(
                            {append_test_row(parameters_schemas, [&](auto const new_parameters) {
                                parameters_schemas.type[new_parameters] =
                                    make_test_type_ref(context, "int32");
                                parameters_schemas.name[new_parameters] = strings.intern("value");
                                parameters_schemas.default_value[new_parameters] =
                                    strings.intern(" \t");
                            })});
                })});
        });

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
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], soas, [&](auto const row_0) {
            soas.functions[row_0] = declarations.range<lispb::codegen::FunctionSchemaId>(
                {append_test_row(functions_schemas, [&](auto const new_functions) {
                    functions_schemas.name[new_functions] = strings.intern("reset_values");
                    functions_schemas.return_type[new_functions] =
                        make_test_type_ref(context, "void");
                    functions_schemas.dependencies[new_functions] =
                        declarations.range<lispb::StringId>({strings.intern("missing")});
                })});
        });

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_soa_module(context);
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], soas, [&](auto const row_0) {
            soas.functions[row_0] = declarations.range<lispb::codegen::FunctionSchemaId>(
                {append_test_row(functions_schemas, [&](auto const new_functions) {
                    functions_schemas.name[new_functions] = strings.intern("reset_values");
                    functions_schemas.return_type[new_functions] =
                        make_test_type_ref(context, "void");
                    functions_schemas.dependencies[new_functions] =
                        declarations.range<lispb::StringId>({strings.intern(" \t")});
                })});
        });

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
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], soas, [&](auto const row_0) {
            soas.functions[row_0] = declarations.range<lispb::codegen::FunctionSchemaId>(
                {append_test_row(functions_schemas, [&](auto const new_functions) {
                    functions_schemas.name[new_functions] = strings.intern("class");
                    functions_schemas.return_type[new_functions] =
                        make_test_type_ref(context, "void");
                    functions_schemas.is_inline[new_functions] = true;
                })});
        });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_soa_module(context);
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], soas, [&](auto const row_0) {
            soas.functions[row_0] = declarations.range<lispb::codegen::FunctionSchemaId>(
                {append_test_row(functions_schemas, [&](auto const new_functions) {
                    functions_schemas.name[new_functions] = strings.intern("update");
                    functions_schemas.return_type[new_functions] =
                        make_test_type_ref(context, "void");
                    functions_schemas.is_inline[new_functions] = true;
                    functions_schemas.definition_in_source[new_functions] = true;
                })});
        });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_soa_module(context);
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], soas, [&](auto const row_0) {
            soas.functions[row_0] = declarations.range<lispb::codegen::FunctionSchemaId>(
                {append_test_row(functions_schemas, [&](auto const new_functions) {
                    functions_schemas.name[new_functions] = strings.intern("invalid_static_const");
                    functions_schemas.return_type[new_functions] =
                        make_test_type_ref(context, "void");
                    functions_schemas.is_const[new_functions] = true;
                    functions_schemas.is_static[new_functions] = true;
                })});
        });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_soa_module(context);
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], soas, [&](auto const row_0) {
            soas.functions[row_0] = declarations.range<lispb::codegen::FunctionSchemaId>(
                {append_test_row(functions_schemas, [&](auto const new_functions) {
                    functions_schemas.name[new_functions] =
                        strings.intern("invalid_trailing_return");
                    functions_schemas.return_type[new_functions] =
                        make_test_type_ref(context, "void");
                    functions_schemas.trailing_return_type[new_functions] =
                        make_test_type_ref(context, "int32");
                })});
        });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_soa_module(context);
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], soas, [&](auto const row_0) {
            soas.functions[row_0] = declarations.range<lispb::codegen::FunctionSchemaId>(
                {append_test_row(functions_schemas, [&](auto const new_functions) {
                    functions_schemas.name[new_functions] = strings.intern("invalid_template");
                    functions_schemas.return_type[new_functions] =
                        make_test_type_ref(context, "void");
                    functions_schemas.template_parameters[new_functions] = strings.intern(" \t");
                })});
        });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_soa_module(context);
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], soas, [&](auto const row_0) {
            soas.functions[row_0] = declarations.range<lispb::codegen::FunctionSchemaId>(
                {append_test_row(functions_schemas, [&](auto const new_functions) {
                    functions_schemas.name[new_functions] = strings.intern("invalid_requires");
                    functions_schemas.return_type[new_functions] =
                        make_test_type_ref(context, "void");
                    functions_schemas.requires_clause[new_functions] = strings.intern(" \t");
                })});
        });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

using SoaFunctionNameCollision = testing::TestWithParam<InvalidSchemaTextCase>;

TEST_P(SoaFunctionNameCollision, RejectsGeneratedApiAndTypeNames) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto const module{valid_soa_module(context)};
    auto const soa{declarations.items(declarations.normal_modules.declarations[module]).front()};
    auto& functions{declarations.functions};
    auto const function{functions.append()};
    functions.name[function] = context.storage.strings.intern(GetParam().value);
    functions.return_type[function] = make_test_type_ref(context, "void");
    functions.is_inline[function] = true;
    declarations.soas.functions[soa] = declarations.range<FunctionSchemaId>({function});

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

INSTANTIATE_TEST_SUITE_P(Validation,
                         SoaFunctionNameCollision,
                         testing::Values(InvalidSchemaTextCase{"Sort", "sort"},
                                         InvalidSchemaTextCase{"GetView", "get_view"},
                                         InvalidSchemaTextCase{"TypeName", "FData"}),
                         [](testing::TestParamInfo<InvalidSchemaTextCase> const& info) {
                             return info.param.name;
                         });

TEST(Validation, RejectsMalformedExportSpecifiers) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& strings{context.storage.strings};
    auto soa{valid_soa_module(context)};
    normal_modules.declarations[soa] = edit_first_test_item(
        declarations, normal_modules.declarations[soa], declarations.soas, [&](auto const row_0) {
            declarations.soas.export_specifier[row_0] = strings.intern("bad-specifier");
        });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, soa)), std::invalid_argument);

    auto facade{valid_facade_module(context)};
    normal_modules.declarations[facade] =
        edit_first_test_item(declarations,
                             normal_modules.declarations[facade],
                             declarations.facades,
                             [&](auto const row_0) {
                                 declarations.facades.export_specifier[row_0] =
                                     strings.intern("two words");
                             });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, facade)),
                 std::invalid_argument);

    auto table{valid_static_table_module(context)};
    normal_modules.declarations[table] =
        edit_first_test_item(declarations,
                             normal_modules.declarations[table],
                             declarations.static_tables,
                             [&](auto const row_0) {
                                 declarations.static_tables.export_specifier[row_0] =
                                     strings.intern("bad-specifier");
                             });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, table)),
                 std::invalid_argument);
}

TEST(Validation, RejectsFacadesWithoutMethods) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto module{valid_facade_module(context)};
    normal_modules.declarations[module] =
        edit_first_test_item(declarations,
                             normal_modules.declarations[module],
                             declarations.facades,
                             [&](auto const row_0) { declarations.facades.methods[row_0] = {}; });

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
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], facades, [&](auto const row_2) {
            auto const row_2_row{facades.index(row_2)};
            facades.methods[row_2_row] =
                edit_first_test_item(declarations,
                                     facades.methods[row_2_row],
                                     declarations.facade_methods,
                                     [&](auto const row_0) {
                                         declarations.facade_methods.name[row_0] =
                                             context.storage.strings.intern("bad-name");
                                     });
        });

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsOutOfLineFacadesWithoutSourceOutput) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto module{valid_facade_module(context)};
    normal_modules.declarations[module] = edit_first_test_item(
        declarations,
        normal_modules.declarations[module],
        declarations.facades,
        [&](auto const row_0) { declarations.facades.definitions_in_source[row_0] = true; });

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, AllowsInlineFacadesInModulesWithSourceOutput) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto module{valid_facade_module(context)};
    normal_modules.settings[module] = [&] {
        auto row_0{declarations.module_settings_store.clone(normal_modules.settings[module])};
        declarations.module_settings_store.source[row_0] =
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
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], facades, [&](auto const row_2) {
            auto const row_2_row{facades.index(row_2)};
            facades.methods[row_2_row] = edit_first_test_item(
                declarations,
                facades.methods[row_2_row],
                declarations.facade_methods,
                [&](auto const row_0) {
                    declarations.facade_methods.parameters[row_0] =
                        declarations.range<lispb::codegen::ParameterId>(
                            {append_test_row(parameters_schemas,
                                             [&](auto const new_parameters) {
                                                 parameters_schemas.type[new_parameters] =
                                                     make_test_type_ref(context, "int32");
                                                 parameters_schemas.name[new_parameters] =
                                                     strings.intern("value");
                                             }),
                             append_test_row(parameters_schemas, [&](auto const new_parameters) {
                                 parameters_schemas.type[new_parameters] =
                                     make_test_type_ref(context, "float");
                                 parameters_schemas.name[new_parameters] = strings.intern("value");
                             })});
                });
        });

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsDuplicateFacadeMethodSignatures) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules_schemas{declarations.normal_modules};
    auto& facades{declarations.facades};
    auto module{valid_facade_module(context)};
    normal_modules_schemas
        .declarations[normal_modules_schemas.index(module)] = edit_first_test_item(
        declarations,
        normal_modules_schemas.declarations[normal_modules_schemas.index(module)],
        facades,
        [&](auto const row_0) {
            auto const row_0_row{facades.index(row_0)};
            facades.methods[row_0_row] = [&] {
                auto const original_items{declarations.items(facades.methods[row_0_row])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.push_back(
                    declarations
                        .items(
                            facades
                                .methods[declarations
                                             .items(normal_modules_schemas.declarations
                                                        [declarations.normal_modules.index(module)])
                                             .front()])
                        .front());
                return declarations.range(rows);
            }();
        });

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
            .items(facades.methods[declarations.items(normal_modules.declarations[module]).front()])
            .front())};
    declarations.facade_methods.is_noexcept[second] = true;
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], facades, [&](auto const row_0) {
            auto const row_0_row{facades.index(row_0)};
            facades.methods[row_0_row] = [&] {
                auto const original_items{declarations.items(facades.methods[row_0_row])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.push_back(second);
                return declarations.range(rows);
            }();
        });

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
    normal_modules.declarations[module] = edit_first_test_item(
        declarations,
        normal_modules.declarations[module],
        declarations.facades,
        [&](auto const row_0) {
            declarations.facades.methods[row_0] =
                declarations.range<lispb::codegen::FacadeMethodId>(
                    {append_test_row(facade_methods, [&](auto const new_facade_methods) {
                        facade_methods.name[new_facade_methods] = strings.intern("bind");
                        facade_methods.return_type[new_facade_methods] =
                            make_test_type_ref(context, "void");
                        facade_methods.parameters[new_facade_methods] =
                            declarations.range<lispb::codegen::ParameterId>({append_test_row(
                                parameters_schemas, [&](auto const new_parameters) {
                                    parameters_schemas.type[new_parameters] =
                                        make_test_type_ref(context, "FTarget&");
                                    parameters_schemas.name[new_parameters] =
                                        strings.intern("target");
                                })});
                    })});
        });

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
    normal_modules.declarations[module] = edit_first_test_item(
        declarations, normal_modules.declarations[module], facades, [&](auto const row_2) {
            auto const row_2_row{facades.index(row_2)};
            facades.methods[row_2_row] = edit_first_test_item(
                declarations,
                facades.methods[row_2_row],
                declarations.facade_methods,
                [&](auto const row_0) {
                    declarations.facade_methods.parameters[row_0] =
                        declarations.range<lispb::codegen::ParameterId>(
                            {append_test_row(parameters_schemas,
                                             [&](auto const new_parameters) {
                                                 parameters_schemas.type[new_parameters] =
                                                     make_test_type_ref(context, "int32");
                                                 parameters_schemas.name[new_parameters] =
                                                     strings.intern("first");
                                                 parameters_schemas.default_value[new_parameters] =
                                                     strings.intern("0");
                                             }),
                             append_test_row(parameters_schemas, [&](auto const new_parameters) {
                                 parameters_schemas.type[new_parameters] =
                                     make_test_type_ref(context, "int32");
                                 parameters_schemas.name[new_parameters] = strings.intern("second");
                             })});
                });
        });

    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, RejectsUnknownFacadeValidationDependencies) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto module{valid_facade_module(context)};
    normal_modules.declarations[module] = edit_first_test_item(
        declarations,
        normal_modules.declarations[module],
        declarations.facades,
        [&](auto const row_0) {
            declarations.facades.validation_dependencies[row_0] =
                declarations.range<lispb::StringId>({context.storage.strings.intern("missing")});
        });

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
    normal_modules_schemas.declarations[normal_modules_schemas.index(module)] =
        edit_first_test_item(
            declarations,
            normal_modules_schemas.declarations[normal_modules_schemas.index(module)],
            facades,
            [&](auto const row_0) {
                facades.friends[row_0] =
                    declarations.range<lispb::StringId>({strings.intern("class")});
            });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_facade_module(context);
    normal_modules_schemas.declarations[normal_modules_schemas.index(module)] =
        edit_first_test_item(
            declarations,
            normal_modules_schemas.declarations[normal_modules_schemas.index(module)],
            facades,
            [&](auto const row_0) { facades.target_member_name[row_0] = strings.intern("bind"); });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = valid_facade_module(context);
    normal_modules_schemas.declarations[normal_modules_schemas.index(module)] =
        edit_first_test_item(
            declarations,
            normal_modules_schemas.declarations[normal_modules_schemas.index(module)],
            facades,
            [&](auto const row_2) {
                auto const row_2_row{facades.index(row_2)};
                facades.methods[row_2_row] = edit_first_test_item(
                    declarations,
                    facades.methods[row_2_row],
                    declarations.facade_methods,
                    [&](auto const row_0) {
                        declarations.facade_methods.name[row_0] =
                            facades.target_member_name
                                [declarations
                                     .items(normal_modules_schemas.declarations
                                                [declarations.normal_modules.index(module)])
                                     .front()];
                    });
            });
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
    umbrella_modules.settings[module_row] =
        append_test_row(module_settings_store, [&](auto const new_module_settings_store) {
            module_settings_store.name[new_module_settings_store] = strings.intern("all");
            module_settings_store.header[new_module_settings_store] = strings.intern("All.h");
        });
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
    declarations.umbrella_modules.settings[module] =
        append_test_row(module_settings_store, [&](auto const new_module_settings_store) {
            module_settings_store.name[new_module_settings_store] = strings.intern("all");
            module_settings_store.header[new_module_settings_store] = strings.intern("All.h");
        });

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
    umbrella_modules.settings[module_row] =
        append_test_row(module_settings_store, [&](auto const new_module_settings_store) {
            module_settings_store.name[new_module_settings_store] = strings.intern("all");
            module_settings_store.header[new_module_settings_store] = strings.intern("All.h");
            module_settings_store.source[new_module_settings_store] = strings.intern("All.cpp");
        });
    umbrella_modules.headers[module_row] =
        declarations.range<lispb::StringId>({strings.intern("Value.h")});
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    umbrella_modules.settings[module_row] = [&] {
        auto row_0{module_settings_store.clone(umbrella_modules.settings[module_row])};
        module_settings_store.source[row_0].reset();
        return row_0;
    }();
    umbrella_modules.settings[module_row] = [&] {
        auto row_0{module_settings_store.clone(umbrella_modules.settings[module_row])};
        module_settings_store.namespace_name[row_0] = strings.intern("project");
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
    auto module = append_test_row(settings_modules, [&](auto const new_settings_modules) {
        settings_modules.settings[new_settings_modules] =
            append_test_row(module_settings_store, [&](auto const new_module_settings_store) {
                module_settings_store.name[new_module_settings_store] = strings.intern("settings");
                module_settings_store.header[new_module_settings_store] =
                    strings.intern("Settings.h");
                module_settings_store.source[new_module_settings_store] =
                    strings.intern("Settings.cpp");
            });
        settings_modules.api_name[new_settings_modules] = strings.intern("TSettingsAccess");
        settings_modules.state_name[new_settings_modules] = strings.intern("FSettingsState");
        settings_modules.categories[new_settings_modules] =
            declarations.range<lispb::codegen::SettingCategoryId>(
                {append_test_row(setting_categories, [&](auto const new_setting_categories) {
                    setting_categories.name[new_setting_categories] = strings.intern("video");
                    setting_categories.label[new_setting_categories] = strings.intern("Video");
                })});
        settings_modules.settings_list[new_settings_modules] =
            declarations.range<lispb::codegen::SettingId>(
                {append_test_row(settings_schemas, [&](auto const new_settings) {
                    settings_schemas.name[new_settings] = strings.intern("vsync");
                    settings_schemas.label[new_settings] = strings.intern("VSync");
                    settings_schemas.category[new_settings] = strings.intern("missing");
                    settings_schemas.value_type[new_settings] = make_test_type_ref(context, "bool");
                    settings_schemas.backend[new_settings] = strings.intern("engine");
                    settings_schemas.apply_mode[new_settings] = SettingApplyMode::deferred;
                    settings_schemas.control[new_settings] =
                        append_test_row(setting_controls, [&](auto const new_setting_controls) {
                            setting_controls.kind[new_setting_controls] =
                                SettingControlKind::toggle;
                        });
                })});
    });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    settings_modules.settings_list[module] =
        edit_first_test_item(declarations,
                             settings_modules.settings_list[module],
                             settings_schemas,
                             [&](auto const row_0) {
                                 settings_schemas.category[settings_schemas.index(row_0)] =
                                     strings.intern("video");
                             });
    settings_modules.settings_list[module] = edit_first_test_item(
        declarations,
        settings_modules.settings_list[module],
        settings_schemas,
        [&](auto const row_1) {
            auto const row_1_row{settings_schemas.index(row_1)};
            settings_schemas.control[row_1_row] = [&] {
                auto row_0{setting_controls.clone(settings_schemas.control[row_1_row])};
                setting_controls.kind[row_0] = SettingControlKind::choice;
                return row_0;
            }();
        });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    settings_modules.settings_list[module] = edit_first_test_item(
        declarations,
        settings_modules.settings_list[module],
        settings_schemas,
        [&](auto const row_1) {
            auto const row_1_row{settings_schemas.index(row_1)};
            settings_schemas.control[row_1_row] = [&] {
                auto row_0{setting_controls.clone(settings_schemas.control[row_1_row])};
                setting_controls.kind[row_0] = SettingControlKind::custom;
                return row_0;
            }();
        });
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);
}

TEST(Validation, NormalModuleReservesGeneratedNamesAcrossDeclarationKinds) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& enums{declarations.enums};

    auto& enumerators{declarations.enumerators};
    auto& records{declarations.records};
    auto& record_members{declarations.child_occurrences};
    auto& soas{declarations.soas};
    auto& soa_members{declarations.soa_members};
    auto& strings{context.storage.strings};
    auto const vector{plain_soa(context, "Foo")};
    auto module = normal_module(
        context,
        {vector,
         append_test_row(enums,
                         [&](auto const new_enums) {
                             enums.cpp_projection[new_enums] = CppProjection{};
                             enums.name[new_enums] = strings.intern("foo");
                             enums.underlying_type[new_enums] =
                                 make_test_type_ref(context, "uint8");
                             enums.values[new_enums] =
                                 declarations.range<lispb::codegen::EnumeratorId>(
                                     {append_test_row(enumerators, [&](auto const new_enumerators) {
                                         enumerators.name[new_enumerators] = strings.intern("One");
                                     })});
                         })},
        "Mixed.cpp");
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = normal_module(
        context,
        {vector,
         append_test_row(
             records,
             [&](auto const new_records) {
                 records.cpp_projection[new_records] = CppProjection{};
                 records.name[new_records] = strings.intern("foo");
                 records.members[new_records] =
                     declarations.range<lispb::codegen::ChildOccurrenceId>(
                         {append_test_row(record_members, [&](auto const new_record_members) {
                             record_members.name[new_record_members] = strings.intern("value");
                             record_members.type[new_record_members] =
                                 make_test_type_ref(context, "int32");
                         })});
             })},
        "Mixed.cpp");
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    auto soa = append_test_row(soas, [&](auto const new_soas) {
        soas.name[new_soas] = strings.intern("Data");
        soas.members[new_soas] = declarations.range<lispb::codegen::SoaMemberId>(
            {append_test_row(soa_members, [&](auto const new_soa_members) {
                soa_members.name[new_soa_members] = strings.intern("values");
                soa_members.kind[new_soa_members] = SoaMemberKind::array;
                soa_members.type[new_soa_members] = make_test_type_ref(context, "int32");
            })});
    });
    module = normal_module(
        context,
        {soa,
         append_test_row(enums,
                         [&](auto const new_enums) {
                             enums.cpp_projection[new_enums] = CppProjection{};
                             enums.name[new_enums] = strings.intern("data");
                             enums.underlying_type[new_enums] =
                                 make_test_type_ref(context, "uint8");
                             enums.values[new_enums] =
                                 declarations.range<lispb::codegen::EnumeratorId>(
                                     {append_test_row(enumerators, [&](auto const new_enumerators) {
                                         enumerators.name[new_enumerators] = strings.intern("One");
                                     })});
                         })},
        "Mixed.cpp");
    EXPECT_THROW(lower_test_manifest(context, manifest_with(context, module)),
                 std::invalid_argument);

    module = normal_module(
        context,
        {append_test_row(enums,
                         [&](auto const new_enums) {
                             enums.cpp_projection[new_enums] = CppProjection{};
                             enums.name[new_enums] = strings.intern("State");
                             enums.underlying_type[new_enums] =
                                 make_test_type_ref(context, "uint8");
                             enums.values[new_enums] =
                                 declarations.range<lispb::codegen::EnumeratorId>(
                                     {append_test_row(enumerators, [&](auto const new_enumerators) {
                                         enumerators.name[new_enumerators] = strings.intern("One");
                                     })});
                         }),
         append_test_row(records, [&](auto const new_records) {
             records.cpp_projection[new_records] = CppProjection{};
             records.name[new_records] = strings.intern("Snapshot");
             records.members[new_records] = declarations.range<lispb::codegen::ChildOccurrenceId>(
                 {append_test_row(record_members, [&](auto const new_record_members) {
                     record_members.name[new_record_members] = strings.intern("value");
                     record_members.type[new_record_members] = make_test_type_ref(context, "int32");
                 })});
         })});
    EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));
}

TEST(Validation, NormalModuleAppliesSourceRequirementsPerDeclaration) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& facades{declarations.facades};

    auto& facade_methods{declarations.facade_methods};
    auto& strings{context.storage.strings};
    auto facade = append_test_row(facades, [&](auto const new_facades) {
        facades.name[new_facades] = strings.intern("InlineFacade");
        facades.target_type[new_facades] = make_test_type_ref(context, "int32");
        facades.target_member_name[new_facades] = strings.intern("target");
        facades.methods[new_facades] = declarations.range<lispb::codegen::FacadeMethodId>(
            {append_test_row(facade_methods, [&](auto const new_facade_methods) {
                facade_methods.name[new_facade_methods] = strings.intern("reset");
                facade_methods.return_type[new_facade_methods] =
                    make_test_type_ref(context, "void");
            })});
    });
    auto module = normal_module(context, {facade}, "Mixed.cpp");
    EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));

    auto const source_facade{facades.clone(facade)};
    facades.definitions_in_source[source_facade] = true;
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
        {append_test_row(enums,
                         [&](auto const new_enums) {
                             enums.cpp_projection[new_enums] = CppProjection{};
                             enums.name[new_enums] = strings.intern("NativeMode");
                             enums.underlying_type[new_enums] =
                                 make_test_type_ref(context, "uint8");
                             enums.values[new_enums] =
                                 declarations.range<lispb::codegen::EnumeratorId>(
                                     {append_test_row(enumerators, [&](auto const new_enumerators) {
                                         enumerators.name[new_enumerators] = strings.intern("One");
                                     })});
                             enums.native_api[new_enums] = true;
                         }),
         append_test_row(enums, [&](auto const new_enums) {
             enums.cpp_projection[new_enums] = CppProjection{};
             enums.name[new_enums] = strings.intern("UnrealMode");
             enums.underlying_type[new_enums] = make_test_type_ref(context, "uint8");
             enums.values[new_enums] = declarations.range<lispb::codegen::EnumeratorId>(
                 {append_test_row(enumerators, [&](auto const new_enumerators) {
                     enumerators.name[new_enumerators] = strings.intern("One");
                 })});
         })});
    EXPECT_NO_THROW(lower_test_manifest(context, manifest_with(context, module)));
}

using IncompleteEquivalentType = testing::TestWithParam<InvalidSchemaTextCase>;

TEST_P(IncompleteEquivalentType, RejectsIncompleteMappings) {
    CompilerTestContext context;
    auto manifest{parse_test_manifest(
        context, std::format("(module test :header \"Test.h\" {})", GetParam().value))};
    EXPECT_THROW(lower_test_manifest(context, std::move(manifest)), std::invalid_argument);
}

INSTANTIATE_TEST_SUITE_P(
    Validation,
    IncompleteEquivalentType,
    testing::Values(
        InvalidSchemaTextCase{"MissingType",
                              "(struct Rows (member xs array float :equivalent-member x))"},
        InvalidSchemaTextCase{"MissingMember",
                              "(struct Rows :equivalent-type Pair (member xs array float "
                              ":equivalent-accessor true))"},
        InvalidSchemaTextCase{"UnmappedMember",
                              "(struct Rows :equivalent-type Pair (member xs array float "
                              ":equivalent-member x) (member ys array float))"},
        InvalidSchemaTextCase{
            "ConstructorWithoutType",
            "(struct Rows :equivalent-constructor make_pair (member xs array float))"}),
    [](testing::TestParamInfo<InvalidSchemaTextCase> const& info) { return info.param.name; });

} // namespace
} // namespace lispb::codegen
