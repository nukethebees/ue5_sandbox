#include "protocol.h"

#include <ioj/ascii.h>

#include <codegen/path_utils.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <istream>
#include <ostream>

namespace lispb::lsp {
auto file_path(std::string_view uri) -> fs::path {
    if (!uri.starts_with("file://")) {
        throw std::invalid_argument{"expected a file URI"};
    }
    uri.remove_prefix(7);
    std::string bytes;
    if (uri.starts_with("localhost/")) {
        uri.remove_prefix(9);
    } else if (!uri.starts_with('/')) {
        bytes = "//";
    }
    while (!uri.empty()) {
        if (uri.front() == '%') {
            unsigned value{};
            if (uri.size() < 3) {
                throw std::invalid_argument{"invalid URI escape"};
            }
            auto const end{uri.data() + 3};
            auto const parsed{std::from_chars(uri.data() + 1, end, value, 16)};
            if (parsed.ec != std::errc{} || parsed.ptr != end || value == 0) {
                throw std::invalid_argument{"invalid URI escape"};
            }
            bytes.push_back(static_cast<char>(value));
            uri.remove_prefix(3);
        } else {
            bytes.push_back(uri.front());
            uri.remove_prefix(1);
        }
    }
#ifdef _WIN32
    if (bytes.size() >= 3 && bytes.front() == '/' && bytes[2] == ':') {
        bytes.erase(0, 1);
        bytes[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(bytes[0])));
    }
#endif
    auto const path{codegen::path_from_utf8(bytes)};
    if (!path.is_absolute()) {
        throw std::invalid_argument{"expected an absolute file URI"};
    }
    return path.lexically_normal();
}
auto file_uri(fs::path const& path) -> std::string {
    auto const bytes{path.generic_u8string()};
    std::string result{"file://"};
    std::u8string_view view{bytes};
    if (view.starts_with(u8"//")) {
        view.remove_prefix(2);
    } else if (!view.starts_with(u8'/')) {
        result += '/';
    }
    for (auto const c : view) {
        if (ioj::isupper(static_cast<char>(c)) || ioj::islower(static_cast<char>(c)) ||
            ioj::isdigit(static_cast<char>(c)) || c == '/' || c == ':' || c == '-' || c == '.' ||
            c == '_' || c == '~') {
            result += static_cast<char>(c);
        } else {
            constexpr std::string_view hex{"0123456789ABCDEF"};
            result += '%';
            result += hex[c >> 4];
            result += hex[c & 15];
        }
    }
    return result;
}
static auto utf8_sequence_size(unsigned char lead) -> unsigned {
    constexpr unsigned ascii_limit{0x80};
    constexpr unsigned three_byte_prefix{0xe0};
    constexpr unsigned four_byte_prefix{0xf0};
    if (lead < ascii_limit) {
        return 1;
    }
    if (lead < three_byte_prefix) {
        return 2;
    }
    return lead < four_byte_prefix ? 3 : 4;
}
auto utf16_length(std::string_view bytes) -> std::uint32_t {
    std::uint32_t length{};
    for (std::size_t i{}; i < bytes.size();) {
        auto const size{utf8_sequence_size(static_cast<unsigned char>(bytes[i]))};
        i += size;
        length += size == 4 ? 2u : 1u;
    }
    return length;
}
auto position(sexpr::Sources const& sources, sexpr::SourceFileId file, std::uint32_t offset)
    -> Json {
    auto const lines{sources.line_offsets(file)};
    auto const line{
        static_cast<std::size_t>(std::ranges::upper_bound(lines, offset) - lines.begin() - 1)};
    auto const text{sources.text(file)};
    return {{"line", line},
            {"character", utf16_length(text.substr(lines[line], offset - lines[line]))}};
}
auto offset_at(sexpr::Sources const& sources, sexpr::SourceFileId file, Json const& pos)
    -> std::uint32_t {
    auto const lines{sources.line_offsets(file)};
    auto const line{pos.at("line").get<std::int64_t>()};
    auto const character{pos.at("character").get<std::int64_t>()};
    if (line < 0 || character < 0) {
        throw std::invalid_argument{"negative position"};
    }
    auto const text{sources.text(file)};
    if (static_cast<std::size_t>(line) >= lines.size()) {
        return static_cast<std::uint32_t>(text.size());
    }
    auto byte{lines[static_cast<std::size_t>(line)]};
    for (std::int64_t unit{};
         unit < character && byte < text.size() && text[byte] != '\n' && text[byte] != '\r';) {
        auto const count{utf8_sequence_size(static_cast<unsigned char>(text[byte]))};
        unit += count == 4 ? 2 : 1;
        if (unit > character) {
            break;
        }
        byte += count;
    }
    return byte;
}
auto range(sexpr::Sources const& sources, sexpr::SourceSpan span) -> Json {
    if (!span.file.value) {
        Json zero{{"line", 0}, {"character", 0}};
        return {{"start", zero}, {"end", zero}};
    }
    return {{"start", position(sources, span.file, span.offset)},
            {"end", position(sources, span.file, span.end_offset)}};
}

auto read_message(std::istream& input) -> std::optional<Json> {
    std::string header;
    std::optional<std::size_t> length;
    while (std::getline(input, header)) {
        if (header == "\r" || header.empty()) {
            break;
        }
        auto const separator{header.find(':')};
        auto key{header.substr(0, separator)};
        std::ranges::transform(
            key, key.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (key == "content-length" && separator != std::string::npos) {
            std::string_view number{header};
            number.remove_prefix(separator + 1);
            auto const begin{number.find_first_not_of(" \t")};
            if (begin == number.npos) {
                throw std::invalid_argument{"empty Content-Length"};
            }
            number.remove_prefix(begin);
            number = number.substr(0, number.find_last_not_of(" \t\r") + 1);
            std::size_t size{};
            auto const end{number.data() + number.size()};
            auto const parsed{std::from_chars(number.data(), end, size)};
            if (parsed.ec != std::errc{} || parsed.ptr != end || length) {
                throw std::invalid_argument{"invalid Content-Length"};
            }
            length = size;
        }
    }
    if (!input) {
        return {};
    }
    if (!length) {
        throw std::invalid_argument{"missing Content-Length"};
    }
    std::string body(*length, '\0');
    if (!input.read(body.data(), static_cast<std::streamsize>(body.size()))) {
        throw std::runtime_error{"truncated message"};
    }
    return Json::parse(body, nullptr, false);
}
void write_message(std::ostream& output, Json const& message) {
    auto const body{message.dump()};
    output << "Content-Length: " << body.size() << "\r\n\r\n" << body << std::flush;
}
}
