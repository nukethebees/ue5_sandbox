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
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lispb::codegen {
namespace {

template <typename Id>
auto first_item(Declarations& declarations, SchemaRange<Id> const range) -> Id {
    auto const items{declarations.items(range)};
    assert(!items.empty());
    return items.front();
}

template <typename T>
auto manifest_with(CompilerTestContext& ctx,
                   T module,
                   std::vector<std::pair<std::string_view, CppTypeId>> const& types = {})
    -> Manifest {
    return lispb::codegen::Manifest{manifest_schema_version,
                                    make_test_registry(ctx, types),
                                    ctx.storage.declarations,
                                    {std::move(module)}};
}

template <typename Module>
void expect_invalid_module(CompilerTestContext& ctx,
                           Module const module,
                           std::string_view const diagnostic = {},
                           std::source_location const location = std::source_location::current()) {
    ::testing::ScopedTrace const trace{
        location.file_name(), static_cast<int>(location.line()), "invalid module"};
    auto const message{capture_exception_message<std::invalid_argument>(
        [&] { lower_test_manifest(ctx, manifest_with(ctx, module)); })};
    ASSERT_TRUE(message) << "Expected invalid schema";
    EXPECT_TRUE(message->contains(diagnostic)) << *message;
}

auto valid_soa_module(CompilerTestContext& ctx) -> lispb::codegen::ModuleHandle {
    return parse_test_manifest(ctx, R"(
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
            CompilerTestContext ctx;
            auto const source{std::format("(module test :header \"Test.h\" :source \"Test.cpp\" "
                                          "(enum State int {} {} (projections (cpp))))",
                                          options,
                                          values)};
            auto const message{capture_exception_message<std::invalid_argument>(
                [&] { render_test_manifest(ctx, parse_test_manifest(ctx, source)); })};
            ASSERT_TRUE(message) << "Expected dense enum diagnostic";
            EXPECT_NE(message->find("zero-based contiguous codes"), std::string_view::npos)
                << *message;
        }
    }
}

TEST(Validation, FunctionQualifiersAreSharedByRecordsAndAllSoaReceivers) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& functions_schemas{declarations.functions};
    auto& type_refs{declarations.type_refs};
    auto& normal_modules{declarations.normal_modules};
    auto& records{declarations.records};
    auto& soas{declarations.soas};
    auto& strings{ctx.storage.strings};
    auto const valid{functions_schemas.append()};
    auto const valid_row{functions_schemas.index(valid)};
    functions_schemas.name[valid_row] = strings.intern("read");
    functions_schemas.return_type[valid_row] = make_test_type_ref(ctx, "int");
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
            auto module{valid_soa_module(ctx)};
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
                {
                    auto const soa{first_item(declarations, normal_modules.declarations[module])};

                    auto const soa_row{soas.index(soa)};
                    auto& functions{receiver == 1   ? soas.functions[soa_row]
                                    : receiver == 2 ? soas.const_view_functions[soa_row]
                                                    : soas.mutable_view_functions[soa_row]};
                    functions = declarations.range<FunctionSchemaId>({function});
                }
            }
            auto const manifest{manifest_with(ctx, module)};
            if (diagnostic.empty()) {
                EXPECT_NO_THROW(lispb::schema::resolve_type_graph(manifest));
            } else {
                auto const message{capture_exception_message<std::invalid_argument>(
                    [&] { lispb::schema::resolve_type_graph(manifest); })};
                ASSERT_TRUE(message) << "Expected " << diagnostic;
                EXPECT_TRUE(message->contains(diagnostic)) << *message;
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
        make_test_type_ref(ctx, "void");
    functions_schemas.body_lines[functions_schemas.index(function)] =
        declarations.range<lispb::StringId>({strings.intern("return;")});
    check(function, "nodiscard function must not return void");
    functions_schemas.return_type[functions_schemas.index(function)] =
        make_test_type_ref(ctx, "auto");
    functions_schemas.trailing_return_type[functions_schemas.index(function)] =
        make_test_type_ref(ctx, "void");
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
        make_test_type_ref(ctx, "auto");
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

auto valid_mask_soa_module(CompilerTestContext& ctx) -> lispb::codegen::ModuleHandle {
    return parse_test_manifest(ctx, R"(
(module soa :header "Soa.h" :source "Soa.cpp"
  (struct FData :field-mask-name FFieldMask :field-enum-name EField
    (member masks array FFieldMask)
    (member values array int32 :mask-field true)))
)")
        .modules.front();
}

auto valid_facade_module(CompilerTestContext& ctx) -> lispb::codegen::ModuleHandle {
    return parse_test_manifest(ctx, R"(
(module facade :header "Facade.h"
  (facade FFacade FTarget target
    (method reset void)))
)")
        .modules.front();
}

auto normal_module(CompilerTestContext& ctx,
                   std::vector<DeclarationHandle> const& declarations,
                   std::optional<std::string_view> source = std::nullopt)
    -> lispb::codegen::ModuleHandle {
    auto& strings{ctx.storage.strings};
    auto& modules{ctx.storage.declarations.normal_modules};
    auto& settings{ctx.storage.declarations.module_settings_store};
    auto const module{modules.append()};
    auto const module_row{modules.index(module)};
    auto const config{settings.append()};
    auto const settings_row{settings.index(config)};
    settings.name[settings_row] = strings.intern("mixed");
    settings.header[settings_row] = strings.intern("Mixed.h");
    settings.source[settings_row] = source ? std::optional{strings.intern(*source)} : std::nullopt;
    modules.settings[module_row] = config;
    modules.declarations[module_row] = ctx.storage.declarations.range(declarations);
    return module;
}

auto plain_soa(CompilerTestContext& ctx, std::string name = "Data")
    -> lispb::codegen::DeclarationHandle {
    auto& declarations{ctx.storage.declarations};
    auto& soas{declarations.soas};
    auto& soa_members{declarations.soa_members};
    auto& strings{ctx.storage.strings};
    return append_test_row(soas, [&](auto const new_soas) {
        soas.name[new_soas] = strings.intern(std::move(name));
        soas.members[new_soas] = declarations.range<lispb::codegen::SoaMemberId>(
            {append_test_row(soa_members, [&](auto const new_soa_members) {
                soa_members.name[new_soa_members] = strings.intern("values");
                soa_members.kind[new_soa_members] = SoaMemberKind::array;
                soa_members.type[new_soa_members] = make_test_type_ref(ctx, "int32");
            })});
    });
}

auto valid_enum_module(CompilerTestContext& ctx) -> ModuleHandle {
    return parse_test_manifest(ctx, R"(
      (module enums :header "Enums.h" :source "Enums.cpp" :helper-namespace project
        (enum EMode uint8 :conversions (string-view) :reflection uenum
          (value Value) (projections (cpp))))
    )")
        .modules.front();
}

auto valid_native_enum_module(CompilerTestContext& ctx) -> ModuleHandle {
    return parse_test_manifest(ctx, R"(
      (module native_enums :header "NativeEnums.h" :namespace project
        (enum NativeMode std::uint8_t :native-api true
          (value Idle :value 0 :serialized-name "idle") (projections (cpp))))
    )")
        .modules.front();
}

auto valid_integer_scalar_module(CompilerTestContext& ctx) -> lispb::codegen::ModuleHandle {
    return parse_test_manifest(ctx, R"(
(module scalars :header "Scalars.h"
  (integer-scalar DamageReason :signed false :minimum 0 :maximum 10
    (code Unknown :value 0)
    (code Invalid :value 15 :sentinel true)))
)")
        .modules.front();
}

auto valid_linear_quantized_manifest(CompilerTestContext& ctx) -> Manifest {
    auto const scalar{valid_integer_scalar_module(ctx)};
    auto manifest{parse_test_manifest(ctx, R"(
      (module representations :header "Representations.h"
        (linear-quantized DamageReasonQ4 :source @scalars/DamageReason
          :bits 4 :reserved-codes 1 :clipping reject))
    )")};
    manifest.modules.insert(manifest.modules.begin(), scalar);
    return manifest;
}

auto valid_integer_varint_manifest(
    CompilerTestContext& ctx,
    bool const signedness = false,
    IntegerVarintEncoding const encoding = IntegerVarintEncoding::unsigned_varint) -> Manifest {
    auto const scalar{valid_integer_scalar_module(ctx)};
    auto& declarations{ctx.storage.declarations};
    auto& scalars{declarations.integer_scalars};
    if (signedness) {
        auto const value{
            first_item(declarations, declarations.normal_modules.declarations[scalar])};
        scalars.signedness[value] = true;
        scalars.minimum_value[value] = -100;
        scalars.maximum_value[value] = 100;
        scalars.named_codes[value] = {};
    }
    auto manifest{parse_test_manifest(ctx, R"(
      (module representations :header "Representations.h"
        (integer-varint DamageReasonVarint :source @scalars/DamageReason :encoding unsigned))
    )")};
    auto const value{first_item(
        declarations, declarations.normal_modules.declarations[manifest.modules.front()])};
    declarations.integer_varints.encoding[value] = encoding;
    manifest.modules.insert(manifest.modules.begin(), scalar);
    return manifest;
}

auto valid_fixed_point_manifest(CompilerTestContext& ctx, bool const signedness = true)
    -> Manifest {
    auto manifest{parse_test_manifest(ctx, R"(
      (module representations :header "Representations.h"
        (fixed-point VelocityQ12_4 :total-bits 16 :form (binary-point :signed true :fractional-bits 4) :rounding nearest-even))
    )")};
    auto& declarations{ctx.storage.declarations};
    auto const value{first_item(
        declarations, declarations.normal_modules.declarations[manifest.modules.front()])};
    declarations.fixed_points.format[value] =
        *BinaryPointFormat::try_make({BitWidth{16}, signedness, 4});
    return manifest;
}

auto valid_mini_float_manifest(CompilerTestContext& ctx) -> Manifest {
    return parse_test_manifest(ctx, R"(
(module representations :header "Representations.h"
  (mini-float CompactFloat :sign-bits 1 :exponent-bits 5 :significand-bits 10 :bias 15))
)");
}

auto valid_optional_sentinel_manifest(CompilerTestContext& ctx) -> Manifest {
    auto const scalar{valid_integer_scalar_module(ctx)};
    auto manifest{parse_test_manifest(ctx, R"(
      (module representations :header "Representations.h"
        (optional-sentinel OptionalDamageReason :source @scalars/DamageReason :sentinel Invalid))
    )")};
    manifest.modules.insert(manifest.modules.begin(), scalar);
    return manifest;
}

auto valid_optional_presence_bit_manifest(CompilerTestContext& ctx) -> Manifest {
    auto const scalar{valid_integer_scalar_module(ctx)};
    auto manifest{parse_test_manifest(ctx, R"(
      (module representations :header "Representations.h"
        (optional-presence-bit PresentDamageReason :source @scalars/DamageReason))
    )")};
    manifest.modules.insert(manifest.modules.begin(), scalar);
    return manifest;
}

auto valid_static_table_module(CompilerTestContext& ctx) -> lispb::codegen::ModuleHandle {
    auto& declarations{ctx.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& module_settings_store{declarations.module_settings_store};
    auto& static_tables{declarations.static_tables};
    auto& static_table_rows{declarations.static_table_rows};
    auto& static_table_columns{declarations.static_table_columns};
    auto& strings{ctx.storage.strings};
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
                                    make_test_type_ref(ctx, "int32");
                            })});
                })});
    });
}

TEST(Validation, IntegerProjectionRequiresUnsignedSufficientStorage) {
    for (auto const storage : {"std::uint8_t", "std::int32_t", "float"}) {
        CompilerTestContext ctx;
        auto const source{
            std::format("(module values :header \"Values.h\" (integer-scalar Index :bit-width 24 "
                        "(projections (cpp :storage {}))))",
                        storage)};
        EXPECT_THROW(render_test_manifest(ctx, parse_test_manifest(ctx, source)),
                     std::invalid_argument);
    }
    CompilerTestContext ctx;
    EXPECT_NO_THROW(render_test_manifest(ctx, parse_test_manifest(ctx, R"(
      (module values :header "Values.h"
        (integer-scalar Index :bit-width 24 (projections (cpp :storage std::uint64_t))))
    )")));
}

TEST(Validation, RejectsEmptyModuleNames) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto module{valid_soa_module(ctx)};
    declarations.module_settings_store.name[normal_modules.settings[module]] = {};

    expect_invalid_module(ctx, module);
}

TEST(Validation, RejectsInvalidEnumDefinitions) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& enums{declarations.enums};
    auto& enumerators{declarations.enumerators};
    auto& strings{ctx.storage.strings};
    auto module{valid_enum_module(ctx)};
    enums.values[first_item(declarations, normal_modules.declarations[module])] = {};

    expect_invalid_module(ctx, module);

    module = valid_enum_module(ctx);
    {
        auto const row_0{first_item(declarations, normal_modules.declarations[module])};

        enums.values[row_0] = [&] {
            auto const original_items{declarations.items(enums.values[row_0])};
            auto rows{std::vector(original_items.begin(), original_items.end())};
            rows.push_back(append_test_row(enumerators, [&](auto const new_enumerators) {
                enumerators.name[new_enumerators] = strings.intern("Value");
            }));
            return declarations.range(rows);
        }();
    }
    expect_invalid_module(ctx, module);

    module = valid_enum_module(ctx);
    enumerators.name[first_item(
        declarations,
        enums.values[first_item(declarations, normal_modules.declarations[module])])] =
        strings.intern("bad-name");

    expect_invalid_module(ctx, module);

    module = valid_enum_module(ctx);
    enums.underlying_type[first_item(declarations, normal_modules.declarations[module])] =
        make_test_type_ref(ctx, "@missing");

    expect_invalid_module(ctx, module);
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
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& modules{declarations.normal_modules};
    auto& enums{declarations.enums};
    auto& enumerators{declarations.enumerators};
    auto& strings{ctx.storage.strings};
    auto const& test_case{GetParam()};
    auto const module{valid_enum_module(ctx)};
    {
        auto const enumeration{first_item(declarations, modules.declarations[module])};

        enums.reflection[enumeration] = EnumReflection::none;
        enums.conversions[enumeration] = {};
        enums.bit_width[enumeration] = test_case.bit_width.transform(
            [](auto const value) { return *EnumBitWidth::try_make(value); });
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
    }

    if (test_case.valid) {
        EXPECT_NO_THROW(lower_test_manifest(ctx, manifest_with(ctx, module)));
    } else {
        expect_invalid_module(ctx, module);
    }
}

INSTANTIATE_TEST_SUITE_P(Validation,
                         EnumDomainValidation,
                         testing::Values(EnumDomainCase{"InsufficientUnsignedWidth",
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
                         [](testing::TestParamInfo<EnumDomainCase> const& info) {
                             return info.param.name;
                         });

TEST(Validation, RejectsInvalidIntegerScalarDomainsAndNamedCodes) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& integer_scalars{declarations.integer_scalars};
    auto& packed_named_codes{declarations.packed_named_codes};
    auto& strings{ctx.storage.strings};
    auto module{valid_integer_scalar_module(ctx)};
    integer_scalars.bit_width[first_item(declarations, normal_modules.declarations[module])] =
        BitWidth{3};

    expect_invalid_module(ctx, module);

    module = valid_integer_scalar_module(ctx);
    integer_scalars.minimum_value[first_item(declarations, normal_modules.declarations[module])] =
        -1;

    expect_invalid_module(ctx, module);

    module = valid_integer_scalar_module(ctx);
    packed_named_codes.value[declarations.items(integer_scalars.named_codes[first_item(
        declarations, normal_modules.declarations[module])])[1]] = 10;

    expect_invalid_module(ctx, module);

    module = valid_integer_scalar_module(ctx);
    packed_named_codes.value[declarations.items(integer_scalars.named_codes[first_item(
        declarations, normal_modules.declarations[module])])[0]] = 11;

    expect_invalid_module(ctx, module);

    module = valid_integer_scalar_module(ctx);
    packed_named_codes.name[declarations.items(integer_scalars.named_codes[first_item(
        declarations, normal_modules.declarations[module])])[1]] = strings.intern("Unknown");

    expect_invalid_module(ctx, module);

    module = valid_integer_scalar_module(ctx);
    packed_named_codes.value[declarations.items(integer_scalars.named_codes[first_item(
        declarations, normal_modules.declarations[module])])[1]] = 0;

    expect_invalid_module(ctx, module);

    module = valid_integer_scalar_module(ctx);
    declarations.module_settings_store.source[normal_modules.settings[module]] =
        strings.intern("Scalars.cpp");

    EXPECT_NO_THROW(lower_test_manifest(ctx, manifest_with(ctx, module)));

    module = valid_integer_scalar_module(ctx);
    auto const lowered{lower_test_manifest(ctx, manifest_with(ctx, module))};
    EXPECT_TRUE(lowered.empty());
    EXPECT_TRUE(render_modules(ctx.ir, lowered).empty());
}

TEST(Validation, LogicalScalarNeedsNoCppProjectionOrNamedCodes) {
    CompilerTestContext ctx;
    EXPECT_NO_THROW(schema::resolve_type_graph(parse_test_manifest(ctx, R"(
      (module values
        (integer-scalar Index :bit-width 24)
        (integer-scalar Delta :bit-width 16 :signed true)
        (integer-scalar Health :bit-width 8 :minimum 0 :maximum 100))
    )")));
}

TEST(Validation, EmptyEditableModulesDoNotEmitCppWithoutDeclarations) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& module_settings_store{declarations.module_settings_store};
    auto& normal_modules{declarations.normal_modules};
    auto& strings{ctx.storage.strings};
    auto settings = [&](std::string const& name) {
        return append_test_row(module_settings_store, [&](auto const new_module_settings_store) {
            module_settings_store.name[new_module_settings_store] = strings.intern(name);
            module_settings_store.header[new_module_settings_store] = strings.intern(name + ".h");
        });
    };
    auto module{append_test_row(normal_modules, [&](auto const new_normal_modules) {
        normal_modules.settings[new_normal_modules] = settings("empty");
    })};
    EXPECT_NO_THROW(render_modules(ctx.ir, lower_test_manifest(ctx, manifest_with(ctx, module))));
    module_settings_store.source[normal_modules.settings[module]] = strings.intern("empty.cpp");

    EXPECT_EQ(render_modules(ctx.ir, lower_test_manifest(ctx, manifest_with(ctx, module))).size(),
              0U);
}

TEST(Validation, ValidatesMemberRelationshipUnits) {
    struct Case {
        SemanticRelationKind kind;
        std::optional<SemanticRelationUnit> unit;
        char const* diagnostic;
    };
    for (auto const soa : {false, true}) {
        for (auto const test :
             {Case{SemanticRelationKind::references, std::nullopt, ""},
              Case{SemanticRelationKind::offset_into, std::nullopt, "requires an explicit unit"},
              Case{SemanticRelationKind::references,
                   SemanticRelationUnit::elements,
                   "unit is only valid for 'offset_into'"},
              Case{SemanticRelationKind::offset_into, SemanticRelationUnit::bytes, ""}}) {
            SCOPED_TRACE(soa);
            SCOPED_TRACE(test.diagnostic);
            CompilerTestContext ctx;
            auto manifest{parse_test_manifest(ctx,
                                              soa ? R"(
              (module values :header "Values.h"
                (struct Target (member values array std::uint32_t))
                (struct User (member values array std::uint32_t)))
            )"
                                                  : R"(
              (module values :header "Values.h"
                (record Target (member value std::uint32_t) (projections (cpp)))
                (record User (member value std::uint32_t) (projections (cpp))))
            )")};
            auto& declarations{ctx.storage.declarations};
            auto const module{manifest.modules.front()};
            auto const owner{
                declarations.items(declarations.normal_modules.declarations[module])[1]};
            auto const relation{declarations.semantic_relations.append()};
            declarations.semantic_relations.kind[relation] = test.kind;
            declarations.semantic_relations.unit[relation] = test.unit;
            declarations.semantic_relations.target[relation] = make_test_type_ref(ctx, "Target");
            if (soa) {
                auto const member{first_item(declarations, declarations.soas.members[owner])};
                declarations.soa_members.relationship[member] = relation;
            } else {
                auto const member{first_item(declarations, declarations.records.members[owner])};
                declarations.child_occurrences.relationship[member] = relation;
            }
            if (std::string_view{test.diagnostic}.empty()) {
                EXPECT_NO_THROW(lower_test_manifest(ctx, manifest));
            } else {
                expect_invalid_module(ctx, module, test.diagnostic);
            }
        }
    }
}

TEST(Validation, RequiresUnsignedIndexCountAndOffsetIntegerScalars) {
    struct Case {
        SemanticRelationKind kind;
        bool signedness;
        std::optional<SemanticRelationUnit> unit;
        char const* diagnostic;
    };
    for (auto const test :
         {Case{SemanticRelationKind::index_into,
               true,
               std::nullopt,
               "requires an unsigned integer scalar"},
          Case{SemanticRelationKind::count_of,
               true,
               std::nullopt,
               "requires an unsigned integer scalar"},
          Case{SemanticRelationKind::offset_into,
               true,
               SemanticRelationUnit::bytes,
               "requires an unsigned integer scalar"},
          Case{SemanticRelationKind::references, false, std::nullopt, ""},
          Case{SemanticRelationKind::offset_into, false, std::nullopt, "requires an explicit unit"},
          Case{SemanticRelationKind::index_into,
               false,
               SemanticRelationUnit::elements,
               "unit is only valid for 'offset_into'"},
          Case{SemanticRelationKind::offset_into, false, SemanticRelationUnit::bytes, ""}}) {
        SCOPED_TRACE(test.diagnostic);
        CompilerTestContext ctx;
        auto const module{valid_integer_scalar_module(ctx)};
        auto& declarations{ctx.storage.declarations};
        auto& scalars{declarations.integer_scalars};
        auto const scalar{
            first_item(declarations, declarations.normal_modules.declarations[module])};
        if (test.signedness) {
            scalars.signedness[scalar] = true;
            scalars.minimum_value[scalar] = -10;
            scalars.maximum_value[scalar] = 10;
            scalars.named_codes[scalar] = {};
        }
        auto const relation{declarations.semantic_relations.append()};
        declarations.semantic_relations.kind[relation] = test.kind;
        declarations.semantic_relations.unit[relation] = test.unit;
        declarations.semantic_relations.target[relation] =
            make_test_type_ref(ctx, "@scalars/DamageReason");
        scalars.relationship[scalar] = relation;
        if (std::string_view{test.diagnostic}.empty()) {
            EXPECT_NO_THROW(lower_test_manifest(ctx, manifest_with(ctx, module)));
        } else {
            expect_invalid_module(ctx, module, test.diagnostic);
        }
    }
}

TEST(Validation, ValidatesLogicalLinearQuantizedRepresentationsWithoutCppOutput) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& integer_scalars{declarations.integer_scalars};
    auto& linear_quantizeds{declarations.linear_quantizeds};
    auto& strings{ctx.storage.strings};
    auto manifest{valid_linear_quantized_manifest(ctx)};
    auto lowered{lower_test_manifest(ctx, manifest)};
    EXPECT_TRUE(lowered.empty());

    manifest = valid_linear_quantized_manifest(ctx);
    integer_scalars
        .signedness[first_item(declarations, normal_modules.declarations[manifest.modules[0]])] =
        true;

    integer_scalars
        .minimum_value[first_item(declarations, normal_modules.declarations[manifest.modules[0]])] =
        -100;

    integer_scalars
        .maximum_value[first_item(declarations, normal_modules.declarations[manifest.modules[0]])] =
        100;

    integer_scalars.named_codes[first_item(declarations,
                                           normal_modules.declarations[manifest.modules[0]])] = {};

    EXPECT_NO_THROW(lower_test_manifest(ctx, manifest));

    manifest = valid_linear_quantized_manifest(ctx);
    linear_quantizeds
        .bit_width[first_item(declarations, normal_modules.declarations[manifest.modules[1]])] =
        BitWidth{1};

    EXPECT_THROW(lower_test_manifest(ctx, manifest), std::invalid_argument);

    manifest = valid_linear_quantized_manifest(ctx);
    linear_quantizeds
        .bit_width[first_item(declarations, normal_modules.declarations[manifest.modules[1]])] =
        BitWidth{2};

    linear_quantizeds.reserved_codes[first_item(
        declarations, normal_modules.declarations[manifest.modules[1]])] = 3;

    EXPECT_THROW(lower_test_manifest(ctx, manifest), std::invalid_argument);

    manifest = valid_linear_quantized_manifest(ctx);
    integer_scalars
        .maximum_value[first_item(declarations, normal_modules.declarations[manifest.modules[0]])] =
        0;

    EXPECT_THROW(lower_test_manifest(ctx, manifest), std::invalid_argument);

    manifest = valid_linear_quantized_manifest(ctx);
    linear_quantizeds
        .source[first_item(declarations, normal_modules.declarations[manifest.modules[1]])] =
        make_test_type_ref(ctx, "std::uint32_t");

    EXPECT_THROW(lower_test_manifest(ctx, manifest), std::invalid_argument);

    manifest = valid_linear_quantized_manifest(ctx);
    declarations.module_settings_store.source[normal_modules.settings[manifest.modules[1]]] =
        strings.intern("Representations.cpp");

    EXPECT_TRUE(render_modules(ctx.ir, lower_test_manifest(ctx, manifest)).empty());
}

TEST(Validation, ValidatesIntegerVarintSourceAndSignedness) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& integer_varints{declarations.integer_varints};

    auto& linear_quantizeds{declarations.linear_quantizeds};

    EXPECT_NO_THROW(lower_test_manifest(ctx, valid_integer_varint_manifest(ctx)));
    EXPECT_NO_THROW(lower_test_manifest(
        ctx, valid_integer_varint_manifest(ctx, true, IntegerVarintEncoding::signed_varint)));
    EXPECT_NO_THROW(lower_test_manifest(
        ctx, valid_integer_varint_manifest(ctx, true, IntegerVarintEncoding::zigzag_varint)));

    EXPECT_THROW(
        lower_test_manifest(
            ctx, valid_integer_varint_manifest(ctx, true, IntegerVarintEncoding::unsigned_varint)),
        std::invalid_argument);
    EXPECT_THROW(
        lower_test_manifest(
            ctx, valid_integer_varint_manifest(ctx, false, IntegerVarintEncoding::signed_varint)),
        std::invalid_argument);
    EXPECT_THROW(
        lower_test_manifest(
            ctx, valid_integer_varint_manifest(ctx, false, IntegerVarintEncoding::zigzag_varint)),
        std::invalid_argument);

    auto manifest{valid_integer_varint_manifest(ctx)};
    integer_varints
        .source[first_item(declarations, normal_modules.declarations[manifest.modules[1]])] =
        make_test_type_ref(ctx, "std::uint32_t");

    EXPECT_THROW(lower_test_manifest(ctx, manifest), std::invalid_argument);

    manifest = valid_integer_varint_manifest(ctx);
    normal_modules.declarations[manifest.modules[1]] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[manifest.modules[1]])};
        auto rows{std::vector(original_items.begin(), original_items.end())};
        rows.push_back(linear_quantizeds.append(integer_varints.name[original_items.front()],
                                                make_test_type_ref(ctx, "@scalars/DamageReason"),
                                                BitWidth{4},
                                                0,
                                                QuantizationClipping::reject,
                                                std::nullopt));
        return declarations.range(rows);
    }();

    EXPECT_THROW(lower_test_manifest(ctx, manifest), std::invalid_argument);
}

TEST(Validation, ValidatesLogicalFixedPointWidthsWithoutCppOutput) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto manifest{valid_fixed_point_manifest(ctx)};
    auto lowered{lower_test_manifest(ctx, manifest)};
    EXPECT_TRUE(lowered.empty());

    EXPECT_NO_THROW(lower_test_manifest(ctx, parse_test_manifest(ctx, R"(
      (module values :header "Values.h" (fixed-point Value :total-bits 8
        :form (binary-point :signed false :fractional-bits 8))))")));
    auto const diagnostic{capture_exception_message<sexpr::SourceError>([&] {
        return parse_test_manifest(ctx, R"(
          (module values :header "Values.h" (fixed-point Value :total-bits 16
            :form (binary-point :signed true :fractional-bits 16))))");
    })};
    ASSERT_TRUE(diagnostic.has_value());
    EXPECT_NE(diagnostic->find("fractional width must be at most 15 so one sign bit remains"),
              std::string::npos);

    for (auto const form : {"(range :minimum 0.0625 :maximum 16)",
                            "(range :minimum -1 :maximum 1)",
                            "(range :minimum 0 :maximum 1)"}) {
        EXPECT_NO_THROW(lower_test_manifest(
            ctx,
            parse_test_manifest(ctx,
                                std::format("(module values :header \"Values.h\" (fixed-point "
                                            "Value :total-bits 8 :form {}))",
                                            form))));
    }
    for (auto const form : {"(range :minimum 1 :maximum 1)",
                            "(range :minimum 2 :maximum 1)",
                            "(range :minimum nan :maximum 1)",
                            "(range :minimum 0 :maximum inf)"}) {
        EXPECT_THROW(
            lower_test_manifest(
                ctx,
                parse_test_manifest(ctx,
                                    std::format("(module values :header \"Values.h\" (fixed-point "
                                                "Value :total-bits 8 :form {}))",
                                                form))),
            sexpr::SourceError);
    }

    EXPECT_THROW(parse_test_manifest(ctx, R"(
      (module values :header "Values.h" (fixed-point Value :total-bits 0
        :form (binary-point :signed false :fractional-bits 0))))"),
                 sexpr::SourceError);

    manifest = valid_fixed_point_manifest(ctx);
    normal_modules.declarations[manifest.modules.front()] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[manifest.modules.front()])};
        auto rows{std::vector(original_items.begin(), original_items.end())};
        rows.push_back(
            declarations.items(normal_modules.declarations[manifest.modules.front()]).front());
        return declarations.range(rows);
    }();

    EXPECT_THROW(lower_test_manifest(ctx, manifest), std::invalid_argument);
}

TEST(Validation, ValidatesMiniFloatEncodingAndEmitsNoPhysicalType) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& floats{declarations.mini_floats};
    auto& points{declarations.fixed_points};
    auto& modules{declarations.normal_modules};
    auto manifest{valid_mini_float_manifest(ctx)};
    auto lowered{lower_test_manifest(ctx, manifest)};
    EXPECT_TRUE(lowered.empty());

    for (auto const fields : {":sign-bits 2 :exponent-bits 5 :significand-bits 10 :bias 15",
                              ":sign-bits 1 :exponent-bits 1 :significand-bits 10 :bias 15",
                              ":sign-bits 1 :exponent-bits 16 :significand-bits 10 :bias 15",
                              ":sign-bits 1 :exponent-bits 5 :significand-bits 63 :bias 15",
                              ":sign-bits 1 :exponent-bits 15 :significand-bits 49 :bias 15",
                              ":sign-bits 1 :exponent-bits 5 :significand-bits 10 :bias -32769",
                              ":sign-bits 1 :exponent-bits 5 :significand-bits 10 :bias 32768"}) {
        EXPECT_THROW(
            parse_test_manifest(
                ctx,
                std::format("(module values :header \"Values.h\" (mini-float Value {}))", fields)),
            sexpr::SourceError);
    }

    manifest = valid_mini_float_manifest(ctx);
    modules.declarations[manifest.modules.front()] = [&] {
        auto const original_items{
            declarations.items(modules.declarations[manifest.modules.front()])};
        auto rows{std::vector(original_items.begin(), original_items.end())};
        rows.push_back(points.append(
            floats.name[declarations.items(modules.declarations[manifest.modules.front()]).front()],
            BinaryPointFormat{{BitWidth{16}, true, 4}},
            std::nullopt,
            FixedPointRounding::nearest_even));
        return declarations.range(rows);
    }();

    EXPECT_THROW(lower_test_manifest(ctx, manifest), std::invalid_argument);
}

TEST(Validation, ValidatesOptionalSentinelSourceAndCodeRole) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& optional_sentinels{declarations.optional_sentinels};
    auto& fixed_points{declarations.fixed_points};
    auto& strings{ctx.storage.strings};
    EXPECT_NO_THROW(lower_test_manifest(ctx, valid_optional_sentinel_manifest(ctx)));

    auto manifest{valid_optional_sentinel_manifest(ctx)};
    optional_sentinels
        .source[first_item(declarations, normal_modules.declarations[manifest.modules[1]])] =
        make_test_type_ref(ctx, "std::uint32_t");

    EXPECT_THROW(lower_test_manifest(ctx, manifest), std::invalid_argument);

    manifest = valid_optional_sentinel_manifest(ctx);
    optional_sentinels
        .sentinel[first_item(declarations, normal_modules.declarations[manifest.modules[1]])] =
        strings.intern("Missing");

    EXPECT_THROW(lower_test_manifest(ctx, manifest), std::invalid_argument);

    manifest = valid_optional_sentinel_manifest(ctx);
    optional_sentinels
        .sentinel[first_item(declarations, normal_modules.declarations[manifest.modules[1]])] =
        strings.intern("Unknown");

    EXPECT_THROW(lower_test_manifest(ctx, manifest), std::invalid_argument);

    manifest = valid_optional_sentinel_manifest(ctx);
    declarations.integer_scalars.named_codes[first_item(
        declarations, normal_modules.declarations[manifest.modules[0]])] = {};

    EXPECT_THROW(lower_test_manifest(ctx, manifest), std::invalid_argument);

    manifest = valid_optional_sentinel_manifest(ctx);
    normal_modules.declarations[manifest.modules[1]] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[manifest.modules[1]])};
        auto rows{std::vector(original_items.begin(), original_items.end())};
        rows.push_back(fixed_points.append(
            optional_sentinels
                .name[declarations.items(normal_modules.declarations[manifest.modules[1]]).front()],
            BinaryPointFormat{{BitWidth{8}, false, 0}},
            std::nullopt,
            FixedPointRounding::nearest_even));
        return declarations.range(rows);
    }();

    EXPECT_THROW(lower_test_manifest(ctx, manifest), std::invalid_argument);
}

TEST(Validation, ValidatesOptionalPresenceBitSourceAndSharedNames) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& optional_presence_bits{declarations.optional_presence_bits};

    auto& optional_sentinels{declarations.optional_sentinels};
    auto& strings{ctx.storage.strings};
    EXPECT_NO_THROW(lower_test_manifest(ctx, valid_optional_presence_bit_manifest(ctx)));

    auto manifest{valid_optional_presence_bit_manifest(ctx)};
    optional_presence_bits
        .source[first_item(declarations, normal_modules.declarations[manifest.modules[1]])] =
        make_test_type_ref(ctx, "std::uint32_t");

    EXPECT_THROW(lower_test_manifest(ctx, manifest), std::invalid_argument);

    manifest = valid_optional_presence_bit_manifest(ctx);
    normal_modules.declarations[manifest.modules[1]] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[manifest.modules[1]])};
        auto rows{std::vector(original_items.begin(), original_items.end())};
        rows.push_back(append_test_row(optional_sentinels, [&](auto const new_optional_sentinels) {
            optional_sentinels.name[new_optional_sentinels] =
                optional_presence_bits.name
                    [declarations.items(normal_modules.declarations[manifest.modules[1]]).front()];
            optional_sentinels.source[new_optional_sentinels] =
                make_test_type_ref(ctx, "@scalars/DamageReason");
            optional_sentinels.sentinel[new_optional_sentinels] = strings.intern("Invalid");
        }));
        return declarations.range(rows);
    }();

    EXPECT_THROW(lower_test_manifest(ctx, manifest), std::invalid_argument);
}

TEST(Validation, RejectsInvalidEnumModuleConfiguration) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& module_settings_store{declarations.module_settings_store};
    auto& enums{declarations.enums};
    auto& strings{ctx.storage.strings};
    auto module{valid_enum_module(ctx)};
    auto settings_row{module_settings_store.index(normal_modules.settings[module])};
    module_settings_store.namespace_name[settings_row] = strings.intern("project");
    expect_invalid_module(ctx, module);

    module = valid_enum_module(ctx);
    settings_row = module_settings_store.index(normal_modules.settings[module]);
    module_settings_store.source[settings_row].reset();
    expect_invalid_module(ctx, module);

    module = valid_enum_module(ctx);
    normal_modules.enum_helper_namespace[module] = strings.intern("bad-name");
    expect_invalid_module(ctx, module);

    module = valid_enum_module(ctx);
    auto enum_row{enums.index(declarations.items(normal_modules.declarations[module]).front())};
    auto const original_conversions{declarations.items(enums.conversions[enum_row])};
    auto conversions{std::vector(original_conversions.begin(), original_conversions.end())};
    conversions.push_back(EnumConversion::string_view);
    enums.conversions[enum_row] = declarations.range(conversions);
    expect_invalid_module(ctx, module);

    module = valid_enum_module(ctx);
    enum_row = enums.index(declarations.items(normal_modules.declarations[module]).front());
    enums.reflection[enum_row] = EnumReflection::none;
    auto const first_value{declarations.items(enums.values[enum_row]).front()};
    declarations.enumerators.hidden[first_value] = true;
    expect_invalid_module(ctx, module);
}

TEST(Validation, RejectsUnsupportedNativeEnumCombinations) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& enums{declarations.enums};
    auto& enum_unreal_projections{declarations.enum_unreal_projections};
    auto& strings{ctx.storage.strings};
    for (auto const& helpers : {std::vector{NativeEnumHelper::traits, NativeEnumHelper::traits},
                                std::vector{static_cast<NativeEnumHelper>(999)},
                                std::vector{NativeEnumHelper::to_serialized_string},
                                std::vector{NativeEnumHelper::try_parse_serialized}}) {
        auto invalid{valid_native_enum_module(ctx)};
        enums.helpers[first_item(declarations, normal_modules.declarations[invalid])] =
            declarations.range<NativeEnumHelper>(helpers);

        declarations.enumerators
            .serialized_name[first_item(
                declarations,
                enums.values[first_item(declarations, normal_modules.declarations[invalid])])]
            .reset();

        EXPECT_THROW(lower_test_manifest(ctx, manifest_with(ctx, invalid)), std::invalid_argument);
    }
    auto unreal{valid_enum_module(ctx)};
    enums.helpers[first_item(declarations, normal_modules.declarations[unreal])] =
        declarations.range<lispb::codegen::NativeEnumHelper>({NativeEnumHelper::traits});

    EXPECT_THROW(lower_test_manifest(ctx, manifest_with(ctx, unreal)), std::invalid_argument);

    auto module{valid_native_enum_module(ctx)};
    normal_modules.declarations[module] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module])};
        auto rows{std::vector(original_items.begin(), original_items.end())};
        rows.push_back(
            declarations.items(normal_modules.declarations[valid_enum_module(ctx)]).front());
        return declarations.range(rows);
    }();
    expect_invalid_module(ctx, module);

    module = valid_native_enum_module(ctx);
    enums.reflection[first_item(declarations, normal_modules.declarations[module])] =
        EnumReflection::uenum;

    expect_invalid_module(ctx, module);

    module = valid_native_enum_module(ctx);
    enums.enum_array[first_item(declarations, normal_modules.declarations[module])] = true;

    EXPECT_NO_THROW(lower_test_manifest(ctx, manifest_with(ctx, module)));

    module = valid_native_enum_module(ctx);
    enums.conversions[first_item(declarations, normal_modules.declarations[module])] =
        declarations.range<lispb::codegen::EnumConversion>({EnumConversion::string_view});

    expect_invalid_module(ctx, module);

    module = valid_native_enum_module(ctx);
    enums.export_specifier[first_item(declarations, normal_modules.declarations[module])] =
        strings.intern("PROJECT_API");

    expect_invalid_module(ctx, module);

    module = valid_native_enum_module(ctx);
    {

        enums.unreal_projection[first_item(declarations, normal_modules.declarations[module])] =
            append_test_row(enum_unreal_projections, [&](auto const new_enum_unreal_projections) {
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
    }
    expect_invalid_module(ctx, module);

    module = valid_enum_module(ctx);
    {

        enums.unreal_projection[first_item(declarations, normal_modules.declarations[module])] =
            append_test_row(enum_unreal_projections, [&](auto const new_enum_unreal_projections) {
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
    }
    expect_invalid_module(ctx, module);
}

TEST(Validation, RejectsIncompleteOrAmbiguousSerializedEnumNames) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& enums{declarations.enums};
    auto& enumerators{declarations.enumerators};
    auto& strings{ctx.storage.strings};
    auto module{valid_enum_module(ctx)};
    {
        auto const row_0{first_item(declarations, normal_modules.declarations[module])};

        enums.conversions[row_0] = [&] {
            auto const original_items{declarations.items(enums.conversions[row_0])};
            auto rows{std::vector(original_items.begin(), original_items.end())};
            rows.push_back(EnumConversion::try_parse_serialized);
            return declarations.range(rows);
        }();
    }
    expect_invalid_module(ctx, module);

    module = valid_enum_module(ctx);
    enumerators.serialized_name[first_item(
        declarations,
        enums.values[first_item(declarations, normal_modules.declarations[module])])] =
        strings.intern("value");

    {
        auto const row_0{first_item(declarations, normal_modules.declarations[module])};

        enums.values[row_0] = [&] {
            auto const original_items{declarations.items(enums.values[row_0])};
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
    }
    {
        auto const row_0{first_item(declarations, normal_modules.declarations[module])};

        enums.conversions[row_0] = [&] {
            auto const original_items{declarations.items(enums.conversions[row_0])};
            auto rows{std::vector(original_items.begin(), original_items.end())};
            rows.push_back(EnumConversion::try_parse_serialized);
            return declarations.range(rows);
        }();
    }
    expect_invalid_module(ctx, module);
}

TEST(Validation, RejectsInvalidEnumArrayDefinitions) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& enums{declarations.enums};
    auto& enumerators{declarations.enumerators};
    auto& strings{ctx.storage.strings};
    auto module{valid_enum_module(ctx)};
    enums.enum_array[first_item(declarations, normal_modules.declarations[module])] = true;

    enumerators.initializer[first_item(
        declarations,
        enums.values[first_item(declarations, normal_modules.declarations[module])])] =
        strings.intern("0");

    EXPECT_NO_THROW(lower_test_manifest(ctx, manifest_with(ctx, module)));

    for (auto const* code : {"2", "0", "-1", "unknown_code", "0x2"}) {
        SCOPED_TRACE(code);
        module = valid_enum_module(ctx);
        enums.enum_array[first_item(declarations, normal_modules.declarations[module])] = true;

        enums.values[first_item(declarations, normal_modules.declarations[module])] =
            declarations.range<lispb::codegen::EnumeratorId>(
                {append_test_row(enumerators,
                                 [&](auto const new_enumerators) {
                                     enumerators.name[new_enumerators] = strings.intern("First");
                                     enumerators.initializer[new_enumerators] = strings.intern("0");
                                 }),
                 append_test_row(enumerators, [&](auto const new_enumerators) {
                     enumerators.name[new_enumerators] = strings.intern("Second");
                     enumerators.initializer[new_enumerators] = strings.intern(code);
                 })});

        expect_invalid_module(ctx, module);
    }

    module = valid_native_enum_module(ctx);
    enums.enum_array[first_item(declarations, normal_modules.declarations[module])] = true;

    enums.values[first_item(declarations, normal_modules.declarations[module])] =
        declarations.range<lispb::codegen::EnumeratorId>(
            {append_test_row(enumerators,
                             [&](auto const new_enumerators) {
                                 enumerators.name[new_enumerators] = strings.intern("First");
                                 enumerators.initializer[new_enumerators] = strings.intern("0x0");
                             }),
             append_test_row(enumerators, [&](auto const new_enumerators) {
                 enumerators.name[new_enumerators] = strings.intern("Second");
                 enumerators.initializer[new_enumerators] = strings.intern("1");
             })});

    EXPECT_NO_THROW(lower_test_manifest(ctx, manifest_with(ctx, module)));

    module = valid_enum_module(ctx);
    enums.enum_array[first_item(declarations, normal_modules.declarations[module])] = true;

    enumerators.hidden[first_item(
        declarations,
        enums.values[first_item(declarations, normal_modules.declarations[module])])] = true;

    expect_invalid_module(ctx, module);

    module = valid_enum_module(ctx);
    enums.count[first_item(declarations, normal_modules.declarations[module])] =
        strings.intern("Value");

    expect_invalid_module(ctx, module);

    module = valid_enum_module(ctx);
    enums.enum_array[first_item(declarations, normal_modules.declarations[module])] = true;

    enums.count[first_item(declarations, normal_modules.declarations[module])] =
        strings.intern("Missing");

    expect_invalid_module(ctx, module);

    module = valid_enum_module(ctx);
    enums.enum_array[first_item(declarations, normal_modules.declarations[module])] = true;

    {
        auto const row_0{first_item(declarations, normal_modules.declarations[module])};

        enums.values[row_0] = [&] {
            auto const original_items{declarations.items(enums.values[row_0])};
            auto rows{std::vector(original_items.begin(), original_items.end())};
            rows.push_back(append_test_row(enumerators, [&](auto const new_enumerators) {
                enumerators.name[new_enumerators] = strings.intern("COUNT");
                enumerators.initializer[new_enumerators] = std::nullopt;
                enumerators.display_name[new_enumerators] = std::nullopt;
                enumerators.hidden[new_enumerators] = true;
            }));
            return declarations.range(rows);
        }();
    }
    {
        auto const row_0{first_item(declarations, normal_modules.declarations[module])};

        enums.values[row_0] = [&] {
            auto const original_items{declarations.items(enums.values[row_0])};
            auto rows{std::vector(original_items.begin(), original_items.end())};
            rows.push_back(append_test_row(enumerators, [&](auto const new_enumerators) {
                enumerators.name[new_enumerators] = strings.intern("After");
            }));
            return declarations.range(rows);
        }();
    }
    enums.count[first_item(declarations, normal_modules.declarations[module])] =
        strings.intern("COUNT");

    expect_invalid_module(ctx, module);

    module = valid_enum_module(ctx);
    enums.enum_array[first_item(declarations, normal_modules.declarations[module])] = true;

    {
        auto const row_0{first_item(declarations, normal_modules.declarations[module])};

        enums.values[row_0] = [&] {
            auto const original_items{declarations.items(enums.values[row_0])};
            auto rows{std::vector(original_items.begin(), original_items.end())};
            rows.push_back(append_test_row(enumerators, [&](auto const new_enumerators) {
                enumerators.name[new_enumerators] = strings.intern("COUNT");
            }));
            return declarations.range(rows);
        }();
    }
    enums.count[first_item(declarations, normal_modules.declarations[module])] =
        strings.intern("COUNT");

    expect_invalid_module(ctx, module);

    module = valid_enum_module(ctx);
    enums.enum_array[first_item(declarations, normal_modules.declarations[module])] = true;

    enums.values[first_item(declarations, normal_modules.declarations[module])] =
        declarations.range<lispb::codegen::EnumeratorId>(
            {append_test_row(enumerators, [&](auto const new_enumerators) {
                enumerators.name[new_enumerators] = strings.intern("COUNT");
                enumerators.initializer[new_enumerators] = std::nullopt;
                enumerators.display_name[new_enumerators] = std::nullopt;
                enumerators.hidden[new_enumerators] = true;
            })});

    enums.count[first_item(declarations, normal_modules.declarations[module])] =
        strings.intern("COUNT");

    expect_invalid_module(ctx, module);
}

TEST(Validation, SupportsSignedEnumValuesWhenTheyFitTheUnderlyingType) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& enums{declarations.enums};
    auto& enumerators{declarations.enumerators};
    auto& strings{ctx.storage.strings};
    auto module{valid_native_enum_module(ctx)};
    enums.underlying_type[first_item(declarations, normal_modules.declarations[module])] =
        make_test_type_ref(ctx, "@native_int8");

    enums.values[first_item(declarations, normal_modules.declarations[module])] =
        declarations.range<lispb::codegen::EnumeratorId>(
            {append_test_row(enumerators,
                             [&](auto const new_enumerators) {
                                 enumerators.name[new_enumerators] = strings.intern("Minimum");
                                 enumerators.initializer[new_enumerators] = strings.intern("-128");
                             }),
             append_test_row(enumerators, [&](auto const new_enumerators) {
                 enumerators.name[new_enumerators] = strings.intern("Maximum");
                 enumerators.initializer[new_enumerators] = strings.intern("127");
             })});

    enums.count[first_item(declarations, normal_modules.declarations[module])].reset();

    auto const types{std::vector<std::pair<std::string_view, CppTypeId>>{
        {"native_int8",
         ctx.storage.cpp_types.add("std::int8_t",
                                   {ctx.storage.cpp_types.dependency("std::int8_t", "cstdint")})},
    }};

    EXPECT_NO_THROW(static_cast<void>(lower_test_manifest(ctx, manifest_with(ctx, module, types))));
    {
        auto const row_2{first_item(declarations, normal_modules.declarations[module])};

        enums.values[row_2] = [&] {
            auto const original_items{declarations.items(enums.values[row_2])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.back() = [&] {
                auto row_0{enumerators.clone(rows_1.back())};
                enumerators.initializer[row_0] = strings.intern("128");
                return row_0;
            }();
            return declarations.range(rows_1);
        }();
    }
    EXPECT_THROW(static_cast<void>(lower_test_manifest(ctx, manifest_with(ctx, module, types))),
                 std::invalid_argument);
}

TEST(Validation, RequiresKnownSemanticFactsWhenEnumBackingIsDerived) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& enums{declarations.enums};
    auto& enumerators{declarations.enumerators};
    auto& strings{ctx.storage.strings};
    auto module{valid_native_enum_module(ctx)};
    {
        auto const row_0{first_item(declarations, normal_modules.declarations[module])};

        enums.values[row_0] = [&] {
            auto const original_items{declarations.items(enums.values[row_0])};
            auto rows{std::vector(original_items.begin(), original_items.end())};
            rows.push_back(append_test_row(enumerators, [&](auto const new_enumerators) {
                enumerators.name[new_enumerators] = strings.intern("Active");
            }));
            return declarations.range(rows);
        }();
    }
    enums.underlying_type[first_item(declarations, normal_modules.declarations[module])].reset();

    enums.bit_width[first_item(declarations, normal_modules.declarations[module])] =
        EnumBitWidth{12};

    enums.signedness[first_item(declarations, normal_modules.declarations[module])] =
        schema::EnumSignedness::unsigned_;

    EXPECT_NO_THROW(static_cast<void>(lower_test_manifest(ctx, manifest_with(ctx, module))));
    enumerators.initializer[first_item(
        declarations,
        enums.values[first_item(declarations, normal_modules.declarations[module])])] =
        strings.intern("calculate_state()");

    {
        auto const row_2{first_item(declarations, normal_modules.declarations[module])};

        enums.values[row_2] = [&] {
            auto const original_items{declarations.items(enums.values[row_2])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.back() = [&] {
                auto row_0{enumerators.clone(rows_1.back())};
                enumerators.initializer[row_0].reset();
                return row_0;
            }();
            return declarations.range(rows_1);
        }();
    }
    enums.bit_width[first_item(declarations, normal_modules.declarations[module])].reset();

    enums.signedness[first_item(declarations, normal_modules.declarations[module])] =
        schema::EnumSignedness::unspecified;

    EXPECT_THROW(static_cast<void>(lower_test_manifest(ctx, manifest_with(ctx, module))),
                 std::invalid_argument);
}

TEST(Validation, RejectsInvalidStaticTableModuleConfiguration) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& static_tables{declarations.static_tables};
    auto module{valid_static_table_module(ctx)};
    normal_modules.declarations[module] = {};
    EXPECT_NO_THROW(lower_test_manifest(ctx, manifest_with(ctx, module)));

    module = valid_static_table_module(ctx);
    declarations.module_settings_store.source[normal_modules.settings[module]] =
        ctx.storage.strings.intern("Tables.cpp");

    EXPECT_NO_THROW(lower_test_manifest(ctx, manifest_with(ctx, module)));

    module = valid_static_table_module(ctx);
    static_tables.rows[first_item(declarations, normal_modules.declarations[module])] = {};

    expect_invalid_module(ctx, module);

    module = valid_static_table_module(ctx);
    static_tables.columns[first_item(declarations, normal_modules.declarations[module])] = {};

    expect_invalid_module(ctx, module);
}

TEST(Validation, RejectsInvalidStaticTableNamesAndTypes) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& normal_modules_schemas{declarations.normal_modules};
    auto& static_tables{declarations.static_tables};
    auto& static_table_rows{declarations.static_table_rows};
    auto& strings{ctx.storage.strings};
    auto module{valid_static_table_module(ctx)};
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
    expect_invalid_module(ctx, module);

    module = valid_static_table_module(ctx);
    {
        auto const row_0{
            first_item(declarations,
                       normal_modules_schemas.declarations[normal_modules_schemas.index(module)])};

        static_tables.rows[row_0] = [&] {
            auto const original_items{declarations.items(static_tables.rows[row_0])};
            auto rows{std::vector(original_items.begin(), original_items.end())};
            rows.push_back(
                append_test_row(static_table_rows, [&](auto const new_static_table_rows) {
                    static_table_rows.name[new_static_table_rows] = strings.intern("first");
                }));
            return declarations.range(rows);
        }();
    }
    expect_invalid_module(ctx, module);

    module = valid_static_table_module(ctx);
    static_table_rows.name[first_item(
        declarations,
        static_tables.rows[first_item(
            declarations,
            normal_modules_schemas.declarations[normal_modules_schemas.index(module)])])] =
        strings.intern("bad-name");

    expect_invalid_module(ctx, module);

    module = valid_static_table_module(ctx);
    {
        auto const row_0{
            first_item(declarations,
                       normal_modules_schemas.declarations[normal_modules_schemas.index(module)])};

        static_tables.columns[row_0] = [&] {
            auto const original_items{declarations.items(static_tables.columns[row_0])};
            auto rows{std::vector(original_items.begin(), original_items.end())};
            rows.push_back(
                declarations
                    .items(static_tables
                               .columns[declarations
                                            .items(normal_modules_schemas.declarations
                                                       [declarations.normal_modules.index(module)])
                                            .front()])
                    .front());
            return declarations.range(rows);
        }();
    }
    expect_invalid_module(ctx, module);

    module = valid_static_table_module(ctx);
    declarations.static_table_columns.type[first_item(
        declarations,
        static_tables.columns[first_item(
            declarations,
            normal_modules_schemas.declarations[normal_modules_schemas.index(module)])])] =
        make_test_type_ref(ctx, "@missing");

    expect_invalid_module(ctx, module);
}

struct InvalidSchemaTextCase {
    char const* name;
    std::string_view value;
};

using StaticTableColumnCollision = testing::TestWithParam<InvalidSchemaTextCase>;

TEST_P(StaticTableColumnCollision, RejectsGeneratedApiNames) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto const module{valid_static_table_module(ctx)};
    auto const table{declarations.items(declarations.normal_modules.declarations[module]).front()};
    auto const column{declarations.items(declarations.static_tables.columns[table]).front()};
    declarations.static_table_columns.name[column] = ctx.storage.strings.intern(GetParam().value);

    expect_invalid_module(ctx, module);
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
    struct Case {
        char const* name;
        char const* type;
        std::vector<std::string_view> columns;
        bool duplicate;
        char const* diagnostic;
    };
    for (auto const& test :
         {Case{"point", "FPoint", {"ids"}, true, "Duplicate static table group name"},
          Case{"bad-name", "FPoint", {"ids"}, false, "group name"},
          Case{"point", "@missing", {"ids"}, false, "missing"},
          Case{"point", "FPoint", {}, false, "must have columns"},
          Case{"point", "FPoint", {"missing"}, false, "references unknown column"},
          Case{"point", "FPoint", {"ids", "ids"}, false, "columns contains duplicate name"}}) {
        SCOPED_TRACE(test.diagnostic);
        CompilerTestContext ctx;
        auto const module{valid_static_table_module(ctx)};
        auto& declarations{ctx.storage.declarations};
        auto& strings{ctx.storage.strings};
        auto const table{
            first_item(declarations, declarations.normal_modules.declarations[module])};
        auto const group{declarations.static_table_groups.append()};
        auto& groups{declarations.static_table_groups};
        groups.name[group] = strings.intern(test.name);
        groups.type[group] = make_test_type_ref(ctx, test.type);
        std::vector<StringId> columns;
        for (auto const column : test.columns) {
            columns.push_back(strings.intern(column));
        }
        groups.columns[group] = declarations.range(columns);
        declarations.static_tables.groups[table] =
            test.duplicate ? declarations.range<StaticTableGroupId>({group, group})
                           : declarations.range<StaticTableGroupId>({group});
        expect_invalid_module(ctx, module, test.diagnostic);
    }
}

TEST(Validation, RejectsStaticTableGroupGetterCollisions) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& static_tables{declarations.static_tables};
    auto& static_table_columns{declarations.static_table_columns};

    auto& static_table_groups{declarations.static_table_groups};
    auto& strings{ctx.storage.strings};
    auto module{valid_static_table_module(ctx)};
    {
        auto const row_0{first_item(declarations, normal_modules.declarations[module])};

        static_tables.columns[row_0] = [&] {
            auto const original_items{declarations.items(static_tables.columns[row_0])};
            auto rows{std::vector(original_items.begin(), original_items.end())};
            rows.push_back(
                append_test_row(static_table_columns, [&](auto const new_static_table_columns) {
                    static_table_columns.name[new_static_table_columns] =
                        strings.intern("get_point");
                    static_table_columns.type[new_static_table_columns] =
                        make_test_type_ref(ctx, "float");
                }));
            return declarations.range(rows);
        }();
    }
    static_tables.groups[first_item(declarations, normal_modules.declarations[module])] =
        declarations.range<lispb::codegen::StaticTableGroupId>(
            {append_test_row(static_table_groups, [&](auto const new_static_table_groups) {
                static_table_groups.name[new_static_table_groups] = strings.intern("point");
                static_table_groups.type[new_static_table_groups] =
                    make_test_type_ref(ctx, "FPoint");
                static_table_groups.columns[new_static_table_groups] =
                    declarations.range<lispb::StringId>({strings.intern("ids")});
            })});

    expect_invalid_module(ctx, module);
}

TEST(Validation, RejectsEmptyModuleOutputs) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& settings{declarations.module_settings_store};
    auto const module{valid_soa_module(ctx)};
    auto const row{settings.index(normal_modules.settings[module])};
    settings.header[row] = {};

    expect_invalid_module(ctx, module);
}

struct EmptyModuleSettingCase {
    char const* name;
    decltype(&ModuleSettingsStore::header_include) column;
};

using EmptyModuleSetting = testing::TestWithParam<EmptyModuleSettingCase>;

TEST_P(EmptyModuleSetting, RejectsPresentButEmptyValues) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& settings{declarations.module_settings_store};
    auto const module{valid_soa_module(ctx)};
    auto const row{settings.index(declarations.normal_modules.settings[module])};
    (settings.*GetParam().column)[row] = ctx.storage.strings.intern("");

    expect_invalid_module(ctx, module);
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
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto const module{valid_soa_module(ctx)};
    auto const settings{declarations.normal_modules.settings[module]};
    std::vector<StringId> prefixes;
    for (auto const prefix : GetParam().prefixes) {
        prefixes.push_back(ctx.storage.strings.intern(prefix));
    }
    declarations.module_settings_store.include_order[settings] = declarations.range(prefixes);

    expect_invalid_module(ctx, module);
}

INSTANTIATE_TEST_SUITE_P(
    Validation,
    InvalidIncludeOrder,
    testing::Values(InvalidIncludeOrderCase{"Duplicate", {"Project/", "Project/"}},
                    InvalidIncludeOrderCase{"Empty", {""}}),
    [](testing::TestParamInfo<InvalidIncludeOrderCase> const& info) { return info.param.name; });

TEST(Validation, RejectsMalformedQualifiedNamespaces) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& settings{declarations.module_settings_store};
    auto const module{valid_soa_module(ctx)};
    auto const row{settings.index(normal_modules.settings[module])};
    settings.namespace_name[row] = ctx.storage.strings.intern("project:::invalid");

    expect_invalid_module(ctx, module);
}

TEST(Validation, RejectsDuplicateModuleNames) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& settings{declarations.module_settings_store};
    auto& strings{ctx.storage.strings};
    auto const first{valid_soa_module(ctx)};
    auto const second{valid_soa_module(ctx)};
    auto const row{settings.index(normal_modules.settings[second])};
    settings.header[row] = strings.intern("Other.h");
    settings.source[row] = strings.intern("Other.cpp");
    lispb::codegen::Manifest const manifest{
        manifest_schema_version, make_test_registry(ctx), declarations, {first, second}};

    EXPECT_THROW(lower_test_manifest(ctx, manifest), std::invalid_argument);
}

TEST(Validation, RejectsUnsupportedProgrammaticSchemaVersions) {
    CompilerTestContext ctx;
    auto manifest{manifest_with(ctx, valid_soa_module(ctx))};
    manifest.schema_version = manifest_schema_version + 1;

    EXPECT_THROW(lower_test_manifest(ctx, manifest), std::invalid_argument);
}

TEST(Validation, AllowsManifestsWithoutModules) {
    CompilerTestContext ctx;
    lispb::codegen::Manifest const manifest{
        manifest_schema_version, make_test_registry(ctx), ctx.storage.declarations, {}};

    EXPECT_NO_THROW(lower_test_manifest(ctx, manifest));
}

TEST(Validation, RejectsKeywordsInQualifiedNamespaces) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto const module{valid_soa_module(ctx)};
    auto const settings{declarations.normal_modules.settings[module]};
    declarations.module_settings_store.namespace_name[settings] =
        ctx.storage.strings.intern("project::namespace");

    expect_invalid_module(ctx, module);
}

TEST(Validation, RejectsEmptyTypeSpellings) {
    CompilerTestContext ctx;
    auto module{valid_soa_module(ctx)};
    EXPECT_THROW(lower_test_manifest(
                     ctx, manifest_with(ctx, module, {{"empty", ctx.storage.cpp_types.add("")}})),
                 std::invalid_argument);
}

TEST(Validation, RejectsInvalidTypeDependencyAndOperationSpellings) {
    CompilerTestContext ctx;
    auto empty_header{
        ctx.storage.cpp_types.add("FValue", {ctx.storage.cpp_types.dependency("FValue", "")})};
    EXPECT_THROW(lower_test_manifest(
                     ctx, manifest_with(ctx, valid_soa_module(ctx), {{"value", empty_header}})),
                 std::invalid_argument);

    auto const invalid_operation{ctx.storage.cpp_types.add("FValue")};
    ctx.storage.cpp_types.set_operation(
        invalid_operation, TypeOperation::remove_at_swap, "bad-name");
    EXPECT_THROW(
        lower_test_manifest(
            ctx, manifest_with(ctx, valid_soa_module(ctx), {{"value", invalid_operation}})),
        std::invalid_argument);
}

TEST(Validation, RejectsEmptySoaNames) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto module{valid_soa_module(ctx)};
    declarations.soas.name[first_item(declarations, normal_modules.declarations[module])] = {};

    expect_invalid_module(ctx, module);
}

TEST(Validation, RejectsDuplicateSoaMembers) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& soas{declarations.soas};
    auto& soa_members{declarations.soa_members};
    auto& strings{ctx.storage.strings};
    auto module{valid_soa_module(ctx)};
    {
        auto const row_0{first_item(declarations, normal_modules.declarations[module])};

        soas.members[row_0] = [&] {
            auto const original_items{declarations.items(soas.members[row_0])};
            auto rows{std::vector(original_items.begin(), original_items.end())};
            rows.push_back(append_test_row(soa_members, [&](auto const new_soa_members) {
                soa_members.name[new_soa_members] = strings.intern("values");
                soa_members.kind[new_soa_members] = SoaMemberKind::array;
                soa_members.type[new_soa_members] = make_test_type_ref(ctx, "float");
            }));
            return declarations.range(rows);
        }();
    }

    expect_invalid_module(ctx, module);
}

TEST(Validation, RejectsMalformedSoaMemberIdentifiers) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& soas{declarations.soas};
    auto module{valid_soa_module(ctx)};
    declarations.soa_members.name[first_item(
        declarations,
        soas.members[first_item(declarations, normal_modules.declarations[module])])] =
        ctx.storage.strings.intern("bad-name");

    expect_invalid_module(ctx, module);
}

TEST(Validation, RejectsIncompleteSoaFieldMaskDeclarations) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& soas{declarations.soas};
    auto module{valid_mask_soa_module(ctx)};
    soas.field_enum_name[first_item(declarations, normal_modules.declarations[module])].reset();

    expect_invalid_module(ctx, module);

    module = valid_mask_soa_module(ctx);
    soas.field_mask_name[first_item(declarations, normal_modules.declarations[module])].reset();

    expect_invalid_module(ctx, module);
}

TEST(Validation, RejectsInvalidSoaFieldMaskMembers) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& soas{declarations.soas};
    auto& soa_members{declarations.soa_members};
    auto module{valid_mask_soa_module(ctx)};
    soa_members.type[first_item(
        declarations,
        soas.members[first_item(declarations, normal_modules.declarations[module])])] =
        make_test_type_ref(ctx, "uint8");

    expect_invalid_module(ctx, module);

    module = valid_mask_soa_module(ctx);
    {
        auto const row_2{first_item(declarations, normal_modules.declarations[module])};

        soas.members[row_2] = [&] {
            auto const original_items{declarations.items(soas.members[row_2])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.back() = [&] {
                auto row_0{soa_members.clone(rows_1.back())};
                soa_members.mask_field[row_0] = false;
                return row_0;
            }();
            return declarations.range(rows_1);
        }();
    }
    expect_invalid_module(ctx, module);

    module = valid_soa_module(ctx);
    soa_members.mask_field[first_item(
        declarations,
        soas.members[first_item(declarations, normal_modules.declarations[module])])] = true;

    expect_invalid_module(ctx, module);
}

TEST(Validation, RejectsInvalidSoaMaskDimensions) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& soas{declarations.soas};
    auto& soa_members{declarations.soa_members};
    auto& soa_mask_dimensions{declarations.soa_mask_dimensions};
    auto& strings{ctx.storage.strings};
    auto module{valid_mask_soa_module(ctx)};
    {
        auto const row_2{first_item(declarations, normal_modules.declarations[module])};

        soas.members[row_2] = [&] {
            auto const original_items{declarations.items(soas.members[row_2])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.back() = [&] {
                auto row_0{soa_members.clone(rows_1.back())};

                soa_members.mask_dimensions[row_0] = [&] {
                    auto const original_items{
                        declarations.items(soa_members.mask_dimensions[row_0])};
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
    }
    {
        auto const row_2{first_item(declarations, normal_modules.declarations[module])};

        soas.members[row_2] = [&] {
            auto const original_items{declarations.items(soas.members[row_2])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.back() = [&] {
                auto row_0{soa_members.clone(rows_1.back())};

                soa_members.mask_dimensions[row_0] = [&] {
                    auto const original_items{
                        declarations.items(soa_members.mask_dimensions[row_0])};
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
    }
    expect_invalid_module(ctx, module);

    module = valid_mask_soa_module(ctx);
    {
        auto const row_2{first_item(declarations, normal_modules.declarations[module])};

        soas.members[row_2] = [&] {
            auto const original_items{declarations.items(soas.members[row_2])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.back() = [&] {
                auto row_0{soa_members.clone(rows_1.back())};

                soa_members.mask_dimensions[row_0] = [&] {
                    auto const original_items{
                        declarations.items(soa_members.mask_dimensions[row_0])};
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
    }
    expect_invalid_module(ctx, module);
}

using InvalidSoaMemberName = testing::TestWithParam<InvalidSchemaTextCase>;

TEST_P(InvalidSoaMemberName, RejectsReservedNamesAndGeneratedApiCollisions) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto const module{valid_soa_module(ctx)};
    auto const soa{declarations.items(declarations.normal_modules.declarations[module]).front()};
    auto const member{declarations.items(declarations.soas.members[soa]).front()};
    declarations.soa_members.name[member] = ctx.storage.strings.intern(GetParam().value);

    expect_invalid_module(ctx, module);
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
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& soas{declarations.soas};
    auto module{valid_soa_module(ctx)};
    declarations.soa_members.type[first_item(
        declarations,
        soas.members[first_item(declarations, normal_modules.declarations[module])])] =
        make_test_type_ref(ctx, "@missing");

    expect_invalid_module(ctx, module);
}

TEST(Validation, RejectsCollidingGeneratedSoaTypeNames) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto module{valid_soa_module(ctx)};
    auto second{
        declarations.soas.clone(declarations.items(normal_modules.declarations[module]).front())};
    declarations.soas.name[second] = ctx.storage.strings.intern("F_Data");
    normal_modules.declarations[module] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module])};
        auto rows{std::vector(original_items.begin(), original_items.end())};
        rows.push_back(second);
        return declarations.range(rows);
    }();

    expect_invalid_module(ctx, module);
}

TEST(Validation, RejectsReservedSoaNamespaceNames) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto module{valid_soa_module(ctx)};
    declarations.soas.name[first_item(declarations, normal_modules.declarations[module])] =
        ctx.storage.strings.intern("Class");

    expect_invalid_module(ctx, module);
}

using BlankSoaUsingDeclaration = testing::TestWithParam<InvalidSchemaTextCase>;

TEST_P(BlankSoaUsingDeclaration, RejectsBlankDeclarations) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto const module{valid_soa_module(ctx)};
    auto const soa{declarations.items(declarations.normal_modules.declarations[module]).front()};
    declarations.soas.using_declarations[soa] =
        declarations.range<StringId>({ctx.storage.strings.intern(GetParam().value)});

    expect_invalid_module(ctx, module);
}

INSTANTIATE_TEST_SUITE_P(Validation,
                         BlankSoaUsingDeclaration,
                         testing::Values(InvalidSchemaTextCase{"Empty", ""},
                                         InvalidSchemaTextCase{"Whitespace", " \t\r\n"}),
                         [](testing::TestParamInfo<InvalidSchemaTextCase> const& info) {
                             return info.param.name;
                         });

TEST(Validation, RejectsDuplicateCustomSoaFunctionParameters) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& functions_schemas{declarations.functions};

    auto& parameters_schemas{declarations.parameters};
    auto& strings{ctx.storage.strings};
    auto module{valid_soa_module(ctx)};
    declarations.soas.functions[first_item(declarations, normal_modules.declarations[module])] =
        declarations.range<lispb::codegen::FunctionSchemaId>(
            {append_test_row(functions_schemas, [&](auto const new_functions) {
                functions_schemas.name[new_functions] = strings.intern("set");
                functions_schemas.return_type[new_functions] = make_test_type_ref(ctx, "void");
                functions_schemas.parameters[new_functions] =
                    declarations.range<lispb::codegen::ParameterId>(
                        {parameters_schemas.append(make_test_type_ref(ctx, "int32"),
                                                   strings.intern("value"),
                                                   std::nullopt,
                                                   false),
                         parameters_schemas.append(make_test_type_ref(ctx, "float"),
                                                   strings.intern("value"),
                                                   std::nullopt,
                                                   false)});
            })});

    expect_invalid_module(ctx, module);
}

TEST(Validation, RejectsNonTrailingCustomSoaDefaultArguments) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& soas{declarations.soas};
    auto& functions_schemas{declarations.functions};

    auto& parameters_schemas{declarations.parameters};
    auto& strings{ctx.storage.strings};
    auto module{valid_soa_module(ctx)};
    soas.functions[first_item(declarations, normal_modules.declarations[module])] =
        declarations.range<lispb::codegen::FunctionSchemaId>(
            {append_test_row(functions_schemas, [&](auto const new_functions) {
                functions_schemas.name[new_functions] = strings.intern("set");
                functions_schemas.return_type[new_functions] = make_test_type_ref(ctx, "void");
                functions_schemas.parameters[new_functions] =
                    declarations.range<lispb::codegen::ParameterId>(
                        {parameters_schemas.append(make_test_type_ref(ctx, "int32"),
                                                   strings.intern("first"),
                                                   strings.intern("0"),
                                                   false),
                         parameters_schemas.append(make_test_type_ref(ctx, "int32"),
                                                   strings.intern("second"),
                                                   std::nullopt,
                                                   false)});
            })});

    expect_invalid_module(ctx, module);

    module = valid_soa_module(ctx);
    soas.functions[first_item(declarations, normal_modules.declarations[module])] =
        declarations.range<lispb::codegen::FunctionSchemaId>(
            {append_test_row(functions_schemas, [&](auto const new_functions) {
                functions_schemas.name[new_functions] = strings.intern("set");
                functions_schemas.return_type[new_functions] = make_test_type_ref(ctx, "void");
                functions_schemas.parameters[new_functions] =
                    declarations.range<lispb::codegen::ParameterId>(
                        {parameters_schemas.append(make_test_type_ref(ctx, "int32"),
                                                   strings.intern("value"),
                                                   strings.intern(" \t"),
                                                   false)});
            })});

    expect_invalid_module(ctx, module);
}

TEST(Validation, RejectsUnknownCustomSoaDependencies) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& soas{declarations.soas};
    auto& functions_schemas{declarations.functions};

    auto& strings{ctx.storage.strings};
    auto module{valid_soa_module(ctx)};
    soas.functions[first_item(declarations, normal_modules.declarations[module])] =
        declarations.range<lispb::codegen::FunctionSchemaId>(
            {append_test_row(functions_schemas, [&](auto const new_functions) {
                functions_schemas.name[new_functions] = strings.intern("reset_values");
                functions_schemas.return_type[new_functions] = make_test_type_ref(ctx, "void");
                functions_schemas.dependencies[new_functions] =
                    declarations.range<lispb::StringId>({strings.intern("missing")});
            })});

    expect_invalid_module(ctx, module);

    module = valid_soa_module(ctx);
    soas.functions[first_item(declarations, normal_modules.declarations[module])] =
        declarations.range<lispb::codegen::FunctionSchemaId>(
            {append_test_row(functions_schemas, [&](auto const new_functions) {
                functions_schemas.name[new_functions] = strings.intern("reset_values");
                functions_schemas.return_type[new_functions] = make_test_type_ref(ctx, "void");
                functions_schemas.dependencies[new_functions] =
                    declarations.range<lispb::StringId>({strings.intern(" \t")});
            })});

    expect_invalid_module(ctx, module);
}

TEST(Validation, RejectsInvalidCustomSoaFunctionDefinitions) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& soas{declarations.soas};
    auto& functions_schemas{declarations.functions};

    auto& strings{ctx.storage.strings};
    auto module{valid_soa_module(ctx)};
    soas.functions[first_item(declarations, normal_modules.declarations[module])] =
        declarations.range<lispb::codegen::FunctionSchemaId>(
            {append_test_row(functions_schemas, [&](auto const new_functions) {
                functions_schemas.name[new_functions] = strings.intern("class");
                functions_schemas.return_type[new_functions] = make_test_type_ref(ctx, "void");
                functions_schemas.is_inline[new_functions] = true;
            })});

    expect_invalid_module(ctx, module);

    module = valid_soa_module(ctx);
    soas.functions[first_item(declarations, normal_modules.declarations[module])] =
        declarations.range<lispb::codegen::FunctionSchemaId>(
            {append_test_row(functions_schemas, [&](auto const new_functions) {
                functions_schemas.name[new_functions] = strings.intern("update");
                functions_schemas.return_type[new_functions] = make_test_type_ref(ctx, "void");
                functions_schemas.is_inline[new_functions] = true;
                functions_schemas.definition_in_source[new_functions] = true;
            })});

    expect_invalid_module(ctx, module);

    module = valid_soa_module(ctx);
    soas.functions[first_item(declarations, normal_modules.declarations[module])] =
        declarations.range<lispb::codegen::FunctionSchemaId>(
            {append_test_row(functions_schemas, [&](auto const new_functions) {
                functions_schemas.name[new_functions] = strings.intern("invalid_static_const");
                functions_schemas.return_type[new_functions] = make_test_type_ref(ctx, "void");
                functions_schemas.is_const[new_functions] = true;
                functions_schemas.is_static[new_functions] = true;
            })});

    expect_invalid_module(ctx, module);

    module = valid_soa_module(ctx);
    soas.functions[first_item(declarations, normal_modules.declarations[module])] =
        declarations.range<lispb::codegen::FunctionSchemaId>(
            {append_test_row(functions_schemas, [&](auto const new_functions) {
                functions_schemas.name[new_functions] = strings.intern("invalid_trailing_return");
                functions_schemas.return_type[new_functions] = make_test_type_ref(ctx, "void");
                functions_schemas.trailing_return_type[new_functions] =
                    make_test_type_ref(ctx, "int32");
            })});

    expect_invalid_module(ctx, module);

    module = valid_soa_module(ctx);
    soas.functions[first_item(declarations, normal_modules.declarations[module])] =
        declarations.range<lispb::codegen::FunctionSchemaId>(
            {append_test_row(functions_schemas, [&](auto const new_functions) {
                functions_schemas.name[new_functions] = strings.intern("invalid_template");
                functions_schemas.return_type[new_functions] = make_test_type_ref(ctx, "void");
                functions_schemas.template_parameters[new_functions] = strings.intern(" \t");
            })});

    expect_invalid_module(ctx, module);

    module = valid_soa_module(ctx);
    soas.functions[first_item(declarations, normal_modules.declarations[module])] =
        declarations.range<lispb::codegen::FunctionSchemaId>(
            {append_test_row(functions_schemas, [&](auto const new_functions) {
                functions_schemas.name[new_functions] = strings.intern("invalid_requires");
                functions_schemas.return_type[new_functions] = make_test_type_ref(ctx, "void");
                functions_schemas.requires_clause[new_functions] = strings.intern(" \t");
            })});

    expect_invalid_module(ctx, module);
}

using SoaFunctionNameCollision = testing::TestWithParam<InvalidSchemaTextCase>;

TEST_P(SoaFunctionNameCollision, RejectsGeneratedApiAndTypeNames) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto const module{valid_soa_module(ctx)};
    auto const soa{declarations.items(declarations.normal_modules.declarations[module]).front()};
    auto& functions{declarations.functions};
    auto const function{functions.append()};
    functions.name[function] = ctx.storage.strings.intern(GetParam().value);
    functions.return_type[function] = make_test_type_ref(ctx, "void");
    functions.is_inline[function] = true;
    declarations.soas.functions[soa] = declarations.range<FunctionSchemaId>({function});

    expect_invalid_module(ctx, module);
}

INSTANTIATE_TEST_SUITE_P(Validation,
                         SoaFunctionNameCollision,
                         testing::Values(InvalidSchemaTextCase{"Sort", "sort"},
                                         InvalidSchemaTextCase{"GetView", "view"},
                                         InvalidSchemaTextCase{"TypeName", "FData"}),
                         [](testing::TestParamInfo<InvalidSchemaTextCase> const& info) {
                             return info.param.name;
                         });

TEST(Validation, RejectsMalformedExportSpecifiers) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& strings{ctx.storage.strings};
    auto soa{valid_soa_module(ctx)};
    declarations.soas.export_specifier[first_item(declarations, normal_modules.declarations[soa])] =
        strings.intern("bad-specifier");

    EXPECT_THROW(lower_test_manifest(ctx, manifest_with(ctx, soa)), std::invalid_argument);

    auto facade{valid_facade_module(ctx)};
    declarations.facades
        .export_specifier[first_item(declarations, normal_modules.declarations[facade])] =
        strings.intern("two words");

    EXPECT_THROW(lower_test_manifest(ctx, manifest_with(ctx, facade)), std::invalid_argument);

    auto table{valid_static_table_module(ctx)};
    declarations.static_tables
        .export_specifier[first_item(declarations, normal_modules.declarations[table])] =
        strings.intern("bad-specifier");

    EXPECT_THROW(lower_test_manifest(ctx, manifest_with(ctx, table)), std::invalid_argument);
}

TEST(Validation, RejectsFacadesWithoutMethods) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto module{valid_facade_module(ctx)};
    declarations.facades
        .methods[first_item(declarations, normal_modules.declarations[module])] = {};

    expect_invalid_module(ctx, module);
}

TEST(Validation, RejectsUnknownFacadeAccess) {
    CompilerTestContext ctx;
    EXPECT_THROW(parse_test_manifest(ctx, R"((module facade :header "Facade.h"
      (facade FFacade FTarget target :method-access invalid (method reset void))))"),
                 lispb::codegen::sexpr::SourceError);
}

TEST(Validation, RejectsMalformedFacadeMethodIdentifiers) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& facades{declarations.facades};
    auto module{valid_facade_module(ctx)};
    declarations.facade_methods.name[first_item(
        declarations,
        facades.methods[first_item(declarations, normal_modules.declarations[module])])] =
        ctx.storage.strings.intern("bad-name");

    expect_invalid_module(ctx, module);
}

TEST(Validation, RejectsOutOfLineFacadesWithoutSourceOutput) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto module{valid_facade_module(ctx)};
    declarations.facades
        .definitions_in_source[first_item(declarations, normal_modules.declarations[module])] =
        true;

    expect_invalid_module(ctx, module);
}

TEST(Validation, AllowsInlineFacadesInModulesWithSourceOutput) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto module{valid_facade_module(ctx)};
    declarations.module_settings_store.source[normal_modules.settings[module]] =
        ctx.storage.strings.intern("Facade.cpp");

    EXPECT_NO_THROW(lower_test_manifest(ctx, manifest_with(ctx, module)));
}

TEST(Validation, RejectsDuplicateFacadeMethodParameters) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& facades{declarations.facades};
    auto& parameters_schemas{declarations.parameters};

    auto& strings{ctx.storage.strings};
    auto module{valid_facade_module(ctx)};
    declarations.facade_methods.parameters[first_item(
        declarations,
        facades.methods[first_item(declarations, normal_modules.declarations[module])])] =
        declarations.range<lispb::codegen::ParameterId>(
            {parameters_schemas.append(
                 make_test_type_ref(ctx, "int32"), strings.intern("value"), std::nullopt, false),
             parameters_schemas.append(
                 make_test_type_ref(ctx, "float"), strings.intern("value"), std::nullopt, false)});

    expect_invalid_module(ctx, module);
}

TEST(Validation, RejectsDuplicateFacadeMethodSignatures) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& normal_modules_schemas{declarations.normal_modules};
    auto& facades{declarations.facades};
    auto module{valid_facade_module(ctx)};
    {
        auto const row_0{
            first_item(declarations,
                       normal_modules_schemas.declarations[normal_modules_schemas.index(module)])};

        facades.methods[row_0] = [&] {
            auto const original_items{declarations.items(facades.methods[row_0])};
            auto rows{std::vector(original_items.begin(), original_items.end())};
            rows.push_back(
                declarations
                    .items(
                        facades.methods[declarations
                                            .items(normal_modules_schemas.declarations
                                                       [declarations.normal_modules.index(module)])
                                            .front()])
                    .front());
            return declarations.range(rows);
        }();
    }

    expect_invalid_module(ctx, module);
}

TEST(Validation, RejectsFacadeOverloadsDifferingOnlyByNoexcept) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& facades{declarations.facades};
    auto& normal_modules{declarations.normal_modules};
    auto module{valid_facade_module(ctx)};
    auto second{declarations.facade_methods.clone(
        declarations
            .items(facades.methods[declarations.items(normal_modules.declarations[module]).front()])
            .front())};
    declarations.facade_methods.is_noexcept[second] = true;
    {
        auto const row_0{first_item(declarations, normal_modules.declarations[module])};

        facades.methods[row_0] = [&] {
            auto const original_items{declarations.items(facades.methods[row_0])};
            auto rows{std::vector(original_items.begin(), original_items.end())};
            rows.push_back(second);
            return declarations.range(rows);
        }();
    }

    expect_invalid_module(ctx, module);
}

TEST(Validation, RejectsFacadeMethodsCollidingWithBind) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& facade_methods{declarations.facade_methods};

    auto& parameters_schemas{declarations.parameters};
    auto& strings{ctx.storage.strings};
    auto module{valid_facade_module(ctx)};
    declarations.facades.methods[first_item(declarations, normal_modules.declarations[module])] =
        declarations.range<lispb::codegen::FacadeMethodId>(
            {append_test_row(facade_methods, [&](auto const new_facade_methods) {
                facade_methods.name[new_facade_methods] = strings.intern("bind");
                facade_methods.return_type[new_facade_methods] = make_test_type_ref(ctx, "void");
                facade_methods.parameters[new_facade_methods] =
                    declarations.range<lispb::codegen::ParameterId>(
                        {parameters_schemas.append(make_test_type_ref(ctx, "FTarget&"),
                                                   strings.intern("target"),
                                                   std::nullopt,
                                                   false)});
            })});

    expect_invalid_module(ctx, module);
}

TEST(Validation, RejectsNonTrailingFacadeDefaultArguments) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& facades{declarations.facades};
    auto& parameters_schemas{declarations.parameters};

    auto& strings{ctx.storage.strings};
    auto module{valid_facade_module(ctx)};
    declarations.facade_methods.parameters[first_item(
        declarations,
        facades.methods[first_item(declarations, normal_modules.declarations[module])])] =
        declarations.range<lispb::codegen::ParameterId>(
            {parameters_schemas.append(make_test_type_ref(ctx, "int32"),
                                       strings.intern("first"),
                                       strings.intern("0"),
                                       false),
             parameters_schemas.append(
                 make_test_type_ref(ctx, "int32"), strings.intern("second"), std::nullopt, false)});

    expect_invalid_module(ctx, module);
}

TEST(Validation, RejectsUnknownFacadeValidationDependencies) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto module{valid_facade_module(ctx)};
    declarations.facades
        .validation_dependencies[first_item(declarations, normal_modules.declarations[module])] =
        declarations.range<lispb::StringId>({ctx.storage.strings.intern("missing")});

    expect_invalid_module(ctx, module);
}

TEST(Validation, RejectsInvalidFacadeFriendAndMemberCollisions) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& normal_modules_schemas{declarations.normal_modules};
    auto& facades{declarations.facades};
    auto& strings{ctx.storage.strings};
    auto module{valid_facade_module(ctx)};
    EXPECT_THROW(parse_test_manifest(ctx, R"((module facade :header "Facade.h"
      (facade FFacade :target FTarget :friend-kind union (method reset void))))"),
                 lispb::codegen::sexpr::SourceError);

    module = valid_facade_module(ctx);
    facades.friends[first_item(
        declarations, normal_modules_schemas.declarations[normal_modules_schemas.index(module)])] =
        declarations.range<lispb::StringId>({strings.intern("class")});

    expect_invalid_module(ctx, module);

    module = valid_facade_module(ctx);
    facades.target_member_name[first_item(
        declarations, normal_modules_schemas.declarations[normal_modules_schemas.index(module)])] =
        strings.intern("bind");

    expect_invalid_module(ctx, module);

    module = valid_facade_module(ctx);
    declarations.facade_methods.name[first_item(
        declarations,
        facades.methods[first_item(
            declarations,
            normal_modules_schemas.declarations[normal_modules_schemas.index(module)])])] =
        facades.target_member_name[declarations
                                       .items(normal_modules_schemas.declarations
                                                  [declarations.normal_modules.index(module)])
                                       .front()];

    expect_invalid_module(ctx, module);
}

TEST(Validation, RejectsDuplicateUmbrellaHeaders) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& umbrella_modules{declarations.umbrella_modules};
    auto& module_settings_store{declarations.module_settings_store};
    auto& strings{ctx.storage.strings};
    auto const module{umbrella_modules.append()};
    auto const module_row{umbrella_modules.index(module)};
    umbrella_modules.settings[module_row] =
        append_test_row(module_settings_store, [&](auto const new_module_settings_store) {
            module_settings_store.name[new_module_settings_store] = strings.intern("all");
            module_settings_store.header[new_module_settings_store] = strings.intern("All.h");
        });
    umbrella_modules.headers[module_row] =
        declarations.range<lispb::StringId>({strings.intern("Value.h"), strings.intern("Value.h")});

    expect_invalid_module(ctx, module);
}

TEST(Validation, RejectsUmbrellaModulesWithoutHeaders) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& module_settings_store{declarations.module_settings_store};
    auto& strings{ctx.storage.strings};
    auto const module{declarations.umbrella_modules.append()};
    declarations.umbrella_modules.settings[module] =
        append_test_row(module_settings_store, [&](auto const new_module_settings_store) {
            module_settings_store.name[new_module_settings_store] = strings.intern("all");
            module_settings_store.header[new_module_settings_store] = strings.intern("All.h");
        });

    expect_invalid_module(ctx, module);
}

TEST(Validation, RejectsUnsupportedUmbrellaSourceAndNamespaceSettings) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& umbrella_modules{declarations.umbrella_modules};
    auto& module_settings_store{declarations.module_settings_store};
    auto& strings{ctx.storage.strings};
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
    expect_invalid_module(ctx, module);

    module_settings_store.source[umbrella_modules.settings[module_row]].reset();

    module_settings_store.namespace_name[umbrella_modules.settings[module_row]] =
        strings.intern("project");

    expect_invalid_module(ctx, module);
}

TEST(Validation, RejectsUnknownSettingsCategory) {
    CompilerTestContext ctx;
    auto const manifest{parse_test_manifest(ctx, R"(
      (settings-module settings :header "Settings.h" :source "Settings.cpp"
        :api-name TSettingsAccess :state-name FSettingsState
        (category video "Video")
        (setting vsync "VSync" :category missing :value-type bool :backend engine
          :apply deferred (control toggle)))
    )")};
    expect_invalid_module(ctx, manifest.modules.front(), "unknown category");
}

TEST(Validation, NormalModuleReservesGeneratedNamesAcrossDeclarationKinds) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& enums{declarations.enums};

    auto& enumerators{declarations.enumerators};
    auto& records{declarations.records};
    auto& record_members{declarations.child_occurrences};
    auto& soas{declarations.soas};
    auto& soa_members{declarations.soa_members};
    auto& strings{ctx.storage.strings};
    auto const vector{plain_soa(ctx, "Foo")};
    auto module = normal_module(
        ctx,
        {vector,
         append_test_row(enums,
                         [&](auto const new_enums) {
                             enums.cpp_projection[new_enums] = CppProjection{};
                             enums.name[new_enums] = strings.intern("foo");
                             enums.underlying_type[new_enums] = make_test_type_ref(ctx, "uint8");
                             enums.values[new_enums] =
                                 declarations.range<lispb::codegen::EnumeratorId>(
                                     {append_test_row(enumerators, [&](auto const new_enumerators) {
                                         enumerators.name[new_enumerators] = strings.intern("One");
                                     })});
                         })},
        "Mixed.cpp");
    expect_invalid_module(ctx, module);

    module = normal_module(
        ctx,
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
                                 make_test_type_ref(ctx, "int32");
                         })});
             })},
        "Mixed.cpp");
    expect_invalid_module(ctx, module);

    auto soa = append_test_row(soas, [&](auto const new_soas) {
        soas.name[new_soas] = strings.intern("Data");
        soas.members[new_soas] = declarations.range<lispb::codegen::SoaMemberId>(
            {append_test_row(soa_members, [&](auto const new_soa_members) {
                soa_members.name[new_soa_members] = strings.intern("values");
                soa_members.kind[new_soa_members] = SoaMemberKind::array;
                soa_members.type[new_soa_members] = make_test_type_ref(ctx, "int32");
            })});
    });
    module = normal_module(
        ctx,
        {soa,
         append_test_row(enums,
                         [&](auto const new_enums) {
                             enums.cpp_projection[new_enums] = CppProjection{};
                             enums.name[new_enums] = strings.intern("data");
                             enums.underlying_type[new_enums] = make_test_type_ref(ctx, "uint8");
                             enums.values[new_enums] =
                                 declarations.range<lispb::codegen::EnumeratorId>(
                                     {append_test_row(enumerators, [&](auto const new_enumerators) {
                                         enumerators.name[new_enumerators] = strings.intern("One");
                                     })});
                         })},
        "Mixed.cpp");
    expect_invalid_module(ctx, module);

    module = normal_module(
        ctx,
        {append_test_row(enums,
                         [&](auto const new_enums) {
                             enums.cpp_projection[new_enums] = CppProjection{};
                             enums.name[new_enums] = strings.intern("State");
                             enums.underlying_type[new_enums] = make_test_type_ref(ctx, "uint8");
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
                     record_members.type[new_record_members] = make_test_type_ref(ctx, "int32");
                 })});
         })});
    EXPECT_NO_THROW(lower_test_manifest(ctx, manifest_with(ctx, module)));
}

TEST(Validation, NormalModuleAppliesSourceRequirementsPerDeclaration) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& facades{declarations.facades};

    auto& facade_methods{declarations.facade_methods};
    auto& strings{ctx.storage.strings};
    auto facade = append_test_row(facades, [&](auto const new_facades) {
        facades.name[new_facades] = strings.intern("InlineFacade");
        facades.target_type[new_facades] = make_test_type_ref(ctx, "int32");
        facades.target_member_name[new_facades] = strings.intern("target");
        facades.methods[new_facades] = declarations.range<lispb::codegen::FacadeMethodId>(
            {append_test_row(facade_methods, [&](auto const new_facade_methods) {
                facade_methods.name[new_facade_methods] = strings.intern("reset");
                facade_methods.return_type[new_facade_methods] = make_test_type_ref(ctx, "void");
            })});
    });
    auto module = normal_module(ctx, {facade}, "Mixed.cpp");
    EXPECT_NO_THROW(lower_test_manifest(ctx, manifest_with(ctx, module)));

    auto const source_facade{facades.clone(facade)};
    facades.definitions_in_source[source_facade] = true;
    module = normal_module(ctx, {source_facade});
    expect_invalid_module(ctx, module);
}

TEST(Validation, NormalModuleCanMixIndependentEnumApis) {
    CompilerTestContext ctx;
    auto& declarations{ctx.storage.declarations};
    auto& enums{declarations.enums};

    auto& enumerators{declarations.enumerators};
    auto& strings{ctx.storage.strings};
    auto module = normal_module(
        ctx,
        {append_test_row(enums,
                         [&](auto const new_enums) {
                             enums.cpp_projection[new_enums] = CppProjection{};
                             enums.name[new_enums] = strings.intern("NativeMode");
                             enums.underlying_type[new_enums] = make_test_type_ref(ctx, "uint8");
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
             enums.underlying_type[new_enums] = make_test_type_ref(ctx, "uint8");
             enums.values[new_enums] = declarations.range<lispb::codegen::EnumeratorId>(
                 {append_test_row(enumerators, [&](auto const new_enumerators) {
                     enumerators.name[new_enumerators] = strings.intern("One");
                 })});
         })});
    EXPECT_NO_THROW(lower_test_manifest(ctx, manifest_with(ctx, module)));
}

using IncompleteEquivalentType = testing::TestWithParam<InvalidSchemaTextCase>;

TEST_P(IncompleteEquivalentType, RejectsIncompleteMappings) {
    CompilerTestContext ctx;
    auto manifest{parse_test_manifest(
        ctx, std::format("(module test :header \"Test.h\" {})", GetParam().value))};
    EXPECT_THROW(lower_test_manifest(ctx, std::move(manifest)), std::invalid_argument);
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
