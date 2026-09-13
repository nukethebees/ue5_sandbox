#include <sandbox/level_authoring/CampaignDefinitionReader.h>

#include <native/s7/interpreter.h>
#include <native/s7/value.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <unordered_set>
#include <utility>

namespace ml::level_authoring {
namespace campaign_definition_reader_detail {
inline constexpr std::string_view campaign_prelude{R"(
(define (campaign . clauses) (cons 'campaign clauses))
(define (id value) (list 'id value))
(define (title value) (list 'title value))
(define (levels . values) (cons 'levels values))
)"};
} // namespace campaign_definition_reader_detail

namespace {
auto campaign_indexed_path(std::string const& path, std::int64_t const index) -> std::string {
    return path + "[" + std::to_string(index) + "]";
}

void lowercase_campaign_ascii(std::string& value) {
    std::ranges::transform(value, value.begin(), [](unsigned char const character) {
        if (character >= 'A' && character <= 'Z') {
            return static_cast<char>(character - 'A' + 'a');
        }
        return static_cast<char>(character);
    });
}

auto blank(std::string const& value) -> bool {
    return std::ranges::all_of(
        value, [](unsigned char const character) { return std::isspace(character) != 0; });
}

class CampaignDecoder final {
  public:
    CampaignDecoder(s7::Scheme& scheme, s7::Value const root)
        : scheme_{scheme}
        , root_{root} {}

    auto decode() -> CampaignDefinitionReadResult {
        if (!expect_tagged_list(root_, "campaign", "campaign")) {
            return {.decode_errors = std::move(errors_)};
        }

        bool has_id{};
        bool has_title{};
        bool has_levels{};
        auto const clause_count{list_length(root_) - 1};
        for (std::int64_t index{}; index < clause_count; ++index) {
            auto const clause{list_value(root_, index + 1)};
            auto const path{campaign_indexed_path("campaign", index)};
            if (!is_non_empty_list(clause)) {
                add_error(path, "Expected a campaign clause");
                continue;
            }

            auto const tag_value{list_value(clause, 0)};
            if (!s7::is_symbol(tag_value)) {
                add_error(path, "Campaign clause tag must be a symbol");
                continue;
            }

            auto const tag{std::string{s7::symbol_name(tag_value)}};
            if (tag == "id") {
                if (has_id) {
                    add_error(path, "Duplicate id clause");
                    continue;
                }
                has_id = true;
                if (expect_length(clause, 2, path)) {
                    read_symbol(list_value(clause, 1), path + ".value", definition_.id);
                }
            } else if (tag == "title") {
                if (has_title) {
                    add_error(path, "Duplicate title clause");
                    continue;
                }
                has_title = true;
                if (expect_length(clause, 2, path)) {
                    read_string(list_value(clause, 1), path + ".value", definition_.title);
                }
            } else if (tag == "levels") {
                if (has_levels) {
                    add_error(path, "Duplicate levels clause");
                    continue;
                }
                has_levels = true;
                read_levels(clause, path);
            } else {
                add_error(path, "Unknown campaign clause '" + tag + "'");
            }
        }

        if (definition_.id.empty()) {
            add_error("campaign.id", "Campaign has no stable id");
        }
        if (blank(definition_.title)) {
            add_error("campaign.title", "Campaign has no title");
        }
        if (definition_.level_ids.empty()) {
            add_error("campaign.levels", "Campaign has no levels");
        }
        if (!errors_.empty()) {
            return {.decode_errors = std::move(errors_)};
        }
        return {.definition = std::move(definition_)};
    }
  private:
    auto list_length(s7::Value const value) const -> std::int64_t {
        return s7::list_length(scheme_, value);
    }

    auto list_value(s7::Value const value, std::int64_t const index) const -> s7::Value {
        return s7::list_value(scheme_, value, index);
    }

    auto is_non_empty_list(s7::Value const value) const -> bool {
        return s7::is_list(scheme_, value) && list_length(value) > 0;
    }

    void add_error(std::string path, std::string message) {
        errors_.push_back({std::move(path), std::move(message)});
    }

    auto expect_length(s7::Value const value, std::int64_t const expected, std::string const& path)
        -> bool {
        if (!s7::is_list(scheme_, value)) {
            add_error(path, "Expected a list");
            return false;
        }
        auto const actual{list_length(value)};
        if (actual != expected) {
            add_error(path,
                      "Expected " + std::to_string(expected) + " values but found " +
                          std::to_string(actual));
            return false;
        }
        return true;
    }

    auto expect_tagged_list(s7::Value const value,
                            std::string_view const expected_tag,
                            std::string const& path) -> bool {
        if (!is_non_empty_list(value)) {
            add_error(path, "Expected a non-empty list");
            return false;
        }
        auto const tag{list_value(value, 0)};
        if (!s7::is_symbol(tag) || s7::symbol_name(tag) != expected_tag) {
            add_error(path, "Expected a '" + std::string{expected_tag} + "' value");
            return false;
        }
        return true;
    }

    auto read_symbol(s7::Value const value, std::string const& path, std::string& output) -> bool {
        if (!s7::is_symbol(value)) {
            add_error(path, "Expected a symbol");
            return false;
        }
        output = s7::symbol_name(value);
        lowercase_campaign_ascii(output);
        return true;
    }

    auto read_string(s7::Value const value, std::string const& path, std::string& output) -> bool {
        if (!s7::is_string(value)) {
            add_error(path, "Expected a string");
            return false;
        }
        output = s7::string_value(value);
        return true;
    }

    void read_levels(s7::Value const clause, std::string const& path) {
        std::unordered_set<std::string> seen;
        auto const count{list_length(clause) - 1};
        definition_.level_ids.reserve(static_cast<std::size_t>(count));
        for (std::int64_t index{}; index < count; ++index) {
            std::string id;
            auto const level_path{campaign_indexed_path(path, index)};
            if (!read_symbol(list_value(clause, index + 1), level_path, id)) {
                continue;
            }
            if (!seen.insert(id).second) {
                add_error(level_path, "Level id '" + id + "' is duplicated");
                continue;
            }
            definition_.level_ids.push_back(std::move(id));
        }
    }

    s7::Scheme& scheme_;
    s7::Value root_{};
    CampaignDefinition definition_{};
    std::vector<CampaignDefinitionDecodeError> errors_{};
};
} // namespace

CampaignDefinitionReader::CampaignDefinitionReader(std::string script_library_root)
    : script_library_root_{std::move(script_library_root)} {}

auto CampaignDefinitionReader::read_source(std::string_view const source) const
    -> CampaignDefinitionReadResult {
    s7::InterpreterOptions options;
    if (!script_library_root_.empty()) {
        options.script_library_root_utf8 = script_library_root_;
    }
    s7::Interpreter interpreter{std::move(options)};

    std::string expression;
    auto const prelude{campaign_definition_reader_detail::campaign_prelude};
    expression.reserve(prelude.size() + source.size() + 10);
    expression.append("(begin\n");
    expression.append(prelude);
    expression.append(source);
    expression.append("\n)");

    CampaignDefinitionReadResult decoded;
    auto const evaluation{interpreter.evaluate_value(
        expression, [&decoded](s7::Scheme& scheme, s7::Value const value) {
            decoded = CampaignDecoder{scheme, value}.decode();
        })};
    if (!evaluation.succeeded) {
        return {.script_error = evaluation.error};
    }
    return decoded;
}
} // namespace ml::level_authoring
