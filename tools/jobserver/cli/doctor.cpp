#include "doctor.hpp"

#include "jobserver/client.hpp"
#include "jobserver/protocol.hpp"
#include "jobserver/types.hpp"

#include <Windows.h>

#include <taskschd.h>
#include <wrl/client.h>

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace jobserver::cli {
namespace {
using Json = nlohmann::json;
using Microsoft::WRL::ComPtr;

auto local_app_data() -> std::filesystem::path {
    char* value{};
    std::size_t size{};
    if (_dupenv_s(&value, &size, "LOCALAPPDATA") != 0 || value == nullptr) {
        return {};
    }
    std::filesystem::path result{value};
    std::free(value);
    return result;
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

auto process_executable(DWORD const process_id) -> std::filesystem::path {
    auto const process{OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id)};
    if (process == nullptr) {
        return {};
    }
    std::vector<wchar_t> storage(32'768);
    DWORD size{static_cast<DWORD>(storage.size())};
    auto const queried{QueryFullProcessImageNameW(process, 0, storage.data(), &size) != FALSE};
    CloseHandle(process);
    return queried ? std::filesystem::path{std::wstring_view{storage.data(), size}}
                   : std::filesystem::path{};
}

auto paths_equal(std::filesystem::path const& left, std::filesystem::path const& right) -> bool {
    return _wcsicmp(left.lexically_normal().c_str(), right.lexically_normal().c_str()) == 0;
}

auto decode_xml(std::wstring value) -> std::wstring {
    for (auto const& [encoded, decoded] :
         std::vector<std::pair<std::wstring_view, std::wstring_view>>{{L"&amp;", L"&"},
                                                                      {L"&quot;", L"\""},
                                                                      {L"&apos;", L"'"},
                                                                      {L"&lt;", L"<"},
                                                                      {L"&gt;", L">"}}) {
        for (auto position{value.find(encoded)}; position != std::wstring::npos;
             position = value.find(encoded, position + decoded.size())) {
            value.replace(position, encoded.size(), decoded);
        }
    }
    return value;
}

auto scheduled_task_executable() -> std::optional<std::filesystem::path> {
    auto const initialized{CoInitializeEx(nullptr, COINIT_MULTITHREADED)};
    auto const uninitialize{SUCCEEDED(initialized)};
    if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE) {
        return std::nullopt;
    }

    ComPtr<ITaskService> service;
    auto result{CoCreateInstance(CLSID_TaskScheduler,
                                 nullptr,
                                 CLSCTX_INPROC_SERVER,
                                 IID_ITaskService,
                                 reinterpret_cast<void**>(service.GetAddressOf()))};
    VARIANT empty{};
    VariantInit(&empty);
    if (SUCCEEDED(result)) {
        result = service->Connect(empty, empty, empty, empty);
    }

    ComPtr<ITaskFolder> root;
    auto const root_name{SysAllocString(L"\\")};
    if (SUCCEEDED(result) && root_name != nullptr) {
        result = service->GetFolder(root_name, &root);
    } else if (root_name == nullptr) {
        result = E_OUTOFMEMORY;
    }
    SysFreeString(root_name);

    ComPtr<IRegisteredTask> task;
    auto const task_name{SysAllocString(L"NukeTheBeesJobserver")};
    if (SUCCEEDED(result) && task_name != nullptr) {
        result = root->GetTask(task_name, &task);
    } else if (task_name == nullptr) {
        result = E_OUTOFMEMORY;
    }
    SysFreeString(task_name);

    BSTR xml{};
    if (SUCCEEDED(result)) {
        result = task->get_Xml(&xml);
    }
    std::optional<std::filesystem::path> path;
    if (SUCCEEDED(result) && xml != nullptr) {
        std::wstring_view const text{xml, SysStringLen(xml)};
        constexpr std::wstring_view opening{L"<Command>"};
        constexpr std::wstring_view closing{L"</Command>"};
        auto const begin{text.find(opening)};
        auto const end{begin == std::wstring_view::npos
                           ? std::wstring_view::npos
                           : text.find(closing, begin + opening.size())};
        if (end != std::wstring_view::npos) {
            path = std::filesystem::path{decode_xml(
                std::wstring{text.substr(begin + opening.size(), end - begin - opening.size())})};
        }
    }
    SysFreeString(xml);
    task.Reset();
    root.Reset();
    service.Reset();
    if (uninitialize) {
        CoUninitialize();
    }
    return path;
}

void add(std::vector<DoctorCheck>& checks,
         DoctorStatus const status,
         std::string name,
         std::string detail) {
    checks.push_back({.status = status, .name = std::move(name), .detail = std::move(detail)});
}
}

auto run_doctor() -> std::vector<DoctorCheck> {
    std::vector<DoctorCheck> checks;
    auto const app_data{local_app_data()};
    if (app_data.empty()) {
        add(checks, DoctorStatus::failure, "local application data", "LOCALAPPDATA is unavailable");
        return checks;
    }
    auto const root{app_data / "NukeTheBees" / "jobserver"};
    auto const bin{root / "bin"};
    auto const data{root / "data"};
    auto const installed_client{bin / "jobserver.exe"};
    auto const installed_daemon{bin / "jobserverd.exe"};

    std::error_code filesystem_error;
    auto const client_exists{std::filesystem::is_regular_file(installed_client, filesystem_error)};
    filesystem_error.clear();
    auto const daemon_exists{std::filesystem::is_regular_file(installed_daemon, filesystem_error)};
    add(checks,
        client_exists ? DoctorStatus::pass : DoctorStatus::failure,
        "client binary",
        path_to_utf8(installed_client));
    add(checks,
        daemon_exists ? DoctorStatus::pass : DoctorStatus::failure,
        "daemon binary",
        path_to_utf8(installed_daemon));

    auto const running_client{current_executable()};
    add(checks,
        paths_equal(running_client, installed_client) ? DoctorStatus::pass : DoctorStatus::warning,
        "client location",
        path_to_utf8(running_client));

    auto const task_executable{scheduled_task_executable()};
    add(checks,
        task_executable && paths_equal(*task_executable, installed_daemon) ? DoctorStatus::pass
                                                                           : DoctorStatus::failure,
        "scheduled task",
        task_executable ? path_to_utf8(*task_executable) : "missing or unreadable");

    auto status{Client::status()};
    if (!status) {
        add(checks, DoctorStatus::failure, "daemon connection", status.error().message);
    } else {
        auto const json = Json::parse(*status, nullptr, false);
        auto const daemon = json.is_object() ? json.value("daemon", Json{}) : Json{};
        if (!daemon.is_object()) {
            add(checks, DoctorStatus::failure, "daemon connection", "invalid status response");
        } else {
            auto const process_id{daemon.value("process_id", 0U)};
            auto const executable{process_executable(process_id)};
            add(checks, DoctorStatus::pass, "daemon connection", "responsive");
            add(checks,
                !executable.empty() && paths_equal(executable, installed_daemon)
                    ? DoctorStatus::pass
                    : DoctorStatus::failure,
                "daemon identity",
                executable.empty() ? "process is unavailable" : path_to_utf8(executable));
            auto const major{daemon.value("protocol_major", 0U)};
            auto const minor{daemon.value("protocol_minor", 0U)};
            add(checks,
                major == protocol::major_version ? DoctorStatus::pass : DoctorStatus::failure,
                "protocol",
                std::to_string(major) + "." + std::to_string(minor));
        }
    }

    if (paths_equal(running_client, installed_client)) {
        auto authority{Client::check_daemon_recovery()};
        auto const valid{authority && authority->responsive && authority->authority_valid &&
                         authority->process_running &&
                         paths_equal(authority->executable, installed_daemon)};
        add(checks,
            valid ? DoctorStatus::pass : DoctorStatus::failure,
            "authority record",
            authority ? authority->reason : authority.error().message);
    } else {
        add(checks,
            DoctorStatus::warning,
            "authority record",
            "run the installed client for canonical authority validation");
    }

    filesystem_error.clear();
    std::filesystem::create_directories(data, filesystem_error);
    auto const probe{data / (".doctor-" + std::to_string(GetCurrentProcessId()) + ".tmp")};
    auto writable{!filesystem_error};
    if (writable) {
        std::ofstream output{probe, std::ios::trunc};
        output << "probe";
        writable = output.good();
    }
    std::filesystem::remove(probe, filesystem_error);
    add(checks,
        writable ? DoctorStatus::pass : DoctorStatus::failure,
        "data directory",
        path_to_utf8(data));

    auto const daemon_log{data / "jobserverd.log"};
    filesystem_error.clear();
    auto const log_exists{std::filesystem::is_regular_file(daemon_log, filesystem_error)};
    add(checks,
        log_exists ? DoctorStatus::pass : DoctorStatus::warning,
        "daemon log",
        path_to_utf8(daemon_log));

    std::size_t abandoned_staging{};
    filesystem_error.clear();
    for (std::filesystem::directory_iterator iterator{root, filesystem_error}, end;
         !filesystem_error && iterator != end;
         iterator.increment(filesystem_error)) {
        if (iterator->is_directory() &&
            iterator->path().filename().wstring().starts_with(L".staging-")) {
            ++abandoned_staging;
        }
    }
    add(checks,
        abandoned_staging == 0 ? DoctorStatus::pass : DoctorStatus::warning,
        "staging directories",
        abandoned_staging == 0 ? "none" : std::to_string(abandoned_staging) + " abandoned");
    return checks;
}
}
