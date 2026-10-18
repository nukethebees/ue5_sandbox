#pragma once
#include <ioj/layout/document_id.hpp>
#include <ioj/layout/revision.hpp>
#include <ioj/layout/transaction_kind.hpp>
#include <ioj/layout/variant_id.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lispb::codegen {
struct Manifest;
}

namespace ioj::layout {
class Document;
class EditorSession;
class DocumentState;
// Names resolve in instruction order. A transaction belongs to its creating document revision.
class Transaction {
  public:
    void set_field_bits(std::string_view module,
                        std::string_view declaration,
                        std::string_view field,
                        int bits);
    void set_packed_storage(std::string_view module,
                            std::string_view declaration,
                            std::string_view type);
    void set_member_type(std::string_view module,
                         std::string_view declaration,
                         std::string_view member,
                         std::string_view type);
    void create_declaration(std::string_view module, std::string_view source);
    void replace_declaration(std::string_view module,
                             std::string_view name,
                             std::string_view source);
    void delete_declaration(std::string_view module, std::string_view name);
    void rename_declaration(std::string_view module,
                            std::string_view name,
                            std::string_view new_name);
    void duplicate_declaration(std::string_view module,
                               std::string_view name,
                               std::string_view new_name);
    void move_declaration(std::string_view module,
                          std::string_view name,
                          std::string_view destination);
    void create_module(std::string_view source, std::string_view path = "document.lispb");
    void delete_module(std::string_view module);
    void rename_module(std::string_view module, std::string_view new_name);
    void add_member(std::string_view module,
                    std::string_view declaration,
                    std::string_view name,
                    std::string_view type);
    void
        delete_member(std::string_view module, std::string_view declaration, std::string_view name);
    void rename_member(std::string_view module,
                       std::string_view declaration,
                       std::string_view name,
                       std::string_view new_name);
    void reorder_member(std::string_view module,
                        std::string_view declaration,
                        std::string_view name,
                        std::size_t position);
    void set_member_count(std::string_view module,
                          std::string_view declaration,
                          std::string_view name,
                          std::optional<std::uint64_t> count);
    void add_field(std::string_view module,
                   std::string_view declaration,
                   std::string_view name,
                   std::string_view type,
                   int bits);
    void set_field_type(std::string_view module,
                        std::string_view declaration,
                        std::string_view name,
                        std::string_view type);
    void delete_field(std::string_view module, std::string_view declaration, std::string_view name);
    void rename_field(std::string_view module,
                      std::string_view declaration,
                      std::string_view name,
                      std::string_view new_name);
    void reorder_field(std::string_view module,
                       std::string_view declaration,
                       std::string_view name,
                       std::size_t position);
    void add_enumerator(std::string_view module,
                        std::string_view declaration,
                        std::string_view name,
                        std::string_view initializer = {});
    void delete_enumerator(std::string_view module,
                           std::string_view declaration,
                           std::string_view name);
    void rename_enumerator(std::string_view module,
                           std::string_view declaration,
                           std::string_view name,
                           std::string_view new_name);
    void set_enumerator_initializer(std::string_view module,
                                    std::string_view declaration,
                                    std::string_view name,
                                    std::string_view initializer);
    void reorder_enumerator(std::string_view module,
                            std::string_view declaration,
                            std::string_view name,
                            std::size_t position);
    void add_source(std::string_view path, std::string_view source);
    void replace_source(std::string_view path, std::string_view source);
    void remove_source(std::string_view path);
    void rename_source(std::string_view path, std::string_view new_path);
    void create_variant(std::string_view name);
    void duplicate_variant(VariantId id, std::string_view name);
    void rename_variant(VariantId id, std::string_view name);
    void delete_variant(VariantId id);
    void reset_variant(VariantId id);
    void set_variant_packed_storage(VariantId id,
                                    std::string_view module,
                                    std::string_view declaration,
                                    std::optional<std::string> type);
    void set_variant_field_bits(VariantId id,
                                std::string_view module,
                                std::string_view declaration,
                                std::string_view field,
                                std::optional<std::uint32_t> bits);
    void set_variant_member_type(VariantId id,
                                 std::string_view module,
                                 std::string_view declaration,
                                 std::string_view member,
                                 std::optional<std::string> type);
    void set_variant_capacity(VariantId id,
                              std::string_view module,
                              std::string_view declaration,
                              std::optional<std::uint64_t> capacity);
  private:
    friend class Document;
    Transaction(EditorSession const& session, DocumentId owner, Revision revision);
    void append(TransactionKind kind,
                std::string_view module,
                std::string_view declaration,
                std::string_view member,
                std::string_view text,
                std::optional<std::uint64_t> number = 0);
    void apply(DocumentState& state, lispb::codegen::Manifest& manifest) const;
    EditorSession const* session_;
    DocumentId owner_;
    Revision revision_;
    std::vector<TransactionKind> kinds_;
    std::vector<std::string> modules_;
    std::vector<std::string> declarations_;
    std::vector<std::string> members_;
    std::vector<std::string> texts_;
    std::vector<std::optional<std::uint64_t>> numbers_;
    std::vector<VariantId> variant_ids_;
};

}
