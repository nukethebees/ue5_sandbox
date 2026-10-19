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
    return [&] {
        auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
        context.storage.declarations.normal_modules
            .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] = [&] {
            auto const new_module_settings_store{
                context.storage.declarations.module_settings_store.append()};
            context.storage.declarations.module_settings_store
                .name[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.intern("packed");
            context.storage.declarations.module_settings_store
                .header[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.add("Packed.h");
            return new_module_settings_store;
        }();
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(new_normal_modules)] =
            context.storage.declarations.range<DeclarationHandle>({[&] {
                auto const new_packed_values{context.storage.declarations.packed_values.append()};
                context.storage.declarations.packed_values
                    .name[context.storage.declarations.packed_values.index(new_packed_values)] =
                    context.storage.strings.intern("FighterState");
                context.storage.declarations.packed_values
                    .storage_type[context.storage.declarations.packed_values.index(
                        new_packed_values)] = [&] {
                    auto const new_type_refs{context.storage.declarations.type_refs.append()};
                    context.storage.declarations.type_refs
                        .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                        context.storage.strings.intern("std::uint32_t");
                    return new_type_refs;
                }();
                context.storage.declarations.packed_values
                    .segments[context.storage.declarations.packed_values.index(new_packed_values)] =
                    context.storage.declarations.range<PackedSegmentId>(
                        {[&] {
                             auto const new_packed_fields{
                                 context.storage.declarations.packed_fields.append()};
                             context.storage.declarations.packed_fields
                                 .name[context.storage.declarations.packed_fields.index(
                                     new_packed_fields)] =
                                 context.storage.strings.intern("entity_index");
                             context.storage.declarations.packed_fields
                                 .type[context.storage.declarations.packed_fields.index(
                                     new_packed_fields)] = [&] {
                                 auto const new_type_refs{
                                     context.storage.declarations.type_refs.append()};
                                 context.storage.declarations.type_refs
                                     .name[context.storage.declarations.type_refs.index(
                                         new_type_refs)] =
                                     context.storage.strings.intern("std::uint32_t");
                                 return new_type_refs;
                             }();
                             context.storage.declarations.packed_fields
                                 .bits[context.storage.declarations.packed_fields.index(
                                     new_packed_fields)] = 24;
                             return new_packed_fields;
                         }(),
                         [&] {
                             auto const new_packed_fields{
                                 context.storage.declarations.packed_fields.append()};
                             context.storage.declarations.packed_fields
                                 .name[context.storage.declarations.packed_fields.index(
                                     new_packed_fields)] = context.storage.strings.intern("state");
                             context.storage.declarations.packed_fields
                                 .type[context.storage.declarations.packed_fields.index(
                                     new_packed_fields)] = [&] {
                                 auto const new_type_refs{
                                     context.storage.declarations.type_refs.append()};
                                 context.storage.declarations.type_refs
                                     .name[context.storage.declarations.type_refs.index(
                                         new_type_refs)] =
                                     context.storage.strings.intern("FighterStateKind");
                                 return new_type_refs;
                             }();
                             context.storage.declarations.packed_fields
                                 .bits[context.storage.declarations.packed_fields.index(
                                     new_packed_fields)] = 8;
                             context.storage.declarations.packed_fields
                                 .kind[context.storage.declarations.packed_fields.index(
                                     new_packed_fields)] = PackedFieldKind::enumeration;
                             return new_packed_fields;
                         }()});
                return new_packed_values;
            }()});
        return new_normal_modules;
    }();
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
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[1])};
                    context.storage.declarations.packed_fields
                        .type[context.storage.declarations.packed_fields.index(row_0)] = [&] {
                        auto const new_type_refs{context.storage.declarations.type_refs.append()};
                        context.storage.declarations.type_refs
                            .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                            context.storage.strings.intern("@state");
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
    auto const files{render_test_manifest(
        context,
        Manifest{manifest_schema_version,
                 make_test_registry(context,
                                    {{"state", context.storage.cpp_types.add("FighterStateKind")}}),
                 context.storage.declarations,
                 {[&] {
                      auto const new_normal_modules{
                          context.storage.declarations.normal_modules.append()};
                      context.storage.declarations.normal_modules
                          .settings[context.storage.declarations.normal_modules.index(
                              new_normal_modules)] = [&] {
                          auto const new_module_settings_store{
                              context.storage.declarations.module_settings_store.append()};
                          context.storage.declarations.module_settings_store
                              .name[context.storage.declarations.module_settings_store.index(
                                  new_module_settings_store)] =
                              context.storage.strings.intern("enums");
                          context.storage.declarations.module_settings_store
                              .header[context.storage.declarations.module_settings_store.index(
                                  new_module_settings_store)] =
                              context.storage.strings.add("Enums.h");
                          return new_module_settings_store;
                      }();
                      context.storage.declarations.normal_modules
                          .declarations[context.storage.declarations.normal_modules.index(
                              new_normal_modules)] =
                          context.storage.declarations.range<DeclarationHandle>({schema});
                      return new_normal_modules;
                  }(),
                  module}})};
    EXPECT_EQ(files.size(), 2);
    return files.back().content;
}

auto scalar_backed_manifest(CompilerTestContext& context,
                            ModuleHandle module,
                            bool const signedness = false) -> Manifest {

    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .type[context.storage.declarations.packed_fields.index(row_0)] = [&] {
                        auto const new_type_refs{context.storage.declarations.type_refs.append()};
                        context.storage.declarations.type_refs
                            .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                            context.storage.strings.intern("project::Health");
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
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .kind[context.storage.declarations.packed_fields.index(row_0)] =
                        signedness ? PackedFieldKind::signed_integer
                                   : PackedFieldKind::unsigned_integer;
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
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .bits[context.storage.declarations.packed_fields.index(row_0)]
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
        .settings[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto row_0{context.storage.declarations.module_settings_store.clone(
            context.storage.declarations.normal_modules
                .settings[context.storage.declarations.normal_modules.index(module)])};
        context.storage.declarations.module_settings_store
            .namespace_name[context.storage.declarations.module_settings_store.index(row_0)] =
            context.storage.strings.intern("project");
        return row_0;
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.packed_values.clone(rows_1.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_0)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_0)])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.resize(1);
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
            auto row_0{context.storage.declarations.packed_values.clone(rows_1.front())};
            context.storage.declarations.packed_values
                .mutable_value[context.storage.declarations.packed_values.index(row_0)] = true;
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();

    return Manifest{
        manifest_schema_version,
        make_test_registry(context),
        context.storage.declarations,
        {[&] {
             auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
             context.storage.declarations.normal_modules
                 .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] =
                 [&] {
                     auto const new_module_settings_store{
                         context.storage.declarations.module_settings_store.append()};
                     context.storage.declarations.module_settings_store
                         .name[context.storage.declarations.module_settings_store.index(
                             new_module_settings_store)] =
                         context.storage.strings.intern("scalars");
                     context.storage.declarations.module_settings_store
                         .header[context.storage.declarations.module_settings_store.index(
                             new_module_settings_store)] = context.storage.strings.add("Scalars.h");
                     context.storage.declarations.module_settings_store
                         .namespace_name[context.storage.declarations.module_settings_store.index(
                             new_module_settings_store)] =
                         context.storage.strings.intern("project");
                     return new_module_settings_store;
                 }();
             context.storage.declarations.normal_modules
                 .declarations[context.storage.declarations.normal_modules.index(
                     new_normal_modules)] =
                 context.storage.declarations.range<DeclarationHandle>({[&] {
                     auto const new_integer_scalars{
                         context.storage.declarations.integer_scalars.append()};
                     context.storage.declarations.integer_scalars
                         .name[context.storage.declarations.integer_scalars.index(
                             new_integer_scalars)] = context.storage.strings.intern("Health");
                     context.storage.declarations.integer_scalars
                         .signedness[context.storage.declarations.integer_scalars.index(
                             new_integer_scalars)] = signedness;
                     context.storage.declarations.integer_scalars
                         .minimum_value[context.storage.declarations.integer_scalars.index(
                             new_integer_scalars)] =
                         signedness ? PackedIntegerValue{-100} : PackedIntegerValue{0};
                     context.storage.declarations.integer_scalars
                         .maximum_value[context.storage.declarations.integer_scalars.index(
                             new_integer_scalars)] =
                         signedness ? PackedIntegerValue{100} : PackedIntegerValue{1000};
                     context.storage.declarations.integer_scalars
                         .bit_width[context.storage.declarations.integer_scalars.index(
                             new_integer_scalars)] = std::nullopt;
                     context.storage.declarations.integer_scalars
                         .named_codes[context.storage.declarations.integer_scalars.index(
                             new_integer_scalars)] =
                         context.storage.declarations.range<PackedNamedCodeId>({[&] {
                             auto const new_packed_named_codes{
                                 context.storage.declarations.packed_named_codes.append()};
                             context.storage.declarations.packed_named_codes
                                 .name[context.storage.declarations.packed_named_codes.index(
                                     new_packed_named_codes)] =
                                 context.storage.strings.intern("Unknown");
                             context.storage.declarations.packed_named_codes
                                 .value[context.storage.declarations.packed_named_codes.index(
                                     new_packed_named_codes)] =
                                 signedness ? PackedIntegerValue{-128} : PackedIntegerValue{4095};
                             context.storage.declarations.packed_named_codes
                                 .sentinel[context.storage.declarations.packed_named_codes.index(
                                     new_packed_named_codes)] = true;
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
    return Manifest{
        manifest_schema_version,
        make_test_registry(context),
        context.storage.declarations,
        {[&] {
             auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
             context.storage.declarations.normal_modules
                 .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] =
                 [&] {
                     auto const new_module_settings_store{
                         context.storage.declarations.module_settings_store.append()};
                     context.storage.declarations.module_settings_store
                         .name[context.storage.declarations.module_settings_store.index(
                             new_module_settings_store)] =
                         context.storage.strings.intern("scalars");
                     context.storage.declarations.module_settings_store
                         .header[context.storage.declarations.module_settings_store.index(
                             new_module_settings_store)] = context.storage.strings.add("Scalars.h");
                     context.storage.declarations.module_settings_store
                         .namespace_name[context.storage.declarations.module_settings_store.index(
                             new_module_settings_store)] =
                         context.storage.strings.intern("project");
                     return new_module_settings_store;
                 }();
             context.storage.declarations.normal_modules
                 .declarations[context.storage.declarations.normal_modules.index(
                     new_normal_modules)] =
                 context.storage.declarations.range<DeclarationHandle>({[&] {
                     auto const new_integer_scalars{
                         context.storage.declarations.integer_scalars.append()};
                     context.storage.declarations.integer_scalars
                         .name[context.storage.declarations.integer_scalars.index(
                             new_integer_scalars)] = context.storage.strings.intern("Health");
                     context.storage.declarations.integer_scalars
                         .signedness[context.storage.declarations.integer_scalars.index(
                             new_integer_scalars)] = false;
                     context.storage.declarations.integer_scalars
                         .minimum_value[context.storage.declarations.integer_scalars.index(
                             new_integer_scalars)] = 0;
                     context.storage.declarations.integer_scalars
                         .maximum_value[context.storage.declarations.integer_scalars.index(
                             new_integer_scalars)] = 1000;
                     context.storage.declarations.integer_scalars
                         .bit_width[context.storage.declarations.integer_scalars.index(
                             new_integer_scalars)] = std::nullopt;
                     return new_integer_scalars;
                 }()});
             return new_normal_modules;
         }(),
         [&] {
             auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
             context.storage.declarations.normal_modules
                 .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] =
                 [&] {
                     auto const new_module_settings_store{
                         context.storage.declarations.module_settings_store.append()};
                     context.storage.declarations.module_settings_store
                         .name[context.storage.declarations.module_settings_store.index(
                             new_module_settings_store)] =
                         context.storage.strings.intern("representations");
                     context.storage.declarations.module_settings_store
                         .header[context.storage.declarations.module_settings_store.index(
                             new_module_settings_store)] =
                         context.storage.strings.add("Representations.h");
                     context.storage.declarations.module_settings_store
                         .namespace_name[context.storage.declarations.module_settings_store.index(
                             new_module_settings_store)] =
                         context.storage.strings.intern("project");
                     return new_module_settings_store;
                 }();
             context.storage.declarations.normal_modules
                 .declarations[context.storage.declarations.normal_modules.index(
                     new_normal_modules)] =
                 context.storage.declarations.range<DeclarationHandle>({[&] {
                     auto const new_linear_quantizeds{
                         context.storage.declarations.linear_quantizeds.append()};
                     context.storage.declarations.linear_quantizeds
                         .name[context.storage.declarations.linear_quantizeds.index(
                             new_linear_quantizeds)] = context.storage.strings.intern("HealthQ8");
                     context.storage.declarations.linear_quantizeds
                         .source[context.storage.declarations.linear_quantizeds.index(
                             new_linear_quantizeds)] = [&] {
                         auto const new_type_refs{context.storage.declarations.type_refs.append()};
                         context.storage.declarations.type_refs
                             .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                             context.storage.strings.intern("project::Health");
                         return new_type_refs;
                     }();
                     context.storage.declarations.linear_quantizeds
                         .bit_width[context.storage.declarations.linear_quantizeds.index(
                             new_linear_quantizeds)] = 8;
                     context.storage.declarations.linear_quantizeds
                         .reserved_codes[context.storage.declarations.linear_quantizeds.index(
                             new_linear_quantizeds)] = reserved_codes;
                     context.storage.declarations.linear_quantizeds
                         .clipping[context.storage.declarations.linear_quantizeds.index(
                             new_linear_quantizeds)] = QuantizationClipping::clamp;
                     return new_linear_quantizeds;
                 }()});
             return new_normal_modules;
         }(),
         [&] {
             auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
             context.storage.declarations.normal_modules
                 .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] =
                 [&] {
                     auto const new_module_settings_store{
                         context.storage.declarations.module_settings_store.append()};
                     context.storage.declarations.module_settings_store
                         .name[context.storage.declarations.module_settings_store.index(
                             new_module_settings_store)] = context.storage.strings.intern("packed");
                     context.storage.declarations.module_settings_store
                         .header[context.storage.declarations.module_settings_store.index(
                             new_module_settings_store)] = context.storage.strings.add("Packed.h");
                     context.storage.declarations.module_settings_store
                         .namespace_name[context.storage.declarations.module_settings_store.index(
                             new_module_settings_store)] =
                         context.storage.strings.intern("project");
                     return new_module_settings_store;
                 }();
             context.storage.declarations.normal_modules
                 .declarations[context.storage.declarations.normal_modules.index(
                     new_normal_modules)] =
                 context.storage.declarations.range<DeclarationHandle>({[&] {
                     auto const new_packed_values{
                         context.storage.declarations.packed_values.append()};
                     context.storage.declarations.packed_values
                         .name[context.storage.declarations.packed_values.index(
                             new_packed_values)] = context.storage.strings.intern("Vitals");
                     context.storage.declarations.packed_values
                         .storage_type[context.storage.declarations.packed_values.index(
                             new_packed_values)] = [&] {
                         auto const new_type_refs{context.storage.declarations.type_refs.append()};
                         context.storage.declarations.type_refs
                             .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                             context.storage.strings.intern("std::uint16_t");
                         return new_type_refs;
                     }();
                     context.storage.declarations.packed_values
                         .segments[context.storage.declarations.packed_values.index(
                             new_packed_values)] =
                         context.storage.declarations.range<PackedSegmentId>(
                             {[&] {
                                  auto const new_packed_fields{
                                      context.storage.declarations.packed_fields.append()};
                                  context.storage.declarations.packed_fields
                                      .name[context.storage.declarations.packed_fields.index(
                                          new_packed_fields)] =
                                      context.storage.strings.intern("health");
                                  context.storage.declarations.packed_fields
                                      .type[context.storage.declarations.packed_fields.index(
                                          new_packed_fields)] = [&] {
                                      auto const new_type_refs{
                                          context.storage.declarations.type_refs.append()};
                                      context.storage.declarations.type_refs
                                          .name[context.storage.declarations.type_refs.index(
                                              new_type_refs)] =
                                          context.storage.strings.intern("project::HealthQ8");
                                      return new_type_refs;
                                  }();
                                  context.storage.declarations.packed_fields
                                      .bits[context.storage.declarations.packed_fields.index(
                                          new_packed_fields)] = std::nullopt;
                                  context.storage.declarations.packed_fields
                                      .kind[context.storage.declarations.packed_fields.index(
                                          new_packed_fields)] = PackedFieldKind::linear_quantized;
                                  return new_packed_fields;
                              }(),
                              [&] {
                                  auto const new_packed_fields{
                                      context.storage.declarations.packed_fields.append()};
                                  context.storage.declarations.packed_fields
                                      .name[context.storage.declarations.packed_fields.index(
                                          new_packed_fields)] =
                                      context.storage.strings.intern("state");
                                  context.storage.declarations.packed_fields
                                      .type[context.storage.declarations.packed_fields.index(
                                          new_packed_fields)] = [&] {
                                      auto const new_type_refs{
                                          context.storage.declarations.type_refs.append()};
                                      context.storage.declarations.type_refs
                                          .name[context.storage.declarations.type_refs.index(
                                              new_type_refs)] =
                                          context.storage.strings.intern("std::uint8_t");
                                      return new_type_refs;
                                  }();
                                  context.storage.declarations.packed_fields
                                      .bits[context.storage.declarations.packed_fields.index(
                                          new_packed_fields)] = 8;
                                  return new_packed_fields;
                              }()});
                     context.storage.declarations.packed_values
                         .mutable_value[context.storage.declarations.packed_values.index(
                             new_packed_values)] = true;
                     return new_packed_values;
                 }()});
             return new_normal_modules;
         }()}};
}

auto fixed_point_backed_manifest(CompilerTestContext& context, bool const signedness = true)
    -> Manifest {
    return Manifest{
        manifest_schema_version,
        make_test_registry(context),
        context.storage.declarations,
        {[&] {
             auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
             context.storage.declarations.normal_modules
                 .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] =
                 [&] {
                     auto const new_module_settings_store{
                         context.storage.declarations.module_settings_store.append()};
                     context.storage.declarations.module_settings_store
                         .name[context.storage.declarations.module_settings_store.index(
                             new_module_settings_store)] =
                         context.storage.strings.intern("representations");
                     context.storage.declarations.module_settings_store
                         .header[context.storage.declarations.module_settings_store.index(
                             new_module_settings_store)] =
                         context.storage.strings.add("Representations.h");
                     context.storage.declarations.module_settings_store
                         .namespace_name[context.storage.declarations.module_settings_store.index(
                             new_module_settings_store)] =
                         context.storage.strings.intern("project");
                     return new_module_settings_store;
                 }();
             context.storage.declarations.normal_modules
                 .declarations[context.storage.declarations.normal_modules.index(
                     new_normal_modules)] =
                 context.storage.declarations.range<DeclarationHandle>({[&] {
                     auto const new_fixed_points{
                         context.storage.declarations.fixed_points.append()};
                     context.storage.declarations.fixed_points
                         .name[context.storage.declarations.fixed_points.index(new_fixed_points)] =
                         context.storage.strings.intern("VelocityQ8_4");
                     context.storage.declarations.fixed_points
                         .signedness[context.storage.declarations.fixed_points.index(
                             new_fixed_points)] = signedness;
                     context.storage.declarations.fixed_points
                         .total_bits[context.storage.declarations.fixed_points.index(
                             new_fixed_points)] = 12;
                     context.storage.declarations.fixed_points
                         .fractional_bits[context.storage.declarations.fixed_points.index(
                             new_fixed_points)] = 4;
                     context.storage.declarations.fixed_points
                         .rounding[context.storage.declarations.fixed_points.index(
                             new_fixed_points)] = FixedPointRounding::nearest_even;
                     return new_fixed_points;
                 }()});
             return new_normal_modules;
         }(),
         [&] {
             auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
             context.storage.declarations.normal_modules
                 .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] =
                 [&] {
                     auto const new_module_settings_store{
                         context.storage.declarations.module_settings_store.append()};
                     context.storage.declarations.module_settings_store
                         .name[context.storage.declarations.module_settings_store.index(
                             new_module_settings_store)] = context.storage.strings.intern("packed");
                     context.storage.declarations.module_settings_store
                         .header[context.storage.declarations.module_settings_store.index(
                             new_module_settings_store)] = context.storage.strings.add("Packed.h");
                     context.storage.declarations.module_settings_store
                         .namespace_name[context.storage.declarations.module_settings_store.index(
                             new_module_settings_store)] =
                         context.storage.strings.intern("project");
                     return new_module_settings_store;
                 }();
             context.storage.declarations.normal_modules
                 .declarations[context.storage.declarations.normal_modules.index(
                     new_normal_modules)] =
                 context.storage.declarations.range<DeclarationHandle>({[&] {
                     auto const new_packed_values{
                         context.storage.declarations.packed_values.append()};
                     context.storage.declarations.packed_values
                         .name[context.storage.declarations.packed_values.index(
                             new_packed_values)] = context.storage.strings.intern("Motion");
                     context.storage.declarations.packed_values
                         .storage_type[context.storage.declarations.packed_values.index(
                             new_packed_values)] = [&] {
                         auto const new_type_refs{context.storage.declarations.type_refs.append()};
                         context.storage.declarations.type_refs
                             .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                             context.storage.strings.intern("std::uint16_t");
                         return new_type_refs;
                     }();
                     context.storage.declarations.packed_values
                         .segments[context.storage.declarations.packed_values.index(
                             new_packed_values)] =
                         context.storage.declarations.range<PackedSegmentId>(
                             {[&] {
                                  auto const new_packed_fields{
                                      context.storage.declarations.packed_fields.append()};
                                  context.storage.declarations.packed_fields
                                      .name[context.storage.declarations.packed_fields.index(
                                          new_packed_fields)] =
                                      context.storage.strings.intern("velocity");
                                  context.storage.declarations.packed_fields
                                      .type[context.storage.declarations.packed_fields.index(
                                          new_packed_fields)] = [&] {
                                      auto const new_type_refs{
                                          context.storage.declarations.type_refs.append()};
                                      context.storage.declarations.type_refs
                                          .name[context.storage.declarations.type_refs.index(
                                              new_type_refs)] =
                                          context.storage.strings.intern("project::VelocityQ8_4");
                                      return new_type_refs;
                                  }();
                                  context.storage.declarations.packed_fields
                                      .bits[context.storage.declarations.packed_fields.index(
                                          new_packed_fields)] = std::nullopt;
                                  context.storage.declarations.packed_fields
                                      .kind[context.storage.declarations.packed_fields.index(
                                          new_packed_fields)] = PackedFieldKind::fixed_point;
                                  return new_packed_fields;
                              }(),
                              [&] {
                                  auto const new_packed_fields{
                                      context.storage.declarations.packed_fields.append()};
                                  context.storage.declarations.packed_fields
                                      .name[context.storage.declarations.packed_fields.index(
                                          new_packed_fields)] =
                                      context.storage.strings.intern("state");
                                  context.storage.declarations.packed_fields
                                      .type[context.storage.declarations.packed_fields.index(
                                          new_packed_fields)] = [&] {
                                      auto const new_type_refs{
                                          context.storage.declarations.type_refs.append()};
                                      context.storage.declarations.type_refs
                                          .name[context.storage.declarations.type_refs.index(
                                              new_type_refs)] =
                                          context.storage.strings.intern("std::uint8_t");
                                      return new_type_refs;
                                  }();
                                  context.storage.declarations.packed_fields
                                      .bits[context.storage.declarations.packed_fields.index(
                                          new_packed_fields)] = 4;
                                  return new_packed_fields;
                              }()});
                     context.storage.declarations.packed_values
                         .mutable_value[context.storage.declarations.packed_values.index(
                             new_packed_values)] = true;
                     return new_packed_values;
                 }()});
             return new_normal_modules;
         }()}};
}

auto mini_float_backed_manifest(CompilerTestContext& context) -> Manifest {
    return Manifest{
        manifest_schema_version,
        make_test_registry(context),
        context.storage.declarations,
        {[&] {
             auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
             context.storage.declarations.normal_modules
                 .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] =
                 [&] {
                     auto const new_module_settings_store{
                         context.storage.declarations.module_settings_store.append()};
                     context.storage.declarations.module_settings_store
                         .name[context.storage.declarations.module_settings_store.index(
                             new_module_settings_store)] =
                         context.storage.strings.intern("representations");
                     context.storage.declarations.module_settings_store
                         .header[context.storage.declarations.module_settings_store.index(
                             new_module_settings_store)] =
                         context.storage.strings.add("Representations.h");
                     context.storage.declarations.module_settings_store
                         .namespace_name[context.storage.declarations.module_settings_store.index(
                             new_module_settings_store)] =
                         context.storage.strings.intern("project");
                     return new_module_settings_store;
                 }();
             context.storage.declarations.normal_modules
                 .declarations[context.storage.declarations.normal_modules.index(
                     new_normal_modules)] =
                 context.storage.declarations.range<DeclarationHandle>({[&] {
                     auto const new_mini_floats{context.storage.declarations.mini_floats.append()};
                     context.storage.declarations.mini_floats
                         .name[context.storage.declarations.mini_floats.index(new_mini_floats)] =
                         context.storage.strings.intern("PositionF12");
                     context.storage.declarations.mini_floats
                         .sign_bits[context.storage.declarations.mini_floats.index(
                             new_mini_floats)] = 1;
                     context.storage.declarations.mini_floats
                         .exponent_bits[context.storage.declarations.mini_floats.index(
                             new_mini_floats)] = 5;
                     context.storage.declarations.mini_floats
                         .significand_bits[context.storage.declarations.mini_floats.index(
                             new_mini_floats)] = 6;
                     context.storage.declarations.mini_floats
                         .exponent_bias[context.storage.declarations.mini_floats.index(
                             new_mini_floats)] = 15;
                     return new_mini_floats;
                 }()});
             return new_normal_modules;
         }(),
         [&] {
             auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
             context.storage.declarations.normal_modules
                 .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] =
                 [&] {
                     auto const new_module_settings_store{
                         context.storage.declarations.module_settings_store.append()};
                     context.storage.declarations.module_settings_store
                         .name[context.storage.declarations.module_settings_store.index(
                             new_module_settings_store)] = context.storage.strings.intern("packed");
                     context.storage.declarations.module_settings_store
                         .header[context.storage.declarations.module_settings_store.index(
                             new_module_settings_store)] = context.storage.strings.add("Packed.h");
                     context.storage.declarations.module_settings_store
                         .namespace_name[context.storage.declarations.module_settings_store.index(
                             new_module_settings_store)] =
                         context.storage.strings.intern("project");
                     return new_module_settings_store;
                 }();
             context.storage.declarations.normal_modules
                 .declarations[context.storage.declarations.normal_modules.index(
                     new_normal_modules)] =
                 context.storage.declarations.range<DeclarationHandle>({[&] {
                     auto const new_packed_values{
                         context.storage.declarations.packed_values.append()};
                     context.storage.declarations.packed_values
                         .name[context.storage.declarations.packed_values.index(
                             new_packed_values)] = context.storage.strings.intern("Position");
                     context.storage.declarations.packed_values
                         .storage_type[context.storage.declarations.packed_values.index(
                             new_packed_values)] = [&] {
                         auto const new_type_refs{context.storage.declarations.type_refs.append()};
                         context.storage.declarations.type_refs
                             .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                             context.storage.strings.intern("std::uint16_t");
                         return new_type_refs;
                     }();
                     context.storage.declarations.packed_values
                         .segments[context.storage.declarations.packed_values.index(
                             new_packed_values)] =
                         context.storage.declarations.range<PackedSegmentId>(
                             {[&] {
                                  auto const new_packed_fields{
                                      context.storage.declarations.packed_fields.append()};
                                  context.storage.declarations.packed_fields
                                      .name[context.storage.declarations.packed_fields.index(
                                          new_packed_fields)] =
                                      context.storage.strings.intern("component");
                                  context.storage.declarations.packed_fields
                                      .type[context.storage.declarations.packed_fields.index(
                                          new_packed_fields)] = [&] {
                                      auto const new_type_refs{
                                          context.storage.declarations.type_refs.append()};
                                      context.storage.declarations.type_refs
                                          .name[context.storage.declarations.type_refs.index(
                                              new_type_refs)] =
                                          context.storage.strings.intern("project::PositionF12");
                                      return new_type_refs;
                                  }();
                                  context.storage.declarations.packed_fields
                                      .bits[context.storage.declarations.packed_fields.index(
                                          new_packed_fields)] = std::nullopt;
                                  context.storage.declarations.packed_fields
                                      .kind[context.storage.declarations.packed_fields.index(
                                          new_packed_fields)] = PackedFieldKind::mini_float;
                                  return new_packed_fields;
                              }(),
                              [&] {
                                  auto const new_packed_fields{
                                      context.storage.declarations.packed_fields.append()};
                                  context.storage.declarations.packed_fields
                                      .name[context.storage.declarations.packed_fields.index(
                                          new_packed_fields)] =
                                      context.storage.strings.intern("state");
                                  context.storage.declarations.packed_fields
                                      .type[context.storage.declarations.packed_fields.index(
                                          new_packed_fields)] = [&] {
                                      auto const new_type_refs{
                                          context.storage.declarations.type_refs.append()};
                                      context.storage.declarations.type_refs
                                          .name[context.storage.declarations.type_refs.index(
                                              new_type_refs)] =
                                          context.storage.strings.intern("std::uint8_t");
                                      return new_type_refs;
                                  }();
                                  context.storage.declarations.packed_fields
                                      .bits[context.storage.declarations.packed_fields.index(
                                          new_packed_fields)] = 4;
                                  return new_packed_fields;
                              }()});
                     context.storage.declarations.packed_values
                         .mutable_value[context.storage.declarations.packed_values.index(
                             new_packed_values)] = true;
                     return new_packed_values;
                 }()});
             return new_normal_modules;
         }()}};
}

TEST(PackedValue, ValidatesExplicitDefaultsAndReportsStorageBudget) {
    CompilerTestContext context;
    auto const module{context.storage.declarations.normal_modules.append()};
    context.storage.declarations.normal_modules
        .settings[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const new_module_settings_store{
            context.storage.declarations.module_settings_store.append()};
        context.storage.declarations.module_settings_store
            .name[context.storage.declarations.module_settings_store.index(
                new_module_settings_store)] = context.storage.strings.intern("orders");
        context.storage.declarations.module_settings_store
            .header[context.storage.declarations.module_settings_store.index(
                new_module_settings_store)] = context.storage.strings.add("Orders.h");
        return new_module_settings_store;
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(
            module)] = context.storage.declarations.range<DeclarationHandle>({[&] {
        auto const new_packed_values{context.storage.declarations.packed_values.append()};
        context.storage.declarations.packed_values
            .name[context.storage.declarations.packed_values.index(new_packed_values)] =
            context.storage.strings.intern("FighterOrder");
        context.storage.declarations.packed_values
            .storage_type[context.storage.declarations.packed_values.index(new_packed_values)] =
            [&] {
                auto const new_type_refs{context.storage.declarations.type_refs.append()};
                context.storage.declarations.type_refs
                    .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                    context.storage.strings.intern("std::uint8_t");
                return new_type_refs;
            }();
        context.storage.declarations.packed_values
            .segments[context.storage.declarations.packed_values.index(new_packed_values)] =
            context.storage.declarations.range<PackedSegmentId>(
                {[&] {
                     auto const new_packed_fields{
                         context.storage.declarations.packed_fields.append()};
                     context.storage.declarations.packed_fields
                         .name[context.storage.declarations.packed_fields.index(
                             new_packed_fields)] = context.storage.strings.intern("task");
                     context.storage.declarations.packed_fields
                         .type[context.storage.declarations.packed_fields.index(
                             new_packed_fields)] = [&] {
                         auto const new_type_refs{context.storage.declarations.type_refs.append()};
                         context.storage.declarations.type_refs
                             .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                             context.storage.strings.intern("std::uint8_t");
                         return new_type_refs;
                     }();
                     context.storage.declarations.packed_fields
                         .bits[context.storage.declarations.packed_fields.index(
                             new_packed_fields)] = 1;
                     context.storage.declarations.packed_fields
                         .default_value[context.storage.declarations.packed_fields.index(
                             new_packed_fields)] = 0;
                     return new_packed_fields;
                 }(),
                 [&] {
                     auto const new_packed_fields{
                         context.storage.declarations.packed_fields.append()};
                     context.storage.declarations.packed_fields
                         .name[context.storage.declarations.packed_fields.index(
                             new_packed_fields)] = context.storage.strings.intern("target");
                     context.storage.declarations.packed_fields
                         .type[context.storage.declarations.packed_fields.index(
                             new_packed_fields)] = [&] {
                         auto const new_type_refs{context.storage.declarations.type_refs.append()};
                         context.storage.declarations.type_refs
                             .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                             context.storage.strings.intern("std::uint8_t");
                         return new_type_refs;
                     }();
                     context.storage.declarations.packed_fields
                         .bits[context.storage.declarations.packed_fields.index(
                             new_packed_fields)] = 1;
                     context.storage.declarations.packed_fields
                         .default_value[context.storage.declarations.packed_fields.index(
                             new_packed_fields)] = 1;
                     return new_packed_fields;
                 }()});
        return new_packed_values;
    }()});
    EXPECT_NO_THROW(lower(context, module));

    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3[0] = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3[0])};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[1])};
                    context.storage.declarations.packed_fields
                        .default_value[context.storage.declarations.packed_fields.index(row_0)] = 2;
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3[0] = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3[0])};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[1])};
                    context.storage.declarations.packed_fields
                        .default_value[context.storage.declarations.packed_fields.index(row_0)] =
                        -1;
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3[0] = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3[0])};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[1])};
                    context.storage.declarations.packed_fields
                        .default_value[context.storage.declarations.packed_fields.index(row_0)] = 1;
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
        rows_1[0] = [&] {
            auto row_0{context.storage.declarations.packed_values.clone(rows_1[0])};
            context.storage.declarations.packed_values
                .invalid_value[context.storage.declarations.packed_values.index(row_0)] = 2;
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1[0] = [&] {
            auto row_0{context.storage.declarations.packed_values.clone(rows_1[0])};
            context.storage.declarations.packed_values
                .bit_order[context.storage.declarations.packed_values.index(row_0)] =
                PackedBitOrder::most_significant_first;
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_NO_THROW(lower(context, module));
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1[0] = [&] {
            auto row_0{context.storage.declarations.packed_values.clone(rows_1[0])};
            context.storage.declarations.packed_values
                .invalid_value[context.storage.declarations.packed_values.index(row_0)] = 64;
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3[0] = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3[0])};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .default_value[context.storage.declarations.packed_fields.index(row_0)]
                        .reset();
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_NO_THROW(lower(context, module));
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1[0] = [&] {
            auto row_0{context.storage.declarations.packed_values.clone(rows_1[0])};
            context.storage.declarations.packed_values
                .invalid_value[context.storage.declarations.packed_values.index(row_0)]
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
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3[0] = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3[0])};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[1])};
                    context.storage.declarations.packed_fields
                        .bits[context.storage.declarations.packed_fields.index(row_0)] = 8;
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
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
    auto manifest{scalar_backed_manifest(context, valid_module(context))};

    manifest.modules.back() = [&] {
        auto row_4{context.storage.declarations.normal_modules.clone(manifest.modules.back())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_4)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_4)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
                context.storage.declarations.packed_values
                    .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.packed_values
                            .segments[context.storage.declarations.packed_values.index(row_2)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                        context.storage.declarations.packed_fields
                            .default_value[context.storage.declarations.packed_fields.index(
                                row_0)] = 0;
                        return row_0;
                    }();
                    return context.storage.declarations.range(rows_1);
                }();
                return row_2;
            }();
            return context.storage.declarations.range(rows_3);
        }();
        return row_4;
    }();
    EXPECT_NO_THROW(lower_test_manifest(context, manifest));
    manifest.modules.back() = [&] {
        auto row_4{context.storage.declarations.normal_modules.clone(manifest.modules.back())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_4)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_4)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
                context.storage.declarations.packed_values
                    .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.packed_values
                            .segments[context.storage.declarations.packed_values.index(row_2)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                        context.storage.declarations.packed_fields
                            .default_value[context.storage.declarations.packed_fields.index(
                                row_0)] = 4095;
                        return row_0;
                    }();
                    return context.storage.declarations.range(rows_1);
                }();
                return row_2;
            }();
            return context.storage.declarations.range(rows_3);
        }();
        return row_4;
    }();
    EXPECT_NO_THROW(lower_test_manifest(context, manifest));
    manifest.modules.back() = [&] {
        auto row_4{context.storage.declarations.normal_modules.clone(manifest.modules.back())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_4)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_4)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
                context.storage.declarations.packed_values
                    .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.packed_values
                            .segments[context.storage.declarations.packed_values.index(row_2)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                        context.storage.declarations.packed_fields
                            .default_value[context.storage.declarations.packed_fields.index(
                                row_0)] = 1001;
                        return row_0;
                    }();
                    return context.storage.declarations.range(rows_1);
                }();
                return row_2;
            }();
            return context.storage.declarations.range(rows_3);
        }();
        return row_4;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);
    manifest.modules.back() = [&] {
        auto row_4{context.storage.declarations.normal_modules.clone(manifest.modules.back())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_4)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_4)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
                context.storage.declarations.packed_values
                    .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.packed_values
                            .segments[context.storage.declarations.packed_values.index(row_2)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                        context.storage.declarations.packed_fields
                            .default_value[context.storage.declarations.packed_fields.index(
                                row_0)] = 1000000;
                        return row_0;
                    }();
                    return context.storage.declarations.range(rows_1);
                }();
                return row_2;
            }();
            return context.storage.declarations.range(rows_3);
        }();
        return row_4;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);
}

TEST(PackedValue, LowersTypedFieldsAndThreeWayComparison) {
    CompilerTestContext context;
    auto module{context.storage.declarations.normal_modules.clone(valid_module(context))};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.packed_values.clone(rows_1.front())};
            context.storage.declarations.packed_values
                .invalid_value[context.storage.declarations.packed_values.index(row_0)] =
                0x7fffffffu;
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
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .range_helper[context.storage.declarations.packed_fields.index(row_0)] =
                        true;
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
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .bits[context.storage.declarations.packed_fields.index(row_0)]
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
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .minimum_value[context.storage.declarations.packed_fields.index(row_0)] =
                        10;
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
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .maximum_value[context.storage.declarations.packed_fields.index(row_0)] =
                        1'000'000;
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
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .named_codes[context.storage.declarations.packed_fields.index(row_0)] =
                        context.storage.declarations.range<PackedNamedCodeId>(
                            {[&] {
                                 auto const new_packed_named_codes{
                                     context.storage.declarations.packed_named_codes.append()};
                                 context.storage.declarations.packed_named_codes
                                     .name[context.storage.declarations.packed_named_codes.index(
                                         new_packed_named_codes)] =
                                     context.storage.strings.intern("Player");
                                 context.storage.declarations.packed_named_codes
                                     .value[context.storage.declarations.packed_named_codes.index(
                                         new_packed_named_codes)] = 42;
                                 context.storage.declarations.packed_named_codes
                                     .sentinel[context.storage.declarations.packed_named_codes
                                                   .index(new_packed_named_codes)] = false;
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
                                         new_packed_named_codes)] = 0xffffff;
                                 context.storage.declarations.packed_named_codes
                                     .sentinel[context.storage.declarations.packed_named_codes
                                                   .index(new_packed_named_codes)] = true;
                                 return new_packed_named_codes;
                             }()});
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
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
    auto module{context.storage.declarations.normal_modules.clone(valid_module(context))};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.packed_values.clone(rows_1.front())};
            context.storage.declarations.packed_values
                .mutable_value[context.storage.declarations.packed_values.index(row_0)] = true;
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    auto const header{lower(context, module)};

    EXPECT_EQ(header.find("try_make(entity_index_type const entity_index_value"),
              std::string::npos);
    EXPECT_EQ(header.find("try_set_entity_index"), std::string::npos);
    EXPECT_NE(header.find("set_entity_index"), std::string::npos);
}

TEST(PackedValue, UsesFieldAliasesAcrossImmutableAndMutablePublicApis) {
    CompilerTestContext context;
    for (auto const mutable_value : {false, true}) {
        auto module{context.storage.declarations.normal_modules.clone(valid_module(context))};

        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(module)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{context.storage.declarations.packed_values.clone(rows_1.front())};
                context.storage.declarations.packed_values
                    .mutable_value[context.storage.declarations.packed_values.index(row_0)] =
                    mutable_value;
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
                auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
                context.storage.declarations.packed_values
                    .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.packed_values
                            .segments[context.storage.declarations.packed_values.index(row_2)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                        context.storage.declarations.packed_fields
                            .range_helper[context.storage.declarations.packed_fields.index(row_0)] =
                            true;
                        return row_0;
                    }();
                    return context.storage.declarations.range(rows_1);
                }();
                return row_2;
            }();
            return context.storage.declarations.range(rows_3);
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
    for (auto const& manifest : {quantized_backed_manifest(context),
                                 fixed_point_backed_manifest(context),
                                 mini_float_backed_manifest(context)}) {
        auto const schema{
            context.storage.declarations
                .items(context.storage.declarations.normal_modules
                           .declarations[context.storage.declarations.normal_modules.index(
                               manifest.modules.back())])
                .front()};
        auto const fixed_point{
            std::string{context.storage.strings.view(
                context.storage.declarations.packed_values
                    .name[context.storage.declarations.packed_values.index(schema)])} == "Motion"};
        auto const accessor{
            fixed_point ? std::string{"velocity_raw"}
            : std::string{context.storage.strings.view(
                  context.storage.declarations.packed_values
                      .name[context.storage.declarations.packed_values.index(schema)])} == "Vitals"
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
            fixed_point ||
                    std::string{context.storage.strings.view(
                        context.storage.declarations.packed_values
                            .name[context.storage.declarations.packed_values.index(schema)])} ==
                        "Vitals"
                ? 1
                : 2);
        expect_type_only_in_alias_definitions(
            header,
            "std::uint8_t",
            std::string{context.storage.strings.view(
                context.storage.declarations.packed_values
                    .name[context.storage.declarations.packed_values.index(schema)])} == "Vitals"
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
                      std::string{context.storage.strings.view(
                          context.storage.declarations.packed_values
                              .name[context.storage.declarations.packed_values.index(schema)])} +
                      "(" + parameter + ",state_typeconststate_value)noexcept"),
            std::string::npos);
        EXPECT_EQ(
            text.find("try_make(" + parameter + ",state_typeconststate_value," +
                      std::string{context.storage.strings.view(
                          context.storage.declarations.packed_values
                              .name[context.storage.declarations.packed_values.index(schema)])} +
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
    auto module{context.storage.declarations.normal_modules.clone(valid_module(context))};

    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.packed_values.clone(rows_1.front())};
            context.storage.declarations.packed_values
                .mutable_value[context.storage.declarations.packed_values.index(row_0)] = true;
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
            auto row_0{context.storage.declarations.packed_values.clone(rows_1.front())};
            context.storage.declarations.packed_values
                .storage_type[context.storage.declarations.packed_values.index(row_0)] = [&] {
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
            auto row_0{context.storage.declarations.packed_values.clone(rows_1.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_0)] =
                context.storage.declarations.range<PackedSegmentId>({[&] {
                    auto const new_packed_fields{
                        context.storage.declarations.packed_fields.append()};
                    context.storage.declarations.packed_fields
                        .name[context.storage.declarations.packed_fields.index(new_packed_fields)] =
                        context.storage.strings.intern("flag");
                    context.storage.declarations.packed_fields
                        .type[context.storage.declarations.packed_fields.index(new_packed_fields)] =
                        [&] {
                            auto const new_type_refs{
                                context.storage.declarations.type_refs.append()};
                            context.storage.declarations.type_refs
                                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                                context.storage.strings.intern("bool");
                            return new_type_refs;
                        }();
                    context.storage.declarations.packed_fields
                        .bits[context.storage.declarations.packed_fields.index(new_packed_fields)] =
                        1;
                    return new_packed_fields;
                }()});
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
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
    auto module{context.storage.declarations.normal_modules.clone(valid_module(context))};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .bits[context.storage.declarations.packed_fields.index(row_0)] = 20;
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
            auto row_0{context.storage.declarations.packed_values.clone(rows_1.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_0)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_0)])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.insert(rows.begin() + 1, [&] {
                    auto const new_packed_reserved_bits{
                        context.storage.declarations.packed_reserved_bits.append()};
                    context.storage.declarations.packed_reserved_bits
                        .name[context.storage.declarations.packed_reserved_bits.index(
                            new_packed_reserved_bits)] = context.storage.strings.intern("future");
                    context.storage.declarations.packed_reserved_bits
                        .bits[context.storage.declarations.packed_reserved_bits.index(
                            new_packed_reserved_bits)] = 4;
                    return new_packed_reserved_bits;
                }());
                return context.storage.declarations.range(rows);
            }();
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
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
    auto module{context.storage.declarations.normal_modules.clone(valid_module(context))};

    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .bits[context.storage.declarations.packed_fields.index(row_0)] = 20;
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
            auto row_0{context.storage.declarations.packed_values.clone(rows_1.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_0)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_0)])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.insert(rows.begin() + 1, [&] {
                    auto const new_packed_reserved_bits{
                        context.storage.declarations.packed_reserved_bits.append()};
                    context.storage.declarations.packed_reserved_bits
                        .name[context.storage.declarations.packed_reserved_bits.index(
                            new_packed_reserved_bits)] = context.storage.strings.intern("future");
                    context.storage.declarations.packed_reserved_bits
                        .bits[context.storage.declarations.packed_reserved_bits.index(
                            new_packed_reserved_bits)] = 4;
                    return new_packed_reserved_bits;
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
            auto row_0{context.storage.declarations.packed_values.clone(rows_1.front())};
            context.storage.declarations.packed_values
                .byte_order[context.storage.declarations.packed_values.index(row_0)] =
                PackedByteOrder::big_endian;
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
            auto row_0{context.storage.declarations.packed_values.clone(rows_1.front())};
            context.storage.declarations.packed_values
                .bit_order[context.storage.declarations.packed_values.index(row_0)] =
                PackedBitOrder::most_significant_first;
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();

    auto const header{lower(context, module)};

    EXPECT_NE(header.find("ioj::PackedField<storage_type, entity_index_type, 12, 20>"),
              std::string::npos);
    EXPECT_NE(header.find("ioj::PackedField<storage_type, state_type, 0, 8>"), std::string::npos);
    EXPECT_EQ(header.find("future_field"), std::string::npos);
}

TEST(PackedValue, MostSignificantFirstEnumDomainCanExcludeInvalidRawValue) {
    CompilerTestContext context;
    auto module{context.storage.declarations.normal_modules.clone(valid_module(context))};

    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.packed_values.clone(rows_1.front())};
            context.storage.declarations.packed_values
                .invalid_value[context.storage.declarations.packed_values.index(row_0)] = 7;
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
            auto row_0{context.storage.declarations.packed_values.clone(rows_1.front())};
            context.storage.declarations.packed_values
                .bit_order[context.storage.declarations.packed_values.index(row_0)] =
                PackedBitOrder::most_significant_first;
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();

    auto const header{lower_known_enum(context, module, [&] {
        auto const new_enums{context.storage.declarations.enums.append()};
        context.storage.declarations.enums
            .name[context.storage.declarations.enums.index(new_enums)] =
            context.storage.strings.intern("FighterStateKind");
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
            context.storage.declarations.range<EnumeratorId>(
                {[&] {
                     auto const new_enumerators{context.storage.declarations.enumerators.append()};
                     context.storage.declarations.enumerators
                         .name[context.storage.declarations.enumerators.index(new_enumerators)] =
                         context.storage.strings.intern("Zero");
                     context.storage.declarations.enumerators
                         .initializer[context.storage.declarations.enumerators.index(
                             new_enumerators)] = context.storage.strings.add("0");
                     return new_enumerators;
                 }(),
                 [&] {
                     auto const new_enumerators{context.storage.declarations.enumerators.append()};
                     context.storage.declarations.enumerators
                         .name[context.storage.declarations.enumerators.index(new_enumerators)] =
                         context.storage.strings.intern("COUNT");
                     context.storage.declarations.enumerators
                         .initializer[context.storage.declarations.enumerators.index(
                             new_enumerators)] = context.storage.strings.add("5");
                     context.storage.declarations.enumerators
                         .display_name[context.storage.declarations.enumerators.index(
                             new_enumerators)] = std::nullopt;
                     context.storage.declarations.enumerators
                         .hidden[context.storage.declarations.enumerators.index(new_enumerators)] =
                         true;
                     return new_enumerators;
                 }()});
        context.storage.declarations.enums
            .count[context.storage.declarations.enums.index(new_enums)] =
            context.storage.strings.intern("COUNT");
        return new_enums;
    }())};

    EXPECT_EQ(header.find("assert(raw != invalid_value);"), std::string::npos);
}

TEST(PackedValue, SerializedByteOrderDoesNotChangeHostNumericOffsets) {
    CompilerTestContext context;
    auto module{context.storage.declarations.normal_modules.clone(valid_module(context))};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.packed_values.clone(rows_1.front())};
            context.storage.declarations.packed_values
                .byte_order[context.storage.declarations.packed_values.index(row_0)] =
                PackedByteOrder::big_endian;
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();

    auto const header{lower(context, module)};

    EXPECT_NE(header.find("ioj::PackedField<storage_type, entity_index_type, 0, 24>"),
              std::string::npos);
    EXPECT_NE(header.find("ioj::PackedField<storage_type, state_type, 24, 8>"), std::string::npos);
}

TEST(PackedValue, LowersSignedArbitraryWidthFieldWithSafeSignExtension) {
    CompilerTestContext context;
    auto module{context.storage.declarations.normal_modules.clone(valid_module(context))};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.packed_values.clone(rows_1.front())};
            context.storage.declarations.packed_values
                .mutable_value[context.storage.declarations.packed_values.index(row_0)] = true;
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
            auto row_0{context.storage.declarations.packed_values.clone(rows_1.front())};
            context.storage.declarations.packed_values
                .name[context.storage.declarations.packed_values.index(row_0)] =
                context.storage.strings.intern("SignedDelta");
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
            auto row_0{context.storage.declarations.packed_values.clone(rows_1.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_0)] =
                context.storage.declarations.range<PackedSegmentId>(
                    {[&] {
                         auto const new_packed_fields{
                             context.storage.declarations.packed_fields.append()};
                         context.storage.declarations.packed_fields
                             .name[context.storage.declarations.packed_fields.index(
                                 new_packed_fields)] = context.storage.strings.intern("delta");
                         context.storage.declarations.packed_fields
                             .type[context.storage.declarations.packed_fields.index(
                                 new_packed_fields)] = [&] {
                             auto const new_type_refs{
                                 context.storage.declarations.type_refs.append()};
                             context.storage.declarations.type_refs
                                 .name[context.storage.declarations.type_refs.index(
                                     new_type_refs)] =
                                 context.storage.strings.intern("std::int32_t");
                             return new_type_refs;
                         }();
                         context.storage.declarations.packed_fields
                             .bits[context.storage.declarations.packed_fields.index(
                                 new_packed_fields)] = 17;
                         context.storage.declarations.packed_fields
                             .kind[context.storage.declarations.packed_fields.index(
                                 new_packed_fields)] = PackedFieldKind::signed_integer;
                         return new_packed_fields;
                     }(),
                     [&] {
                         auto const new_packed_reserved_bits{
                             context.storage.declarations.packed_reserved_bits.append()};
                         context.storage.declarations.packed_reserved_bits
                             .name[context.storage.declarations.packed_reserved_bits.index(
                                 new_packed_reserved_bits)] =
                             context.storage.strings.intern("future");
                         context.storage.declarations.packed_reserved_bits
                             .bits[context.storage.declarations.packed_reserved_bits.index(
                                 new_packed_reserved_bits)] = 15;
                         return new_packed_reserved_bits;
                     }()});
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
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
    auto module{context.storage.declarations.normal_modules.clone(valid_module(context))};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.packed_values.clone(rows_1.front())};
            context.storage.declarations.packed_values
                .mutable_value[context.storage.declarations.packed_values.index(row_0)] = true;
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
            auto row_0{context.storage.declarations.packed_values.clone(rows_1.front())};
            context.storage.declarations.packed_values
                .name[context.storage.declarations.packed_values.index(row_0)] =
                context.storage.strings.intern("SignedTemperature");
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
            auto row_0{context.storage.declarations.packed_values.clone(rows_1.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_0)] =
                context.storage.declarations.range<PackedSegmentId>(
                    {[&] {
                         auto const new_packed_fields{
                             context.storage.declarations.packed_fields.append()};
                         context.storage.declarations.packed_fields
                             .name[context.storage.declarations.packed_fields.index(
                                 new_packed_fields)] =
                             context.storage.strings.intern("temperature");
                         context.storage.declarations.packed_fields
                             .type[context.storage.declarations.packed_fields.index(
                                 new_packed_fields)] = [&] {
                             auto const new_type_refs{
                                 context.storage.declarations.type_refs.append()};
                             context.storage.declarations.type_refs
                                 .name[context.storage.declarations.type_refs.index(
                                     new_type_refs)] =
                                 context.storage.strings.intern("std::int16_t");
                             return new_type_refs;
                         }();
                         context.storage.declarations.packed_fields
                             .bits[context.storage.declarations.packed_fields.index(
                                 new_packed_fields)] = std::nullopt;
                         context.storage.declarations.packed_fields
                             .kind[context.storage.declarations.packed_fields.index(
                                 new_packed_fields)] = PackedFieldKind::signed_integer;
                         context.storage.declarations.packed_fields
                             .range_helper[context.storage.declarations.packed_fields.index(
                                 new_packed_fields)] = false;
                         context.storage.declarations.packed_fields
                             .minimum_value[context.storage.declarations.packed_fields.index(
                                 new_packed_fields)] = -100;
                         context.storage.declarations.packed_fields
                             .maximum_value[context.storage.declarations.packed_fields.index(
                                 new_packed_fields)] = 100;
                         context.storage.declarations.packed_fields
                             .named_codes[context.storage.declarations.packed_fields.index(
                                 new_packed_fields)] =
                             context.storage.declarations.range<PackedNamedCodeId>(
                                 {[&] {
                                      auto const new_packed_named_codes{
                                          context.storage.declarations.packed_named_codes.append()};
                                      context.storage.declarations.packed_named_codes
                                          .name[context.storage.declarations.packed_named_codes
                                                    .index(new_packed_named_codes)] =
                                          context.storage.strings.intern("Freezing");
                                      context.storage.declarations.packed_named_codes
                                          .value[context.storage.declarations.packed_named_codes
                                                     .index(new_packed_named_codes)] = 0;
                                      context.storage.declarations.packed_named_codes
                                          .sentinel[context.storage.declarations.packed_named_codes
                                                        .index(new_packed_named_codes)] = false;
                                      return new_packed_named_codes;
                                  }(),
                                  [&] {
                                      auto const new_packed_named_codes{
                                          context.storage.declarations.packed_named_codes.append()};
                                      context.storage.declarations.packed_named_codes
                                          .name[context.storage.declarations.packed_named_codes
                                                    .index(new_packed_named_codes)] =
                                          context.storage.strings.intern("Unknown");
                                      context.storage.declarations.packed_named_codes
                                          .value[context.storage.declarations.packed_named_codes
                                                     .index(new_packed_named_codes)] = -128;
                                      context.storage.declarations.packed_named_codes
                                          .sentinel[context.storage.declarations.packed_named_codes
                                                        .index(new_packed_named_codes)] = true;
                                      return new_packed_named_codes;
                                  }()});
                         return new_packed_fields;
                     }(),
                     [&] {
                         auto const new_packed_reserved_bits{
                             context.storage.declarations.packed_reserved_bits.append()};
                         context.storage.declarations.packed_reserved_bits
                             .name[context.storage.declarations.packed_reserved_bits.index(
                                 new_packed_reserved_bits)] =
                             context.storage.strings.intern("future");
                         context.storage.declarations.packed_reserved_bits
                             .bits[context.storage.declarations.packed_reserved_bits.index(
                                 new_packed_reserved_bits)] = 24;
                         return new_packed_reserved_bits;
                     }()});
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
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
    auto manifest{quantized_backed_manifest(context)};

    manifest.modules.back() = [&] {
        auto row_4{context.storage.declarations.normal_modules.clone(manifest.modules.back())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_4)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_4)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
                context.storage.declarations.packed_values
                    .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.packed_values
                            .segments[context.storage.declarations.packed_values.index(row_2)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                        context.storage.declarations.packed_fields
                            .bits[context.storage.declarations.packed_fields.index(row_0)] = 7;
                        return row_0;
                    }();
                    return context.storage.declarations.range(rows_1);
                }();
                return row_2;
            }();
            return context.storage.declarations.range(rows_3);
        }();
        return row_4;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);

    manifest.modules.back() = [&] {
        auto row_4{context.storage.declarations.normal_modules.clone(manifest.modules.back())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_4)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_4)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
                context.storage.declarations.packed_values
                    .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.packed_values
                            .segments[context.storage.declarations.packed_values.index(row_2)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                        context.storage.declarations.packed_fields
                            .bits[context.storage.declarations.packed_fields.index(row_0)]
                            .reset();
                        return row_0;
                    }();
                    return context.storage.declarations.range(rows_1);
                }();
                return row_2;
            }();
            return context.storage.declarations.range(rows_3);
        }();
        return row_4;
    }();
    manifest.modules.back() = [&] {
        auto row_4{context.storage.declarations.normal_modules.clone(manifest.modules.back())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_4)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_4)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
                context.storage.declarations.packed_values
                    .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.packed_values
                            .segments[context.storage.declarations.packed_values.index(row_2)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                        context.storage.declarations.packed_fields
                            .kind[context.storage.declarations.packed_fields.index(row_0)] =
                            PackedFieldKind::unsigned_integer;
                        return row_0;
                    }();
                    return context.storage.declarations.range(rows_1);
                }();
                return row_2;
            }();
            return context.storage.declarations.range(rows_3);
        }();
        return row_4;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);

    manifest.modules.back() = [&] {
        auto row_4{context.storage.declarations.normal_modules.clone(manifest.modules.back())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_4)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_4)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
                context.storage.declarations.packed_values
                    .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.packed_values
                            .segments[context.storage.declarations.packed_values.index(row_2)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                        context.storage.declarations.packed_fields
                            .kind[context.storage.declarations.packed_fields.index(row_0)] =
                            PackedFieldKind::linear_quantized;
                        return row_0;
                    }();
                    return context.storage.declarations.range(rows_1);
                }();
                return row_2;
            }();
            return context.storage.declarations.range(rows_3);
        }();
        return row_4;
    }();
    manifest.modules.back() = [&] {
        auto row_4{context.storage.declarations.normal_modules.clone(manifest.modules.back())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_4)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_4)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
                context.storage.declarations.packed_values
                    .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.packed_values
                            .segments[context.storage.declarations.packed_values.index(row_2)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                        context.storage.declarations.packed_fields
                            .minimum_value[context.storage.declarations.packed_fields.index(
                                row_0)] = 0;
                        return row_0;
                    }();
                    return context.storage.declarations.range(rows_1);
                }();
                return row_2;
            }();
            return context.storage.declarations.range(rows_3);
        }();
        return row_4;
    }();
    manifest.modules.back() = [&] {
        auto row_4{context.storage.declarations.normal_modules.clone(manifest.modules.back())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_4)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_4)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
                context.storage.declarations.packed_values
                    .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.packed_values
                            .segments[context.storage.declarations.packed_values.index(row_2)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                        context.storage.declarations.packed_fields
                            .maximum_value[context.storage.declarations.packed_fields.index(
                                row_0)] = 1000;
                        return row_0;
                    }();
                    return context.storage.declarations.range(rows_1);
                }();
                return row_2;
            }();
            return context.storage.declarations.range(rows_3);
        }();
        return row_4;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);

    manifest.modules.back() = [&] {
        auto row_4{context.storage.declarations.normal_modules.clone(manifest.modules.back())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_4)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_4)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
                context.storage.declarations.packed_values
                    .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.packed_values
                            .segments[context.storage.declarations.packed_values.index(row_2)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                        context.storage.declarations.packed_fields
                            .minimum_value[context.storage.declarations.packed_fields.index(row_0)]
                            .reset();
                        return row_0;
                    }();
                    return context.storage.declarations.range(rows_1);
                }();
                return row_2;
            }();
            return context.storage.declarations.range(rows_3);
        }();
        return row_4;
    }();
    manifest.modules.back() = [&] {
        auto row_4{context.storage.declarations.normal_modules.clone(manifest.modules.back())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_4)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_4)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
                context.storage.declarations.packed_values
                    .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.packed_values
                            .segments[context.storage.declarations.packed_values.index(row_2)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                        context.storage.declarations.packed_fields
                            .maximum_value[context.storage.declarations.packed_fields.index(row_0)]
                            .reset();
                        return row_0;
                    }();
                    return context.storage.declarations.range(rows_1);
                }();
                return row_2;
            }();
            return context.storage.declarations.range(rows_3);
        }();
        return row_4;
    }();
    manifest.modules.back() = [&] {
        auto row_4{context.storage.declarations.normal_modules.clone(manifest.modules.back())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_4)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_4)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
                context.storage.declarations.packed_values
                    .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.packed_values
                            .segments[context.storage.declarations.packed_values.index(row_2)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                        context.storage.declarations.packed_fields
                            .range_helper[context.storage.declarations.packed_fields.index(row_0)] =
                            true;
                        return row_0;
                    }();
                    return context.storage.declarations.range(rows_1);
                }();
                return row_2;
            }();
            return context.storage.declarations.range(rows_3);
        }();
        return row_4;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);
}

TEST(PackedValue, LowersFixedPointPlacementToExplicitRawCodeApi) {
    CompilerTestContext context;
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
        auto row_2{
            context.storage.declarations.normal_modules.clone(bounded_manifest.modules.front())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_2)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_2)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{context.storage.declarations.fixed_points.clone(rows_1.front())};
                context.storage.declarations.fixed_points
                    .minimum_value[context.storage.declarations.fixed_points.index(row_0)] =
                    context.storage.strings.add("-1.5");
                return row_0;
            }();
            return context.storage.declarations.range(rows_1);
        }();
        return row_2;
    }();
    bounded_manifest.modules.front() = [&] {
        auto row_2{
            context.storage.declarations.normal_modules.clone(bounded_manifest.modules.front())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_2)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_2)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{context.storage.declarations.fixed_points.clone(rows_1.front())};
                context.storage.declarations.fixed_points
                    .maximum_value[context.storage.declarations.fixed_points.index(row_0)] =
                    context.storage.strings.add("2.25");
                return row_0;
            }();
            return context.storage.declarations.range(rows_1);
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
        auto row_2{
            context.storage.declarations.normal_modules.clone(immutable_manifest.modules.back())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_2)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_2)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{context.storage.declarations.packed_values.clone(rows_1.front())};
                context.storage.declarations.packed_values
                    .mutable_value[context.storage.declarations.packed_values.index(row_0)] = false;
                return row_0;
            }();
            return context.storage.declarations.range(rows_1);
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
    auto manifest{fixed_point_backed_manifest(context)};

    manifest.modules.back() = [&] {
        auto row_4{context.storage.declarations.normal_modules.clone(manifest.modules.back())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_4)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_4)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
                context.storage.declarations.packed_values
                    .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.packed_values
                            .segments[context.storage.declarations.packed_values.index(row_2)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                        context.storage.declarations.packed_fields
                            .bits[context.storage.declarations.packed_fields.index(row_0)] = 11;
                        return row_0;
                    }();
                    return context.storage.declarations.range(rows_1);
                }();
                return row_2;
            }();
            return context.storage.declarations.range(rows_3);
        }();
        return row_4;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);

    manifest.modules.back() = [&] {
        auto row_4{context.storage.declarations.normal_modules.clone(manifest.modules.back())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_4)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_4)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
                context.storage.declarations.packed_values
                    .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.packed_values
                            .segments[context.storage.declarations.packed_values.index(row_2)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                        context.storage.declarations.packed_fields
                            .bits[context.storage.declarations.packed_fields.index(row_0)]
                            .reset();
                        return row_0;
                    }();
                    return context.storage.declarations.range(rows_1);
                }();
                return row_2;
            }();
            return context.storage.declarations.range(rows_3);
        }();
        return row_4;
    }();
    manifest.modules.back() = [&] {
        auto row_4{context.storage.declarations.normal_modules.clone(manifest.modules.back())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_4)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_4)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
                context.storage.declarations.packed_values
                    .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.packed_values
                            .segments[context.storage.declarations.packed_values.index(row_2)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                        context.storage.declarations.packed_fields
                            .kind[context.storage.declarations.packed_fields.index(row_0)] =
                            PackedFieldKind::signed_integer;
                        return row_0;
                    }();
                    return context.storage.declarations.range(rows_1);
                }();
                return row_2;
            }();
            return context.storage.declarations.range(rows_3);
        }();
        return row_4;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);

    manifest.modules.back() = [&] {
        auto row_4{context.storage.declarations.normal_modules.clone(manifest.modules.back())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_4)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_4)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
                context.storage.declarations.packed_values
                    .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.packed_values
                            .segments[context.storage.declarations.packed_values.index(row_2)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                        context.storage.declarations.packed_fields
                            .kind[context.storage.declarations.packed_fields.index(row_0)] =
                            PackedFieldKind::fixed_point;
                        return row_0;
                    }();
                    return context.storage.declarations.range(rows_1);
                }();
                return row_2;
            }();
            return context.storage.declarations.range(rows_3);
        }();
        return row_4;
    }();
    manifest.modules.back() = [&] {
        auto row_4{context.storage.declarations.normal_modules.clone(manifest.modules.back())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_4)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_4)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
                context.storage.declarations.packed_values
                    .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.packed_values
                            .segments[context.storage.declarations.packed_values.index(row_2)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                        context.storage.declarations.packed_fields
                            .minimum_value[context.storage.declarations.packed_fields.index(
                                row_0)] = -10;
                        return row_0;
                    }();
                    return context.storage.declarations.range(rows_1);
                }();
                return row_2;
            }();
            return context.storage.declarations.range(rows_3);
        }();
        return row_4;
    }();
    manifest.modules.back() = [&] {
        auto row_4{context.storage.declarations.normal_modules.clone(manifest.modules.back())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_4)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_4)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
                context.storage.declarations.packed_values
                    .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.packed_values
                            .segments[context.storage.declarations.packed_values.index(row_2)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                        context.storage.declarations.packed_fields
                            .maximum_value[context.storage.declarations.packed_fields.index(
                                row_0)] = 10;
                        return row_0;
                    }();
                    return context.storage.declarations.range(rows_1);
                }();
                return row_2;
            }();
            return context.storage.declarations.range(rows_3);
        }();
        return row_4;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);

    manifest.modules.back() = [&] {
        auto row_4{context.storage.declarations.normal_modules.clone(manifest.modules.back())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_4)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_4)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
                context.storage.declarations.packed_values
                    .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.packed_values
                            .segments[context.storage.declarations.packed_values.index(row_2)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                        context.storage.declarations.packed_fields
                            .minimum_value[context.storage.declarations.packed_fields.index(row_0)]
                            .reset();
                        return row_0;
                    }();
                    return context.storage.declarations.range(rows_1);
                }();
                return row_2;
            }();
            return context.storage.declarations.range(rows_3);
        }();
        return row_4;
    }();
    manifest.modules.back() = [&] {
        auto row_4{context.storage.declarations.normal_modules.clone(manifest.modules.back())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_4)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_4)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
                context.storage.declarations.packed_values
                    .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.packed_values
                            .segments[context.storage.declarations.packed_values.index(row_2)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                        context.storage.declarations.packed_fields
                            .maximum_value[context.storage.declarations.packed_fields.index(row_0)]
                            .reset();
                        return row_0;
                    }();
                    return context.storage.declarations.range(rows_1);
                }();
                return row_2;
            }();
            return context.storage.declarations.range(rows_3);
        }();
        return row_4;
    }();
    manifest.modules.back() = [&] {
        auto row_4{context.storage.declarations.normal_modules.clone(manifest.modules.back())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_4)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_4)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
                context.storage.declarations.packed_values
                    .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.packed_values
                            .segments[context.storage.declarations.packed_values.index(row_2)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                        context.storage.declarations.packed_fields
                            .named_codes[context.storage.declarations.packed_fields.index(
                                row_0)] = [&] {
                            auto const original_items{context.storage.declarations.items(
                                context.storage.declarations.packed_fields
                                    .named_codes[context.storage.declarations.packed_fields.index(
                                        row_0)])};
                            auto rows{std::vector(original_items.begin(), original_items.end())};
                            rows.push_back([&] {
                                auto const new_packed_named_codes{
                                    context.storage.declarations.packed_named_codes.append()};
                                context.storage.declarations.packed_named_codes
                                    .name[context.storage.declarations.packed_named_codes.index(
                                        new_packed_named_codes)] =
                                    context.storage.strings.intern("Zero");
                                context.storage.declarations.packed_named_codes
                                    .value[context.storage.declarations.packed_named_codes.index(
                                        new_packed_named_codes)] = 0;
                                context.storage.declarations.packed_named_codes
                                    .sentinel[context.storage.declarations.packed_named_codes.index(
                                        new_packed_named_codes)] = false;
                                return new_packed_named_codes;
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
        return row_4;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);

    manifest.modules.back() = [&] {
        auto row_4{context.storage.declarations.normal_modules.clone(manifest.modules.back())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_4)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_4)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
                context.storage.declarations.packed_values
                    .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.packed_values
                            .segments[context.storage.declarations.packed_values.index(row_2)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                        context.storage.declarations.packed_fields
                            .named_codes[context.storage.declarations.packed_fields.index(row_0)] =
                            {};
                        return row_0;
                    }();
                    return context.storage.declarations.range(rows_1);
                }();
                return row_2;
            }();
            return context.storage.declarations.range(rows_3);
        }();
        return row_4;
    }();
    manifest.modules.back() = [&] {
        auto row_4{context.storage.declarations.normal_modules.clone(manifest.modules.back())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_4)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_4)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
                context.storage.declarations.packed_values
                    .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.packed_values
                            .segments[context.storage.declarations.packed_values.index(row_2)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                        context.storage.declarations.packed_fields
                            .range_helper[context.storage.declarations.packed_fields.index(row_0)] =
                            true;
                        return row_0;
                    }();
                    return context.storage.declarations.range(rows_1);
                }();
                return row_2;
            }();
            return context.storage.declarations.range(rows_3);
        }();
        return row_4;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);

    manifest.modules.back() = [&] {
        auto row_4{context.storage.declarations.normal_modules.clone(manifest.modules.back())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_4)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_4)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
                context.storage.declarations.packed_values
                    .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.packed_values
                            .segments[context.storage.declarations.packed_values.index(row_2)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                        context.storage.declarations.packed_fields
                            .range_helper[context.storage.declarations.packed_fields.index(row_0)] =
                            false;
                        return row_0;
                    }();
                    return context.storage.declarations.range(rows_1);
                }();
                return row_2;
            }();
            return context.storage.declarations.range(rows_3);
        }();
        return row_4;
    }();
    manifest.modules.back() = [&] {
        auto row_4{context.storage.declarations.normal_modules.clone(manifest.modules.back())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_4)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_4)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
                context.storage.declarations.packed_values
                    .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.packed_values
                            .segments[context.storage.declarations.packed_values.index(row_2)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                        context.storage.declarations.packed_fields
                            .relationship[context.storage.declarations.packed_fields.index(row_0)] =
                            [&] {
                                auto const new_semantic_relations{
                                    context.storage.declarations.semantic_relations.append()};
                                context.storage.declarations.semantic_relations
                                    .kind[context.storage.declarations.semantic_relations.index(
                                        new_semantic_relations)] = SemanticRelationKind::encoded_as;
                                context.storage.declarations.semantic_relations
                                    .target[context.storage.declarations.semantic_relations.index(
                                        new_semantic_relations)] = [&] {
                                    auto const new_type_refs{
                                        context.storage.declarations.type_refs.append()};
                                    context.storage.declarations.type_refs
                                        .name[context.storage.declarations.type_refs.index(
                                            new_type_refs)] =
                                        context.storage.strings.intern("project::VelocityQ8_4");
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
        return row_4;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);
}

TEST(PackedValue, LowersMiniFloatPlacementAsRawEncodedBits) {
    CompilerTestContext context;
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
        auto row_2{context.storage.declarations.normal_modules.clone(wide.modules.front())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_2)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_2)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{context.storage.declarations.mini_floats.clone(rows_1.front())};
                context.storage.declarations.mini_floats
                    .significand_bits[context.storage.declarations.mini_floats.index(row_0)] = 58;
                return row_0;
            }();
            return context.storage.declarations.range(rows_1);
        }();
        return row_2;
    }();

    wide.modules.back() = [&] {
        auto row_2{context.storage.declarations.normal_modules.clone(wide.modules.back())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_2)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_2)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{context.storage.declarations.packed_values.clone(rows_1.front())};
                context.storage.declarations.packed_values
                    .storage_type[context.storage.declarations.packed_values.index(row_0)] = [&] {
                    auto const new_type_refs{context.storage.declarations.type_refs.append()};
                    context.storage.declarations.type_refs
                        .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                        context.storage.strings.intern("std::uint64_t");
                    return new_type_refs;
                }();
                return row_0;
            }();
            return context.storage.declarations.range(rows_1);
        }();
        return row_2;
    }();
    wide.modules.back() = [&] {
        auto row_2{context.storage.declarations.normal_modules.clone(wide.modules.back())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_2)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_2)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{context.storage.declarations.packed_values.clone(rows_1.front())};
                context.storage.declarations.packed_values
                    .segments[context.storage.declarations.packed_values.index(row_0)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.packed_values
                            .segments[context.storage.declarations.packed_values.index(row_0)])};
                    auto rows{std::vector(original_items.begin(), original_items.end())};
                    rows.resize(1);
                    return context.storage.declarations.range(rows);
                }();
                return row_0;
            }();
            return context.storage.declarations.range(rows_1);
        }();
        return row_2;
    }();
    wide.modules.back() = [&] {
        auto row_2{context.storage.declarations.normal_modules.clone(wide.modules.back())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_2)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_2)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.front() = [&] {
                auto row_0{context.storage.declarations.packed_values.clone(rows_1.front())};
                context.storage.declarations.packed_values
                    .mutable_value[context.storage.declarations.packed_values.index(row_0)] = false;
                return row_0;
            }();
            return context.storage.declarations.range(rows_1);
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
    auto manifest{mini_float_backed_manifest(context)};

    manifest.modules.back() = [&] {
        auto row_4{context.storage.declarations.normal_modules.clone(manifest.modules.back())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_4)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_4)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
                context.storage.declarations.packed_values
                    .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.packed_values
                            .segments[context.storage.declarations.packed_values.index(row_2)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                        context.storage.declarations.packed_fields
                            .bits[context.storage.declarations.packed_fields.index(row_0)] = 11;
                        return row_0;
                    }();
                    return context.storage.declarations.range(rows_1);
                }();
                return row_2;
            }();
            return context.storage.declarations.range(rows_3);
        }();
        return row_4;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);
    manifest.modules.back() = [&] {
        auto row_4{context.storage.declarations.normal_modules.clone(manifest.modules.back())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_4)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_4)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
                context.storage.declarations.packed_values
                    .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.packed_values
                            .segments[context.storage.declarations.packed_values.index(row_2)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                        context.storage.declarations.packed_fields
                            .bits[context.storage.declarations.packed_fields.index(row_0)]
                            .reset();
                        return row_0;
                    }();
                    return context.storage.declarations.range(rows_1);
                }();
                return row_2;
            }();
            return context.storage.declarations.range(rows_3);
        }();
        return row_4;
    }();
    manifest.modules.back() = [&] {
        auto row_4{context.storage.declarations.normal_modules.clone(manifest.modules.back())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_4)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_4)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
                context.storage.declarations.packed_values
                    .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.packed_values
                            .segments[context.storage.declarations.packed_values.index(row_2)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                        context.storage.declarations.packed_fields
                            .kind[context.storage.declarations.packed_fields.index(row_0)] =
                            PackedFieldKind::unsigned_integer;
                        return row_0;
                    }();
                    return context.storage.declarations.range(rows_1);
                }();
                return row_2;
            }();
            return context.storage.declarations.range(rows_3);
        }();
        return row_4;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);
    manifest.modules.back() = [&] {
        auto row_4{context.storage.declarations.normal_modules.clone(manifest.modules.back())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_4)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_4)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
                context.storage.declarations.packed_values
                    .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.packed_values
                            .segments[context.storage.declarations.packed_values.index(row_2)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                        context.storage.declarations.packed_fields
                            .kind[context.storage.declarations.packed_fields.index(row_0)] =
                            PackedFieldKind::mini_float;
                        return row_0;
                    }();
                    return context.storage.declarations.range(rows_1);
                }();
                return row_2;
            }();
            return context.storage.declarations.range(rows_3);
        }();
        return row_4;
    }();
    manifest.modules.back() = [&] {
        auto row_4{context.storage.declarations.normal_modules.clone(manifest.modules.back())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_4)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_4)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
                context.storage.declarations.packed_values
                    .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.packed_values
                            .segments[context.storage.declarations.packed_values.index(row_2)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                        context.storage.declarations.packed_fields
                            .minimum_value[context.storage.declarations.packed_fields.index(
                                row_0)] = 0;
                        return row_0;
                    }();
                    return context.storage.declarations.range(rows_1);
                }();
                return row_2;
            }();
            return context.storage.declarations.range(rows_3);
        }();
        return row_4;
    }();
    manifest.modules.back() = [&] {
        auto row_4{context.storage.declarations.normal_modules.clone(manifest.modules.back())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_4)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_4)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
                context.storage.declarations.packed_values
                    .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.packed_values
                            .segments[context.storage.declarations.packed_values.index(row_2)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                        context.storage.declarations.packed_fields
                            .maximum_value[context.storage.declarations.packed_fields.index(
                                row_0)] = 1;
                        return row_0;
                    }();
                    return context.storage.declarations.range(rows_1);
                }();
                return row_2;
            }();
            return context.storage.declarations.range(rows_3);
        }();
        return row_4;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);
    manifest.modules.back() = [&] {
        auto row_4{context.storage.declarations.normal_modules.clone(manifest.modules.back())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_4)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_4)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
                context.storage.declarations.packed_values
                    .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.packed_values
                            .segments[context.storage.declarations.packed_values.index(row_2)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                        context.storage.declarations.packed_fields
                            .minimum_value[context.storage.declarations.packed_fields.index(row_0)]
                            .reset();
                        return row_0;
                    }();
                    return context.storage.declarations.range(rows_1);
                }();
                return row_2;
            }();
            return context.storage.declarations.range(rows_3);
        }();
        return row_4;
    }();
    manifest.modules.back() = [&] {
        auto row_4{context.storage.declarations.normal_modules.clone(manifest.modules.back())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_4)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_4)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
                context.storage.declarations.packed_values
                    .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.packed_values
                            .segments[context.storage.declarations.packed_values.index(row_2)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                        context.storage.declarations.packed_fields
                            .maximum_value[context.storage.declarations.packed_fields.index(row_0)]
                            .reset();
                        return row_0;
                    }();
                    return context.storage.declarations.range(rows_1);
                }();
                return row_2;
            }();
            return context.storage.declarations.range(rows_3);
        }();
        return row_4;
    }();
    manifest.modules.back() = [&] {
        auto row_4{context.storage.declarations.normal_modules.clone(manifest.modules.back())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_4)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_4)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
                context.storage.declarations.packed_values
                    .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.packed_values
                            .segments[context.storage.declarations.packed_values.index(row_2)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                        context.storage.declarations.packed_fields
                            .named_codes[context.storage.declarations.packed_fields.index(
                                row_0)] = [&] {
                            auto const original_items{context.storage.declarations.items(
                                context.storage.declarations.packed_fields
                                    .named_codes[context.storage.declarations.packed_fields.index(
                                        row_0)])};
                            auto rows{std::vector(original_items.begin(), original_items.end())};
                            rows.push_back([&] {
                                auto const new_packed_named_codes{
                                    context.storage.declarations.packed_named_codes.append()};
                                context.storage.declarations.packed_named_codes
                                    .name[context.storage.declarations.packed_named_codes.index(
                                        new_packed_named_codes)] =
                                    context.storage.strings.intern("Zero");
                                context.storage.declarations.packed_named_codes
                                    .value[context.storage.declarations.packed_named_codes.index(
                                        new_packed_named_codes)] = 0;
                                context.storage.declarations.packed_named_codes
                                    .sentinel[context.storage.declarations.packed_named_codes.index(
                                        new_packed_named_codes)] = false;
                                return new_packed_named_codes;
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
        return row_4;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);
    manifest.modules.back() = [&] {
        auto row_4{context.storage.declarations.normal_modules.clone(manifest.modules.back())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_4)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_4)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
                context.storage.declarations.packed_values
                    .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.packed_values
                            .segments[context.storage.declarations.packed_values.index(row_2)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                        context.storage.declarations.packed_fields
                            .named_codes[context.storage.declarations.packed_fields.index(row_0)] =
                            {};
                        return row_0;
                    }();
                    return context.storage.declarations.range(rows_1);
                }();
                return row_2;
            }();
            return context.storage.declarations.range(rows_3);
        }();
        return row_4;
    }();
    manifest.modules.back() = [&] {
        auto row_4{context.storage.declarations.normal_modules.clone(manifest.modules.back())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_4)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_4)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
                context.storage.declarations.packed_values
                    .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.packed_values
                            .segments[context.storage.declarations.packed_values.index(row_2)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                        context.storage.declarations.packed_fields
                            .range_helper[context.storage.declarations.packed_fields.index(row_0)] =
                            true;
                        return row_0;
                    }();
                    return context.storage.declarations.range(rows_1);
                }();
                return row_2;
            }();
            return context.storage.declarations.range(rows_3);
        }();
        return row_4;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);
    manifest.modules.back() = [&] {
        auto row_4{context.storage.declarations.normal_modules.clone(manifest.modules.back())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_4)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_4)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
                context.storage.declarations.packed_values
                    .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.packed_values
                            .segments[context.storage.declarations.packed_values.index(row_2)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                        context.storage.declarations.packed_fields
                            .range_helper[context.storage.declarations.packed_fields.index(row_0)] =
                            false;
                        return row_0;
                    }();
                    return context.storage.declarations.range(rows_1);
                }();
                return row_2;
            }();
            return context.storage.declarations.range(rows_3);
        }();
        return row_4;
    }();
    manifest.modules.back() = [&] {
        auto row_4{context.storage.declarations.normal_modules.clone(manifest.modules.back())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_4)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_4)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
                context.storage.declarations.packed_values
                    .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.packed_values
                            .segments[context.storage.declarations.packed_values.index(row_2)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                        context.storage.declarations.packed_fields
                            .relationship[context.storage.declarations.packed_fields.index(row_0)] =
                            [&] {
                                auto const new_semantic_relations{
                                    context.storage.declarations.semantic_relations.append()};
                                context.storage.declarations.semantic_relations
                                    .kind[context.storage.declarations.semantic_relations.index(
                                        new_semantic_relations)] = SemanticRelationKind::encoded_as;
                                context.storage.declarations.semantic_relations
                                    .target[context.storage.declarations.semantic_relations.index(
                                        new_semantic_relations)] = [&] {
                                    auto const new_type_refs{
                                        context.storage.declarations.type_refs.append()};
                                    context.storage.declarations.type_refs
                                        .name[context.storage.declarations.type_refs.index(
                                            new_type_refs)] =
                                        context.storage.strings.intern("project::PositionF12");
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
        return row_4;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);
}

TEST(PackedValue, RejectsCompetingOrMismatchedIntegerScalarFieldDomain) {
    CompilerTestContext context;
    auto module{valid_module(context)};
    auto manifest{scalar_backed_manifest(context, module)};

    manifest.modules.back() = [&] {
        auto row_4{context.storage.declarations.normal_modules.clone(manifest.modules.back())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_4)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_4)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
                context.storage.declarations.packed_values
                    .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.packed_values
                            .segments[context.storage.declarations.packed_values.index(row_2)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                        context.storage.declarations.packed_fields
                            .minimum_value[context.storage.declarations.packed_fields.index(
                                row_0)] = 0;
                        return row_0;
                    }();
                    return context.storage.declarations.range(rows_1);
                }();
                return row_2;
            }();
            return context.storage.declarations.range(rows_3);
        }();
        return row_4;
    }();
    manifest.modules.back() = [&] {
        auto row_4{context.storage.declarations.normal_modules.clone(manifest.modules.back())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_4)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_4)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
                context.storage.declarations.packed_values
                    .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.packed_values
                            .segments[context.storage.declarations.packed_values.index(row_2)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                        context.storage.declarations.packed_fields
                            .maximum_value[context.storage.declarations.packed_fields.index(
                                row_0)] = 1000;
                        return row_0;
                    }();
                    return context.storage.declarations.range(rows_1);
                }();
                return row_2;
            }();
            return context.storage.declarations.range(rows_3);
        }();
        return row_4;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);

    manifest.modules.back() = [&] {
        auto row_4{context.storage.declarations.normal_modules.clone(manifest.modules.back())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_4)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_4)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
                context.storage.declarations.packed_values
                    .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.packed_values
                            .segments[context.storage.declarations.packed_values.index(row_2)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                        context.storage.declarations.packed_fields
                            .minimum_value[context.storage.declarations.packed_fields.index(row_0)]
                            .reset();
                        return row_0;
                    }();
                    return context.storage.declarations.range(rows_1);
                }();
                return row_2;
            }();
            return context.storage.declarations.range(rows_3);
        }();
        return row_4;
    }();
    manifest.modules.back() = [&] {
        auto row_4{context.storage.declarations.normal_modules.clone(manifest.modules.back())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_4)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_4)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
                context.storage.declarations.packed_values
                    .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.packed_values
                            .segments[context.storage.declarations.packed_values.index(row_2)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                        context.storage.declarations.packed_fields
                            .maximum_value[context.storage.declarations.packed_fields.index(row_0)]
                            .reset();
                        return row_0;
                    }();
                    return context.storage.declarations.range(rows_1);
                }();
                return row_2;
            }();
            return context.storage.declarations.range(rows_3);
        }();
        return row_4;
    }();
    manifest.modules.back() = [&] {
        auto row_4{context.storage.declarations.normal_modules.clone(manifest.modules.back())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_4)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_4)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
                context.storage.declarations.packed_values
                    .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.packed_values
                            .segments[context.storage.declarations.packed_values.index(row_2)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                        context.storage.declarations.packed_fields
                            .kind[context.storage.declarations.packed_fields.index(row_0)] =
                            PackedFieldKind::signed_integer;
                        return row_0;
                    }();
                    return context.storage.declarations.range(rows_1);
                }();
                return row_2;
            }();
            return context.storage.declarations.range(rows_3);
        }();
        return row_4;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);

    manifest.modules.back() = [&] {
        auto row_4{context.storage.declarations.normal_modules.clone(manifest.modules.back())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_4)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_4)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
                context.storage.declarations.packed_values
                    .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.packed_values
                            .segments[context.storage.declarations.packed_values.index(row_2)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                        context.storage.declarations.packed_fields
                            .kind[context.storage.declarations.packed_fields.index(row_0)] =
                            PackedFieldKind::unsigned_integer;
                        return row_0;
                    }();
                    return context.storage.declarations.range(rows_1);
                }();
                return row_2;
            }();
            return context.storage.declarations.range(rows_3);
        }();
        return row_4;
    }();
    manifest.modules.back() = [&] {
        auto row_4{context.storage.declarations.normal_modules.clone(manifest.modules.back())};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(row_4)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(row_4)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
                context.storage.declarations.packed_values
                    .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.packed_values
                            .segments[context.storage.declarations.packed_values.index(row_2)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[0] = [&] {
                        auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                        context.storage.declarations.packed_fields
                            .bits[context.storage.declarations.packed_fields.index(row_0)] = 11;
                        return row_0;
                    }();
                    return context.storage.declarations.range(rows_1);
                }();
                return row_2;
            }();
            return context.storage.declarations.range(rows_3);
        }();
        return row_4;
    }();
    EXPECT_THROW(lower_test_manifest(context, manifest), std::invalid_argument);
}

TEST(PackedValue, RejectsInvalidLayoutsAndTypes) {
    CompilerTestContext context;
    auto module{context.storage.declarations.normal_modules.clone(valid_module(context))};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .bits[context.storage.declarations.packed_fields.index(row_0)] = 0;
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .bits[context.storage.declarations.packed_fields.index(row_0)] = 25;
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.packed_values.clone(rows_1.front())};
            context.storage.declarations.packed_values
                .storage_type[context.storage.declarations.packed_values.index(row_0)] = [&] {
                auto const new_type_refs{context.storage.declarations.type_refs.append()};
                context.storage.declarations.type_refs
                    .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                    context.storage.strings.intern("int32");
                return new_type_refs;
            }();
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .type[context.storage.declarations.packed_fields.index(row_0)] = [&] {
                        auto const new_type_refs{context.storage.declarations.type_refs.append()};
                        context.storage.declarations.type_refs
                            .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                            context.storage.strings.intern("int32");
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
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .kind[context.storage.declarations.packed_fields.index(row_0)] =
                        PackedFieldKind::signed_integer;
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .type[context.storage.declarations.packed_fields.index(row_0)] = [&] {
                        auto const new_type_refs{context.storage.declarations.type_refs.append()};
                        context.storage.declarations.type_refs
                            .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                            context.storage.strings.intern("int32");
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
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .kind[context.storage.declarations.packed_fields.index(row_0)] =
                        PackedFieldKind::signed_integer;
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
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .bits[context.storage.declarations.packed_fields.index(row_0)]
                        .reset();
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .type[context.storage.declarations.packed_fields.index(row_0)] = [&] {
                        auto const new_type_refs{context.storage.declarations.type_refs.append()};
                        context.storage.declarations.type_refs
                            .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                            context.storage.strings.intern("int16");
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
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .kind[context.storage.declarations.packed_fields.index(row_0)] =
                        PackedFieldKind::signed_integer;
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
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .bits[context.storage.declarations.packed_fields.index(row_0)] = 17;
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_2{std::vector(original_items.begin(), original_items.end())};
        rows_2.front() = [&] {
            auto row_1{context.storage.declarations.packed_values.clone(rows_2.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_1)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_1)])};
                auto rows_0{std::vector(original_items.begin(), original_items.end())};
                rows_0.front() = [&] {
                    auto const new_packed_fields{
                        context.storage.declarations.packed_fields.append()};
                    context.storage.declarations.packed_fields
                        .name[context.storage.declarations.packed_fields.index(new_packed_fields)] =
                        context.storage.strings.intern("flag");
                    context.storage.declarations.packed_fields
                        .type[context.storage.declarations.packed_fields.index(new_packed_fields)] =
                        [&] {
                            auto const new_type_refs{
                                context.storage.declarations.type_refs.append()};
                            context.storage.declarations.type_refs
                                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                                context.storage.strings.intern("bool");
                            return new_type_refs;
                        }();
                    context.storage.declarations.packed_fields
                        .bits[context.storage.declarations.packed_fields.index(new_packed_fields)] =
                        2;
                    return new_packed_fields;
                }();
                return context.storage.declarations.range(rows_0);
            }();
            return row_1;
        }();
        return context.storage.declarations.range(rows_2);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    context.storage.declarations.normal_modules
        .settings[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto row_0{context.storage.declarations.module_settings_store.clone(
            context.storage.declarations.normal_modules
                .settings[context.storage.declarations.normal_modules.index(module)])};
        context.storage.declarations.module_settings_store
            .source[context.storage.declarations.module_settings_store.index(row_0)] =
            context.storage.strings.add("Packed.cpp");
        return row_0;
    }();
    EXPECT_EQ(render_test_manifest(context,
                                   Manifest{manifest_schema_version,
                                            make_test_registry(context),
                                            context.storage.declarations,
                                            {module}})
                  .size(),
              2U);

    module = valid_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[1])};
                    context.storage.declarations.packed_fields
                        .range_helper[context.storage.declarations.packed_fields.index(row_0)] =
                        true;
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.packed_values.clone(rows_1.front())};
            context.storage.declarations.packed_values
                .invalid_value[context.storage.declarations.packed_values.index(row_0)] =
                std::uint64_t{1} << 32;
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.packed_values.clone(rows_1.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_0)] =
                context.storage.declarations.range<PackedSegmentId>({[&] {
                    auto const new_packed_reserved_bits{
                        context.storage.declarations.packed_reserved_bits.append()};
                    context.storage.declarations.packed_reserved_bits
                        .name[context.storage.declarations.packed_reserved_bits.index(
                            new_packed_reserved_bits)] = context.storage.strings.intern("future");
                    context.storage.declarations.packed_reserved_bits
                        .bits[context.storage.declarations.packed_reserved_bits.index(
                            new_packed_reserved_bits)] = 32;
                    return new_packed_reserved_bits;
                }()});
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.packed_values.clone(rows_1.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_0)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_0)])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.insert(rows.begin() + 1, [&] {
                    auto const new_packed_reserved_bits{
                        context.storage.declarations.packed_reserved_bits.append()};
                    context.storage.declarations.packed_reserved_bits
                        .name[context.storage.declarations.packed_reserved_bits.index(
                            new_packed_reserved_bits)] = context.storage.strings.intern("future");
                    context.storage.declarations.packed_reserved_bits
                        .bits[context.storage.declarations.packed_reserved_bits.index(
                            new_packed_reserved_bits)] = 0;
                    return new_packed_reserved_bits;
                }());
                return context.storage.declarations.range(rows);
            }();
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.packed_values.clone(rows_1.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_0)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_0)])};
                auto rows{std::vector(original_items.begin(), original_items.end())};
                rows.insert(rows.begin() + 1, [&] {
                    auto const new_packed_reserved_bits{
                        context.storage.declarations.packed_reserved_bits.append()};
                    context.storage.declarations.packed_reserved_bits
                        .name[context.storage.declarations.packed_reserved_bits.index(
                            new_packed_reserved_bits)] =
                        context.storage.strings.intern("entity_index");
                    context.storage.declarations.packed_reserved_bits
                        .bits[context.storage.declarations.packed_reserved_bits.index(
                            new_packed_reserved_bits)] = 1;
                    return new_packed_reserved_bits;
                }());
                return context.storage.declarations.range(rows);
            }();
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .minimum_value[context.storage.declarations.packed_fields.index(row_0)] = 1;
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .bits[context.storage.declarations.packed_fields.index(row_0)]
                        .reset();
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[1])};
                    context.storage.declarations.packed_fields
                        .bits[context.storage.declarations.packed_fields.index(row_0)]
                        .reset();
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .minimum_value[context.storage.declarations.packed_fields.index(row_0)] =
                        100;
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
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .maximum_value[context.storage.declarations.packed_fields.index(row_0)] =
                        10;
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .minimum_value[context.storage.declarations.packed_fields.index(row_0)] =
                        -1;
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
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .maximum_value[context.storage.declarations.packed_fields.index(row_0)] =
                        100;
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .type[context.storage.declarations.packed_fields.index(row_0)] = [&] {
                        auto const new_type_refs{context.storage.declarations.type_refs.append()};
                        context.storage.declarations.type_refs
                            .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                            context.storage.strings.intern("std::int32_t");
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
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .kind[context.storage.declarations.packed_fields.index(row_0)] =
                        PackedFieldKind::signed_integer;
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
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .bits[context.storage.declarations.packed_fields.index(row_0)] = 7;
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
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .minimum_value[context.storage.declarations.packed_fields.index(row_0)] =
                        -100;
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
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .maximum_value[context.storage.declarations.packed_fields.index(row_0)] =
                        100;
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .type[context.storage.declarations.packed_fields.index(row_0)] = [&] {
                        auto const new_type_refs{context.storage.declarations.type_refs.append()};
                        context.storage.declarations.type_refs
                            .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                            context.storage.strings.intern("std::int32_t");
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
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .kind[context.storage.declarations.packed_fields.index(row_0)] =
                        PackedFieldKind::signed_integer;
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
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .bits[context.storage.declarations.packed_fields.index(row_0)] = 8;
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
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .minimum_value[context.storage.declarations.packed_fields.index(row_0)] =
                        -100;
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
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .maximum_value[context.storage.declarations.packed_fields.index(row_0)] =
                        100;
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
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .named_codes[context.storage.declarations.packed_fields.index(row_0)] =
                        context.storage.declarations.range<PackedNamedCodeId>({[&] {
                            auto const new_packed_named_codes{
                                context.storage.declarations.packed_named_codes.append()};
                            context.storage.declarations.packed_named_codes
                                .name[context.storage.declarations.packed_named_codes.index(
                                    new_packed_named_codes)] =
                                context.storage.strings.intern("Invalid");
                            context.storage.declarations.packed_named_codes
                                .value[context.storage.declarations.packed_named_codes.index(
                                    new_packed_named_codes)] = 0;
                            context.storage.declarations.packed_named_codes
                                .sentinel[context.storage.declarations.packed_named_codes.index(
                                    new_packed_named_codes)] = true;
                            return new_packed_named_codes;
                        }()});
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .minimum_value[context.storage.declarations.packed_fields.index(row_0)] = 0;
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
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .maximum_value[context.storage.declarations.packed_fields.index(row_0)] =
                        std::uint64_t{1} << 24;
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[1])};
                    context.storage.declarations.packed_fields
                        .minimum_value[context.storage.declarations.packed_fields.index(row_0)] = 0;
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
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[1])};
                    context.storage.declarations.packed_fields
                        .maximum_value[context.storage.declarations.packed_fields.index(row_0)] = 3;
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .named_codes[context.storage.declarations.packed_fields.index(row_0)] =
                        context.storage.declarations.range<PackedNamedCodeId>({[&] {
                            auto const new_packed_named_codes{
                                context.storage.declarations.packed_named_codes.append()};
                            context.storage.declarations.packed_named_codes
                                .name[context.storage.declarations.packed_named_codes.index(
                                    new_packed_named_codes)] =
                                context.storage.strings.intern("Invalid");
                            context.storage.declarations.packed_named_codes
                                .value[context.storage.declarations.packed_named_codes.index(
                                    new_packed_named_codes)] = 0xffffff;
                            context.storage.declarations.packed_named_codes
                                .sentinel[context.storage.declarations.packed_named_codes.index(
                                    new_packed_named_codes)] = true;
                            return new_packed_named_codes;
                        }()});
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .minimum_value[context.storage.declarations.packed_fields.index(row_0)] = 0;
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
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .maximum_value[context.storage.declarations.packed_fields.index(row_0)] =
                        100;
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
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .named_codes[context.storage.declarations.packed_fields.index(row_0)] =
                        context.storage.declarations.range<PackedNamedCodeId>({[&] {
                            auto const new_packed_named_codes{
                                context.storage.declarations.packed_named_codes.append()};
                            context.storage.declarations.packed_named_codes
                                .name[context.storage.declarations.packed_named_codes.index(
                                    new_packed_named_codes)] =
                                context.storage.strings.intern("Invalid");
                            context.storage.declarations.packed_named_codes
                                .value[context.storage.declarations.packed_named_codes.index(
                                    new_packed_named_codes)] = 100;
                            context.storage.declarations.packed_named_codes
                                .sentinel[context.storage.declarations.packed_named_codes.index(
                                    new_packed_named_codes)] = true;
                            return new_packed_named_codes;
                        }()});
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .minimum_value[context.storage.declarations.packed_fields.index(row_0)] = 0;
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
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .maximum_value[context.storage.declarations.packed_fields.index(row_0)] =
                        100;
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
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .named_codes[context.storage.declarations.packed_fields.index(row_0)] =
                        context.storage.declarations.range<PackedNamedCodeId>({[&] {
                            auto const new_packed_named_codes{
                                context.storage.declarations.packed_named_codes.append()};
                            context.storage.declarations.packed_named_codes
                                .name[context.storage.declarations.packed_named_codes.index(
                                    new_packed_named_codes)] =
                                context.storage.strings.intern("Named");
                            context.storage.declarations.packed_named_codes
                                .value[context.storage.declarations.packed_named_codes.index(
                                    new_packed_named_codes)] = 101;
                            context.storage.declarations.packed_named_codes
                                .sentinel[context.storage.declarations.packed_named_codes.index(
                                    new_packed_named_codes)] = false;
                            return new_packed_named_codes;
                        }()});
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .minimum_value[context.storage.declarations.packed_fields.index(row_0)] = 0;
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
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .maximum_value[context.storage.declarations.packed_fields.index(row_0)] =
                        100;
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
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .named_codes[context.storage.declarations.packed_fields.index(row_0)] =
                        context.storage.declarations.range<PackedNamedCodeId>({[&] {
                            auto const new_packed_named_codes{
                                context.storage.declarations.packed_named_codes.append()};
                            context.storage.declarations.packed_named_codes
                                .name[context.storage.declarations.packed_named_codes.index(
                                    new_packed_named_codes)] =
                                context.storage.strings.intern("Invalid");
                            context.storage.declarations.packed_named_codes
                                .value[context.storage.declarations.packed_named_codes.index(
                                    new_packed_named_codes)] = 0x1000000;
                            context.storage.declarations.packed_named_codes
                                .sentinel[context.storage.declarations.packed_named_codes.index(
                                    new_packed_named_codes)] = true;
                            return new_packed_named_codes;
                        }()});
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .minimum_value[context.storage.declarations.packed_fields.index(row_0)] = 0;
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
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .maximum_value[context.storage.declarations.packed_fields.index(row_0)] =
                        100;
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
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .named_codes[context.storage.declarations.packed_fields.index(row_0)] =
                        context.storage.declarations.range<PackedNamedCodeId>(
                            {[&] {
                                 auto const new_packed_named_codes{
                                     context.storage.declarations.packed_named_codes.append()};
                                 context.storage.declarations.packed_named_codes
                                     .name[context.storage.declarations.packed_named_codes.index(
                                         new_packed_named_codes)] =
                                     context.storage.strings.intern("Invalid");
                                 context.storage.declarations.packed_named_codes
                                     .value[context.storage.declarations.packed_named_codes.index(
                                         new_packed_named_codes)] = 0xffffff;
                                 context.storage.declarations.packed_named_codes
                                     .sentinel[context.storage.declarations.packed_named_codes
                                                   .index(new_packed_named_codes)] = true;
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
                                         new_packed_named_codes)] = 0xfffffe;
                                 context.storage.declarations.packed_named_codes
                                     .sentinel[context.storage.declarations.packed_named_codes
                                                   .index(new_packed_named_codes)] = true;
                                 return new_packed_named_codes;
                             }()});
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .minimum_value[context.storage.declarations.packed_fields.index(row_0)] = 0;
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
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .maximum_value[context.storage.declarations.packed_fields.index(row_0)] =
                        100;
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
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .named_codes[context.storage.declarations.packed_fields.index(row_0)] =
                        context.storage.declarations.range<PackedNamedCodeId>(
                            {[&] {
                                 auto const new_packed_named_codes{
                                     context.storage.declarations.packed_named_codes.append()};
                                 context.storage.declarations.packed_named_codes
                                     .name[context.storage.declarations.packed_named_codes.index(
                                         new_packed_named_codes)] =
                                     context.storage.strings.intern("Invalid");
                                 context.storage.declarations.packed_named_codes
                                     .value[context.storage.declarations.packed_named_codes.index(
                                         new_packed_named_codes)] = 0xffffff;
                                 context.storage.declarations.packed_named_codes
                                     .sentinel[context.storage.declarations.packed_named_codes
                                                   .index(new_packed_named_codes)] = true;
                                 return new_packed_named_codes;
                             }(),
                             [&] {
                                 auto const new_packed_named_codes{
                                     context.storage.declarations.packed_named_codes.append()};
                                 context.storage.declarations.packed_named_codes
                                     .name[context.storage.declarations.packed_named_codes.index(
                                         new_packed_named_codes)] =
                                     context.storage.strings.intern("Pending");
                                 context.storage.declarations.packed_named_codes
                                     .value[context.storage.declarations.packed_named_codes.index(
                                         new_packed_named_codes)] = 0xffffff;
                                 context.storage.declarations.packed_named_codes
                                     .sentinel[context.storage.declarations.packed_named_codes
                                                   .index(new_packed_named_codes)] = true;
                                 return new_packed_named_codes;
                             }()});
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .relationship[context.storage.declarations.packed_fields.index(
                            row_0)] = [&] {
                        auto const new_semantic_relations{
                            context.storage.declarations.semantic_relations.append()};
                        context.storage.declarations.semantic_relations
                            .kind[context.storage.declarations.semantic_relations.index(
                                new_semantic_relations)] = SemanticRelationKind::discriminates;
                        context.storage.declarations.semantic_relations
                            .target[context.storage.declarations.semantic_relations.index(
                                new_semantic_relations)] = [&] {
                            auto const new_type_refs{
                                context.storage.declarations.type_refs.append()};
                            context.storage.declarations.type_refs
                                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                                context.storage.strings.intern("ExternalPayload");
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
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[1])};
                    context.storage.declarations.packed_fields
                        .relationship[context.storage.declarations.packed_fields.index(
                            row_0)] = [&] {
                        auto const new_semantic_relations{
                            context.storage.declarations.semantic_relations.append()};
                        context.storage.declarations.semantic_relations
                            .kind[context.storage.declarations.semantic_relations.index(
                                new_semantic_relations)] = SemanticRelationKind::index_into;
                        context.storage.declarations.semantic_relations
                            .target[context.storage.declarations.semantic_relations.index(
                                new_semantic_relations)] = [&] {
                            auto const new_type_refs{
                                context.storage.declarations.type_refs.append()};
                            context.storage.declarations.type_refs
                                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                                context.storage.strings.intern("ExternalTable");
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
    EXPECT_THROW(lower(context, module), std::invalid_argument);

    module = valid_module(context);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .relationship[context.storage.declarations.packed_fields.index(
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
                                context.storage.strings.intern("TypeDefinitionId");
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
    EXPECT_THROW(lower(context, module), std::invalid_argument);
}

TEST(PackedValue, RejectsGeneratedApiCollisions) {
    CompilerTestContext context;
    auto module{context.storage.declarations.normal_modules.clone(valid_module(context))};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.packed_values.clone(rows_1.front())};
            context.storage.declarations.packed_values
                .mutable_value[context.storage.declarations.packed_values.index(row_0)] = true;
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
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[1])};
                    context.storage.declarations.packed_fields
                        .name[context.storage.declarations.packed_fields.index(row_0)] =
                        context.storage.strings.intern("set_entity_index");
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);
}

TEST(PackedValue, RejectsDescriptorAndRawFactoryNameCollisions) {
    CompilerTestContext context;
    auto module{context.storage.declarations.normal_modules.clone(valid_module(context))};

    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[1])};
                    context.storage.declarations.packed_fields
                        .name[context.storage.declarations.packed_fields.index(row_0)] =
                        context.storage.strings.intern("entity_index_field");
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[1])};
                    context.storage.declarations.packed_fields
                        .name[context.storage.declarations.packed_fields.index(row_0)] =
                        context.storage.strings.intern("from_raw");
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[1])};
                    context.storage.declarations.packed_fields
                        .name[context.storage.declarations.packed_fields.index(row_0)] =
                        context.storage.strings.intern("entity_index_minimum");
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[1])};
                    context.storage.declarations.packed_fields
                        .name[context.storage.declarations.packed_fields.index(row_0)] =
                        context.storage.strings.intern("entity_index_maximum");
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower(context, module), std::invalid_argument);
}

TEST(PackedValue, ValidatesKnownEnumUnderlyingType) {
    CompilerTestContext context;
    auto module{context.storage.declarations.normal_modules.clone(valid_module(context))};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[1])};
                    context.storage.declarations.packed_fields
                        .type[context.storage.declarations.packed_fields.index(row_0)] = [&] {
                        auto const new_type_refs{context.storage.declarations.type_refs.append()};
                        context.storage.declarations.type_refs
                            .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                            context.storage.strings.intern("@state");
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
    auto const enums{context.storage.declarations.normal_modules.append()};
    context.storage.declarations.normal_modules
        .settings[context.storage.declarations.normal_modules.index(enums)] = [&] {
        auto const new_module_settings_store{
            context.storage.declarations.module_settings_store.append()};
        context.storage.declarations.module_settings_store
            .name[context.storage.declarations.module_settings_store.index(
                new_module_settings_store)] = context.storage.strings.intern("enums");
        context.storage.declarations.module_settings_store
            .header[context.storage.declarations.module_settings_store.index(
                new_module_settings_store)] = context.storage.strings.add("Enums.h");
        return new_module_settings_store;
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(enums)] =
        context.storage.declarations.range<DeclarationHandle>({[&] {
            auto const new_enums{context.storage.declarations.enums.append()};
            context.storage.declarations.enums
                .name[context.storage.declarations.enums.index(new_enums)] =
                context.storage.strings.intern("FighterStateKind");
            context.storage.declarations.enums
                .underlying_type[context.storage.declarations.enums.index(new_enums)] = [&] {
                auto const new_type_refs{context.storage.declarations.type_refs.append()};
                context.storage.declarations.type_refs
                    .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                    context.storage.strings.intern("int8");
                return new_type_refs;
            }();
            context.storage.declarations.enums
                .values[context.storage.declarations.enums.index(new_enums)] =
                context.storage.declarations.range<EnumeratorId>({[&] {
                    auto const new_enumerators{context.storage.declarations.enumerators.append()};
                    context.storage.declarations.enumerators
                        .name[context.storage.declarations.enumerators.index(new_enumerators)] =
                        context.storage.strings.intern("Value");
                    return new_enumerators;
                }()});
            return new_enums;
        }()});
    Manifest manifest{
        manifest_schema_version,
        make_test_registry(context, {{"state", context.storage.cpp_types.add("FighterStateKind")}}),
        context.storage.declarations,
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
    auto module{context.storage.declarations.normal_modules.clone(valid_module(context))};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.packed_values.clone(rows_1.front())};
            context.storage.declarations.packed_values
                .invalid_value[context.storage.declarations.packed_values.index(row_0)] =
                0x07ffffffu;
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
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[1])};
                    context.storage.declarations.packed_fields
                        .bits[context.storage.declarations.packed_fields.index(row_0)] = 4;
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    auto schema{[&] {
        auto const new_enums{context.storage.declarations.enums.append()};
        context.storage.declarations.enums
            .name[context.storage.declarations.enums.index(new_enums)] =
            context.storage.strings.intern("FighterStateKind");
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
            context.storage.declarations.range<EnumeratorId>(
                {[&] {
                     auto const new_enumerators{context.storage.declarations.enumerators.append()};
                     context.storage.declarations.enumerators
                         .name[context.storage.declarations.enumerators.index(new_enumerators)] =
                         context.storage.strings.intern("Zero");
                     context.storage.declarations.enumerators
                         .initializer[context.storage.declarations.enumerators.index(
                             new_enumerators)] = context.storage.strings.add("0");
                     return new_enumerators;
                 }(),
                 [&] {
                     auto const new_enumerators{context.storage.declarations.enumerators.append()};
                     context.storage.declarations.enumerators
                         .name[context.storage.declarations.enumerators.index(new_enumerators)] =
                         context.storage.strings.intern("High");
                     context.storage.declarations.enumerators
                         .initializer[context.storage.declarations.enumerators.index(
                             new_enumerators)] = context.storage.strings.add("7");
                     return new_enumerators;
                 }(),
                 [&] {
                     auto const new_enumerators{context.storage.declarations.enumerators.append()};
                     context.storage.declarations.enumerators
                         .name[context.storage.declarations.enumerators.index(new_enumerators)] =
                         context.storage.strings.intern("COUNT");
                     context.storage.declarations.enumerators
                         .initializer[context.storage.declarations.enumerators.index(
                             new_enumerators)] = context.storage.strings.add("8");
                     context.storage.declarations.enumerators
                         .display_name[context.storage.declarations.enumerators.index(
                             new_enumerators)] = std::nullopt;
                     context.storage.declarations.enumerators
                         .hidden[context.storage.declarations.enumerators.index(new_enumerators)] =
                         true;
                     return new_enumerators;
                 }()});
        context.storage.declarations.enums
            .count[context.storage.declarations.enums.index(new_enums)] =
            context.storage.strings.intern("COUNT");
        return new_enums;
    }()};

    auto const header{lower_known_enum(context, module, schema)};
    EXPECT_EQ(header.find("static_assert(static_cast<state_underlying_type>"), std::string::npos);
    EXPECT_EQ(header.find("static_assert(state"), std::string::npos);
    EXPECT_EQ(header.find("assert(static_cast<state_underlying_type>(state_value)"),
              std::string::npos);
    EXPECT_NE(header.find("assert(raw != invalid_value);"), std::string::npos);

    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[1])};
                    context.storage.declarations.packed_fields
                        .bits[context.storage.declarations.packed_fields.index(row_0)] = 3;
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_known_enum(context, module, schema), std::invalid_argument);
}

TEST(PackedValue, OmitsSentinelAssertionWhenTheKnownEnumDomainExcludesIt) {
    CompilerTestContext context;
    auto module{context.storage.declarations.normal_modules.clone(valid_module(context))};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_1{std::vector(original_items.begin(), original_items.end())};
        rows_1.front() = [&] {
            auto row_0{context.storage.declarations.packed_values.clone(rows_1.front())};
            context.storage.declarations.packed_values
                .invalid_value[context.storage.declarations.packed_values.index(row_0)] =
                0xffffffffu;
            return row_0;
        }();
        return context.storage.declarations.range(rows_1);
    }();
    auto const header{lower_known_enum(context, module, [&] {
        auto const new_enums{context.storage.declarations.enums.append()};
        context.storage.declarations.enums
            .name[context.storage.declarations.enums.index(new_enums)] =
            context.storage.strings.intern("FighterStateKind");
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
            context.storage.declarations.range<EnumeratorId>(
                {[&] {
                     auto const new_enumerators{context.storage.declarations.enumerators.append()};
                     context.storage.declarations.enumerators
                         .name[context.storage.declarations.enumerators.index(new_enumerators)] =
                         context.storage.strings.intern("Zero");
                     context.storage.declarations.enumerators
                         .initializer[context.storage.declarations.enumerators.index(
                             new_enumerators)] = context.storage.strings.add("0");
                     return new_enumerators;
                 }(),
                 [&] {
                     auto const new_enumerators{context.storage.declarations.enumerators.append()};
                     context.storage.declarations.enumerators
                         .name[context.storage.declarations.enumerators.index(new_enumerators)] =
                         context.storage.strings.intern("COUNT");
                     context.storage.declarations.enumerators
                         .initializer[context.storage.declarations.enumerators.index(
                             new_enumerators)] = context.storage.strings.add("5");
                     context.storage.declarations.enumerators
                         .display_name[context.storage.declarations.enumerators.index(
                             new_enumerators)] = std::nullopt;
                     context.storage.declarations.enumerators
                         .hidden[context.storage.declarations.enumerators.index(new_enumerators)] =
                         true;
                     return new_enumerators;
                 }()});
        context.storage.declarations.enums
            .count[context.storage.declarations.enums.index(new_enums)] =
            context.storage.strings.intern("COUNT");
        return new_enums;
    }())};

    EXPECT_EQ(header.find("assert(raw != invalid_value);"), std::string::npos);
}

TEST(PackedValue, ResolvesImplicitValuesAfterExplicitHexadecimalValues) {
    CompilerTestContext context;
    auto module{context.storage.declarations.normal_modules.clone(valid_module(context))};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[1])};
                    context.storage.declarations.packed_fields
                        .bits[context.storage.declarations.packed_fields.index(row_0)] = 4;
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    auto schema{[&] {
        auto const new_enums{context.storage.declarations.enums.append()};
        context.storage.declarations.enums
            .name[context.storage.declarations.enums.index(new_enums)] =
            context.storage.strings.intern("FighterStateKind");
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
            context.storage.declarations.range<EnumeratorId>(
                {[&] {
                     auto const new_enumerators{context.storage.declarations.enumerators.append()};
                     context.storage.declarations.enumerators
                         .name[context.storage.declarations.enumerators.index(new_enumerators)] =
                         context.storage.strings.intern("Zero");
                     context.storage.declarations.enumerators
                         .initializer[context.storage.declarations.enumerators.index(
                             new_enumerators)] = context.storage.strings.add("0");
                     return new_enumerators;
                 }(),
                 [&] {
                     auto const new_enumerators{context.storage.declarations.enumerators.append()};
                     context.storage.declarations.enumerators
                         .name[context.storage.declarations.enumerators.index(new_enumerators)] =
                         context.storage.strings.intern("High");
                     context.storage.declarations.enumerators
                         .initializer[context.storage.declarations.enumerators.index(
                             new_enumerators)] = context.storage.strings.add("0x7");
                     return new_enumerators;
                 }(),
                 [&] {
                     auto const new_enumerators{context.storage.declarations.enumerators.append()};
                     context.storage.declarations.enumerators
                         .name[context.storage.declarations.enumerators.index(new_enumerators)] =
                         context.storage.strings.intern("Next");
                     return new_enumerators;
                 }(),
                 [&] {
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
                 }()});
        context.storage.declarations.enums
            .count[context.storage.declarations.enums.index(new_enums)] =
            context.storage.strings.intern("COUNT");
        return new_enums;
    }()};

    EXPECT_NO_THROW(static_cast<void>(lower_known_enum(context, module, schema)));

    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[1])};
                    context.storage.declarations.packed_fields
                        .bits[context.storage.declarations.packed_fields.index(row_0)] = 3;
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    EXPECT_THROW(lower_known_enum(context, module, schema), std::invalid_argument);
}

TEST(PackedValue, RejectsKnownEnumValuesOutsideTheirUnderlyingType) {
    CompilerTestContext context;
    auto module{context.storage.declarations.normal_modules.clone(valid_module(context))};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[1])};
                    context.storage.declarations.packed_fields
                        .bits[context.storage.declarations.packed_fields.index(row_0)] = 8;
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    auto schema{[&] {
        auto const new_enums{context.storage.declarations.enums.append()};
        context.storage.declarations.enums
            .name[context.storage.declarations.enums.index(new_enums)] =
            context.storage.strings.intern("FighterStateKind");
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
            context.storage.declarations.range<EnumeratorId>(
                {[&] {
                     auto const new_enumerators{context.storage.declarations.enumerators.append()};
                     context.storage.declarations.enumerators
                         .name[context.storage.declarations.enumerators.index(new_enumerators)] =
                         context.storage.strings.intern("Zero");
                     context.storage.declarations.enumerators
                         .initializer[context.storage.declarations.enumerators.index(
                             new_enumerators)] = context.storage.strings.add("0");
                     return new_enumerators;
                 }(),
                 [&] {
                     auto const new_enumerators{context.storage.declarations.enumerators.append()};
                     context.storage.declarations.enumerators
                         .name[context.storage.declarations.enumerators.index(new_enumerators)] =
                         context.storage.strings.intern("Maximum");
                     context.storage.declarations.enumerators
                         .initializer[context.storage.declarations.enumerators.index(
                             new_enumerators)] = context.storage.strings.add("255");
                     return new_enumerators;
                 }()});
        return new_enums;
    }()};
    EXPECT_NO_THROW(static_cast<void>(lower_known_enum(context, module, schema)));

    context.storage.declarations.enums.values[context.storage.declarations.enums.index(schema)] =
        [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.enums
                    .values[context.storage.declarations.enums.index(schema)])};
            auto rows_1{std::vector(original_items.begin(), original_items.end())};
            rows_1.back() = [&] {
                auto row_0{context.storage.declarations.enumerators.clone(rows_1.back())};
                context.storage.declarations.enumerators
                    .initializer[context.storage.declarations.enumerators.index(row_0)] =
                    context.storage.strings.add("256");
                return row_0;
            }();
            return context.storage.declarations.range(rows_1);
        }();
    EXPECT_THROW(lower_known_enum(context, module, schema), std::invalid_argument);
}

TEST(PackedValue, AllowsEnumAliasesAndUsesStructuralCountSemantics) {
    CompilerTestContext context;
    auto module{context.storage.declarations.normal_modules.clone(valid_module(context))};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[1])};
                    context.storage.declarations.packed_fields
                        .bits[context.storage.declarations.packed_fields.index(row_0)] = 8;
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    auto schema{[&] {
        auto const new_enums{context.storage.declarations.enums.append()};
        context.storage.declarations.enums
            .name[context.storage.declarations.enums.index(new_enums)] =
            context.storage.strings.intern("FighterStateKind");
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
            context.storage.declarations.range<EnumeratorId>(
                {[&] {
                     auto const new_enumerators{context.storage.declarations.enumerators.append()};
                     context.storage.declarations.enumerators
                         .name[context.storage.declarations.enumerators.index(new_enumerators)] =
                         context.storage.strings.intern("Zero");
                     context.storage.declarations.enumerators
                         .initializer[context.storage.declarations.enumerators.index(
                             new_enumerators)] = context.storage.strings.add("0");
                     return new_enumerators;
                 }(),
                 [&] {
                     auto const new_enumerators{context.storage.declarations.enumerators.append()};
                     context.storage.declarations.enumerators
                         .name[context.storage.declarations.enumerators.index(new_enumerators)] =
                         context.storage.strings.intern("Alias");
                     context.storage.declarations.enumerators
                         .initializer[context.storage.declarations.enumerators.index(
                             new_enumerators)] = context.storage.strings.add("0");
                     return new_enumerators;
                 }()});
        return new_enums;
    }()};
    EXPECT_NO_THROW(static_cast<void>(lower_known_enum(context, module, schema)));

    context.storage.declarations.enums.values[context.storage.declarations.enums.index(schema)] =
        context.storage.declarations.range<EnumeratorId>(
            {[&] {
                 auto const new_enumerators{context.storage.declarations.enumerators.append()};
                 context.storage.declarations.enumerators
                     .name[context.storage.declarations.enumerators.index(new_enumerators)] =
                     context.storage.strings.intern("Zero");
                 context.storage.declarations.enumerators
                     .initializer[context.storage.declarations.enumerators.index(new_enumerators)] =
                     context.storage.strings.add("0");
                 return new_enumerators;
             }(),
             [&] {
                 auto const new_enumerators{context.storage.declarations.enumerators.append()};
                 context.storage.declarations.enumerators
                     .name[context.storage.declarations.enumerators.index(new_enumerators)] =
                     context.storage.strings.intern("High");
                 context.storage.declarations.enumerators
                     .initializer[context.storage.declarations.enumerators.index(new_enumerators)] =
                     context.storage.strings.add("7");
                 return new_enumerators;
             }(),
             [&] {
                 auto const new_enumerators{context.storage.declarations.enumerators.append()};
                 context.storage.declarations.enumerators
                     .name[context.storage.declarations.enumerators.index(new_enumerators)] =
                     context.storage.strings.intern("COUNT");
                 context.storage.declarations.enumerators
                     .initializer[context.storage.declarations.enumerators.index(new_enumerators)] =
                     context.storage.strings.add("6");
                 context.storage.declarations.enumerators
                     .display_name[context.storage.declarations.enumerators.index(
                         new_enumerators)] = std::nullopt;
                 context.storage.declarations.enumerators
                     .hidden[context.storage.declarations.enumerators.index(new_enumerators)] =
                     true;
                 return new_enumerators;
             }()});
    context.storage.declarations.enums.count[context.storage.declarations.enums.index(schema)] =
        context.storage.strings.intern("COUNT");
    EXPECT_NO_THROW(static_cast<void>(lower_known_enum(context, module, schema)));
}

TEST(PackedValue, EmitsCompactFallbackForOpaqueKnownEnumValues) {
    CompilerTestContext context;
    auto module{context.storage.declarations.normal_modules.clone(valid_module(context))};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[1])};
                    context.storage.declarations.packed_fields
                        .bits[context.storage.declarations.packed_fields.index(row_0)] = 3;
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();
    auto const header{lower_known_enum(context, module, [&] {
        auto const new_enums{context.storage.declarations.enums.append()};
        context.storage.declarations.enums
            .name[context.storage.declarations.enums.index(new_enums)] =
            context.storage.strings.intern("FighterStateKind");
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
            context.storage.declarations.range<EnumeratorId>(
                {[&] {
                     auto const new_enumerators{context.storage.declarations.enumerators.append()};
                     context.storage.declarations.enumerators
                         .name[context.storage.declarations.enumerators.index(new_enumerators)] =
                         context.storage.strings.intern("Zero");
                     context.storage.declarations.enumerators
                         .initializer[context.storage.declarations.enumerators.index(
                             new_enumerators)] =
                         context.storage.strings.add("static_cast<uint8>(0)");
                     return new_enumerators;
                 }(),
                 [&] {
                     auto const new_enumerators{context.storage.declarations.enumerators.append()};
                     context.storage.declarations.enumerators
                         .name[context.storage.declarations.enumerators.index(new_enumerators)] =
                         context.storage.strings.intern("High");
                     context.storage.declarations.enumerators
                         .initializer[context.storage.declarations.enumerators.index(
                             new_enumerators)] =
                         context.storage.strings.add("static_cast<uint8>(7)");
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
    auto manifest = [&](std::optional<std::uint32_t> const bit_width,
                        std::string maximum,
                        std::optional<bool> const signedness = std::nullopt) -> Manifest {
        auto module{context.storage.declarations.normal_modules.clone(valid_module(context))};
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
            auto const original_items{context.storage.declarations.items(
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(module)])};
            auto rows_3{std::vector(original_items.begin(), original_items.end())};
            rows_3.front() = [&] {
                auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
                context.storage.declarations.packed_values
                    .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                    auto const original_items{context.storage.declarations.items(
                        context.storage.declarations.packed_values
                            .segments[context.storage.declarations.packed_values.index(row_2)])};
                    auto rows_1{std::vector(original_items.begin(), original_items.end())};
                    rows_1[1] = [&] {
                        auto row_0{context.storage.declarations.packed_fields.clone(rows_1[1])};
                        context.storage.declarations.packed_fields
                            .bits[context.storage.declarations.packed_fields.index(row_0)] = 2;
                        return row_0;
                    }();
                    return context.storage.declarations.range(rows_1);
                }();
                return row_2;
            }();
            return context.storage.declarations.range(rows_3);
        }();
        auto const enums{context.storage.declarations.normal_modules.append()};
        context.storage.declarations.normal_modules
            .settings[context.storage.declarations.normal_modules.index(enums)] = [&] {
            auto const new_module_settings_store{
                context.storage.declarations.module_settings_store.append()};
            context.storage.declarations.module_settings_store
                .name[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.intern("enums");
            context.storage.declarations.module_settings_store
                .header[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.add("Enums.h");
            return new_module_settings_store;
        }();
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(enums)] =
            context.storage.declarations.range<DeclarationHandle>({[&] {
                auto const new_enums{context.storage.declarations.enums.append()};
                context.storage.declarations.enums
                    .name[context.storage.declarations.enums.index(new_enums)] =
                    context.storage.strings.intern("FighterStateKind");
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
                    context.storage.declarations.range<EnumeratorId>(
                        {[&] {
                             auto const new_enumerators{
                                 context.storage.declarations.enumerators.append()};
                             context.storage.declarations.enumerators
                                 .name[context.storage.declarations.enumerators.index(
                                     new_enumerators)] = context.storage.strings.intern("Idle");
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
                                     new_enumerators)] = context.storage.strings.intern("Maximum");
                             context.storage.declarations.enumerators
                                 .initializer[context.storage.declarations.enumerators.index(
                                     new_enumerators)] =
                                 context.storage.strings.add(std::move(maximum));
                             return new_enumerators;
                         }()});
                context.storage.declarations.enums
                    .bit_width[context.storage.declarations.enums.index(new_enums)] = bit_width;
                context.storage.declarations.enums
                    .signedness[context.storage.declarations.enums.index(new_enums)] =
                    signedness ? (*signedness ? schema::EnumSignedness::signed_
                                              : schema::EnumSignedness::unsigned_)
                               : schema::EnumSignedness::unspecified;
                return new_enums;
            }()});
        return {manifest_schema_version,
                make_test_registry(context),
                context.storage.declarations,
                {enums, module}};
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
    auto module{context.storage.declarations.normal_modules.clone(valid_module(context))};
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[1] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[1])};
                    context.storage.declarations.packed_fields
                        .bits[context.storage.declarations.packed_fields.index(row_0)]
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
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .relationship[context.storage.declarations.packed_fields.index(
                            row_0)] = [&] {
                        auto const new_semantic_relations{
                            context.storage.declarations.semantic_relations.append()};
                        context.storage.declarations.semantic_relations
                            .kind[context.storage.declarations.semantic_relations.index(
                                new_semantic_relations)] = SemanticRelationKind::index_into;
                        context.storage.declarations.semantic_relations
                            .target[context.storage.declarations.semantic_relations.index(
                                new_semantic_relations)] = [&] {
                            auto const new_type_refs{
                                context.storage.declarations.type_refs.append()};
                            context.storage.declarations.type_refs
                                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                                context.storage.strings.intern("FighterStateKind");
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
    auto const enums{context.storage.declarations.normal_modules.append()};
    context.storage.declarations.normal_modules
        .settings[context.storage.declarations.normal_modules.index(enums)] = [&] {
        auto const new_module_settings_store{
            context.storage.declarations.module_settings_store.append()};
        context.storage.declarations.module_settings_store
            .name[context.storage.declarations.module_settings_store.index(
                new_module_settings_store)] = context.storage.strings.intern("enums");
        context.storage.declarations.module_settings_store
            .header[context.storage.declarations.module_settings_store.index(
                new_module_settings_store)] = context.storage.strings.add("Enums.h");
        return new_module_settings_store;
    }();
    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(
            enums)] = context.storage.declarations.range<DeclarationHandle>({[&] {
        auto const new_enums{context.storage.declarations.enums.append()};
        context.storage.declarations.enums
            .name[context.storage.declarations.enums.index(new_enums)] =
            context.storage.strings.intern("FighterStateKind");
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
            context.storage.declarations.range<EnumeratorId>(
                {[&] {
                     auto const new_enumerators{context.storage.declarations.enumerators.append()};
                     context.storage.declarations.enumerators
                         .name[context.storage.declarations.enumerators.index(new_enumerators)] =
                         context.storage.strings.intern("Idle");
                     context.storage.declarations.enumerators
                         .initializer[context.storage.declarations.enumerators.index(
                             new_enumerators)] = context.storage.strings.add("0");
                     return new_enumerators;
                 }(),
                 [&] {
                     auto const new_enumerators{context.storage.declarations.enumerators.append()};
                     context.storage.declarations.enumerators
                         .name[context.storage.declarations.enumerators.index(new_enumerators)] =
                         context.storage.strings.intern("Maximum");
                     context.storage.declarations.enumerators
                         .initializer[context.storage.declarations.enumerators.index(
                             new_enumerators)] = context.storage.strings.add("7");
                     return new_enumerators;
                 }()});
        context.storage.declarations.enums
            .bit_width[context.storage.declarations.enums.index(new_enums)] = std::nullopt;
        context.storage.declarations.enums
            .signedness[context.storage.declarations.enums.index(new_enums)] =
            lispb::schema::EnumSignedness::unsigned_;
        return new_enums;
    }()});
    auto const files{render_test_manifest(context,
                                          Manifest{manifest_schema_version,
                                                   make_test_registry(context),
                                                   context.storage.declarations,
                                                   {enums, module}})};

    auto const generated{
        std::ranges::find(files, std::filesystem::path{"Packed.h"}, &GeneratedFile::path)};
    ASSERT_NE(generated, files.end());
    EXPECT_NE(generated->content.find("ioj::PackedField<storage_type, state_type, 24, 3>"),
              std::string::npos);
}

TEST(PackedValue, SentinelCodeCanIncreaseDerivedIntegerWidth) {
    CompilerTestContext context;
    auto module{context.storage.declarations.normal_modules.clone(valid_module(context))};

    context.storage.declarations.normal_modules
        .declarations[context.storage.declarations.normal_modules.index(module)] = [&] {
        auto const original_items{context.storage.declarations.items(
            context.storage.declarations.normal_modules
                .declarations[context.storage.declarations.normal_modules.index(module)])};
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .bits[context.storage.declarations.packed_fields.index(row_0)]
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
        auto rows_3{std::vector(original_items.begin(), original_items.end())};
        rows_3.front() = [&] {
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .minimum_value[context.storage.declarations.packed_fields.index(row_0)] = 0;
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
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .maximum_value[context.storage.declarations.packed_fields.index(row_0)] = 7;
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
            auto row_2{context.storage.declarations.packed_values.clone(rows_3.front())};
            context.storage.declarations.packed_values
                .segments[context.storage.declarations.packed_values.index(row_2)] = [&] {
                auto const original_items{context.storage.declarations.items(
                    context.storage.declarations.packed_values
                        .segments[context.storage.declarations.packed_values.index(row_2)])};
                auto rows_1{std::vector(original_items.begin(), original_items.end())};
                rows_1[0] = [&] {
                    auto row_0{context.storage.declarations.packed_fields.clone(rows_1[0])};
                    context.storage.declarations.packed_fields
                        .named_codes[context.storage.declarations.packed_fields.index(row_0)] =
                        context.storage.declarations.range<PackedNamedCodeId>({[&] {
                            auto const new_packed_named_codes{
                                context.storage.declarations.packed_named_codes.append()};
                            context.storage.declarations.packed_named_codes
                                .name[context.storage.declarations.packed_named_codes.index(
                                    new_packed_named_codes)] =
                                context.storage.strings.intern("Invalid");
                            context.storage.declarations.packed_named_codes
                                .value[context.storage.declarations.packed_named_codes.index(
                                    new_packed_named_codes)] = 8;
                            context.storage.declarations.packed_named_codes
                                .sentinel[context.storage.declarations.packed_named_codes.index(
                                    new_packed_named_codes)] = true;
                            return new_packed_named_codes;
                        }()});
                    return row_0;
                }();
                return context.storage.declarations.range(rows_1);
            }();
            return row_2;
        }();
        return context.storage.declarations.range(rows_3);
    }();

    auto const header{lower(context, module)};

    EXPECT_NE(header.find("ioj::PackedField<storage_type, entity_index_type, 0, 4>"),
              std::string::npos);
    EXPECT_NE(header.find("entity_index_Invalid{static_cast<entity_index_type>(8)}"),
              std::string::npos);
}

} // namespace
} // namespace lispb::codegen
