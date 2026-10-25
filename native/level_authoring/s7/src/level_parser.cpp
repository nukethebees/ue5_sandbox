#include <ioj/levels/authoring/level_parser.h>

#include "level_processing.h"
#include "reader_diagnostics.h"
#include <ioj/levels/authoring/level_inputs_reader.h>

namespace ioj::levels::authoring {
auto parse_level(s7::Ast const& ast,
                 s7::NodeIndex root,
                 settings::Settings base,
                 std::string_view const path) -> LevelDefinitionReadResult {
    LevelInput input{.settings = std::move(base)};
    s7::Reader const reader{ast};
    s7::AstDiagnostics errors;
    s7::TypeReader<LevelInput>::decode({reader, root, path, s7::ReadMode::Create, errors}, input);
    if (!errors.empty()) {
        return std::unexpected{detail::diagnostics(std::move(errors))};
    }
    return detail::process_level(std::move(input));
}
}
