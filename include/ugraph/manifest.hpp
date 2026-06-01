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

    template<typename tag_t, typename data_t, std::size_t in, std::size_t out, bool strict = true>
    struct TaggedIO {
        using tag = tag_t;
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
            using tag = void;
            static constexpr std::size_t input_count = 1;
            static constexpr std::size_t output_count = 1;
            static constexpr bool strict_connection = false;
            static constexpr bool is_tagged = false;
        };

        template<typename data_t, std::size_t in, std::size_t out, bool strict>
        struct io_traits<IO<data_t, in, out, strict>> {
            using type = data_t;
            using tag = void;
            static constexpr std::size_t input_count = in;
            static constexpr std::size_t output_count = out;
            static constexpr bool strict_connection = strict;
            static constexpr bool is_tagged = false;
        };

        template<typename tag_t, typename data_t, std::size_t in, std::size_t out, bool strict>
        struct io_traits<TaggedIO<tag_t, data_t, in, out, strict>> {
            using type = data_t;
            using tag = tag_t;
            static constexpr std::size_t input_count = in;
            static constexpr std::size_t output_count = out;
            static constexpr bool strict_connection = strict;
            static constexpr bool is_tagged = true;
        };

        template<typename io_t>
        struct io_key {
            using type = std::conditional_t<io_traits<io_t>::is_tagged, typename io_traits<io_t>::tag, typename io_traits<io_t>::type>;
        };

        template<typename key_t, typename io_t>
        struct io_matches_key : std::bool_constant<
            std::is_same_v<key_t, io_t> ||
            std::is_same_v<key_t, typename io_traits<io_t>::tag> ||
            std::is_same_v<key_t, typename io_key<io_t>::type>
        > {};

        template<typename key_t, typename list_t>
        struct io_entry_for;

        template<typename key_t>
        struct io_entry_for<key_t, detail::type_list<>> {
            using type = void;
        };

        template<typename key_t, typename io_t, typename... rest_t>
        struct io_entry_for<key_t, detail::type_list<io_t, rest_t...>> {
            using type = std::conditional_t<
                io_matches_key<key_t, io_t>::value,
                io_t,
                typename io_entry_for<key_t, detail::type_list<rest_t...>>::type
            >;
        };

        template<typename key_t, typename list_t>
        struct io_entry_count;

        template<typename key_t>
        struct io_entry_count<key_t, detail::type_list<>> : std::integral_constant<std::size_t, 0> {};

        template<typename key_t, typename io_t, typename... rest_t>
        struct io_entry_count<key_t, detail::type_list<io_t, rest_t...>>
            : std::integral_constant<
                std::size_t,
                (io_matches_key<key_t, io_t>::value ? 1 : 0) + io_entry_count<key_t, detail::type_list<rest_t...>>::value
            > {};

        template<typename key_t, typename... io_t>
        static constexpr std::size_t io_entry_index() {
            std::size_t result = static_cast<std::size_t>(-1);
            std::size_t index = 0;
            (((result == static_cast<std::size_t>(-1) && io_matches_key<key_t, io_t>::value)
                ? (result = index, void())
                : void(), ++index), ...);
            return result;
        }


    } // namespace detail

    template<typename... ios_t>
    struct Manifest {

        using specs_list = detail::type_list<ios_t...>;
        using data_types_list = detail::type_list<typename detail::io_traits<ios_t>::type...>;

        static constexpr std::size_t spec_count = detail::type_list_size<specs_list>::value;
        static constexpr std::size_t type_count = detail::type_list_size<data_types_list>::value;

        template<typename T>
        static constexpr bool contains = !std::is_same_v<typename detail::io_entry_for<T, specs_list>::type, void>;

        template<typename Tag>
        static constexpr bool contains_tag = !std::is_same_v<typename detail::io_entry_for<Tag, specs_list>::type, void>;

        template<typename T>
        using spec_for = typename detail::io_entry_for<T, specs_list>::type;

        template<typename T>
        static constexpr std::size_t spec_count_for_value = detail::io_entry_count<T, specs_list>::value;

        template<typename Tag>
        static constexpr std::size_t spec_count_for_tag = detail::io_entry_count<Tag, specs_list>::value;

        template<typename T>
        static constexpr std::size_t index() {
            static_assert(contains<T>, "Type not declared in Manifest");
            constexpr std::size_t result = detail::io_entry_index<T, ios_t...>();
            static_assert(result != static_cast<std::size_t>(-1), "Type or tag not found in Manifest");
            return result;
        }

        template<typename T>
        static constexpr std::size_t input_count() {
            static_assert(contains<T>, "Type not declared in Manifest");
            using spec = spec_for<T>;
            return detail::io_traits<spec>::input_count;
        }

        template<typename T>
        static constexpr std::size_t output_count() {
            static_assert(contains<T>, "Type not declared in Manifest");
            using spec = spec_for<T>;
            return detail::io_traits<spec>::output_count;
        }

        template<typename T>
        static constexpr bool strict_connection() {
            static_assert(contains<T>, "Type not declared in Manifest");
            using spec = spec_for<T>;
            return detail::io_traits<spec>::strict_connection;
        }

        template<typename T>
        using data_type_for = typename detail::io_traits<spec_for<T>>::type;

        template<typename T>
        using key_for = typename detail::io_key<spec_for<T>>::type;

        template<std::size_t I>
        using spec_at = typename detail::type_list_at<I, specs_list>::type;

        template<std::size_t I>
        using key_at = typename detail::io_key<spec_at<I>>::type;

        template<std::size_t I>
        using type_at = typename detail::type_list_at<I, data_types_list>::type;
    };

} // namespace ugraph
