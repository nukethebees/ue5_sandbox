#include <ioj/levels/authoring/level_parser.h>

#include "level_processing.h"
#include "reader_diagnostics.h"
#include <ioj/levels/authoring/level_inputs_reader.h>

namespace ioj::levels::authoring {
auto parse_level(s7::Ast const& ast, s7::NodeIndex root, std::optional<settings::Settings> base)
    -> LevelDefinitionReadResult {
    LevelInput input{.settings = std::move(base)};
    s7::Reader const reader{ast};
    s7::AstDiagnostics errors;
    s7::TypeReader<LevelInput>::decode({reader, root, "level", s7::ReadMode::Create, errors},
                                       input);
    if (!errors.empty()) {
        return std::unexpected{detail::diagnostics(std::move(errors))};
    }
    return detail::process_level(std::move(input));
}
}
