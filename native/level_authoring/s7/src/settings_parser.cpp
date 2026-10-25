#include <ioj/levels/authoring/settings_parser.h>

#include "reader_diagnostics.h"
#include <ioj/settings/settings_values_reader.h>

namespace ioj::levels::authoring {
auto parse_settings(s7::Ast const& ast, s7::NodeIndex root, settings::Settings base)
    -> SettingsReadResult {
    s7::Reader const reader{ast};
    return s7::apply_overrides(std::move(base), {{reader, root, ""}})
        .transform_error(detail::diagnostics);
}
}
