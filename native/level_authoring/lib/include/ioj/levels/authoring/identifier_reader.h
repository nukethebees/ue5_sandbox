#pragma once

#include <ioj/levels/identifiers.h>
#include <ioj/s7/reader.h>

namespace ioj::s7 {
template <levels::Identifier Id>
struct TypeReader<Id> {
    static auto read(Reader const& reader, NodeIndex const node, std::string_view const path)
        -> ReadResult<Id> {
        auto value{reader.symbol(node, path)};
        if (!value) {
            return std::unexpected{std::move(value.error())};
        }
        if constexpr (!std::same_as<Id, levels::CampaignId>) {
            if (value->empty()) {
                return read_error(AstErrorCode::InvalidSymbol, path, "Symbol must not be empty");
            }
        }
        return Id{std::move(*value)};
    }
};
}
