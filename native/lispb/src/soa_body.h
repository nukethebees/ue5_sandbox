#pragma once

#include <codegen/schema/declarations.h>
#include <codegen/schema/soa_schemas.h>
#include <lispb/indices.h>

#include <algorithm>
#include <cstddef>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace lispb::codegen {
class Declarations;
struct DeclarationHandle;
}

namespace lispb::codegen::detail {

template <typename Visit>
void scan_soa_columns(std::string_view const body,
                      std::string_view const schema_name,
                      Visit&& visit) {
    constexpr std::string_view marker{"$column("};
    std::size_t begin{};
    std::vector<std::string> path;
    while ((begin = body.find(marker, begin)) != std::string::npos) {
        auto const end{body.find(')', begin + marker.size())};
        if (end == std::string::npos) {
            throw std::invalid_argument{"Unterminated $column reference in SOA '" +
                                        std::string{schema_name} + "'"};
        }
        path.clear();
        auto part{begin + marker.size()};
        while (part <= end) {
            auto const separator{std::min(body.find('.', part), end)};
            auto const name{body.substr(part, separator - part)};
            if (name.empty() ||
                name.find_first_not_of(
                    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_0123456789") !=
                    std::string::npos) {
                throw std::invalid_argument{"Invalid $column path in SOA '" +
                                            std::string{schema_name} + "'"};
            }
            path.emplace_back(name);
            part = separator + 1;
        }
        visit(begin, end + 1, path);
        begin = end + 1;
    }
}

void validate_soa_body_columns(Declarations const& declarations,
                               DeclarationHandle schema,
                               std::map<IdentifierId, DeclarationHandle> const& schemas,
                               std::string_view body);

}
