#include <ioj/levels/authoring/settings_parser.h>

#include "reader_diagnostics.h"
#include <ioj/settings/settings_values_reader.h>

namespace ioj::levels::authoring {
auto parse_settings(s7::Ast const& ast, s7::NodeIndex root, settings::Settings base)
    -> SettingsReadResult {
    return s7::read_override(s7::Reader{ast}, root, "", std::move(base))
        .transform_error(detail::diagnostics);
}
}
