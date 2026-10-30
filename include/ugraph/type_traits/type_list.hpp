/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * MIT License                                                                     *
 *                                                                                 *
 * Copyright (c) 2026 Thomas AUBERT                                                *
 *                                                                                 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy    *
 * of this software and associated documentation files (the "Software"), to deal   *
 * in the Software without restriction, including without limitation the rights    *
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell       *
 * copies of the Software, and to permit persons to whom the Software is           *
 * furnished to do so, subject to the following conditions:                        *
 *                                                                                 *
 * The above copyright notice and this permission notice shall be included in all  *
 * copies or substantial portions of the Software.                                 *
 *                                                                                 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR      *
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,        *
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE     *
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER          *
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,   *
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE   *
 * SOFTWARE.                                                                       *
 *                                                                                 *
 * github : https://github.com/ThomasAUB/ugraph                                    *
 *                                                                                 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#pragma once

#include <cstddef>
#include <utility>
#include <type_traits>

// __type_pack_element indexes a pack in O(1) with zero template instantiations.
// Available on clang and on GCC >= 14; the portable fallback below is used elsewhere.
#if defined(__has_builtin)
#  if __has_builtin(__type_pack_element)
#    define UGRAPH_HAS_TYPE_PACK_ELEMENT 1
#  endif
#endif

namespace ugraph::detail {

    template<typename... Ts>
    struct type_list {};

#ifdef UGRAPH_HAS_TYPE_PACK_ELEMENT

    template<std::size_t N, typename List>
    struct type_list_at; // primary template

    template<std::size_t N, typename... Ts>
    struct type_list_at<N, type_list<Ts...>> {
        static_assert(N < sizeof...(Ts), "type_list_at index out of range");
        using type = __type_pack_element<N, Ts...>;
    };

#else

    // Portable fallback: build one indexed-base map per list (shared by every index)
    // and resolve each lookup with a single deduction, instead of walking an
    // O(N) recursive inheritance chain per index.
    template<std::size_t I, typename T>
    struct type_list_slot {};

    template<typename Seq, typename... Ts>
    struct type_list_slot_map;

    template<std::size_t... Is, typename... Ts>
    struct type_list_slot_map<std::index_sequence<Is...>, Ts...> : type_list_slot<Is, Ts>... {};

    template<std::size_t I, typename T>
    static type_list_slot<I, T> type_list_slot_probe(type_list_slot<I, T>*);

    template<typename Slot>
    struct type_list_slot_value;

    template<std::size_t I, typename T>
    struct type_list_slot_value<type_list_slot<I, T>> { using type = T; };

    template<std::size_t N, typename List>
    struct type_list_at; // primary template

    template<std::size_t N, typename... Ts>
    struct type_list_at<N, type_list<Ts...>> {
        static_assert(N < sizeof...(Ts), "type_list_at index out of range");
        using map_t = type_list_slot_map<std::make_index_sequence<sizeof...(Ts)>, Ts...>;
        using type = typename type_list_slot_value<
            decltype(type_list_slot_probe<N>(static_cast<map_t*>(nullptr)))
        >::type;
    };

#endif

    template<typename List>
    struct type_list_size;

    template<typename... Ts>
    struct type_list_size<type_list<Ts...>> : std::integral_constant<std::size_t, sizeof...(Ts)> {};

    static constexpr std::size_t type_list_npos = static_cast<std::size_t>(-1);

    template<typename T, typename List>
    struct type_list_index;

    template<typename T, typename... Ts>
    struct type_list_index<T, type_list<Ts...>> {
        // One constexpr scan rather than one class instantiation per element.
        static constexpr std::size_t find() {
            std::size_t i = 0;
            std::size_t found = type_list_npos;
            (((found == type_list_npos && std::is_same_v<T, Ts>) ? (found = i, void()) : void(), ++i), ...);
            return found;
        }
        static constexpr std::size_t value = find();
        static_assert(value != type_list_npos, "type_list_index<T>: T was not found in the type_list — this is an internal ugraph error; the lookup should have been guarded by a contains check");
    };

    template<typename T, typename List>
    struct type_list_prepend;

    template<typename T, typename... Ts>
    struct type_list_prepend<T, type_list<Ts...>> { using type = type_list<T, Ts...>; };

    template<typename... Lists>
    struct type_list_cat;

    template<>
    struct type_list_cat<> { using type = type_list<>; };

    template<typename... Ts>
    struct type_list_cat<type_list<Ts...>> { using type = type_list<Ts...>; };

    template<typename... A, typename... B, typename... Rest>
    struct type_list_cat<type_list<A...>, type_list<B...>, Rest...> {
        using type = typename type_list_cat<type_list<A..., B...>, Rest...>::type;
    };

} // namespace ugraph::detail

