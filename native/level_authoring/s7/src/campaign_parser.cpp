#include <ioj/levels/authoring/campaign_parser.h>

#include "reader_diagnostics.h"
#include <ioj/levels/campaign_definition_reader.h>

namespace ioj::levels::authoring {
auto parse_campaign(s7::Ast const& ast, s7::NodeIndex const root) -> CampaignDefinitionReadResult {
    return s7::read<CampaignDefinition>(s7::Reader{ast}, root, "campaign")
        .transform_error(detail::diagnostics);
}
}
