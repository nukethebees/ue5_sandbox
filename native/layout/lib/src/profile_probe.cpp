#include <ioj/layout/profile_probe.hpp>

#include <codegen/schema/physical_type_use.h>
#include <ioj/layout/abi_profile.hpp>

#include <iomanip>
#include <set>
#include <sstream>

namespace ioj::layout {

auto profile_probe_source(std::span<std::string const> types, std::span<std::string const> headers)
    -> std::expected<std::string, std::string> {
    std::ostringstream source;
    source << "// Compile with the target SDK, flags and defines. Run: probe platform architecture "
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
            source << "#include " << std::quoted(header) << '\n';
        }
    }
    source << R"cpp(
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
    for (auto const& spelling : measured) {
        auto const use{codegen::classify_physical_type_use(spelling)};
        if ((use.form != codegen::PhysicalTypeForm::value &&
             use.form != codegen::PhysicalTypeForm::object_pointer) ||
            spelling == "void" || spelling == "auto" ||
            spelling.find_first_of(";{}\r\n\"#") != std::string::npos) {
            return std::unexpected{"Cannot probe unsupported complete-object spelling: " +
                                   spelling};
        }
        source << "    emit_type<" << spelling << ">(" << std::quoted(spelling) << ");\n";
    }
    source << R"cpp(
#if defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__) || defined(__i386__)
    std::cout << "object-pointers \"void*\"\n";
#endif
    return std::cout ? 0 : 1;
}
)cpp";
    return source.str();
}

} // namespace ioj::layout
