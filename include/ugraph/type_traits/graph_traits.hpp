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

    template<typename... edges_t>
    struct data_graph_traits {
        using topology_t = Topology<edges_t...>;

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
            if constexpr (S::id() == id) {
                return &e.first.module();
            }
            else {
                using D = typename detail::edge_traits<Edge>::dst_vertex_t;
                if constexpr (D::id() == id) {
                    return &e.second.module();
                }
                else {
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
        using edge_list_for_t = typename detail::filter_edges<T, detail::type_list<edges_t...>>::type;

        template<typename T>
        using coloring_t = typename detail::coloring_or_empty<topology_t, edge_list_for_t<T>>::type;

        template<typename T, std::size_t VID, std::size_t PORT, typename... Es>
        struct has_input_edge_impl;

        template<typename T, std::size_t VID, std::size_t PORT>
        struct has_input_edge_impl<T, VID, PORT> : std::false_type {};

        template<typename T, std::size_t VID, std::size_t PORT, typename E0, typename... Rest>
        struct has_input_edge_impl<T, VID, PORT, E0, Rest...> {
            using tr = detail::edge_traits<E0>;
            static constexpr bool match = std::is_same_v<T, typename detail::edge_data_type<E0>::type> &&
                (tr::dst_id == VID) && (tr::dst_port_index == PORT);
            static constexpr bool value = match ? true : has_input_edge_impl<T, VID, PORT, Rest...>::value;
        };

        template<typename T, std::size_t VID, std::size_t PORT>
        static constexpr bool has_input_edge() {
            return has_input_edge_impl<T, VID, PORT, edges_t...>::value;
        }

        template<typename T, std::size_t VID, std::size_t PORT, typename... Es>
        struct has_output_edge_impl;

        template<typename T, std::size_t VID, std::size_t PORT>
        struct has_output_edge_impl<T, VID, PORT> : std::false_type {};

        template<typename T, std::size_t VID, std::size_t PORT, typename E0, typename... Rest>
        struct has_output_edge_impl<T, VID, PORT, E0, Rest...> {
            using tr = detail::edge_traits<E0>;
            static constexpr bool match = std::is_same_v<T, typename detail::edge_data_type<E0>::type> &&
                (tr::src_id == VID) && (tr::src_port_index == PORT);
            static constexpr bool value = match ? true : has_output_edge_impl<T, VID, PORT, Rest...>::value;
        };

        template<typename T, std::size_t VID, std::size_t PORT>
        static constexpr bool has_output_edge() {
            return has_output_edge_impl<T, VID, PORT, edges_t...>::value;
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
