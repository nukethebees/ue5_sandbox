#include "jobserver/server/server.hpp"

auto wmain(int argc, wchar_t* argv[]) -> int {
    if (argc != 2) {
        return 2;
    }
    jobserver::server::Server server{argv[1]};
    return server.run();
}
