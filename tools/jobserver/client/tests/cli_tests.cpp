#include "commands.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace jobserver::client::tests {
TEST(JobsCli, RejectsInvalidSyntaxWithoutContactingServer) {
    for (auto const& args :
         std::vector<std::vector<std::string>>{{},
                                               {"request", "other", "name"},
                                               {"request", "shared", ""},
                                               {"request", "shared"},
                                               {"check", "0"},
                                               {"start", "-1"},
                                               {"end", "1x"},
                                               {"cancel", "18446744073709551616"},
                                               {"status", "extra"}}) {
        EXPECT_FALSE(cli::parse(args));
    }
}
}
