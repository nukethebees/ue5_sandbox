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
            return ReadResult<Id>{std::unexpect, std::move(value.error())};
        }
        if constexpr (!std::same_as<Id, levels::CampaignId>) {
            if (value->empty()) {
                return read_error(AstErrorCode::InvalidSymbol, path, "Symbol must not be empty");
            }
        }
        return ReadResult<Id>{std::in_place, std::move(*value)};
    }
};

}
