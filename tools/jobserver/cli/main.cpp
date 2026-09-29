#include "commands.hpp"

#include "jobserver/client.hpp"
#include "jobserver/protocol.hpp"

#include <iostream>

auto wmain(int argc, wchar_t** argv) -> int {
    using namespace jobserver;
    std::vector<std::string> args;
    for (int i{1}; i < argc; ++i) {
        args.push_back(path_to_utf8(argv[i]));
    }
    if (args.empty() || (args.size() == 1 && (args[0] == "--help" || args[0] == "-h"))) {
        std::cout << "agent-task jobs request shared|exclusive NAME\n"
                     "agent-task jobs check|start|end|cancel ID\n"
                     "agent-task jobs status\n"
                     "Append --json for JSON output.\n"
                     "Daemon administration: jobserver ping | shutdown | --version\n";
        return 0;
    }
    if (args.size() == 1 && args[0] == "--version") {
        std::cout << "jobserver 0.4.0 (protocol " << protocol::major_version << ")\n";
        return 0;
    }
    auto const json_output{args.back() == "--json"};
    if (json_output) {
        args.pop_back();
    }
    auto const parsed{cli::parse(args)};
    if (!parsed) {
        std::cerr << "jobserver: " << parsed.error().message << '\n';
        return 1;
    }
    auto const reply{request(*parsed)};
    if (!reply) {
        std::cerr << "jobserver: " << reply.error().code << ": " << reply.error().message << '\n';
        return 1;
    }
    if (json_output) {
        std::cout << reply->dump() << '\n';
    } else {
        cli::print(*reply, std::cout);
    }
    return 0;
}
