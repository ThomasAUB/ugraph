#pragma once

#include <cstddef>
#include <type_traits>

namespace ugraph::detail {

    template<typename... Ts>
    struct type_list {};

    template<std::size_t N, typename List>
    struct type_list_at; // primary template

    template<std::size_t N, typename T, typename... Ts>
    struct type_list_at<N, type_list<T, Ts...>> : type_list_at<N - 1, type_list<Ts...>> {};

    template<typename T, typename... Ts>
    struct type_list_at<0, type_list<T, Ts...>> { using type = T; };

    template<typename List>
    struct type_list_size;

    template<typename... Ts>
    struct type_list_size<type_list<Ts...>> : std::integral_constant<std::size_t, sizeof...(Ts)> {};

} // namespace ugraph::detail
