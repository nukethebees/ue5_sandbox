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
            {[&] {
                auto const new_normal_modules{normal_modules.append()};
                auto const normal_modules_row{normal_modules.index(new_normal_modules)};
                normal_modules.settings[normal_modules_row] = [&] {
                    auto const new_module_settings_store{module_settings_store.append()};
                    auto const module_settings_store_row{
                        module_settings_store.index(new_module_settings_store)};
                    module_settings_store.name[module_settings_store_row] =
                        strings.intern("native_enum");
                    module_settings_store.header[module_settings_store_row] =
                        strings.intern("NativeEnum.h");
                    module_settings_store.namespace_name[module_settings_store_row] =
                        strings.intern("fixture");
                    return new_module_settings_store;
                }();
                normal_modules.declarations[normal_modules_row] =
                    store.range<DeclarationHandle>({[&] {
                        auto const new_enums{enums.append()};
                        auto const enums_row{enums.index(new_enums)};
                        enums.name[enums_row] = strings.intern("NativeState");
                        enums.underlying_type[enums_row] = [&] {
                            auto const new_type_refs{type_refs.append()};
                            auto const type_refs_row{type_refs.index(new_type_refs)};
                            type_refs.name[type_refs_row] = strings.intern("@native_uint8");
                            return new_type_refs;
                        }();
                        enums.values[enums_row] = store.range<EnumeratorId>(
                            {[&] {
                                 auto const new_enumerators{enumerators.append()};
                                 auto const enumerators_row{enumerators.index(new_enumerators)};
                                 enumerators.name[enumerators_row] = strings.intern("Idle");
                                 enumerators.initializer[enumerators_row] = strings.intern("0");
                                 enumerators.display_name[enumerators_row] = strings.intern("Idle");
                                 enumerators.serialized_name[enumerators_row] =
                                     strings.intern("idle");
                                 return new_enumerators;
                             }(),
                             [&] {
                                 auto const new_enumerators{enumerators.append()};
                                 auto const enumerators_row{enumerators.index(new_enumerators)};
                                 enumerators.name[enumerators_row] = strings.intern("Active");
                                 enumerators.initializer[enumerators_row] = strings.intern("1");
                                 enumerators.display_name[enumerators_row] =
                                     strings.intern("Active");
                                 enumerators.serialized_name[enumerators_row] =
                                     strings.intern("active");
                                 return new_enumerators;
                             }()});
                        enums.native_api[enums_row] = true;
                        enums.helpers[enums_row] =
                            store.range<NativeEnumHelper>({NativeEnumHelper::to_string,
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
                    manifest_schema_version, make_test_registry(context), store, {[&] {
                        auto const new_normal_modules{normal_modules.append()};
                        auto const normal_modules_row{normal_modules.index(new_normal_modules)};
                        normal_modules.settings[normal_modules_row] = [&] {
                            auto const new_module_settings_store{module_settings_store.append()};
                            auto const module_settings_store_row{
                                module_settings_store.index(new_module_settings_store)};
                            module_settings_store.name[module_settings_store_row] =
                                strings.intern("selected");
                            module_settings_store.header[module_settings_store_row] =
                                strings.intern("Selected.h");
                            return new_module_settings_store;
                        }();
                        normal_modules.declarations[normal_modules_row] =
                            store.range<DeclarationHandle>({[&] {
                                auto const new_enums{enums.append()};
                                auto const enums_row{enums.index(new_enums)};
                                enums.name[enums_row] = strings.intern("State");
                                enums.values[enums_row] = store.range<EnumeratorId>({[&] {
                                    auto const new_enumerators{enumerators.append()};
                                    auto const enumerators_row{enumerators.index(new_enumerators)};
                                    enumerators.name[enumerators_row] = strings.intern("Idle");
                                    enumerators.initializer[enumerators_row] = strings.intern("0");
                                    enumerators.serialized_name[enumerators_row] =
                                        strings.intern("idle");
                                    return new_enumerators;
                                }()});
                                enums.enum_array[enums_row] = true;
                                enums.native_api[enums_row] = true;
                                enums.helpers[enums_row] = store.range<NativeEnumHelper>({helper});
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
    auto& store{context.storage.declarations};
    auto& strings{context.storage.strings};
    auto& enumerators{store.enumerators};
    auto& enums{store.enums};
    auto& module_settings_store{store.module_settings_store};
    auto& normal_modules{store.normal_modules};
    auto const files{render_test_manifest(
        context,
        Manifest{
            manifest_schema_version, make_test_registry(context), store, {[&] {
                auto const new_normal_modules{normal_modules.append()};
                auto const normal_modules_row{normal_modules.index(new_normal_modules)};
                normal_modules.settings[normal_modules_row] = [&] {
                    auto const new_module_settings_store{module_settings_store.append()};
                    auto const module_settings_store_row{
                        module_settings_store.index(new_module_settings_store)};
                    module_settings_store.name[module_settings_store_row] =
                        strings.intern("native_enum");
                    module_settings_store.header[module_settings_store_row] =
                        strings.intern("NativeEnum.h");
                    module_settings_store.namespace_name[module_settings_store_row] =
                        strings.intern("fixture");
                    return new_module_settings_store;
                }();
                normal_modules.declarations[normal_modules_row] =
                    store.range<DeclarationHandle>({[&] {
                        auto const new_enums{enums.append()};
                        auto const enums_row{enums.index(new_enums)};
                        enums.name[enums_row] = strings.intern("SignedState");
                        enums.underlying_type[enums_row] = std::nullopt;
                        enums.values[enums_row] = store.range<EnumeratorId>(
                            {[&] {
                                 auto const new_enumerators{enumerators.append()};
                                 auto const enumerators_row{enumerators.index(new_enumerators)};
                                 enumerators.name[enumerators_row] = strings.intern("Below");
                                 enumerators.initializer[enumerators_row] = strings.intern("-1");
                                 return new_enumerators;
                             }(),
                             [&] {
                                 auto const new_enumerators{enumerators.append()};
                                 auto const enumerators_row{enumerators.index(new_enumerators)};
                                 enumerators.name[enumerators_row] = strings.intern("Above");
                                 enumerators.initializer[enumerators_row] = strings.intern("1");
                                 return new_enumerators;
                             }()});
                        enums.bit_width[enums_row] = 12;
                        enums.signedness[enums_row] = lispb::schema::EnumSignedness::signed_;
                        enums.native_api[enums_row] = true;
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
    auto& store{context.storage.declarations};
    auto& strings{context.storage.strings};
    auto& enumerators{store.enumerators};
    auto& enums{store.enums};
    auto& module_settings_store{store.module_settings_store};
    auto& normal_modules{store.normal_modules};
    auto const output{render_enum(context, [&] {
        auto const new_normal_modules{normal_modules.append()};
        auto const normal_modules_row{normal_modules.index(new_normal_modules)};
        normal_modules.settings[normal_modules_row] = [&] {
            auto const new_module_settings_store{module_settings_store.append()};
            auto const module_settings_store_row{
                module_settings_store.index(new_module_settings_store)};
            module_settings_store.name[module_settings_store_row] = strings.intern("states");
            module_settings_store.header[module_settings_store_row] = strings.intern("States.h");
            module_settings_store.source[module_settings_store_row] = strings.intern("States.cpp");
            return new_module_settings_store;
        }();
        normal_modules.declarations[normal_modules_row] = store.range<DeclarationHandle>({[&] {
            auto const new_enums{enums.append()};
            auto const enums_row{enums.index(new_enums)};
            enums.name[enums_row] = strings.intern("EState");
            enums.underlying_type[enums_row] = std::nullopt;
            enums.values[enums_row] = store.range<EnumeratorId>(
                {[&] {
                     auto const new_enumerators{enumerators.append()};
                     auto const enumerators_row{enumerators.index(new_enumerators)};
                     enumerators.name[enumerators_row] = strings.intern("Idle");
                     enumerators.initializer[enumerators_row] = strings.intern("0");
                     return new_enumerators;
                 }(),
                 [&] {
                     auto const new_enumerators{enumerators.append()};
                     auto const enumerators_row{enumerators.index(new_enumerators)};
                     enumerators.name[enumerators_row] = strings.intern("Maximum");
                     enumerators.initializer[enumerators_row] = strings.intern("4095");
                     return new_enumerators;
                 }()});
            enums.bit_width[enums_row] = 12;
            enums.signedness[enums_row] = lispb::schema::EnumSignedness::unsigned_;
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
    auto& store{context.storage.declarations};
    auto& strings{context.storage.strings};
    auto& integer_scalars{store.integer_scalars};
    auto& module_settings_store{store.module_settings_store};
    auto& normal_modules{store.normal_modules};
    auto& packed_named_codes{store.packed_named_codes};
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
                      {context.storage.cpp_types.dependency("std::uint8_t", "cstdint")})},
                 {"native_int64",
                  context.storage.cpp_types.add(
                      "std::int64_t",
                      {context.storage.cpp_types.dependency("std::int64_t", "cstdint")})},
                 {"native_uint64",
                  context.storage.cpp_types.add(
                      "std::uint64_t",
                      {context.storage.cpp_types.dependency("std::uint64_t", "cstdint")})}}),
            store,
            {[&] {
                auto const new_normal_modules{normal_modules.append()};
                auto const normal_modules_row{normal_modules.index(new_normal_modules)};
                normal_modules.settings[normal_modules_row] = [&] {
                    auto const new_module_settings_store{module_settings_store.append()};
                    auto const module_settings_store_row{
                        module_settings_store.index(new_module_settings_store)};
                    module_settings_store.name[module_settings_store_row] =
                        strings.intern("semantic_values");
                    module_settings_store.header[module_settings_store_row] =
                        strings.intern("SemanticValues.h");
                    module_settings_store.namespace_name[module_settings_store_row] =
                        strings.intern("fixture");
                    return new_module_settings_store;
                }();
                normal_modules.declarations[normal_modules_row] = store.range<DeclarationHandle>(
                    {[&] {
                         auto const new_integer_scalars{integer_scalars.append()};
                         auto const integer_scalars_row{integer_scalars.index(new_integer_scalars)};
                         integer_scalars.name[integer_scalars_row] = strings.intern("NoOutput");
                         integer_scalars.signedness[integer_scalars_row] = false;
                         integer_scalars.minimum_value[integer_scalars_row] = 0;
                         integer_scalars.maximum_value[integer_scalars_row] = 1;
                         integer_scalars.bit_width[integer_scalars_row] = std::nullopt;
                         integer_scalars.named_codes[integer_scalars_row] =
                             store.range<PackedNamedCodeId>({[&] {
                                 auto const new_packed_named_codes{packed_named_codes.append()};
                                 auto const packed_named_codes_row{
                                     packed_named_codes.index(new_packed_named_codes)};
                                 packed_named_codes.name[packed_named_codes_row] =
                                     strings.intern("Zero");
                                 packed_named_codes.value[packed_named_codes_row] = 0;
                                 packed_named_codes.sentinel[packed_named_codes_row] = false;
                                 return new_packed_named_codes;
                             }()});
                         return new_integer_scalars;
                     }(),
                     [&] {
                         auto const new_integer_scalars{integer_scalars.append()};
                         auto const integer_scalars_row{integer_scalars.index(new_integer_scalars)};
                         integer_scalars.name[integer_scalars_row] = strings.intern("DamageReason");
                         integer_scalars.signedness[integer_scalars_row] = false;
                         integer_scalars.minimum_value[integer_scalars_row] = 0;
                         integer_scalars.maximum_value[integer_scalars_row] = 10;
                         integer_scalars.bit_width[integer_scalars_row] = std::nullopt;
                         integer_scalars.named_codes[integer_scalars_row] =
                             store.range<PackedNamedCodeId>(
                                 {[&] {
                                      auto const new_packed_named_codes{
                                          packed_named_codes.append()};
                                      auto const packed_named_codes_row{
                                          packed_named_codes.index(new_packed_named_codes)};
                                      packed_named_codes.name[packed_named_codes_row] =
                                          strings.intern("Unknown");
                                      packed_named_codes.value[packed_named_codes_row] = 0;
                                      packed_named_codes.sentinel[packed_named_codes_row] = false;
                                      return new_packed_named_codes;
                                  }(),
                                  [&] {
                                      auto const new_packed_named_codes{
                                          packed_named_codes.append()};
                                      auto const packed_named_codes_row{
                                          packed_named_codes.index(new_packed_named_codes)};
                                      packed_named_codes.name[packed_named_codes_row] =
                                          strings.intern("Invalid");
                                      packed_named_codes.value[packed_named_codes_row] = 255;
                                      packed_named_codes.sentinel[packed_named_codes_row] = true;
                                      return new_packed_named_codes;
                                  }()});
                         integer_scalars.cpp_emission[integer_scalars_row] =
                             IntegerScalarCppEmission::constants_with_names;
                         integer_scalars.cpp_type[integer_scalars_row] = [&] {
                             auto const new_type_refs{type_refs.append()};
                             auto const type_refs_row{type_refs.index(new_type_refs)};
                             type_refs.name[type_refs_row] = strings.intern("@native_uint8");
                             return new_type_refs;
                         }();
                         return new_integer_scalars;
                     }(),
                     [&] {
                         auto const new_integer_scalars{integer_scalars.append()};
                         auto const integer_scalars_row{integer_scalars.index(new_integer_scalars)};
                         integer_scalars.name[integer_scalars_row] = strings.intern("SignedLimit");
                         integer_scalars.signedness[integer_scalars_row] = true;
                         integer_scalars.minimum_value[integer_scalars_row] =
                             PackedIntegerValue::from_parts(true, std::uint64_t{1} << 63);
                         integer_scalars.maximum_value[integer_scalars_row] = 0;
                         integer_scalars.bit_width[integer_scalars_row] = 64;
                         integer_scalars.named_codes[integer_scalars_row] =
                             store.range<PackedNamedCodeId>({[&] {
                                 auto const new_packed_named_codes{packed_named_codes.append()};
                                 auto const packed_named_codes_row{
                                     packed_named_codes.index(new_packed_named_codes)};
                                 packed_named_codes.name[packed_named_codes_row] =
                                     strings.intern("Minimum");
                                 packed_named_codes.value[packed_named_codes_row] =
                                     PackedIntegerValue::from_parts(true, std::uint64_t{1} << 63);
                                 packed_named_codes.sentinel[packed_named_codes_row] = false;
                                 return new_packed_named_codes;
                             }()});
                         integer_scalars.cpp_emission[integer_scalars_row] =
                             IntegerScalarCppEmission::constants;
                         integer_scalars.cpp_type[integer_scalars_row] = [&] {
                             auto const new_type_refs{type_refs.append()};
                             auto const type_refs_row{type_refs.index(new_type_refs)};
                             type_refs.name[type_refs_row] = strings.intern("@native_int64");
                             return new_type_refs;
                         }();
                         return new_integer_scalars;
                     }(),
                     [&] {
                         auto const new_integer_scalars{integer_scalars.append()};
                         auto const integer_scalars_row{integer_scalars.index(new_integer_scalars)};
                         integer_scalars.name[integer_scalars_row] =
                             strings.intern("UnsignedLimit");
                         integer_scalars.signedness[integer_scalars_row] = false;
                         integer_scalars.minimum_value[integer_scalars_row] = 0;
                         integer_scalars.maximum_value[integer_scalars_row] =
                             (std::numeric_limits<std::uint64_t>::max)();
                         integer_scalars.bit_width[integer_scalars_row] = 64;
                         integer_scalars.named_codes[integer_scalars_row] =
                             store.range<PackedNamedCodeId>({[&] {
                                 auto const new_packed_named_codes{packed_named_codes.append()};
                                 auto const packed_named_codes_row{
                                     packed_named_codes.index(new_packed_named_codes)};
                                 packed_named_codes.name[packed_named_codes_row] =
                                     strings.intern("Maximum");
                                 packed_named_codes.value[packed_named_codes_row] =
                                     (std::numeric_limits<std::uint64_t>::max)();
                                 packed_named_codes.sentinel[packed_named_codes_row] = false;
                                 return new_packed_named_codes;
                             }()});
                         integer_scalars.cpp_emission[integer_scalars_row] =
                             IntegerScalarCppEmission::constants;
                         integer_scalars.cpp_type[integer_scalars_row] = [&] {
                             auto const new_type_refs{type_refs.append()};
                             auto const type_refs_row{type_refs.index(new_type_refs)};
                             type_refs.name[type_refs_row] = strings.intern("@native_uint64");
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
            {[&] {
                auto const new_normal_modules{normal_modules.append()};
                auto const normal_modules_row{normal_modules.index(new_normal_modules)};
                normal_modules.settings[normal_modules_row] = [&] {
                    auto const new_module_settings_store{module_settings_store.append()};
                    auto const module_settings_store_row{
                        module_settings_store.index(new_module_settings_store)};
                    module_settings_store.name[module_settings_store_row] =
                        strings.intern("native_enum");
                    module_settings_store.header[module_settings_store_row] =
                        strings.intern("NativeEnum.h");
                    module_settings_store.namespace_name[module_settings_store_row] =
                        strings.intern("fixture");
                    return new_module_settings_store;
                }();
                normal_modules
                    .declarations[normal_modules_row] = store.range<DeclarationHandle>({[&] {
                    auto const new_enums{enums.append()};
                    auto const enums_row{enums.index(new_enums)};
                    enums.name[enums_row] = strings.intern("NativeState");
                    enums.underlying_type[enums_row] = [&] {
                        auto const new_type_refs{type_refs.append()};
                        auto const type_refs_row{type_refs.index(new_type_refs)};
                        type_refs.name[type_refs_row] = strings.intern("@native_uint8");
                        return new_type_refs;
                    }();
                    enums.values[enums_row] = store.range<EnumeratorId>(
                        {[&] {
                             auto const new_enumerators{enumerators.append()};
                             auto const enumerators_row{enumerators.index(new_enumerators)};
                             enumerators.name[enumerators_row] = strings.intern("Idle");
                             enumerators.initializer[enumerators_row] = strings.intern("0");
                             enumerators.serialized_name[enumerators_row] = strings.intern("idle");
                             return new_enumerators;
                         }(),
                         [&] {
                             auto const new_enumerators{enumerators.append()};
                             auto const enumerators_row{enumerators.index(new_enumerators)};
                             enumerators.name[enumerators_row] = strings.intern("Active");
                             enumerators.initializer[enumerators_row] = strings.intern("1");
                             enumerators.serialized_name[enumerators_row] =
                                 strings.intern("active");
                             return new_enumerators;
                         }()});
                    enums.unreal_projection[enums_row] = [&] {
                        auto const new_enum_unreal_projections{enum_unreal_projections.append()};
                        auto const enum_unreal_projections_row{
                            enum_unreal_projections.index(new_enum_unreal_projections)};
                        enum_unreal_projections.name[enum_unreal_projections_row] =
                            strings.intern("ENativeState");
                        enum_unreal_projections.header[enum_unreal_projections_row] =
                            strings.intern("Project/NativeState.h");
                        enum_unreal_projections.header_include[enum_unreal_projections_row] =
                            strings.intern("Project/NativeState.h");
                        enum_unreal_projections.conversion_header[enum_unreal_projections_row] =
                            strings.intern("Project/NativeStateConversion.h");
                        enum_unreal_projections.native_header_include[enum_unreal_projections_row] =
                            strings.intern("fixture/NativeEnum.h");
                        enum_unreal_projections.enum_array[enum_unreal_projections_row] = true;
                        return new_enum_unreal_projections;
                    }();
                    enums.enum_array[enums_row] = false;
                    enums.native_api[enums_row] = true;
                    enums.helpers[enums_row] =
                        store.range<NativeEnumHelper>({NativeEnumHelper::traits});
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
    auto& store{context.storage.declarations};
    auto& strings{context.storage.strings};
    auto& enumerators{store.enumerators};
    auto& enums{store.enums};
    auto& module_settings_store{store.module_settings_store};
    auto& normal_modules{store.normal_modules};
    auto& type_refs{store.type_refs};
    auto const output{render_enum(context, [&] {
        auto const new_normal_modules{normal_modules.append()};
        auto const normal_modules_row{normal_modules.index(new_normal_modules)};
        normal_modules.settings[normal_modules_row] = [&] {
            auto const new_module_settings_store{module_settings_store.append()};
            auto const module_settings_store_row{
                module_settings_store.index(new_module_settings_store)};
            module_settings_store.name[module_settings_store_row] = strings.intern("modes");
            module_settings_store.header[module_settings_store_row] = strings.intern("Modes.h");
            module_settings_store.source[module_settings_store_row] = strings.intern("Modes.cpp");
            module_settings_store.header_include[module_settings_store_row] =
                strings.intern("Project/Modes.h");
            return new_module_settings_store;
        }();
        normal_modules.declarations[normal_modules_row] = store.range<DeclarationHandle>({[&] {
            auto const new_enums{enums.append()};
            auto const enums_row{enums.index(new_enums)};
            enums.name[enums_row] = strings.intern("EMode");
            enums.underlying_type[enums_row] = [&] {
                auto const new_type_refs{type_refs.append()};
                auto const type_refs_row{type_refs.index(new_type_refs)};
                type_refs.name[type_refs_row] = strings.intern("uint8");
                return new_type_refs;
            }();
            enums.values[enums_row] = store.range<EnumeratorId>(
                {[&] {
                     auto const new_enumerators{enumerators.append()};
                     auto const enumerators_row{enumerators.index(new_enumerators)};
                     enumerators.name[enumerators_row] = strings.intern("First");
                     enumerators.initializer[enumerators_row] = std::nullopt;
                     enumerators.display_name[enumerators_row] = std::nullopt;
                     enumerators.hidden[enumerators_row] = false;
                     enumerators.serialized_name[enumerators_row] = strings.intern("first");
                     return new_enumerators;
                 }(),
                 [&] {
                     auto const new_enumerators{enumerators.append()};
                     auto const enumerators_row{enumerators.index(new_enumerators)};
                     enumerators.name[enumerators_row] = strings.intern("Readable");
                     enumerators.initializer[enumerators_row] = strings.intern("7");
                     enumerators.display_name[enumerators_row] = strings.intern("Readable Value");
                     enumerators.hidden[enumerators_row] = false;
                     enumerators.serialized_name[enumerators_row] = strings.intern("readable");
                     return new_enumerators;
                 }(),
                 [&] {
                     auto const new_enumerators{enumerators.append()};
                     auto const enumerators_row{enumerators.index(new_enumerators)};
                     enumerators.name[enumerators_row] = strings.intern("COUNT");
                     enumerators.initializer[enumerators_row] = std::nullopt;
                     enumerators.display_name[enumerators_row] = std::nullopt;
                     enumerators.hidden[enumerators_row] = true;
                     enumerators.serialized_name[enumerators_row] = strings.intern("count");
                     return new_enumerators;
                 }()});
            enums.conversions[enums_row] =
                store.range<EnumConversion>({EnumConversion::lex_to_string,
                                             EnumConversion::string_view,
                                             EnumConversion::display_string_view,
                                             EnumConversion::lex_to_serialized_string,
                                             EnumConversion::try_parse_serialized});
            enums.export_specifier[enums_row] = strings.intern("PROJECT_API");
            enums.reflection[enums_row] = EnumReflection::blueprint;
            return new_enums;
        }()});
        normal_modules.enum_helper_namespace[normal_modules_row] = strings.intern("project");
        return new_normal_modules;
    }())};

    EXPECT_NE(output.header.find("#include \"Modes.generated.h\""), std::string::npos);
    EXPECT_NE(output.header.find("UENUM(BlueprintType)\nenum class EMode : std::uint8_t"),
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
    auto& store{context.storage.declarations};
    auto& strings{context.storage.strings};
    auto& enumerators{store.enumerators};
    auto& enums{store.enums};
    auto& module_settings_store{store.module_settings_store};
    auto& normal_modules{store.normal_modules};
    auto& type_refs{store.type_refs};
    auto const output{render_enum(context, [&] {
        auto const new_normal_modules{normal_modules.append()};
        auto const normal_modules_row{normal_modules.index(new_normal_modules)};
        normal_modules.settings[normal_modules_row] = [&] {
            auto const new_module_settings_store{module_settings_store.append()};
            auto const module_settings_store_row{
                module_settings_store.index(new_module_settings_store)};
            module_settings_store.name[module_settings_store_row] = strings.intern("states");
            module_settings_store.header[module_settings_store_row] = strings.intern("States.h");
            module_settings_store.source[module_settings_store_row] = strings.intern("States.cpp");
            module_settings_store.namespace_name[module_settings_store_row] =
                strings.intern("project::states");
            return new_module_settings_store;
        }();
        normal_modules.declarations[normal_modules_row] = store.range<DeclarationHandle>({[&] {
            auto const new_enums{enums.append()};
            auto const enums_row{enums.index(new_enums)};
            enums.name[enums_row] = strings.intern("EState");
            enums.underlying_type[enums_row] = [&] {
                auto const new_type_refs{type_refs.append()};
                auto const type_refs_row{type_refs.index(new_type_refs)};
                type_refs.name[type_refs_row] = strings.intern("int");
                return new_type_refs;
            }();
            enums.values[enums_row] = store.range<EnumeratorId>({[&] {
                auto const new_enumerators{enumerators.append()};
                auto const enumerators_row{enumerators.index(new_enumerators)};
                enumerators.name[enumerators_row] = strings.intern("Ready");
                return new_enumerators;
            }()});
            enums.conversions[enums_row] = store.range<EnumConversion>({EnumConversion::string});
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
    auto& store{context.storage.declarations};
    auto& strings{context.storage.strings};
    auto& enumerators{store.enumerators};
    auto& enums{store.enums};
    auto& module_settings_store{store.module_settings_store};
    auto& normal_modules{store.normal_modules};
    auto& type_refs{store.type_refs};
    auto const output{render_enum(context, [&] {
        auto const new_normal_modules{normal_modules.append()};
        auto const normal_modules_row{normal_modules.index(new_normal_modules)};
        normal_modules.settings[normal_modules_row] = [&] {
            auto const new_module_settings_store{module_settings_store.append()};
            auto const module_settings_store_row{
                module_settings_store.index(new_module_settings_store)};
            module_settings_store.name[module_settings_store_row] = strings.intern("modes");
            module_settings_store.header[module_settings_store_row] = strings.intern("Modes.h");
            module_settings_store.source[module_settings_store_row] = strings.intern("Modes.cpp");
            return new_module_settings_store;
        }();
        normal_modules.declarations[normal_modules_row] = store.range<DeclarationHandle>({[&] {
            auto const new_enums{enums.append()};
            auto const enums_row{enums.index(new_enums)};
            enums.name[enums_row] = strings.intern("EMode");
            enums.underlying_type[enums_row] = [&] {
                auto const new_type_refs{type_refs.append()};
                auto const type_refs_row{type_refs.index(new_type_refs)};
                type_refs.name[type_refs_row] = strings.intern("uint8");
                return new_type_refs;
            }();
            enums.values[enums_row] = store.range<EnumeratorId>(
                {[&] {
                     auto const new_enumerators{enumerators.append()};
                     auto const enumerators_row{enumerators.index(new_enumerators)};
                     enumerators.name[enumerators_row] = strings.intern("First");
                     return new_enumerators;
                 }(),
                 [&] {
                     auto const new_enumerators{enumerators.append()};
                     auto const enumerators_row{enumerators.index(new_enumerators)};
                     enumerators.name[enumerators_row] = strings.intern("Second");
                     return new_enumerators;
                 }()});
            enums.enum_array[enums_row] = true;
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
    auto& store{context.storage.declarations};
    auto& strings{context.storage.strings};
    auto& enumerators{store.enumerators};
    auto& enums{store.enums};
    auto& module_settings_store{store.module_settings_store};
    auto& normal_modules{store.normal_modules};
    auto& type_refs{store.type_refs};
    auto const output{render_enum(context, [&] {
        auto const new_normal_modules{normal_modules.append()};
        auto const normal_modules_row{normal_modules.index(new_normal_modules)};
        normal_modules.settings[normal_modules_row] = [&] {
            auto const new_module_settings_store{module_settings_store.append()};
            auto const module_settings_store_row{
                module_settings_store.index(new_module_settings_store)};
            module_settings_store.name[module_settings_store_row] = strings.intern("modes");
            module_settings_store.header[module_settings_store_row] = strings.intern("Modes.h");
            module_settings_store.source[module_settings_store_row] = strings.intern("Modes.cpp");
            return new_module_settings_store;
        }();
        normal_modules.declarations[normal_modules_row] = store.range<DeclarationHandle>({[&] {
            auto const new_enums{enums.append()};
            auto const enums_row{enums.index(new_enums)};
            enums.name[enums_row] = strings.intern("EMode");
            enums.underlying_type[enums_row] = [&] {
                auto const new_type_refs{type_refs.append()};
                auto const type_refs_row{type_refs.index(new_type_refs)};
                type_refs.name[type_refs_row] = strings.intern("uint8");
                return new_type_refs;
            }();
            enums.values[enums_row] = store.range<EnumeratorId>(
                {[&] {
                     auto const new_enumerators{enumerators.append()};
                     auto const enumerators_row{enumerators.index(new_enumerators)};
                     enumerators.name[enumerators_row] = strings.intern("First");
                     return new_enumerators;
                 }(),
                 [&] {
                     auto const new_enumerators{enumerators.append()};
                     auto const enumerators_row{enumerators.index(new_enumerators)};
                     enumerators.name[enumerators_row] = strings.intern("Second");
                     return new_enumerators;
                 }()});
            enums.reflection[enums_row] = EnumReflection::uenum;
            enums.enum_array[enums_row] = true;
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
    auto& store{context.storage.declarations};
    auto& strings{context.storage.strings};
    auto& enumerators{store.enumerators};
    auto& enums{store.enums};
    auto& module_settings_store{store.module_settings_store};
    auto& normal_modules{store.normal_modules};
    auto& type_refs{store.type_refs};
    auto const output{render_enum(context, [&] {
        auto const new_normal_modules{normal_modules.append()};
        auto const normal_modules_row{normal_modules.index(new_normal_modules)};
        normal_modules.settings[normal_modules_row] = [&] {
            auto const new_module_settings_store{module_settings_store.append()};
            auto const module_settings_store_row{
                module_settings_store.index(new_module_settings_store)};
            module_settings_store.name[module_settings_store_row] = strings.intern("states");
            module_settings_store.header[module_settings_store_row] = strings.intern("States.h");
            module_settings_store.source[module_settings_store_row] = strings.intern("States.cpp");
            return new_module_settings_store;
        }();
        normal_modules.declarations[normal_modules_row] = store.range<DeclarationHandle>({[&] {
            auto const new_enums{enums.append()};
            auto const enums_row{enums.index(new_enums)};
            enums.name[enums_row] = strings.intern("EState");
            enums.underlying_type[enums_row] = [&] {
                auto const new_type_refs{type_refs.append()};
                auto const type_refs_row{type_refs.index(new_type_refs)};
                type_refs.name[type_refs_row] = strings.intern("int");
                return new_type_refs;
            }();
            enums.values[enums_row] = store.range<EnumeratorId>({[&] {
                auto const new_enumerators{enumerators.append()};
                auto const enumerators_row{enumerators.index(new_enumerators)};
                enumerators.name[enumerators_row] = strings.intern("Ready");
                return new_enumerators;
            }()});
            enums.conversions[enums_row] =
                store.range<EnumConversion>({EnumConversion::display_string});
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
    auto& store{context.storage.declarations};
    auto& strings{context.storage.strings};
    auto& enumerators{store.enumerators};
    auto& enums{store.enums};
    auto& module_settings_store{store.module_settings_store};
    auto& normal_modules{store.normal_modules};
    auto& type_refs{store.type_refs};
    auto const output{render_enum(context, [&] {
        auto const new_normal_modules{normal_modules.append()};
        auto const normal_modules_row{normal_modules.index(new_normal_modules)};
        normal_modules.settings[normal_modules_row] = [&] {
            auto const new_module_settings_store{module_settings_store.append()};
            auto const module_settings_store_row{
                module_settings_store.index(new_module_settings_store)};
            module_settings_store.name[module_settings_store_row] = strings.intern("states");
            module_settings_store.header[module_settings_store_row] = strings.intern("States.h");
            module_settings_store.source[module_settings_store_row] = strings.intern("States.cpp");
            return new_module_settings_store;
        }();
        normal_modules.declarations[normal_modules_row] = store.range<DeclarationHandle>({[&] {
            auto const new_enums{enums.append()};
            auto const enums_row{enums.index(new_enums)};
            enums.name[enums_row] = strings.intern("EState");
            enums.underlying_type[enums_row] = [&] {
                auto const new_type_refs{type_refs.append()};
                auto const type_refs_row{type_refs.index(new_type_refs)};
                type_refs.name[type_refs_row] = strings.intern("int");
                return new_type_refs;
            }();
            enums.values[enums_row] = store.range<EnumeratorId>(
                {[&] {
                     auto const new_enumerators{enumerators.append()};
                     auto const enumerators_row{enumerators.index(new_enumerators)};
                     enumerators.name[enumerators_row] = strings.intern("Ready");
                     enumerators.initializer[enumerators_row] = std::nullopt;
                     enumerators.display_name[enumerators_row] = std::nullopt;
                     enumerators.hidden[enumerators_row] = false;
                     enumerators.serialized_name[enumerators_row] = strings.intern("ready");
                     return new_enumerators;
                 }(),
                 [&] {
                     auto const new_enumerators{enumerators.append()};
                     auto const enumerators_row{enumerators.index(new_enumerators)};
                     enumerators.name[enumerators_row] = strings.intern("Special");
                     enumerators.initializer[enumerators_row] = std::nullopt;
                     enumerators.display_name[enumerators_row] =
                         strings.intern("Quote\" Slash\\\n\t");
                     enumerators.hidden[enumerators_row] = false;
                     enumerators.serialized_name[enumerators_row] = strings.intern("a\"b\\c\n");
                     return new_enumerators;
                 }()});
            enums.conversions[enums_row] =
                store.range<EnumConversion>({EnumConversion::display_string,
                                             EnumConversion::lex_to_serialized_string,
                                             EnumConversion::try_parse_serialized});
            enums.reflection[enums_row] = EnumReflection::uenum;
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
    auto& store{context.storage.declarations};
    auto& strings{context.storage.strings};
    auto& enumerators{store.enumerators};
    auto& enums{store.enums};
    auto& module_settings_store{store.module_settings_store};
    auto& normal_modules{store.normal_modules};
    auto& type_refs{store.type_refs};
    auto const files{render_test_manifest(
        context,
        Manifest{manifest_schema_version, make_test_registry(context), store, {[&] {
                     auto const new_normal_modules{normal_modules.append()};
                     auto const normal_modules_row{normal_modules.index(new_normal_modules)};
                     normal_modules.settings[normal_modules_row] = [&] {
                         auto const new_module_settings_store{module_settings_store.append()};
                         auto const module_settings_store_row{
                             module_settings_store.index(new_module_settings_store)};
                         module_settings_store.name[module_settings_store_row] =
                             strings.intern("states");
                         module_settings_store.header[module_settings_store_row] =
                             strings.intern("States.h");
                         module_settings_store.namespace_name[module_settings_store_row] =
                             strings.intern("project::states");
                         return new_module_settings_store;
                     }();
                     normal_modules.declarations[normal_modules_row] =
                         store.range<DeclarationHandle>({[&] {
                             auto const new_enums{enums.append()};
                             auto const enums_row{enums.index(new_enums)};
                             enums.name[enums_row] = strings.intern("EState");
                             enums.underlying_type[enums_row] = [&] {
                                 auto const new_type_refs{type_refs.append()};
                                 auto const type_refs_row{type_refs.index(new_type_refs)};
                                 type_refs.name[type_refs_row] = strings.intern("int");
                                 return new_type_refs;
                             }();
                             enums.values[enums_row] = store.range<EnumeratorId>({[&] {
                                 auto const new_enumerators{enumerators.append()};
                                 auto const enumerators_row{enumerators.index(new_enumerators)};
                                 enumerators.name[enumerators_row] = strings.intern("Ready");
                                 return new_enumerators;
                             }()});
                             enums.enum_array[enums_row] = true;
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
    auto const output{render_settings(context, [&] {
        auto const new_settings_modules{settings_modules.append()};
        auto const settings_modules_row{settings_modules.index(new_settings_modules)};
        settings_modules.settings[settings_modules_row] = [&] {
            auto const new_module_settings_store{module_settings_store.append()};
            auto const module_settings_store_row{
                module_settings_store.index(new_module_settings_store)};
            module_settings_store.name[module_settings_store_row] = strings.intern("settings");
            module_settings_store.header[module_settings_store_row] = strings.intern("Settings.h");
            module_settings_store.source[module_settings_store_row] =
                strings.intern("Settings.cpp");
            module_settings_store.namespace_name[module_settings_store_row] =
                strings.intern("game");
            return new_module_settings_store;
        }();
        settings_modules.api_name[settings_modules_row] = strings.intern("TSettingsAccess");
        settings_modules.state_name[settings_modules_row] = strings.intern("FSettingsState");
        settings_modules.categories[settings_modules_row] = store.range<SettingCategoryId>({[&] {
            auto const new_setting_categories{setting_categories.append()};
            auto const setting_categories_row{setting_categories.index(new_setting_categories)};
            setting_categories.name[setting_categories_row] = strings.intern("video");
            setting_categories.label[setting_categories_row] = strings.intern("Video");
            return new_setting_categories;
        }()});
        settings_modules.settings_list[settings_modules_row] = store.range<SettingId>(
            {[&] {
                 auto const new_settings{settings.append()};
                 auto const settings_row{settings.index(new_settings)};
                 settings.name[settings_row] = strings.intern("vsync");
                 settings.label[settings_row] = strings.intern("VSync");
                 settings.category[settings_row] = strings.intern("video");
                 settings.value_type[settings_row] = [&] {
                     auto const new_type_refs{type_refs.append()};
                     auto const type_refs_row{type_refs.index(new_type_refs)};
                     type_refs.name[type_refs_row] = strings.intern("bool");
                     return new_type_refs;
                 }();
                 settings.backend[settings_row] = strings.intern("engine");
                 settings.apply_mode[settings_row] = SettingApplyMode::deferred;
                 settings.control[settings_row] = [&] {
                     auto const new_setting_controls{setting_controls.append()};
                     auto const setting_controls_row{setting_controls.index(new_setting_controls)};
                     setting_controls.kind[setting_controls_row] = SettingControlKind::toggle;
                     return new_setting_controls;
                 }();
                 return new_settings;
             }(),
             [&] {
                 auto const new_settings{settings.append()};
                 auto const settings_row{settings.index(new_settings)};
                 settings.name[settings_row] = strings.intern("frame_limit");
                 settings.label[settings_row] = strings.intern("Frame Limit");
                 settings.category[settings_row] = strings.intern("video");
                 settings.value_type[settings_row] = [&] {
                     auto const new_type_refs{type_refs.append()};
                     auto const type_refs_row{type_refs.index(new_type_refs)};
                     type_refs.name[type_refs_row] = strings.intern("float");
                     return new_type_refs;
                 }();
                 settings.backend[settings_row] = strings.intern("engine");
                 settings.apply_mode[settings_row] = SettingApplyMode::immediate;
                 settings.device[settings_row] = SettingDevice::controller;
                 settings.control[settings_row] = [&] {
                     auto const new_setting_controls{setting_controls.append()};
                     auto const setting_controls_row{setting_controls.index(new_setting_controls)};
                     setting_controls.kind[setting_controls_row] = SettingControlKind::choice;
                     setting_controls.options_provider[setting_controls_row] =
                         strings.intern("frame_limits");
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
