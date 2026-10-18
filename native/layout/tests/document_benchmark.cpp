#include <ioj/layout/document.hpp>
#include <ioj/layout/planner_session.hpp>

#include <lispb/diagnostics.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)
#include <windows.h>

#include <psapi.h>
#endif

namespace ioj::layout::benchmark {
using Clock = std::chrono::steady_clock;

auto private_bytes() -> std::optional<std::uint64_t> {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = static_cast<DWORD>(sizeof(counters));
    if (K32GetProcessMemoryInfo(GetCurrentProcess(),
                                reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
                                static_cast<DWORD>(sizeof(counters)))) {
        return counters.PrivateUsage;
    }
#endif
    return std::nullopt;
}

void report_memory(std::string_view stage) {
    auto const memory{private_bytes()};
    std::cout << stage << " private_bytes=";
    if (memory) {
        std::cout << *memory;
    } else {
        std::cout << "unavailable";
    }
    std::cout << std::endl;
}

void report_samples(std::string_view operation, std::vector<double> samples) {
    std::cout << operation << " samples=" << samples.size();
    if (!samples.empty()) {
        std::ranges::sort(samples);
        auto const last{samples.size() - 1};
        std::cout << " p50_ms=" << samples[last / 2] << " p95_ms=" << samples[last * 95 / 100]
                  << " max_ms=" << samples.back();
    }
    std::cout << std::endl;
}

void report_failure(Document const& document) {
    auto const& diagnostics{document.diagnostics()};
    auto const count{diagnostics.size()};
    auto const first{count > 8 ? count - 8 : 0};
    for (auto row{first}; row < count; ++row) {
        std::cerr << diagnostics.message(lispb::DiagnosticId{static_cast<std::uint32_t>(row)})
                  << '\n';
    }
}

auto run(char const* project, char const* target) -> int {
    auto const deadline{Clock::now() + std::chrono::seconds{170}};
    auto document{Document::load(project, target)};
    if (!document.valid()) {
        report_failure(document);
        return 1;
    }
    auto const selected{document.find_type("sandbox_ismc_render", "PackedTransform")};
    if (!selected) {
        std::cerr << "Expected sandbox_ismc_render/PackedTransform in the repository target.\n";
        return 1;
    }
    PlannerAnalysisSession session{document.types()};
    session.edit_inputs().selection.select_type(document.types(), selected);
    session.refresh(&document);
    auto create{document.transaction()};
    create.create_variant("Benchmark");
    if (!document.commit(create)) {
        report_failure(document);
        return 1;
    }
    session.refresh(&document);
    auto const variant{document.variants().ids().back()};
    report_memory("loaded");

    constexpr std::size_t transactions{1'000};
    std::vector<double> commits;
    std::vector<double> undos;
    std::vector<double> redos;
    commits.reserve(transactions);
    undos.reserve(transactions);
    redos.reserve(transactions);
    for (std::size_t row{}; row < transactions && Clock::now() < deadline; ++row) {
        auto transaction{document.transaction()};
        transaction.rename_variant(variant, "Benchmark " + std::to_string(row));
        auto const alternate{(row / 4) % 2 == 0};
        switch (row % 4) {
            case 0:
            case 3:
                transaction.set_member_count(
                    "sandbox_ismc_render", "PackedTransform", "position", alternate ? 4 : 3);
                break;
            case 1:
                transaction.set_member_type("sandbox_ismc_render",
                                            "PackedTransform",
                                            "reserved",
                                            alternate ? "std::uint32_t" : "std::uint16_t");
                break;
            default:
                break;
        }
        auto const start{Clock::now()};
        if (!document.commit(transaction)) {
            report_failure(document);
            return 1;
        }
        session.refresh(&document);
        commits.push_back(std::chrono::duration<double, std::milli>{Clock::now() - start}.count());
    }
    report_samples("commit+analysis", commits);
    report_memory("history");

    for (std::size_t row{}; row < commits.size() && Clock::now() < deadline; ++row) {
        auto const start{Clock::now()};
        if (!document.undo()) {
            report_failure(document);
            return 1;
        }
        session.refresh(&document);
        undos.push_back(std::chrono::duration<double, std::milli>{Clock::now() - start}.count());
    }
    report_samples("undo+analysis", undos);
    report_memory("undone");

    for (std::size_t row{}; row < undos.size() && Clock::now() < deadline; ++row) {
        auto const start{Clock::now()};
        if (!document.redo()) {
            report_failure(document);
            return 1;
        }
        session.refresh(&document);
        redos.push_back(std::chrono::duration<double, std::milli>{Clock::now() - start}.count());
    }
    report_samples("redo+analysis", redos);
    report_memory("redone");
    if (commits.size() != transactions || undos.size() != transactions ||
        redos.size() != transactions) {
        std::cerr << "Benchmark exceeded its 170-second budget; results are incomplete.\n";
        return 2;
    }
    return 0;
}
}

auto main(int argc, char** argv) -> int {
    if (argc < 2 || argc > 3) {
        std::cerr << "Usage: native-layout-document-benchmark <project.lispb> [sandbox-code]\n";
        return 1;
    }
    std::cout << std::fixed << std::setprecision(3);
    try {
        return ioj::layout::benchmark::run(argv[1], argc == 3 ? argv[2] : "sandbox-code");
    } catch (std::exception const& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
