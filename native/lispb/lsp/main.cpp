#include "analysis.h"

#include <iostream>
#include <memory>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

namespace lispb::lsp {
struct Unit {
    fs::path path;
    TargetId target;
    std::unique_ptr<Analysis> analysis;
    bool dirty{};
    bool needed{};
};

class Workspace {
  public:
    void change(fs::path const& path, std::optional<Buffer> buffer) {
        auto const id{file_id(path)};
        auto& row{files_[id.value]};
        if (row.buffer && buffer && buffer->version <= row.buffer->version) {
            return;
        }
        row.buffer = std::move(buffer);
        invalidate(path);
        refresh();
    }
    void invalidate(fs::path const& path) {
        for (auto& unit : units_) {
            if (unit.analysis &&
                (unit.path == path || unit.analysis->contains(path) ||
                 std::ranges::any_of(unit.analysis->diagnostics(),
                                     [&](auto const& issue) { return issue.path == path; }))) {
                unit.dirty = true;
            }
        }
    }
    void refresh() {
        for (auto& unit : units_) {
            unit.needed = false;
        }
        for (auto& row : files_) {
            row.active = {};
            if (row.buffer) {
                select_analysis(row);
            }
        }
        for (auto& unit : units_) {
            if (!unit.needed) {
                unit = {};
            }
        }
        publish();
        std::erase_if(files_,
                      [](WorkspaceFile const& row) { return !row.buffer && !row.published; });
    }
    auto query(fs::path const& path,
               Method method,
               Json const& params,
               ClientCapabilities capabilities) -> std::expected<Json, std::string> {
        auto const found{std::ranges::find(files_, path, &WorkspaceFile::path)};
        if (found == files_.end() || found->active == UnitId{}) {
            return nullptr;
        }
        auto& analysis{*units_[found->active.value].analysis};
        if (analysis.contains(path)) {
            return analysis.query(path, method, params, capabilities);
        }
        return nullptr;
    }
  private:
    auto file_id(fs::path const& path) -> WorkspaceFileId {
        auto const found{std::ranges::find(files_, path, &WorkspaceFile::path)};
        if (found != files_.end()) {
            return WorkspaceFileId{static_cast<std::uint32_t>(found - files_.begin())};
        }
        files_.push_back({.path = path});
        return WorkspaceFileId{static_cast<std::uint32_t>(files_.size() - 1)};
    }
    auto unit_id(fs::path const& path, TargetId target) -> UnitId {
        auto const found{std::ranges::find_if(
            units_, [&](Unit const& unit) { return unit.path == path && unit.target == target; })};
        if (found != units_.end()) {
            return UnitId{static_cast<std::uint32_t>(found - units_.begin())};
        }
        auto const slot{
            std::ranges::find_if(units_, [](Unit const& unit) { return unit.path.empty(); })};
        if (slot != units_.end()) {
            slot->path = path;
            slot->target = target;
            return UnitId{static_cast<std::uint32_t>(slot - units_.begin())};
        }
        units_.push_back({.path = path, .target = target});
        return UnitId{static_cast<std::uint32_t>(units_.size() - 1)};
    }
    auto standalone(fs::path const& path) -> UnitId {
        auto const id{unit_id(path, {})};
        auto& unit{units_[id.value]};
        if (!unit.analysis || unit.dirty) {
            unit.analysis = std::make_unique<Analysis>(files_, path);
            unit.dirty = false;
        }
        return id;
    }
    auto discover(fs::path const& path) -> UnitId {
        for (auto dir{path.parent_path()}; !dir.empty();) {
            for (auto relative : {"project.lispb", "lispb/project.lispb"}) {
                auto const manifest{dir / relative};
                auto const open{std::ranges::find(files_, manifest, &WorkspaceFile::path)};
                if ((open != files_.end() && open->buffer) || fs::is_regular_file(manifest)) {
                    auto const id{standalone(manifest)};
                    if (units_[id.value].analysis->project()) {
                        return id;
                    }
                }
            }
            auto const parent{dir.parent_path()};
            if (parent == dir) {
                break;
            }
            dir = parent;
        }
        return {};
    }
    void select_target(WorkspaceFile& row, UnitId id, Project const& project, TargetId target) {
        auto& unit{units_[id.value]};
        if (!unit.analysis || unit.dirty) {
            unit.analysis = std::make_unique<Analysis>(files_, project, target);
            unit.dirty = false;
        }
        if (unit.analysis->contains(row.path)) {
            unit.needed = true;
            if (row.active == UnitId{}) {
                row.active = id;
            }
        }
    }
    void select_analysis(WorkspaceFile& row) {
        auto const manifest{discover(row.path)};
        if (manifest != UnitId{}) {
            units_[manifest.value].needed = true;
            auto const manifest_path{units_[manifest.value].path};
            if (manifest_path == row.path) {
                row.active = manifest;
                return;
            }
            auto const& project{*units_[manifest.value].analysis->project()};
            for (auto target : project.target_ids()) {
                if (target.kind() == TargetKind::group) {
                    continue;
                }
                auto const id{unit_id(manifest_path, target)};
                auto const& existing{units_[id.value].analysis};
                auto const inputs{project.source_files(target)};
                bool owns{std::ranges::any_of(
                              inputs,
                              [&](auto const& input) {
                                  return (project.root / input).lexically_normal() == row.path;
                              }) ||
                          (existing && existing->contains(row.path))};
                if (owns) {
                    select_target(row, id, project, target);
                }
            }
            if (row.active == UnitId{}) {
                // An included registry or macro library can open before its consumer.
                auto const loose{standalone(row.path)};
                auto& analysis{*units_[loose.value].analysis};
                auto const head{analysis.contains(row.path) ? analysis.root_head(row.path)
                                                            : std::nullopt};
                for (auto target : project.target_ids()) {
                    if ((target.kind() == TargetKind::cpp_schema &&
                         (head == FormHead::type || head == FormHead::include)) ||
                        (target.kind() == TargetKind::slate &&
                         (head == FormHead::defmacro || head == FormHead::include))) {
                        select_target(row, unit_id(manifest_path, target), project, target);
                    }
                }
            }
        }
        if (row.active == UnitId{}) {
            row.active = standalone(row.path);
            units_[row.active.value].needed = true;
        }
    }
    void publish() {
        std::vector<Json> pending(files_.size());
        for (auto const& unit : units_) {
            if (!unit.analysis) {
                continue;
            }
            for (auto const& issue : unit.analysis->diagnostics()) {
                auto const id{file_id(issue.path)};
                pending.resize(files_.size());
                auto& list{pending[id.value]};
                auto item = unit.analysis->diagnostic(issue.id);
                if (std::ranges::find(list, item) == list.end()) {
                    list.push_back(std::move(item));
                }
            }
        }
        auto const count{files_.size()};
        for (std::size_t i{}; i < count; ++i) {
            auto& row{files_[i]};
            auto& items{pending[i]};
            if (!row.buffer && !row.published && items.empty()) {
                continue;
            }
            row.published = !items.empty();
            Json params{{"uri", row.buffer ? row.buffer->uri : file_uri(row.path)}};
            params["diagnostics"] = items.is_null() ? Json::array() : std::move(items);
            if (row.buffer) {
                params["version"] = row.buffer->version;
            }
            write_message(std::cout,
                          {{"jsonrpc", "2.0"},
                           {"method", "textDocument/publishDiagnostics"},
                           {"params", std::move(params)}});
        }
    }
    std::vector<WorkspaceFile> files_;
    std::vector<Unit> units_;
};

enum class SessionState { awaiting_initialize, running, shutdown };

static auto preferred_markup(Json const& params, Json::json_pointer const& formats) -> MarkupKind {
    if (params.contains(formats)) {
        for (auto const& item : params.at(formats)) {
            if (auto format{try_parse_serialized_markup_kind(item.get_ref<std::string const&>())}) {
                return *format;
            }
        }
    }
    return MarkupKind::plaintext;
}
static auto initialize(Json const& params, ClientCapabilities& capabilities) -> Json {
    Json::json_pointer const hierarchy{
        "/capabilities/textDocument/documentSymbol/hierarchicalDocumentSymbolSupport"};
    capabilities.hierarchical_symbols = params.value(hierarchy, false);
    capabilities.hover_format = preferred_markup(
        params, Json::json_pointer{"/capabilities/textDocument/hover/contentFormat"});
    capabilities.completion_format = preferred_markup(
        params,
        Json::json_pointer{
            "/capabilities/textDocument/completion/completionItem/documentationFormat"});

    Json types = Json::array();
    for (unsigned i{}; i <= static_cast<unsigned>(ioj::enum_max<SemanticKind>); ++i) {
        types.push_back(to_serialized_string(static_cast<SemanticKind>(i)));
    }
    return {{"capabilities",
             {{"textDocumentSync", {{"openClose", true}, {"change", 1}, {"save", true}}},
              {"positionEncoding", "utf-16"},
              {"documentSymbolProvider", true},
              {"foldingRangeProvider", true},
              {"completionProvider", {{"triggerCharacters", {"(", ":", "@", "/"}}}},
              {"hoverProvider", true},
              {"definitionProvider", true},
              {"referencesProvider", true},
              {"renameProvider",
               params.value(Json::json_pointer{"/capabilities/textDocument/rename/prepareSupport"},
                            false)
                   ? Json{{"prepareProvider", true}}
                   : Json(true)},
              {"semanticTokensProvider",
               {{"legend", {{"tokenTypes", std::move(types)}, {"tokenModifiers", {"declaration"}}}},
                {"full", true},
                {"range", true}}}}}};
}
static void
    update_document(Workspace& workspace, Method method, fs::path const& path, Json const& params) {
    auto const& doc{params.at("textDocument")};
    switch (method) {
        case Method::open:
            workspace.change(path, Buffer{doc.at("uri"), doc.at("text"), doc.at("version")});
            break;
        case Method::change: {
            auto const& changes{params.at("contentChanges")};
            if (!changes.empty() && !changes.back().contains("range")) {
                workspace.change(
                    path, Buffer{doc.at("uri"), changes.back().at("text"), doc.at("version")});
            }
            break;
        }
        case Method::close:
            workspace.change(path, {});
            break;
        case Method::save:
            workspace.invalidate(path);
            workspace.refresh();
            break;
        default:
            std::unreachable();
    }
}
static void dispatch(Workspace& workspace,
                     std::optional<Method> method,
                     Json const& params,
                     SessionState& state,
                     ClientCapabilities& capabilities,
                     Json& reply) {
    if (method == Method::initialize && state == SessionState::awaiting_initialize) {
        reply["result"] = initialize(params, capabilities);
        state = SessionState::running;
    } else if (state != SessionState::running) {
        reply["error"] = {{"code", state == SessionState::shutdown ? -32600 : -32002},
                          {"message", "server is not active"}};
    } else if (method == Method::shutdown) {
        state = SessionState::shutdown;
        reply["result"] = nullptr;
    } else if (method == Method::watched_files) {
        for (auto const& change : params.at("changes")) {
            workspace.invalidate(file_path(change.at("uri").get<std::string>()));
        }
        workspace.refresh();
    } else if (method && (is_update(*method) || is_query(*method))) {
        auto path{file_path(params.at("textDocument").at("uri").get<std::string>())};
        if (is_query(*method)) {
            auto result{workspace.query(path, *method, params, capabilities)};
            if (result) {
                reply["result"] = std::move(*result);
            } else {
                reply["error"] = {{"code", -32803}, {"message", std::move(result.error())}};
            }
        } else {
            update_document(workspace, *method, path, params);
        }
    } else if (method != Method::initialized) {
        reply["error"] = {{"code", -32601}, {"message", "unknown method"}};
    }
}
auto run() -> int {
    Workspace workspace;
    SessionState state{SessionState::awaiting_initialize};
    ClientCapabilities capabilities;
    while (auto message = read_message(std::cin)) {
        auto const& rpc{*message};
        if (!rpc.is_object() || !rpc.contains("method") || !rpc.at("method").is_string()) {
            auto const error{rpc.is_discarded() ? -32700 : -32600};
            write_message(std::cout,
                          {{"jsonrpc", "2.0"},
                           {"id", nullptr},
                           {"error", {{"code", error}, {"message", "invalid JSON-RPC request"}}}});
            continue;
        }
        auto const& spelling{rpc.at("method").get_ref<std::string const&>()};
        auto const method{try_parse_serialized_method(spelling)};
        if (method == Method::exit) {
            return state == SessionState::shutdown ? 0 : 1;
        }
        Json reply{{"jsonrpc", "2.0"}, {"id", rpc.value("id", Json{})}};
        try {
            auto const empty = Json::object();
            auto const& params{rpc.contains("params") ? rpc.at("params") : empty};
            dispatch(workspace, method, params, state, capabilities, reply);
        } catch (Json::exception const& error) {
            reply["error"] = {{"code", -32602}, {"message", error.what()}};
        } catch (std::invalid_argument const& error) {
            reply["error"] = {{"code", -32602}, {"message", error.what()}};
        } catch (std::exception const& error) {
            reply["error"] = {{"code", -32603}, {"message", error.what()}};
        }
        if (!rpc.contains("id") && reply.contains("error")) {
            std::cerr << spelling << ": " << reply["error"]["message"] << '\n';
        }
        if (rpc.contains("id")) {
            write_message(std::cout, reply);
        }
    }
    return state == SessionState::shutdown ? 0 : 1;
}
}
int main() {
#ifdef _WIN32
    // Preserve byte counts and CRLF framing on the protocol streams.
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    try {
        return lispb::lsp::run();
    } catch (std::exception const& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
