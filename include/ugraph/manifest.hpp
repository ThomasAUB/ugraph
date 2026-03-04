/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
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
#include "type_traits/type_list.hpp"

namespace ugraph {

    template<typename data_t, std::size_t in, std::size_t out, bool strict = true>
    struct IO {
        using type = data_t;
        static constexpr std::size_t input_count = in;
        static constexpr std::size_t output_count = out;
        static constexpr bool strict_connection = strict;
    };

    template<typename... ios_t>
    struct Manifest;

    namespace detail {

        template<typename io_t>
        struct io_traits {
            using type = io_t;
            static constexpr std::size_t input_count = 1;
            static constexpr std::size_t output_count = 1;
            static constexpr bool strict_connection = false;
        };

        template<typename data_t, std::size_t in, std::size_t out, bool strict>
        struct io_traits<IO<data_t, in, out, strict>> {
            using type = data_t;
            static constexpr std::size_t input_count = in;
            static constexpr std::size_t output_count = out;
            static constexpr bool strict_connection = strict;
        };

        template<typename data_t, typename list_t>
        struct io_entry_for;

        template<typename data_t>
        struct io_entry_for<data_t, detail::type_list<>> {
            using type = void;
        };

        template<typename data_t, typename io_t, typename... rest_t>
        struct io_entry_for<data_t, detail::type_list<io_t, rest_t...>> {
            using traits = io_traits<io_t>;
            using type = std::conditional_t<
                std::is_same_v<data_t, typename traits::type>,
                io_t,
                typename io_entry_for<data_t, detail::type_list<rest_t...>>::type
            >;
        };


    } // namespace detail

    template<typename... ios_t>
    struct Manifest {

        using specs_list = detail::type_list<ios_t...>;
        using data_types_list = detail::type_list<typename detail::io_traits<ios_t>::type...>;

        static constexpr std::size_t type_count = detail::type_list_size<data_types_list>::value;

        template<typename T>
        static constexpr bool contains = !std::is_same_v<typename detail::io_entry_for<T, specs_list>::type, void>;

        template<typename T>
        static constexpr std::size_t index() {
            static_assert(contains<T>, "Type not declared in Manifest");
            return detail::type_list_index<T, data_types_list>::value;
        }

        template<typename T>
        static constexpr std::size_t input_count() {
            static_assert(contains<T>, "Type not declared in Manifest");
            using spec = typename detail::io_entry_for<T, specs_list>::type;
            return detail::io_traits<spec>::input_count;
        }

        template<typename T>
        static constexpr std::size_t output_count() {
            static_assert(contains<T>, "Type not declared in Manifest");
            using spec = typename detail::io_entry_for<T, specs_list>::type;
            return detail::io_traits<spec>::output_count;
        }

        template<typename T>
        static constexpr bool strict_connection() {
            static_assert(contains<T>, "Type not declared in Manifest");
            using spec = typename detail::io_entry_for<T, specs_list>::type;
            return detail::io_traits<spec>::strict_connection;
        }

        template<std::size_t I>
        using type_at = typename detail::type_list_at<I, data_types_list>::type;
    };

} // namespace ugraph
