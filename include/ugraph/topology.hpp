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

#include <tuple>
#include <array>
#include <cstddef>
#include <utility>
#include <type_traits>

#include "graph_printer.hpp"
#include "type_traits/type_list.hpp"
#include "type_traits/edge_traits.hpp"

namespace ugraph {

    template<typename... edges_t>
    class Topology {

        template<typename A, typename B>
        struct same_vertex_identity : std::bool_constant<
            (A::id() == B::id()) &&
            std::is_same_v<typename A::module_type, typename B::module_type> &&
            (A::priority() == B::priority())
        > {};

        template<typename List, typename V> struct list_append_unique;
        template<typename V, typename... Ts>
        struct list_append_unique<detail::type_list<Ts...>, V> {
            static constexpr bool exists = (same_vertex_identity<V, Ts>::value || ... || false);
            using type = std::conditional_t<exists, detail::type_list<Ts...>, detail::type_list<Ts..., V>>;
        };

        template<typename List, typename Edge>
        struct list_add_edge_vertices {
            using src_t = typename detail::edge_traits<Edge>::src_vertex_t;
            using dst_t = typename detail::edge_traits<Edge>::dst_vertex_t;
            using with_src = typename list_append_unique<List, src_t>::type;
            using type = typename list_append_unique<with_src, dst_t>::type;
        };
        template<typename List, typename... Edges> struct fold_edges;
        template<typename List> struct fold_edges<List> { using type = List; };
        template<typename List, typename E, typename... R>
        struct fold_edges<List, E, R...> { using type = typename fold_edges<typename list_add_edge_vertices<List, E>::type, R...>::type; };
        using vertex_types_list = typename fold_edges<detail::type_list<>, edges_t...>::type;

        static constexpr std::size_t vertex_count = detail::type_list_size<vertex_types_list>::value;

        // Collect vertex ids in declared topological order (before sorting)
        template<std::size_t... I>
        static constexpr auto make_vertex_ids(std::index_sequence<I...>) {
            return std::array<std::size_t, sizeof...(I)>{ detail::type_list_at<I, vertex_types_list>::type::id()... };
        }
        static constexpr auto vertex_ids = make_vertex_ids(std::make_index_sequence<vertex_count>{});

        static constexpr bool has_duplicate_vertex_ids() {
            for (std::size_t i = 0; i < vertex_count; ++i) {
                for (std::size_t j = i + 1; j < vertex_count; ++j) {
                    if (vertex_ids[i] == vertex_ids[j]) {
                        return true;
                    }
                }
            }
            return false;
        }

        static_assert(!has_duplicate_vertex_ids(), "Duplicate node ids detected in topology");

        template<std::size_t... I>
        static constexpr auto make_vertex_priorities(std::index_sequence<I...>) {
            return std::array<std::size_t, sizeof...(I)>{ detail::type_list_at<I, vertex_types_list>::type::priority()... };
        }
        static constexpr auto vertex_priorities = make_vertex_priorities(std::make_index_sequence<vertex_count>{});

        template<std::size_t... I>
        static constexpr auto make_edges_ids_impl(std::index_sequence<I...>) {
            return std::array<std::pair<std::size_t, std::size_t>, sizeof...(I)>{
                std::pair<std::size_t, std::size_t> {
                    detail::edge_traits<typename detail::type_list_at<I, detail::type_list<edges_t...>>::type>::src_id,
                        detail::edge_traits<typename detail::type_list_at<I, detail::type_list<edges_t...>>::type>::dst_id
                }...
            };
        }

        static constexpr auto make_edges_ids() {
            return make_edges_ids_impl(std::make_index_sequence<sizeof...(edges_t)>{});
        }

        static constexpr auto edges_ids = make_edges_ids();

    public:
        using vertex_types_list_public = vertex_types_list;

        // Kahn topological sort executed at compile time.
        struct topo_result {
            std::array<std::size_t, vertex_count> order {};
            bool has_cycle = false;
        };
        static constexpr topo_result compute_topology() {
            topo_result r {};
            std::array<std::size_t, vertex_count> indeg {};
            auto id2idx =
                [] (std::size_t id) constexpr {
                for (std::size_t i = 0; i < vertex_count; ++i) {
                    if (vertex_ids[i] == id) {
                        return i;
                    }
                }
                return (std::size_t) vertex_count;
                };
            for (std::size_t ei = 0; ei < edges_ids.size(); ++ei) {
                auto& e = edges_ids[ei];
                auto idx = id2idx(e.second);
                if (idx < vertex_count) {
                    ++indeg[idx];
                }
            }
            std::array<bool, vertex_count> used {};
            std::size_t placed = 0;
            while (placed < vertex_count) {
                std::size_t pick = vertex_count;
                std::size_t best_prio = 0;
                bool found = false;
                for (std::size_t i = 0; i < vertex_count; ++i) {
                    if (!used[i] && indeg[i] == 0) {
                        auto pr = vertex_priorities[i];
                        if (!found || pr > best_prio) {
                            best_prio = pr;
                            pick = i;
                            found = true;
                        }
                    }
                }
                if (!found) { // cycle: return original order for determinism
                    r.has_cycle = true;
                    for (std::size_t i = 0; i < vertex_count; ++i) {
                        r.order[i] = vertex_ids[i];
                    }
                    return r;
                }
                r.order[placed++] = vertex_ids[pick];
                used[pick] = true;
                for (std::size_t ei = 0; ei < edges_ids.size(); ++ei) {
                    auto& e = edges_ids[ei];
                    if (e.first == vertex_ids[pick]) {
                        auto idx = id2idx(e.second);
                        if (idx < vertex_count && indeg[idx] > 0) {
                            --indeg[idx];
                        }
                    }
                }
            }
            return r;
        }

        static constexpr auto topo = compute_topology();

        // Mapping from vertex id -> vertex type
        template<std::size_t Id, typename V, typename... Vs>
        struct find_impl {
            using type = std::conditional_t<(V::id() == Id), V, typename find_impl<Id, Vs...>::type>;
        };
        template<std::size_t Id, typename V>
        struct find_impl<Id, V> {
            using type = std::conditional_t<(V::id() == Id), V, void>;
        };

        template<std::size_t... I, typename F>
        static constexpr void for_each_impl(std::index_sequence<I...>, F&& f) {
            (f(typename find_type_by_id<topo.order[I]>::type {}), ...);
        }

        // Helper for variadic apply: invokes a callable once with all vertex types.
        // Supports both void and non-void returning callables in a strictly standard-compliant way.
        template<typename F, std::size_t... I>
        static constexpr auto apply_variadic_impl(F&& f, std::index_sequence<I...>) {
            using result_t = std::invoke_result_t<F, typename find_type_by_id<topo.order[I]>::type...>;
            if constexpr (std::is_void_v<result_t>) {
                std::forward<F>(f)(typename find_type_by_id<topo.order[I]>::type {}...);
            }
            else {
                return std::forward<F>(f)(typename find_type_by_id<topo.order[I]>::type {}...);
            }
        }

    public:

        static constexpr bool is_cyclic() { return topo.has_cycle; }
        static constexpr auto ids() { return topo.order; }
        static constexpr std::size_t size() { return vertex_count; }
        static constexpr auto edges() { return edges_ids; }

        template<std::size_t I>
        static constexpr std::size_t id_at() {
            static_assert(I < vertex_count, "Topology::id_at index out of range");
            return topo.order[I];
        }

        template<std::size_t Id>
        static constexpr bool has_id() {
            for (std::size_t i = 0; i < vertex_count; ++i) {
                if (topo.order[i] == Id) {
                    return true;
                }
            }
            return false;
        }

        // Query vertex type by id at compile-time: Topology::find_type_by_id<VID>::type
        template<std::size_t Id>
        struct find_type_by_id {
            template<std::size_t... I>
            static auto helper(std::index_sequence<I...>) -> typename find_impl<Id, typename detail::type_list_at<I, vertex_types_list>::type...>::type;
            using type = decltype(helper(std::make_index_sequence<vertex_count>{}));
            static_assert(!std::is_void_v<type>, "Vertex id not found");
        };

        // for_each: Visit each vertex type in topological order. The callable receives a distinct
        // default-constructed tag object instance for each vertex type.
        template<typename F>
        static constexpr void for_each(F&& f) {
            for_each_impl(std::make_index_sequence<vertex_count>{}, std::forward<F>(f));
        }

        // apply: Invoke a callable exactly once with all vertex tag objects passed variadically
        // in topological order. Example:
        //   Topology<Edges...>::apply([](auto vA, auto vB, auto vC){ /* ... */ });
        // This enables operations that depend on the full pack of vertex types simultaneously.
        // Public apply: forwards to variadic impl; supports both void and non-void lambdas without UB.
        template<typename F>
        static constexpr auto apply(F&& f)
            -> decltype(apply_variadic_impl(std::forward<F>(f), std::make_index_sequence<vertex_count>{})) {
            using result_t = decltype(apply_variadic_impl(std::forward<F>(f), std::make_index_sequence<vertex_count>{}));
            if constexpr (std::is_void_v<result_t>) {
                apply_variadic_impl(std::forward<F>(f), std::make_index_sequence<vertex_count>{});
            }
            else {
                return apply_variadic_impl(std::forward<F>(f), std::make_index_sequence<vertex_count>{});
            }
        }

        template<typename stream_t>
        static void print(stream_t& stream, const std::string_view& inGraphName = "") {
            ugraph::print_graph<Topology<edges_t...>>(stream, inGraphName);
        }

        template<typename stream_t>
        static void print_pipeline(stream_t& stream, const std::string_view& inGraphName = "") {
            ugraph::print_pipeline<Topology<edges_t...>>(stream, inGraphName);
        }

    };

} // namespace ugraph
