#pragma once

#include <ioj/layout/analyzer/access_operation.hpp>

#include <functional>
#include <map>
#include <string>

namespace ioj::layout {

using AccessSet = std::map<std::string, AccessOperation, std::less<>>;

}
