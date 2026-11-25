#pragma once

#include "cli_args.h"

#include <expected>
#include <string>

namespace ioj::sandbox_host {
class Application {
  public:
    explicit Application(CliArgs args);
    // Run one engine instance through shutdown.
    auto run() const -> std::expected<void, std::string>;
  private:
    CliArgs args_;
};
}
