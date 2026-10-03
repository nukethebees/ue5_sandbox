#include "commands.hpp"

#include "jobserver/client/request.hpp"
#include "jobserver/path_encoding.hpp"

#include <format>
#include <iostream>

auto wmain(int argc, wchar_t** argv) -> int {
    std::vector<std::string> args;
    for (int i{1}; i < argc; ++i) {
        args.push_back(jobserver::path_to_utf8(argv[i]));
    }

    auto const parsed{jobserver::client::cli::parse(args)};
    if (!parsed) {
        auto const& result{parsed.error()};
        auto& output{result.exit_code == 0 ? std::cout : std::cerr};
        output << result.message;
        return result.exit_code;
    }

    auto const reply{jobserver::client::request(parsed->message)};
    if (!reply) {
        std::cerr << std::format("jobserver: {}: {}\n", reply.error().code, reply.error().message);
        return 1;
    }
    if (parsed->json_output) {
        std::cout << reply->dump() << '\n';
    } else {
        std::cout << jobserver::client::cli::format_reply(*reply);
    }
    return 0;
}
