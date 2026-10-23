#include "compiler_test_context.h"
#include "test_utilities.h"

#include <codegen/ast/cpp_types.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/parameter_passing.h>
#include <codegen/ast/type_operation.h>
#include <codegen/generator.h>
#include <codegen/schema.h>
#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/enum_conversion.h>
#include <codegen/schema/enum_reflection.h>
#include <codegen/schema/enum_schemas.h>
#include <codegen/schema/enum_unreal_projections.h>
#include <codegen/schema/enumerator_schemas.h>
#include <codegen/schema/fixed_soa_schemas.h>
#include <codegen/schema/function_schemas.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/integer_scalar_cpp_emission.h>
#include <codegen/schema/integer_scalar_schemas.h>
#include <codegen/schema/manifest.h>
#include <codegen/schema/module_settings_store.h>
#include <codegen/schema/native_enum_helper.h>
#include <codegen/schema/normal_module_schemas.h>
#include <codegen/schema/packed_integer_value.h>
#include <codegen/schema/packed_named_code_schemas.h>
#include <codegen/schema/schema_version.h>
#include <codegen/schema/setting_apply_mode.h>
#include <codegen/schema/setting_category_schemas.h>
#include <codegen/schema/setting_control_kind.h>
#include <codegen/schema/setting_control_schemas.h>
#include <codegen/schema/setting_device.h>
#include <codegen/schema/setting_schemas.h>
#include <codegen/schema/settings_module_schemas.h>
#include <codegen/schema/soa_member_kind.h>
#include <codegen/schema/soa_member_schemas.h>
#include <codegen/schema/soa_schemas.h>
#include <codegen/schema/storage_operation.h>
#include <codegen/schema/type_refs.h>
#include <lispb/compiler_storage.h>
#include <lispb/indices.h>
#include <lispb/strings.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lispb::codegen {
namespace {

struct RenderedModule {
    std::string header;
    std::string source;
};

auto render_soa(CompilerTestContext& context,
                DeclarationHandle schema,
                std::vector<std::pair<std::string_view, CppTypeId>> const& registrations = {})
    -> RenderedModule {
    auto const files{render_test_manifest(
        context,
        Manifest{
            manifest_schema_version,
            make_test_registry(context, registrations),
            context.storage.declarations,
            {[&] {
                auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
                context.storage.declarations.normal_modules
                    .settings[context.storage.declarations.normal_modules.index(
                        new_normal_modules)] = [&] {
                    auto const new_module_settings_store{
                        context.storage.declarations.module_settings_store.append()};
                    context.storage.declarations.module_settings_store
                        .name[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] = context.storage.strings.intern("test");
                    context.storage.declarations.module_settings_store
                        .header[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] =
                        context.storage.strings.intern("Generated.h");
                    context.storage.declarations.module_settings_store
                        .source[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] =
                        context.storage.strings.intern("Generated.cpp");
                    context.storage.declarations.module_settings_store
                        .header_include[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] =
                        context.storage.strings.intern("Project/Generated.h");
                    return new_module_settings_store;
                }();
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(
                        new_normal_modules)] =
                    context.storage.declarations.range<DeclarationHandle>({schema});
                return new_normal_modules;
            }()}})};
    EXPECT_EQ(files.size(), 2);
    return RenderedModule{files[0].content, files[1].content};
}

auto render_enum(CompilerTestContext& context, ModuleHandle module) -> RenderedModule {
    auto const files{render_test_manifest(context,
                                          Manifest{manifest_schema_version,
                                                   make_test_registry(context),
                                                   context.storage.declarations,
                                                   {module}})};
    EXPECT_EQ(files.size(), 2);
    return RenderedModule{files[0].content, files[1].content};
}

auto render_settings(CompilerTestContext& context, ModuleHandle module) -> RenderedModule {
    auto const files{render_test_manifest(context,
                                          Manifest{manifest_schema_version,
                                                   make_test_registry(context),
                                                   context.storage.declarations,
                                                   {module}})};
    EXPECT_EQ(files.size(), 2);
    return RenderedModule{files[0].content, files[1].content};
}

auto basic_schema(CompilerTestContext& context) -> DeclarationHandle {
    return [&] {
        auto const new_soas{context.storage.declarations.soas.append()};
        context.storage.declarations.soas.name[context.storage.declarations.soas.index(new_soas)] =
            context.storage.strings.intern("FData");
        context.storage.declarations.soas
            .members[context.storage.declarations.soas.index(new_soas)] =
            context.storage.declarations.range<SoaMemberId>(
                {[&] {
                     auto const new_soa_members{context.storage.declarations.soa_members.append()};
                     context.storage.declarations.soa_members
                         .name[context.storage.declarations.soa_members.index(new_soa_members)] =
                         context.storage.strings.intern("ids");
                     context.storage.declarations.soa_members
                         .kind[context.storage.declarations.soa_members.index(new_soa_members)] =
                         SoaMemberKind::array;
                     context.storage.declarations.soa_members
                         .type[context.storage.declarations.soa_members.index(
                             new_soa_members)] = [&] {
                         auto const new_type_refs{context.storage.declarations.type_refs.append()};
                         context.storage.declarations.type_refs
                             .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                             context.storage.strings.intern("int32");
                         return new_type_refs;
                     }();
                     return new_soa_members;
                 }(),
                 [&] {
                     auto const new_soa_members{context.storage.declarations.soa_members.append()};
                     context.storage.declarations.soa_members
                         .name[context.storage.declarations.soa_members.index(new_soa_members)] =
                         context.storage.strings.intern("weights");
                     context.storage.declarations.soa_members
                         .kind[context.storage.declarations.soa_members.index(new_soa_members)] =
                         SoaMemberKind::array;
                     context.storage.declarations.soa_members
                         .type[context.storage.declarations.soa_members.index(
                             new_soa_members)] = [&] {
                         auto const new_type_refs{context.storage.declarations.type_refs.append()};
                         context.storage.declarations.type_refs
                             .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                             context.storage.strings.intern("float");
                         return new_type_refs;
                     }();
                     return new_soa_members;
                 }()});
        return new_soas;
    }();
}

TEST(Lowering, EmitsOnlyRequestedStorageOperations) {
    CompilerTestContext context;
    auto schema{basic_schema(context)};
    context.storage.declarations.soas.operations[context.storage.declarations.soas.index(schema)] =
        context.storage.declarations.range<StorageOperation>(all_storage_operations());

    auto const output{render_soa(context, schema)};

    EXPECT_NE(output.source.find("void FData::reset()"), std::string::npos);
    EXPECT_NE(output.source.find("void FData::reserve(int32 const count)"), std::string::npos);
    EXPECT_NE(output.source.find("void FData::add_uninitialised(int32 const count)"),
              std::string::npos);
    EXPECT_NE(output.source.find("void FData::add_defaulted(int32 const count)"),
              std::string::npos);
    EXPECT_NE(output.source.find("void FData::set_num("), std::string::npos);
    EXPECT_NE(output.header.find("void remove_at_swap("), std::string::npos);
    EXPECT_NE(output.header.find("void copy_element("), std::string::npos);
    EXPECT_NE(output.header.find("void copy_elements("), std::string::npos);
    EXPECT_NE(output.header.find("void copy_to_tail("), std::string::npos);
    EXPECT_NE(output.header.find("void append_from("), std::string::npos);

    auto no_operations{basic_schema(context)};
    auto const minimal{render_soa(context, no_operations)};
    EXPECT_EQ(minimal.header.find("void remove_at_swap("), std::string::npos);
    EXPECT_EQ(minimal.header.find("void copy_element("), std::string::npos);
    EXPECT_EQ(minimal.header.find("void append_from("), std::string::npos);
    EXPECT_EQ(minimal.source.find("void FData::reset()"), std::string::npos);
}

TEST(Lowering, EmitsStandaloneNativeEnumApi) {
    CompilerTestContext context;
    auto const files{render_test_manifest(
        context,
        Manifest{
            manifest_schema_version,
            make_test_registry(
                context,
                {{"native_uint8",
                  context.storage.cpp_types.add(
                      "std::uint8_t",
                      {context.storage.cpp_types.dependency("std::uint8_t", "cstdint")})}}),
            context.storage.declarations,
            {[&] {
                auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
                context.storage.declarations.normal_modules
                    .settings[context.storage.declarations.normal_modules.index(
                        new_normal_modules)] = [&] {
                    auto const new_module_settings_store{
                        context.storage.declarations.module_settings_store.append()};
                    context.storage.declarations.module_settings_store
                        .name[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] =
                        context.storage.strings.intern("native_enum");
                    context.storage.declarations.module_settings_store
                        .header[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] =
                        context.storage.strings.intern("NativeEnum.h");
                    context.storage.declarations.module_settings_store
                        .namespace_name[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] = context.storage.strings.intern("fixture");
                    return new_module_settings_store;
                }();
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(
                        new_normal_modules)] =
                    context.storage.declarations.range<DeclarationHandle>({[&] {
                        auto const new_enums{context.storage.declarations.enums.append()};
                        context.storage.declarations.enums
                            .name[context.storage.declarations.enums.index(new_enums)] =
                            context.storage.strings.intern("NativeState");
                        context.storage.declarations.enums
                            .underlying_type[context.storage.declarations.enums.index(
                                new_enums)] = [&] {
                            auto const new_type_refs{
                                context.storage.declarations.type_refs.append()};
                            context.storage.declarations.type_refs
                                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                                context.storage.strings.intern("@native_uint8");
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
                                             new_enumerators)] =
                                         context.storage.strings.intern("Idle");
                                     context.storage.declarations.enumerators
                                         .initializer[context.storage.declarations.enumerators
                                                          .index(new_enumerators)] =
                                         context.storage.strings.intern("0");
                                     context.storage.declarations.enumerators
                                         .display_name[context.storage.declarations.enumerators
                                                           .index(new_enumerators)] =
                                         context.storage.strings.intern("Idle");
                                     context.storage.declarations.enumerators
                                         .serialized_name[context.storage.declarations.enumerators
                                                              .index(new_enumerators)] =
                                         context.storage.strings.intern("idle");
                                     return new_enumerators;
                                 }(),
                                 [&] {
                                     auto const new_enumerators{
                                         context.storage.declarations.enumerators.append()};
                                     context.storage.declarations.enumerators
                                         .name[context.storage.declarations.enumerators.index(
                                             new_enumerators)] =
                                         context.storage.strings.intern("Active");
                                     context.storage.declarations.enumerators
                                         .initializer[context.storage.declarations.enumerators
                                                          .index(new_enumerators)] =
                                         context.storage.strings.intern("1");
                                     context.storage.declarations.enumerators
                                         .display_name[context.storage.declarations.enumerators
                                                           .index(new_enumerators)] =
                                         context.storage.strings.intern("Active");
                                     context.storage.declarations.enumerators
                                         .serialized_name[context.storage.declarations.enumerators
                                                              .index(new_enumerators)] =
                                         context.storage.strings.intern("active");
                                     return new_enumerators;
                                 }()});
                        context.storage.declarations.enums
                            .native_api[context.storage.declarations.enums.index(new_enums)] = true;
                        context.storage.declarations.enums
                            .helpers[context.storage.declarations.enums.index(new_enums)] =
                            context.storage.declarations.range<NativeEnumHelper>(
                                {NativeEnumHelper::to_string,
                                 NativeEnumHelper::try_parse,
                                 NativeEnumHelper::to_display_string,
                                 NativeEnumHelper::to_serialized_string,
                                 NativeEnumHelper::try_parse_serialized,
                                 NativeEnumHelper::traits});
                        return new_enums;
                    }()});
                return new_normal_modules;
            }()}})};

    ASSERT_EQ(files.size(), 1);
    auto const& header{files.front().content};
    EXPECT_NE(header.find("using NativeStateUnderlying = std::uint8_t;"), std::string::npos);
    EXPECT_NE(header.find("enum class NativeState : NativeStateUnderlying"), std::string::npos);
    EXPECT_NE(header.find("try_parse_native_state"), std::string::npos);
    EXPECT_NE(header.find("try_parse_serialized_native_state"), std::string::npos);
    EXPECT_NE(header.find("EnumTraits<::fixture::NativeState>"), std::string::npos);
    EXPECT_EQ(header.find("CoreMinimal.h"), std::string::npos);

    std::vector<std::pair<NativeEnumHelper, std::string>> const selections{
        {NativeEnumHelper::to_string, "auto to_string("},
        {NativeEnumHelper::try_parse, "auto try_parse_state("},
        {NativeEnumHelper::to_display_string, "auto to_display_string("},
        {NativeEnumHelper::to_serialized_string, "auto to_serialized_string("},
        {NativeEnumHelper::try_parse_serialized, "auto try_parse_serialized_state("},
        {NativeEnumHelper::traits, "struct EnumTraits<"},
    };
    for (auto const& [helper, spelling] : selections) {
        auto const selected{
            render_test_manifest(
                context,
                Manifest{
                    manifest_schema_version,
                    make_test_registry(context),
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
                                context.storage.strings.intern("selected");
                            context.storage.declarations.module_settings_store
                                .header[context.storage.declarations.module_settings_store.index(
                                    new_module_settings_store)] =
                                context.storage.strings.intern("Selected.h");
                            return new_module_settings_store;
                        }();
                        context.storage.declarations.normal_modules
                            .declarations[context.storage.declarations.normal_modules.index(
                                new_normal_modules)] =
                            context.storage.declarations.range<DeclarationHandle>({[&] {
                                auto const new_enums{context.storage.declarations.enums.append()};
                                context.storage.declarations.enums
                                    .name[context.storage.declarations.enums.index(new_enums)] =
                                    context.storage.strings.intern("State");
                                context.storage.declarations.enums
                                    .values[context.storage.declarations.enums.index(new_enums)] =
                                    context.storage.declarations.range<EnumeratorId>({[&] {
                                        auto const new_enumerators{
                                            context.storage.declarations.enumerators.append()};
                                        context.storage.declarations.enumerators
                                            .name[context.storage.declarations.enumerators.index(
                                                new_enumerators)] =
                                            context.storage.strings.intern("Idle");
                                        context.storage.declarations.enumerators
                                            .initializer[context.storage.declarations.enumerators
                                                             .index(new_enumerators)] =
                                            context.storage.strings.intern("0");
                                        context.storage.declarations.enumerators.serialized_name
                                            [context.storage.declarations.enumerators.index(
                                                new_enumerators)] =
                                            context.storage.strings.intern("idle");
                                        return new_enumerators;
                                    }()});
                                context.storage.declarations.enums
                                    .enum_array[context.storage.declarations.enums.index(
                                        new_enums)] = true;
                                context.storage.declarations.enums
                                    .native_api[context.storage.declarations.enums.index(
                                        new_enums)] = true;
                                context.storage.declarations.enums
                                    .helpers[context.storage.declarations.enums.index(new_enums)] =
                                    context.storage.declarations.range<NativeEnumHelper>({helper});
                                return new_enums;
                            }()});
                        return new_normal_modules;
                    }()}})
                .front()
                .content};
        for (auto const& [candidate, candidate_spelling] : selections) {
            EXPECT_EQ(selected.contains(candidate_spelling), candidate == helper)
                << spelling << ": " << candidate_spelling;
        }
        EXPECT_NE(selected.find("enum_count<"), std::string::npos);
        EXPECT_EQ(selected.find("std::array names"), std::string::npos);
        EXPECT_EQ(selected.contains("<string_view>"), helper != NativeEnumHelper::traits);
        EXPECT_EQ(selected.find("<unordered_map>"), std::string::npos);
    }
}

TEST(Lowering, DerivesNativeEnumBackingFromSemanticWidthAndSignedness) {
    CompilerTestContext context;
    auto const files{render_test_manifest(
        context,
        Manifest{
            manifest_schema_version,
            make_test_registry(context),
            context.storage.declarations,
            {[&] {
                auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
                context.storage.declarations.normal_modules
                    .settings[context.storage.declarations.normal_modules.index(
                        new_normal_modules)] = [&] {
                    auto const new_module_settings_store{
                        context.storage.declarations.module_settings_store.append()};
                    context.storage.declarations.module_settings_store
                        .name[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] =
                        context.storage.strings.intern("native_enum");
                    context.storage.declarations.module_settings_store
                        .header[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] =
                        context.storage.strings.intern("NativeEnum.h");
                    context.storage.declarations.module_settings_store
                        .namespace_name[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] = context.storage.strings.intern("fixture");
                    return new_module_settings_store;
                }();
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(
                        new_normal_modules)] =
                    context.storage.declarations.range<DeclarationHandle>({[&] {
                        auto const new_enums{context.storage.declarations.enums.append()};
                        context.storage.declarations.enums
                            .name[context.storage.declarations.enums.index(new_enums)] =
                            context.storage.strings.intern("SignedState");
                        context.storage.declarations.enums
                            .underlying_type[context.storage.declarations.enums.index(new_enums)] =
                            std::nullopt;
                        context.storage.declarations.enums
                            .values[context.storage.declarations.enums.index(new_enums)] =
                            context.storage.declarations.range<EnumeratorId>(
                                {[&] {
                                     auto const new_enumerators{
                                         context.storage.declarations.enumerators.append()};
                                     context.storage.declarations.enumerators
                                         .name[context.storage.declarations.enumerators.index(
                                             new_enumerators)] =
                                         context.storage.strings.intern("Below");
                                     context.storage.declarations.enumerators
                                         .initializer[context.storage.declarations.enumerators
                                                          .index(new_enumerators)] =
                                         context.storage.strings.intern("-1");
                                     return new_enumerators;
                                 }(),
                                 [&] {
                                     auto const new_enumerators{
                                         context.storage.declarations.enumerators.append()};
                                     context.storage.declarations.enumerators
                                         .name[context.storage.declarations.enumerators.index(
                                             new_enumerators)] =
                                         context.storage.strings.intern("Above");
                                     context.storage.declarations.enumerators
                                         .initializer[context.storage.declarations.enumerators
                                                          .index(new_enumerators)] =
                                         context.storage.strings.intern("1");
                                     return new_enumerators;
                                 }()});
                        context.storage.declarations.enums
                            .bit_width[context.storage.declarations.enums.index(new_enums)] = 12;
                        context.storage.declarations.enums
                            .signedness[context.storage.declarations.enums.index(new_enums)] =
                            lispb::schema::EnumSignedness::signed_;
                        context.storage.declarations.enums
                            .native_api[context.storage.declarations.enums.index(new_enums)] = true;
                        return new_enums;
                    }()});
                return new_normal_modules;
            }()}})};

    ASSERT_EQ(files.size(), 1);
    auto const& header{files.front().content};
    EXPECT_NE(header.find("using SignedStateUnderlying = std::int16_t;"), std::string::npos);
    EXPECT_NE(header.find("enum class SignedState : SignedStateUnderlying"), std::string::npos);
    EXPECT_EQ(header.find("to_string("), std::string::npos);
    EXPECT_EQ(header.find("try_parse_"), std::string::npos);
    EXPECT_EQ(header.find("EnumTraits<"), std::string::npos);
    EXPECT_EQ(header.find("<optional>"), std::string::npos);
    EXPECT_EQ(header.find("<string_view>"), std::string::npos);
    EXPECT_NE(header.find("#include <cstdint>"), std::string::npos);
}

TEST(Lowering, DerivesUnrealStyleEnumBackingWithoutChangingSemanticWidth) {
    CompilerTestContext context;
    auto const output{render_enum(context, [&] {
        auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
        context.storage.declarations.normal_modules
            .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] = [&] {
            auto const new_module_settings_store{
                context.storage.declarations.module_settings_store.append()};
            context.storage.declarations.module_settings_store
                .name[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.intern("states");
            context.storage.declarations.module_settings_store
                .header[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.intern("States.h");
            context.storage.declarations.module_settings_store
                .source[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.intern("States.cpp");
            return new_module_settings_store;
        }();
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(new_normal_modules)] =
            context.storage.declarations.range<DeclarationHandle>({[&] {
                auto const new_enums{context.storage.declarations.enums.append()};
                context.storage.declarations.enums
                    .name[context.storage.declarations.enums.index(new_enums)] =
                    context.storage.strings.intern("EState");
                context.storage.declarations.enums
                    .underlying_type[context.storage.declarations.enums.index(new_enums)] =
                    std::nullopt;
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
                                     new_enumerators)] = context.storage.strings.intern("0");
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
                                     new_enumerators)] = context.storage.strings.intern("4095");
                             return new_enumerators;
                         }()});
                context.storage.declarations.enums
                    .bit_width[context.storage.declarations.enums.index(new_enums)] = 12;
                context.storage.declarations.enums
                    .signedness[context.storage.declarations.enums.index(new_enums)] =
                    lispb::schema::EnumSignedness::unsigned_;
                return new_enums;
            }()});
        return new_normal_modules;
    }())};

    EXPECT_NE(output.header.find("#include \"CoreMinimal.h\""), std::string::npos);
    EXPECT_NE(output.header.find("using EStateUnderlying = uint16;"), std::string::npos);
    EXPECT_NE(output.header.find("enum class EState : EStateUnderlying"), std::string::npos);
}

TEST(Lowering, EmitsRequestedNamedScalarConstantsWithoutInventingScalarTypes) {
    CompilerTestContext context;
    auto const files{render_test_manifest(
        context,
        Manifest{
            manifest_schema_version,
            make_test_registry(
                context,
                {{"native_uint8",
                  context.storage.cpp_types.add(
                      "std::uint8_t",
                      {context.storage.cpp_types.dependency("std::uint8_t", "cstdint")})},
                 {"native_int64",
                  context.storage.cpp_types.add(
                      "std::int64_t",
                      {context.storage.cpp_types.dependency("std::int64_t", "cstdint")})},
                 {"native_uint64",
                  context.storage.cpp_types.add(
                      "std::uint64_t",
                      {context.storage.cpp_types.dependency("std::uint64_t", "cstdint")})}}),
            context.storage.declarations,
            {[&] {
                auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
                context.storage.declarations.normal_modules
                    .settings[context.storage.declarations.normal_modules.index(
                        new_normal_modules)] = [&] {
                    auto const new_module_settings_store{
                        context.storage.declarations.module_settings_store.append()};
                    context.storage.declarations.module_settings_store
                        .name[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] =
                        context.storage.strings.intern("semantic_values");
                    context.storage.declarations.module_settings_store
                        .header[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] =
                        context.storage.strings.intern("SemanticValues.h");
                    context.storage.declarations.module_settings_store
                        .namespace_name[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] = context.storage.strings.intern("fixture");
                    return new_module_settings_store;
                }();
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(
                        new_normal_modules)] =
                    context.storage.declarations.range<DeclarationHandle>(
                        {[&] {
                             auto const new_integer_scalars{
                                 context.storage.declarations.integer_scalars.append()};
                             context.storage.declarations.integer_scalars
                                 .name[context.storage.declarations.integer_scalars.index(
                                     new_integer_scalars)] =
                                 context.storage.strings.intern("NoOutput");
                             context.storage.declarations.integer_scalars
                                 .signedness[context.storage.declarations.integer_scalars.index(
                                     new_integer_scalars)] = false;
                             context.storage.declarations.integer_scalars
                                 .minimum_value[context.storage.declarations.integer_scalars.index(
                                     new_integer_scalars)] = 0;
                             context.storage.declarations.integer_scalars
                                 .maximum_value[context.storage.declarations.integer_scalars.index(
                                     new_integer_scalars)] = 1;
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
                                         .name[context.storage.declarations.packed_named_codes
                                                   .index(new_packed_named_codes)] =
                                         context.storage.strings.intern("Zero");
                                     context.storage.declarations.packed_named_codes
                                         .value[context.storage.declarations.packed_named_codes
                                                    .index(new_packed_named_codes)] = 0;
                                     context.storage.declarations.packed_named_codes
                                         .sentinel[context.storage.declarations.packed_named_codes
                                                       .index(new_packed_named_codes)] = false;
                                     return new_packed_named_codes;
                                 }()});
                             return new_integer_scalars;
                         }(),
                         [&] {
                             auto const new_integer_scalars{
                                 context.storage.declarations.integer_scalars.append()};
                             context.storage.declarations.integer_scalars
                                 .name[context.storage.declarations.integer_scalars.index(
                                     new_integer_scalars)] =
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
                                 context.storage.declarations.range<PackedNamedCodeId>(
                                     {[&] {
                                          auto const new_packed_named_codes{
                                              context.storage.declarations.packed_named_codes
                                                  .append()};
                                          context.storage.declarations.packed_named_codes
                                              .name[context.storage.declarations.packed_named_codes
                                                        .index(new_packed_named_codes)] =
                                              context.storage.strings.intern("Unknown");
                                          context.storage.declarations.packed_named_codes
                                              .value[context.storage.declarations.packed_named_codes
                                                         .index(new_packed_named_codes)] = 0;
                                          context.storage.declarations.packed_named_codes
                                              .sentinel[context.storage.declarations
                                                            .packed_named_codes.index(
                                                                new_packed_named_codes)] = false;
                                          return new_packed_named_codes;
                                      }(),
                                      [&] {
                                          auto const new_packed_named_codes{
                                              context.storage.declarations.packed_named_codes
                                                  .append()};
                                          context.storage.declarations.packed_named_codes
                                              .name[context.storage.declarations.packed_named_codes
                                                        .index(new_packed_named_codes)] =
                                              context.storage.strings.intern("Invalid");
                                          context.storage.declarations.packed_named_codes
                                              .value[context.storage.declarations.packed_named_codes
                                                         .index(new_packed_named_codes)] = 255;
                                          context.storage.declarations.packed_named_codes
                                              .sentinel[context.storage.declarations
                                                            .packed_named_codes.index(
                                                                new_packed_named_codes)] = true;
                                          return new_packed_named_codes;
                                      }()});
                             context.storage.declarations.integer_scalars
                                 .cpp_emission[context.storage.declarations.integer_scalars.index(
                                     new_integer_scalars)] =
                                 IntegerScalarCppEmission::constants_with_names;
                             context.storage.declarations.integer_scalars
                                 .cpp_type[context.storage.declarations.integer_scalars.index(
                                     new_integer_scalars)] = [&] {
                                 auto const new_type_refs{
                                     context.storage.declarations.type_refs.append()};
                                 context.storage.declarations.type_refs
                                     .name[context.storage.declarations.type_refs.index(
                                         new_type_refs)] =
                                     context.storage.strings.intern("@native_uint8");
                                 return new_type_refs;
                             }();
                             return new_integer_scalars;
                         }(),
                         [&] {
                             auto const new_integer_scalars{
                                 context.storage.declarations.integer_scalars.append()};
                             context.storage.declarations.integer_scalars
                                 .name[context.storage.declarations.integer_scalars.index(
                                     new_integer_scalars)] =
                                 context.storage.strings.intern("SignedLimit");
                             context.storage.declarations.integer_scalars
                                 .signedness[context.storage.declarations.integer_scalars.index(
                                     new_integer_scalars)] = true;
                             context.storage.declarations.integer_scalars
                                 .minimum_value[context.storage.declarations.integer_scalars.index(
                                     new_integer_scalars)] =
                                 PackedIntegerValue::from_parts(true, std::uint64_t{1} << 63);
                             context.storage.declarations.integer_scalars
                                 .maximum_value[context.storage.declarations.integer_scalars.index(
                                     new_integer_scalars)] = 0;
                             context.storage.declarations.integer_scalars
                                 .bit_width[context.storage.declarations.integer_scalars.index(
                                     new_integer_scalars)] = 64;
                             context.storage.declarations.integer_scalars
                                 .named_codes[context.storage.declarations.integer_scalars.index(
                                     new_integer_scalars)] =
                                 context.storage.declarations.range<PackedNamedCodeId>({[&] {
                                     auto const new_packed_named_codes{
                                         context.storage.declarations.packed_named_codes.append()};
                                     context.storage.declarations.packed_named_codes
                                         .name[context.storage.declarations.packed_named_codes
                                                   .index(new_packed_named_codes)] =
                                         context.storage.strings.intern("Minimum");
                                     context.storage.declarations.packed_named_codes
                                         .value[context.storage.declarations.packed_named_codes
                                                    .index(new_packed_named_codes)] =
                                         PackedIntegerValue::from_parts(true,
                                                                        std::uint64_t{1} << 63);
                                     context.storage.declarations.packed_named_codes
                                         .sentinel[context.storage.declarations.packed_named_codes
                                                       .index(new_packed_named_codes)] = false;
                                     return new_packed_named_codes;
                                 }()});
                             context.storage.declarations.integer_scalars
                                 .cpp_emission[context.storage.declarations.integer_scalars.index(
                                     new_integer_scalars)] = IntegerScalarCppEmission::constants;
                             context.storage.declarations.integer_scalars
                                 .cpp_type[context.storage.declarations.integer_scalars.index(
                                     new_integer_scalars)] = [&] {
                                 auto const new_type_refs{
                                     context.storage.declarations.type_refs.append()};
                                 context.storage.declarations.type_refs
                                     .name[context.storage.declarations.type_refs.index(
                                         new_type_refs)] =
                                     context.storage.strings.intern("@native_int64");
                                 return new_type_refs;
                             }();
                             return new_integer_scalars;
                         }(),
                         [&] {
                             auto const new_integer_scalars{
                                 context.storage.declarations.integer_scalars.append()};
                             context.storage.declarations.integer_scalars
                                 .name[context.storage.declarations.integer_scalars.index(
                                     new_integer_scalars)] =
                                 context.storage.strings.intern("UnsignedLimit");
                             context.storage.declarations.integer_scalars
                                 .signedness[context.storage.declarations.integer_scalars.index(
                                     new_integer_scalars)] = false;
                             context.storage.declarations.integer_scalars
                                 .minimum_value[context.storage.declarations.integer_scalars.index(
                                     new_integer_scalars)] = 0;
                             context.storage.declarations.integer_scalars
                                 .maximum_value[context.storage.declarations.integer_scalars.index(
                                     new_integer_scalars)] =
                                 (std::numeric_limits<std::uint64_t>::max)();
                             context.storage.declarations.integer_scalars
                                 .bit_width[context.storage.declarations.integer_scalars.index(
                                     new_integer_scalars)] = 64;
                             context.storage.declarations.integer_scalars
                                 .named_codes[context.storage.declarations.integer_scalars.index(
                                     new_integer_scalars)] =
                                 context.storage.declarations.range<PackedNamedCodeId>({[&] {
                                     auto const new_packed_named_codes{
                                         context.storage.declarations.packed_named_codes.append()};
                                     context.storage.declarations.packed_named_codes
                                         .name[context.storage.declarations.packed_named_codes
                                                   .index(new_packed_named_codes)] =
                                         context.storage.strings.intern("Maximum");
                                     context.storage.declarations.packed_named_codes
                                         .value[context.storage.declarations.packed_named_codes
                                                    .index(new_packed_named_codes)] =
                                         (std::numeric_limits<std::uint64_t>::max)();
                                     context.storage.declarations.packed_named_codes
                                         .sentinel[context.storage.declarations.packed_named_codes
                                                       .index(new_packed_named_codes)] = false;
                                     return new_packed_named_codes;
                                 }()});
                             context.storage.declarations.integer_scalars
                                 .cpp_emission[context.storage.declarations.integer_scalars.index(
                                     new_integer_scalars)] = IntegerScalarCppEmission::constants;
                             context.storage.declarations.integer_scalars
                                 .cpp_type[context.storage.declarations.integer_scalars.index(
                                     new_integer_scalars)] = [&] {
                                 auto const new_type_refs{
                                     context.storage.declarations.type_refs.append()};
                                 context.storage.declarations.type_refs
                                     .name[context.storage.declarations.type_refs.index(
                                         new_type_refs)] =
                                     context.storage.strings.intern("@native_uint64");
                                 return new_type_refs;
                             }();
                             return new_integer_scalars;
                         }()});
                return new_normal_modules;
            }()}})};

    ASSERT_EQ(files.size(), 1U);
    auto const& header{files.front().content};
    EXPECT_NE(header.find("#include <cstdint>"), std::string::npos) << header;
    EXPECT_NE(header.find("namespace fixture"), std::string::npos);
    EXPECT_NE(header.find("inline constexpr std::uint8_t DamageReason_Unknown"), std::string::npos);
    EXPECT_NE(header.find("inline constexpr std::uint8_t DamageReason_Invalid"), std::string::npos);
    EXPECT_NE(header.find("#include <string_view>"), std::string::npos);
    EXPECT_NE(header.find("constexpr auto DamageReason_name(std::uint8_t const value)"),
              std::string::npos);
    EXPECT_NE(header.find("case DamageReason_Unknown"), std::string::npos);
    EXPECT_NE(header.find("return \"Unknown\""), std::string::npos);
    EXPECT_NE(header.find("return {};"), std::string::npos);
    EXPECT_NE(header.find("static_cast<std::int64_t>((-9223372036854775807LL - 1))"),
              std::string::npos);
    EXPECT_NE(header.find("static_cast<std::uint64_t>(18446744073709551615ULL)"),
              std::string::npos);
    EXPECT_EQ(header.find("NoOutput_Zero"), std::string::npos);
    EXPECT_EQ(header.find("using DamageReason"), std::string::npos);
}

TEST(Lowering, EmitsNativeEnumUnrealProjectionWithExplicitNumericCompatibility) {
    CompilerTestContext context;
    auto const files{render_test_manifest(
        context,
        Manifest{
            manifest_schema_version,
            make_test_registry(
                context,
                {{"native_uint8",
                  context.storage.cpp_types.add(
                      "std::uint8_t",
                      {context.storage.cpp_types.dependency("std::uint8_t", "cstdint")})}}),
            context.storage.declarations,
            {[&] {
                auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
                context.storage.declarations.normal_modules
                    .settings[context.storage.declarations.normal_modules.index(
                        new_normal_modules)] = [&] {
                    auto const new_module_settings_store{
                        context.storage.declarations.module_settings_store.append()};
                    context.storage.declarations.module_settings_store
                        .name[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] =
                        context.storage.strings.intern("native_enum");
                    context.storage.declarations.module_settings_store
                        .header[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] =
                        context.storage.strings.intern("NativeEnum.h");
                    context.storage.declarations.module_settings_store
                        .namespace_name[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] = context.storage.strings.intern("fixture");
                    return new_module_settings_store;
                }();
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(
                        new_normal_modules)] =
                    context.storage.declarations.range<DeclarationHandle>({[&] {
                        auto const new_enums{context.storage.declarations.enums.append()};
                        context.storage.declarations.enums
                            .name[context.storage.declarations.enums.index(new_enums)] =
                            context.storage.strings.intern("NativeState");
                        context.storage.declarations.enums
                            .underlying_type[context.storage.declarations.enums.index(
                                new_enums)] = [&] {
                            auto const new_type_refs{
                                context.storage.declarations.type_refs.append()};
                            context.storage.declarations.type_refs
                                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                                context.storage.strings.intern("@native_uint8");
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
                                             new_enumerators)] =
                                         context.storage.strings.intern("Idle");
                                     context.storage.declarations.enumerators
                                         .initializer[context.storage.declarations.enumerators
                                                          .index(new_enumerators)] =
                                         context.storage.strings.intern("0");
                                     context.storage.declarations.enumerators
                                         .serialized_name[context.storage.declarations.enumerators
                                                              .index(new_enumerators)] =
                                         context.storage.strings.intern("idle");
                                     return new_enumerators;
                                 }(),
                                 [&] {
                                     auto const new_enumerators{
                                         context.storage.declarations.enumerators.append()};
                                     context.storage.declarations.enumerators
                                         .name[context.storage.declarations.enumerators.index(
                                             new_enumerators)] =
                                         context.storage.strings.intern("Active");
                                     context.storage.declarations.enumerators
                                         .initializer[context.storage.declarations.enumerators
                                                          .index(new_enumerators)] =
                                         context.storage.strings.intern("1");
                                     context.storage.declarations.enumerators
                                         .serialized_name[context.storage.declarations.enumerators
                                                              .index(new_enumerators)] =
                                         context.storage.strings.intern("active");
                                     return new_enumerators;
                                 }()});
                        context.storage.declarations.enums
                            .unreal_projection[context.storage.declarations.enums.index(
                                new_enums)] = [&] {
                            auto const new_enum_unreal_projections{
                                context.storage.declarations.enum_unreal_projections.append()};
                            context.storage.declarations.enum_unreal_projections
                                .name[context.storage.declarations.enum_unreal_projections.index(
                                    new_enum_unreal_projections)] =
                                context.storage.strings.intern("ENativeState");
                            context.storage.declarations.enum_unreal_projections
                                .header[context.storage.declarations.enum_unreal_projections.index(
                                    new_enum_unreal_projections)] =
                                context.storage.strings.intern("Project/NativeState.h");
                            context.storage.declarations.enum_unreal_projections
                                .header_include[context.storage.declarations.enum_unreal_projections
                                                    .index(new_enum_unreal_projections)] =
                                context.storage.strings.intern("Project/NativeState.h");
                            context.storage.declarations.enum_unreal_projections.conversion_header
                                [context.storage.declarations.enum_unreal_projections.index(
                                    new_enum_unreal_projections)] =
                                context.storage.strings.intern("Project/NativeStateConversion.h");
                            context.storage.declarations.enum_unreal_projections
                                .native_header_include[context.storage.declarations
                                                           .enum_unreal_projections.index(
                                                               new_enum_unreal_projections)] =
                                context.storage.strings.intern("fixture/NativeEnum.h");
                            context.storage.declarations.enum_unreal_projections
                                .enum_array[context.storage.declarations.enum_unreal_projections
                                                .index(new_enum_unreal_projections)] = true;
                            return new_enum_unreal_projections;
                        }();
                        context.storage.declarations.enums
                            .enum_array[context.storage.declarations.enums.index(new_enums)] =
                            false;
                        context.storage.declarations.enums
                            .native_api[context.storage.declarations.enums.index(new_enums)] = true;
                        context.storage.declarations.enums
                            .helpers[context.storage.declarations.enums.index(new_enums)] =
                            context.storage.declarations.range<NativeEnumHelper>(
                                {NativeEnumHelper::traits});
                        return new_enums;
                    }()});
                return new_normal_modules;
            }()}})};

    ASSERT_EQ(files.size(), 3);
    auto const& native_header{files[0].content};
    auto const& projection_header{files[1].content};
    auto const& conversion_header{files[2].content};
    EXPECT_EQ(native_header.find("enum_count<"), std::string::npos);
    EXPECT_NE(projection_header.find("enum_count<::ENativeState>"), std::string::npos);
    EXPECT_NE(native_header.find("enum class NativeState : NativeStateUnderlying"),
              std::string::npos);
    auto const traits_start{native_header.find("struct EnumTraits<::fixture::NativeState>")};
    ASSERT_NE(traits_start, std::string::npos);
    auto const traits_end{native_header.find("};", traits_start)};
    ASSERT_NE(traits_end, std::string::npos);
    auto const traits{native_header.substr(traits_start, traits_end - traits_start)};
    EXPECT_NE(traits.find("::fixture::NativeState::Idle"), std::string::npos);
    EXPECT_NE(traits.find("::fixture::NativeState::Active"), std::string::npos);
    EXPECT_EQ(traits.find("::fixture::NativeState::COUNT"), std::string::npos);
    EXPECT_EQ(native_header.find("CoreMinimal.h"), std::string::npos);
    EXPECT_NE(projection_header.find("#include \"NativeState.generated.h\""), std::string::npos);
    EXPECT_NE(projection_header.find("UENUM()\nenum class ENativeState : uint8"),
              std::string::npos);
    EXPECT_NE(conversion_header.find("#include \"fixture/NativeEnum.h\"\n"
                                     "#include \"Project/NativeState.h\""),
              std::string::npos);
    EXPECT_NE(conversion_header.find("static_assert(static_cast<int>(ENativeState::Active) == "
                                     "static_cast<int>(::fixture::NativeState::Active));"),
              std::string::npos);
}

TEST(Lowering, EmitsReflectedEnumsAndSelectableOutOfLineConversions) {
    CompilerTestContext context;
    auto const output{render_enum(context, [&] {
        auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
        context.storage.declarations.normal_modules
            .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] = [&] {
            auto const new_module_settings_store{
                context.storage.declarations.module_settings_store.append()};
            context.storage.declarations.module_settings_store
                .name[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.intern("modes");
            context.storage.declarations.module_settings_store
                .header[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.intern("Modes.h");
            context.storage.declarations.module_settings_store
                .source[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.intern("Modes.cpp");
            context.storage.declarations.module_settings_store
                .header_include[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.intern("Project/Modes.h");
            return new_module_settings_store;
        }();
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(new_normal_modules)] =
            context.storage.declarations.range<DeclarationHandle>({[&] {
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
                    context.storage.declarations.range<EnumeratorId>(
                        {[&] {
                             auto const new_enumerators{
                                 context.storage.declarations.enumerators.append()};
                             context.storage.declarations.enumerators
                                 .name[context.storage.declarations.enumerators.index(
                                     new_enumerators)] = context.storage.strings.intern("First");
                             context.storage.declarations.enumerators
                                 .initializer[context.storage.declarations.enumerators.index(
                                     new_enumerators)] = std::nullopt;
                             context.storage.declarations.enumerators
                                 .display_name[context.storage.declarations.enumerators.index(
                                     new_enumerators)] = std::nullopt;
                             context.storage.declarations.enumerators
                                 .hidden[context.storage.declarations.enumerators.index(
                                     new_enumerators)] = false;
                             context.storage.declarations.enumerators
                                 .serialized_name[context.storage.declarations.enumerators.index(
                                     new_enumerators)] = context.storage.strings.intern("first");
                             return new_enumerators;
                         }(),
                         [&] {
                             auto const new_enumerators{
                                 context.storage.declarations.enumerators.append()};
                             context.storage.declarations.enumerators
                                 .name[context.storage.declarations.enumerators.index(
                                     new_enumerators)] = context.storage.strings.intern("Readable");
                             context.storage.declarations.enumerators
                                 .initializer[context.storage.declarations.enumerators.index(
                                     new_enumerators)] = context.storage.strings.intern("7");
                             context.storage.declarations.enumerators
                                 .display_name[context.storage.declarations.enumerators.index(
                                     new_enumerators)] =
                                 context.storage.strings.intern("Readable Value");
                             context.storage.declarations.enumerators
                                 .hidden[context.storage.declarations.enumerators.index(
                                     new_enumerators)] = false;
                             context.storage.declarations.enumerators
                                 .serialized_name[context.storage.declarations.enumerators.index(
                                     new_enumerators)] = context.storage.strings.intern("readable");
                             return new_enumerators;
                         }(),
                         [&] {
                             auto const new_enumerators{
                                 context.storage.declarations.enumerators.append()};
                             context.storage.declarations.enumerators
                                 .name[context.storage.declarations.enumerators.index(
                                     new_enumerators)] = context.storage.strings.intern("COUNT");
                             context.storage.declarations.enumerators
                                 .initializer[context.storage.declarations.enumerators.index(
                                     new_enumerators)] = std::nullopt;
                             context.storage.declarations.enumerators
                                 .display_name[context.storage.declarations.enumerators.index(
                                     new_enumerators)] = std::nullopt;
                             context.storage.declarations.enumerators
                                 .hidden[context.storage.declarations.enumerators.index(
                                     new_enumerators)] = true;
                             context.storage.declarations.enumerators
                                 .serialized_name[context.storage.declarations.enumerators.index(
                                     new_enumerators)] = context.storage.strings.intern("count");
                             return new_enumerators;
                         }()});
                context.storage.declarations.enums
                    .conversions[context.storage.declarations.enums.index(new_enums)] =
                    context.storage.declarations.range<EnumConversion>(
                        {EnumConversion::lex_to_string,
                         EnumConversion::string_view,
                         EnumConversion::display_string_view,
                         EnumConversion::lex_to_serialized_string,
                         EnumConversion::try_parse_serialized});
                context.storage.declarations.enums
                    .export_specifier[context.storage.declarations.enums.index(new_enums)] =
                    context.storage.strings.intern("PROJECT_API");
                context.storage.declarations.enums
                    .reflection[context.storage.declarations.enums.index(new_enums)] =
                    EnumReflection::blueprint;
                return new_enums;
            }()});
        context.storage.declarations.normal_modules
            .enum_helper_namespace[context.storage.declarations.normal_modules.index(
                new_normal_modules)] = context.storage.strings.intern("project");
        return new_normal_modules;
    }())};

    EXPECT_NE(output.header.find("#include \"Modes.generated.h\""), std::string::npos);
    EXPECT_NE(output.header.find("UENUM(BlueprintType)\nenum class EMode : uint8"),
              std::string::npos);
    EXPECT_NE(output.header.find("Readable = 7 UMETA(DisplayName = \"Readable Value\")"),
              std::string::npos);
    EXPECT_NE(output.header.find("COUNT UMETA(Hidden)"), std::string::npos);
    EXPECT_NE(
        output.header.find("PROJECT_API auto LexToString(EMode const value) -> TCHAR const*;"),
        std::string::npos);
    EXPECT_NE(output.header.find("namespace project {\nPROJECT_API auto to_string_view"),
              std::string::npos);
    EXPECT_NE(output.header.find(
                  "PROJECT_API auto LexToSerializedString(EMode const value) -> TCHAR const*;"),
              std::string::npos);
    EXPECT_NE(
        output.header.find(
            "PROJECT_API auto try_parse_serialized(FStringView const value, EMode& result) -> "
            "bool;"),
        std::string::npos);
    EXPECT_EQ(output.header.find("auto to_string(EMode"), std::string::npos);

    EXPECT_NE(output.source.find("#include \"Project/Modes.h\""), std::string::npos);
    EXPECT_NE(output.source.find("switch (value)"), std::string::npos);
    EXPECT_NE(output.source.find("TEXT(\"<invalid EMode>\")"), std::string::npos);
    EXPECT_NE(output.source.find("return TEXT(\"Readable Value\");"), std::string::npos);
    EXPECT_NE(output.source.find("return TEXT(\"readable\");"), std::string::npos);
    EXPECT_NE(output.source.find("result = EMode::Readable;"), std::string::npos);
    EXPECT_NE(output.source.find("auto LexToString(EMode const value) -> TCHAR const*"),
              std::string::npos);
}

TEST(Lowering, EmitsPlainEnumsInTheirNamespace) {
    CompilerTestContext context;
    auto const output{render_enum(context, [&] {
        auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
        context.storage.declarations.normal_modules
            .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] = [&] {
            auto const new_module_settings_store{
                context.storage.declarations.module_settings_store.append()};
            context.storage.declarations.module_settings_store
                .name[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.intern("states");
            context.storage.declarations.module_settings_store
                .header[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.intern("States.h");
            context.storage.declarations.module_settings_store
                .source[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.intern("States.cpp");
            context.storage.declarations.module_settings_store
                .namespace_name[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.intern("project::states");
            return new_module_settings_store;
        }();
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(new_normal_modules)] =
            context.storage.declarations.range<DeclarationHandle>({[&] {
                auto const new_enums{context.storage.declarations.enums.append()};
                context.storage.declarations.enums
                    .name[context.storage.declarations.enums.index(new_enums)] =
                    context.storage.strings.intern("EState");
                context.storage.declarations.enums
                    .underlying_type[context.storage.declarations.enums.index(new_enums)] = [&] {
                    auto const new_type_refs{context.storage.declarations.type_refs.append()};
                    context.storage.declarations.type_refs
                        .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                        context.storage.strings.intern("int");
                    return new_type_refs;
                }();
                context.storage.declarations.enums.values[context.storage.declarations.enums.index(
                    new_enums)] = context.storage.declarations.range<EnumeratorId>({[&] {
                    auto const new_enumerators{context.storage.declarations.enumerators.append()};
                    context.storage.declarations.enumerators
                        .name[context.storage.declarations.enumerators.index(new_enumerators)] =
                        context.storage.strings.intern("Ready");
                    return new_enumerators;
                }()});
                context.storage.declarations.enums
                    .conversions[context.storage.declarations.enums.index(new_enums)] =
                    context.storage.declarations.range<EnumConversion>({EnumConversion::string});
                return new_enums;
            }()});
        return new_normal_modules;
    }())};

    EXPECT_NE(output.header.find("namespace project::states {\nusing EStateUnderlying = int;\n\n"
                                 "// Logical bit width: 1.\nenum class EState : EStateUnderlying"),
              std::string::npos);
    EXPECT_NE(output.header.find("using EStateUnderlying = int;"), std::string::npos);
    EXPECT_NE(output.header.find("namespace project::states {\nauto to_string(EState const value)"),
              std::string::npos);
    EXPECT_EQ(output.header.find("UENUM"), std::string::npos);
    EXPECT_NE(output.source.find("project::states::EState::Ready"), std::string::npos);
}

TEST(Lowering, EmitsCountsForEnumArrays) {
    CompilerTestContext context;
    auto const output{render_enum(context, [&] {
        auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
        context.storage.declarations.normal_modules
            .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] = [&] {
            auto const new_module_settings_store{
                context.storage.declarations.module_settings_store.append()};
            context.storage.declarations.module_settings_store
                .name[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.intern("modes");
            context.storage.declarations.module_settings_store
                .header[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.intern("Modes.h");
            context.storage.declarations.module_settings_store
                .source[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.intern("Modes.cpp");
            return new_module_settings_store;
        }();
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(new_normal_modules)] =
            context.storage.declarations.range<DeclarationHandle>({[&] {
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
                    context.storage.declarations.range<EnumeratorId>(
                        {[&] {
                             auto const new_enumerators{
                                 context.storage.declarations.enumerators.append()};
                             context.storage.declarations.enumerators
                                 .name[context.storage.declarations.enumerators.index(
                                     new_enumerators)] = context.storage.strings.intern("First");
                             return new_enumerators;
                         }(),
                         [&] {
                             auto const new_enumerators{
                                 context.storage.declarations.enumerators.append()};
                             context.storage.declarations.enumerators
                                 .name[context.storage.declarations.enumerators.index(
                                     new_enumerators)] = context.storage.strings.intern("Second");
                             return new_enumerators;
                         }()});
                context.storage.declarations.enums
                    .enum_array[context.storage.declarations.enums.index(new_enums)] = true;
                return new_enums;
            }()});
        return new_normal_modules;
    }())};

    EXPECT_NE(output.header.find("#include \"ioj/enum_traits.h\""), std::string::npos);
    EXPECT_NE(
        output.header.find(
            "constexpr auto enum_count<::EMode>() noexcept -> std::size_t {\n    return 2;\n}"),
        std::string::npos);
    EXPECT_EQ(output.header.find("COUNT"), std::string::npos);
}

TEST(Lowering, EmitsCountsForReflectedEnumArrays) {
    CompilerTestContext context;
    auto const output{render_enum(context, [&] {
        auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
        context.storage.declarations.normal_modules
            .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] = [&] {
            auto const new_module_settings_store{
                context.storage.declarations.module_settings_store.append()};
            context.storage.declarations.module_settings_store
                .name[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.intern("modes");
            context.storage.declarations.module_settings_store
                .header[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.intern("Modes.h");
            context.storage.declarations.module_settings_store
                .source[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.intern("Modes.cpp");
            return new_module_settings_store;
        }();
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(new_normal_modules)] =
            context.storage.declarations.range<DeclarationHandle>({[&] {
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
                    context.storage.declarations.range<EnumeratorId>(
                        {[&] {
                             auto const new_enumerators{
                                 context.storage.declarations.enumerators.append()};
                             context.storage.declarations.enumerators
                                 .name[context.storage.declarations.enumerators.index(
                                     new_enumerators)] = context.storage.strings.intern("First");
                             return new_enumerators;
                         }(),
                         [&] {
                             auto const new_enumerators{
                                 context.storage.declarations.enumerators.append()};
                             context.storage.declarations.enumerators
                                 .name[context.storage.declarations.enumerators.index(
                                     new_enumerators)] = context.storage.strings.intern("Second");
                             return new_enumerators;
                         }()});
                context.storage.declarations.enums
                    .reflection[context.storage.declarations.enums.index(new_enums)] =
                    EnumReflection::uenum;
                context.storage.declarations.enums
                    .enum_array[context.storage.declarations.enums.index(new_enums)] = true;
                return new_enums;
            }()});
        return new_normal_modules;
    }())};

    EXPECT_NE(output.header.find("#include \"ioj/enum_traits.h\""), std::string::npos);
    EXPECT_NE(
        output.header.find("enum_count<::EMode>() noexcept -> std::size_t {\n    return 2;\n}"),
        std::string::npos);
}

TEST(Lowering, EnumDisplayLookupWithoutOverridesFallsBackDirectly) {
    CompilerTestContext context;
    auto const output{render_enum(context, [&] {
        auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
        context.storage.declarations.normal_modules
            .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] = [&] {
            auto const new_module_settings_store{
                context.storage.declarations.module_settings_store.append()};
            context.storage.declarations.module_settings_store
                .name[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.intern("states");
            context.storage.declarations.module_settings_store
                .header[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.intern("States.h");
            context.storage.declarations.module_settings_store
                .source[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.intern("States.cpp");
            return new_module_settings_store;
        }();
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(new_normal_modules)] =
            context.storage.declarations.range<DeclarationHandle>({[&] {
                auto const new_enums{context.storage.declarations.enums.append()};
                context.storage.declarations.enums
                    .name[context.storage.declarations.enums.index(new_enums)] =
                    context.storage.strings.intern("EState");
                context.storage.declarations.enums
                    .underlying_type[context.storage.declarations.enums.index(new_enums)] = [&] {
                    auto const new_type_refs{context.storage.declarations.type_refs.append()};
                    context.storage.declarations.type_refs
                        .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                        context.storage.strings.intern("int");
                    return new_type_refs;
                }();
                context.storage.declarations.enums.values[context.storage.declarations.enums.index(
                    new_enums)] = context.storage.declarations.range<EnumeratorId>({[&] {
                    auto const new_enumerators{context.storage.declarations.enumerators.append()};
                    context.storage.declarations.enumerators
                        .name[context.storage.declarations.enumerators.index(new_enumerators)] =
                        context.storage.strings.intern("Ready");
                    return new_enumerators;
                }()});
                context.storage.declarations.enums
                    .conversions[context.storage.declarations.enums.index(new_enums)] =
                    context.storage.declarations.range<EnumConversion>(
                        {EnumConversion::display_string});
                return new_enums;
            }()});
        return new_normal_modules;
    }())};

    EXPECT_NE(
        output.source.find("auto get_state_display_name(EState const value) -> TCHAR const* {\n"
                           "    return get_state_name(value);\n"
                           "}"),
        std::string::npos);
    EXPECT_EQ(occurrences(output.source, "switch (value)"), 1);
    EXPECT_NE(output.source.find("return FString{get_state_display_name(value)};"),
              std::string::npos);
}

TEST(Lowering, EnumEscapingAndSparseDisplayCasesPreserveFallbacks) {
    CompilerTestContext context;
    auto const output{render_enum(context, [&] {
        auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
        context.storage.declarations.normal_modules
            .settings[context.storage.declarations.normal_modules.index(new_normal_modules)] = [&] {
            auto const new_module_settings_store{
                context.storage.declarations.module_settings_store.append()};
            context.storage.declarations.module_settings_store
                .name[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.intern("states");
            context.storage.declarations.module_settings_store
                .header[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.intern("States.h");
            context.storage.declarations.module_settings_store
                .source[context.storage.declarations.module_settings_store.index(
                    new_module_settings_store)] = context.storage.strings.intern("States.cpp");
            return new_module_settings_store;
        }();
        context.storage.declarations.normal_modules
            .declarations[context.storage.declarations.normal_modules.index(new_normal_modules)] =
            context.storage.declarations.range<DeclarationHandle>({[&] {
                auto const new_enums{context.storage.declarations.enums.append()};
                context.storage.declarations.enums
                    .name[context.storage.declarations.enums.index(new_enums)] =
                    context.storage.strings.intern("EState");
                context.storage.declarations.enums
                    .underlying_type[context.storage.declarations.enums.index(new_enums)] = [&] {
                    auto const new_type_refs{context.storage.declarations.type_refs.append()};
                    context.storage.declarations.type_refs
                        .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                        context.storage.strings.intern("int");
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
                                     new_enumerators)] = context.storage.strings.intern("Ready");
                             context.storage.declarations.enumerators
                                 .initializer[context.storage.declarations.enumerators.index(
                                     new_enumerators)] = std::nullopt;
                             context.storage.declarations.enumerators
                                 .display_name[context.storage.declarations.enumerators.index(
                                     new_enumerators)] = std::nullopt;
                             context.storage.declarations.enumerators
                                 .hidden[context.storage.declarations.enumerators.index(
                                     new_enumerators)] = false;
                             context.storage.declarations.enumerators
                                 .serialized_name[context.storage.declarations.enumerators.index(
                                     new_enumerators)] = context.storage.strings.intern("ready");
                             return new_enumerators;
                         }(),
                         [&] {
                             auto const new_enumerators{
                                 context.storage.declarations.enumerators.append()};
                             context.storage.declarations.enumerators
                                 .name[context.storage.declarations.enumerators.index(
                                     new_enumerators)] = context.storage.strings.intern("Special");
                             context.storage.declarations.enumerators
                                 .initializer[context.storage.declarations.enumerators.index(
                                     new_enumerators)] = std::nullopt;
                             context.storage.declarations.enumerators
                                 .display_name[context.storage.declarations.enumerators.index(
                                     new_enumerators)] =
                                 context.storage.strings.intern("Quote\" Slash\\\n\t");
                             context.storage.declarations.enumerators
                                 .hidden[context.storage.declarations.enumerators.index(
                                     new_enumerators)] = false;
                             context.storage.declarations.enumerators
                                 .serialized_name[context.storage.declarations.enumerators.index(
                                     new_enumerators)] =
                                 context.storage.strings.intern("a\"b\\c\n");
                             return new_enumerators;
                         }()});
                context.storage.declarations.enums
                    .conversions[context.storage.declarations.enums.index(new_enums)] =
                    context.storage.declarations.range<EnumConversion>(
                        {EnumConversion::display_string,
                         EnumConversion::lex_to_serialized_string,
                         EnumConversion::try_parse_serialized});
                context.storage.declarations.enums
                    .reflection[context.storage.declarations.enums.index(new_enums)] =
                    EnumReflection::uenum;
                return new_enums;
            }()});
        return new_normal_modules;
    }())};

    EXPECT_NE(output.header.find("UMETA(DisplayName = \"Quote\\\" Slash\\\\\\n\\t\")"),
              std::string::npos);
    EXPECT_NE(
        output.source.find("auto get_state_display_name(EState const value) -> TCHAR const* {\n"
                           "    switch (value) {\n"
                           "    case EState::Special: {\n"
                           "        return TEXT(\"Quote\\\" Slash\\\\\\n\\t\");\n"
                           "    }\n"
                           "    default: {\n"
                           "        break;\n"
                           "    }\n"
                           "    }\n\n"
                           "    return get_state_name(value);\n"
                           "}"),
        std::string::npos);
    EXPECT_NE(output.source.find("if (value == TEXT(\"a\\\"b\\\\c\\n\")) {\n"
                                 "        result = EState::Special;\n"
                                 "        return true;\n"
                                 "    }\n"
                                 "    return false;"),
              std::string::npos);
}

TEST(Lowering, EnumCountsUseMlNamespaceForQualifiedHeaderOnlyEnums) {
    CompilerTestContext context;
    auto const files{render_test_manifest(
        context,
        Manifest{
            manifest_schema_version,
            make_test_registry(context),
            context.storage.declarations,
            {[&] {
                auto const new_normal_modules{context.storage.declarations.normal_modules.append()};
                context.storage.declarations.normal_modules
                    .settings[context.storage.declarations.normal_modules.index(
                        new_normal_modules)] = [&] {
                    auto const new_module_settings_store{
                        context.storage.declarations.module_settings_store.append()};
                    context.storage.declarations.module_settings_store
                        .name[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] = context.storage.strings.intern("states");
                    context.storage.declarations.module_settings_store
                        .header[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] =
                        context.storage.strings.intern("States.h");
                    context.storage.declarations.module_settings_store
                        .namespace_name[context.storage.declarations.module_settings_store.index(
                            new_module_settings_store)] =
                        context.storage.strings.intern("project::states");
                    return new_module_settings_store;
                }();
                context.storage.declarations.normal_modules
                    .declarations[context.storage.declarations.normal_modules.index(
                        new_normal_modules)] =
                    context.storage.declarations.range<DeclarationHandle>({[&] {
                        auto const new_enums{context.storage.declarations.enums.append()};
                        context.storage.declarations.enums
                            .name[context.storage.declarations.enums.index(new_enums)] =
                            context.storage.strings.intern("EState");
                        context.storage.declarations.enums
                            .underlying_type[context.storage.declarations.enums.index(
                                new_enums)] = [&] {
                            auto const new_type_refs{
                                context.storage.declarations.type_refs.append()};
                            context.storage.declarations.type_refs
                                .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                                context.storage.strings.intern("int");
                            return new_type_refs;
                        }();
                        context.storage.declarations.enums
                            .values[context.storage.declarations.enums.index(new_enums)] =
                            context.storage.declarations.range<EnumeratorId>({[&] {
                                auto const new_enumerators{
                                    context.storage.declarations.enumerators.append()};
                                context.storage.declarations.enumerators
                                    .name[context.storage.declarations.enumerators.index(
                                        new_enumerators)] = context.storage.strings.intern("Ready");
                                return new_enumerators;
                            }()});
                        context.storage.declarations.enums
                            .enum_array[context.storage.declarations.enums.index(new_enums)] = true;
                        return new_enums;
                    }()});
                return new_normal_modules;
            }()}})};
    ASSERT_EQ(files.size(), 1);
    EXPECT_NE(files.front().content.find("namespace ioj {"), std::string::npos);
    EXPECT_NE(
        files.front().content.find(
            "enum_count<::project::states::EState>() noexcept -> std::size_t {\n    return 1;\n}"),
        std::string::npos);
}

TEST(Lowering, AppliesEveryStorageOperationToEveryMember) {
    CompilerTestContext context;
    auto schema{basic_schema(context)};
    context.storage.declarations.soas.operations[context.storage.declarations.soas.index(schema)] =
        context.storage.declarations.range<StorageOperation>(all_storage_operations());
    auto const output{render_soa(context, schema)};

    std::vector<std::pair<std::string const*, std::string>> const expectations{
        {&output.source, "ioj::soa_ops::reset(*this);"},
        {&output.source, "ioj::soa_ops::reserve(*this, count);"},
        {&output.source, "ioj::soa_ops::add_uninitialised(*this, count);"},
        {&output.source, "ioj::soa_ops::add_defaulted(*this, count);"},
        {&output.header, "ioj::soa_ops::remove_at_swap(*this, index, count, allow_shrinking);"},
        {&output.source, "ioj::soa_ops::set_num(*this, count, allow_shrinking);"},
        {&output.header, "ioj::copy_element(ids, dst_i, other.ids, src_i);"},
        {&output.header, "ioj::copy_element(weights, dst_i, other.weights, src_i);"},
        {&output.header, "ioj::append_from(ids, other.ids);"},
        {&output.header, "ioj::append_from(weights, other.weights);"},
        {&output.source, "ioj::soa_ops::apply_permutation(*this, indices);"},
    };

    for (auto const& [text, expected] : expectations) {
        SCOPED_TRACE(expected);
        EXPECT_NE(text->find(expected), std::string::npos);
    }
}

TEST(Lowering, PreservesCustomNestedRemovalOperation) {
    CompilerTestContext context;
    auto const registered{context.storage.cpp_types.add(
        "FRegistered",
        {context.storage.cpp_types.dependency("FRegistered", "Project/Registered.h")})};
    context.storage.cpp_types.set_operation(
        registered, TypeOperation::remove_at_swap, "erase_swap");
    auto schema{[&] {
        auto const new_soas{context.storage.declarations.soas.append()};
        context.storage.declarations.soas.name[context.storage.declarations.soas.index(new_soas)] =
            context.storage.strings.intern("FData");
        context.storage.declarations.soas
            .members[context.storage.declarations.soas.index(new_soas)] =
            context.storage.declarations.range<SoaMemberId>(
                {[&] {
                     auto const new_soa_members{context.storage.declarations.soa_members.append()};
                     context.storage.declarations.soa_members
                         .name[context.storage.declarations.soa_members.index(new_soa_members)] =
                         context.storage.strings.intern("values");
                     context.storage.declarations.soa_members
                         .kind[context.storage.declarations.soa_members.index(new_soa_members)] =
                         SoaMemberKind::array;
                     context.storage.declarations.soa_members
                         .type[context.storage.declarations.soa_members.index(
                             new_soa_members)] = [&] {
                         auto const new_type_refs{context.storage.declarations.type_refs.append()};
                         context.storage.declarations.type_refs
                             .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                             context.storage.strings.intern("int32");
                         return new_type_refs;
                     }();
                     return new_soa_members;
                 }(),
                 [&] {
                     auto const new_soa_members{context.storage.declarations.soa_members.append()};
                     context.storage.declarations.soa_members
                         .name[context.storage.declarations.soa_members.index(new_soa_members)] =
                         context.storage.strings.intern("registered");
                     context.storage.declarations.soa_members
                         .kind[context.storage.declarations.soa_members.index(new_soa_members)] =
                         SoaMemberKind::nested;
                     context.storage.declarations.soa_members
                         .type[context.storage.declarations.soa_members.index(
                             new_soa_members)] = [&] {
                         auto const new_type_refs{context.storage.declarations.type_refs.append()};
                         context.storage.declarations.type_refs
                             .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                             context.storage.strings.intern("@registered");
                         return new_type_refs;
                     }();
                     return new_soa_members;
                 }(),
                 [&] {
                     auto const new_soa_members{context.storage.declarations.soa_members.append()};
                     context.storage.declarations.soa_members
                         .name[context.storage.declarations.soa_members.index(new_soa_members)] =
                         context.storage.strings.intern("generic");
                     context.storage.declarations.soa_members
                         .kind[context.storage.declarations.soa_members.index(new_soa_members)] =
                         SoaMemberKind::nested;
                     context.storage.declarations.soa_members
                         .type[context.storage.declarations.soa_members.index(
                             new_soa_members)] = [&] {
                         auto const new_type_refs{context.storage.declarations.type_refs.append()};
                         context.storage.declarations.type_refs
                             .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                             context.storage.strings.intern("@generic");
                         return new_type_refs;
                     }();
                     return new_soa_members;
                 }()});
        context.storage.declarations.soas
            .operations[context.storage.declarations.soas.index(new_soas)] =
            context.storage.declarations.range<StorageOperation>(
                {StorageOperation::remove_at_swap});
        return new_soas;
    }()};

    auto const output{render_soa(
        context,
        schema,
        {{"registered", registered}, {"generic", context.storage.cpp_types.add("FGeneric")}})};

    auto const removal{output.header.find("void remove_at_swap(")};
    ASSERT_NE(removal, std::string::npos);
    auto const validation{output.header.find("validate_array_sizes();", removal)};
    auto const first_member{
        output.header.find("values.RemoveAtSwap(index, count, allow_shrinking);", removal)};
    ASSERT_NE(validation, std::string::npos);
    ASSERT_NE(first_member, std::string::npos);
    EXPECT_LT(validation, first_member);
    EXPECT_NE(output.header.find("registered.erase_swap(index, count, allow_shrinking);"),
              std::string::npos);
    EXPECT_NE(output.header.find("ioj::remove_at_swap(generic, index, count, allow_shrinking);"),
              std::string::npos);
    EXPECT_EQ(occurrences(output.header, "#include \"SandboxCore/container_ops.h\""), 1);
}

TEST(Lowering, CustomRemovalDoesNotRequireGenericContainerHelpers) {
    CompilerTestContext context;
    auto const registered{context.storage.cpp_types.add(
        "FRegistered",
        {context.storage.cpp_types.dependency("FRegistered", "Project/Registered.h")})};
    context.storage.cpp_types.set_operation(
        registered, TypeOperation::remove_at_swap, "erase_swap");
    auto schema{[&] {
        auto const new_soas{context.storage.declarations.soas.append()};
        context.storage.declarations.soas.name[context.storage.declarations.soas.index(new_soas)] =
            context.storage.strings.intern("FData");
        context.storage.declarations.soas
            .members[context.storage.declarations.soas.index(new_soas)] =
            context.storage.declarations.range<SoaMemberId>({[&] {
                auto const new_soa_members{context.storage.declarations.soa_members.append()};
                context.storage.declarations.soa_members
                    .name[context.storage.declarations.soa_members.index(new_soa_members)] =
                    context.storage.strings.intern("registered");
                context.storage.declarations.soa_members
                    .kind[context.storage.declarations.soa_members.index(new_soa_members)] =
                    SoaMemberKind::nested;
                context.storage.declarations.soa_members
                    .type[context.storage.declarations.soa_members.index(new_soa_members)] = [&] {
                    auto const new_type_refs{context.storage.declarations.type_refs.append()};
                    context.storage.declarations.type_refs
                        .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                        context.storage.strings.intern("@registered");
                    return new_type_refs;
                }();
                return new_soa_members;
            }()});
        context.storage.declarations.soas
            .operations[context.storage.declarations.soas.index(new_soas)] =
            context.storage.declarations.range<StorageOperation>(
                {StorageOperation::remove_at_swap});
        return new_soas;
    }()};
    auto const output{render_soa(context, schema, {{"registered", registered}})};
    EXPECT_NE(output.header.find("#include \"Project/Registered.h\""), std::string::npos);
    EXPECT_NE(output.header.find("registered.erase_swap(index, count, allow_shrinking);"),
              std::string::npos);
    EXPECT_EQ(output.header.find("SandboxCore/container_ops.h"), std::string::npos);
}

TEST(Lowering, UsesSharedConventionalNestedRemovalOperation) {
    CompilerTestContext context;
    auto const nested{context.storage.cpp_types.add(
        "FNested", {context.storage.cpp_types.dependency("FNested", "Project/Nested.h")})};
    context.storage.cpp_types.set_operation(
        nested, TypeOperation::remove_at_swap, "remove_at_swap");
    auto schema{[&] {
        auto const new_soas{context.storage.declarations.soas.append()};
        context.storage.declarations.soas.name[context.storage.declarations.soas.index(new_soas)] =
            context.storage.strings.intern("FData");
        context.storage.declarations.soas
            .members[context.storage.declarations.soas.index(new_soas)] =
            context.storage.declarations.range<SoaMemberId>({[&] {
                auto const new_soa_members{context.storage.declarations.soa_members.append()};
                context.storage.declarations.soa_members
                    .name[context.storage.declarations.soa_members.index(new_soa_members)] =
                    context.storage.strings.intern("nested");
                context.storage.declarations.soa_members
                    .kind[context.storage.declarations.soa_members.index(new_soa_members)] =
                    SoaMemberKind::nested;
                context.storage.declarations.soa_members
                    .type[context.storage.declarations.soa_members.index(new_soa_members)] = [&] {
                    auto const new_type_refs{context.storage.declarations.type_refs.append()};
                    context.storage.declarations.type_refs
                        .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                        context.storage.strings.intern("@nested");
                    return new_type_refs;
                }();
                return new_soa_members;
            }()});
        context.storage.declarations.soas
            .operations[context.storage.declarations.soas.index(new_soas)] =
            context.storage.declarations.range<StorageOperation>(
                {StorageOperation::remove_at_swap});
        return new_soas;
    }()};
    auto const output{render_soa(context, schema, {{"nested", nested}})};

    EXPECT_NE(
        output.header.find("ioj::soa_ops::remove_at_swap(*this, index, count, allow_shrinking);"),
        std::string::npos);
    EXPECT_EQ(occurrences(output.header, "#include \"SandboxCore/soa_storage_ops.h\""), 1);
}

TEST(Lowering, SourceOnlyStorageHelpersDoNotLeakIntoHeader) {
    CompilerTestContext context;
    auto schema{basic_schema(context)};
    context.storage.declarations.soas.operations[context.storage.declarations.soas.index(schema)] =
        context.storage.declarations.range<StorageOperation>({StorageOperation::reserve});
    auto const output{render_soa(context, schema)};
    EXPECT_EQ(output.header.find("SandboxCore/container_ops.h"), std::string::npos);
    EXPECT_EQ(occurrences(output.source, "#include \"SandboxCore/container_ops.h\""), 1);
    EXPECT_NE(output.source.find("ioj::soa_ops::reserve(*this, count);"), std::string::npos);
}

TEST(Lowering, CustomSetAndAddStillSuppressGeneratedBodies) {
    CompilerTestContext context;
    auto schema{basic_schema(context)};
    context.storage.declarations.soas.functions[context.storage.declarations.soas.index(schema)] =
        context.storage.declarations.range<FunctionSchemaId>(
            {[&] {
                 auto const new_functions{context.storage.declarations.functions.append()};
                 context.storage.declarations.functions
                     .name[context.storage.declarations.functions.index(new_functions)] =
                     context.storage.strings.intern("set");
                 context.storage.declarations.functions
                     .return_type[context.storage.declarations.functions.index(new_functions)] =
                     [&] {
                         auto const new_type_refs{context.storage.declarations.type_refs.append()};
                         context.storage.declarations.type_refs
                             .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                             context.storage.strings.intern("void");
                         return new_type_refs;
                     }();
                 context.storage.declarations.functions
                     .body_lines[context.storage.declarations.functions.index(new_functions)] =
                     context.storage.declarations.range<StringId>(
                         {context.storage.strings.intern("custom_set();")});
                 context.storage.declarations.functions
                     .is_inline[context.storage.declarations.functions.index(new_functions)] = true;
                 return new_functions;
             }(),
             [&] {
                 auto const new_functions{context.storage.declarations.functions.append()};
                 context.storage.declarations.functions
                     .name[context.storage.declarations.functions.index(new_functions)] =
                     context.storage.strings.intern("add");
                 context.storage.declarations.functions
                     .return_type[context.storage.declarations.functions.index(new_functions)] =
                     [&] {
                         auto const new_type_refs{context.storage.declarations.type_refs.append()};
                         context.storage.declarations.type_refs
                             .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                             context.storage.strings.intern("void");
                         return new_type_refs;
                     }();
                 context.storage.declarations.functions
                     .body_lines[context.storage.declarations.functions.index(new_functions)] =
                     context.storage.declarations.range<StringId>(
                         {context.storage.strings.intern("custom_add();")});
                 context.storage.declarations.functions
                     .is_inline[context.storage.declarations.functions.index(new_functions)] = true;
                 return new_functions;
             }()});
    auto const output{render_soa(context, schema)};
    EXPECT_EQ(occurrences(output.header, "custom_set();"), 1);
    EXPECT_EQ(occurrences(output.header, "custom_add();"), 1);
    EXPECT_EQ(output.header.find("ids[index] = new_ids;"), std::string::npos);
    EXPECT_EQ(output.header.find("ids.Add(new_ids);"), std::string::npos);
}

TEST(Lowering, EmitsLogicalElementSettersAndAddForArraysAndSupportedNestedMembers) {
    CompilerTestContext context;
    auto const vectors{context.storage.cpp_types.add(
        "FVectors3f", {context.storage.cpp_types.dependency("FVectors3f", "Project/Vectors.h")})};
    context.storage.cpp_types.set_operation(
        vectors, TypeOperation::add_element, "add", ParameterPassing::value);
    context.storage.cpp_types.set_operation(
        vectors, TypeOperation::set_element, "set", ParameterPassing::value);
    auto schema{[&] {
        auto const new_soas{context.storage.declarations.soas.append()};
        context.storage.declarations.soas.name[context.storage.declarations.soas.index(new_soas)] =
            context.storage.strings.intern("FData");
        context.storage.declarations.soas
            .members[context.storage.declarations.soas.index(new_soas)] =
            context.storage.declarations.range<SoaMemberId>(
                {[&] {
                     auto const new_soa_members{context.storage.declarations.soa_members.append()};
                     context.storage.declarations.soa_members
                         .name[context.storage.declarations.soa_members.index(new_soa_members)] =
                         context.storage.strings.intern("locations");
                     context.storage.declarations.soa_members
                         .kind[context.storage.declarations.soa_members.index(new_soa_members)] =
                         SoaMemberKind::nested;
                     context.storage.declarations.soa_members
                         .type[context.storage.declarations.soa_members.index(
                             new_soa_members)] = [&] {
                         auto const new_type_refs{context.storage.declarations.type_refs.append()};
                         context.storage.declarations.type_refs
                             .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                             context.storage.strings.intern("@vectors");
                         return new_type_refs;
                     }();
                     return new_soa_members;
                 }(),
                 [&] {
                     auto const new_soa_members{context.storage.declarations.soa_members.append()};
                     context.storage.declarations.soa_members
                         .name[context.storage.declarations.soa_members.index(new_soa_members)] =
                         context.storage.strings.intern("teams");
                     context.storage.declarations.soa_members
                         .kind[context.storage.declarations.soa_members.index(new_soa_members)] =
                         SoaMemberKind::array;
                     context.storage.declarations.soa_members
                         .type[context.storage.declarations.soa_members.index(
                             new_soa_members)] = [&] {
                         auto const new_type_refs{context.storage.declarations.type_refs.append()};
                         context.storage.declarations.type_refs
                             .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                             context.storage.strings.intern("ETeam");
                         return new_type_refs;
                     }();
                     return new_soa_members;
                 }(),
                 [&] {
                     auto const new_soa_members{context.storage.declarations.soa_members.append()};
                     context.storage.declarations.soa_members
                         .name[context.storage.declarations.soa_members.index(new_soa_members)] =
                         context.storage.strings.intern("healths");
                     context.storage.declarations.soa_members
                         .kind[context.storage.declarations.soa_members.index(new_soa_members)] =
                         SoaMemberKind::array;
                     context.storage.declarations.soa_members
                         .type[context.storage.declarations.soa_members.index(
                             new_soa_members)] = [&] {
                         auto const new_type_refs{context.storage.declarations.type_refs.append()};
                         context.storage.declarations.type_refs
                             .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                             context.storage.strings.intern("int32");
                         return new_type_refs;
                     }();
                     return new_soa_members;
                 }()});
        return new_soas;
    }()};

    auto const output{render_soa(context, schema, {{"vectors", vectors}})};

    EXPECT_EQ(occurrences(output.header,
                          "void set(int32 const index, FVectors3f::equivalent_type const "
                          "new_locations, ETeam const& new_teams, int32 const new_healths)"),
              2);
    EXPECT_NE(output.header.find("locations.set(index, new_locations);"), std::string::npos);
    EXPECT_NE(output.header.find("teams[index] = new_teams;"), std::string::npos);
    EXPECT_NE(output.header.find("healths[index] = new_healths;"), std::string::npos);
    EXPECT_NE(output.header.find("auto add(FVectors3f::equivalent_type const new_locations, "
                                 "ETeam const& new_teams, int32 const new_healths) -> int32"),
              std::string::npos);
    EXPECT_NE(output.header.find("auto const index{num()};"), std::string::npos);
    EXPECT_NE(output.header.find("locations.add(new_locations);"), std::string::npos);
    EXPECT_NE(output.header.find("teams.Add(new_teams);"), std::string::npos);
    EXPECT_NE(output.header.find("healths.Add(new_healths);"), std::string::npos);
    EXPECT_NE(output.header.find("return index;"), std::string::npos);
}

TEST(Lowering, OmitsLogicalElementSetterAndAddForUnsupportedNestedMembers) {
    CompilerTestContext context;
    auto schema{[&] {
        auto const new_soas{context.storage.declarations.soas.append()};
        context.storage.declarations.soas.name[context.storage.declarations.soas.index(new_soas)] =
            context.storage.strings.intern("FData");
        context.storage.declarations.soas
            .members[context.storage.declarations.soas.index(new_soas)] =
            context.storage.declarations.range<SoaMemberId>(
                {[&] {
                     auto const new_soa_members{context.storage.declarations.soa_members.append()};
                     context.storage.declarations.soa_members
                         .name[context.storage.declarations.soa_members.index(new_soa_members)] =
                         context.storage.strings.intern("ids");
                     context.storage.declarations.soa_members
                         .kind[context.storage.declarations.soa_members.index(new_soa_members)] =
                         SoaMemberKind::array;
                     context.storage.declarations.soa_members
                         .type[context.storage.declarations.soa_members.index(
                             new_soa_members)] = [&] {
                         auto const new_type_refs{context.storage.declarations.type_refs.append()};
                         context.storage.declarations.type_refs
                             .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                             context.storage.strings.intern("int32");
                         return new_type_refs;
                     }();
                     return new_soa_members;
                 }(),
                 [&] {
                     auto const new_soa_members{context.storage.declarations.soa_members.append()};
                     context.storage.declarations.soa_members
                         .name[context.storage.declarations.soa_members.index(new_soa_members)] =
                         context.storage.strings.intern("values");
                     context.storage.declarations.soa_members
                         .kind[context.storage.declarations.soa_members.index(new_soa_members)] =
                         SoaMemberKind::nested;
                     context.storage.declarations.soa_members
                         .type[context.storage.declarations.soa_members.index(
                             new_soa_members)] = [&] {
                         auto const new_type_refs{context.storage.declarations.type_refs.append()};
                         context.storage.declarations.type_refs
                             .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                             context.storage.strings.intern("@nested");
                         return new_type_refs;
                     }();
                     return new_soa_members;
                 }()});
        return new_soas;
    }()};

    auto const output{
        render_soa(context, schema, {{"nested", context.storage.cpp_types.add("FNested")}})};

    EXPECT_EQ(output.header.find("void set("), std::string::npos);
    EXPECT_EQ(output.header.find("auto add("), std::string::npos);
}

TEST(Lowering, SupportsMemberwiseCopying) {
    CompilerTestContext context;
    auto schema{basic_schema(context)};
    context.storage.declarations.soas.operations[context.storage.declarations.soas.index(schema)] =
        context.storage.declarations.range<StorageOperation>({StorageOperation::copy_element});
    context.storage.declarations.soas
        .copy_element_memberwise[context.storage.declarations.soas.index(schema)] = true;

    auto const output{render_soa(context, schema)};

    EXPECT_NE(output.header.find("ids[dst_i] = other.ids[src_i];"), std::string::npos);
    EXPECT_NE(output.header.find("weights[dst_i] = other.weights[src_i];"), std::string::npos);
    EXPECT_EQ(output.header.find("ioj::copy_element(ids"), std::string::npos);
    EXPECT_NE(output.header.find("ioj::copy_elements(ids, dst_i, other.ids, src_i, count);"),
              std::string::npos);
    EXPECT_NE(output.header.find("auto const count{other.num()};\n"
                                 "        check(num() >= count);\n"
                                 "        copy_elements(num() - count, other, 0, count);"),
              std::string::npos);
}

TEST(Lowering, LowersCustomFunctionsToTheirRequestedFile) {
    CompilerTestContext context;
    auto schema{basic_schema(context)};
    context.storage.declarations.soas.view_name[context.storage.declarations.soas.index(schema)] =
        context.storage.strings.intern("FMutableRows");
    context.storage.declarations.soas
        .const_view_name[context.storage.declarations.soas.index(schema)] =
        context.storage.strings.intern("FRows");
    context.storage.declarations.soas
        .using_declarations[context.storage.declarations.soas.index(schema)] =
        context.storage.declarations.range<StringId>(
            {context.storage.strings.intern("Index = int32")});
    context.storage.declarations.soas.functions[context.storage.declarations.soas.index(schema)] =
        context.storage.declarations.range<FunctionSchemaId>(
            {[&] {
                 auto const new_functions{context.storage.declarations.functions.append()};
                 context.storage.declarations.functions
                     .name[context.storage.declarations.functions.index(new_functions)] =
                     context.storage.strings.intern("first");
                 context.storage.declarations.functions
                     .return_type[context.storage.declarations.functions.index(new_functions)] =
                     [&] {
                         auto const new_type_refs{context.storage.declarations.type_refs.append()};
                         context.storage.declarations.type_refs
                             .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                             context.storage.strings.intern("int32");
                         return new_type_refs;
                     }();
                 context.storage.declarations.functions
                     .body_lines[context.storage.declarations.functions.index(new_functions)] =
                     context.storage.declarations.range<StringId>(
                         {context.storage.strings.intern("return ids[0];")});
                 context.storage.declarations.functions
                     .is_inline[context.storage.declarations.functions.index(new_functions)] = true;
                 return new_functions;
             }(),
             [&] {
                 auto const new_functions{context.storage.declarations.functions.append()};
                 context.storage.declarations.functions
                     .name[context.storage.declarations.functions.index(new_functions)] =
                     context.storage.strings.intern("sum");
                 context.storage.declarations.functions
                     .return_type[context.storage.declarations.functions.index(new_functions)] =
                     [&] {
                         auto const new_type_refs{context.storage.declarations.type_refs.append()};
                         context.storage.declarations.type_refs
                             .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                             context.storage.strings.intern("int32");
                         return new_type_refs;
                     }();
                 context.storage.declarations.functions
                     .body_lines[context.storage.declarations.functions.index(new_functions)] =
                     context.storage.declarations.range<StringId>(
                         {context.storage.strings.intern("return helper(ids);")});
                 context.storage.declarations.functions
                     .dependencies[context.storage.declarations.functions.index(new_functions)] =
                     context.storage.declarations.range<StringId>(
                         {context.storage.strings.intern("helper")});
                 context.storage.declarations.functions
                     .definition_in_source[context.storage.declarations.functions.index(
                         new_functions)] = true;
                 return new_functions;
             }()});

    auto const output{render_soa(
        context,
        schema,
        {{"helper",
          context.storage.cpp_types.add(
              "helper", {context.storage.cpp_types.dependency("helper", "Project/Helper.h")})}})};

    EXPECT_NE(output.header.find("using View = FMutableRows;"), std::string::npos);
    EXPECT_NE(output.header.find("using ConstView = FRows;"), std::string::npos);
    EXPECT_NE(output.header.find("using Index = int32;"), std::string::npos);
    EXPECT_NE(output.header.find("return ids[0];"), std::string::npos);
    EXPECT_EQ(output.header.find("return helper(ids);"), std::string::npos);
    EXPECT_NE(output.source.find("auto FData::sum() -> int32"), std::string::npos);
    EXPECT_NE(output.source.find("return helper(ids);"), std::string::npos);
    EXPECT_NE(output.source.find("#include \"Project/Helper.h\""), std::string::npos);
}

TEST(Lowering, ValidatesSizesBeforePermutationAndSorting) {
    CompilerTestContext context;
    auto const output{render_soa(context, basic_schema(context))};

    EXPECT_NE(output.source.find("void FData::apply_permutation("), std::string::npos);
    EXPECT_NE(output.source.find("ioj::soa_ops::apply_permutation(*this, indices);"),
              std::string::npos);
    EXPECT_NE(output.header.find("template <typename Compare>"), std::string::npos);
    EXPECT_NE(output.header.find("template <auto Compare>"), std::string::npos);
    EXPECT_EQ(occurrences(output.header, "ioj::soa_ops::validate_array_sizes(*this);"), 0);
    EXPECT_NE(output.header.find("ioj::soa_ops::sort(*this, std::forward<Compare>(compare),"),
              std::string::npos);
}

TEST(Lowering, FixedContainersOwnOneSizeAndImplementValueSemantics) {
    CompilerTestContext context;
    auto schema{basic_schema(context)};
    context.storage.declarations.soas.fixed[context.storage.declarations.soas.index(schema)] = [&] {
        auto const new_fixed_soas{context.storage.declarations.fixed_soas.append()};
        context.storage.declarations.fixed_soas
            .storage_name[context.storage.declarations.fixed_soas.index(new_fixed_soas)] =
            context.storage.strings.intern("TDataStorage");
        context.storage.declarations.fixed_soas
            .containers[context.storage.declarations.fixed_soas.index(new_fixed_soas)] =
            context.storage.declarations.range<StringId>(
                {context.storage.strings.intern("TFixedData")});
        return new_fixed_soas;
    }();

    auto const output{render_soa(context, schema)};

    EXPECT_NE(output.header.find("struct TDataStorage"), std::string::npos);
    EXPECT_NE(output.header.find("struct TFixedData"), std::string::npos);
    EXPECT_NE(output.header.find("TFixedData(TFixedData const& other)"), std::string::npos);
    EXPECT_NE(output.header.find("TFixedData(TFixedData&& other)"), std::string::npos);
    EXPECT_NE(output.header.find("~TFixedData() { reset(); }"), std::string::npos);
    EXPECT_NE(output.header.find("auto operator=(TFixedData const& other)"), std::string::npos);
    EXPECT_NE(output.header.find("void set(int32 const index, int32 const new_ids, float const "
                                 "new_weights)"),
              std::string::npos);
    EXPECT_NE(output.header.find("get_view().set(index, new_ids, new_weights);"),
              std::string::npos);
    EXPECT_NE(output.header.find("requires (Capacity >= 0)"), std::string::npos);
    EXPECT_EQ(occurrences(output.header, "size_type size_{};"), 1);
}

TEST(Lowering, GeneratesTypedSettingsApiAndRuntimeDescriptors) {
    CompilerTestContext context;
    auto const output{render_settings(context, [&] {
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
                        new_module_settings_store)] = context.storage.strings.intern("Settings.h");
                context.storage.declarations.module_settings_store
                    .source[context.storage.declarations.module_settings_store.index(
                        new_module_settings_store)] =
                    context.storage.strings.intern("Settings.cpp");
                context.storage.declarations.module_settings_store
                    .namespace_name[context.storage.declarations.module_settings_store.index(
                        new_module_settings_store)] = context.storage.strings.intern("game");
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
            context.storage.declarations.range<SettingCategoryId>({[&] {
                auto const new_setting_categories{
                    context.storage.declarations.setting_categories.append()};
                context.storage.declarations.setting_categories
                    .name[context.storage.declarations.setting_categories.index(
                        new_setting_categories)] = context.storage.strings.intern("video");
                context.storage.declarations.setting_categories
                    .label[context.storage.declarations.setting_categories.index(
                        new_setting_categories)] = context.storage.strings.intern("Video");
                return new_setting_categories;
            }()});
        context.storage.declarations.settings_modules
            .settings_list[context.storage.declarations.settings_modules.index(
                new_settings_modules)] =
            context.storage.declarations.range<SettingId>(
                {[&] {
                     auto const new_settings{context.storage.declarations.settings.append()};
                     context.storage.declarations.settings
                         .name[context.storage.declarations.settings.index(new_settings)] =
                         context.storage.strings.intern("vsync");
                     context.storage.declarations.settings
                         .label[context.storage.declarations.settings.index(new_settings)] =
                         context.storage.strings.intern("VSync");
                     context.storage.declarations.settings
                         .category[context.storage.declarations.settings.index(new_settings)] =
                         context.storage.strings.intern("video");
                     context.storage.declarations.settings
                         .value_type[context.storage.declarations.settings.index(
                             new_settings)] = [&] {
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
                 }(),
                 [&] {
                     auto const new_settings{context.storage.declarations.settings.append()};
                     context.storage.declarations.settings
                         .name[context.storage.declarations.settings.index(new_settings)] =
                         context.storage.strings.intern("frame_limit");
                     context.storage.declarations.settings
                         .label[context.storage.declarations.settings.index(new_settings)] =
                         context.storage.strings.intern("Frame Limit");
                     context.storage.declarations.settings
                         .category[context.storage.declarations.settings.index(new_settings)] =
                         context.storage.strings.intern("video");
                     context.storage.declarations.settings
                         .value_type[context.storage.declarations.settings.index(
                             new_settings)] = [&] {
                         auto const new_type_refs{context.storage.declarations.type_refs.append()};
                         context.storage.declarations.type_refs
                             .name[context.storage.declarations.type_refs.index(new_type_refs)] =
                             context.storage.strings.intern("float");
                         return new_type_refs;
                     }();
                     context.storage.declarations.settings
                         .backend[context.storage.declarations.settings.index(new_settings)] =
                         context.storage.strings.intern("engine");
                     context.storage.declarations.settings
                         .apply_mode[context.storage.declarations.settings.index(new_settings)] =
                         SettingApplyMode::immediate;
                     context.storage.declarations.settings
                         .device[context.storage.declarations.settings.index(new_settings)] =
                         SettingDevice::controller;
                     context.storage.declarations.settings
                         .control[context.storage.declarations.settings.index(new_settings)] = [&] {
                         auto const new_setting_controls{
                             context.storage.declarations.setting_controls.append()};
                         context.storage.declarations.setting_controls
                             .kind[context.storage.declarations.setting_controls.index(
                                 new_setting_controls)] = SettingControlKind::choice;
                         context.storage.declarations.setting_controls
                             .options_provider[context.storage.declarations.setting_controls.index(
                                 new_setting_controls)] =
                             context.storage.strings.intern("frame_limits");
                         return new_setting_controls;
                     }();
                     return new_settings;
                 }()});
        return new_settings_modules;
    }())};

    EXPECT_NE(output.header.find("enum class EGameSetting : uint8"), std::string::npos);
    EXPECT_NE(output.header.find("using FGameSettingValue = std::variant<bool, float>"),
              std::string::npos);
    EXPECT_NE(output.header.find("void set_vsync(bool const value)"), std::string::npos);
    EXPECT_NE(output.header.find("auto operator==(FSettingsState const&) const -> bool = default"),
              std::string::npos);
    EXPECT_NE(output.source.find("ESettingApplyMode::Deferred"), std::string::npos);
    EXPECT_NE(output.header.find("enum class EGameSettingDevice"), std::string::npos);
    EXPECT_NE(output.source.find("EGameSettingDevice::Controller"), std::string::npos);
    EXPECT_NE(output.source.find("EGameSettingOptionProvider::FrameLimits"), std::string::npos);
    EXPECT_NE(output.source.find("std::get_if<float>"), std::string::npos);
}

} // namespace
} // namespace lispb::codegen
