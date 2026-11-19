#include <ioj/levels/authoring/campaign_parser.h>

#include "reader_diagnostics.h"
#include <ioj/levels/campaign_definition_reader.h>
#include <ioj/levels/campaign_validation.h>

namespace ioj::levels::authoring {
auto parse_campaign(s7::Ast const& ast, s7::NodeIndex const root) -> CampaignDefinitionReadResult {
    auto definition{s7::read<CampaignDefinition>(s7::Reader{ast}, root, "campaign")
                        .transform_error(detail::diagnostics)};
    if (definition) {
        if (auto valid{validate_campaign(*definition)}; !valid) {
            return std::unexpected{std::move(valid.error())};
        }
    }
    return definition;
}
}
