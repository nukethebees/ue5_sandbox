#include <ioj/levels/authoring/level_parser.h>

#include "level_processing.h"
#include "reader_diagnostics.h"
#include <ioj/levels/authoring/generated/level_inputs_reader.h>

namespace ioj::levels::authoring {
auto parse_level(s7::Ast const& ast, s7::NodeIndex root) -> LevelDefinitionReadResult {
    return s7::read<LevelInput>(s7::Reader{ast}, root, "level")
        .transform_error(detail::diagnostics)
        .and_then(detail::process_level);
}
}
