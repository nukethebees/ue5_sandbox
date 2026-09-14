#include "jobserver/authority.hpp"

#include "jobserver/types.hpp"

#include <Windows.h>

#include <sddl.h>

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <vector>

namespace jobserver {
namespace {
using Json = nlohmann::json;

struct AuthorityRecord {
    DWORD process_id{};
    std::uint64_t creation_time{};
    std::filesystem::path executable;
    std::wstring user_sid;
};

class Handle {
  public:
    explicit Handle(HANDLE const value = nullptr)
        : value_{value} {}
    ~Handle() {
        if (value_ != nullptr && value_ != INVALID_HANDLE_VALUE) {
            CloseHandle(value_);
        }
    }

    Handle(Handle const&) = delete;
    auto operator=(Handle const&) -> Handle& = delete;

    [[nodiscard]] auto get() const -> HANDLE { return value_; }
  private:
    HANDLE value_{};
};

class MutexOwnership {
  public:
    explicit MutexOwnership(HANDLE const mutex)
        : mutex_{mutex} {}
    ~MutexOwnership() { ReleaseMutex(mutex_); }

    MutexOwnership(MutexOwnership const&) = delete;
    auto operator=(MutexOwnership const&) -> MutexOwnership& = delete;
  private:
    HANDLE mutex_{};
};

auto authority_path() -> std::filesystem::path {
    char* test_data{};
    std::size_t test_data_size{};
    if (_dupenv_s(&test_data, &test_data_size, "NUKETHEBEES_JOBSERVER_TEST_DATA") == 0 &&
        test_data != nullptr) {
        std::filesystem::path const result{std::filesystem::path{test_data} / "authority.json"};
        std::free(test_data);
        return result;
    }

    char* local_app_data{};
    std::size_t size{};
    if (_dupenv_s(&local_app_data, &size, "LOCALAPPDATA") != 0 || local_app_data == nullptr) {
        return {};
    }
    std::filesystem::path const result{std::filesystem::path{local_app_data} / "NukeTheBees" /
                                       "jobserver" / "data" / "authority.json"};
    std::free(local_app_data);
    return result;
}

auto process_creation_time(HANDLE const process) -> std::uint64_t {
    FILETIME created{};
    FILETIME exited{};
    FILETIME kernel{};
    FILETIME user{};
    if (!GetProcessTimes(process, &created, &exited, &kernel, &user)) {
        return 0;
    }
    ULARGE_INTEGER value{};
    value.LowPart = created.dwLowDateTime;
    value.HighPart = created.dwHighDateTime;
    return value.QuadPart;
}

auto process_path(HANDLE const process) -> std::filesystem::path {
    std::vector<wchar_t> storage(32'768);
    DWORD size{static_cast<DWORD>(storage.size())};
    if (!QueryFullProcessImageNameW(process, 0, storage.data(), &size)) {
        return {};
    }
    return std::filesystem::path{std::wstring_view{storage.data(), size}};
}

auto current_executable() -> std::filesystem::path {
    std::vector<wchar_t> storage(32'768);
    auto const size{
        GetModuleFileNameW(nullptr, storage.data(), static_cast<DWORD>(storage.size()))};
    if (size == 0 || size >= storage.size()) {
        return {};
    }
    return std::filesystem::path{std::wstring_view{storage.data(), size}};
}

auto token_user_sid(HANDLE const token) -> std::wstring {
    DWORD size{};
    GetTokenInformation(token, TokenUser, nullptr, 0, &size);
    std::vector<std::byte> storage(size);
    if (size == 0 || !GetTokenInformation(token, TokenUser, storage.data(), size, &size)) {
        return {};
    }
    auto const user{reinterpret_cast<TOKEN_USER const*>(storage.data())};
    LPWSTR sid{};
    if (!ConvertSidToStringSidW(user->User.Sid, &sid)) {
        return {};
    }
    std::wstring result{sid};
    LocalFree(sid);
    return result;
}

auto process_user_sid(HANDLE const process) -> std::wstring {
    HANDLE raw_token{};
    if (!OpenProcessToken(process, TOKEN_QUERY, &raw_token)) {
        return {};
    }
    Handle const token{raw_token};
    return token_user_sid(token.get());
}

auto paths_equal(std::filesystem::path const& left, std::filesystem::path const& right) -> bool {
    std::error_code left_error;
    std::error_code right_error;
    auto const left_path{std::filesystem::absolute(left, left_error)};
    auto const right_path{std::filesystem::absolute(right, right_error)};
    if (left_error || right_error) {
        return false;
    }
    auto const left_text{left_path.lexically_normal().wstring()};
    auto const right_text{right_path.lexically_normal().wstring()};
    return CompareStringOrdinal(left_text.c_str(),
                                static_cast<int>(left_text.size()),
                                right_text.c_str(),
                                static_cast<int>(right_text.size()),
                                TRUE) == CSTR_EQUAL;
}

auto read_authority() -> std::expected<AuthorityRecord, Error> {
    auto const path{authority_path()};
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        return std::unexpected(
            Error{"authority_missing", "No daemon authority record is available"});
    }
    auto const json = Json::parse(input, nullptr, false);
    if (!json.is_object() || !json.contains("process_id") ||
        !json["process_id"].is_number_unsigned() || !json.contains("creation_time") ||
        !json["creation_time"].is_number_unsigned() || !json.contains("executable") ||
        !json["executable"].is_string() || !json.contains("user_sid") ||
        !json["user_sid"].is_string()) {
        return std::unexpected(
            Error{"authority_invalid", "The daemon authority record is malformed"});
    }
    auto const process_id{json["process_id"].get<std::uint64_t>()};
    auto const creation_time{json["creation_time"].get<std::uint64_t>()};
    if (process_id == 0 || process_id > std::numeric_limits<DWORD>::max() || creation_time == 0) {
        return std::unexpected(
            Error{"authority_invalid", "The daemon authority record has invalid process identity"});
    }
    return AuthorityRecord{
        .process_id = static_cast<DWORD>(process_id),
        .creation_time = creation_time,
        .executable = path_from_utf8(json["executable"].get<std::string>()),
        .user_sid =
            [&] {
                auto const utf8{json["user_sid"].get<std::string>()};
                auto const size{MultiByteToWideChar(CP_UTF8,
                                                    MB_ERR_INVALID_CHARS,
                                                    utf8.data(),
                                                    static_cast<int>(utf8.size()),
                                                    nullptr,
                                                    0)};
                std::wstring result(static_cast<std::size_t>(size), L'\0');
                if (size > 0) {
                    MultiByteToWideChar(CP_UTF8,
                                        MB_ERR_INVALID_CHARS,
                                        utf8.data(),
                                        static_cast<int>(utf8.size()),
                                        result.data(),
                                        size);
                }
                return result;
            }(),
    };
}

auto record_json(AuthorityRecord const& record) -> Json {
    auto const sid_size{WideCharToMultiByte(CP_UTF8,
                                            WC_ERR_INVALID_CHARS,
                                            record.user_sid.data(),
                                            static_cast<int>(record.user_sid.size()),
                                            nullptr,
                                            0,
                                            nullptr,
                                            nullptr)};
    std::string sid(static_cast<std::size_t>(sid_size), '\0');
    if (sid_size > 0) {
        WideCharToMultiByte(CP_UTF8,
                            WC_ERR_INVALID_CHARS,
                            record.user_sid.data(),
                            static_cast<int>(record.user_sid.size()),
                            sid.data(),
                            sid_size,
                            nullptr,
                            nullptr);
    }
    return Json{{"process_id", record.process_id},
                {"creation_time", record.creation_time},
                {"executable", path_to_utf8(record.executable)},
                {"user_sid", sid}};
}

auto current_authority() -> std::expected<AuthorityRecord, Error> {
    auto const process{GetCurrentProcess()};
    auto const creation_time{process_creation_time(process)};
    auto const executable{current_executable()};
    auto const user_sid{process_user_sid(process)};
    if (creation_time == 0 || executable.empty() || user_sid.empty()) {
        return std::unexpected(
            Error{"authority_unavailable", "Could not identify the daemon process"});
    }
    return AuthorityRecord{.process_id = GetCurrentProcessId(),
                           .creation_time = creation_time,
                           .executable = executable,
                           .user_sid = user_sid};
}

auto records_match(AuthorityRecord const& left, AuthorityRecord const& right) -> bool {
    return left.process_id == right.process_id && left.creation_time == right.creation_time &&
           paths_equal(left.executable, right.executable) && left.user_sid == right.user_sid;
}

void remove_authority_if_matches(AuthorityRecord const& expected) {
    auto const stored{read_authority()};
    if (!stored || !records_match(*stored, expected)) {
        return;
    }
    std::error_code error;
    std::filesystem::remove(authority_path(), error);
}
}

auto publish_authority() -> std::expected<void, Error> {
    auto record{current_authority()};
    if (!record) {
        return std::unexpected(record.error());
    }
    auto const path{authority_path()};
    if (path.empty()) {
        return std::unexpected(
            Error{"authority_unavailable", "Could not locate the jobserver data directory"});
    }
    std::error_code filesystem_error;
    std::filesystem::create_directories(path.parent_path(), filesystem_error);
    if (filesystem_error) {
        return std::unexpected(
            Error{"authority_write_failed", "Could not create the jobserver data directory"});
    }
    auto temporary{path};
    temporary += "." + std::to_string(GetCurrentProcessId()) + ".tmp";
    {
        std::ofstream output{temporary, std::ios::binary | std::ios::trunc};
        output << record_json(*record).dump();
        if (!output) {
            return std::unexpected(
                Error{"authority_write_failed", "Could not write the daemon authority record"});
        }
    }
    if (!MoveFileExW(
            temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        std::filesystem::remove(temporary, filesystem_error);
        return std::unexpected(
            Error{"authority_write_failed", "Could not publish the daemon authority record"});
    }
    return {};
}

void clear_authority() noexcept {
    try {
        auto const stored{read_authority()};
        auto const current{current_authority()};
        if (!stored || !current || !records_match(*stored, *current)) {
            return;
        }
        remove_authority_if_matches(*current);
    } catch (...) {}
}

auto check_recovery_authority(std::function<bool()> const& is_responsive)
    -> std::expected<RecoveryAssessment, Error> {
    RecoveryAssessment result;
    result.responsive = is_responsive();

    auto const current_sid{process_user_sid(GetCurrentProcess())};
    if (current_sid.empty()) {
        return std::unexpected(Error{"recovery_failed", "Could not identify the current user"});
    }
    auto const stored{read_authority()};
    if (!stored) {
        result.reason = stored.error().message;
        return result;
    }
    result.process_id = stored->process_id;
    result.creation_time = stored->creation_time;
    result.executable = stored->executable;

    auto expected_executable{current_executable()};
    expected_executable.replace_filename(L"jobserverd.exe");
    if (!paths_equal(stored->executable, expected_executable)) {
        result.reason = "The authority record does not identify the expected jobserver executable";
        return result;
    }
    if (stored->user_sid != current_sid) {
        result.reason = "The authority record belongs to a different user";
        return result;
    }
    Handle const process{
        OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, stored->process_id)};
    if (process.get() == nullptr) {
        if (GetLastError() == ERROR_INVALID_PARAMETER) {
            result.authority_valid = true;
            result.recoverable = !result.responsive;
            result.reason = result.responsive ? "The daemon is responsive but its record is stale"
                                              : "The recorded daemon has already exited";
            return result;
        }
        result.reason = "Could not inspect the recorded daemon process";
        return result;
    }
    auto const live_creation_time{process_creation_time(process.get())};
    if (live_creation_time != stored->creation_time) {
        result.reason = "The recorded daemon process ID has been reused";
        return result;
    }
    if (WaitForSingleObject(process.get(), 0) == WAIT_OBJECT_0) {
        result.authority_valid = true;
        result.recoverable = !result.responsive;
        result.reason = result.responsive
                          ? "The daemon is responsive but its recorded process exited"
                          : "The recorded daemon has exited";
        return result;
    }
    auto const live{AuthorityRecord{.process_id = stored->process_id,
                                    .creation_time = live_creation_time,
                                    .executable = process_path(process.get()),
                                    .user_sid = process_user_sid(process.get())}};
    if (!records_match(*stored, live) || !paths_equal(live.executable, expected_executable) ||
        live.user_sid != current_sid) {
        result.reason = "The live process does not match the recorded daemon identity";
        return result;
    }

    result.authority_valid = true;
    result.process_running = true;
    result.recoverable = !result.responsive;
    result.reason = result.responsive ? "The daemon is responsive; recovery is not needed"
                                      : "The daemon is unresponsive and safe to recover";
    return result;
}

auto force_recover_authority(std::function<bool()> const& is_responsive)
    -> std::expected<void, Error> {
    auto const current_sid{process_user_sid(GetCurrentProcess())};
    if (current_sid.empty()) {
        return std::unexpected(Error{"recovery_failed", "Could not identify the current user"});
    }
    auto const mutex_name{L"Local\\NukeTheBees.Jobserver.Recovery." + current_sid};
    Handle const mutex{CreateMutexW(nullptr, FALSE, mutex_name.c_str())};
    if (mutex.get() == nullptr) {
        return std::unexpected(
            Error{"recovery_busy", "Could not create the daemon recovery mutex"});
    }
    auto const wait_result{WaitForSingleObject(mutex.get(), 10'000)};
    if (wait_result != WAIT_OBJECT_0 && wait_result != WAIT_ABANDONED) {
        return std::unexpected(
            Error{"recovery_busy", "Another daemon recovery operation is in progress"});
    }
    MutexOwnership const ownership{mutex.get()};
    if (is_responsive()) {
        return std::unexpected(
            Error{"daemon_healthy", "The daemon is responsive; recovery was refused"});
    }

    auto const stored{read_authority()};
    if (!stored) {
        return std::unexpected(stored.error());
    }
    auto expected_executable{current_executable()};
    expected_executable.replace_filename(L"jobserverd.exe");
    if (!paths_equal(stored->executable, expected_executable)) {
        return std::unexpected(
            Error{"authority_mismatch",
                  "The authority record does not identify the expected jobserver executable"});
    }
    if (stored->user_sid != current_sid) {
        return std::unexpected(
            Error{"authority_mismatch", "The authority record belongs to a different user"});
    }
    Handle const process{
        OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE | SYNCHRONIZE,
                    FALSE,
                    stored->process_id)};
    if (process.get() == nullptr) {
        if (GetLastError() == ERROR_INVALID_PARAMETER) {
            remove_authority_if_matches(*stored);
            return {};
        }
        return std::unexpected(
            Error{"recovery_failed", "Could not inspect the recorded daemon process"});
    }
    auto const live_creation_time{process_creation_time(process.get())};
    if (live_creation_time != stored->creation_time) {
        return std::unexpected(
            Error{"authority_mismatch", "The recorded daemon process ID has been reused"});
    }
    if (WaitForSingleObject(process.get(), 0) == WAIT_OBJECT_0) {
        remove_authority_if_matches(*stored);
        return {};
    }
    auto const live{AuthorityRecord{.process_id = stored->process_id,
                                    .creation_time = live_creation_time,
                                    .executable = process_path(process.get()),
                                    .user_sid = process_user_sid(process.get())}};
    if (!records_match(*stored, live) || !paths_equal(live.executable, expected_executable) ||
        live.user_sid != current_sid) {
        return std::unexpected(
            Error{"authority_mismatch",
                  "The authority record does not identify this user's canonical jobserver daemon"});
    }
    if (!TerminateProcess(process.get(), 70) ||
        WaitForSingleObject(process.get(), 10'000) != WAIT_OBJECT_0) {
        return std::unexpected(
            Error{"recovery_failed", "Could not terminate the unresponsive daemon"});
    }
    remove_authority_if_matches(*stored);
    return {};
}
}
