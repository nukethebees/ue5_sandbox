#pragma once

#include <concepts>
#include <type_traits>

namespace ioj::soa_storage_detail {

template <typename Schema>
struct ViewOperations;
struct StorageState;

template <typename Owner>
using StorageSize = typename std::remove_cvref_t<Owner>::size_type;

template <typename Owner>
concept StorageOwner = std::is_base_of_v<StorageState, std::remove_cvref_t<Owner>> &&
                       requires { typename StorageSize<Owner>; };

template <typename Left, typename Right>
concept SameSoaSchema = std::same_as<typename std::remove_cvref_t<Left>::soa_schema,
                                     typename std::remove_cvref_t<Right>::soa_schema>;

template <typename Source, typename Destination, typename Size>
concept SoaSourceFor =
    SameSoaSchema<Source, Destination> &&
    requires(Source const& source) {
        { source.num() } -> std::convertible_to<Size>;
    } &&
    (std::derived_from<std::remove_cvref_t<Source>,
                       ViewOperations<typename std::remove_cvref_t<Source>::soa_schema>> ||
     std::is_base_of_v<StorageState, std::remove_cvref_t<Source>> ||
     requires(Source const& source) { source.validate(); });

template <typename Owner>
using StorageConstView = typename std::remove_cvref_t<Owner>::CompactConstView;
template <typename Owner>
using StorageView = std::conditional_t<std::is_const_v<std::remove_reference_t<Owner>>,
                                       StorageConstView<Owner>,
                                       typename std::remove_cvref_t<Owner>::CompactView>;

template <typename Owner>
concept MutableStorage = StorageOwner<Owner> && !std::is_const_v<std::remove_reference_t<Owner>>;

template <typename Owner, typename Source>
concept CopyableStorage = MutableStorage<Owner> && SoaSourceFor<Source, Owner, StorageSize<Owner>>;

template <typename Owner>
concept BorrowableStorage = StorageOwner<Owner> && std::is_lvalue_reference_v<Owner>;

} // namespace ioj::soa_storage_detail
