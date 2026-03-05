#pragma once

#include <tuple>
#include <type_traits>

#include "graph_coloring.hpp"

namespace ugraph::detail {

    template<typename Edge>
    struct edge_data_type {
        using type = typename Edge::first_type::data_type;
    };

    template<typename T, typename Edge>
    struct edge_is_type : std::false_type {};

    template<typename T, typename S, typename D>
    struct edge_is_type<T, std::pair<S, D>> : std::bool_constant<std::is_same_v<typename S::data_type, T>> {};

    template<typename List, typename T>
    struct type_list_append_unique;

    template<typename T, typename... Ts>
    struct type_list_append_unique<detail::type_list<Ts...>, T> {
        static constexpr bool exists = (std::is_same_v<T, Ts> || ... || false);
        using type = std::conditional_t<exists, detail::type_list<Ts...>, detail::type_list<Ts..., T>>;
    };

    template<typename List, typename... Ts>
    struct fold_append_unique;

    template<typename List>
    struct fold_append_unique<List> { using type = List; };

    template<typename List, typename T, typename... Rest>
    struct fold_append_unique<List, T, Rest...> {
        using next = typename type_list_append_unique<List, T>::type;
        using type = typename fold_append_unique<next, Rest...>::type;
    };

    template<typename List, typename TL>
    struct append_type_list_unique;
    template<typename List, typename... Ts>
    struct append_type_list_unique<List, detail::type_list<Ts...>> { using type = typename fold_append_unique<List, Ts...>::type; };

    template<typename TL>
    struct collect_specs_from_typelist;
    template<>
    struct collect_specs_from_typelist<detail::type_list<>> { using type = detail::type_list<>; };

    template<typename V, typename... Vs>
    struct collect_specs_from_typelist<detail::type_list<V, Vs...>> {
        using head_specs = typename V::module_type::Manifest::specs_list;
        using tail = typename collect_specs_from_typelist<detail::type_list<Vs...>>::type;
        using type = typename append_type_list_unique<head_specs, tail>::type;
    };

    template<typename List>
    struct manifest_from_list;

    template<typename... Ts>
    struct manifest_from_list<detail::type_list<Ts...>> { using type = Manifest<Ts...>; };

    template<typename T, typename List>
    struct filter_edges;

    template<typename T>
    struct filter_edges<T, detail::type_list<>> {
        using type = detail::type_list<>;
    };

    template<typename T, typename E, typename... Rest>
    struct filter_edges<T, detail::type_list<E, Rest...>> {
        using tail = typename filter_edges<T, detail::type_list<Rest...>>::type;
        using type = std::conditional_t<
            edge_is_type<T, E>::value,
            typename detail::type_list_prepend<E, tail>::type,
            tail
        >;
    };

    template<typename... Lists>
    struct type_list_concat;

    template<>
    struct type_list_concat<> { using type = detail::type_list<>; };

    template<typename... Ts>
    struct type_list_concat<detail::type_list<Ts...>> { using type = detail::type_list<Ts...>; };

    template<typename... A, typename... B, typename... Rest>
    struct type_list_concat<detail::type_list<A...>, detail::type_list<B...>, Rest...> {
        using type = typename type_list_concat<detail::type_list<A..., B...>, Rest...>::type;
    };

    template<typename... edges_t>
    struct data_graph_traits {
        using topology_t = Topology<edges_t...>;

        template<typename T, typename = void>
        struct has_nested_graph_interface : std::false_type {};

        template<typename T>
        struct has_nested_graph_interface<T, std::void_t<
            typename T::vertex_types_list_public,
            typename T::edge_types_list_public,
            typename T::topology_type
        >> : std::true_type {};

        template<typename node_t, std::size_t port_idx>
        struct synthetic_input_port {
            using node_type = node_t;
            static constexpr std::size_t index() { return port_idx; }
        };

        template<typename data_t, typename node_t, std::size_t port_idx>
        struct synthetic_output_port : synthetic_input_port<node_t, port_idx> {
            using data_type = data_t;
        };

        template<typename data_t, typename src_node_t, std::size_t src_port_idx, typename dst_node_t, std::size_t dst_port_idx>
        using synthetic_edge = std::pair<
            synthetic_output_port<data_t, src_node_t, src_port_idx>,
            synthetic_input_port<dst_node_t, dst_port_idx>
        >;

        template<typename V, std::size_t Base>
        struct shifted_vertex {
            static constexpr std::size_t id() { return Base + V::id(); }
            static constexpr std::size_t priority() { return V::priority(); }
            static constexpr std::size_t index() { return 0; }
            using module_type = typename V::module_type;
            using node_type = shifted_vertex<V, Base>;
        };

        template<typename M, typename = void>
        struct module_entry_count { static constexpr std::size_t value = 1; };

        template<typename M>
        struct module_entry_count<M, std::enable_if_t<has_nested_graph_interface<M>::value>> {
            static constexpr std::size_t compute() {
                constexpr auto ids = M::topology_type::ids();
                constexpr auto edges = M::topology_type::edges();
                std::size_t c = 0;
                for (std::size_t i = 0; i < ids.size(); ++i) {
                    bool has_in = false;
                    for (std::size_t j = 0; j < edges.size(); ++j) {
                        if (edges[j].second == ids[i]) {
                            has_in = true;
                            break;
                        }
                    }
                    if (!has_in) {
                        ++c;
                    }
                }
                return c;
            }
            static constexpr std::size_t value = compute();
        };

        template<typename M, typename = void>
        struct module_exit_count { static constexpr std::size_t value = 1; };

        template<typename M>
        struct module_exit_count<M, std::enable_if_t<has_nested_graph_interface<M>::value>> {
            static constexpr std::size_t compute() {
                constexpr auto ids = M::topology_type::ids();
                constexpr auto edges = M::topology_type::edges();
                std::size_t c = 0;
                for (std::size_t i = 0; i < ids.size(); ++i) {
                    bool has_out = false;
                    for (std::size_t j = 0; j < edges.size(); ++j) {
                        if (edges[j].first == ids[i]) {
                            has_out = true;
                            break;
                        }
                    }
                    if (!has_out) {
                        ++c;
                    }
                }
                return c;
            }
            static constexpr std::size_t value = compute();
        };

        template<typename M, std::size_t K>
        static constexpr std::size_t module_entry_id_at() {
            constexpr auto ids = M::topology_type::ids();
            constexpr auto edges = M::topology_type::edges();
            std::size_t found = 0;
            for (std::size_t i = 0; i < ids.size(); ++i) {
                bool has_in = false;
                for (std::size_t j = 0; j < edges.size(); ++j) {
                    if (edges[j].second == ids[i]) {
                        has_in = true;
                        break;
                    }
                }
                if (!has_in) {
                    if (found == K) {
                        return ids[i];
                    }
                    ++found;
                }
            }
            return static_cast<std::size_t>(0);
        }

        template<typename M, std::size_t K>
        static constexpr std::size_t module_exit_id_at() {
            constexpr auto ids = M::topology_type::ids();
            constexpr auto edges = M::topology_type::edges();
            std::size_t found = 0;
            for (std::size_t i = 0; i < ids.size(); ++i) {
                bool has_out = false;
                for (std::size_t j = 0; j < edges.size(); ++j) {
                    if (edges[j].first == ids[i]) {
                        has_out = true;
                        break;
                    }
                }
                if (!has_out) {
                    if (found == K) {
                        return ids[i];
                    }
                    ++found;
                }
            }
            return static_cast<std::size_t>(0);
        }

        template<typename M, std::size_t K, std::size_t Base>
        using module_entry_vertex_t = shifted_vertex<
            typename M::topology_type::template find_type_by_id<module_entry_id_at<M, K>()>::type,
            Base
        >;

        template<typename M, std::size_t K, std::size_t Base>
        using module_exit_vertex_t = shifted_vertex<
            typename M::topology_type::template find_type_by_id<module_exit_id_at<M, K>()>::type,
            Base
        >;

        template<typename data_t, typename src_node_t, std::size_t src_port_idx, typename dst_node_t, std::size_t dst_port_idx, std::size_t... I>
        static auto make_src_to_entries(std::index_sequence<I...>)
            -> detail::type_list<synthetic_edge<data_t, src_node_t, src_port_idx, module_entry_vertex_t<typename dst_node_t::module_type, I, dst_node_t::id()>, 0>...>;

        template<typename data_t, typename src_node_t, std::size_t src_port_idx, typename dst_node_t, std::size_t dst_port_idx, std::size_t... I>
        static auto make_exits_to_dst(std::index_sequence<I...>)
            -> detail::type_list<synthetic_edge<data_t, module_exit_vertex_t<typename src_node_t::module_type, I, src_node_t::id()>, 0, dst_node_t, dst_port_idx>...>;

        template<typename data_t, typename src_node_t, std::size_t src_port_idx, typename dst_node_t, std::size_t dst_port_idx, std::size_t S, std::size_t... D>
        static auto make_exit_to_entries_for_source(std::index_sequence<D...>)
            -> detail::type_list<synthetic_edge<data_t, module_exit_vertex_t<typename src_node_t::module_type, S, src_node_t::id()>, 0, module_entry_vertex_t<typename dst_node_t::module_type, D, dst_node_t::id()>, 0>...>;

        template<typename Edge, std::size_t Base>
        struct remap_edge_with_base {
            using tr = detail::edge_traits<Edge>;
            using data_t = typename detail::edge_data_type<Edge>::type;
            using src_node_t = typename tr::src_vertex_t;
            using dst_node_t = typename tr::dst_vertex_t;
            using type = synthetic_edge<
                data_t,
                shifted_vertex<src_node_t, Base>,
                tr::src_port_index,
                shifted_vertex<dst_node_t, Base>,
                tr::dst_port_index
            >;
        };

        template<typename TL, std::size_t Base>
        struct remap_edge_list_with_base;

        template<std::size_t Base, typename... Es>
        struct remap_edge_list_with_base<detail::type_list<Es...>, Base> {
            using type = detail::type_list<typename remap_edge_with_base<Es, Base>::type...>;
        };

        template<typename data_t, typename src_node_t, std::size_t src_port_idx, typename dst_node_t, std::size_t dst_port_idx, std::size_t... S>
        static auto make_exits_to_entries(std::index_sequence<S...>)
            -> typename type_list_concat<decltype(make_exit_to_entries_for_source<data_t, src_node_t, src_port_idx, dst_node_t, dst_port_idx, S>(
                std::make_index_sequence<module_entry_count<typename dst_node_t::module_type>::value>{
            }))...>::type;

        template<typename Edge, bool SrcNested, bool DstNested>
        struct expand_edge_types_impl;

        template<typename Edge>
        struct expand_edge_types_impl<Edge, false, false> {
            using type = detail::type_list<Edge>;
        };

        template<typename Edge>
        struct expand_edge_types_impl<Edge, true, false> {
            using tr = detail::edge_traits<Edge>;
            using data_t = typename detail::edge_data_type<Edge>::type;
            using src_node_t = typename tr::src_vertex_t;
            using dst_node_t = typename tr::dst_vertex_t;
            using type = decltype(make_exits_to_dst<data_t, src_node_t, tr::src_port_index, dst_node_t, tr::dst_port_index>(
                std::make_index_sequence<module_exit_count<typename src_node_t::module_type>::value>{
            }));
        };

        template<typename Edge>
        struct expand_edge_types_impl<Edge, false, true> {
            using tr = detail::edge_traits<Edge>;
            using data_t = typename detail::edge_data_type<Edge>::type;
            using src_node_t = typename tr::src_vertex_t;
            using dst_node_t = typename tr::dst_vertex_t;
            using type = decltype(make_src_to_entries<data_t, src_node_t, tr::src_port_index, dst_node_t, tr::dst_port_index>(
                std::make_index_sequence<module_entry_count<typename dst_node_t::module_type>::value>{
            }));
        };

        template<typename Edge>
        struct expand_edge_types_impl<Edge, true, true> {
            using tr = detail::edge_traits<Edge>;
            using data_t = typename detail::edge_data_type<Edge>::type;
            using src_node_t = typename tr::src_vertex_t;
            using dst_node_t = typename tr::dst_vertex_t;
            using type = decltype(make_exits_to_entries<data_t, src_node_t, tr::src_port_index, dst_node_t, tr::dst_port_index>(
                std::make_index_sequence<module_exit_count<typename src_node_t::module_type>::value>{
            }));
        };

        template<typename Edge>
        struct expand_edge_types {
            using tr = detail::edge_traits<Edge>;
            using src_node_t = typename tr::src_vertex_t;
            using dst_node_t = typename tr::dst_vertex_t;
            static constexpr bool src_nested = has_nested_graph_interface<typename src_node_t::module_type>::value;
            static constexpr bool dst_nested = has_nested_graph_interface<typename dst_node_t::module_type>::value;
            using type = typename expand_edge_types_impl<Edge, src_nested, dst_nested>::type;
        };

        template<typename Edge, bool SrcNested, bool DstNested>
        struct nested_internal_edge_types_impl;

        template<typename Edge>
        struct nested_internal_edge_types_impl<Edge, false, false> { using type = detail::type_list<>; };

        template<typename Edge>
        struct nested_internal_edge_types_impl<Edge, true, false> {
            using src_node_t = typename detail::edge_traits<Edge>::src_vertex_t;
            using type = typename remap_edge_list_with_base<
                typename src_node_t::module_type::edge_types_list_public,
                src_node_t::id()
            >::type;
        };

        template<typename Edge>
        struct nested_internal_edge_types_impl<Edge, false, true> {
            using dst_node_t = typename detail::edge_traits<Edge>::dst_vertex_t;
            using type = typename remap_edge_list_with_base<
                typename dst_node_t::module_type::edge_types_list_public,
                dst_node_t::id()
            >::type;
        };

        template<typename Edge>
        struct nested_internal_edge_types_impl<Edge, true, true> {
            using src_node_t = typename detail::edge_traits<Edge>::src_vertex_t;
            using dst_node_t = typename detail::edge_traits<Edge>::dst_vertex_t;
            using type = typename type_list_concat<
                typename remap_edge_list_with_base<
                    typename src_node_t::module_type::edge_types_list_public,
                    src_node_t::id()
                >::type,
                typename remap_edge_list_with_base<
                    typename dst_node_t::module_type::edge_types_list_public,
                    dst_node_t::id()
                >::type
            >::type;
        };

        template<typename Edge>
        struct nested_internal_edge_types_for {
            using tr = detail::edge_traits<Edge>;
            using src_node_t = typename tr::src_vertex_t;
            using dst_node_t = typename tr::dst_vertex_t;
            static constexpr bool src_nested = has_nested_graph_interface<typename src_node_t::module_type>::value;
            static constexpr bool dst_nested = has_nested_graph_interface<typename dst_node_t::module_type>::value;
            using type = typename nested_internal_edge_types_impl<Edge, src_nested, dst_nested>::type;
        };

        template<typename List, typename... Es>
        struct fold_flatten_edges;

        template<typename List>
        struct fold_flatten_edges<List> { using type = List; };

        template<typename List, typename E0, typename... Rest>
        struct fold_flatten_edges<List, E0, Rest...> {
            using expanded_t = typename expand_edge_types<E0>::type;
            using nested_t = typename nested_internal_edge_types_for<E0>::type;
            using next_t = typename type_list_concat<List, expanded_t, nested_t>::type;
            using type = typename fold_flatten_edges<next_t, Rest...>::type;
        };

        using flattened_edges_t = typename fold_flatten_edges<detail::type_list<>, edges_t...>::type;

        template<std::size_t I>
        using node_type_at = typename topology_t::template find_type_by_id<topology_t::template id_at<I>()>::type;

        using graph_types_list = typename collect_specs_from_typelist<typename topology_t::vertex_types_list_public>::type;
        using manifest_t = typename manifest_from_list<graph_types_list>::type;
        static constexpr std::size_t invalid_index = static_cast<std::size_t>(-1);

        template<std::size_t... I>
        static constexpr auto make_modules_tuple_t(std::index_sequence<I...>) ->
            std::tuple<typename topology_t::template find_type_by_id<topology_t::template id_at<I>()>::type::module_type*...>;

        using modules_tuple_t = decltype(make_modules_tuple_t(std::make_index_sequence<topology_t::size()>{}));

        template<std::size_t id, typename Edge>
        static constexpr auto try_edge_module(const Edge& e) {
            using S = typename detail::edge_traits<Edge>::src_vertex_t;
            using wanted_module_t = typename topology_t::template find_type_by_id<id>::type::module_type;
            using wanted_ptr_t = wanted_module_t*;

            using D = typename detail::edge_traits<Edge>::dst_vertex_t;

            auto try_nested_from_src = [] (auto& module) constexpr -> wanted_ptr_t {
                using module_t = std::decay_t<decltype(module)>;
                if constexpr (has_nested_graph_interface<module_t>::value) {
                    if constexpr ((id >= S::id()) && module_t::template contains_node_id<id - S::id()>()) {
                        if constexpr (std::is_convertible_v<decltype(module.template module_ptr_by_id<id - S::id()>()), wanted_ptr_t>) {
                            return module.template module_ptr_by_id<id - S::id()>();
                        }
                    }
                }
                return nullptr;
                };

            auto try_nested_from_dst = [] (auto& module) constexpr -> wanted_ptr_t {
                using module_t = std::decay_t<decltype(module)>;
                if constexpr (has_nested_graph_interface<module_t>::value) {
                    if constexpr ((id >= D::id()) && module_t::template contains_node_id<id - D::id()>()) {
                        if constexpr (std::is_convertible_v<decltype(module.template module_ptr_by_id<id - D::id()>()), wanted_ptr_t>) {
                            return module.template module_ptr_by_id<id - D::id()>();
                        }
                    }
                }
                return nullptr;
                };

            if constexpr (S::id() == id) {
                return &e.first.module();
            }
            else {
                if constexpr (D::id() == id) {
                    return &e.second.module();
                }
                else {
                    if (auto* p = try_nested_from_src(e.first.module())) {
                        return p;
                    }
                    if (auto* p = try_nested_from_dst(e.second.module())) {
                        return p;
                    }
                    return (typename topology_t::template find_type_by_id<id>::type::module_type*)nullptr;
                }
            }
        }

        template<std::size_t id>
        static constexpr auto get_module_ptr(const edges_t&... es) {
            typename topology_t::template find_type_by_id<id>::type::module_type* r = nullptr;
            ((r = r ? r : try_edge_module<id>(es)), ...);
            return r;
        }

        template<std::size_t... I>
        static constexpr modules_tuple_t build_modules(std::index_sequence<I...>, const edges_t&... es) {
            return { get_module_ptr<topology_t::template id_at<I>()>(es...)... };
        }

        template<typename T>
        using edge_list_for_t = typename detail::filter_edges<T, flattened_edges_t>::type;

        template<typename T>
        using coloring_t = typename detail::coloring_or_empty<topology_t, edge_list_for_t<T>>::type;

        template<typename T, std::size_t VID, std::size_t PORT, typename EdgeList>
        struct has_input_edge_impl;

        template<typename T, std::size_t VID, std::size_t PORT>
        struct has_input_edge_impl<T, VID, PORT, detail::type_list<>> : std::false_type {};

        template<typename T, std::size_t VID, std::size_t PORT, typename E0, typename... Rest>
        struct has_input_edge_impl<T, VID, PORT, detail::type_list<E0, Rest...>> {
            using tr = detail::edge_traits<E0>;
            static constexpr bool match = std::is_same_v<T, typename detail::edge_data_type<E0>::type> &&
                (tr::dst_id == VID) && (tr::dst_port_index == PORT);
            static constexpr bool value = match ? true : has_input_edge_impl<T, VID, PORT, detail::type_list<Rest...>>::value;
        };

        template<typename T, std::size_t VID, std::size_t PORT>
        static constexpr bool has_input_edge() {
            return has_input_edge_impl<T, VID, PORT, flattened_edges_t>::value;
        }

        template<typename T, std::size_t VID, std::size_t PORT, typename EdgeList>
        struct has_output_edge_impl;

        template<typename T, std::size_t VID, std::size_t PORT>
        struct has_output_edge_impl<T, VID, PORT, detail::type_list<>> : std::false_type {};

        template<typename T, std::size_t VID, std::size_t PORT, typename E0, typename... Rest>
        struct has_output_edge_impl<T, VID, PORT, detail::type_list<E0, Rest...>> {
            using tr = detail::edge_traits<E0>;
            static constexpr bool match = std::is_same_v<T, typename detail::edge_data_type<E0>::type> &&
                (tr::src_id == VID) && (tr::src_port_index == PORT);
            static constexpr bool value = match ? true : has_output_edge_impl<T, VID, PORT, detail::type_list<Rest...>>::value;
        };

        template<typename T, std::size_t VID, std::size_t PORT>
        static constexpr bool has_output_edge() {
            return has_output_edge_impl<T, VID, PORT, flattened_edges_t>::value;
        }

        template<typename T, std::size_t NodeIndex, std::size_t PortIndex>
        static constexpr std::size_t input_index_for() {
            constexpr std::size_t vid = topology_t::template id_at<NodeIndex>();
            if constexpr (has_input_edge<T, vid, PortIndex>()) {
                return coloring_t<T>::template input_data_index<vid, PortIndex>();
            }
            else {
                return invalid_index;
            }
        }

        template<typename T, std::size_t NodeIndex, std::size_t PortIndex>
        static constexpr std::size_t output_index_for() {
            constexpr std::size_t vid = topology_t::template id_at<NodeIndex>();
            if constexpr (has_output_edge<T, vid, PortIndex>()) {
                return coloring_t<T>::template output_data_index<vid, PortIndex>();
            }
            else {
                return invalid_index;
            }
        }


    };

} // namespace ugraph::detail
