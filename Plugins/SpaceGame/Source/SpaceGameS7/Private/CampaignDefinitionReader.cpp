#include <SpaceGameS7/CampaignDefinitionReader.h>

#include <S7Lab/Interpreter.h>
#include <S7Lab/NativeApi.h>

#include <Containers/Set.h>
#include <Containers/StringConv.h>
#include <Misc/FileHelper.h>

namespace ml::s7 {
namespace {
namespace s7_native = S7Lab::native;

constexpr TCHAR campaign_prelude[]{LR"(
(define (campaign . clauses) (cons 'campaign clauses))
(define (id value) (list 'id value))
(define (title value) (list 'title value))
(define (levels . values) (cons 'levels values))
)"};

auto campaign_to_fstring(ANSICHAR const* const value) -> FString {
    auto const converted{FUTF8ToTCHAR{value}};
    return FString{converted.Length(), converted.Get()};
}

class FCampaignDecoder final {
  public:
    FCampaignDecoder(s7_scheme& scheme, s7_native::FValue const root)
        : scheme_{scheme}
        , root_{root} {}

    auto decode() -> FCampaignDefinitionReadResult {
        if (!expect_tagged_list(root_, TEXT("campaign"), TEXT("campaign"))) {
            return {.decode_errors = MoveTemp(errors_)};
        }

        FCampaignDefinition definition;
        bool has_id{};
        bool has_title{};
        bool has_levels{};
        auto const clause_count{list_length(root_) - 1};
        for (int64 i{0}; i < clause_count; ++i) {
            auto const clause{list_value(root_, i + 1)};
            auto const path{FString::Printf(TEXT("campaign[%lld]"), i)};
            if (!is_non_empty_list(clause)) {
                add_error(path, TEXT("Expected a campaign clause"));
                continue;
            }

            auto const tag_value{list_value(clause, 0)};
            if (!s7_native::is_symbol(tag_value)) {
                add_error(path, TEXT("Campaign clause tag must be a symbol"));
                continue;
            }

            auto const tag{campaign_to_fstring(s7_native::symbol_name(tag_value))};
            if (tag == TEXT("id")) {
                if (has_id) {
                    add_error(path, TEXT("Duplicate id clause"));
                    continue;
                }
                has_id = true;
                FName id;
                if (expect_length(clause, 2, path) &&
                    read_symbol(list_value(clause, 1), path + TEXT(".value"), id)) {
                    definition.id = FCampaignId{id};
                }
            } else if (tag == TEXT("title")) {
                if (has_title) {
                    add_error(path, TEXT("Duplicate title clause"));
                    continue;
                }
                has_title = true;
                if (expect_length(clause, 2, path)) {
                    read_string(list_value(clause, 1), path + TEXT(".value"), definition.title);
                }
            } else if (tag == TEXT("levels")) {
                if (has_levels) {
                    add_error(path, TEXT("Duplicate levels clause"));
                    continue;
                }
                has_levels = true;
                read_levels(clause, path, definition.level_ids);
            } else {
                add_error(path, FString::Printf(TEXT("Unknown campaign clause '%s'"), *tag));
            }
        }

        if (!definition.id.is_set()) {
            add_error(TEXT("campaign.id"), TEXT("Campaign has no stable id"));
        }
        if (definition.title.TrimStartAndEnd().IsEmpty()) {
            add_error(TEXT("campaign.title"), TEXT("Campaign has no title"));
        }
        if (definition.level_ids.IsEmpty()) {
            add_error(TEXT("campaign.levels"), TEXT("Campaign has no levels"));
        }
        if (!errors_.IsEmpty()) {
            return {.decode_errors = MoveTemp(errors_)};
        }

        FCampaignDefinitionReadResult result;
        result.definition.Emplace(MoveTemp(definition));
        return result;
    }
  private:
    auto list_length(s7_native::FValue const value) const -> int64 {
        return s7_native::list_length(scheme_, value);
    }

    auto list_value(s7_native::FValue const value, int64 const index) const -> s7_native::FValue {
        return s7_native::list_value(scheme_, value, index);
    }

    auto is_non_empty_list(s7_native::FValue const value) const -> bool {
        return s7_native::is_list(scheme_, value) && list_length(value) > 0;
    }

    void add_error(FString path, FString message) {
        errors_.Add({.path = MoveTemp(path), .message = MoveTemp(message)});
    }

    auto expect_length(s7_native::FValue const value, int64 const expected, FString const& path)
        -> bool {
        if (!s7_native::is_list(scheme_, value)) {
            add_error(path, TEXT("Expected a list"));
            return false;
        }
        auto const actual{list_length(value)};
        if (actual != expected) {
            add_error(
                path,
                FString::Printf(TEXT("Expected %lld values but found %lld"), expected, actual));
            return false;
        }
        return true;
    }

    auto expect_tagged_list(s7_native::FValue const value,
                            FString const& expected_tag,
                            FString const& path) -> bool {
        if (!is_non_empty_list(value)) {
            add_error(path, TEXT("Expected a non-empty list"));
            return false;
        }
        auto const tag{list_value(value, 0)};
        if (!s7_native::is_symbol(tag) ||
            campaign_to_fstring(s7_native::symbol_name(tag)) != expected_tag) {
            add_error(path, FString::Printf(TEXT("Expected a '%s' value"), *expected_tag));
            return false;
        }
        return true;
    }

    auto read_symbol(s7_native::FValue const value, FString const& path, FName& output) -> bool {
        if (!s7_native::is_symbol(value)) {
            add_error(path, TEXT("Expected a symbol"));
            return false;
        }
        output = FName{campaign_to_fstring(s7_native::symbol_name(value))};
        return true;
    }

    auto read_string(s7_native::FValue const value, FString const& path, FString& output) -> bool {
        if (!s7_native::is_string(value)) {
            add_error(path, TEXT("Expected a string"));
            return false;
        }
        output = campaign_to_fstring(s7_native::string_value(value));
        return true;
    }

    void
        read_levels(s7_native::FValue const clause, FString const& path, TArray<FLevelId>& output) {
        TSet<FLevelId> seen;
        auto const count{list_length(clause) - 1};
        output.Reserve(count);
        for (int64 i{0}; i < count; ++i) {
            FName id;
            auto const level_path{FString::Printf(TEXT("%s[%lld]"), *path, i)};
            if (!read_symbol(list_value(clause, i + 1), level_path, id)) {
                continue;
            }
            FLevelId const level_id{id};
            if (seen.Contains(level_id)) {
                add_error(level_path,
                          FString::Printf(TEXT("Level id '%s' is duplicated"), *id.ToString()));
                continue;
            }
            seen.Add(level_id);
            output.Add(level_id);
        }
    }

    s7_scheme& scheme_;
    s7_native::FValue root_{};
    TArray<FCampaignDefinitionDecodeError> errors_{};
};
}

auto FCampaignDefinitionReader::read_source(FStringView const source) const
    -> FCampaignDefinitionReadResult {
    S7Lab::FInterpreter interpreter;
    FString expression{TEXT("(begin\n")};
    expression.Append(campaign_prelude);
    expression.AppendChars(source.GetData(), source.Len());
    expression.Append(TEXT("\n)"));

    FCampaignDefinitionReadResult decoded;
    auto const evaluation{interpreter.evaluate_value(
        expression, [&decoded](s7_scheme& scheme, s7_native::FValue const value) {
            decoded = FCampaignDecoder{scheme, value}.decode();
        })};
    if (!evaluation.succeeded) {
        return {.script_error = evaluation.error};
    }
    return decoded;
}

auto FCampaignDefinitionReader::read_file(FStringView const path) const
    -> FCampaignDefinitionReadResult {
    FString source;
    auto const owned_path{FString{path}};
    if (!FFileHelper::LoadFileToString(source, *owned_path)) {
        return {.script_error =
                    FString::Printf(TEXT("Could not read campaign script '%s'."), *owned_path)};
    }
    return read_source(source);
}
}
