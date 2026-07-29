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

#include "edge_traits.hpp"
#include "port_traits.hpp"
#include "../node.hpp" // is_data_binding, binding_traits

namespace ugraph::detail {

    static constexpr std::size_t edge_npos = static_cast<std::size_t>(-1);

    // Uniform, *query-independent* description of one link.
    //
    // Every graph query (does port X have an input edge? how many connections? which key?)
    // used to walk the edge pack with its own recursive class template, so the per-edge work
    // was redone for every (node, spec, port) triple -- O(nodes * edges) instantiations.
    // Hoisting the per-edge facts into this single trait means it is instantiated once per
    // edge type for the whole translation unit and every query becomes a fold over
    // already-memoized constants.
    //
    // Both specializations expose the same member set so a fold can reference any field
    // regardless of link kind; fields that do not apply hold sentinel values that can
    // never compare equal to a real vertex id or port index.
    template<typename E, bool IsBinding = is_data_binding<E>::value>
    struct edge_info;

    // Node-to-node edge (std::pair<out_port, in_port>).
    template<typename E>
    struct edge_info<E, false> {
        using traits_t = edge_traits<E>;

        static constexpr bool is_binding = false;
        static constexpr bool binds_input = false;
        static constexpr bool binds_output = false;

        static constexpr std::size_t src_id = traits_t::src_id;
        static constexpr std::size_t src_port = traits_t::src_port_index;
        static constexpr std::size_t dst_id = traits_t::dst_id;
        static constexpr std::size_t dst_port = traits_t::dst_port_index;

        using src_spec = typename traits_t::src_port_t::spec_type;
        using dst_spec = typename traits_t::dst_port_t::spec_type;

        static constexpr std::size_t bind_id = edge_npos;
        static constexpr std::size_t bind_port = edge_npos;
        using bind_spec = void;
    };

    // External data binding (InDataBind / OutDataBind).
    template<typename E>
    struct edge_info<E, true> {
        using port_t = typename ::ugraph::binding_traits<E>::port_type;

        static constexpr bool is_binding = true;
        static constexpr bool binds_output = is_output_port<port_t>::value;
        static constexpr bool binds_input = is_input_port<port_t>::value && !binds_output;

        static constexpr std::size_t src_id = edge_npos;
        static constexpr std::size_t src_port = edge_npos;
        static constexpr std::size_t dst_id = edge_npos;
        static constexpr std::size_t dst_port = edge_npos;

        using src_spec = void;
        using dst_spec = void;

        static constexpr std::size_t bind_id = port_t::node_type::id();
        static constexpr std::size_t bind_port = port_t::index();
        using bind_spec = typename port_t::spec_type;
    };

} // namespace ugraph::detail
