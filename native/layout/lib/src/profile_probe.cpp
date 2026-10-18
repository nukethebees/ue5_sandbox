#include <ioj/layout/profile_probe.hpp>

#include <ioj/format_writer.h>
#include <ioj/layout/abi_profile.hpp>
#include <ioj/layout/external_dependency.hpp>

#include <codegen/ast/indices.h>
#include <codegen/schema.h>
#include <codegen/schema/physical_type_form.h>
#include <codegen/schema/physical_type_use.h>
#include <lispb/compiler_storage.h>
#include <lispb/schema/external_types.h>
#include <lispb/schema/indices.h>
#include <lispb/schema/type_graph.h>

#include <algorithm>
#include <cstdint>
#include <expected>
#include <iterator>
#include <limits>
#include <optional>
#include <ranges>
#include <set>
#include <span>
#include <string>
#include <string_view>

namespace ioj::layout {
namespace {

auto probe_error(lispb::codegen::CppTypes const& types,
                 std::string const& spelling,
                 lispb::codegen::PhysicalTypeForm const form,
                 lispb::StringId const object_spelling) -> std::optional<std::string> {
    if ((form != lispb::codegen::PhysicalTypeForm::value &&
         form != lispb::codegen::PhysicalTypeForm::object_pointer) ||
        (form == lispb::codegen::PhysicalTypeForm::value &&
         types.strings().view(object_spelling) == "void") ||
        types.strings().view(object_spelling) == "auto" ||
        spelling.find_first_of(";{}\r\n\"#") != std::string::npos) {
        return "Cannot probe unsupported complete-object spelling: " + spelling;
    }
    return std::nullopt;
}

void append_quoted(std::string& output, std::string_view const value) {
    output += '"';
    for (auto const character : value) {
        if (character == '"' || character == '\\') {
            output += '\\';
        }
        output += character;
    }
    output += '"';
}

} // namespace

auto external_probe_request(lispb::schema::TypeGraph const& types, lispb::schema::TypeId selected)
    -> ExternalProbeRequest {
    ExternalProbeRequest result;
    auto const& node{selected};
    auto const external{
        ((types.definition(node)).kind() == lispb::schema::TypeDefinitionKind::external
             ? std::optional{types.definition(node)}
             : std::nullopt)};
    if (!external) {
        result.diagnostics.push_back("Compiler probe selection must be an external type.");
        return result;
    }
    std::set<std::string> spellings;
    std::set<std::string> headers;
    std::set<std::string> diagnostics;
    auto const collect_headers{[&](auto const dependencies) -> void {
        for (auto const& dependency : dependencies) {
            if (auto const header{types.cpp_types().dependency_header(dependency)}) {
                headers.emplace(*header);
            }
        }
    }};
    auto const collect{[&](lispb::codegen::CppTypeId cpp_type,
                           lispb::codegen::PhysicalTypeForm const form,
                           lispb::StringId const object_spelling) {
        auto const spelling{std::string{types.cpp_types().spelling(cpp_type)}};
        if (auto const error{probe_error(types.cpp_types(), spelling, form, object_spelling)}) {
            diagnostics.insert(*error);
        } else {
            spellings.insert(lispb::codegen::native_spelling(spelling));
            collect_headers(types.cpp_types().dependencies(cpp_type));
        }
    }};
    collect_headers(types.cpp_types().dependencies(
        types.definitions().externals.cpp_type[types.definitions().externals.index(*external)]));
    auto const cpp_type{
        types.definitions().externals.cpp_type[types.definitions().externals.index(*external)]};
    auto const physical{lispb::codegen::classify_physical_type_use(types.cpp_types(), cpp_type)};
    collect(cpp_type, physical.form, physical.object_spelling);
    for (auto const& dependency : external_dependencies(types)) {
        if (std::ranges::find(dependency.types, selected) == dependency.types.end()) {
            continue;
        }
        for (auto const type : dependency.types) {
            auto const& grouped{types.definition(type)};
            collect_headers(types.cpp_types().dependencies(
                types.definitions()
                    .externals.cpp_type[types.definitions().externals.index(grouped)]));
        }
        for (auto const use : dependency.uses) {
            auto const row{types.occurrences().index(types.target(use))};
            collect(types.occurrences().cpp_type[row],
                    types.occurrences().physical_form[row],
                    types.occurrences().object_spelling[row]);
        }
        break;
    }
    result.spellings.assign(spellings.begin(), spellings.end());
    result.headers.assign(headers.begin(), headers.end());
    result.diagnostics.assign(diagnostics.begin(), diagnostics.end());
    return result;
}

auto profile_probe_source(std::span<std::string const> types, std::span<std::string const> headers)
    -> std::expected<std::string, std::string> {
    std::string source;
    auto write{ioj::FormatWriter{std::back_inserter(source)}};
    source += "// Compile with the target SDK, flags and defines. Run: probe platform architecture "
              "configuration > target.profile\n"
              "#include <cstdint>\n#include <iomanip>\n#include <iostream>\n#include "
              "<limits>\n#include <type_traits>\n";
    std::set<std::string> included;
    for (auto const& header : headers) {
        if (header.empty() || header.find_first_of("\"<>\r\n") != std::string::npos) {
            return std::unexpected{
                "Probe headers must be plain include paths without quotes or line breaks."};
        }
        if (included.insert(header).second) {
            source += "#include ";
            append_quoted(source, header);
            source += '\n';
        }
    }
    source += R"cpp(
template <typename T> void emit_type(char const* spelling) {
    std::cout << "type " << std::quoted(spelling) << ' ' << sizeof(T) << ' ' << alignof(T) << ' ';
    if constexpr (std::is_integral_v<T>) {
        std::cout << (std::is_signed_v<T> ? "signed" : "unsigned") << ' ';
        if constexpr (std::is_unsigned_v<T>) { std::cout << std::numeric_limits<T>::digits; }
        else { std::cout << "unknown"; }
    } else { std::cout << "non-integer unknown"; }
    std::cout << " \"sizeof/alignof; built " __DATE__ " " __TIME__ "\" compiler-probe\n";
}
int main(int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "Usage: probe platform architecture build-configuration > target.profile\n";
        return 2;
    }
    std::cout << "ioj-layout-profile 2\nname \"Exported compiler probe\"\n";
    std::cout << "identity platform " << std::quoted(argv[1]) << '\n';
    std::cout << "identity architecture " << std::quoted(argv[2]) << '\n';
    std::cout << "identity build-configuration " << std::quoted(argv[3]) << '\n';
#if defined(__clang__)
    std::cout << "identity compiler " << std::quoted("Clang " __clang_version__) << '\n';
#elif defined(_MSC_VER)
    std::cout << "identity compiler \"MSVC " << _MSC_VER << "\"\n";
#elif defined(__VERSION__)
    std::cout << "identity compiler " << std::quoted(__VERSION__) << '\n';
#endif
)cpp";
    std::set<std::string> measured{"bool",
                                   "char",
                                   "short",
                                   "int",
                                   "long",
                                   "long long",
                                   "unsigned char",
                                   "unsigned short",
                                   "unsigned int",
                                   "unsigned long",
                                   "unsigned long long",
                                   "float",
                                   "double",
                                   "std::int8_t",
                                   "std::int16_t",
                                   "std::int32_t",
                                   "std::int64_t",
                                   "std::uint8_t",
                                   "std::uint16_t",
                                   "std::uint32_t",
                                   "std::uint64_t",
                                   "void*"};
    measured.insert(types.begin(), types.end());
    lispb::CompilerStorage storage;
    for (auto const& spelling : measured) {
        auto const type{storage.cpp_types.intern(spelling)};
        auto const use{lispb::codegen::classify_physical_type_use(storage.cpp_types, type)};
        if (auto const error{
                probe_error(storage.cpp_types, spelling, use.form, use.object_spelling)}) {
            return std::unexpected{*error};
        }
        write("    emit_type<{}>(", spelling);
        append_quoted(source, spelling);
        source += ");\n";
    }
    source += R"cpp(
#if defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__) || defined(__i386__)
    std::cout << "object-pointers \"void*\"\n";
#endif
    return std::cout ? 0 : 1;
}
)cpp";
    return source;
}

} // namespace ioj::layout
