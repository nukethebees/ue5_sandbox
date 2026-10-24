#pragma once
#include "ast_parser.h"
#include <ioj/levels/mission_definition.h>

namespace ioj::levels::authoring::detail {
auto parse_mission(AstParser& parser,
                   s7::Ast const& ast,
                   s7::NodeIndex node,
                   std::string const& path) -> MissionDefinition;
}
