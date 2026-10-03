#pragma once

#include <string>

namespace jobserver {
struct Error {
    std::string code;
    std::string message;
};
}
