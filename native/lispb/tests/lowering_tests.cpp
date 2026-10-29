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
#include <codegen/schema/function_schemas.h>
#include <codegen/schema/indices.h>
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
#include <codegen/schema/type_refs.h>
#include <lispb/compiler_storage.h>
#include <lispb/indices.h>
#include <lispb/schema/source_renderer.h>
#include <lispb/strings.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <format>
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

auto render_module(CompilerTestContext& context, ModuleHandle module) -> RenderedModule {
    auto const files{render_test_manifest(context,
                                          Manifest{manifest_schema_version,
                                                   make_test_registry(context),
                                                   context.storage.declarations,
                                                   {module}})};
    EXPECT_EQ(files.size(), 2);
    return RenderedModule{files[0].content, files[1].content};
}

TEST(Lowering, PackedEnumOffsetsUseTheSmallestStorageIncludingTheSentinel) {
    for (auto const length : {254, 255, 65534, 65535}) {
        SCOPED_TRACE(length);
        CompilerTestContext context;
        auto const source{
            std::format("(module test :header \"Test.h\" (enum State :native-api true "
                        ":helpers (to-serialized-string) (value Only :serialized-name \"{}\") "
                        "(projections (cpp))))",
                        std::string(length, 'x'))};
        auto const files{render_test_manifest(context, parse_test_manifest(context, source))};
        ASSERT_EQ(files.size(), 1);
        auto const bits{length < 255 ? 8 : length < 65535 ? 16 : 32};
        EXPECT_NE(files.front().content.find(std::format("std::array<std::uint{}_t, 2>", bits)),
                  std::string::npos);
        EXPECT_NE(files.front().content.find(std::format("{{0, {}}}", length + 1)),
                  std::string::npos);
    }
}

TEST(Lowering, EmitsStandaloneNativeEnumApi) {
    CompilerTestContext context;
    auto& store{context.storage.declarations};
    auto& strings{context.storage.strings};
    auto& enumerators{store.enumerators};
    auto& enums{store.enums};
    auto& module_settings_store{store.module_settings_store};
    auto& normal_modules{store.normal_modules};
    auto& type_refs{store.type_refs};
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
            store,
            {append_test_row(normal_modules, [&](auto const new_normal_modules) {
                normal_modules.settings[new_normal_modules] = append_test_row(
                    module_settings_store, [&](auto const new_module_settings_store) {
                        module_settings_store.name[new_module_settings_store] =
                            strings.intern("native_enum");
                        module_settings_store.header[new_module_settings_store] =
                            strings.intern("NativeEnum.h");
                        module_settings_store.namespace_name[new_module_settings_store] =
                            strings.intern("fixture");
                    });
                normal_modules.declarations[new_normal_modules] = store.range<DeclarationHandle>(
                    {append_test_row(enums, [&](auto const new_enums) {
                        enums.cpp_projection[new_enums] = CppProjection{};
                        enums.name[new_enums] = strings.intern("NativeState");
                        enums.underlying_type[new_enums] =
                            append_test_row(type_refs, [&](auto const new_type_refs) {
                                type_refs.name[new_type_refs] = strings.intern("@native_uint8");
                            });
                        enums.values[new_enums] = store.range<EnumeratorId>(
                            {append_test_row(enumerators,
                                             [&](auto const new_enumerators) {
                                                 enumerators.name[new_enumerators] =
                                                     strings.intern("Idle");
                                                 enumerators.initializer[new_enumerators] =
                                                     strings.intern("0");
                                                 enumerators.display_name[new_enumerators] =
                                                     strings.intern("Idle");
                                                 enumerators.serialized_name[new_enumerators] =
                                                     strings.intern("idle");
                                             }),
                             append_test_row(enumerators, [&](auto const new_enumerators) {
                                 enumerators.name[new_enumerators] = strings.intern("Active");
                                 enumerators.initializer[new_enumerators] = strings.intern("1");
                                 enumerators.display_name[new_enumerators] =
                                     strings.intern("Active");
                                 enumerators.serialized_name[new_enumerators] =
                                     strings.intern("active");
                             })});
                        enums.native_api[new_enums] = true;
                        enums.helpers[new_enums] =
                            store.range<NativeEnumHelper>({NativeEnumHelper::to_string,
                                                           NativeEnumHelper::try_parse,
                                                           NativeEnumHelper::to_display_string,
                                                           NativeEnumHelper::to_serialized_string,
                                                           NativeEnumHelper::try_parse_serialized,
                                                           NativeEnumHelper::traits});
                    })});
            })}})};

    ASSERT_EQ(files.size(), 1);
    auto const& header{files.front().content};
    EXPECT_NE(header.find("using NativeStateUnderlying = std::uint8_t;"), std::string::npos);
    EXPECT_NE(header.find("enum class NativeState : NativeStateUnderlying"), std::string::npos);
    EXPECT_NE(header.find("try_parse_native_state"), std::string::npos);
    EXPECT_NE(header.find("try_parse_serialized_native_state"), std::string::npos);
    EXPECT_NE(header.find("EnumTraits<::fixture::NativeState>"), std::string::npos);
    EXPECT_EQ(header.find("CoreMinimal.h"), std::string::npos);

    std::vector<std::pair<NativeEnumHelper, std::string>> const selections{
        {NativeEnumHelper::to_string, "using ioj::to_string;"},
        {NativeEnumHelper::try_parse, "auto try_parse_state("},
        {NativeEnumHelper::to_display_string, "using ioj::to_display_string;"},
        {NativeEnumHelper::to_serialized_string, "using ioj::to_serialized_string;"},
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
                    store,
                    {append_test_row(
                        normal_modules,
                        [&](auto const new_normal_modules) {
                            normal_modules.settings[new_normal_modules] = append_test_row(
                                module_settings_store, [&](auto const new_module_settings_store) {
                                    module_settings_store.name[new_module_settings_store] =
                                        strings.intern("selected");
                                    module_settings_store.header[new_module_settings_store] =
                                        strings.intern("Selected.h");
                                });
                            normal_modules.declarations[new_normal_modules] =
                                store.range<DeclarationHandle>(
                                    {append_test_row(enums, [&](auto const new_enums) {
                                        enums.cpp_projection[new_enums] = CppProjection{};
                                        enums.name[new_enums] = strings.intern("State");
                                        enums.values[new_enums] =
                                            store.range<EnumeratorId>({append_test_row(
                                                enumerators, [&](auto const new_enumerators) {
                                                    enumerators.name[new_enumerators] =
                                                        strings.intern("Idle");
                                                    enumerators.initializer[new_enumerators] =
                                                        strings.intern("0");
                                                    enumerators.serialized_name[new_enumerators] =
                                                        strings.intern("idle");
                                                })});
                                        enums.enum_array[new_enums] = true;
                                        enums.native_api[new_enums] = true;
                                        enums.helpers[new_enums] =
                                            store.range<NativeEnumHelper>({helper});
                                    })});
                        })}})
                .front()
                .content};
        for (auto const& [candidate, candidate_spelling] : selections) {
            EXPECT_EQ(selected.contains(candidate_spelling), candidate == helper)
                << spelling << ": " << candidate_spelling;
        }
        EXPECT_NE(selected.find("enum_count<"), std::string::npos);
        EXPECT_EQ(selected.find("std::array names"), std::string::npos);
        EXPECT_EQ(selected.contains("<string_view>"),
                  helper == NativeEnumHelper::try_parse ||
                      helper == NativeEnumHelper::try_parse_serialized);
        EXPECT_EQ(selected.find("<unordered_map>"), std::string::npos);
    }
}

TEST(Lowering, EnumFormatterOptInPreservesSourceRoundTrip) {
    for (auto const enabled : {false, true}) {
        SCOPED_TRACE(enabled);
        CompilerTestContext context;
        auto const manifest{parse_test_manifest(context,
                                                std::format(R"(
          (module states :header States.h :namespace example
            (enum State std::uint8_t :native-api true :helpers (to-string {})
              (value Ready)
    (projections (cpp)))))",
                                                            enabled ? "formatter" : ""))};
        auto const output{render_test_manifest(context, manifest).front().content};
        EXPECT_EQ(output.contains("ioj/enum_formatter.h"), enabled);
        EXPECT_EQ(output.contains("ioj::EnumFormatter<"), enabled);
        EXPECT_EQ(output.contains("struct formatter<"), enabled);

        auto const source{
            schema::render_module_source(manifest.declarations, manifest.modules.front())};
        CompilerTestContext reparsed;
        auto const round_trip{parse_test_manifest(reparsed, source)};
        EXPECT_EQ(output, render_test_manifest(reparsed, round_trip).front().content);
    }
}

TEST(Lowering, EnumFormatterRequiresNameConversion) {
    CompilerTestContext context;
    auto const manifest{parse_test_manifest(context, R"(
      (module states :header States.h
        (enum State std::uint8_t :native-api true :helpers (formatter)
          (value Ready)
    (projections (cpp)))))")};
    try {
        static_cast<void>(render_test_manifest(context, manifest));
        FAIL() << "Expected an invalid formatter declaration";
    } catch (std::invalid_argument const& error) {
        EXPECT_STREQ(error.what(), "Enum 'State' formatter requires the to-string helper");
    }
}

TEST(Lowering, DerivesNativeEnumBackingFromSemanticWidthAndSignedness) {
    CompilerTestContext context;
    auto& store{context.storage.declarations};
    auto& strings{context.storage.strings};
    auto& enumerators{store.enumerators};
    auto& enums{store.enums};
    auto& module_settings_store{store.module_settings_store};
    auto& normal_modules{store.normal_modules};
    auto const files{render_test_manifest(
        context,
        Manifest{
            manifest_schema_version,
            make_test_registry(context),
            store,
            {append_test_row(normal_modules, [&](auto const new_normal_modules) {
                normal_modules.settings[new_normal_modules] = append_test_row(
                    module_settings_store, [&](auto const new_module_settings_store) {
                        module_settings_store.name[new_module_settings_store] =
                            strings.intern("native_enum");
                        module_settings_store.header[new_module_settings_store] =
                            strings.intern("NativeEnum.h");
                        module_settings_store.namespace_name[new_module_settings_store] =
                            strings.intern("fixture");
                    });
                normal_modules.declarations[new_normal_modules] = store.range<DeclarationHandle>(
                    {append_test_row(enums, [&](auto const new_enums) {
                        enums.cpp_projection[new_enums] = CppProjection{};
                        enums.name[new_enums] = strings.intern("SignedState");
                        enums.underlying_type[new_enums] = std::nullopt;
                        enums.values[new_enums] = store.range<EnumeratorId>(
                            {append_test_row(enumerators,
                                             [&](auto const new_enumerators) {
                                                 enumerators.name[new_enumerators] =
                                                     strings.intern("Below");
                                                 enumerators.initializer[new_enumerators] =
                                                     strings.intern("-1");
                                             }),
                             append_test_row(enumerators, [&](auto const new_enumerators) {
                                 enumerators.name[new_enumerators] = strings.intern("Above");
                                 enumerators.initializer[new_enumerators] = strings.intern("1");
                             })});
                        enums.bit_width[new_enums] = 12;
                        enums.signedness[new_enums] = lispb::schema::EnumSignedness::signed_;
                        enums.native_api[new_enums] = true;
                    })});
            })}})};

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

TEST(Lowering, DerivesFixedWidthEnumBackingWithoutNativeHelpers) {
    CompilerTestContext context;
    auto& store{context.storage.declarations};
    auto& strings{context.storage.strings};
    auto& enumerators{store.enumerators};
    auto& enums{store.enums};
    auto& module_settings_store{store.module_settings_store};
    auto& normal_modules{store.normal_modules};
    auto const output{render_module(
        context, append_test_row(normal_modules, [&](auto const new_normal_modules) {
            normal_modules.settings[new_normal_modules] =
                append_test_row(module_settings_store, [&](auto const new_module_settings_store) {
                    module_settings_store.name[new_module_settings_store] =
                        strings.intern("states");
                    module_settings_store.header[new_module_settings_store] =
                        strings.intern("States.h");
                    module_settings_store.source[new_module_settings_store] =
                        strings.intern("States.cpp");
                });
            normal_modules.declarations[new_normal_modules] =
                store.range<DeclarationHandle>({append_test_row(enums, [&](auto const new_enums) {
                    enums.cpp_projection[new_enums] = CppProjection{};
                    enums.name[new_enums] = strings.intern("EState");
                    enums.underlying_type[new_enums] = std::nullopt;
                    enums.values[new_enums] = store.range<EnumeratorId>(
                        {append_test_row(enumerators,
                                         [&](auto const new_enumerators) {
                                             enumerators.name[new_enumerators] =
                                                 strings.intern("Idle");
                                             enumerators.initializer[new_enumerators] =
                                                 strings.intern("0");
                                         }),
                         append_test_row(enumerators, [&](auto const new_enumerators) {
                             enumerators.name[new_enumerators] = strings.intern("Maximum");
                             enumerators.initializer[new_enumerators] = strings.intern("4095");
                         })});
                    enums.bit_width[new_enums] = 12;
                    enums.signedness[new_enums] = lispb::schema::EnumSignedness::unsigned_;
                })});
        }))};

    EXPECT_NE(output.header.find("#include <cstdint>"), std::string::npos);
    EXPECT_EQ(output.header.find("#include \"CoreMinimal.h\""), std::string::npos);
    EXPECT_NE(output.header.find("using EStateUnderlying = std::uint16_t;"), std::string::npos);
    EXPECT_NE(output.header.find("enum class EState : EStateUnderlying"), std::string::npos);
}

TEST(Lowering, EmitsOwningIntegerScalarWithInferredStorage) {
    CompilerTestContext context;
    auto const files{render_test_manifest(context, parse_test_manifest(context, R"(
      (module values :header "Values.h"
        (integer-scalar Index :bit-width 24 (projections (cpp)))
        (integer-scalar Logical :bit-width 8))
    )"))};
    ASSERT_EQ(files.size(), 1U);
    auto const& output{files.front().content};
    EXPECT_NE(output.find("PackedValueBase<Index, ioj::PackedStorage<std::uint32_t, 24>"),
              std::string::npos);
    EXPECT_EQ(output.find("using storage_type"), std::string::npos);
    EXPECT_NE(output.find("using value_type = std::uint32_t;"), std::string::npos);
    EXPECT_EQ(output.find("raw_value() const"), std::string::npos);
    EXPECT_EQ(output.find("from_raw(storage_type"), std::string::npos);
    EXPECT_NE(output.find("set_value("), std::string::npos);
    EXPECT_EQ(output.find("struct Logical"), std::string::npos);
}

TEST(Lowering, EmitsNativeEnumUnrealProjectionWithExplicitNumericCompatibility) {
    CompilerTestContext context;
    auto& store{context.storage.declarations};
    auto& strings{context.storage.strings};
    auto& enum_unreal_projections{store.enum_unreal_projections};
    auto& enumerators{store.enumerators};
    auto& enums{store.enums};
    auto& module_settings_store{store.module_settings_store};
    auto& normal_modules{store.normal_modules};
    auto& type_refs{store.type_refs};
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
            store,
            {append_test_row(normal_modules, [&](auto const new_normal_modules) {
                normal_modules.settings[new_normal_modules] = append_test_row(
                    module_settings_store, [&](auto const new_module_settings_store) {
                        module_settings_store.name[new_module_settings_store] =
                            strings.intern("native_enum");
                        module_settings_store.header[new_module_settings_store] =
                            strings.intern("NativeEnum.h");
                        module_settings_store.namespace_name[new_module_settings_store] =
                            strings.intern("fixture");
                    });
                normal_modules.declarations[new_normal_modules] = store.range<
                    DeclarationHandle>({append_test_row(enums, [&](auto const new_enums) {
                    enums.cpp_projection[new_enums] = CppProjection{};
                    enums.name[new_enums] = strings.intern("NativeState");
                    enums.underlying_type[new_enums] =
                        append_test_row(type_refs, [&](auto const new_type_refs) {
                            type_refs.name[new_type_refs] = strings.intern("@native_uint8");
                        });
                    enums.values[new_enums] = store.range<EnumeratorId>(
                        {append_test_row(enumerators,
                                         [&](auto const new_enumerators) {
                                             enumerators.name[new_enumerators] =
                                                 strings.intern("Idle");
                                             enumerators.initializer[new_enumerators] =
                                                 strings.intern("0");
                                             enumerators.serialized_name[new_enumerators] =
                                                 strings.intern("idle");
                                         }),
                         append_test_row(enumerators, [&](auto const new_enumerators) {
                             enumerators.name[new_enumerators] = strings.intern("Active");
                             enumerators.initializer[new_enumerators] = strings.intern("1");
                             enumerators.serialized_name[new_enumerators] =
                                 strings.intern("active");
                         })});
                    enums.unreal_projection[new_enums] = append_test_row(
                        enum_unreal_projections, [&](auto const new_enum_unreal_projections) {
                            enum_unreal_projections.name[new_enum_unreal_projections] =
                                strings.intern("ENativeState");
                            enum_unreal_projections.header[new_enum_unreal_projections] =
                                strings.intern("Project/NativeState.h");
                            enum_unreal_projections.header_include[new_enum_unreal_projections] =
                                strings.intern("Project/NativeState.h");
                            enum_unreal_projections.conversion_header[new_enum_unreal_projections] =
                                strings.intern("Project/NativeStateConversion.h");
                            enum_unreal_projections
                                .native_header_include[new_enum_unreal_projections] =
                                strings.intern("fixture/NativeEnum.h");
                            enum_unreal_projections.enum_array[new_enum_unreal_projections] = true;
                        });
                    enums.enum_array[new_enums] = false;
                    enums.native_api[new_enums] = true;
                    enums.helpers[new_enums] =
                        store.range<NativeEnumHelper>({NativeEnumHelper::traits});
                })});
            })}})};

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
    auto& store{context.storage.declarations};
    auto& strings{context.storage.strings};
    auto& enumerators{store.enumerators};
    auto& enums{store.enums};
    auto& module_settings_store{store.module_settings_store};
    auto& normal_modules{store.normal_modules};
    auto& type_refs{store.type_refs};
    auto const output{render_module(
        context, append_test_row(normal_modules, [&](auto const new_normal_modules) {
            normal_modules.settings[new_normal_modules] =
                append_test_row(module_settings_store, [&](auto const new_module_settings_store) {
                    module_settings_store.name[new_module_settings_store] = strings.intern("modes");
                    module_settings_store.header[new_module_settings_store] =
                        strings.intern("Modes.h");
                    module_settings_store.source[new_module_settings_store] =
                        strings.intern("Modes.cpp");
                    module_settings_store.header_include[new_module_settings_store] =
                        strings.intern("Project/Modes.h");
                });
            normal_modules.declarations[new_normal_modules] =
                store.range<DeclarationHandle>({append_test_row(enums, [&](auto const new_enums) {
                    enums.cpp_projection[new_enums] = CppProjection{};
                    enums.name[new_enums] = strings.intern("EMode");
                    enums.underlying_type[new_enums] =
                        append_test_row(type_refs, [&](auto const new_type_refs) {
                            type_refs.name[new_type_refs] = strings.intern("uint8");
                        });
                    enums.values[new_enums] = store.range<EnumeratorId>(
                        {append_test_row(enumerators,
                                         [&](auto const new_enumerators) {
                                             enumerators.name[new_enumerators] =
                                                 strings.intern("First");
                                             enumerators.initializer[new_enumerators] =
                                                 std::nullopt;
                                             enumerators.display_name[new_enumerators] =
                                                 std::nullopt;
                                             enumerators.hidden[new_enumerators] = false;
                                             enumerators.serialized_name[new_enumerators] =
                                                 strings.intern("first");
                                         }),
                         append_test_row(enumerators,
                                         [&](auto const new_enumerators) {
                                             enumerators.name[new_enumerators] =
                                                 strings.intern("Readable");
                                             enumerators.initializer[new_enumerators] =
                                                 strings.intern("1");
                                             enumerators.display_name[new_enumerators] =
                                                 strings.intern("Readable Value");
                                             enumerators.hidden[new_enumerators] = false;
                                             enumerators.serialized_name[new_enumerators] =
                                                 strings.intern("readable");
                                         }),
                         append_test_row(enumerators, [&](auto const new_enumerators) {
                             enumerators.name[new_enumerators] = strings.intern("COUNT");
                             enumerators.initializer[new_enumerators] = std::nullopt;
                             enumerators.display_name[new_enumerators] = std::nullopt;
                             enumerators.hidden[new_enumerators] = true;
                             enumerators.serialized_name[new_enumerators] = strings.intern("count");
                         })});
                    enums.conversions[new_enums] =
                        store.range<EnumConversion>({EnumConversion::lex_to_string,
                                                     EnumConversion::string_view,
                                                     EnumConversion::display_string_view,
                                                     EnumConversion::lex_to_serialized_string,
                                                     EnumConversion::try_parse_serialized});
                    enums.export_specifier[new_enums] = strings.intern("PROJECT_API");
                    enums.reflection[new_enums] = EnumReflection::blueprint;
                })});
            normal_modules.enum_helper_namespace[new_normal_modules] = strings.intern("project");
        }))};

    EXPECT_NE(output.header.find("#include \"Modes.generated.h\""), std::string::npos);
    EXPECT_NE(output.header.find("UENUM(BlueprintType)\nenum class EMode : uint8"),
              std::string::npos);
    EXPECT_NE(output.header.find("Readable = 1 UMETA(DisplayName = \"Readable Value\")"),
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
    EXPECT_EQ(output.source.find("switch (value)"), std::string::npos);
    EXPECT_NE(output.header.find("ioj::enum_strings<::EMode, ioj::Name>"), std::string::npos);
    EXPECT_NE(output.header.find("Readable Value"), std::string::npos);
    EXPECT_NE(output.header.find("readable"), std::string::npos);
    EXPECT_NE(output.source.find("ioj::enum_string<ioj::Serialized>(value).data()"),
              std::string::npos);
    EXPECT_NE(output.source.find("result = EMode::Readable;"), std::string::npos);
    EXPECT_NE(output.source.find("auto LexToString(EMode const value) -> TCHAR const*"),
              std::string::npos);
}

TEST(Lowering, EmitsPlainEnumsInTheirNamespace) {
    CompilerTestContext context;
    auto& store{context.storage.declarations};
    auto& strings{context.storage.strings};
    auto& enumerators{store.enumerators};
    auto& enums{store.enums};
    auto& module_settings_store{store.module_settings_store};
    auto& normal_modules{store.normal_modules};
    auto& type_refs{store.type_refs};
    auto const output{render_module(
        context, append_test_row(normal_modules, [&](auto const new_normal_modules) {
            normal_modules.settings[new_normal_modules] =
                append_test_row(module_settings_store, [&](auto const new_module_settings_store) {
                    module_settings_store.name[new_module_settings_store] =
                        strings.intern("states");
                    module_settings_store.header[new_module_settings_store] =
                        strings.intern("States.h");
                    module_settings_store.source[new_module_settings_store] =
                        strings.intern("States.cpp");
                    module_settings_store.namespace_name[new_module_settings_store] =
                        strings.intern("project::states");
                });
            normal_modules.declarations[new_normal_modules] =
                store.range<DeclarationHandle>({append_test_row(enums, [&](auto const new_enums) {
                    enums.cpp_projection[new_enums] = CppProjection{};
                    enums.name[new_enums] = strings.intern("EState");
                    enums.underlying_type[new_enums] =
                        append_test_row(type_refs, [&](auto const new_type_refs) {
                            type_refs.name[new_type_refs] = strings.intern("int");
                        });
                    enums.values[new_enums] = store.range<EnumeratorId>(
                        {append_test_row(enumerators, [&](auto const new_enumerators) {
                            enumerators.name[new_enumerators] = strings.intern("Ready");
                        })});
                    enums.conversions[new_enums] =
                        store.range<EnumConversion>({EnumConversion::string});
                })});
        }))};

    EXPECT_NE(output.header.find("namespace project::states {\nusing EStateUnderlying = int;\n\n"
                                 "// Logical bit width: 1.\nenum class EState : EStateUnderlying"),
              std::string::npos);
    EXPECT_NE(output.header.find("using EStateUnderlying = int;"), std::string::npos);
    EXPECT_NE(output.header.find("namespace project::states {\nauto to_string(EState const value)"),
              std::string::npos);
    EXPECT_EQ(output.header.find("UENUM"), std::string::npos);
    EXPECT_NE(output.header.find("ioj::enum_strings<::project::states::EState, ioj::Name>"),
              std::string::npos);
}

TEST(Lowering, EmitsCountsForEnumArrays) {
    CompilerTestContext context;
    auto& store{context.storage.declarations};
    auto& strings{context.storage.strings};
    auto& enumerators{store.enumerators};
    auto& enums{store.enums};
    auto& module_settings_store{store.module_settings_store};
    auto& normal_modules{store.normal_modules};
    auto& type_refs{store.type_refs};
    auto const output{render_module(
        context, append_test_row(normal_modules, [&](auto const new_normal_modules) {
            normal_modules.settings[new_normal_modules] =
                append_test_row(module_settings_store, [&](auto const new_module_settings_store) {
                    module_settings_store.name[new_module_settings_store] = strings.intern("modes");
                    module_settings_store.header[new_module_settings_store] =
                        strings.intern("Modes.h");
                    module_settings_store.source[new_module_settings_store] =
                        strings.intern("Modes.cpp");
                });
            normal_modules.declarations[new_normal_modules] =
                store.range<DeclarationHandle>({append_test_row(enums, [&](auto const new_enums) {
                    enums.cpp_projection[new_enums] = CppProjection{};
                    enums.name[new_enums] = strings.intern("EMode");
                    enums.underlying_type[new_enums] =
                        append_test_row(type_refs, [&](auto const new_type_refs) {
                            type_refs.name[new_type_refs] = strings.intern("uint8");
                        });
                    enums.values[new_enums] = store.range<EnumeratorId>(
                        {append_test_row(enumerators,
                                         [&](auto const new_enumerators) {
                                             enumerators.name[new_enumerators] =
                                                 strings.intern("First");
                                         }),
                         append_test_row(enumerators, [&](auto const new_enumerators) {
                             enumerators.name[new_enumerators] = strings.intern("Second");
                         })});
                    enums.enum_array[new_enums] = true;
                })});
        }))};

    EXPECT_NE(output.header.find("#include \"ioj/enum_traits.h\""), std::string::npos);
    EXPECT_NE(
        output.header.find(
            "constexpr auto enum_count<::EMode>() noexcept -> std::size_t {\n    return 2;\n}"),
        std::string::npos);
    EXPECT_EQ(output.header.find("COUNT"), std::string::npos);
}

TEST(Lowering, EmitsCountsForReflectedEnumArrays) {
    CompilerTestContext context;
    auto& store{context.storage.declarations};
    auto& strings{context.storage.strings};
    auto& enumerators{store.enumerators};
    auto& enums{store.enums};
    auto& module_settings_store{store.module_settings_store};
    auto& normal_modules{store.normal_modules};
    auto& type_refs{store.type_refs};
    auto const output{render_module(
        context, append_test_row(normal_modules, [&](auto const new_normal_modules) {
            normal_modules.settings[new_normal_modules] =
                append_test_row(module_settings_store, [&](auto const new_module_settings_store) {
                    module_settings_store.name[new_module_settings_store] = strings.intern("modes");
                    module_settings_store.header[new_module_settings_store] =
                        strings.intern("Modes.h");
                    module_settings_store.source[new_module_settings_store] =
                        strings.intern("Modes.cpp");
                });
            normal_modules.declarations[new_normal_modules] =
                store.range<DeclarationHandle>({append_test_row(enums, [&](auto const new_enums) {
                    enums.cpp_projection[new_enums] = CppProjection{};
                    enums.name[new_enums] = strings.intern("EMode");
                    enums.underlying_type[new_enums] =
                        append_test_row(type_refs, [&](auto const new_type_refs) {
                            type_refs.name[new_type_refs] = strings.intern("uint8");
                        });
                    enums.values[new_enums] = store.range<EnumeratorId>(
                        {append_test_row(enumerators,
                                         [&](auto const new_enumerators) {
                                             enumerators.name[new_enumerators] =
                                                 strings.intern("First");
                                         }),
                         append_test_row(enumerators, [&](auto const new_enumerators) {
                             enumerators.name[new_enumerators] = strings.intern("Second");
                         })});
                    enums.reflection[new_enums] = EnumReflection::uenum;
                    enums.enum_array[new_enums] = true;
                })});
        }))};

    EXPECT_NE(output.header.find("#include \"ioj/enum_traits.h\""), std::string::npos);
    EXPECT_NE(
        output.header.find("enum_count<::EMode>() noexcept -> std::size_t {\n    return 2;\n}"),
        std::string::npos);
}

TEST(Lowering, EnumDisplayLookupWithoutOverridesFallsBackDirectly) {
    CompilerTestContext context;
    auto& store{context.storage.declarations};
    auto& strings{context.storage.strings};
    auto& enumerators{store.enumerators};
    auto& enums{store.enums};
    auto& module_settings_store{store.module_settings_store};
    auto& normal_modules{store.normal_modules};
    auto& type_refs{store.type_refs};
    auto const output{render_module(
        context, append_test_row(normal_modules, [&](auto const new_normal_modules) {
            normal_modules.settings[new_normal_modules] =
                append_test_row(module_settings_store, [&](auto const new_module_settings_store) {
                    module_settings_store.name[new_module_settings_store] =
                        strings.intern("states");
                    module_settings_store.header[new_module_settings_store] =
                        strings.intern("States.h");
                    module_settings_store.source[new_module_settings_store] =
                        strings.intern("States.cpp");
                });
            normal_modules.declarations[new_normal_modules] =
                store.range<DeclarationHandle>({append_test_row(enums, [&](auto const new_enums) {
                    enums.cpp_projection[new_enums] = CppProjection{};
                    enums.name[new_enums] = strings.intern("EState");
                    enums.underlying_type[new_enums] =
                        append_test_row(type_refs, [&](auto const new_type_refs) {
                            type_refs.name[new_type_refs] = strings.intern("int");
                        });
                    enums.values[new_enums] = store.range<EnumeratorId>(
                        {append_test_row(enumerators, [&](auto const new_enumerators) {
                            enumerators.name[new_enumerators] = strings.intern("Ready");
                        })});
                    enums.conversions[new_enums] =
                        store.range<EnumConversion>({EnumConversion::display_string});
                })});
        }))};

    EXPECT_NE(output.source.find("return ioj::enum_string<ioj::Display>(value).data();"),
              std::string::npos);
    EXPECT_NE(output.header.find("ioj::enum_strings<::EState, ioj::Display>"), std::string::npos);
    EXPECT_EQ(output.header.find("ioj::enum_strings<::EState, ioj::Name>"), std::string::npos);
    EXPECT_EQ(occurrences(output.source, "switch (value)"), 0);
    EXPECT_NE(output.source.find("return FString{get_state_display_name(value)};"),
              std::string::npos);
}

TEST(Lowering, EnumEscapingAndSparseDisplayCasesPreserveFallbacks) {
    CompilerTestContext context;
    auto& store{context.storage.declarations};
    auto& strings{context.storage.strings};
    auto& enumerators{store.enumerators};
    auto& enums{store.enums};
    auto& module_settings_store{store.module_settings_store};
    auto& normal_modules{store.normal_modules};
    auto& type_refs{store.type_refs};
    auto const output{render_module(
        context, append_test_row(normal_modules, [&](auto const new_normal_modules) {
            normal_modules.settings[new_normal_modules] =
                append_test_row(module_settings_store, [&](auto const new_module_settings_store) {
                    module_settings_store.name[new_module_settings_store] =
                        strings.intern("states");
                    module_settings_store.header[new_module_settings_store] =
                        strings.intern("States.h");
                    module_settings_store.source[new_module_settings_store] =
                        strings.intern("States.cpp");
                });
            normal_modules.declarations[new_normal_modules] =
                store.range<DeclarationHandle>({append_test_row(enums, [&](auto const new_enums) {
                    enums.cpp_projection[new_enums] = CppProjection{};
                    enums.name[new_enums] = strings.intern("EState");
                    enums.underlying_type[new_enums] =
                        append_test_row(type_refs, [&](auto const new_type_refs) {
                            type_refs.name[new_type_refs] = strings.intern("int");
                        });
                    enums.values[new_enums] = store.range<EnumeratorId>(
                        {append_test_row(enumerators,
                                         [&](auto const new_enumerators) {
                                             enumerators.name[new_enumerators] =
                                                 strings.intern("Ready");
                                             enumerators.initializer[new_enumerators] =
                                                 std::nullopt;
                                             enumerators.display_name[new_enumerators] =
                                                 std::nullopt;
                                             enumerators.hidden[new_enumerators] = false;
                                             enumerators.serialized_name[new_enumerators] =
                                                 strings.intern("ready");
                                         }),
                         append_test_row(enumerators, [&](auto const new_enumerators) {
                             enumerators.name[new_enumerators] = strings.intern("Special");
                             enumerators.initializer[new_enumerators] = std::nullopt;
                             enumerators.display_name[new_enumerators] =
                                 strings.intern("Quote\" Slash\\\n\t");
                             enumerators.hidden[new_enumerators] = false;
                             enumerators.serialized_name[new_enumerators] =
                                 strings.intern("a\"b\\c\n");
                         })});
                    enums.conversions[new_enums] =
                        store.range<EnumConversion>({EnumConversion::display_string,
                                                     EnumConversion::lex_to_serialized_string,
                                                     EnumConversion::try_parse_serialized});
                    enums.reflection[new_enums] = EnumReflection::uenum;
                })});
        }))};

    EXPECT_NE(output.header.find("UMETA(DisplayName = \"Quote\\\" Slash\\\\\\n\\t\")"),
              std::string::npos);
    EXPECT_NE(output.header.find("TEXT(\"Ready\\000Quote\\\" Slash\\\\\\n\\t\")"),
              std::string::npos);
    EXPECT_NE(output.source.find("return ioj::enum_string<ioj::Display>(value).data();"),
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
    auto& store{context.storage.declarations};
    auto& strings{context.storage.strings};
    auto& enumerators{store.enumerators};
    auto& enums{store.enums};
    auto& module_settings_store{store.module_settings_store};
    auto& normal_modules{store.normal_modules};
    auto& type_refs{store.type_refs};
    auto const files{render_test_manifest(
        context,
        Manifest{manifest_schema_version,
                 make_test_registry(context),
                 store,
                 {append_test_row(normal_modules, [&](auto const new_normal_modules) {
                     normal_modules.settings[new_normal_modules] = append_test_row(
                         module_settings_store, [&](auto const new_module_settings_store) {
                             module_settings_store.name[new_module_settings_store] =
                                 strings.intern("states");
                             module_settings_store.header[new_module_settings_store] =
                                 strings.intern("States.h");
                             module_settings_store.namespace_name[new_module_settings_store] =
                                 strings.intern("project::states");
                         });
                     normal_modules.declarations[new_normal_modules] =
                         store.range<DeclarationHandle>(
                             {append_test_row(enums, [&](auto const new_enums) {
                                 enums.cpp_projection[new_enums] = CppProjection{};
                                 enums.name[new_enums] = strings.intern("EState");
                                 enums.underlying_type[new_enums] =
                                     append_test_row(type_refs, [&](auto const new_type_refs) {
                                         type_refs.name[new_type_refs] = strings.intern("int");
                                     });
                                 enums.values[new_enums] = store.range<EnumeratorId>(
                                     {append_test_row(enumerators, [&](auto const new_enumerators) {
                                         enumerators.name[new_enumerators] =
                                             strings.intern("Ready");
                                     })});
                                 enums.enum_array[new_enums] = true;
                             })});
                 })}})};
    ASSERT_EQ(files.size(), 1);
    EXPECT_NE(files.front().content.find("namespace ioj {"), std::string::npos);
    EXPECT_NE(
        files.front().content.find(
            "enum_count<::project::states::EState>() noexcept -> std::size_t {\n    return 1;\n}"),
        std::string::npos);
}

TEST(Lowering, GeneratesTypedSettingsApiAndRuntimeDescriptors) {
    CompilerTestContext context;
    auto& store{context.storage.declarations};
    auto& strings{context.storage.strings};
    auto& module_settings_store{store.module_settings_store};
    auto& setting_categories{store.setting_categories};
    auto& setting_controls{store.setting_controls};
    auto& settings{store.settings};
    auto& settings_modules{store.settings_modules};
    auto& type_refs{store.type_refs};
    auto const output{render_module(
        context, append_test_row(settings_modules, [&](auto const new_settings_modules) {
            settings_modules.settings[new_settings_modules] =
                append_test_row(module_settings_store, [&](auto const new_module_settings_store) {
                    module_settings_store.name[new_module_settings_store] =
                        strings.intern("settings");
                    module_settings_store.header[new_module_settings_store] =
                        strings.intern("Settings.h");
                    module_settings_store.source[new_module_settings_store] =
                        strings.intern("Settings.cpp");
                    module_settings_store.namespace_name[new_module_settings_store] =
                        strings.intern("game");
                });
            settings_modules.api_name[new_settings_modules] = strings.intern("TSettingsAccess");
            settings_modules.state_name[new_settings_modules] = strings.intern("FSettingsState");
            settings_modules.categories[new_settings_modules] = store.range<SettingCategoryId>(
                {append_test_row(setting_categories, [&](auto const new_setting_categories) {
                    setting_categories.name[new_setting_categories] = strings.intern("video");
                    setting_categories.label[new_setting_categories] = strings.intern("Video");
                })});
            settings_modules.settings_list[new_settings_modules] = store.range<SettingId>(
                {append_test_row(settings,
                                 [&](auto const new_settings) {
                                     settings.name[new_settings] = strings.intern("vsync");
                                     settings.label[new_settings] = strings.intern("VSync");
                                     settings.category[new_settings] = strings.intern("video");
                                     settings.value_type[new_settings] =
                                         append_test_row(type_refs, [&](auto const new_type_refs) {
                                             type_refs.name[new_type_refs] = strings.intern("bool");
                                         });
                                     settings.backend[new_settings] = strings.intern("engine");
                                     settings.apply_mode[new_settings] = SettingApplyMode::deferred;
                                     settings.control[new_settings] = append_test_row(
                                         setting_controls, [&](auto const new_setting_controls) {
                                             setting_controls.kind[new_setting_controls] =
                                                 SettingControlKind::toggle;
                                         });
                                 }),
                 append_test_row(settings, [&](auto const new_settings) {
                     settings.name[new_settings] = strings.intern("frame_limit");
                     settings.label[new_settings] = strings.intern("Frame Limit");
                     settings.category[new_settings] = strings.intern("video");
                     settings.value_type[new_settings] =
                         append_test_row(type_refs, [&](auto const new_type_refs) {
                             type_refs.name[new_type_refs] = strings.intern("float");
                         });
                     settings.backend[new_settings] = strings.intern("engine");
                     settings.apply_mode[new_settings] = SettingApplyMode::immediate;
                     settings.device[new_settings] = SettingDevice::controller;
                     settings.control[new_settings] =
                         append_test_row(setting_controls, [&](auto const new_setting_controls) {
                             setting_controls.kind[new_setting_controls] =
                                 SettingControlKind::choice;
                             setting_controls.options_provider[new_setting_controls] =
                                 strings.intern("frame_limits");
                         });
                 })});
        }))};

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
