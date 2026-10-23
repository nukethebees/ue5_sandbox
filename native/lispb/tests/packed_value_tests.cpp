#include "compiler_test_context.h"
#include "test_utilities.h"

#include <codegen/ast/cpp_types.h>
#include <codegen/ast/indices.h>
#include <codegen/generated_file.h>
#include <codegen/generator.h>
#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/enum_schemas.h>
#include <codegen/schema/enumerator_schemas.h>
#include <codegen/schema/fixed_point_rounding.h>
#include <codegen/schema/fixed_point_schemas.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/integer_scalar_schemas.h>
#include <codegen/schema/linear_quantized_schemas.h>
#include <codegen/schema/manifest.h>
#include <codegen/schema/mini_float_schemas.h>
#include <codegen/schema/module_settings_store.h>
#include <codegen/schema/normal_module_schemas.h>
#include <codegen/schema/packed_bit_order.h>
#include <codegen/schema/packed_byte_order.h>
#include <codegen/schema/packed_field_kind.h>
#include <codegen/schema/packed_field_schemas.h>
#include <codegen/schema/packed_integer_value.h>
#include <codegen/schema/packed_named_code_schemas.h>
#include <codegen/schema/packed_reserved_bits_schemas.h>
#include <codegen/schema/packed_segment_id.h>
#include <codegen/schema/packed_value_schemas.h>
#include <codegen/schema/quantization_clipping.h>
#include <codegen/schema/schema_version.h>
#include <codegen/schema/semantic_relation_kind.h>
#include <codegen/schema/semantic_relation_schemas.h>
#include <codegen/schema/type_refs.h>
#include <lispb/compiler_storage.h>
#include <lispb/indices.h>
#include <lispb/schema/enum_signedness.h>
#include <lispb/schema/packed_segment_id.h>
#include <lispb/strings.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lispb::codegen {
namespace {

auto compact(std::string text) -> std::string {
    std::erase_if(text, [](unsigned char const character) { return std::isspace(character) != 0; });
    return text;
}

void expect_type_only_in_alias_definitions(std::string const& header,
                                           std::string const& spelling,
                                           std::size_t const expected_definitions) {
    std::size_t count{};
    for (auto position{header.find(spelling)}; position != std::string::npos;
         position = header.find(spelling, position + spelling.size())) {
        ++count;
        EXPECT_EQ(header.substr(position - 3, 3), " = ");
        EXPECT_EQ(header[position + spelling.size()], ';');
    }
    EXPECT_EQ(count, expected_definitions);
}

auto valid_module(CompilerTestContext& context) -> ModuleHandle {
    return parse_test_manifest(context, R"(
(module packed :header "Packed.h"
  (packed-value FighterState :storage std::uint32_t
    (field entity_index std::uint32_t :bits 24)
    (field state FighterStateKind :bits 8 :kind enum)))
)")
        .modules.front();
}

auto lower(CompilerTestContext& context, ModuleHandle module) -> std::string {
    auto const files{render_test_manifest(context,
                                          Manifest{manifest_schema_version,
                                                   make_test_registry(context),
                                                   context.storage.declarations,
                                                   {module}})};
    EXPECT_EQ(files.size(), 1);
    return files.front().content;
}

auto lower_known_enum(CompilerTestContext& context, ModuleHandle module, DeclarationHandle schema)
    -> std::string {
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& packed_values{declarations.packed_values};
    auto& module_settings_store{declarations.module_settings_store};
    auto& strings{context.storage.strings};
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{declarations.packed_fields.clone(rows_1[1])};
                    declarations.packed_fields.type[declarations.packed_fields.index(row_0)] =
                        make_test_type_ref(context, "@state");
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    auto const files{render_test_manifest(
        context,
        Manifest{manifest_schema_version,
                 make_test_registry(context,
                                    {{"state", context.storage.cpp_types.add("FighterStateKind")}}),
                 declarations,
                 {[&] {
                      auto const new_normal_modules{normal_modules.append()};
                      auto const new_normal_modules_row{normal_modules.index(new_normal_modules)};
                      normal_modules.settings[new_normal_modules_row] = [&] {
                          auto const new_module_settings_store{module_settings_store.append()};
                          auto const new_module_settings_store_row{
                              module_settings_store.index(new_module_settings_store)};
                          module_settings_store.name[new_module_settings_store_row] =
                              strings.intern("enums");
                          module_settings_store.header[new_module_settings_store_row] =
                              strings.intern("Enums.h");
                          return new_module_settings_store;
                      }();
                      normal_modules.declarations[new_normal_modules_row] =
                          declarations.range<DeclarationHandle>({schema});
                      return new_normal_modules;
                  }(),
                  module}})};
    EXPECT_EQ(files.size(), 2);
    return files.back().content;
}

auto scalar_backed_manifest(CompilerTestContext& context,
                            ModuleHandle module,
                            bool const signedness = false) -> Manifest {
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& packed_values{declarations.packed_values};
    auto& packed_fields{declarations.packed_fields};
    auto& module_settings_store{declarations.module_settings_store};
    auto& integer_scalars{declarations.integer_scalars};
    auto& packed_named_codes{declarations.packed_named_codes};
    auto& strings{context.storage.strings};

    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.type[packed_fields.index(row_0)] =
                        make_test_type_ref(context, "project::Health");
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
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.kind[packed_fields.index(row_0)] =
                        signedness ? PackedFieldKind::signed_integer
                                   : PackedFieldKind::unsigned_integer;
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
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.bits[packed_fields.index(row_0)].reset();
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    normal_modules.settings[normal_modules.index(module)] = [&] {
        auto row_0{
            module_settings_store.clone(normal_modules.settings[normal_modules.index(module)])};
        module_settings_store.namespace_name[module_settings_store.index(row_0)] =
            strings.intern("project");
        return row_0;
    }();
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{packed_values.clone(rows_1.front())};
            auto const row_0_row{packed_values.index(row_0)};
            packed_values.segments[row_0_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_0_row])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.resize(1);
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
            auto row_0{packed_values.clone(rows_1.front())};
            packed_values.mutable_value[packed_values.index(row_0)] = true;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();

    return Manifest{
        manifest_schema_version,
        make_test_registry(context),
        declarations,
        {[&] {
             auto const new_normal_modules{normal_modules.append()};
             auto const new_normal_modules_row{normal_modules.index(new_normal_modules)};
             normal_modules.settings[new_normal_modules_row] = [&] {
                 auto const new_module_settings_store{module_settings_store.append()};
                 auto const new_module_settings_store_row{
                     module_settings_store.index(new_module_settings_store)};
                 module_settings_store.name[new_module_settings_store_row] =
                     strings.intern("scalars");
                 module_settings_store.header[new_module_settings_store_row] =
                     strings.intern("Scalars.h");
                 module_settings_store.namespace_name[new_module_settings_store_row] =
                     strings.intern("project");
                 return new_module_settings_store;
             }();
             normal_modules.declarations[new_normal_modules_row] =
                 declarations.range<DeclarationHandle>({[&] {
                     auto const new_integer_scalars{integer_scalars.append()};
                     auto const new_integer_scalars_row{integer_scalars.index(new_integer_scalars)};
                     integer_scalars.name[new_integer_scalars_row] = strings.intern("Health");
                     integer_scalars.signedness[new_integer_scalars_row] = signedness;
                     integer_scalars.minimum_value[new_integer_scalars_row] =
                         signedness ? PackedIntegerValue{-100} : PackedIntegerValue{0};
                     integer_scalars.maximum_value[new_integer_scalars_row] =
                         signedness ? PackedIntegerValue{100} : PackedIntegerValue{1000};
                     integer_scalars.bit_width[new_integer_scalars_row] = std::nullopt;
                     integer_scalars.named_codes[new_integer_scalars_row] =
                         declarations.range<PackedNamedCodeId>({[&] {
                             auto const new_packed_named_codes{packed_named_codes.append()};
                             auto const new_packed_named_codes_row{
                                 packed_named_codes.index(new_packed_named_codes)};
                             packed_named_codes.name[new_packed_named_codes_row] =
                                 strings.intern("Unknown");
                             packed_named_codes.value[new_packed_named_codes_row] =
                                 signedness ? PackedIntegerValue{-128} : PackedIntegerValue{4095};
                             packed_named_codes.sentinel[new_packed_named_codes_row] = true;
                             return new_packed_named_codes;
                         }()});
                     return new_integer_scalars;
                 }()});
             return new_normal_modules;
         }(),
         module}};
}

auto lower_scalar_backed(CompilerTestContext& context,
                         ModuleHandle module,
                         bool const signedness = false) -> std::string {
    auto const files{
        render_test_manifest(context, scalar_backed_manifest(context, module, signedness))};
    EXPECT_EQ(files.size(), 2);
    return files.back().content;
}

auto quantized_backed_manifest(CompilerTestContext& context, std::uint64_t const reserved_codes = 2)
    -> Manifest {
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& module_settings_store{declarations.module_settings_store};
    auto& integer_scalars{declarations.integer_scalars};
    auto& linear_quantizeds{declarations.linear_quantizeds};

    auto& packed_values{declarations.packed_values};
    auto& packed_fields{declarations.packed_fields};
    auto& strings{context.storage.strings};
    return Manifest{
        manifest_schema_version,
        make_test_registry(context),
        declarations,
        {[&] {
             auto const new_normal_modules{normal_modules.append()};
             auto const new_normal_modules_row{normal_modules.index(new_normal_modules)};
             normal_modules.settings[new_normal_modules_row] = [&] {
                 auto const new_module_settings_store{module_settings_store.append()};
                 auto const new_module_settings_store_row{
                     module_settings_store.index(new_module_settings_store)};
                 module_settings_store.name[new_module_settings_store_row] =
                     strings.intern("scalars");
                 module_settings_store.header[new_module_settings_store_row] =
                     strings.intern("Scalars.h");
                 module_settings_store.namespace_name[new_module_settings_store_row] =
                     strings.intern("project");
                 return new_module_settings_store;
             }();
             normal_modules.declarations[new_normal_modules_row] =
                 declarations.range<DeclarationHandle>({[&] {
                     auto const new_integer_scalars{integer_scalars.append()};
                     auto const new_integer_scalars_row{integer_scalars.index(new_integer_scalars)};
                     integer_scalars.name[new_integer_scalars_row] = strings.intern("Health");
                     integer_scalars.signedness[new_integer_scalars_row] = false;
                     integer_scalars.minimum_value[new_integer_scalars_row] = 0;
                     integer_scalars.maximum_value[new_integer_scalars_row] = 1000;
                     integer_scalars.bit_width[new_integer_scalars_row] = std::nullopt;
                     return new_integer_scalars;
                 }()});
             return new_normal_modules;
         }(),
         [&] {
             auto const new_normal_modules{normal_modules.append()};
             auto const new_normal_modules_row{normal_modules.index(new_normal_modules)};
             normal_modules.settings[new_normal_modules_row] = [&] {
                 auto const new_module_settings_store{module_settings_store.append()};
                 auto const new_module_settings_store_row{
                     module_settings_store.index(new_module_settings_store)};
                 module_settings_store.name[new_module_settings_store_row] =
                     strings.intern("representations");
                 module_settings_store.header[new_module_settings_store_row] =
                     strings.intern("Representations.h");
                 module_settings_store.namespace_name[new_module_settings_store_row] =
                     strings.intern("project");
                 return new_module_settings_store;
             }();
             normal_modules.declarations[new_normal_modules_row] =
                 declarations.range<DeclarationHandle>({[&] {
                     auto const new_linear_quantizeds{linear_quantizeds.append()};
                     auto const new_linear_quantizeds_row{
                         linear_quantizeds.index(new_linear_quantizeds)};
                     linear_quantizeds.name[new_linear_quantizeds_row] = strings.intern("HealthQ8");
                     linear_quantizeds.source[new_linear_quantizeds_row] =
                         make_test_type_ref(context, "project::Health");
                     linear_quantizeds.bit_width[new_linear_quantizeds_row] = 8;
                     linear_quantizeds.reserved_codes[new_linear_quantizeds_row] = reserved_codes;
                     linear_quantizeds.clipping[new_linear_quantizeds_row] =
                         QuantizationClipping::clamp;
                     return new_linear_quantizeds;
                 }()});
             return new_normal_modules;
         }(),
         [&] {
             auto const new_normal_modules{normal_modules.append()};
             auto const new_normal_modules_row{normal_modules.index(new_normal_modules)};
             normal_modules.settings[new_normal_modules_row] = [&] {
                 auto const new_module_settings_store{module_settings_store.append()};
                 auto const new_module_settings_store_row{
                     module_settings_store.index(new_module_settings_store)};
                 module_settings_store.name[new_module_settings_store_row] =
                     strings.intern("packed");
                 module_settings_store.header[new_module_settings_store_row] =
                     strings.intern("Packed.h");
                 module_settings_store.namespace_name[new_module_settings_store_row] =
                     strings.intern("project");
                 return new_module_settings_store;
             }();
             normal_modules.declarations[new_normal_modules_row] =
                 declarations.range<DeclarationHandle>({[&] {
                     auto const new_packed_values{packed_values.append()};
                     auto const new_packed_values_row{packed_values.index(new_packed_values)};
                     packed_values.name[new_packed_values_row] = strings.intern("Vitals");
                     packed_values.storage_type[new_packed_values_row] =
                         make_test_type_ref(context, "std::uint16_t");
                     packed_values.segments[new_packed_values_row] =
                         declarations.range<PackedSegmentId>(
                             {[&] {
                                  auto const new_packed_fields{packed_fields.append()};
                                  auto const new_packed_fields_row{
                                      packed_fields.index(new_packed_fields)};
                                  packed_fields.name[new_packed_fields_row] =
                                      strings.intern("health");
                                  packed_fields.type[new_packed_fields_row] =
                                      make_test_type_ref(context, "project::HealthQ8");
                                  packed_fields.bits[new_packed_fields_row] = std::nullopt;
                                  packed_fields.kind[new_packed_fields_row] =
                                      PackedFieldKind::linear_quantized;
                                  return new_packed_fields;
                              }(),
                              [&] {
                                  auto const new_packed_fields{packed_fields.append()};
                                  auto const new_packed_fields_row{
                                      packed_fields.index(new_packed_fields)};
                                  packed_fields.name[new_packed_fields_row] =
                                      strings.intern("state");
                                  packed_fields.type[new_packed_fields_row] =
                                      make_test_type_ref(context, "std::uint8_t");
                                  packed_fields.bits[new_packed_fields_row] = 8;
                                  return new_packed_fields;
                              }()});
                     packed_values.mutable_value[new_packed_values_row] = true;
                     return new_packed_values;
                 }()});
             return new_normal_modules;
         }()}};
}

auto fixed_point_backed_manifest(CompilerTestContext& context, bool const signedness = true)
    -> Manifest {
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& module_settings_store{declarations.module_settings_store};
    auto& fixed_points{declarations.fixed_points};
    auto& packed_values{declarations.packed_values};

    auto& packed_fields{declarations.packed_fields};
    auto& strings{context.storage.strings};
    return Manifest{
        manifest_schema_version,
        make_test_registry(context),
        declarations,
        {[&] {
             auto const new_normal_modules{normal_modules.append()};
             auto const new_normal_modules_row{normal_modules.index(new_normal_modules)};
             normal_modules.settings[new_normal_modules_row] = [&] {
                 auto const new_module_settings_store{module_settings_store.append()};
                 auto const new_module_settings_store_row{
                     module_settings_store.index(new_module_settings_store)};
                 module_settings_store.name[new_module_settings_store_row] =
                     strings.intern("representations");
                 module_settings_store.header[new_module_settings_store_row] =
                     strings.intern("Representations.h");
                 module_settings_store.namespace_name[new_module_settings_store_row] =
                     strings.intern("project");
                 return new_module_settings_store;
             }();
             normal_modules.declarations[new_normal_modules_row] =
                 declarations.range<DeclarationHandle>({[&] {
                     auto const new_fixed_points{fixed_points.append()};
                     auto const new_fixed_points_row{fixed_points.index(new_fixed_points)};
                     fixed_points.name[new_fixed_points_row] = strings.intern("VelocityQ8_4");
                     fixed_points.signedness[new_fixed_points_row] = signedness;
                     fixed_points.total_bits[new_fixed_points_row] = 12;
                     fixed_points.fractional_bits[new_fixed_points_row] = 4;
                     fixed_points.rounding[new_fixed_points_row] = FixedPointRounding::nearest_even;
                     return new_fixed_points;
                 }()});
             return new_normal_modules;
         }(),
         [&] {
             auto const new_normal_modules{normal_modules.append()};
             auto const new_normal_modules_row{normal_modules.index(new_normal_modules)};
             normal_modules.settings[new_normal_modules_row] = [&] {
                 auto const new_module_settings_store{module_settings_store.append()};
                 auto const new_module_settings_store_row{
                     module_settings_store.index(new_module_settings_store)};
                 module_settings_store.name[new_module_settings_store_row] =
                     strings.intern("packed");
                 module_settings_store.header[new_module_settings_store_row] =
                     strings.intern("Packed.h");
                 module_settings_store.namespace_name[new_module_settings_store_row] =
                     strings.intern("project");
                 return new_module_settings_store;
             }();
             normal_modules.declarations[new_normal_modules_row] =
                 declarations.range<DeclarationHandle>({[&] {
                     auto const new_packed_values{packed_values.append()};
                     auto const new_packed_values_row{packed_values.index(new_packed_values)};
                     packed_values.name[new_packed_values_row] = strings.intern("Motion");
                     packed_values.storage_type[new_packed_values_row] =
                         make_test_type_ref(context, "std::uint16_t");
                     packed_values.segments[new_packed_values_row] =
                         declarations.range<PackedSegmentId>(
                             {[&] {
                                  auto const new_packed_fields{packed_fields.append()};
                                  auto const new_packed_fields_row{
                                      packed_fields.index(new_packed_fields)};
                                  packed_fields.name[new_packed_fields_row] =
                                      strings.intern("velocity");
                                  packed_fields.type[new_packed_fields_row] =
                                      make_test_type_ref(context, "project::VelocityQ8_4");
                                  packed_fields.bits[new_packed_fields_row] = std::nullopt;
                                  packed_fields.kind[new_packed_fields_row] =
                                      PackedFieldKind::fixed_point;
                                  return new_packed_fields;
                              }(),
                              [&] {
                                  auto const new_packed_fields{packed_fields.append()};
                                  auto const new_packed_fields_row{
                                      packed_fields.index(new_packed_fields)};
                                  packed_fields.name[new_packed_fields_row] =
                                      strings.intern("state");
                                  packed_fields.type[new_packed_fields_row] =
                                      make_test_type_ref(context, "std::uint8_t");
                                  packed_fields.bits[new_packed_fields_row] = 4;
                                  return new_packed_fields;
                              }()});
                     packed_values.mutable_value[new_packed_values_row] = true;
                     return new_packed_values;
                 }()});
             return new_normal_modules;
         }()}};
}

auto mini_float_backed_manifest(CompilerTestContext& context) -> Manifest {
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& module_settings_store{declarations.module_settings_store};
    auto& mini_floats{declarations.mini_floats};
    auto& packed_values{declarations.packed_values};

    auto& packed_fields{declarations.packed_fields};
    auto& strings{context.storage.strings};
    return Manifest{
        manifest_schema_version,
        make_test_registry(context),
        declarations,
        {[&] {
             auto const new_normal_modules{normal_modules.append()};
             auto const new_normal_modules_row{normal_modules.index(new_normal_modules)};
             normal_modules.settings[new_normal_modules_row] = [&] {
                 auto const new_module_settings_store{module_settings_store.append()};
                 auto const new_module_settings_store_row{
                     module_settings_store.index(new_module_settings_store)};
                 module_settings_store.name[new_module_settings_store_row] =
                     strings.intern("representations");
                 module_settings_store.header[new_module_settings_store_row] =
                     strings.intern("Representations.h");
                 module_settings_store.namespace_name[new_module_settings_store_row] =
                     strings.intern("project");
                 return new_module_settings_store;
             }();
             normal_modules.declarations[new_normal_modules_row] =
                 declarations.range<DeclarationHandle>({[&] {
                     auto const new_mini_floats{mini_floats.append()};
                     auto const new_mini_floats_row{mini_floats.index(new_mini_floats)};
                     mini_floats.name[new_mini_floats_row] = strings.intern("PositionF12");
                     mini_floats.sign_bits[new_mini_floats_row] = 1;
                     mini_floats.exponent_bits[new_mini_floats_row] = 5;
                     mini_floats.significand_bits[new_mini_floats_row] = 6;
                     mini_floats.exponent_bias[new_mini_floats_row] = 15;
                     return new_mini_floats;
                 }()});
             return new_normal_modules;
         }(),
         [&] {
             auto const new_normal_modules{normal_modules.append()};
             auto const new_normal_modules_row{normal_modules.index(new_normal_modules)};
             normal_modules.settings[new_normal_modules_row] = [&] {
                 auto const new_module_settings_store{module_settings_store.append()};
                 auto const new_module_settings_store_row{
                     module_settings_store.index(new_module_settings_store)};
                 module_settings_store.name[new_module_settings_store_row] =
                     strings.intern("packed");
                 module_settings_store.header[new_module_settings_store_row] =
                     strings.intern("Packed.h");
                 module_settings_store.namespace_name[new_module_settings_store_row] =
                     strings.intern("project");
                 return new_module_settings_store;
             }();
             normal_modules.declarations[new_normal_modules_row] =
                 declarations.range<DeclarationHandle>({[&] {
                     auto const new_packed_values{packed_values.append()};
                     auto const new_packed_values_row{packed_values.index(new_packed_values)};
                     packed_values.name[new_packed_values_row] = strings.intern("Position");
                     packed_values.storage_type[new_packed_values_row] =
                         make_test_type_ref(context, "std::uint16_t");
                     packed_values.segments[new_packed_values_row] =
                         declarations.range<PackedSegmentId>(
                             {[&] {
                                  auto const new_packed_fields{packed_fields.append()};
                                  auto const new_packed_fields_row{
                                      packed_fields.index(new_packed_fields)};
                                  packed_fields.name[new_packed_fields_row] =
                                      strings.intern("component");
                                  packed_fields.type[new_packed_fields_row] =
                                      make_test_type_ref(context, "project::PositionF12");
                                  packed_fields.bits[new_packed_fields_row] = std::nullopt;
                                  packed_fields.kind[new_packed_fields_row] =
                                      PackedFieldKind::mini_float;
                                  return new_packed_fields;
                              }(),
                              [&] {
                                  auto const new_packed_fields{packed_fields.append()};
                                  auto const new_packed_fields_row{
                                      packed_fields.index(new_packed_fields)};
                                  packed_fields.name[new_packed_fields_row] =
                                      strings.intern("state");
                                  packed_fields.type[new_packed_fields_row] =
                                      make_test_type_ref(context, "std::uint8_t");
                                  packed_fields.bits[new_packed_fields_row] = 4;
                                  return new_packed_fields;
                              }()});
                     packed_values.mutable_value[new_packed_values_row] = true;
                     return new_packed_values;
                 }()});
             return new_normal_modules;
         }()}};
}

TEST(PackedValue, ValidatesExplicitDefaultsAndReportsStorageBudget) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& module_settings_store{declarations.module_settings_store};
    auto& packed_values{declarations.packed_values};

    auto& packed_fields{declarations.packed_fields};
    auto& strings{context.storage.strings};
    auto const module{normal_modules.append()};
    auto const module_row{normal_modules.index(module)};
    normal_modules.settings[module_row] = [&] {
        auto const new_module_settings_store{module_settings_store.append()};
        auto const new_module_settings_store_row{
            module_settings_store.index(new_module_settings_store)};
        module_settings_store.name[new_module_settings_store_row] = strings.intern("orders");
        module_settings_store.header[new_module_settings_store_row] = strings.intern("Orders.h");
        return new_module_settings_store;
    }();
    normal_modules.declarations[module_row] = declarations.range<DeclarationHandle>({[&] {
        auto const new_packed_values{packed_values.append()};
        auto const new_packed_values_row{packed_values.index(new_packed_values)};
        packed_values.name[new_packed_values_row] = strings.intern("FighterOrder");
        packed_values.storage_type[new_packed_values_row] =
            make_test_type_ref(context, "std::uint8_t");
        packed_values.segments[new_packed_values_row] = declarations.range<PackedSegmentId>(
            {[&] {
                 auto const new_packed_fields{packed_fields.append()};
                 auto const new_packed_fields_row{packed_fields.index(new_packed_fields)};
                 packed_fields.name[new_packed_fields_row] = strings.intern("task");
                 packed_fields.type[new_packed_fields_row] =
                     make_test_type_ref(context, "std::uint8_t");
                 packed_fields.bits[new_packed_fields_row] = 1;
                 packed_fields.default_value[new_packed_fields_row] = 0;
                 return new_packed_fields;
             }(),
             [&] {
                 auto const new_packed_fields{packed_fields.append()};
                 auto const new_packed_fields_row{packed_fields.index(new_packed_fields)};
                 packed_fields.name[new_packed_fields_row] = strings.intern("target");
                 packed_fields.type[new_packed_fields_row] =
                     make_test_type_ref(context, "std::uint8_t");
                 packed_fields.bits[new_packed_fields_row] = 1;
                 packed_fields.default_value[new_packed_fields_row] = 1;
                 return new_packed_fields;
             }()});
        return new_packed_values;
    }()});
    EXPECT_NO_THROW(lower(context, module));

    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3[0] = [&] {
            auto row_2{packed_values.clone(rows_3[0])};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{packed_fields.clone(rows_1[1])};
                    packed_fields.default_value[packed_fields.index(row_0)] = 2;
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);
    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3[0] = [&] {
            auto row_2{packed_values.clone(rows_3[0])};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{packed_fields.clone(rows_1[1])};
                    packed_fields.default_value[packed_fields.index(row_0)] = -1;
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);
    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3[0] = [&] {
            auto row_2{packed_values.clone(rows_3[0])};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{packed_fields.clone(rows_1[1])};
                    packed_fields.default_value[packed_fields.index(row_0)] = 1;
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1[0] = [&] {
            auto row_0{packed_values.clone(rows_1[0])};
            packed_values.invalid_value[packed_values.index(row_0)] = 2;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);
    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1[0] = [&] {
            auto row_0{packed_values.clone(rows_1[0])};
            packed_values.bit_order[packed_values.index(row_0)] =
                PackedBitOrder::most_significant_first;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_NO_THROW(lower(context, module));
    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1[0] = [&] {
            auto row_0{packed_values.clone(rows_1[0])};
            packed_values.invalid_value[packed_values.index(row_0)] = 64;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);
    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3[0] = [&] {
            auto row_2{packed_values.clone(rows_3[0])};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.default_value[packed_fields.index(row_0)].reset();
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_NO_THROW(lower(context, module));
    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1[0] = [&] {
            auto row_0{packed_values.clone(rows_1[0])};
            packed_values.invalid_value[packed_values.index(row_0)].reset();
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3[0] = [&] {
            auto row_2{packed_values.clone(rows_3[0])};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{packed_fields.clone(rows_1[1])};
                    packed_fields.bits[packed_fields.index(row_0)] = 8;
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    try {
        static_cast<void>(lower(context, module));
        FAIL() << "Expected overflow";
    } catch (std::invalid_argument const& error) {
        EXPECT_NE(std::string{error.what()}.find("requires 8 bits, 1 already used, 7 remaining"),
                  std::string::npos);
    }
}

TEST(PackedValue, ValidatesDefaultsAgainstSemanticRangesAndNamedSentinels) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& packed_values{declarations.packed_values};
    auto& packed_fields{declarations.packed_fields};
    auto manifest{scalar_backed_manifest(context, valid_module(context))};

    manifest.modules.back() = [&] {
        auto row_4{normal_modules.clone(manifest.modules.back())};
        auto const row_4_row{normal_modules.index(row_4)};
        normal_modules.declarations[row_4_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_4_row])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{packed_values.clone(rows_3.front())};
                auto const row_2_row{packed_values.index(row_2)};
                packed_values.segments[row_2_row] = [&] {
                    auto const original_items{
                        declarations.items(packed_values.segments[row_2_row])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{packed_fields.clone(rows_1[0])};
                        packed_fields.default_value[packed_fields.index(row_0)] = 0;
                        return row_0;
                    }();
                    return declarations.range(rows_1);
                }();
                return row_2;
            }();
            return declarations.range(rows_3);
        }();
        return row_4;
    }();
    EXPECT_NO_THROW(lower_test_manifest(context, manifest));
    manifest.modules.back() = [&] {
        auto row_4{normal_modules.clone(manifest.modules.back())};
        auto const row_4_row{normal_modules.index(row_4)};
        normal_modules.declarations[row_4_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_4_row])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{packed_values.clone(rows_3.front())};
                auto const row_2_row{packed_values.index(row_2)};
                packed_values.segments[row_2_row] = [&] {
                    auto const original_items{
                        declarations.items(packed_values.segments[row_2_row])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{packed_fields.clone(rows_1[0])};
                        packed_fields.default_value[packed_fields.index(row_0)] = 4095;
                        return row_0;
                    }();
                    return declarations.range(rows_1);
                }();
                return row_2;
            }();
            return declarations.range(rows_3);
        }();
        return row_4;
    }();
    EXPECT_NO_THROW(lower_test_manifest(context, manifest));
    manifest.modules.back() = [&] {
        auto row_4{normal_modules.clone(manifest.modules.back())};
        auto const row_4_row{normal_modules.index(row_4)};
        normal_modules.declarations[row_4_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_4_row])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{packed_values.clone(rows_3.front())};
                auto const row_2_row{packed_values.index(row_2)};
                packed_values.segments[row_2_row] = [&] {
                    auto const original_items{
                        declarations.items(packed_values.segments[row_2_row])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{packed_fields.clone(rows_1[0])};
                        packed_fields.default_value[packed_fields.index(row_0)] = 1001;
                        return row_0;
                    }();
                    return declarations.range(rows_1);
                }();
                return row_2;
            }();
            return declarations.range(rows_3);
        }();
        return row_4;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);
    manifest.modules.back() = [&] {
        auto row_4{normal_modules.clone(manifest.modules.back())};
        auto const row_4_row{normal_modules.index(row_4)};
        normal_modules.declarations[row_4_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_4_row])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{packed_values.clone(rows_3.front())};
                auto const row_2_row{packed_values.index(row_2)};
                packed_values.segments[row_2_row] = [&] {
                    auto const original_items{
                        declarations.items(packed_values.segments[row_2_row])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{packed_fields.clone(rows_1[0])};
                        packed_fields.default_value[packed_fields.index(row_0)] = 1000000;
                        return row_0;
                    }();
                    return declarations.range(rows_1);
                }();
                return row_2;
            }();
            return declarations.range(rows_3);
        }();
        return row_4;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);
}

TEST(PackedValue, LowersTypedFieldsAndThreeWayComparison) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& packed_values{declarations.packed_values};
    auto& packed_fields{declarations.packed_fields};
    auto& packed_named_codes{declarations.packed_named_codes};
    auto& strings{context.storage.strings};
    auto module{normal_modules.clone(valid_module(context))};
    auto const module_row{normal_modules.index(module)};
    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{packed_values.clone(rows_1.front())};
            packed_values.invalid_value[packed_values.index(row_0)] = 0x7fffffffu;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.range_helper[packed_fields.index(row_0)] = true;
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.bits[packed_fields.index(row_0)].reset();
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.minimum_value[packed_fields.index(row_0)] = 10;
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.maximum_value[packed_fields.index(row_0)] = 1'000'000;
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.named_codes[packed_fields.index(row_0)] =
                        declarations.range<PackedNamedCodeId>(
                            {[&] {
                                 auto const new_packed_named_codes{packed_named_codes.append()};
                                 auto const new_packed_named_codes_row{
                                     packed_named_codes.index(new_packed_named_codes)};
                                 packed_named_codes.name[new_packed_named_codes_row] =
                                     strings.intern("Player");
                                 packed_named_codes.value[new_packed_named_codes_row] = 42;
                                 packed_named_codes
                                     .sentinel[packed_named_codes.index(new_packed_named_codes)] =
                                     false;
                                 return new_packed_named_codes;
                             }(),
                             [&] {
                                 auto const new_packed_named_codes{packed_named_codes.append()};
                                 auto const new_packed_named_codes_row{
                                     packed_named_codes.index(new_packed_named_codes)};
                                 packed_named_codes.name[new_packed_named_codes_row] =
                                     strings.intern("Invalid");
                                 packed_named_codes.value[new_packed_named_codes_row] = 0xffffff;
                                 packed_named_codes
                                     .sentinel[packed_named_codes.index(new_packed_named_codes)] =
                                     true;
                                 return new_packed_named_codes;
                             }()});
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    auto const header{lower(context, module)};

    EXPECT_NE(header.find("using storage_type = std::uint32_t;"), std::string::npos);
    EXPECT_NE(header.find("ioj::valid_packed_storage<storage_type, 32>()"), std::string::npos);
    EXPECT_NE(header.find("ioj::PackedField<storage_type, entity_index_type, 0, 24>"),
              std::string::npos);
    EXPECT_NE(header.find("ioj::PackedField<storage_type, state_type, 24, 8>"), std::string::npos);
    EXPECT_NE(header.find("ioj::packed_pack<entity_index_field>(entity_index_value)"),
              std::string::npos);
    EXPECT_NE(header.find("explicit constexpr FighterState("), std::string::npos);
    EXPECT_NE(header.find("from_raw(storage_type const raw)"), std::string::npos);
    EXPECT_EQ(header.find("auto make("), std::string::npos);
    EXPECT_EQ(header.find("entity_index_offset"), std::string::npos);
    EXPECT_EQ(header.find("entity_index_bits"), std::string::npos);
    EXPECT_EQ(header.find("entity_index_value_mask"), std::string::npos);
    EXPECT_EQ(header.find("entity_index_mask"), std::string::npos);
    EXPECT_NE(header.find("operator<=>(FighterState const&) const noexcept = default"),
              std::string::npos);
    EXPECT_NE(header.find("auto const raw{static_cast<storage_type>("), std::string::npos);
    EXPECT_NE(header.find("assert(raw != invalid_value);"), std::string::npos);
    EXPECT_EQ(header.find("try_set_entity_index"), std::string::npos);
    EXPECT_EQ(header.find("set_entity_index"), std::string::npos);
    EXPECT_EQ(header.find("try_make(entity_index_type const entity_index_value"),
              std::string::npos);
    EXPECT_NE(header.find("invalid_value{storage_type{0x7fffffff}}"), std::string::npos);
    EXPECT_NE(header.find("entity_index_range_fits"), std::string::npos);
    EXPECT_NE(header.find("entity_index_minimum{static_cast<entity_index_type>(10)}"),
              std::string::npos);
    EXPECT_NE(header.find("entity_index_maximum{static_cast<entity_index_type>(1000000)}"),
              std::string::npos);
    EXPECT_NE(header.find("entity_index_value >= entity_index_minimum && entity_index_value <= "
                          "entity_index_maximum"),
              std::string::npos);
    EXPECT_NE(
        header.find("ioj::range_fits(first, count, entity_index_minimum, entity_index_maximum)"),
        std::string::npos);
    EXPECT_NE(header.find("entity_index_Player{static_cast<entity_index_type>(42)}"),
              std::string::npos);
    EXPECT_NE(header.find("entity_index_Invalid{static_cast<entity_index_type>(16777215)}"),
              std::string::npos);
    EXPECT_NE(header.find("entity_index_value == entity_index_Invalid"), std::string::npos);
    EXPECT_NE(header.find("entity_index() == entity_index_Invalid"), std::string::npos);
    EXPECT_NE(header.find("is_valid() const noexcept"), std::string::npos);
    EXPECT_NE(header.find("std::underlying_type_t<state_type>"), std::string::npos);
    EXPECT_NE(header.find("static_assert(ioj::valid_packed_enum<state_type, 8>())"),
              std::string::npos);
    EXPECT_NE(header.find("ioj::valid_packed_value_layout<FighterState>()"), std::string::npos);
}

TEST(PackedValue, EmitsAssertBackedMutationOnlyWhenRequested) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto module{normal_modules.clone(valid_module(context))};
    auto const module_row{normal_modules.index(module)};
    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{declarations.packed_values.clone(rows_1.front())};
            declarations.packed_values.mutable_value[declarations.packed_values.index(row_0)] =
                true;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    auto const header{lower(context, module)};

    EXPECT_EQ(header.find("try_make(entity_index_type const entity_index_value"),
              std::string::npos);
    EXPECT_EQ(header.find("try_set_entity_index"), std::string::npos);
    EXPECT_NE(header.find("set_entity_index"), std::string::npos);
}

TEST(PackedValue, UsesFieldAliasesAcrossImmutableAndMutablePublicApis) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& packed_values{declarations.packed_values};
    for (auto const mutable_value : {false, true}) {
        auto module{normal_modules.clone(valid_module(context))};
        auto const module_row{normal_modules.index(module)};

        normal_modules.declarations[module_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[module_row])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{packed_values.clone(rows_1.front())};
                packed_values.mutable_value[packed_values.index(row_0)] = mutable_value;
                return row_0;
            }();
            return declarations.range(rows_1);
        }();
        normal_modules.declarations[module_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[module_row])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{packed_values.clone(rows_3.front())};
                auto const row_2_row{packed_values.index(row_2)};
                packed_values.segments[row_2_row] = [&] {
                    auto const original_items{
                        declarations.items(packed_values.segments[row_2_row])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{declarations.packed_fields.clone(rows_1[0])};
                        declarations.packed_fields
                            .range_helper[declarations.packed_fields.index(row_0)] = true;
                        return row_0;
                    }();
                    return declarations.range(rows_1);
                }();
                return row_2;
            }();
            return declarations.range(rows_3);
        }();
        auto const header{lower(context, module)};
        auto const text{compact(header)};

        expect_type_only_in_alias_definitions(header, "std::uint32_t", 2);
        expect_type_only_in_alias_definitions(header, "FighterStateKind", 1);
        EXPECT_NE(
            text.find("explicitconstexprFighterState(entity_index_typeconstentity_index_value,"
                      "state_typeconststate_value)noexcept"),
            std::string::npos);
        EXPECT_NE(text.find("entity_index()constnoexcept->entity_index_type"), std::string::npos);
        EXPECT_NE(text.find("state()constnoexcept->state_type"), std::string::npos);
        EXPECT_NE(text.find("entity_index_range_fits(entity_index_typeconstfirst,"
                            "entity_index_typeconstcount)noexcept->bool"),
                  std::string::npos);
        if (mutable_value) {
            EXPECT_EQ(
                text.find("try_make(entity_index_typeconstentity_index_value,"
                          "state_typeconststate_value,FighterState&out_result)noexcept->bool"),
                std::string::npos);
            EXPECT_EQ(text.find("try_set_entity_index(entity_index_typeconstvalue)noexcept->bool"),
                      std::string::npos);
            EXPECT_NE(text.find("set_entity_index(entity_index_typeconstvalue)noexcept"),
                      std::string::npos);
            EXPECT_EQ(text.find("try_set_state(state_typeconstvalue)noexcept->bool"),
                      std::string::npos);
            EXPECT_NE(text.find("set_state(state_typeconstvalue)noexcept"), std::string::npos);
        } else {
            EXPECT_EQ(text.find("try_make("), std::string::npos);
            EXPECT_EQ(text.find("set_entity_index("), std::string::npos);
            EXPECT_EQ(text.find("set_state("), std::string::npos);
        }
    }
}

TEST(PackedValue, UsesRepresentationAliasesAcrossPublicApis) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& packed_values{declarations.packed_values};
    auto& strings{context.storage.strings};
    for (auto const& manifest : {quantized_backed_manifest(context),
                                 fixed_point_backed_manifest(context),
                                 mini_float_backed_manifest(context)}) {
        auto const schema{
            declarations
                .items(declarations.normal_modules.declarations[declarations.normal_modules.index(
                    manifest.modules.back())])
                .front()};
        auto const fixed_point{
            std::string{strings.view(packed_values.name[packed_values.index(schema)])} == "Motion"};
        auto const accessor{
            fixed_point ? std::string{"velocity_raw"}
            : std::string{strings.view(packed_values.name[packed_values.index(schema)])} == "Vitals"
                ? std::string{"health_encoded"}
                : std::string{"component_encoded"}};
        auto const alias{accessor + "_type"};
        auto const parameter{alias + "const" + accessor + "_value"};
        auto const files{render_test_manifest(context, manifest)};
        auto const& header{files.back().content};
        auto const text{compact(header)};

        expect_type_only_in_alias_definitions(
            header,
            "std::uint16_t",
            fixed_point || std::string{strings.view(
                               packed_values.name[packed_values.index(schema)])} == "Vitals"
                ? 1
                : 2);
        expect_type_only_in_alias_definitions(
            header,
            "std::uint8_t",
            std::string{strings.view(packed_values.name[packed_values.index(schema)])} == "Vitals"
                ? 2
                : 1);
        if (fixed_point) {
            expect_type_only_in_alias_definitions(header, "std::int16_t", 1);
            EXPECT_NE(text.find("try_encode_velocity_value(doubleconstvalue,velocity_raw_type&out_"
                                "raw)noexcept->bool"),
                      std::string::npos);
            EXPECT_NE(text.find("velocity_raw_typeraw{};"), std::string::npos);
        }
        EXPECT_NE(
            text.find("explicitconstexpr" +
                      std::string{strings.view(packed_values.name[packed_values.index(schema)])} +
                      "(" + parameter + ",state_typeconststate_value)noexcept"),
            std::string::npos);
        EXPECT_EQ(
            text.find("try_make(" + parameter + ",state_typeconststate_value," +
                      std::string{strings.view(packed_values.name[packed_values.index(schema)])} +
                      "&out_result)noexcept->bool"),
            std::string::npos);
        EXPECT_NE(text.find(accessor + "()constnoexcept->" + alias), std::string::npos);
        EXPECT_EQ(text.find("try_set_" + accessor + "(" + alias + "constvalue)noexcept->bool"),
                  std::string::npos);
        EXPECT_NE(text.find("set_" + accessor + "(" + alias + "constvalue)noexcept"),
                  std::string::npos);
    }
}

TEST(PackedValue, UsesBooleanFieldAliasWithoutChangingStatusTypes) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& packed_values{declarations.packed_values};

    auto& packed_fields{declarations.packed_fields};
    auto& strings{context.storage.strings};
    auto module{normal_modules.clone(valid_module(context))};
    auto const module_row{normal_modules.index(module)};

    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{packed_values.clone(rows_1.front())};
            packed_values.mutable_value[packed_values.index(row_0)] = true;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{packed_values.clone(rows_1.front())};
            packed_values.storage_type[packed_values.index(row_0)] =
                make_test_type_ref(context, "std::uint8_t");
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{packed_values.clone(rows_1.front())};
            packed_values.segments[packed_values.index(row_0)] =
                declarations.range<PackedSegmentId>({[&] {
                    auto const new_packed_fields{packed_fields.append()};
                    auto const new_packed_fields_row{packed_fields.index(new_packed_fields)};
                    packed_fields.name[new_packed_fields_row] = strings.intern("flag");
                    packed_fields.type[new_packed_fields_row] = make_test_type_ref(context, "bool");
                    packed_fields.bits[new_packed_fields_row] = 1;
                    return new_packed_fields;
                }()});
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    auto const text{compact(lower(context, module))};

    EXPECT_NE(text.find("usingflag_type=bool;"), std::string::npos);
    EXPECT_NE(text.find("explicitconstexprFighterState(flag_typeconstflag_value)noexcept"),
              std::string::npos);
    EXPECT_NE(text.find("flag()constnoexcept->flag_type"), std::string::npos);
    EXPECT_EQ(text.find("try_set_flag(flag_typeconstvalue)noexcept->bool"), std::string::npos);
    EXPECT_NE(text.find("set_flag(flag_typeconstvalue)noexcept"), std::string::npos);
    EXPECT_NE(text.find("is_valid()constnoexcept->bool"), std::string::npos);
}

TEST(PackedValue, ReservedSegmentsOccupyBitsWithoutGeneratingValueApi) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& packed_values{declarations.packed_values};
    auto& packed_reserved_bits{declarations.packed_reserved_bits};
    auto module{normal_modules.clone(valid_module(context))};
    auto const module_row{normal_modules.index(module)};
    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{declarations.packed_fields.clone(rows_1[0])};
                    declarations.packed_fields.bits[declarations.packed_fields.index(row_0)] = 20;
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{packed_values.clone(rows_1.front())};
            auto const row_0_row{packed_values.index(row_0)};
            packed_values.segments[row_0_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_0_row])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.insert(rows.begin() + 1, [&] {
                    auto const new_packed_reserved_bits{packed_reserved_bits.append()};
                    auto const new_packed_reserved_bits_row{
                        packed_reserved_bits.index(new_packed_reserved_bits)};
                    packed_reserved_bits.name[new_packed_reserved_bits_row] =
                        context.storage.strings.intern("future");
                    packed_reserved_bits.bits[new_packed_reserved_bits_row] = 4;
                    return new_packed_reserved_bits;
                }());
                return declarations.range(rows);
            }();
            return row_0;
        }();
        return declarations.range(rows_1);
    }();

    auto const header{lower(context, module)};

    EXPECT_NE(header.find("ioj::PackedField<storage_type, entity_index_type, 0, 20>"),
              std::string::npos);
    EXPECT_NE(header.find("ioj::PackedField<storage_type, state_type, 24, 8>"), std::string::npos);
    EXPECT_EQ(header.find("future_field"), std::string::npos);
    EXPECT_EQ(header.find("try_set_future"), std::string::npos);
    EXPECT_EQ(header.find("future_value"), std::string::npos);
}

TEST(PackedValue, MostSignificantFirstSegmentsDriveGeneratedNumericOffsets) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& packed_values{declarations.packed_values};
    auto& packed_reserved_bits{declarations.packed_reserved_bits};
    auto module{normal_modules.clone(valid_module(context))};
    auto const module_row{normal_modules.index(module)};

    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{declarations.packed_fields.clone(rows_1[0])};
                    declarations.packed_fields.bits[declarations.packed_fields.index(row_0)] = 20;
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{packed_values.clone(rows_1.front())};
            auto const row_0_row{packed_values.index(row_0)};
            packed_values.segments[row_0_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_0_row])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.insert(rows.begin() + 1, [&] {
                    auto const new_packed_reserved_bits{packed_reserved_bits.append()};
                    auto const new_packed_reserved_bits_row{
                        packed_reserved_bits.index(new_packed_reserved_bits)};
                    packed_reserved_bits.name[new_packed_reserved_bits_row] =
                        context.storage.strings.intern("future");
                    packed_reserved_bits.bits[new_packed_reserved_bits_row] = 4;
                    return new_packed_reserved_bits;
                }());
                return declarations.range(rows);
            }();
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{packed_values.clone(rows_1.front())};
            packed_values.byte_order[packed_values.index(row_0)] = PackedByteOrder::big_endian;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{packed_values.clone(rows_1.front())};
            packed_values.bit_order[packed_values.index(row_0)] =
                PackedBitOrder::most_significant_first;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();

    auto const header{lower(context, module)};

    EXPECT_NE(header.find("ioj::PackedField<storage_type, entity_index_type, 12, 20>"),
              std::string::npos);
    EXPECT_NE(header.find("ioj::PackedField<storage_type, state_type, 0, 8>"), std::string::npos);
    EXPECT_EQ(header.find("future_field"), std::string::npos);
}

TEST(PackedValue, MostSignificantFirstEnumDomainCanExcludeInvalidRawValue) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& packed_values{declarations.packed_values};
    auto& enums{declarations.enums};
    auto& enumerators{declarations.enumerators};
    auto& strings{context.storage.strings};
    auto module{normal_modules.clone(valid_module(context))};
    auto const module_row{normal_modules.index(module)};

    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{packed_values.clone(rows_1.front())};
            packed_values.invalid_value[packed_values.index(row_0)] = 7;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{packed_values.clone(rows_1.front())};
            packed_values.bit_order[packed_values.index(row_0)] =
                PackedBitOrder::most_significant_first;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();

    auto const header{lower_known_enum(context, module, [&] {
        auto const new_enums{enums.append()};
        auto const new_enums_row{enums.index(new_enums)};
        enums.name[new_enums_row] = strings.intern("FighterStateKind");
        enums.underlying_type[new_enums_row] = make_test_type_ref(context, "uint8");
        enums.values[new_enums_row] = declarations.range<EnumeratorId>(
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
                 enumerators.name[new_enumerators_row] = strings.intern("COUNT");
                 enumerators.initializer[new_enumerators_row] = strings.intern("5");
                 enumerators.display_name[new_enumerators_row] = std::nullopt;
                 enumerators.hidden[new_enumerators_row] = true;
                 return new_enumerators;
             }()});
        enums.count[new_enums_row] = strings.intern("COUNT");
        return new_enums;
    }())};

    EXPECT_EQ(header.find("assert(raw != invalid_value);"), std::string::npos);
}

TEST(PackedValue, SerializedByteOrderDoesNotChangeHostNumericOffsets) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto module{normal_modules.clone(valid_module(context))};
    auto const module_row{normal_modules.index(module)};
    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{declarations.packed_values.clone(rows_1.front())};
            declarations.packed_values.byte_order[declarations.packed_values.index(row_0)] =
                PackedByteOrder::big_endian;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();

    auto const header{lower(context, module)};

    EXPECT_NE(header.find("ioj::PackedField<storage_type, entity_index_type, 0, 24>"),
              std::string::npos);
    EXPECT_NE(header.find("ioj::PackedField<storage_type, state_type, 24, 8>"), std::string::npos);
}

TEST(PackedValue, LowersSignedArbitraryWidthFieldWithSafeSignExtension) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& packed_values{declarations.packed_values};
    auto& packed_fields{declarations.packed_fields};
    auto& packed_reserved_bits{declarations.packed_reserved_bits};
    auto& strings{context.storage.strings};
    auto module{normal_modules.clone(valid_module(context))};
    auto const module_row{normal_modules.index(module)};
    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{packed_values.clone(rows_1.front())};
            packed_values.mutable_value[packed_values.index(row_0)] = true;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{packed_values.clone(rows_1.front())};
            packed_values.name[packed_values.index(row_0)] = strings.intern("SignedDelta");
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{packed_values.clone(rows_1.front())};
            packed_values.segments[packed_values.index(row_0)] =
                declarations.range<PackedSegmentId>(
                    {[&] {
                         auto const new_packed_fields{packed_fields.append()};
                         auto const new_packed_fields_row{packed_fields.index(new_packed_fields)};
                         packed_fields.name[new_packed_fields_row] = strings.intern("delta");
                         packed_fields.type[new_packed_fields_row] =
                             make_test_type_ref(context, "std::int32_t");
                         packed_fields.bits[new_packed_fields_row] = 17;
                         packed_fields.kind[new_packed_fields_row] =
                             PackedFieldKind::signed_integer;
                         return new_packed_fields;
                     }(),
                     [&] {
                         auto const new_packed_reserved_bits{packed_reserved_bits.append()};
                         auto const new_packed_reserved_bits_row{
                             packed_reserved_bits.index(new_packed_reserved_bits)};
                         packed_reserved_bits.name[new_packed_reserved_bits_row] =
                             strings.intern("future");
                         packed_reserved_bits.bits[new_packed_reserved_bits_row] = 15;
                         return new_packed_reserved_bits;
                     }()});
            return row_0;
        }();
        return declarations.range(rows_1);
    }();

    auto const header{lower(context, module)};

    EXPECT_NE(header.find("using delta_type = std::int32_t;"), std::string::npos);
    EXPECT_NE(header.find("delta() const noexcept -> delta_type"), std::string::npos);
    EXPECT_EQ(header.find("try_set_delta(delta_type const value)"), std::string::npos);
    EXPECT_NE(header.find("set_delta(delta_type const value)"), std::string::npos);
    EXPECT_NE(header.find("delta_minimum{static_cast<delta_type>(-65536)}"), std::string::npos);
    EXPECT_NE(header.find("delta_maximum{static_cast<delta_type>(65535)}"), std::string::npos);
    EXPECT_NE(header.find("ioj::packed_extract<delta_field>(value_)"), std::string::npos);
    EXPECT_NE(header.find("ioj::packed_insert<delta_field>(value_, value)"), std::string::npos);
    EXPECT_NE(header.find("assert(value >= delta_minimum && value <= delta_maximum)"),
              std::string::npos);
}

TEST(PackedValue, DerivesAndEnforcesSignedSemanticRangeWithNamedSentinel) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& packed_values{declarations.packed_values};
    auto& packed_fields{declarations.packed_fields};
    auto& packed_named_codes{declarations.packed_named_codes};
    auto& packed_reserved_bits{declarations.packed_reserved_bits};
    auto& strings{context.storage.strings};
    auto module{normal_modules.clone(valid_module(context))};
    auto const module_row{normal_modules.index(module)};
    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{packed_values.clone(rows_1.front())};
            packed_values.mutable_value[packed_values.index(row_0)] = true;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{packed_values.clone(rows_1.front())};
            packed_values.name[packed_values.index(row_0)] = strings.intern("SignedTemperature");
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{packed_values.clone(rows_1.front())};
            packed_values
                .segments[packed_values.index(row_0)] = declarations.range<PackedSegmentId>(
                {[&] {
                     auto const new_packed_fields{packed_fields.append()};
                     auto const new_packed_fields_row{packed_fields.index(new_packed_fields)};
                     packed_fields.name[new_packed_fields_row] = strings.intern("temperature");
                     packed_fields.type[new_packed_fields_row] =
                         make_test_type_ref(context, "std::int16_t");
                     packed_fields.bits[new_packed_fields_row] = std::nullopt;
                     packed_fields.kind[new_packed_fields_row] = PackedFieldKind::signed_integer;
                     packed_fields.range_helper[new_packed_fields_row] = false;
                     packed_fields.minimum_value[new_packed_fields_row] = -100;
                     packed_fields.maximum_value[new_packed_fields_row] = 100;
                     packed_fields.named_codes[new_packed_fields_row] =
                         declarations.range<PackedNamedCodeId>(
                             {[&] {
                                  auto const new_packed_named_codes{packed_named_codes.append()};
                                  packed_named_codes
                                      .name[packed_named_codes.index(new_packed_named_codes)] =
                                      strings.intern("Freezing");
                                  packed_named_codes
                                      .value[packed_named_codes.index(new_packed_named_codes)] = 0;
                                  packed_named_codes
                                      .sentinel[packed_named_codes.index(new_packed_named_codes)] =
                                      false;
                                  return new_packed_named_codes;
                              }(),
                              [&] {
                                  auto const new_packed_named_codes{packed_named_codes.append()};
                                  packed_named_codes
                                      .name[packed_named_codes.index(new_packed_named_codes)] =
                                      strings.intern("Unknown");
                                  packed_named_codes
                                      .value[packed_named_codes.index(new_packed_named_codes)] =
                                      -128;
                                  packed_named_codes
                                      .sentinel[packed_named_codes.index(new_packed_named_codes)] =
                                      true;
                                  return new_packed_named_codes;
                              }()});
                     return new_packed_fields;
                 }(),
                 [&] {
                     auto const new_packed_reserved_bits{packed_reserved_bits.append()};
                     auto const new_packed_reserved_bits_row{
                         packed_reserved_bits.index(new_packed_reserved_bits)};
                     packed_reserved_bits.name[new_packed_reserved_bits_row] =
                         strings.intern("future");
                     packed_reserved_bits.bits[new_packed_reserved_bits_row] = 24;
                     return new_packed_reserved_bits;
                 }()});
            return row_0;
        }();
        return declarations.range(rows_1);
    }();

    auto const header{lower(context, module)};

    EXPECT_NE(header.find("ioj::PackedField<storage_type, temperature_type, 0, 8>"),
              std::string::npos);
    EXPECT_NE(header.find("temperature_Unknown{static_cast<temperature_type>(-128)}"),
              std::string::npos);
    EXPECT_NE(header.find("value >= static_cast<temperature_type>(-100)"), std::string::npos);
    EXPECT_NE(header.find("value <= static_cast<temperature_type>(100)"), std::string::npos);
    EXPECT_NE(header.find("value == temperature_Unknown"), std::string::npos);
    EXPECT_NE(header.find("temperature() == temperature_Unknown"), std::string::npos);
}

TEST(PackedValue, LowersSharedIntegerScalarDomainForPackedPlacement) {
    CompilerTestContext context;
    auto module{valid_module(context)};
    auto const header{lower_scalar_backed(context, module)};

    EXPECT_NE(header.find("using entity_index_type = std::uint16_t;"), std::string::npos);
    EXPECT_NE(header.find("ioj::PackedField<storage_type, entity_index_type, 0, 12>"),
              std::string::npos);
    EXPECT_NE(header.find("entity_index_Unknown{static_cast<entity_index_type>(4095)}"),
              std::string::npos);
    EXPECT_NE(header.find("value >= entity_index_minimum"), std::string::npos);
    EXPECT_NE(header.find("value <= entity_index_maximum"), std::string::npos);
    EXPECT_NE(header.find("value == entity_index_Unknown"), std::string::npos);
    EXPECT_NE(header.find("entity_index() == entity_index_Unknown"), std::string::npos);
    EXPECT_EQ(header.find("using entity_index_type = project::Health;"), std::string::npos);
}

TEST(PackedValue, LowersSignedSharedIntegerScalarToSmallestNativeAccessor) {
    CompilerTestContext context;
    auto module{valid_module(context)};
    auto const header{lower_scalar_backed(context, module, true)};

    EXPECT_NE(header.find("using entity_index_type = std::int8_t;"), std::string::npos);
    EXPECT_NE(header.find("ioj::PackedField<storage_type, entity_index_type, 0, 8>"),
              std::string::npos);
    EXPECT_NE(header.find("entity_index_Unknown{static_cast<entity_index_type>(-128)}"),
              std::string::npos);
    EXPECT_NE(header.find("ioj::packed_extract<entity_index_field>(value_)"), std::string::npos);
}

TEST(PackedValue, LowersLinearQuantizedPlacementToExplicitEncodedCodeApi) {
    CompilerTestContext context;
    auto const files{render_test_manifest(context, quantized_backed_manifest(context))};
    ASSERT_EQ(files.size(), 3);
    auto const& header{files.back().content};

    EXPECT_NE(header.find("using health_encoded_type = std::uint8_t;"), std::string::npos);
    EXPECT_NE(header.find("health_maximum_encoded{health_encoded_type{0xfd}}"), std::string::npos);
    EXPECT_EQ(header.find("try_make(health_encoded_type const health_encoded_value"),
              std::string::npos);
    EXPECT_NE(header.find("auto health_encoded() const noexcept -> health_encoded_type"),
              std::string::npos);
    EXPECT_EQ(header.find("try_set_health_encoded(health_encoded_type const value)"),
              std::string::npos);
    EXPECT_NE(header.find("set_health_encoded(health_encoded_type const value)"),
              std::string::npos);
    EXPECT_NE(header.find("value <= health_maximum_encoded"), std::string::npos);
    EXPECT_NE(header.find("health_encoded() <= health_maximum_encoded"), std::string::npos);
    EXPECT_EQ(header.find("project::HealthQ8"), std::string::npos);
    EXPECT_EQ(header.find("health()"), std::string::npos);
}

TEST(PackedValue, RejectsCompetingLinearQuantizedPlacementFacts) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& packed_values{declarations.packed_values};
    auto& packed_fields{declarations.packed_fields};
    auto manifest{quantized_backed_manifest(context)};

    manifest.modules.back() = [&] {
        auto row_4{normal_modules.clone(manifest.modules.back())};
        auto const row_4_row{normal_modules.index(row_4)};
        normal_modules.declarations[row_4_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_4_row])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{packed_values.clone(rows_3.front())};
                auto const row_2_row{packed_values.index(row_2)};
                packed_values.segments[row_2_row] = [&] {
                    auto const original_items{
                        declarations.items(packed_values.segments[row_2_row])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{packed_fields.clone(rows_1[0])};
                        packed_fields.bits[packed_fields.index(row_0)] = 7;
                        return row_0;
                    }();
                    return declarations.range(rows_1);
                }();
                return row_2;
            }();
            return declarations.range(rows_3);
        }();
        return row_4;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);

    manifest.modules.back() = [&] {
        auto row_4{normal_modules.clone(manifest.modules.back())};
        auto const row_4_row{normal_modules.index(row_4)};
        normal_modules.declarations[row_4_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_4_row])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{packed_values.clone(rows_3.front())};
                auto const row_2_row{packed_values.index(row_2)};
                packed_values.segments[row_2_row] = [&] {
                    auto const original_items{
                        declarations.items(packed_values.segments[row_2_row])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{packed_fields.clone(rows_1[0])};
                        packed_fields.bits[packed_fields.index(row_0)].reset();
                        return row_0;
                    }();
                    return declarations.range(rows_1);
                }();
                return row_2;
            }();
            return declarations.range(rows_3);
        }();
        return row_4;
    }();
    manifest.modules.back() = [&] {
        auto row_4{normal_modules.clone(manifest.modules.back())};
        auto const row_4_row{normal_modules.index(row_4)};
        normal_modules.declarations[row_4_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_4_row])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{packed_values.clone(rows_3.front())};
                auto const row_2_row{packed_values.index(row_2)};
                packed_values.segments[row_2_row] = [&] {
                    auto const original_items{
                        declarations.items(packed_values.segments[row_2_row])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{packed_fields.clone(rows_1[0])};
                        packed_fields.kind[packed_fields.index(row_0)] =
                            PackedFieldKind::unsigned_integer;
                        return row_0;
                    }();
                    return declarations.range(rows_1);
                }();
                return row_2;
            }();
            return declarations.range(rows_3);
        }();
        return row_4;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);

    manifest.modules.back() = [&] {
        auto row_4{normal_modules.clone(manifest.modules.back())};
        auto const row_4_row{normal_modules.index(row_4)};
        normal_modules.declarations[row_4_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_4_row])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{packed_values.clone(rows_3.front())};
                auto const row_2_row{packed_values.index(row_2)};
                packed_values.segments[row_2_row] = [&] {
                    auto const original_items{
                        declarations.items(packed_values.segments[row_2_row])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{packed_fields.clone(rows_1[0])};
                        packed_fields.kind[packed_fields.index(row_0)] =
                            PackedFieldKind::linear_quantized;
                        return row_0;
                    }();
                    return declarations.range(rows_1);
                }();
                return row_2;
            }();
            return declarations.range(rows_3);
        }();
        return row_4;
    }();
    manifest.modules.back() = [&] {
        auto row_4{normal_modules.clone(manifest.modules.back())};
        auto const row_4_row{normal_modules.index(row_4)};
        normal_modules.declarations[row_4_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_4_row])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{packed_values.clone(rows_3.front())};
                auto const row_2_row{packed_values.index(row_2)};
                packed_values.segments[row_2_row] = [&] {
                    auto const original_items{
                        declarations.items(packed_values.segments[row_2_row])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{packed_fields.clone(rows_1[0])};
                        packed_fields.minimum_value[packed_fields.index(row_0)] = 0;
                        return row_0;
                    }();
                    return declarations.range(rows_1);
                }();
                return row_2;
            }();
            return declarations.range(rows_3);
        }();
        return row_4;
    }();
    manifest.modules.back() = [&] {
        auto row_4{normal_modules.clone(manifest.modules.back())};
        auto const row_4_row{normal_modules.index(row_4)};
        normal_modules.declarations[row_4_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_4_row])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{packed_values.clone(rows_3.front())};
                auto const row_2_row{packed_values.index(row_2)};
                packed_values.segments[row_2_row] = [&] {
                    auto const original_items{
                        declarations.items(packed_values.segments[row_2_row])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{packed_fields.clone(rows_1[0])};
                        packed_fields.maximum_value[packed_fields.index(row_0)] = 1000;
                        return row_0;
                    }();
                    return declarations.range(rows_1);
                }();
                return row_2;
            }();
            return declarations.range(rows_3);
        }();
        return row_4;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);

    manifest.modules.back() = [&] {
        auto row_4{normal_modules.clone(manifest.modules.back())};
        auto const row_4_row{normal_modules.index(row_4)};
        normal_modules.declarations[row_4_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_4_row])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{packed_values.clone(rows_3.front())};
                auto const row_2_row{packed_values.index(row_2)};
                packed_values.segments[row_2_row] = [&] {
                    auto const original_items{
                        declarations.items(packed_values.segments[row_2_row])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{packed_fields.clone(rows_1[0])};
                        packed_fields.minimum_value[packed_fields.index(row_0)].reset();
                        return row_0;
                    }();
                    return declarations.range(rows_1);
                }();
                return row_2;
            }();
            return declarations.range(rows_3);
        }();
        return row_4;
    }();
    manifest.modules.back() = [&] {
        auto row_4{normal_modules.clone(manifest.modules.back())};
        auto const row_4_row{normal_modules.index(row_4)};
        normal_modules.declarations[row_4_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_4_row])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{packed_values.clone(rows_3.front())};
                auto const row_2_row{packed_values.index(row_2)};
                packed_values.segments[row_2_row] = [&] {
                    auto const original_items{
                        declarations.items(packed_values.segments[row_2_row])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{packed_fields.clone(rows_1[0])};
                        packed_fields.maximum_value[packed_fields.index(row_0)].reset();
                        return row_0;
                    }();
                    return declarations.range(rows_1);
                }();
                return row_2;
            }();
            return declarations.range(rows_3);
        }();
        return row_4;
    }();
    manifest.modules.back() = [&] {
        auto row_4{normal_modules.clone(manifest.modules.back())};
        auto const row_4_row{normal_modules.index(row_4)};
        normal_modules.declarations[row_4_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_4_row])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{packed_values.clone(rows_3.front())};
                auto const row_2_row{packed_values.index(row_2)};
                packed_values.segments[row_2_row] = [&] {
                    auto const original_items{
                        declarations.items(packed_values.segments[row_2_row])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{packed_fields.clone(rows_1[0])};
                        packed_fields.range_helper[packed_fields.index(row_0)] = true;
                        return row_0;
                    }();
                    return declarations.range(rows_1);
                }();
                return row_2;
            }();
            return declarations.range(rows_3);
        }();
        return row_4;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);
}

TEST(PackedValue, LowersFixedPointPlacementToExplicitRawCodeApi) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& fixed_points{declarations.fixed_points};
    auto& strings{context.storage.strings};
    auto const files{render_test_manifest(context, fixed_point_backed_manifest(context))};
    ASSERT_EQ(files.size(), 2);
    auto const& header{files.back().content};

    EXPECT_NE(header.find("using velocity_raw_type = std::int16_t;"), std::string::npos);
    EXPECT_NE(header.find("velocity_minimum_raw{static_cast<velocity_raw_type>(-2048)}"),
              std::string::npos);
    EXPECT_NE(header.find("velocity_maximum_raw{static_cast<velocity_raw_type>(2047)}"),
              std::string::npos);
    EXPECT_EQ(header.find("try_make(velocity_raw_type const velocity_raw_value"),
              std::string::npos);
    EXPECT_NE(header.find("auto velocity_raw() const noexcept -> velocity_raw_type"),
              std::string::npos);
    EXPECT_EQ(header.find("try_set_velocity_raw(velocity_raw_type const value)"),
              std::string::npos);
    EXPECT_NE(header.find("set_velocity_raw(velocity_raw_type const value)"), std::string::npos);
    EXPECT_NE(header.find("value >= velocity_minimum_allowed_raw && value <= "
                          "velocity_maximum_allowed_raw"),
              std::string::npos);
    EXPECT_NE(header.find("auto velocity_value() const noexcept -> double"), std::string::npos);
    EXPECT_NE(header.find("try_encode_velocity_value(double const value"), std::string::npos);
    EXPECT_EQ(header.find("try_set_velocity_value(double const value"), std::string::npos);
    EXPECT_NE(header.find("set_velocity_value(double const value"), std::string::npos);
    EXPECT_EQ(header.find("project::VelocityQ8_4"), std::string::npos);
    EXPECT_EQ(header.find("velocity()"), std::string::npos);

    auto const unsigned_files{
        render_test_manifest(context, fixed_point_backed_manifest(context, false))};
    ASSERT_EQ(unsigned_files.size(), 2);
    auto const& unsigned_header{unsigned_files.back().content};
    EXPECT_NE(unsigned_header.find("using velocity_raw_type = std::uint16_t;"), std::string::npos);
    EXPECT_NE(unsigned_header.find("velocity_maximum_raw{velocity_raw_type{0xfff}}"),
              std::string::npos);

    auto bounded_manifest{fixed_point_backed_manifest(context)};

    bounded_manifest.modules.front() = [&] {
        auto row_2{normal_modules.clone(bounded_manifest.modules.front())};
        auto const row_2_row{normal_modules.index(row_2)};
        normal_modules.declarations[row_2_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_2_row])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{fixed_points.clone(rows_1.front())};
                fixed_points.minimum_value[fixed_points.index(row_0)] = strings.intern("-1.5");
                return row_0;
            }();
            return declarations.range(rows_1);
        }();
        return row_2;
    }();
    bounded_manifest.modules.front() = [&] {
        auto row_2{normal_modules.clone(bounded_manifest.modules.front())};
        auto const row_2_row{normal_modules.index(row_2)};
        normal_modules.declarations[row_2_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_2_row])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{fixed_points.clone(rows_1.front())};
                fixed_points.maximum_value[fixed_points.index(row_0)] = strings.intern("2.25");
                return row_0;
            }();
            return declarations.range(rows_1);
        }();
        return row_2;
    }();
    auto const bounded_files{render_test_manifest(context, bounded_manifest)};
    auto const& bounded_header{bounded_files.back().content};
    EXPECT_NE(
        bounded_header.find("velocity_minimum_allowed_raw{static_cast<velocity_raw_type>(-24)}"),
        std::string::npos);
    EXPECT_NE(
        bounded_header.find("velocity_maximum_allowed_raw{static_cast<velocity_raw_type>(36)}"),
        std::string::npos);

    auto immutable_manifest{fixed_point_backed_manifest(context)};
    immutable_manifest.modules.back() = [&] {
        auto row_2{normal_modules.clone(immutable_manifest.modules.back())};
        auto const row_2_row{normal_modules.index(row_2)};
        normal_modules.declarations[row_2_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_2_row])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{declarations.packed_values.clone(rows_1.front())};
                declarations.packed_values.mutable_value[declarations.packed_values.index(row_0)] =
                    false;
                return row_0;
            }();
            return declarations.range(rows_1);
        }();
        return row_2;
    }();
    auto const immutable_files{render_test_manifest(context, immutable_manifest)};
    ASSERT_EQ(immutable_files.size(), 2);
    EXPECT_NE(immutable_files.back().content.find(
                  "assert(velocity_raw_value >= velocity_minimum_allowed_raw && "
                  "velocity_raw_value <= velocity_maximum_allowed_raw);"),
              std::string::npos);
}

TEST(PackedValue, RejectsCompetingFixedPointPlacementFacts) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& packed_values{declarations.packed_values};
    auto& packed_fields{declarations.packed_fields};
    auto& packed_named_codes{declarations.packed_named_codes};
    auto& semantic_relations{declarations.semantic_relations};
    auto& strings{context.storage.strings};
    auto manifest{fixed_point_backed_manifest(context)};

    manifest.modules.back() = [&] {
        auto row_4{normal_modules.clone(manifest.modules.back())};
        auto const row_4_row{normal_modules.index(row_4)};
        normal_modules.declarations[row_4_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_4_row])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{packed_values.clone(rows_3.front())};
                auto const row_2_row{packed_values.index(row_2)};
                packed_values.segments[row_2_row] = [&] {
                    auto const original_items{
                        declarations.items(packed_values.segments[row_2_row])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{packed_fields.clone(rows_1[0])};
                        packed_fields.bits[packed_fields.index(row_0)] = 11;
                        return row_0;
                    }();
                    return declarations.range(rows_1);
                }();
                return row_2;
            }();
            return declarations.range(rows_3);
        }();
        return row_4;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);

    manifest.modules.back() = [&] {
        auto row_4{normal_modules.clone(manifest.modules.back())};
        auto const row_4_row{normal_modules.index(row_4)};
        normal_modules.declarations[row_4_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_4_row])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{packed_values.clone(rows_3.front())};
                auto const row_2_row{packed_values.index(row_2)};
                packed_values.segments[row_2_row] = [&] {
                    auto const original_items{
                        declarations.items(packed_values.segments[row_2_row])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{packed_fields.clone(rows_1[0])};
                        packed_fields.bits[packed_fields.index(row_0)].reset();
                        return row_0;
                    }();
                    return declarations.range(rows_1);
                }();
                return row_2;
            }();
            return declarations.range(rows_3);
        }();
        return row_4;
    }();
    manifest.modules.back() = [&] {
        auto row_4{normal_modules.clone(manifest.modules.back())};
        auto const row_4_row{normal_modules.index(row_4)};
        normal_modules.declarations[row_4_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_4_row])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{packed_values.clone(rows_3.front())};
                auto const row_2_row{packed_values.index(row_2)};
                packed_values.segments[row_2_row] = [&] {
                    auto const original_items{
                        declarations.items(packed_values.segments[row_2_row])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{packed_fields.clone(rows_1[0])};
                        packed_fields.kind[packed_fields.index(row_0)] =
                            PackedFieldKind::signed_integer;
                        return row_0;
                    }();
                    return declarations.range(rows_1);
                }();
                return row_2;
            }();
            return declarations.range(rows_3);
        }();
        return row_4;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);

    manifest.modules.back() = [&] {
        auto row_4{normal_modules.clone(manifest.modules.back())};
        auto const row_4_row{normal_modules.index(row_4)};
        normal_modules.declarations[row_4_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_4_row])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{packed_values.clone(rows_3.front())};
                auto const row_2_row{packed_values.index(row_2)};
                packed_values.segments[row_2_row] = [&] {
                    auto const original_items{
                        declarations.items(packed_values.segments[row_2_row])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{packed_fields.clone(rows_1[0])};
                        packed_fields.kind[packed_fields.index(row_0)] =
                            PackedFieldKind::fixed_point;
                        return row_0;
                    }();
                    return declarations.range(rows_1);
                }();
                return row_2;
            }();
            return declarations.range(rows_3);
        }();
        return row_4;
    }();
    manifest.modules.back() = [&] {
        auto row_4{normal_modules.clone(manifest.modules.back())};
        auto const row_4_row{normal_modules.index(row_4)};
        normal_modules.declarations[row_4_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_4_row])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{packed_values.clone(rows_3.front())};
                auto const row_2_row{packed_values.index(row_2)};
                packed_values.segments[row_2_row] = [&] {
                    auto const original_items{
                        declarations.items(packed_values.segments[row_2_row])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{packed_fields.clone(rows_1[0])};
                        packed_fields.minimum_value[packed_fields.index(row_0)] = -10;
                        return row_0;
                    }();
                    return declarations.range(rows_1);
                }();
                return row_2;
            }();
            return declarations.range(rows_3);
        }();
        return row_4;
    }();
    manifest.modules.back() = [&] {
        auto row_4{normal_modules.clone(manifest.modules.back())};
        auto const row_4_row{normal_modules.index(row_4)};
        normal_modules.declarations[row_4_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_4_row])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{packed_values.clone(rows_3.front())};
                auto const row_2_row{packed_values.index(row_2)};
                packed_values.segments[row_2_row] = [&] {
                    auto const original_items{
                        declarations.items(packed_values.segments[row_2_row])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{packed_fields.clone(rows_1[0])};
                        packed_fields.maximum_value[packed_fields.index(row_0)] = 10;
                        return row_0;
                    }();
                    return declarations.range(rows_1);
                }();
                return row_2;
            }();
            return declarations.range(rows_3);
        }();
        return row_4;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);

    manifest.modules.back() = [&] {
        auto row_4{normal_modules.clone(manifest.modules.back())};
        auto const row_4_row{normal_modules.index(row_4)};
        normal_modules.declarations[row_4_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_4_row])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{packed_values.clone(rows_3.front())};
                auto const row_2_row{packed_values.index(row_2)};
                packed_values.segments[row_2_row] = [&] {
                    auto const original_items{
                        declarations.items(packed_values.segments[row_2_row])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{packed_fields.clone(rows_1[0])};
                        packed_fields.minimum_value[packed_fields.index(row_0)].reset();
                        return row_0;
                    }();
                    return declarations.range(rows_1);
                }();
                return row_2;
            }();
            return declarations.range(rows_3);
        }();
        return row_4;
    }();
    manifest.modules.back() = [&] {
        auto row_4{normal_modules.clone(manifest.modules.back())};
        auto const row_4_row{normal_modules.index(row_4)};
        normal_modules.declarations[row_4_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_4_row])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{packed_values.clone(rows_3.front())};
                auto const row_2_row{packed_values.index(row_2)};
                packed_values.segments[row_2_row] = [&] {
                    auto const original_items{
                        declarations.items(packed_values.segments[row_2_row])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{packed_fields.clone(rows_1[0])};
                        packed_fields.maximum_value[packed_fields.index(row_0)].reset();
                        return row_0;
                    }();
                    return declarations.range(rows_1);
                }();
                return row_2;
            }();
            return declarations.range(rows_3);
        }();
        return row_4;
    }();
    manifest.modules.back() = [&] {
        auto row_4{normal_modules.clone(manifest.modules.back())};
        auto const row_4_row{normal_modules.index(row_4)};
        normal_modules.declarations[row_4_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_4_row])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{packed_values.clone(rows_3.front())};
                auto const row_2_row{packed_values.index(row_2)};
                packed_values.segments[row_2_row] = [&] {
                    auto const original_items{
                        declarations.items(packed_values.segments[row_2_row])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{packed_fields.clone(rows_1[0])};
                        auto const row_0_row{packed_fields.index(row_0)};
                        packed_fields.named_codes[row_0_row] = [&] {
                            auto const original_items{
                                declarations.items(packed_fields.named_codes[row_0_row])};
                            auto rows{std::vector(original_items.begin(), original_items.end())};
                            rows.push_back([&] {
                                auto const new_packed_named_codes{packed_named_codes.append()};
                                auto const new_packed_named_codes_row{
                                    packed_named_codes.index(new_packed_named_codes)};
                                packed_named_codes.name[new_packed_named_codes_row] =
                                    strings.intern("Zero");
                                packed_named_codes.value[new_packed_named_codes_row] = 0;
                                packed_named_codes.sentinel[new_packed_named_codes_row] = false;
                                return new_packed_named_codes;
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
        return row_4;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);

    manifest.modules.back() = [&] {
        auto row_4{normal_modules.clone(manifest.modules.back())};
        auto const row_4_row{normal_modules.index(row_4)};
        normal_modules.declarations[row_4_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_4_row])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{packed_values.clone(rows_3.front())};
                auto const row_2_row{packed_values.index(row_2)};
                packed_values.segments[row_2_row] = [&] {
                    auto const original_items{
                        declarations.items(packed_values.segments[row_2_row])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{packed_fields.clone(rows_1[0])};
                        packed_fields.named_codes[packed_fields.index(row_0)] = {};
                        return row_0;
                    }();
                    return declarations.range(rows_1);
                }();
                return row_2;
            }();
            return declarations.range(rows_3);
        }();
        return row_4;
    }();
    manifest.modules.back() = [&] {
        auto row_4{normal_modules.clone(manifest.modules.back())};
        auto const row_4_row{normal_modules.index(row_4)};
        normal_modules.declarations[row_4_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_4_row])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{packed_values.clone(rows_3.front())};
                auto const row_2_row{packed_values.index(row_2)};
                packed_values.segments[row_2_row] = [&] {
                    auto const original_items{
                        declarations.items(packed_values.segments[row_2_row])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{packed_fields.clone(rows_1[0])};
                        packed_fields.range_helper[packed_fields.index(row_0)] = true;
                        return row_0;
                    }();
                    return declarations.range(rows_1);
                }();
                return row_2;
            }();
            return declarations.range(rows_3);
        }();
        return row_4;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);

    manifest.modules.back() = [&] {
        auto row_4{normal_modules.clone(manifest.modules.back())};
        auto const row_4_row{normal_modules.index(row_4)};
        normal_modules.declarations[row_4_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_4_row])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{packed_values.clone(rows_3.front())};
                auto const row_2_row{packed_values.index(row_2)};
                packed_values.segments[row_2_row] = [&] {
                    auto const original_items{
                        declarations.items(packed_values.segments[row_2_row])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{packed_fields.clone(rows_1[0])};
                        packed_fields.range_helper[packed_fields.index(row_0)] = false;
                        return row_0;
                    }();
                    return declarations.range(rows_1);
                }();
                return row_2;
            }();
            return declarations.range(rows_3);
        }();
        return row_4;
    }();
    manifest.modules.back() = [&] {
        auto row_4{normal_modules.clone(manifest.modules.back())};
        auto const row_4_row{normal_modules.index(row_4)};
        normal_modules.declarations[row_4_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_4_row])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{packed_values.clone(rows_3.front())};
                auto const row_2_row{packed_values.index(row_2)};
                packed_values.segments[row_2_row] = [&] {
                    auto const original_items{
                        declarations.items(packed_values.segments[row_2_row])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{packed_fields.clone(rows_1[0])};
                        packed_fields.relationship[packed_fields.index(row_0)] = [&] {
                            auto const new_semantic_relations{semantic_relations.append()};
                            auto const new_semantic_relations_row{
                                semantic_relations.index(new_semantic_relations)};
                            semantic_relations.kind[new_semantic_relations_row] =
                                SemanticRelationKind::encoded_as;
                            semantic_relations.target[new_semantic_relations_row] =
                                make_test_type_ref(context, "project::VelocityQ8_4");
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
        return row_4;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);
}

TEST(PackedValue, LowersMiniFloatPlacementAsRawEncodedBits) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& packed_values{declarations.packed_values};
    auto const files{render_test_manifest(context, mini_float_backed_manifest(context))};
    ASSERT_EQ(files.size(), 2U);
    auto const& header{files.back().content};
    EXPECT_NE(header.find("using component_encoded_type = std::uint16_t;"), std::string::npos);
    EXPECT_NE(header.find("ioj::PackedField<storage_type, component_encoded_type, 0, 12>"),
              std::string::npos);
    EXPECT_NE(header.find("component_maximum_encoded{component_encoded_type{0xfff}}"),
              std::string::npos);
    EXPECT_NE(header.find("auto component_encoded() const noexcept -> component_encoded_type"),
              std::string::npos);
    EXPECT_EQ(header.find("try_set_component_encoded(component_encoded_type const value)"),
              std::string::npos);
    EXPECT_NE(header.find("set_component_encoded(component_encoded_type const value)"),
              std::string::npos);
    EXPECT_NE(header.find("value <= component_maximum_encoded"), std::string::npos);
    EXPECT_EQ(header.find("project::PositionF12"), std::string::npos);
    EXPECT_EQ(header.find("component()"), std::string::npos);

    auto wide{mini_float_backed_manifest(context)};

    wide.modules.front() = [&] {
        auto row_2{normal_modules.clone(wide.modules.front())};
        auto const row_2_row{normal_modules.index(row_2)};
        normal_modules.declarations[row_2_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_2_row])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{declarations.mini_floats.clone(rows_1.front())};
                declarations.mini_floats.significand_bits[declarations.mini_floats.index(row_0)] =
                    58;
                return row_0;
            }();
            return declarations.range(rows_1);
        }();
        return row_2;
    }();

    wide.modules.back() = [&] {
        auto row_2{normal_modules.clone(wide.modules.back())};
        auto const row_2_row{normal_modules.index(row_2)};
        normal_modules.declarations[row_2_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_2_row])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{packed_values.clone(rows_1.front())};
                packed_values.storage_type[packed_values.index(row_0)] =
                    make_test_type_ref(context, "std::uint64_t");
                return row_0;
            }();
            return declarations.range(rows_1);
        }();
        return row_2;
    }();
    wide.modules.back() = [&] {
        auto row_2{normal_modules.clone(wide.modules.back())};
        auto const row_2_row{normal_modules.index(row_2)};
        normal_modules.declarations[row_2_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_2_row])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{packed_values.clone(rows_1.front())};
                auto const row_0_row{packed_values.index(row_0)};
                packed_values.segments[row_0_row] = [&] {
                    auto const original_items{
                        declarations.items(packed_values.segments[row_0_row])};
                    auto rows{std::vector(original_items.begin(), original_items.end())};
                    rows.resize(1);
                    return declarations.range(rows);
                }();
                return row_0;
            }();
            return declarations.range(rows_1);
        }();
        return row_2;
    }();
    wide.modules.back() = [&] {
        auto row_2{normal_modules.clone(wide.modules.back())};
        auto const row_2_row{normal_modules.index(row_2)};
        normal_modules.declarations[row_2_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_2_row])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{packed_values.clone(rows_1.front())};
                packed_values.mutable_value[packed_values.index(row_0)] = false;
                return row_0;
            }();
            return declarations.range(rows_1);
        }();
        return row_2;
    }();
    auto const wide_files{render_test_manifest(context, wide)};
    ASSERT_EQ(wide_files.size(), 2U);
    EXPECT_NE(wide_files.back().content.find(
                  "ioj::PackedField<storage_type, component_encoded_type, 0, 64>"),
              std::string::npos);
    EXPECT_NE(wide_files.back().content.find("component_maximum_encoded{"
                                             "component_encoded_type{0xffffffffffffffff}}"),
              std::string::npos);
    EXPECT_NE(wide_files.back().content.find(
                  "assert(component_encoded_value <= component_maximum_encoded);"),
              std::string::npos);
}

TEST(PackedValue, RejectsCompetingMiniFloatPlacementFacts) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& packed_values{declarations.packed_values};
    auto& packed_fields{declarations.packed_fields};
    auto& packed_named_codes{declarations.packed_named_codes};
    auto& semantic_relations{declarations.semantic_relations};
    auto& strings{context.storage.strings};
    auto manifest{mini_float_backed_manifest(context)};

    manifest.modules.back() = [&] {
        auto row_4{normal_modules.clone(manifest.modules.back())};
        auto const row_4_row{normal_modules.index(row_4)};
        normal_modules.declarations[row_4_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_4_row])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{packed_values.clone(rows_3.front())};
                auto const row_2_row{packed_values.index(row_2)};
                packed_values.segments[row_2_row] = [&] {
                    auto const original_items{
                        declarations.items(packed_values.segments[row_2_row])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{packed_fields.clone(rows_1[0])};
                        packed_fields.bits[packed_fields.index(row_0)] = 11;
                        return row_0;
                    }();
                    return declarations.range(rows_1);
                }();
                return row_2;
            }();
            return declarations.range(rows_3);
        }();
        return row_4;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);
    manifest.modules.back() = [&] {
        auto row_4{normal_modules.clone(manifest.modules.back())};
        auto const row_4_row{normal_modules.index(row_4)};
        normal_modules.declarations[row_4_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_4_row])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{packed_values.clone(rows_3.front())};
                auto const row_2_row{packed_values.index(row_2)};
                packed_values.segments[row_2_row] = [&] {
                    auto const original_items{
                        declarations.items(packed_values.segments[row_2_row])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{packed_fields.clone(rows_1[0])};
                        packed_fields.bits[packed_fields.index(row_0)].reset();
                        return row_0;
                    }();
                    return declarations.range(rows_1);
                }();
                return row_2;
            }();
            return declarations.range(rows_3);
        }();
        return row_4;
    }();
    manifest.modules.back() = [&] {
        auto row_4{normal_modules.clone(manifest.modules.back())};
        auto const row_4_row{normal_modules.index(row_4)};
        normal_modules.declarations[row_4_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_4_row])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{packed_values.clone(rows_3.front())};
                auto const row_2_row{packed_values.index(row_2)};
                packed_values.segments[row_2_row] = [&] {
                    auto const original_items{
                        declarations.items(packed_values.segments[row_2_row])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{packed_fields.clone(rows_1[0])};
                        packed_fields.kind[packed_fields.index(row_0)] =
                            PackedFieldKind::unsigned_integer;
                        return row_0;
                    }();
                    return declarations.range(rows_1);
                }();
                return row_2;
            }();
            return declarations.range(rows_3);
        }();
        return row_4;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);
    manifest.modules.back() = [&] {
        auto row_4{normal_modules.clone(manifest.modules.back())};
        auto const row_4_row{normal_modules.index(row_4)};
        normal_modules.declarations[row_4_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_4_row])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{packed_values.clone(rows_3.front())};
                auto const row_2_row{packed_values.index(row_2)};
                packed_values.segments[row_2_row] = [&] {
                    auto const original_items{
                        declarations.items(packed_values.segments[row_2_row])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{packed_fields.clone(rows_1[0])};
                        packed_fields.kind[packed_fields.index(row_0)] =
                            PackedFieldKind::mini_float;
                        return row_0;
                    }();
                    return declarations.range(rows_1);
                }();
                return row_2;
            }();
            return declarations.range(rows_3);
        }();
        return row_4;
    }();
    manifest.modules.back() = [&] {
        auto row_4{normal_modules.clone(manifest.modules.back())};
        auto const row_4_row{normal_modules.index(row_4)};
        normal_modules.declarations[row_4_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_4_row])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{packed_values.clone(rows_3.front())};
                auto const row_2_row{packed_values.index(row_2)};
                packed_values.segments[row_2_row] = [&] {
                    auto const original_items{
                        declarations.items(packed_values.segments[row_2_row])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{packed_fields.clone(rows_1[0])};
                        packed_fields.minimum_value[packed_fields.index(row_0)] = 0;
                        return row_0;
                    }();
                    return declarations.range(rows_1);
                }();
                return row_2;
            }();
            return declarations.range(rows_3);
        }();
        return row_4;
    }();
    manifest.modules.back() = [&] {
        auto row_4{normal_modules.clone(manifest.modules.back())};
        auto const row_4_row{normal_modules.index(row_4)};
        normal_modules.declarations[row_4_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_4_row])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{packed_values.clone(rows_3.front())};
                auto const row_2_row{packed_values.index(row_2)};
                packed_values.segments[row_2_row] = [&] {
                    auto const original_items{
                        declarations.items(packed_values.segments[row_2_row])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{packed_fields.clone(rows_1[0])};
                        packed_fields.maximum_value[packed_fields.index(row_0)] = 1;
                        return row_0;
                    }();
                    return declarations.range(rows_1);
                }();
                return row_2;
            }();
            return declarations.range(rows_3);
        }();
        return row_4;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);
    manifest.modules.back() = [&] {
        auto row_4{normal_modules.clone(manifest.modules.back())};
        auto const row_4_row{normal_modules.index(row_4)};
        normal_modules.declarations[row_4_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_4_row])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{packed_values.clone(rows_3.front())};
                auto const row_2_row{packed_values.index(row_2)};
                packed_values.segments[row_2_row] = [&] {
                    auto const original_items{
                        declarations.items(packed_values.segments[row_2_row])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{packed_fields.clone(rows_1[0])};
                        packed_fields.minimum_value[packed_fields.index(row_0)].reset();
                        return row_0;
                    }();
                    return declarations.range(rows_1);
                }();
                return row_2;
            }();
            return declarations.range(rows_3);
        }();
        return row_4;
    }();
    manifest.modules.back() = [&] {
        auto row_4{normal_modules.clone(manifest.modules.back())};
        auto const row_4_row{normal_modules.index(row_4)};
        normal_modules.declarations[row_4_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_4_row])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{packed_values.clone(rows_3.front())};
                auto const row_2_row{packed_values.index(row_2)};
                packed_values.segments[row_2_row] = [&] {
                    auto const original_items{
                        declarations.items(packed_values.segments[row_2_row])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{packed_fields.clone(rows_1[0])};
                        packed_fields.maximum_value[packed_fields.index(row_0)].reset();
                        return row_0;
                    }();
                    return declarations.range(rows_1);
                }();
                return row_2;
            }();
            return declarations.range(rows_3);
        }();
        return row_4;
    }();
    manifest.modules.back() = [&] {
        auto row_4{normal_modules.clone(manifest.modules.back())};
        auto const row_4_row{normal_modules.index(row_4)};
        normal_modules.declarations[row_4_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_4_row])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{packed_values.clone(rows_3.front())};
                auto const row_2_row{packed_values.index(row_2)};
                packed_values.segments[row_2_row] = [&] {
                    auto const original_items{
                        declarations.items(packed_values.segments[row_2_row])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{packed_fields.clone(rows_1[0])};
                        auto const row_0_row{packed_fields.index(row_0)};
                        packed_fields.named_codes[row_0_row] = [&] {
                            auto const original_items{
                                declarations.items(packed_fields.named_codes[row_0_row])};
                            auto rows{std::vector(original_items.begin(), original_items.end())};
                            rows.push_back([&] {
                                auto const new_packed_named_codes{packed_named_codes.append()};
                                auto const new_packed_named_codes_row{
                                    packed_named_codes.index(new_packed_named_codes)};
                                packed_named_codes.name[new_packed_named_codes_row] =
                                    strings.intern("Zero");
                                packed_named_codes.value[new_packed_named_codes_row] = 0;
                                packed_named_codes.sentinel[new_packed_named_codes_row] = false;
                                return new_packed_named_codes;
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
        return row_4;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);
    manifest.modules.back() = [&] {
        auto row_4{normal_modules.clone(manifest.modules.back())};
        auto const row_4_row{normal_modules.index(row_4)};
        normal_modules.declarations[row_4_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_4_row])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{packed_values.clone(rows_3.front())};
                auto const row_2_row{packed_values.index(row_2)};
                packed_values.segments[row_2_row] = [&] {
                    auto const original_items{
                        declarations.items(packed_values.segments[row_2_row])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{packed_fields.clone(rows_1[0])};
                        packed_fields.named_codes[packed_fields.index(row_0)] = {};
                        return row_0;
                    }();
                    return declarations.range(rows_1);
                }();
                return row_2;
            }();
            return declarations.range(rows_3);
        }();
        return row_4;
    }();
    manifest.modules.back() = [&] {
        auto row_4{normal_modules.clone(manifest.modules.back())};
        auto const row_4_row{normal_modules.index(row_4)};
        normal_modules.declarations[row_4_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_4_row])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{packed_values.clone(rows_3.front())};
                auto const row_2_row{packed_values.index(row_2)};
                packed_values.segments[row_2_row] = [&] {
                    auto const original_items{
                        declarations.items(packed_values.segments[row_2_row])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{packed_fields.clone(rows_1[0])};
                        packed_fields.range_helper[packed_fields.index(row_0)] = true;
                        return row_0;
                    }();
                    return declarations.range(rows_1);
                }();
                return row_2;
            }();
            return declarations.range(rows_3);
        }();
        return row_4;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);
    manifest.modules.back() = [&] {
        auto row_4{normal_modules.clone(manifest.modules.back())};
        auto const row_4_row{normal_modules.index(row_4)};
        normal_modules.declarations[row_4_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_4_row])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{packed_values.clone(rows_3.front())};
                auto const row_2_row{packed_values.index(row_2)};
                packed_values.segments[row_2_row] = [&] {
                    auto const original_items{
                        declarations.items(packed_values.segments[row_2_row])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{packed_fields.clone(rows_1[0])};
                        packed_fields.range_helper[packed_fields.index(row_0)] = false;
                        return row_0;
                    }();
                    return declarations.range(rows_1);
                }();
                return row_2;
            }();
            return declarations.range(rows_3);
        }();
        return row_4;
    }();
    manifest.modules.back() = [&] {
        auto row_4{normal_modules.clone(manifest.modules.back())};
        auto const row_4_row{normal_modules.index(row_4)};
        normal_modules.declarations[row_4_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_4_row])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{packed_values.clone(rows_3.front())};
                auto const row_2_row{packed_values.index(row_2)};
                packed_values.segments[row_2_row] = [&] {
                    auto const original_items{
                        declarations.items(packed_values.segments[row_2_row])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{packed_fields.clone(rows_1[0])};
                        packed_fields.relationship[packed_fields.index(row_0)] = [&] {
                            auto const new_semantic_relations{semantic_relations.append()};
                            auto const new_semantic_relations_row{
                                semantic_relations.index(new_semantic_relations)};
                            semantic_relations.kind[new_semantic_relations_row] =
                                SemanticRelationKind::encoded_as;
                            semantic_relations.target[new_semantic_relations_row] =
                                make_test_type_ref(context, "project::PositionF12");
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
        return row_4;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);
}

TEST(PackedValue, RejectsCompetingOrMismatchedIntegerScalarFieldDomain) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& packed_values{declarations.packed_values};
    auto& packed_fields{declarations.packed_fields};
    auto module{valid_module(context)};
    auto manifest{scalar_backed_manifest(context, module)};

    manifest.modules.back() = [&] {
        auto row_4{normal_modules.clone(manifest.modules.back())};
        auto const row_4_row{normal_modules.index(row_4)};
        normal_modules.declarations[row_4_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_4_row])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{packed_values.clone(rows_3.front())};
                auto const row_2_row{packed_values.index(row_2)};
                packed_values.segments[row_2_row] = [&] {
                    auto const original_items{
                        declarations.items(packed_values.segments[row_2_row])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{packed_fields.clone(rows_1[0])};
                        packed_fields.minimum_value[packed_fields.index(row_0)] = 0;
                        return row_0;
                    }();
                    return declarations.range(rows_1);
                }();
                return row_2;
            }();
            return declarations.range(rows_3);
        }();
        return row_4;
    }();
    manifest.modules.back() = [&] {
        auto row_4{normal_modules.clone(manifest.modules.back())};
        auto const row_4_row{normal_modules.index(row_4)};
        normal_modules.declarations[row_4_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_4_row])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{packed_values.clone(rows_3.front())};
                auto const row_2_row{packed_values.index(row_2)};
                packed_values.segments[row_2_row] = [&] {
                    auto const original_items{
                        declarations.items(packed_values.segments[row_2_row])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{packed_fields.clone(rows_1[0])};
                        packed_fields.maximum_value[packed_fields.index(row_0)] = 1000;
                        return row_0;
                    }();
                    return declarations.range(rows_1);
                }();
                return row_2;
            }();
            return declarations.range(rows_3);
        }();
        return row_4;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);

    manifest.modules.back() = [&] {
        auto row_4{normal_modules.clone(manifest.modules.back())};
        auto const row_4_row{normal_modules.index(row_4)};
        normal_modules.declarations[row_4_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_4_row])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{packed_values.clone(rows_3.front())};
                auto const row_2_row{packed_values.index(row_2)};
                packed_values.segments[row_2_row] = [&] {
                    auto const original_items{
                        declarations.items(packed_values.segments[row_2_row])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{packed_fields.clone(rows_1[0])};
                        packed_fields.minimum_value[packed_fields.index(row_0)].reset();
                        return row_0;
                    }();
                    return declarations.range(rows_1);
                }();
                return row_2;
            }();
            return declarations.range(rows_3);
        }();
        return row_4;
    }();
    manifest.modules.back() = [&] {
        auto row_4{normal_modules.clone(manifest.modules.back())};
        auto const row_4_row{normal_modules.index(row_4)};
        normal_modules.declarations[row_4_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_4_row])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{packed_values.clone(rows_3.front())};
                auto const row_2_row{packed_values.index(row_2)};
                packed_values.segments[row_2_row] = [&] {
                    auto const original_items{
                        declarations.items(packed_values.segments[row_2_row])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{packed_fields.clone(rows_1[0])};
                        packed_fields.maximum_value[packed_fields.index(row_0)].reset();
                        return row_0;
                    }();
                    return declarations.range(rows_1);
                }();
                return row_2;
            }();
            return declarations.range(rows_3);
        }();
        return row_4;
    }();
    manifest.modules.back() = [&] {
        auto row_4{normal_modules.clone(manifest.modules.back())};
        auto const row_4_row{normal_modules.index(row_4)};
        normal_modules.declarations[row_4_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_4_row])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{packed_values.clone(rows_3.front())};
                auto const row_2_row{packed_values.index(row_2)};
                packed_values.segments[row_2_row] = [&] {
                    auto const original_items{
                        declarations.items(packed_values.segments[row_2_row])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{packed_fields.clone(rows_1[0])};
                        packed_fields.kind[packed_fields.index(row_0)] =
                            PackedFieldKind::signed_integer;
                        return row_0;
                    }();
                    return declarations.range(rows_1);
                }();
                return row_2;
            }();
            return declarations.range(rows_3);
        }();
        return row_4;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);

    manifest.modules.back() = [&] {
        auto row_4{normal_modules.clone(manifest.modules.back())};
        auto const row_4_row{normal_modules.index(row_4)};
        normal_modules.declarations[row_4_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_4_row])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{packed_values.clone(rows_3.front())};
                auto const row_2_row{packed_values.index(row_2)};
                packed_values.segments[row_2_row] = [&] {
                    auto const original_items{
                        declarations.items(packed_values.segments[row_2_row])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{packed_fields.clone(rows_1[0])};
                        packed_fields.kind[packed_fields.index(row_0)] =
                            PackedFieldKind::unsigned_integer;
                        return row_0;
                    }();
                    return declarations.range(rows_1);
                }();
                return row_2;
            }();
            return declarations.range(rows_3);
        }();
        return row_4;
    }();
    manifest.modules.back() = [&] {
        auto row_4{normal_modules.clone(manifest.modules.back())};
        auto const row_4_row{normal_modules.index(row_4)};
        normal_modules.declarations[row_4_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[row_4_row])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{packed_values.clone(rows_3.front())};
                auto const row_2_row{packed_values.index(row_2)};
                packed_values.segments[row_2_row] = [&] {
                    auto const original_items{
                        declarations.items(packed_values.segments[row_2_row])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{packed_fields.clone(rows_1[0])};
                        packed_fields.bits[packed_fields.index(row_0)] = 11;
                        return row_0;
                    }();
                    return declarations.range(rows_1);
                }();
                return row_2;
            }();
            return declarations.range(rows_3);
        }();
        return row_4;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);
}

TEST(PackedValue, RejectsInvalidLayoutsAndTypes) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& packed_values{declarations.packed_values};
    auto& packed_fields{declarations.packed_fields};

    auto& packed_reserved_bits{declarations.packed_reserved_bits};
    auto& packed_named_codes{declarations.packed_named_codes};
    auto& semantic_relations{declarations.semantic_relations};
    auto& strings{context.storage.strings};
    auto module{normal_modules.clone(valid_module(context))};
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.bits[packed_fields.index(row_0)] = 0;
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.bits[packed_fields.index(row_0)] = 25;
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{packed_values.clone(rows_1.front())};
            packed_values.storage_type[packed_values.index(row_0)] =
                make_test_type_ref(context, "int32");
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.type[packed_fields.index(row_0)] =
                        make_test_type_ref(context, "int32");
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.kind[packed_fields.index(row_0)] =
                        PackedFieldKind::signed_integer;
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.type[packed_fields.index(row_0)] =
                        make_test_type_ref(context, "int32");
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
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.kind[packed_fields.index(row_0)] =
                        PackedFieldKind::signed_integer;
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
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.bits[packed_fields.index(row_0)].reset();
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.type[packed_fields.index(row_0)] =
                        make_test_type_ref(context, "int16");
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
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.kind[packed_fields.index(row_0)] =
                        PackedFieldKind::signed_integer;
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
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.bits[packed_fields.index(row_0)] = 17;
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_2{std::vector(original_items.begin(), original_items.end())};
        rows_2.front() = [&] {
            auto row_1{packed_values.clone(rows_2.front())};
            auto const row_1_row{packed_values.index(row_1)};
            packed_values.segments[row_1_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_1_row])};
                auto rows_0{std::vector(original_items.begin(), original_items.end())};
                rows_0.front() = [&] {
                    auto const new_packed_fields{packed_fields.append()};
                    auto const new_packed_fields_row{packed_fields.index(new_packed_fields)};
                    packed_fields.name[new_packed_fields_row] = strings.intern("flag");
                    packed_fields.type[new_packed_fields_row] = make_test_type_ref(context, "bool");
                    packed_fields.bits[new_packed_fields_row] = 2;
                    return new_packed_fields;
                }();
                return declarations.range(rows_0);
            }();
            return row_1;
        }();
        return declarations.range(rows_2);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    normal_modules.settings[normal_modules.index(module)] = [&] {
        auto row_0{declarations.module_settings_store.clone(
            normal_modules.settings[normal_modules.index(module)])};
        declarations.module_settings_store.source[declarations.module_settings_store.index(row_0)] =
            strings.intern("Packed.cpp");
        return row_0;
    }();
    EXPECT_EQ(
        render_test_manifest(
            context,
            Manifest{manifest_schema_version, make_test_registry(context), declarations, {module}})
            .size(),
        2U);

    module = valid_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{packed_fields.clone(rows_1[1])};
                    packed_fields.range_helper[packed_fields.index(row_0)] = true;
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{packed_values.clone(rows_1.front())};
            packed_values.invalid_value[packed_values.index(row_0)] = std::uint64_t{1} << 32;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{packed_values.clone(rows_1.front())};
            packed_values.segments[packed_values.index(row_0)] =
                declarations.range<PackedSegmentId>({[&] {
                    auto const new_packed_reserved_bits{packed_reserved_bits.append()};
                    auto const new_packed_reserved_bits_row{
                        packed_reserved_bits.index(new_packed_reserved_bits)};
                    packed_reserved_bits.name[new_packed_reserved_bits_row] =
                        strings.intern("future");
                    packed_reserved_bits.bits[new_packed_reserved_bits_row] = 32;
                    return new_packed_reserved_bits;
                }()});
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{packed_values.clone(rows_1.front())};
            auto const row_0_row{packed_values.index(row_0)};
            packed_values.segments[row_0_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_0_row])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.insert(rows.begin() + 1, [&] {
                    auto const new_packed_reserved_bits{packed_reserved_bits.append()};
                    auto const new_packed_reserved_bits_row{
                        packed_reserved_bits.index(new_packed_reserved_bits)};
                    packed_reserved_bits.name[new_packed_reserved_bits_row] =
                        strings.intern("future");
                    packed_reserved_bits.bits[new_packed_reserved_bits_row] = 0;
                    return new_packed_reserved_bits;
                }());
                return declarations.range(rows);
            }();
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{packed_values.clone(rows_1.front())};
            auto const row_0_row{packed_values.index(row_0)};
            packed_values.segments[row_0_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_0_row])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.insert(rows.begin() + 1, [&] {
                    auto const new_packed_reserved_bits{packed_reserved_bits.append()};
                    auto const new_packed_reserved_bits_row{
                        packed_reserved_bits.index(new_packed_reserved_bits)};
                    packed_reserved_bits.name[new_packed_reserved_bits_row] =
                        strings.intern("entity_index");
                    packed_reserved_bits.bits[new_packed_reserved_bits_row] = 1;
                    return new_packed_reserved_bits;
                }());
                return declarations.range(rows);
            }();
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.minimum_value[packed_fields.index(row_0)] = 1;
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.bits[packed_fields.index(row_0)].reset();
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{packed_fields.clone(rows_1[1])};
                    packed_fields.bits[packed_fields.index(row_0)].reset();
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.minimum_value[packed_fields.index(row_0)] = 100;
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
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.maximum_value[packed_fields.index(row_0)] = 10;
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.minimum_value[packed_fields.index(row_0)] = -1;
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
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.maximum_value[packed_fields.index(row_0)] = 100;
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.type[packed_fields.index(row_0)] =
                        make_test_type_ref(context, "std::int32_t");
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
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.kind[packed_fields.index(row_0)] =
                        PackedFieldKind::signed_integer;
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
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.bits[packed_fields.index(row_0)] = 7;
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
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.minimum_value[packed_fields.index(row_0)] = -100;
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
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.maximum_value[packed_fields.index(row_0)] = 100;
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.type[packed_fields.index(row_0)] =
                        make_test_type_ref(context, "std::int32_t");
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
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.kind[packed_fields.index(row_0)] =
                        PackedFieldKind::signed_integer;
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
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.bits[packed_fields.index(row_0)] = 8;
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
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.minimum_value[packed_fields.index(row_0)] = -100;
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
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.maximum_value[packed_fields.index(row_0)] = 100;
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
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.named_codes[packed_fields.index(row_0)] =
                        declarations.range<PackedNamedCodeId>({[&] {
                            auto const new_packed_named_codes{packed_named_codes.append()};
                            auto const new_packed_named_codes_row{
                                packed_named_codes.index(new_packed_named_codes)};
                            packed_named_codes.name[new_packed_named_codes_row] =
                                strings.intern("Invalid");
                            packed_named_codes.value[new_packed_named_codes_row] = 0;
                            packed_named_codes.sentinel[new_packed_named_codes_row] = true;
                            return new_packed_named_codes;
                        }()});
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.minimum_value[packed_fields.index(row_0)] = 0;
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
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.maximum_value[packed_fields.index(row_0)] = std::uint64_t{1}
                                                                           << 24;
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{packed_fields.clone(rows_1[1])};
                    packed_fields.minimum_value[packed_fields.index(row_0)] = 0;
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
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{packed_fields.clone(rows_1[1])};
                    packed_fields.maximum_value[packed_fields.index(row_0)] = 3;
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.named_codes[packed_fields.index(row_0)] =
                        declarations.range<PackedNamedCodeId>({[&] {
                            auto const new_packed_named_codes{packed_named_codes.append()};
                            auto const new_packed_named_codes_row{
                                packed_named_codes.index(new_packed_named_codes)};
                            packed_named_codes.name[new_packed_named_codes_row] =
                                strings.intern("Invalid");
                            packed_named_codes.value[new_packed_named_codes_row] = 0xffffff;
                            packed_named_codes.sentinel[new_packed_named_codes_row] = true;
                            return new_packed_named_codes;
                        }()});
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.minimum_value[packed_fields.index(row_0)] = 0;
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
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.maximum_value[packed_fields.index(row_0)] = 100;
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
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.named_codes[packed_fields.index(row_0)] =
                        declarations.range<PackedNamedCodeId>({[&] {
                            auto const new_packed_named_codes{packed_named_codes.append()};
                            auto const new_packed_named_codes_row{
                                packed_named_codes.index(new_packed_named_codes)};
                            packed_named_codes.name[new_packed_named_codes_row] =
                                strings.intern("Invalid");
                            packed_named_codes.value[new_packed_named_codes_row] = 100;
                            packed_named_codes.sentinel[new_packed_named_codes_row] = true;
                            return new_packed_named_codes;
                        }()});
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.minimum_value[packed_fields.index(row_0)] = 0;
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
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.maximum_value[packed_fields.index(row_0)] = 100;
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
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.named_codes[packed_fields.index(row_0)] =
                        declarations.range<PackedNamedCodeId>({[&] {
                            auto const new_packed_named_codes{packed_named_codes.append()};
                            auto const new_packed_named_codes_row{
                                packed_named_codes.index(new_packed_named_codes)};
                            packed_named_codes.name[new_packed_named_codes_row] =
                                strings.intern("Named");
                            packed_named_codes.value[new_packed_named_codes_row] = 101;
                            packed_named_codes.sentinel[new_packed_named_codes_row] = false;
                            return new_packed_named_codes;
                        }()});
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.minimum_value[packed_fields.index(row_0)] = 0;
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
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.maximum_value[packed_fields.index(row_0)] = 100;
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
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.named_codes[packed_fields.index(row_0)] =
                        declarations.range<PackedNamedCodeId>({[&] {
                            auto const new_packed_named_codes{packed_named_codes.append()};
                            auto const new_packed_named_codes_row{
                                packed_named_codes.index(new_packed_named_codes)};
                            packed_named_codes.name[new_packed_named_codes_row] =
                                strings.intern("Invalid");
                            packed_named_codes.value[new_packed_named_codes_row] = 0x1000000;
                            packed_named_codes.sentinel[new_packed_named_codes_row] = true;
                            return new_packed_named_codes;
                        }()});
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.minimum_value[packed_fields.index(row_0)] = 0;
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
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.maximum_value[packed_fields.index(row_0)] = 100;
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
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.named_codes[packed_fields.index(row_0)] =
                        declarations.range<PackedNamedCodeId>(
                            {[&] {
                                 auto const new_packed_named_codes{packed_named_codes.append()};
                                 auto const new_packed_named_codes_row{
                                     packed_named_codes.index(new_packed_named_codes)};
                                 packed_named_codes.name[new_packed_named_codes_row] =
                                     strings.intern("Invalid");
                                 packed_named_codes.value[new_packed_named_codes_row] = 0xffffff;
                                 packed_named_codes
                                     .sentinel[packed_named_codes.index(new_packed_named_codes)] =
                                     true;
                                 return new_packed_named_codes;
                             }(),
                             [&] {
                                 auto const new_packed_named_codes{packed_named_codes.append()};
                                 auto const new_packed_named_codes_row{
                                     packed_named_codes.index(new_packed_named_codes)};
                                 packed_named_codes.name[new_packed_named_codes_row] =
                                     strings.intern("Invalid");
                                 packed_named_codes.value[new_packed_named_codes_row] = 0xfffffe;
                                 packed_named_codes
                                     .sentinel[packed_named_codes.index(new_packed_named_codes)] =
                                     true;
                                 return new_packed_named_codes;
                             }()});
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.minimum_value[packed_fields.index(row_0)] = 0;
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
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.maximum_value[packed_fields.index(row_0)] = 100;
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
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.named_codes[packed_fields.index(row_0)] =
                        declarations.range<PackedNamedCodeId>(
                            {[&] {
                                 auto const new_packed_named_codes{packed_named_codes.append()};
                                 auto const new_packed_named_codes_row{
                                     packed_named_codes.index(new_packed_named_codes)};
                                 packed_named_codes.name[new_packed_named_codes_row] =
                                     strings.intern("Invalid");
                                 packed_named_codes.value[new_packed_named_codes_row] = 0xffffff;
                                 packed_named_codes
                                     .sentinel[packed_named_codes.index(new_packed_named_codes)] =
                                     true;
                                 return new_packed_named_codes;
                             }(),
                             [&] {
                                 auto const new_packed_named_codes{packed_named_codes.append()};
                                 auto const new_packed_named_codes_row{
                                     packed_named_codes.index(new_packed_named_codes)};
                                 packed_named_codes.name[new_packed_named_codes_row] =
                                     strings.intern("Pending");
                                 packed_named_codes.value[new_packed_named_codes_row] = 0xffffff;
                                 packed_named_codes
                                     .sentinel[packed_named_codes.index(new_packed_named_codes)] =
                                     true;
                                 return new_packed_named_codes;
                             }()});
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.relationship[packed_fields.index(row_0)] = [&] {
                        auto const new_semantic_relations{semantic_relations.append()};
                        auto const new_semantic_relations_row{
                            semantic_relations.index(new_semantic_relations)};
                        semantic_relations.kind[new_semantic_relations_row] =
                            SemanticRelationKind::discriminates;
                        semantic_relations.target[new_semantic_relations_row] =
                            make_test_type_ref(context, "ExternalPayload");
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
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{packed_fields.clone(rows_1[1])};
                    packed_fields.relationship[packed_fields.index(row_0)] = [&] {
                        auto const new_semantic_relations{semantic_relations.append()};
                        auto const new_semantic_relations_row{
                            semantic_relations.index(new_semantic_relations)};
                        semantic_relations.kind[new_semantic_relations_row] =
                            SemanticRelationKind::index_into;
                        semantic_relations.target[new_semantic_relations_row] =
                            make_test_type_ref(context, "ExternalTable");
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
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    normal_modules.declarations[normal_modules.index(module)] = [&] {
        auto const original_items{
            declarations.items(normal_modules.declarations[normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.relationship[packed_fields.index(row_0)] = [&] {
                        auto const new_semantic_relations{semantic_relations.append()};
                        auto const new_semantic_relations_row{
                            semantic_relations.index(new_semantic_relations)};
                        semantic_relations.kind[new_semantic_relations_row] =
                            SemanticRelationKind::references;
                        semantic_relations.target[new_semantic_relations_row] =
                            make_test_type_ref(context, "TypeDefinitionId");
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
    EXPECT_THROW(lower(context, module), std::invalid_argument);
}

TEST(PackedValue, RejectsGeneratedApiCollisions) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& packed_values{declarations.packed_values};
    auto module{normal_modules.clone(valid_module(context))};
    auto const module_row{normal_modules.index(module)};
    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{packed_values.clone(rows_1.front())};
            packed_values.mutable_value[packed_values.index(row_0)] = true;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{declarations.packed_fields.clone(rows_1[1])};
                    declarations.packed_fields.name[declarations.packed_fields.index(row_0)] =
                        context.storage.strings.intern("set_entity_index");
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);
}

TEST(PackedValue, RejectsDescriptorAndRawFactoryNameCollisions) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& packed_values{declarations.packed_values};
    auto& packed_fields{declarations.packed_fields};
    auto& strings{context.storage.strings};
    auto module{normal_modules.clone(valid_module(context))};
    auto const module_row{normal_modules.index(module)};

    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{packed_fields.clone(rows_1[1])};
                    packed_fields.name[packed_fields.index(row_0)] =
                        strings.intern("entity_index_field");
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);
    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{packed_fields.clone(rows_1[1])};
                    packed_fields.name[packed_fields.index(row_0)] = strings.intern("from_raw");
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);
    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{packed_fields.clone(rows_1[1])};
                    packed_fields.name[packed_fields.index(row_0)] =
                        strings.intern("entity_index_minimum");
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);
    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{packed_fields.clone(rows_1[1])};
                    packed_fields.name[packed_fields.index(row_0)] =
                        strings.intern("entity_index_maximum");
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);
}

TEST(PackedValue, ValidatesKnownEnumUnderlyingType) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& packed_values{declarations.packed_values};

    auto& module_settings_store{declarations.module_settings_store};
    auto& enums_schemas{declarations.enums};
    auto& strings{context.storage.strings};
    auto module{normal_modules.clone(valid_module(context))};
    auto const module_row{normal_modules.index(module)};
    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{declarations.packed_fields.clone(rows_1[1])};
                    declarations.packed_fields.type[declarations.packed_fields.index(row_0)] =
                        make_test_type_ref(context, "@state");
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    auto const enums{normal_modules.append()};
    auto const enums_row{normal_modules.index(enums)};
    normal_modules.settings[enums_row] = [&] {
        auto const new_module_settings_store{module_settings_store.append()};
        auto const new_module_settings_store_row{
            module_settings_store.index(new_module_settings_store)};
        module_settings_store.name[new_module_settings_store_row] = strings.intern("enums");
        module_settings_store.header[new_module_settings_store_row] = strings.intern("Enums.h");
        return new_module_settings_store;
    }();
    normal_modules.declarations[enums_row] = declarations.range<DeclarationHandle>({[&] {
        auto const new_enums{enums_schemas.append()};
        auto const new_enums_row{enums_schemas.index(new_enums)};
        enums_schemas.name[new_enums_row] = strings.intern("FighterStateKind");
        enums_schemas.underlying_type[new_enums_row] = make_test_type_ref(context, "int8");
        enums_schemas.values[new_enums_row] = declarations.range<EnumeratorId>({[&] {
            auto const new_enumerators{declarations.enumerators.append()};
            declarations.enumerators.name[declarations.enumerators.index(new_enumerators)] =
                strings.intern("Value");
            return new_enumerators;
        }()});
        return new_enums;
    }()});
    Manifest manifest{
        manifest_schema_version,
        make_test_registry(context, {{"state", context.storage.cpp_types.add("FighterStateKind")}}),
        declarations,
        {enums, module}};

    try {
        static_cast<void>(lower_test_manifest(context, manifest));
        FAIL() << "Expected validation error";
    } catch (std::invalid_argument const& error) {
        EXPECT_NE(std::string{error.what()}.find(
                      "enum must have an unsigned fixed-width underlying type"),
                  std::string::npos);
    }
}

TEST(PackedValue, ValidatesKnownEnumEncodedWidthWithoutEmittingPerValueAssertions) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& packed_values{declarations.packed_values};
    auto& packed_fields{declarations.packed_fields};
    auto& enums{declarations.enums};
    auto& enumerators{declarations.enumerators};
    auto& strings{context.storage.strings};
    auto module{normal_modules.clone(valid_module(context))};
    auto const module_row{normal_modules.index(module)};
    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{packed_values.clone(rows_1.front())};
            packed_values.invalid_value[packed_values.index(row_0)] = 0x07ffffffu;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{packed_fields.clone(rows_1[1])};
                    packed_fields.bits[packed_fields.index(row_0)] = 4;
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    auto schema{[&] {
        auto const new_enums{enums.append()};
        auto const new_enums_row{enums.index(new_enums)};
        enums.name[new_enums_row] = strings.intern("FighterStateKind");
        enums.underlying_type[new_enums_row] = make_test_type_ref(context, "uint8");
        enums.values[new_enums_row] = declarations.range<EnumeratorId>(
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
                 enumerators.name[new_enumerators_row] = strings.intern("High");
                 enumerators.initializer[new_enumerators_row] = strings.intern("7");
                 return new_enumerators;
             }(),
             [&] {
                 auto const new_enumerators{enumerators.append()};
                 auto const new_enumerators_row{enumerators.index(new_enumerators)};
                 enumerators.name[new_enumerators_row] = strings.intern("COUNT");
                 enumerators.initializer[new_enumerators_row] = strings.intern("8");
                 enumerators.display_name[new_enumerators_row] = std::nullopt;
                 enumerators.hidden[new_enumerators_row] = true;
                 return new_enumerators;
             }()});
        enums.count[new_enums_row] = strings.intern("COUNT");
        return new_enums;
    }()};

    auto const header{lower_known_enum(context, module, schema)};
    EXPECT_EQ(header.find("static_assert(static_cast<state_underlying_type>"), std::string::npos);
    EXPECT_EQ(header.find("static_assert(state"), std::string::npos);
    EXPECT_EQ(header.find("assert(static_cast<state_underlying_type>(state_value)"),
              std::string::npos);
    EXPECT_NE(header.find("assert(raw != invalid_value);"), std::string::npos);

    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{packed_fields.clone(rows_1[1])};
                    packed_fields.bits[packed_fields.index(row_0)] = 3;
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_known_enum(context, module, schema), std::invalid_argument);
}

TEST(PackedValue, OmitsSentinelAssertionWhenTheKnownEnumDomainExcludesIt) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& enums{declarations.enums};
    auto& enumerators{declarations.enumerators};
    auto& strings{context.storage.strings};
    auto module{normal_modules.clone(valid_module(context))};
    auto const module_row{normal_modules.index(module)};
    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{declarations.packed_values.clone(rows_1.front())};
            declarations.packed_values.invalid_value[declarations.packed_values.index(row_0)] =
                0xffffffffu;
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    auto const header{lower_known_enum(context, module, [&] {
        auto const new_enums{enums.append()};
        auto const new_enums_row{enums.index(new_enums)};
        enums.name[new_enums_row] = strings.intern("FighterStateKind");
        enums.underlying_type[new_enums_row] = make_test_type_ref(context, "uint8");
        enums.values[new_enums_row] = declarations.range<EnumeratorId>(
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
                 enumerators.name[new_enumerators_row] = strings.intern("COUNT");
                 enumerators.initializer[new_enumerators_row] = strings.intern("5");
                 enumerators.display_name[new_enumerators_row] = std::nullopt;
                 enumerators.hidden[new_enumerators_row] = true;
                 return new_enumerators;
             }()});
        enums.count[new_enums_row] = strings.intern("COUNT");
        return new_enums;
    }())};

    EXPECT_EQ(header.find("assert(raw != invalid_value);"), std::string::npos);
}

TEST(PackedValue, ResolvesImplicitValuesAfterExplicitHexadecimalValues) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& packed_values{declarations.packed_values};
    auto& packed_fields{declarations.packed_fields};
    auto& enums{declarations.enums};
    auto& enumerators{declarations.enumerators};
    auto& strings{context.storage.strings};
    auto module{normal_modules.clone(valid_module(context))};
    auto const module_row{normal_modules.index(module)};
    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{packed_fields.clone(rows_1[1])};
                    packed_fields.bits[packed_fields.index(row_0)] = 4;
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    auto schema{[&] {
        auto const new_enums{enums.append()};
        auto const new_enums_row{enums.index(new_enums)};
        enums.name[new_enums_row] = strings.intern("FighterStateKind");
        enums.underlying_type[new_enums_row] = make_test_type_ref(context, "uint8");
        enums.values[new_enums_row] = declarations.range<EnumeratorId>(
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
                 enumerators.name[new_enumerators_row] = strings.intern("High");
                 enumerators.initializer[new_enumerators_row] = strings.intern("0x7");
                 return new_enumerators;
             }(),
             [&] {
                 auto const new_enumerators{enumerators.append()};
                 enumerators.name[enumerators.index(new_enumerators)] = strings.intern("Next");
                 return new_enumerators;
             }(),
             [&] {
                 auto const new_enumerators{enumerators.append()};
                 auto const new_enumerators_row{enumerators.index(new_enumerators)};
                 enumerators.name[new_enumerators_row] = strings.intern("COUNT");
                 enumerators.initializer[new_enumerators_row] = std::nullopt;
                 enumerators.display_name[new_enumerators_row] = std::nullopt;
                 enumerators.hidden[new_enumerators_row] = true;
                 return new_enumerators;
             }()});
        enums.count[new_enums_row] = strings.intern("COUNT");
        return new_enums;
    }()};

    EXPECT_NO_THROW(static_cast<void>(lower_known_enum(context, module, schema)));

    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{packed_fields.clone(rows_1[1])};
                    packed_fields.bits[packed_fields.index(row_0)] = 3;
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_known_enum(context, module, schema), std::invalid_argument);
}

TEST(PackedValue, RejectsKnownEnumValuesOutsideTheirUnderlyingType) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& packed_values{declarations.packed_values};
    auto& enums{declarations.enums};
    auto& enumerators{declarations.enumerators};
    auto& strings{context.storage.strings};
    auto module{normal_modules.clone(valid_module(context))};
    auto const module_row{normal_modules.index(module)};
    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{declarations.packed_fields.clone(rows_1[1])};
                    declarations.packed_fields.bits[declarations.packed_fields.index(row_0)] = 8;
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    auto schema{[&] {
        auto const new_enums{enums.append()};
        auto const new_enums_row{enums.index(new_enums)};
        enums.name[new_enums_row] = strings.intern("FighterStateKind");
        enums.underlying_type[new_enums_row] = make_test_type_ref(context, "uint8");
        enums.values[new_enums_row] = declarations.range<EnumeratorId>(
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
                 enumerators.name[new_enumerators_row] = strings.intern("Maximum");
                 enumerators.initializer[new_enumerators_row] = strings.intern("255");
                 return new_enumerators;
             }()});
        return new_enums;
    }()};
    EXPECT_NO_THROW(static_cast<void>(lower_known_enum(context, module, schema)));

    enums.values[enums.index(schema)] = [&] {
        auto const original_items{declarations.items(enums.values[enums.index(schema)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.back() = [&] {
            auto row_0{enumerators.clone(rows_1.back())};
            enumerators.initializer[enumerators.index(row_0)] = strings.intern("256");
            return row_0;
        }();
        return declarations.range(rows_1);
    }();
    EXPECT_THROW(lower_known_enum(context, module, schema), std::invalid_argument);
}

TEST(PackedValue, AllowsEnumAliasesAndUsesStructuralCountSemantics) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& packed_values{declarations.packed_values};
    auto& enums{declarations.enums};
    auto& enumerators{declarations.enumerators};
    auto& strings{context.storage.strings};
    auto module{normal_modules.clone(valid_module(context))};
    auto const module_row{normal_modules.index(module)};
    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{declarations.packed_fields.clone(rows_1[1])};
                    declarations.packed_fields.bits[declarations.packed_fields.index(row_0)] = 8;
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    auto schema{[&] {
        auto const new_enums{enums.append()};
        auto const new_enums_row{enums.index(new_enums)};
        enums.name[new_enums_row] = strings.intern("FighterStateKind");
        enums.underlying_type[new_enums_row] = make_test_type_ref(context, "uint8");
        enums.values[new_enums_row] = declarations.range<EnumeratorId>(
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
                 enumerators.name[new_enumerators_row] = strings.intern("Alias");
                 enumerators.initializer[new_enumerators_row] = strings.intern("0");
                 return new_enumerators;
             }()});
        return new_enums;
    }()};
    EXPECT_NO_THROW(static_cast<void>(lower_known_enum(context, module, schema)));

    enums.values[enums.index(schema)] = declarations.range<EnumeratorId>(
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
             enumerators.name[new_enumerators_row] = strings.intern("High");
             enumerators.initializer[new_enumerators_row] = strings.intern("7");
             return new_enumerators;
         }(),
         [&] {
             auto const new_enumerators{enumerators.append()};
             auto const new_enumerators_row{enumerators.index(new_enumerators)};
             enumerators.name[new_enumerators_row] = strings.intern("COUNT");
             enumerators.initializer[new_enumerators_row] = strings.intern("6");
             enumerators.display_name[new_enumerators_row] = std::nullopt;
             enumerators.hidden[new_enumerators_row] = true;
             return new_enumerators;
         }()});
    enums.count[enums.index(schema)] = strings.intern("COUNT");
    EXPECT_NO_THROW(static_cast<void>(lower_known_enum(context, module, schema)));
}

TEST(PackedValue, EmitsCompactFallbackForOpaqueKnownEnumValues) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& packed_values{declarations.packed_values};
    auto& enums{declarations.enums};
    auto& enumerators{declarations.enumerators};
    auto& strings{context.storage.strings};
    auto module{normal_modules.clone(valid_module(context))};
    auto const module_row{normal_modules.index(module)};
    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{declarations.packed_fields.clone(rows_1[1])};
                    declarations.packed_fields.bits[declarations.packed_fields.index(row_0)] = 3;
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    auto const header{lower_known_enum(context, module, [&] {
        auto const new_enums{enums.append()};
        auto const new_enums_row{enums.index(new_enums)};
        enums.name[new_enums_row] = strings.intern("FighterStateKind");
        enums.underlying_type[new_enums_row] = make_test_type_ref(context, "uint8");
        enums.values[new_enums_row] = declarations.range<EnumeratorId>(
            {[&] {
                 auto const new_enumerators{enumerators.append()};
                 auto const new_enumerators_row{enumerators.index(new_enumerators)};
                 enumerators.name[new_enumerators_row] = strings.intern("Zero");
                 enumerators.initializer[new_enumerators_row] =
                     strings.intern("static_cast<uint8>(0)");
                 return new_enumerators;
             }(),
             [&] {
                 auto const new_enumerators{enumerators.append()};
                 auto const new_enumerators_row{enumerators.index(new_enumerators)};
                 enumerators.name[new_enumerators_row] = strings.intern("High");
                 enumerators.initializer[new_enumerators_row] =
                     strings.intern("static_cast<uint8>(7)");
                 return new_enumerators;
             }()});
        return new_enums;
    }())};

    EXPECT_NE(header.find("static_assert([]<auto... values>() consteval -> bool"),
              std::string::npos);
    EXPECT_NE(header.find("operator()<"), std::string::npos);
    EXPECT_EQ(header.find("static_assert(static_cast<state_underlying_type>"), std::string::npos);
}

TEST(PackedValue, RejectsFieldsNarrowerThanEnumSemanticDomain) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& packed_values{declarations.packed_values};
    auto& module_settings_store{declarations.module_settings_store};
    auto& enums_schemas{declarations.enums};
    auto& enumerators{declarations.enumerators};
    auto& strings{context.storage.strings};
    auto manifest = [&](std::optional<std::uint32_t> const bit_width,
                        std::string maximum,
                        std::optional<bool> const signedness = std::nullopt) -> Manifest {
        auto module{normal_modules.clone(valid_module(context))};
        auto const module_row{normal_modules.index(module)};
        normal_modules.declarations[module_row] = [&] {
            auto const original_items{declarations.items(normal_modules.declarations[module_row])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{packed_values.clone(rows_3.front())};
                auto const row_2_row{packed_values.index(row_2)};
                packed_values.segments[row_2_row] = [&] {
                    auto const original_items{
                        declarations.items(packed_values.segments[row_2_row])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[1] = [&] {
                        auto row_0{declarations.packed_fields.clone(rows_1[1])};
                        declarations.packed_fields.bits[declarations.packed_fields.index(row_0)] =
                            2;
                        return row_0;
                    }();
                    return declarations.range(rows_1);
                }();
                return row_2;
            }();
            return declarations.range(rows_3);
        }();
        auto const enums{normal_modules.append()};
        auto const enums_row{normal_modules.index(enums)};
        normal_modules.settings[enums_row] = [&] {
            auto const new_module_settings_store{module_settings_store.append()};
            auto const new_module_settings_store_row{
                module_settings_store.index(new_module_settings_store)};
            module_settings_store.name[new_module_settings_store_row] = strings.intern("enums");
            module_settings_store.header[new_module_settings_store_row] = strings.intern("Enums.h");
            return new_module_settings_store;
        }();
        normal_modules.declarations[enums_row] = declarations.range<DeclarationHandle>({[&] {
            auto const new_enums{enums_schemas.append()};
            auto const new_enums_row{enums_schemas.index(new_enums)};
            enums_schemas.name[new_enums_row] = strings.intern("FighterStateKind");
            enums_schemas.underlying_type[new_enums_row] = make_test_type_ref(context, "uint8");
            enums_schemas.values[new_enums_row] = declarations.range<EnumeratorId>(
                {[&] {
                     auto const new_enumerators{enumerators.append()};
                     auto const new_enumerators_row{enumerators.index(new_enumerators)};
                     enumerators.name[new_enumerators_row] = strings.intern("Idle");
                     enumerators.initializer[new_enumerators_row] = strings.intern("0");
                     return new_enumerators;
                 }(),
                 [&] {
                     auto const new_enumerators{enumerators.append()};
                     auto const new_enumerators_row{enumerators.index(new_enumerators)};
                     enumerators.name[new_enumerators_row] = strings.intern("Maximum");
                     enumerators.initializer[new_enumerators_row] =
                         strings.intern(std::move(maximum));
                     return new_enumerators;
                 }()});
            enums_schemas.bit_width[new_enums_row] = bit_width;
            enums_schemas.signedness[new_enums_row] =
                signedness ? (*signedness ? schema::EnumSignedness::signed_
                                          : schema::EnumSignedness::unsigned_)
                           : schema::EnumSignedness::unspecified;
            return new_enums;
        }()});
        return {
            manifest_schema_version, make_test_registry(context), declarations, {enums, module}};
    };

    try {
        static_cast<void>(lower_test_manifest(context, manifest(3, "1")));
        FAIL() << "Expected validation error";
    } catch (std::invalid_argument const& error) {
        EXPECT_NE(std::string{error.what()}.find("smaller than the enum's 3-bit semantic domain"),
                  std::string::npos);
    }

    EXPECT_THROW(static_cast<void>(lower_test_manifest(context, manifest(std::nullopt, "7"))),
                 std::invalid_argument);
    EXPECT_THROW(static_cast<void>(lower_test_manifest(context, manifest(std::nullopt, "3", true))),
                 std::invalid_argument);
}

TEST(PackedValue, DerivesAutoWidthFromEnumSemanticDomain) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& packed_values{declarations.packed_values};
    auto& packed_fields{declarations.packed_fields};
    auto& semantic_relations{declarations.semantic_relations};

    auto& module_settings_store{declarations.module_settings_store};
    auto& enums_schemas{declarations.enums};
    auto& enumerators{declarations.enumerators};
    auto& strings{context.storage.strings};
    auto module{normal_modules.clone(valid_module(context))};
    auto const module_row{normal_modules.index(module)};
    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{packed_fields.clone(rows_1[1])};
                    packed_fields.bits[packed_fields.index(row_0)].reset();
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.relationship[packed_fields.index(row_0)] = [&] {
                        auto const new_semantic_relations{semantic_relations.append()};
                        auto const new_semantic_relations_row{
                            semantic_relations.index(new_semantic_relations)};
                        semantic_relations.kind[new_semantic_relations_row] =
                            SemanticRelationKind::index_into;
                        semantic_relations.target[new_semantic_relations_row] =
                            make_test_type_ref(context, "FighterStateKind");
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
    auto const enums{normal_modules.append()};
    auto const enums_row{normal_modules.index(enums)};
    normal_modules.settings[enums_row] = [&] {
        auto const new_module_settings_store{module_settings_store.append()};
        auto const new_module_settings_store_row{
            module_settings_store.index(new_module_settings_store)};
        module_settings_store.name[new_module_settings_store_row] = strings.intern("enums");
        module_settings_store.header[new_module_settings_store_row] = strings.intern("Enums.h");
        return new_module_settings_store;
    }();
    normal_modules.declarations[enums_row] = declarations.range<DeclarationHandle>({[&] {
        auto const new_enums{enums_schemas.append()};
        auto const new_enums_row{enums_schemas.index(new_enums)};
        enums_schemas.name[new_enums_row] = strings.intern("FighterStateKind");
        enums_schemas.underlying_type[new_enums_row] = make_test_type_ref(context, "uint8");
        enums_schemas.values[new_enums_row] = declarations.range<EnumeratorId>(
            {[&] {
                 auto const new_enumerators{enumerators.append()};
                 auto const new_enumerators_row{enumerators.index(new_enumerators)};
                 enumerators.name[new_enumerators_row] = strings.intern("Idle");
                 enumerators.initializer[new_enumerators_row] = strings.intern("0");
                 return new_enumerators;
             }(),
             [&] {
                 auto const new_enumerators{enumerators.append()};
                 auto const new_enumerators_row{enumerators.index(new_enumerators)};
                 enumerators.name[new_enumerators_row] = strings.intern("Maximum");
                 enumerators.initializer[new_enumerators_row] = strings.intern("7");
                 return new_enumerators;
             }()});
        enums_schemas.bit_width[new_enums_row] = std::nullopt;
        enums_schemas.signedness[new_enums_row] = lispb::schema::EnumSignedness::unsigned_;
        return new_enums;
    }()});
    auto const files{render_test_manifest(
        context,
        Manifest{
            manifest_schema_version, make_test_registry(context), declarations, {enums, module}})};

    auto const generated{
        std::ranges::find(files, std::filesystem::path{"Packed.h"}, &GeneratedFile::path)};
    ASSERT_NE(generated, files.end());
    EXPECT_NE(generated->content.find("ioj::PackedField<storage_type, state_type, 24, 3>"),
              std::string::npos);
}

TEST(PackedValue, SentinelCodeCanIncreaseDerivedIntegerWidth) {
    CompilerTestContext context;
    auto& declarations{context.storage.declarations};
    auto& normal_modules{declarations.normal_modules};
    auto& packed_values{declarations.packed_values};
    auto& packed_fields{declarations.packed_fields};
    auto& packed_named_codes{declarations.packed_named_codes};
    auto module{normal_modules.clone(valid_module(context))};
    auto const module_row{normal_modules.index(module)};

    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.bits[packed_fields.index(row_0)].reset();
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.minimum_value[packed_fields.index(row_0)] = 0;
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.maximum_value[packed_fields.index(row_0)] = 7;
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();
    normal_modules.declarations[module_row] = [&] {
        auto const original_items{declarations.items(normal_modules.declarations[module_row])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{packed_values.clone(rows_3.front())};
            auto const row_2_row{packed_values.index(row_2)};
            packed_values.segments[row_2_row] = [&] {
                auto const original_items{declarations.items(packed_values.segments[row_2_row])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{packed_fields.clone(rows_1[0])};
                    packed_fields.named_codes[packed_fields.index(row_0)] =
                        declarations.range<PackedNamedCodeId>({[&] {
                            auto const new_packed_named_codes{packed_named_codes.append()};
                            auto const new_packed_named_codes_row{
                                packed_named_codes.index(new_packed_named_codes)};
                            packed_named_codes.name[new_packed_named_codes_row] =
                                context.storage.strings.intern("Invalid");
                            packed_named_codes.value[new_packed_named_codes_row] = 8;
                            packed_named_codes.sentinel[new_packed_named_codes_row] = true;
                            return new_packed_named_codes;
                        }()});
                    return row_0;
                }();
                return declarations.range(rows_1);
            }();
            return row_2;
        }();
        return declarations.range(rows_3);
    }();

    auto const header{lower(context, module)};

    EXPECT_NE(header.find("ioj::PackedField<storage_type, entity_index_type, 0, 4>"),
              std::string::npos);
    EXPECT_NE(header.find("entity_index_Invalid{static_cast<entity_index_type>(8)}"),
              std::string::npos);
}

} // namespace
} // namespace lispb::codegen
