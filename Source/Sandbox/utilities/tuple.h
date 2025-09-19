#include <tuple>
#include <type_traits>
#include <utility>

namespace ml {
    template<typename T, typename Tuple>
    struct tuple_type_index;

    // Match found
    template<typename T, typename... Types>
    struct tuple_type_index<T, std::tuple<T, Types...>> {
        static constexpr std::size_t value = 0;
    };

    // Keep searching
    template<typename T, typename U, typename... Types>
    struct tuple_type_index<T, std::tuple<U, Types...>> {
        static constexpr std::size_t value = 1 + tuple_type_index<T, std::tuple<Types...>>::value;
    };

    template<typename T, typename Tuple>
    constexpr std::size_t tuple_type_index_v = tuple_type_index<T, Tuple>::value;


    // Not found — triggers error
    template<typename T>
    struct tuple_type_index<T, std::tuple<>> {
        static_assert(sizeof(T) == 0, "Type not found in tuple");
    };
}