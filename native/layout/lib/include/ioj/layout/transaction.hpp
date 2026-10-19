#pragma once
#include <ioj/layout/analysis_settings.hpp>
#include <ioj/layout/document_id.hpp>
#include <ioj/layout/instruction_id.hpp>
#include <ioj/layout/revision.hpp>
#include <ioj/layout/transaction_kind.hpp>
#include <ioj/layout/variant_id.hpp>

#include <lispb/indices.h>

#include <cstdint>
#include <optional>
#include <span>
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
// Names resolve when executed, allowing transactions to be queued in document order.
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
    void rename_source(std::string_view path, std::string_view destination);
    void create_variant(std::string_view name);
    void duplicate_variant(VariantId id, std::string_view name);
    void rename_variant(VariantId id, std::string_view name);
    void delete_variant(VariantId id);
    void reset_variant(VariantId id);
    void set_variant_packed_storage(VariantId id,
                                    std::string_view module,
                                    std::string_view declaration,
                                    std::optional<std::string> const& type);
    void set_variant_field_bits(VariantId id,
                                std::string_view module,
                                std::string_view declaration,
                                std::string_view field,
                                std::optional<std::uint32_t> bits);
    void set_variant_member_type(VariantId id,
                                 std::string_view module,
                                 std::string_view declaration,
                                 std::string_view member,
                                 std::optional<std::string> const& type);
    void set_variant_capacity(VariantId id,
                              std::string_view module,
                              std::string_view declaration,
                              std::optional<std::uint64_t> capacity);
    void set_element_count(std::uint64_t value);
    void set_default_soa_capacity(std::uint64_t value);
    void set_access_operation(AccessOperation value);
    void set_access_multiplicity(std::uint64_t value);
    void set_soa_allocation_strategy(SoaAllocationStrategy value);
    void set_abi_profile(AbiProfile const& profile);
    void set_comparison_abi_profile(std::optional<AbiProfile> const& profile);
    void set_access_set(std::string_view module,
                        std::string_view declaration,
                        AccessSet const& accesses);
    void set_varint_distribution(std::string_view module,
                                 std::string_view declaration,
                                 std::span<IntegerVarintDistributionEntry const> entries);
    void set_union_distribution(std::string_view module,
                                std::string_view declaration,
                                std::span<UnionDistributionEntry const> entries);
    void set_tagged_union_distribution(std::string_view module,
                                       std::string_view declaration,
                                       std::span<TaggedUnionDistributionEntry const> entries);
  private:
    friend class Document;
    friend class DocumentState;
    Transaction(EditorSession& session, DocumentId owner);
    void append(TransactionKind kind,
                std::string_view module,
                std::string_view declaration,
                std::string_view member,
                std::string_view text,
                std::optional<std::uint64_t> number = 0);
    void apply(DocumentState& state, lispb::codegen::Manifest& manifest, std::size_t end) const;
    void append(Transaction const& transaction);
    void truncate(std::size_t count);
    EditorSession* session_;
    DocumentId owner_;
    std::vector<TransactionKind> kinds_;
    std::vector<InstructionId> ids_;
    std::vector<lispb::IdentifierId> modules_;
    std::vector<lispb::IdentifierId> declarations_;
    std::vector<lispb::IdentifierId> members_;
    std::vector<lispb::StringId> texts_;
    std::vector<std::optional<std::uint64_t>> numbers_;
    std::vector<VariantId> variant_ids_;
    std::vector<std::size_t> payload_offsets_;
    std::vector<std::size_t> payload_counts_;
    std::vector<lispb::IdentifierId> payload_names_;
    std::vector<std::uint64_t> payload_weights_;
    std::vector<lispb::codegen::PackedIntegerValue> payload_values_;
};

}
