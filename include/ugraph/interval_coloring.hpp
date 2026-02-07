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

#include <array>
#include <utility>
#include <type_traits>

#include "topology.hpp"
#include "type_list.hpp"
#include "producer_tag.hpp"

namespace ugraph {

    // Interval coloring helper: computes buffer reuse, input/output counts,
    // and producer/input lookups independently of GraphView.
    template<typename... edges_t>
    class IntervalColoring {
        using topology_t = Topology<edges_t...>;
        static_assert(!topology_t::is_cyclic(), "Cycle detected in graph definition");

        template<typename E>
        struct edge_traits {
            using edge_t = std::decay_t<E>;
            using src_port_t = typename edge_t::first_type;
            using dst_port_t = typename edge_t::second_type;
            using src_vertex_t = typename src_port_t::node_type;
            using dst_vertex_t = typename dst_port_t::node_type;
            static constexpr std::size_t src_id = src_vertex_t::id();
            static constexpr std::size_t dst_id = dst_vertex_t::id();
            static constexpr std::size_t src_port_index = src_port_t::index();
            static constexpr std::size_t dst_port_index = dst_port_t::index();
        };

        template<std::size_t _vid, std::size_t _port>
        using producer_tag = ugraph::producer_tag<_vid, _port>;
        template<typename List, typename Tag> struct append_unique;
        template<typename Tag, typename... Ts>
        struct append_unique<detail::type_list<Ts...>, Tag> {
            static constexpr bool exists = ((Tag::vid == Ts::vid && Tag::port == Ts::port) || ... || false);
            using type = std::conditional_t<exists, detail::type_list<Ts...>, detail::type_list<Ts..., Tag>>;
        };
        template<typename List, typename Edge> struct add_edge_prod {
            using tag = producer_tag<edge_traits<Edge>::src_id, edge_traits<Edge>::src_port_index>;
            using type = typename append_unique<List, tag>::type;
        };
        template<typename List, typename... Es> struct fold_prod;
        template<typename List> struct fold_prod<List> { using type = List; };
        template<typename List, typename E, typename... R>
        struct fold_prod<List, E, R...> { using type = typename fold_prod<typename add_edge_prod<List, E>::type, R...>::type; };
        using producer_list = typename fold_prod<detail::type_list<>, edges_t...>::type;

        static constexpr std::size_t producer_count = detail::type_list_size<producer_list>::value;

        static constexpr std::size_t id_to_pos(std::size_t id) {
            auto ids = topology_t::ids();
            for (std::size_t i = 0; i < topology_t::size(); ++i) if (ids[i] == id) return i;
            return (std::size_t) -1;
        }

        template<std::size_t VID, std::size_t PORT, std::size_t I>
        struct find_prod_index_impl {
            using PT = typename detail::type_list_at<I, producer_list>::type;
            static constexpr std::size_t value = (PT::vid == VID && PT::port == PORT) ? I : find_prod_index_impl<VID, PORT, I + 1>::value;
        };
        template<std::size_t VID, std::size_t PORT>
        struct find_prod_index_impl<VID, PORT, producer_count> { static constexpr std::size_t value = (std::size_t) -1; };

        struct lifetimes_t {
            std::array<std::size_t, producer_count == 0 ? 1 : producer_count> start {}, end {};
        };

        template<std::size_t... I>
        static constexpr void init_lifetimes_indices(lifetimes_t& l, std::index_sequence<I...>) {
            ((l.start[I] = id_to_pos(detail::type_list_at<I, producer_list>::type::vid), l.end[I] = l.start[I]), ...);
        }

        static constexpr lifetimes_t build_lifetimes() {
            lifetimes_t lt {};
            if constexpr (producer_count > 0) {
                init_lifetimes_indices(lt, std::make_index_sequence<producer_count>{});

                ([&] () {
                    using ET = edge_traits<edges_t>;
                    constexpr std::size_t idx = find_prod_index_impl<ET::src_id, ET::src_port_index, 0>::value;
                    const std::size_t dpos = id_to_pos(ET::dst_id);
                    if (dpos > lt.end[idx]) {
                        lt.end[idx] = dpos;
                    }
                    }(), ...);
            }
            return lt;
        }

        static constexpr lifetimes_t lifetimes = build_lifetimes();

        struct assignment_t {
            std::array<std::size_t, producer_count == 0 ? 1 : producer_count> buf {}; std::size_t count {};
        };

        static constexpr assignment_t build_assignment() {
            assignment_t a {};
            if constexpr (producer_count > 0) {
                std::array<std::size_t, producer_count> order {};
                for (std::size_t i = 0; i < producer_count; ++i) {
                    order[i] = i;
                }
                for (std::size_t i = 0; i < producer_count; ++i) {
                    std::size_t best = i;
                    for (std::size_t j = i + 1; j < producer_count; ++j) {
                        if (lifetimes.start[order[j]] < lifetimes.start[order[best]]) {
                            best = j;
                            if (best != i) {
                                auto tmp = order[i];
                                order[i] = order[best];
                                order[best] = tmp;
                            }
                        }
                    }
                }
                std::array<std::size_t, producer_count> buffer_end {};
                std::size_t buffers = 0;
                for (std::size_t k = 0; k < producer_count; ++k) {
                    auto p = order[k];
                    auto s = lifetimes.start[p];
                    auto e = lifetimes.end[p];
                    std::size_t reuse = buffers;
                    for (std::size_t b = 0; b < buffers; ++b) {
                        if (buffer_end[b] < s) {
                            reuse = b;
                            break;
                        }
                    }
                    if (reuse == buffers) {
                        buffer_end[buffers] = e;
                        a.buf[p] = buffers;
                        ++buffers;
                    }
                    else {
                        buffer_end[reuse] = e;
                        a.buf[p] = reuse;
                    }
                }
                a.count = buffers;
            }
            else {
                a.count = 0;
            }
            return a;
        }

        static constexpr assignment_t assignment = build_assignment();

        template<std::size_t DVID, std::size_t DPORT, typename... Es>
        struct find_input_edge_impl;

        template<std::size_t DVID, std::size_t DPORT>
        struct find_input_edge_impl<DVID, DPORT> {
            static constexpr std::size_t src_vid = (std::size_t) -1;
            static constexpr std::size_t src_port = (std::size_t) -1;
        };

        template<std::size_t DVID, std::size_t DPORT, typename E0, typename... Rest>
        struct find_input_edge_impl<DVID, DPORT, E0, Rest...> {
            using tr = edge_traits<E0>;
            static constexpr bool match = (tr::dst_id == DVID && tr::dst_port_index == DPORT);
            static constexpr std::size_t src_vid = match ? tr::src_id : find_input_edge_impl<DVID, DPORT, Rest...>::src_vid;
            static constexpr std::size_t src_port = match ? tr::src_port_index : find_input_edge_impl<DVID, DPORT, Rest...>::src_port;
        };

        template<std::size_t DVID, std::size_t DPORT>
        struct find_input_edge : find_input_edge_impl<DVID, DPORT, edges_t...> {};

        template<std::size_t VID, std::size_t PORT>
        static constexpr std::size_t data_index_for_output() {
            constexpr std::size_t pidx = find_prod_index_impl<VID, PORT, 0>::value;
            static_assert(pidx != (std::size_t) -1, "(vertex id, output port) not a producer in this graph");
            return assignment.buf[pidx];
        }

        template<std::size_t VID, std::size_t PORT>
        static constexpr std::size_t data_index_for_input() {
            constexpr std::size_t src_vid = find_input_edge<VID, PORT>::src_vid;
            constexpr std::size_t src_port = find_input_edge<VID, PORT>::src_port;
            static_assert(src_vid != (std::size_t) -1, "No edge found feeding (vertex id, input port)");
            return data_index_for_output<src_vid, src_port>();
        }

        template<std::size_t VID, std::size_t... P>
        static constexpr std::size_t missing_inputs_impl(std::index_sequence<P...>) {
            return ((find_input_edge<VID, P>::src_vid == (std::size_t) -1 ? 1 : 0) + ... + 0);
        }

        template<std::size_t VID>
        static constexpr std::size_t missing_inputs_for_vertex() {
            using V = typename topology_t::template find_type_by_id<VID>::type;
            if constexpr (V::input_count() == 0) return 0;
            else return missing_inputs_impl<VID>(std::make_index_sequence<V::input_count()>{});
        }

        template<std::size_t... I>
        static constexpr std::size_t compute_input_count_impl(std::index_sequence<I...>) {
            return (missing_inputs_for_vertex<topology_t::template id_at<I>()>() + ... + 0);
        }

        template<std::size_t VID, std::size_t... P>
        static constexpr std::size_t missing_outputs_impl(std::index_sequence<P...>) {
            return ((find_prod_index_impl<VID, P, 0>::value == (std::size_t) -1 ? 1 : 0) + ... + 0);
        }

        template<std::size_t VID>
        static constexpr std::size_t missing_outputs_for_vertex() {
            using V = typename topology_t::template find_type_by_id<VID>::type;
            if constexpr (V::output_count() == 0) return 0;
            else return missing_outputs_impl<VID>(std::make_index_sequence<V::output_count()>{});
        }

        template<std::size_t... I>
        static constexpr std::size_t compute_output_count_impl(std::index_sequence<I...>) {
            return (missing_outputs_for_vertex<topology_t::template id_at<I>()>() + ... + 0);
        }

    public:
        static constexpr std::size_t data_instance_count() { return assignment.count; }
        static constexpr std::size_t input_count() {
            return compute_input_count_impl(std::make_index_sequence<topology_t::size()>{});
        }
        static constexpr std::size_t output_count() {
            return compute_output_count_impl(std::make_index_sequence<topology_t::size()>{});
        }
        template<std::size_t VID, std::size_t PORT>
        static constexpr std::size_t output_data_index() { return data_index_for_output<VID, PORT>(); }

        template<std::size_t VID, std::size_t PORT>
        static constexpr std::size_t input_data_index() { return data_index_for_input<VID, PORT>(); }
    };

} // namespace ugraph
