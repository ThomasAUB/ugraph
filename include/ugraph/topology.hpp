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
        template<typename List, typename Edge>
        struct list_add_edge_vertices {
            using with_src = typename list_append_unique<List, typename edge_traits<Edge>::src_vertex_t>::type;
            using type = typename list_append_unique<with_src, typename edge_traits<Edge>::dst_vertex_t>::type;
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

        // Edge list as (src,dst) id pairs for Kahn's algorithm
        static constexpr auto make_edges_ids() {
            return std::array<std::pair<std::size_t, std::size_t>, sizeof...(edges_t)>{
                std::pair<std::size_t, std::size_t>{ edge_traits<edges_t>::src_id, edge_traits<edges_t>::dst_id }...
            };
        }
        static constexpr auto edges_ids = make_edges_ids();

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
            for (auto& e : edges_ids) {
                auto idx = id2idx(e.second);
                if (idx < vertex_count) {
                    ++indeg[idx];
                }
            }
            std::array<bool, vertex_count> used {};
            std::size_t placed = 0;
            while (placed < vertex_count) {
                std::size_t pick = vertex_count;
                for (std::size_t i = 0; i < vertex_count; ++i) {
                    if (!used[i] && indeg[i] == 0) {
                        pick = i;
                        break;
                    }
                }
                if (pick == vertex_count) { // cycle: return original order for determinism
                    r.has_cycle = true;
                    for (std::size_t i = 0; i < vertex_count; ++i) {
                        r.order[i] = vertex_ids[i];
                        return r;
                    }
                }
                r.order[placed++] = vertex_ids[pick];
                used[pick] = true;
                for (auto& e : edges_ids) {
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
