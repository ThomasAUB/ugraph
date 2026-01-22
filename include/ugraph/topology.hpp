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
        template<typename, typename = void>
        struct has_index : std::false_type {};
        template<typename T>
        struct has_index<T, std::void_t<decltype(T::index())>> : std::true_type {};

        template<typename E>
        struct edge_traits_impl {
            using edge_t = std::decay_t<E>;
            using src_port_t = typename edge_t::first_type;
            using dst_port_t = typename edge_t::second_type;
            using src_vertex_t = typename src_port_t::node_type;
            using dst_vertex_t = typename dst_port_t::node_type;
            static constexpr std::size_t src_id = src_vertex_t::id();
            static constexpr std::size_t dst_id = dst_vertex_t::id();
            static constexpr std::size_t src_port_index = [] () constexpr {
                if constexpr (has_index<src_port_t>::value) return src_port_t::index(); else return (std::size_t) 0;
                }();
            static constexpr std::size_t dst_port_index = [] () constexpr {
                if constexpr (has_index<dst_port_t>::value) return dst_port_t::index(); else return (std::size_t) 0;
                }();
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
            using with_src = typename list_append_unique<List, typename edge_traits_impl<Edge>::src_vertex_t>::type;
            using type = typename list_append_unique<with_src, typename edge_traits_impl<Edge>::dst_vertex_t>::type;
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

        template<std::size_t... I>
        static constexpr auto make_vertex_priorities(std::index_sequence<I...>) {
            return std::array<std::size_t, sizeof...(I)>{ detail::type_list_at<I, vertex_types_list>::type::priority()... };
        }
        static constexpr auto vertex_priorities = make_vertex_priorities(std::make_index_sequence<vertex_count>{});

        // Edge list as (src,dst) id pairs for Kahn's algorithm
        static constexpr auto make_edges_ids() {
            return std::array<std::pair<std::size_t, std::size_t>, sizeof...(edges_t)>{
                std::pair<std::size_t, std::size_t>{ edge_traits_impl<edges_t>::src_id, edge_traits_impl<edges_t>::dst_id }...
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

        // Interval coloring (producer lifetime analysis & buffer assignment)
        template<std::size_t _vid, std::size_t _port>
        struct producer_tag { static constexpr std::size_t vid = _vid; static constexpr std::size_t port = _port; };

        template<typename List, typename Tag> struct append_unique;
        template<typename Tag, typename... Ts>
        struct append_unique<detail::type_list<Ts...>, Tag> {
            static constexpr bool exists = ((Tag::vid == Ts::vid && Tag::port == Ts::port) || ... || false);
            using type = std::conditional_t<exists, detail::type_list<Ts...>, detail::type_list<Ts..., Tag>>;
        };
        template<typename List, typename Edge> struct add_edge_prod {
            using tr = edge_traits_impl<Edge>;
            using tag = producer_tag<tr::src_id, tr::src_port_index>;
            using type = typename append_unique<List, tag>::type;
        };
        template<typename List, typename... Es> struct fold_prod;
        template<typename List> struct fold_prod<List> { using type = List; };
        template<typename List, typename E, typename... R>
        struct fold_prod<List, E, R...> { using type = typename fold_prod<typename add_edge_prod<List, E>::type, R...>::type; };
        using producer_list = typename fold_prod<detail::type_list<>, edges_t...>::type;

        static constexpr std::size_t producer_count = detail::type_list_size<producer_list>::value;

        static constexpr std::size_t id_to_pos(std::size_t id) {
            for (std::size_t i = 0; i < vertex_count; ++i) if (topo.order[i] == id) return i;
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
            std::array<std::size_t, producer_count == 0 ? 1 : producer_count> buf {};
            std::size_t count {};
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
            else a.count = 0;
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
            using tr = edge_traits_impl<E0>;
            static constexpr bool match = (tr::dst_id == DVID && tr::dst_port_index == DPORT);
            static constexpr std::size_t src_vid = match ? tr::src_id : find_input_edge_impl<DVID, DPORT, Rest...>::src_vid;
            static constexpr std::size_t src_port = match ? tr::src_port_index : find_input_edge_impl<DVID, DPORT, Rest...>::src_port;
        };

        template<std::size_t DVID, std::size_t DPORT>
        struct find_input_edge : find_input_edge_impl<DVID, DPORT, edges_t...> {};



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

        // Public alias to the internal edge traits. Use this from other components
        // (e.g. GraphView) to extract src/dst ids and port indices safely.
        template<typename E>
        using edge_traits = edge_traits_impl<E>;

        static constexpr bool is_cyclic() { return topo.has_cycle; }
        static constexpr auto ids() { return topo.order; }
        static constexpr std::size_t size() { return vertex_count; }
        static constexpr auto edges() { return edges_ids; }

        static constexpr std::size_t data_instance_count() { return assignment.count; }

        template<std::size_t vid, std::size_t port>
        static constexpr std::size_t output_data_index() {
            constexpr std::size_t pidx = find_prod_index_impl<vid, port, 0>::value;
            static_assert(pidx != (std::size_t) -1, "(vertex id, output port) not a producer in this graph");
            return assignment.buf[pidx];
        }

        template<std::size_t vid, std::size_t port>
        static constexpr std::size_t input_data_index() {
            constexpr std::size_t src_vid = find_input_edge<vid, port>::src_vid;
            constexpr std::size_t src_port = find_input_edge<vid, port>::src_port;
            static_assert(src_vid != (std::size_t) -1, "No edge found feeding (vertex id, input port)");
            return output_data_index<src_vid, src_port>();
        }

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
