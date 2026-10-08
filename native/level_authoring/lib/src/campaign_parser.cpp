#include <ioj/levels/authoring/campaign_parser.h>

#include "ast_parser.h"

namespace ioj::levels::authoring {
auto parse_campaign(s7::Ast const& ast) -> CampaignDefinitionReadResult {
    detail::AstParser parser{ast};
    CampaignDefinition result;
    auto const fields{parser.record(ast.root, RecordKind::Campaign, "campaign")};
    for (auto const& field : fields) {
        switch (field.property) {
            case Property::Id:
                result.id = CampaignId{parser.symbol(field.value, field.node_path)};
                break;
            case Property::Title:
                result.title = parser.string(field.value, field.node_path);
                break;
            case Property::Levels:
                result.level_ids = parser.ids<LevelId>(field.value, field.node_path);
                break;
            default:
                parser.unknown(field);
                break;
        }
    }
    for (auto const property : {Property::Id, Property::Title, Property::Levels}) {
        parser.require(fields, property, "campaign");
    }
    if (!parser.errors().empty()) {
        return std::unexpected{parser.take_errors()};
    }
    return result;
}
}
