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

    template<typename T, typename List>
    struct type_list_index;

    template<typename T, typename... Ts>
    struct type_list_index<T, type_list<T, Ts...>> : std::integral_constant<std::size_t, 0> {};

    template<typename T, typename U, typename... Ts>
    struct type_list_index<T, type_list<U, Ts...>>
        : std::integral_constant<std::size_t, 1 + type_list_index<T, type_list<Ts...>>::value> {};

    template<typename T>
    struct type_list_index<T, type_list<>> {
        static_assert(!std::is_same_v<T, T>, "Type not found in type_list");
    };

    template<typename T, typename List>
    struct type_list_prepend;

    template<typename T, typename... Ts>
    struct type_list_prepend<T, type_list<Ts...>> { using type = type_list<T, Ts...>; };

} // namespace ugraph::detail
