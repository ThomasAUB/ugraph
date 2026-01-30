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
#include "node_tag.hpp"
#include "type_list.hpp"

namespace ugraph {

    template<typename... edges_t>
    class Topology {
        // Implementation detail: edge trait extraction (source & destination vertex ids / types)
        template<typename E>
        struct edge_traits {
            using edge_t = std::decay_t<E>;
            using src_vertex_t = typename edge_t::first_type::node_type;
            using dst_vertex_t = typename edge_t::second_type::node_type;
            static constexpr std::size_t src_id = src_vertex_t::id();
            static constexpr std::size_t dst_id = dst_vertex_t::id();
        };

        // Lightweight internal typelist & helpers (moved to meta.hpp for reuse)
        template<typename List, typename V> struct list_append_unique;
        template<typename V, typename... Ts>
        struct list_append_unique<detail::type_list<Ts...>, V> {
            static constexpr bool exists = ((V::id() == Ts::id()) || ... || false);
            using type = std::conditional_t<exists, detail::type_list<Ts...>, detail::type_list<Ts..., V>>;
        };

        // Clearer name alias for the unique-append metafunction used throughout
        template<typename List, typename V>
        using append_unique = list_append_unique<List, V>;

        // Helper to detect if a type exposes an inner topology vertex_types_list_public
        template<typename T, typename = void>
        struct has_vertex_types_list : std::false_type {};
        template<typename T>
        struct has_vertex_types_list<T, std::void_t<typename T::vertex_types_list_public>> : std::true_type {};

        // Alternate, clearer name for readability in later code
        template<typename T>
        using has_nested_vertex_list = has_vertex_types_list<T>;

        // Append a sequence of types to a type_list while preserving uniqueness
        template<typename List, typename... Vs> struct append_types;
        template<typename List>
        struct append_types<List> { using type = List; };
        template<typename List, typename V, typename... Rest>
        struct append_types<List, V, Rest...> {
            using with = typename list_append_unique<List, V>::type;
            using type = typename append_types<with, Rest...>::type;
        };
        template<typename List, typename TL>
        struct append_type_list;
        template<typename List, typename... Vs>
        struct append_type_list<List, detail::type_list<Vs...>> { using type = typename append_types<List, Vs...>::type; };

        // Convenience alias for appending a typelist into another
        template<typename List, typename TL>
        using append_typelist = append_type_list<List, TL>;

        // When a vertex (NodeTag) wraps a nested Topology as its module_type,
        // expand the inner topology's vertex types into the parent list.
        template<typename List, typename V, bool = has_vertex_types_list<typename V::module_type>::value>
        struct list_add_vertex {
            using type = typename list_append_unique<List, V>::type;
        };
        template<typename List, typename V>
        struct list_add_vertex<List, V, true> {
            using inner = typename V::module_type::vertex_types_list_public;
            using type = typename append_type_list<List, inner>::type;
        };

        // Alias returning the resulting typelist for clarity in metaprograms
        template<typename List, typename V>
        using list_add_vertex_t = typename list_add_vertex<List, V>::type;

        template<typename List, typename Edge>
        struct list_add_edge_vertices {
            using src_t = typename edge_traits<Edge>::src_vertex_t;
            using dst_t = typename edge_traits<Edge>::dst_vertex_t;
            using with_src = typename list_add_vertex<List, src_t>::type;
            using type = typename list_add_vertex<with_src, dst_t>::type;
        };
        // Similar helpers that do NOT expand nested Topology module_type.
        template<typename List, typename V>
        struct list_add_vertex_no_expand { using type = typename list_append_unique<List, V>::type; };
        template<typename List, typename Edge>
        struct list_add_edge_vertices_no_expand {
            using src_t = typename edge_traits<Edge>::src_vertex_t;
            using dst_t = typename edge_traits<Edge>::dst_vertex_t;
            using with_src = typename list_add_vertex_no_expand<List, src_t>::type;
            using type = typename list_add_vertex_no_expand<with_src, dst_t>::type;
        };
        template<typename List, typename... Edges> struct fold_edges_no_expand;
        template<typename List> struct fold_edges_no_expand<List> { using type = List; };
        template<typename List, typename E, typename... R>
        struct fold_edges_no_expand<List, E, R...> { using type = typename fold_edges_no_expand<typename list_add_edge_vertices_no_expand<List, E>::type, R...>::type; };
        using declared_vertex_types_list = typename fold_edges_no_expand<detail::type_list<>, edges_t...>::type;
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

        template<std::size_t... I>
        static constexpr auto make_vertex_priorities(std::index_sequence<I...>) {
            return std::array<std::size_t, sizeof...(I)>{ detail::type_list_at<I, vertex_types_list>::type::priority()... };
        }
        static constexpr auto vertex_priorities = make_vertex_priorities(std::make_index_sequence<vertex_count>{});

        // Helper: build ids array from an arbitrary type_list
        template<typename TL, std::size_t... I>
        static constexpr auto make_ids_from_typelist(std::index_sequence<I...>) {
            return std::array<std::size_t, sizeof...(I)>{ detail::type_list_at<I, TL>::type::id()... };
        }

        // Get the ids represented by a vertex type V (either single id or inner topology ids)
        template<typename V>
        static constexpr auto ids_for_vertex() {
            if constexpr (has_vertex_types_list<typename V::module_type>::value) {
                using inner = typename V::module_type::vertex_types_list_public;
                constexpr std::size_t N = detail::type_list_size<inner>::value;
                return make_ids_from_typelist<inner>(std::make_index_sequence<N>{});
            }
            else {
                return std::array<std::size_t, 1>{ V::id() };
            }
        }

        // Compute expanded size for an Edge (number of src * number of dst ids)
        // Helper: get number of vertices exposed by a module_type (1 if none, else inner list size)
        template<typename M, typename = void>
        struct module_vertex_count { static constexpr std::size_t value = 1; };
        template<typename M>
        struct module_vertex_count<M, std::void_t<typename M::vertex_types_list_public>> {
            static constexpr std::size_t value = detail::type_list_size<typename M::vertex_types_list_public>::value;
        };

        // Instead of exposing all inner vertices, we want to map edges to module boundary nodes:
        // - entry nodes: inner nodes with no incoming edges
        // - exit nodes: inner nodes with no outgoing edges
        // Provide constexpr helpers to count and access these ids for a module Topology type M.
        template<typename M, typename = void>
        struct module_entry_count { static constexpr std::size_t value = 1; };
        template<typename M>
        struct module_entry_count<M, std::void_t<typename M::vertex_types_list_public>> {
            static constexpr std::size_t compute() {
                constexpr auto ids = M::ids();
                constexpr auto edges = M::edges();
                std::size_t c = 0;
                for (std::size_t i = 0; i < ids.size(); ++i) {
                    bool has_in = false;
                    for (std::size_t j = 0; j < edges.size(); ++j) {
                        if (edges[j].second == ids[i]) { has_in = true; break; }
                    }
                    if (!has_in) ++c;
                }
                return c;
            }
            static constexpr std::size_t value = compute();
        };

        template<typename M, typename = void>
        struct module_exit_count { static constexpr std::size_t value = 1; };
        template<typename M>
        struct module_exit_count<M, std::void_t<typename M::vertex_types_list_public>> {
            static constexpr std::size_t compute() {
                constexpr auto ids = M::ids();
                constexpr auto edges = M::edges();
                std::size_t c = 0;
                for (std::size_t i = 0; i < ids.size(); ++i) {
                    bool has_out = false;
                    for (std::size_t j = 0; j < edges.size(); ++j) {
                        if (edges[j].first == ids[i]) { has_out = true; break; }
                    }
                    if (!has_out) ++c;
                }
                return c;
            }
            static constexpr std::size_t value = compute();
        };

        template<typename M, std::size_t K>
        static constexpr std::size_t module_entry_id_at() {
            constexpr auto ids = M::ids();
            constexpr auto edges = M::edges();
            std::size_t found = 0;
            for (std::size_t i = 0; i < ids.size(); ++i) {
                bool has_in = false;
                for (std::size_t j = 0; j < edges.size(); ++j) {
                    if (edges[j].second == ids[i]) { has_in = true; break; }
                }
                if (!has_in) {
                    if (found == K) return ids[i];
                    ++found;
                }
            }
            return (std::size_t)0;
        }

        template<typename M, std::size_t K>
        static constexpr std::size_t module_exit_id_at() {
            constexpr auto ids = M::ids();
            constexpr auto edges = M::edges();
            std::size_t found = 0;
            for (std::size_t i = 0; i < ids.size(); ++i) {
                bool has_out = false;
                for (std::size_t j = 0; j < edges.size(); ++j) {
                    if (edges[j].first == ids[i]) { has_out = true; break; }
                }
                if (!has_out) {
                    if (found == K) return ids[i];
                    ++found;
                }
            }
            return (std::size_t)0;
        }

        template<typename Edge>
        struct expanded_edge_size {
            using S = typename edge_traits<Edge>::src_vertex_t;
            using D = typename edge_traits<Edge>::dst_vertex_t;
            static constexpr std::size_t s = has_vertex_types_list<typename S::module_type>::value ? module_exit_count<typename S::module_type>::value : 1;
            static constexpr std::size_t d = has_vertex_types_list<typename D::module_type>::value ? module_entry_count<typename D::module_type>::value : 1;
            static constexpr std::size_t value = s * d;
        };

        static constexpr std::size_t edge_count = sizeof...(edges_t);
        static constexpr std::size_t total_expanded_edges = (expanded_edge_size<edges_t>::value + ...);

        // Compute total expanded edges coming from nested module-vertices declared in this topology
        // Use a pack-based constexpr implementation to avoid deep recursive template instantiation.
        template<typename Declared>
        struct declared_nested_edges_total;
        // Helper to safely obtain edges count from a module type when present.
        template<typename M, typename = void>
        struct module_edges_count { static constexpr std::size_t value = 0; };
        template<typename M>
        struct module_edges_count<M, std::void_t<decltype(M::edges())>> { static constexpr std::size_t value = M::edges().size(); };

        template<typename... Vs>
        struct declared_nested_edges_total<detail::type_list<Vs...>> {
            static constexpr std::size_t value = ((module_edges_count<typename Vs::module_type>::value) + ... + 0);
        };

        template<typename Edge, std::size_t K>
        static constexpr std::pair<std::size_t, std::size_t> pair_for_edge_const() {
            using S = typename edge_traits<Edge>::src_vertex_t;
            using D = typename edge_traits<Edge>::dst_vertex_t;
            constexpr bool S_is_module = has_vertex_types_list<typename S::module_type>::value;
            constexpr bool D_is_module = has_vertex_types_list<typename D::module_type>::value;
            if constexpr (S_is_module && D_is_module) {
                constexpr std::size_t d = module_entry_count<typename D::module_type>::value;
                constexpr std::size_t src_idx = K / d;
                constexpr std::size_t dst_idx = K % d;
                constexpr std::size_t sid = module_exit_id_at<typename S::module_type, src_idx>();
                constexpr std::size_t did = module_entry_id_at<typename D::module_type, dst_idx>();
                return { sid, did };
            }
            else if constexpr (S_is_module && !D_is_module) {
                constexpr std::size_t sid = module_exit_id_at<typename S::module_type, K>();
                return { sid, D::id() };
            }
            else if constexpr (!S_is_module && D_is_module) {
                constexpr std::size_t d = module_entry_count<typename D::module_type>::value;
                constexpr std::size_t dst_idx = K % d;
                return { S::id(), module_entry_id_at<typename D::module_type, dst_idx>() };
            }
            else {
                return { S::id(), D::id() };
            }
        }

        template<std::size_t K, std::size_t I = 0>
        static constexpr std::pair<std::size_t, std::size_t> find_pair_const() {
            if constexpr (I >= edge_count) {
                return { 0, 0 };
            } else {
                using Edge = typename detail::type_list_at<I, detail::type_list<edges_t...>>::type;
                constexpr std::size_t sz = expanded_edge_size<Edge>::value;
                if constexpr (K < sz) {
                    return pair_for_edge_const<Edge, K>();
                } else {
                    return find_pair_const<K - sz, I + 1>();
                }
            }
        }

        // Find K-th nested-module edge across declared vertices (declared_vertex_types_list)
        template<std::size_t K, std::size_t I = 0>
        static constexpr std::pair<std::size_t, std::size_t> find_nested_pair_const() {
            using declared = declared_vertex_types_list;
            constexpr std::size_t D = detail::type_list_size<declared>::value;
            if constexpr (I >= D) {
                return { 0, 0 };
            } else {
                using vt = typename detail::type_list_at<I, declared>::type;
                if constexpr (!has_vertex_types_list<typename vt::module_type>::value) {
                    return find_nested_pair_const<K, I + 1>();
                } else {
                    constexpr auto m_edges = vt::module_type::edges();
                    constexpr std::size_t msz = m_edges.size();
                    if constexpr (K < msz) {
                        return m_edges[K];
                    } else {
                        return find_nested_pair_const<K - msz, I + 1>();
                    }
                }
            }
        }

        // Total edges = nested module edges + top-level expanded edges
        static constexpr std::size_t nested_total = declared_nested_edges_total<declared_vertex_types_list>::value;
        static constexpr std::size_t all_total_expanded_edges = nested_total + total_expanded_edges;

        template<std::size_t... I>
        static constexpr auto make_edges_ids_impl(std::index_sequence<I...>) {
            return std::array<std::pair<std::size_t, std::size_t>, sizeof...(I)>{ (I < nested_total ? find_nested_pair_const<I>() : find_pair_const<I - nested_total>())... };
        }

        static constexpr auto make_edges_ids() {
            return make_edges_ids_impl(std::make_index_sequence<all_total_expanded_edges>{});
        }

        static constexpr auto edges_ids = make_edges_ids();

        // Provide a public alias so nested Topology types can expose their vertex list
    public:
        using vertex_types_list_public = vertex_types_list;
        using declared_vertex_types_list_public = declared_vertex_types_list;

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

    };

} // namespace ugraph
