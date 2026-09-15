#include "log_store.hpp"

#include <Windows.h>

#include <gtest/gtest.h>

#include <chrono>
#include <fstream>
#include <iterator>
#include <thread>
#include <vector>

namespace {
class JobserverLogs : public ::testing::Test {
  protected:
    void SetUp() override {
        directory_ = std::filesystem::temp_directory_path() /
                     ("jobserver-log-tests-" + std::to_string(GetCurrentProcessId()) + "-" +
                      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(directory_);
    }
    void TearDown() override { std::filesystem::remove_all(directory_); }
    auto read(std::filesystem::path const& path) -> std::string {
        std::ifstream input{path, std::ios::binary};
        return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    }
    void seed(std::string const& id, int age, std::size_t size = 4) {
        for (auto const suffix : {".stdout.log", ".stderr.log"}) {
            auto const path{directory_ / (id + suffix)};
            {
                std::ofstream output{path, std::ios::binary};
                output << std::string(size, 'x');
            }
            std::filesystem::last_write_time(
                path, std::filesystem::file_time_type::clock::now() - std::chrono::hours{age});
        }
    }
    std::filesystem::path directory_;
    inline static std::string const first_ = std::string(32, 'a');
    inline static std::string const second_ = std::string(32, 'b');
    inline static std::string const third_ = std::string(32, 'c');
};
}

TEST_F(JobserverLogs, CapsBytesAndMarksTruncationOnce) {
    auto const path{directory_ / "capped.log"};
    jobserver::CappedLog log{path, 64};
    log.write("prefix");
    log.write(std::string(256, 'x'));
    log.write(std::string(256, 'y'));
    log.close();
    auto const text{read(path)};
    EXPECT_EQ(text.size(), 64U);
    EXPECT_TRUE(text.starts_with("prefix"));
    EXPECT_TRUE(text.ends_with("\n[jobserver: log size limit reached]\n"));
}

TEST_F(JobserverLogs, ExactCapIsPreservedUntilMoreOutputArrives) {
    auto const path{directory_ / "exact.log"};
    jobserver::CappedLog log{path, 64};
    log.write(std::string(32, 'x'));
    log.write(std::string(32, 'y'));
    EXPECT_EQ(read(path), std::string(32, 'x') + std::string(32, 'y'));
    log.write("overflow");
    log.close();
    EXPECT_EQ(std::filesystem::file_size(path), 64U);
    EXPECT_TRUE(read(path).ends_with("\n[jobserver: log size limit reached]\n"));
    jobserver::CappedLog zero{directory_ / "zero.log", 0};
    zero.write("discarded");
    zero.close();
    EXPECT_EQ(std::filesystem::file_size(directory_ / "zero.log"), 0U);
}

TEST_F(JobserverLogs, ShortLogsAndEmptyLogsRemainUnchanged) {
    jobserver::LogStore store{directory_, 64, 1};
    auto logs{store.open(first_)};
    ASSERT_TRUE(logs);
    logs->write("stdout", std::string("short\0text", 10));
    logs.reset();
    EXPECT_EQ(read(directory_ / (first_ + ".stdout.log")), std::string("short\0text", 10));
    EXPECT_EQ(std::filesystem::file_size(directory_ / (first_ + ".stderr.log")), 0U);
}

TEST_F(JobserverLogs, StartupRetainsNewestPairsAndTrimsLegacyOversizedLogs) {
    seed(first_, 3);
    seed(second_, 2);
    seed(third_, 1, 256);
    {
        std::ofstream note{directory_ / "keep.txt"};
        note << "keep";
    }
    {
        std::ofstream unknown{directory_ / "unknown.stdout.log"};
        unknown << "keep";
    }
    jobserver::LogStore store{directory_, 64, 2};
    EXPECT_FALSE(std::filesystem::exists(directory_ / (first_ + ".stdout.log")));
    EXPECT_FALSE(std::filesystem::exists(directory_ / (first_ + ".stderr.log")));
    EXPECT_TRUE(std::filesystem::exists(directory_ / (second_ + ".stdout.log")));
    EXPECT_EQ(std::filesystem::file_size(directory_ / (third_ + ".stdout.log")), 64U);
    EXPECT_TRUE(std::filesystem::exists(directory_ / "keep.txt"));
    EXPECT_TRUE(std::filesystem::exists(directory_ / "unknown.stdout.log"));
}

TEST_F(JobserverLogs, ActivePairsSurviveCleanupAndRetireOnClose) {
    jobserver::LogStore store{directory_, 64, 1};
    auto active{store.open(first_)};
    auto second{store.open(second_)};
    second.reset();
    auto third{store.open(third_)};
    third.reset();
    EXPECT_TRUE(std::filesystem::exists(directory_ / (first_ + ".stdout.log")));
    EXPECT_FALSE(std::filesystem::exists(directory_ / (second_ + ".stdout.log")));
    active->write("stdout", "still active");
    active.reset();
    EXPECT_TRUE(std::filesystem::exists(directory_ / (first_ + ".stdout.log")));
    EXPECT_FALSE(std::filesystem::exists(directory_ / (third_ + ".stdout.log")));
}

TEST_F(JobserverLogs, UnavailableStorageAndZeroRetentionAreSafe) {
    auto const blocked{directory_ / "blocked"};
    {
        std::ofstream output{blocked};
        output << "block";
    }
    jobserver::LogStore blocked_store{blocked, 64, 0};
    auto missing{blocked_store.open(first_)};
    EXPECT_FALSE(missing);
    missing.reset();
    jobserver::LogStore store{directory_, 64, 0};
    auto active{store.open(first_)};
    EXPECT_TRUE(std::filesystem::exists(directory_ / (first_ + ".stdout.log")));
    active.reset();
    EXPECT_FALSE(std::filesystem::exists(directory_ / (first_ + ".stdout.log")));
}

TEST_F(JobserverLogs, ConcurrentCompletionKeepsActiveLogsAndEnforcesRetention) {
    jobserver::LogStore store{directory_, 64, 2};
    auto active{store.open(first_)};
    std::vector<std::jthread> workers;
    for (auto index{1}; index <= 8; ++index) {
        workers.emplace_back([&, index] {
            auto logs{store.open(std::string(31, '0') + std::to_string(index))};
            ASSERT_TRUE(logs);
            logs->write("stdout", std::string(256, 'x'));
        });
    }
    workers.clear();
    EXPECT_TRUE(std::filesystem::exists(directory_ / (first_ + ".stdout.log")));
    auto count{0};
    for (auto const& file : std::filesystem::directory_iterator{directory_}) {
        EXPECT_LE(file.file_size(), 64U);
        ++count;
    }
    EXPECT_EQ(count, 6);
    active.reset();
    EXPECT_EQ(std::distance(std::filesystem::directory_iterator{directory_},
                            std::filesystem::directory_iterator{}),
              4);
}
