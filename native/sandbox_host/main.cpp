#include "application.h"
#include "cli_args.h"

#include <cstdio>
#include <exception>
#include <print>
#include <utility>

auto main(int argc, char** argv) -> int {
    using namespace ioj::sandbox_host;
    try {
        auto cli_args{parse_cli_args(argc, argv)};
        if (!cli_args) {
            return cli_args.error();
        }

        Application application{std::move(*cli_args)};
        if (auto result{application.run()}; !result) {
            std::println(stderr, "{}", result.error());
            return 1;
        }
        return 0;
    } catch (std::exception const& error) {
        std::println(stderr, "{}", error.what());
        return 1;
    }
}
