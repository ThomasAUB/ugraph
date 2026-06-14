#pragma once

#include <tuple>
#include <type_traits>

#include "graph_coloring.hpp"
#include "../graph_io.hpp"

namespace ugraph::detail {





    template<typename Edge, typename = void>
    struct edge_spec_type;

    template<typename Edge>
    struct edge_spec_type<Edge, std::void_t<typename Edge::first_type>> {
        using type = typename Edge::first_type::spec_type;
    };

    template<typename Edge, typename = void>
    struct edge_data_type;

    template<typename Edge>
    struct edge_data_type<Edge, std::void_t<typename edge_spec_type<Edge>::type>> {
        using type = typename io_traits<typename edge_spec_type<Edge>::type>::type;
    };

    template<typename Edge, typename = void>
    struct edge_dst_spec_type;

    template<typename Edge>
    struct edge_dst_spec_type<Edge, std::void_t<typename Edge::second_type>> {
        using type = typename Edge::second_type::spec_type;
    };

    template<typename src_spec_t, typename dst_spec_t>
    struct connection_key_type {
        using src_traits = io_traits<src_spec_t>;
        using dst_traits = io_traits<dst_spec_t>;

        static_assert(
            std::is_same_v<typename src_traits::type, typename dst_traits::type>,
            "Connected ports must use the same value type"
            );

        static constexpr bool use_value_key = !src_traits::is_tagged || !dst_traits::is_tagged;

        using type = std::conditional_t<
            use_value_key,
            typename src_traits::type,
            typename src_traits::tag
        >;
    };

    template<typename Edge>
    struct edge_key_type {
        using type = typename connection_key_type<
            typename edge_spec_type<Edge>::type,
            typename edge_dst_spec_type<Edge>::type
        >::type;
    };

    template<typename T, typename Edge>
    struct edge_is_type : std::false_type {};

    template<typename T, typename S, typename D>
    struct edge_is_type<T, std::pair<S, D>> : std::bool_constant<std::is_same_v<typename edge_key_type<std::pair<S, D>>::type, T>> {};

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

    template<typename TL>
    struct specs_to_keys;

    template<typename... Ts>
    struct specs_to_keys<detail::type_list<Ts...>> {
        using type = typename fold_append_unique<detail::type_list<>, typename io_key<Ts>::type...>::type;
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

    template<typename TL>
    struct filter_out_data_bindings;

    template<>
    struct filter_out_data_bindings<detail::type_list<>> { using type = detail::type_list<>; };

    template<typename E, typename... Rest>
    struct filter_out_data_bindings<detail::type_list<E, Rest...>> {
        using tail = typename filter_out_data_bindings<detail::type_list<Rest...>>::type;
        using type = std::conditional_t<
            detail::is_data_binding<E>::value,
            tail,
            typename detail::type_list_prepend<E, tail>::type
        >;
    };



    template<typename... edges_t>
    struct data_graph_traits {
        template<typename TL>
        struct type_list_to_topology;

        template<typename... Es>
        struct type_list_to_topology<detail::type_list<Es...>> { using type = Topology<Es...>; };

        using edge_types_list = typename filter_out_data_bindings<detail::type_list<edges_t...>>::type;
        using all_edge_types_list = detail::type_list<edges_t...>;
        using topology_t = typename type_list_to_topology<edge_types_list>::type;

        template<std::size_t I>
        using node_type_at = typename topology_t::template type_at<I>;

        template<std::size_t Id>
        using module_ptr_for_id_t = typename topology_t::template find_type_by_id<Id>::type::module_type*;

        using graph_types_list = typename collect_specs_from_typelist<typename topology_t::vertex_types_list>::type;
        using graph_keys_list = typename specs_to_keys<graph_types_list>::type;
        using manifest_t = typename manifest_from_list<graph_types_list>::type;
        using external_inputs = typename ugraph::external_inputs_t<typename topology_t::vertex_types_list>;
        using external_outputs = typename ugraph::external_outputs_t<typename topology_t::vertex_types_list>;
        static constexpr std::size_t invalid_index = static_cast<std::size_t>(-1);

        static constexpr std::size_t key_count = detail::type_list_size<graph_keys_list>::value;

        template<std::size_t I>
        using key_type_at = typename detail::type_list_at<I, graph_keys_list>::type;

        template<std::size_t... I>
        static constexpr auto make_modules_tuple_t(std::index_sequence<I...>) ->
            std::tuple<typename topology_t::template type_at<I>::module_type*...>;

        using modules_tuple_t = decltype(make_modules_tuple_t(std::make_index_sequence<topology_t::size()>{}));

        template<std::size_t Id, typename Edge>
        static constexpr auto try_edge_module(const Edge& e) {
            if constexpr (detail::is_data_binding<Edge>::value) {
                return static_cast<module_ptr_for_id_t<Id>>(nullptr);
            }
            else {
                using S = typename detail::edge_traits<Edge>::src_vertex_t;
                using D = typename detail::edge_traits<Edge>::dst_vertex_t;

                if constexpr (S::id() == Id) {
                    return &e.first.module();
                }
                else if constexpr (D::id() == Id) {
                    return &e.second.module();
                }
                else {
                    return static_cast<module_ptr_for_id_t<Id>>(nullptr);
                }
            }
        }

        template<std::size_t Id>
        static constexpr auto get_module_ptr(const edges_t&... es) {
            module_ptr_for_id_t<Id> r = nullptr;
            ((r = r ? r : try_edge_module<Id>(es)), ...);
            return r;
        }

        template<std::size_t... I>
        static constexpr modules_tuple_t build_modules(std::index_sequence<I...>, const edges_t&... es) {
            return { get_module_ptr<topology_t::template id_at<I>()>(es...)... };
        }

        template<typename T>
        using edge_list_for_t = typename detail::filter_edges<T, edge_types_list>::type;

        template<typename T>
        using coloring_t = typename detail::coloring_or_empty<topology_t, edge_list_for_t<T>>::type;

        template<typename T, std::size_t VID, std::size_t PORT, typename EdgeList>
        struct has_input_edge_impl;

        template<typename T, std::size_t VID, std::size_t PORT>
        struct has_input_edge_impl<T, VID, PORT, detail::type_list<>> : std::false_type {};

        template<typename T, std::size_t VID, std::size_t PORT, typename E0, typename... Rest>
        struct has_input_edge_impl<T, VID, PORT, detail::type_list<E0, Rest...>> {
            using tr = detail::edge_traits<E0>;
            static constexpr bool match = std::is_same_v<T, typename detail::edge_key_type<E0>::type> &&
                (tr::dst_id == VID) && (tr::dst_port_index == PORT);
            static constexpr bool value = match ? true : has_input_edge_impl<T, VID, PORT, detail::type_list<Rest...>>::value;
        };

        template<typename T, std::size_t VID, std::size_t PORT>
        static constexpr bool has_input_edge() {
            return has_input_edge_impl<T, VID, PORT, edge_types_list>::value;
        }

        template<typename T, std::size_t VID, std::size_t PORT, typename EdgeList>
        struct has_output_edge_impl;

        template<typename T, std::size_t VID, std::size_t PORT>
        struct has_output_edge_impl<T, VID, PORT, detail::type_list<>> : std::false_type {};

        template<typename T, std::size_t VID, std::size_t PORT, typename E0, typename... Rest>
        struct has_output_edge_impl<T, VID, PORT, detail::type_list<E0, Rest...>> {
            using tr = detail::edge_traits<E0>;
            static constexpr bool match = std::is_same_v<T, typename detail::edge_key_type<E0>::type> &&
                (tr::src_id == VID) && (tr::src_port_index == PORT);
            static constexpr bool value = match ? true : has_output_edge_impl<T, VID, PORT, detail::type_list<Rest...>>::value;
        };

        template<typename T, std::size_t VID, std::size_t PORT>
        static constexpr bool has_output_edge() {
            return has_output_edge_impl<T, VID, PORT, edge_types_list>::value;
        }

        template<typename spec_t, std::size_t VID, std::size_t PORT, typename Edge, bool IsDataBinding = detail::is_data_binding<Edge>::value>
        struct input_binding_match : std::false_type {};

        template<typename spec_t, std::size_t VID, std::size_t PORT, typename Edge>
        struct input_binding_match<spec_t, VID, PORT, Edge, true> {
            using port_t = typename ::ugraph::binding_traits<Edge>::port_type;

            static constexpr bool value =
                detail::is_input_port<port_t>::value &&
                !detail::is_output_port<port_t>::value &&
                (port_t::node_type::id() == VID) &&
                (port_t::index() == PORT) &&
                std::is_same_v<typename port_t::spec_type, spec_t>;
        };

        template<typename spec_t, std::size_t VID, std::size_t PORT, typename EdgeList>
        struct has_explicit_input_binding_impl;

        template<typename spec_t, std::size_t VID, std::size_t PORT>
        struct has_explicit_input_binding_impl<spec_t, VID, PORT, detail::type_list<>> : std::false_type {};

        template<typename spec_t, std::size_t VID, std::size_t PORT, typename E0, typename... Rest>
        struct has_explicit_input_binding_impl<spec_t, VID, PORT, detail::type_list<E0, Rest...>> {
            static constexpr bool value =
                input_binding_match<spec_t, VID, PORT, E0>::value ||
                has_explicit_input_binding_impl<spec_t, VID, PORT, detail::type_list<Rest...>>::value;
        };

        template<typename spec_t, std::size_t VID, std::size_t PORT, typename Edge, bool IsDataBinding = detail::is_data_binding<Edge>::value>
        struct output_binding_match : std::false_type {};

        template<typename spec_t, std::size_t VID, std::size_t PORT, typename Edge>
        struct output_binding_match<spec_t, VID, PORT, Edge, true> {
            using port_t = typename ::ugraph::binding_traits<Edge>::port_type;

            static constexpr bool value =
                detail::is_output_port<port_t>::value &&
                (port_t::node_type::id() == VID) &&
                (port_t::index() == PORT) &&
                std::is_same_v<typename port_t::spec_type, spec_t>;
        };

        template<typename spec_t, std::size_t VID, std::size_t PORT, typename EdgeList>
        struct has_explicit_output_binding_impl;

        template<typename spec_t, std::size_t VID, std::size_t PORT>
        struct has_explicit_output_binding_impl<spec_t, VID, PORT, detail::type_list<>> : std::false_type {};

        template<typename spec_t, std::size_t VID, std::size_t PORT, typename E0, typename... Rest>
        struct has_explicit_output_binding_impl<spec_t, VID, PORT, detail::type_list<E0, Rest...>> {
            static constexpr bool value =
                output_binding_match<spec_t, VID, PORT, E0>::value ||
                has_explicit_output_binding_impl<spec_t, VID, PORT, detail::type_list<Rest...>>::value;
        };

        template<typename spec_t, std::size_t VID, std::size_t PORT, typename Edge, bool IsDataBinding>
        struct input_edge_match_impl : std::false_type {};

        template<typename spec_t, std::size_t VID, std::size_t PORT, typename Edge>
        struct input_edge_match_impl<spec_t, VID, PORT, Edge, false>
            : std::bool_constant<
            (detail::edge_traits<Edge>::dst_id == VID) &&
            (detail::edge_traits<Edge>::dst_port_index == PORT) &&
            std::is_same_v<typename detail::edge_traits<Edge>::dst_port_t::spec_type, spec_t>
            > {};

        template<typename spec_t, std::size_t VID, std::size_t PORT, typename Edge>
        struct input_edge_match : input_edge_match_impl<spec_t, VID, PORT, Edge, detail::is_data_binding<Edge>::value> {};

        template<typename spec_t, std::size_t VID, std::size_t PORT, typename EdgeList>
        struct input_connection_count_impl;

        template<typename spec_t, std::size_t VID, std::size_t PORT>
        struct input_connection_count_impl<spec_t, VID, PORT, detail::type_list<>>
            : std::integral_constant<std::size_t, 0> {};

        template<typename spec_t, std::size_t VID, std::size_t PORT, typename E0, typename... Rest>
        struct input_connection_count_impl<spec_t, VID, PORT, detail::type_list<E0, Rest...>>
            : std::integral_constant<
            std::size_t,
            (input_edge_match<spec_t, VID, PORT, E0>::value || input_binding_match<spec_t, VID, PORT, E0>::value ? 1u : 0u) +
            input_connection_count_impl<spec_t, VID, PORT, detail::type_list<Rest...>>::value
            > {};

        template<typename spec_t, std::size_t VID, std::size_t PORT, typename Edge, bool IsDataBinding>
        struct output_edge_match_impl : std::false_type {};

        template<typename spec_t, std::size_t VID, std::size_t PORT, typename Edge>
        struct output_edge_match_impl<spec_t, VID, PORT, Edge, false>
            : std::bool_constant<
            (detail::edge_traits<Edge>::src_id == VID) &&
            (detail::edge_traits<Edge>::src_port_index == PORT) &&
            std::is_same_v<typename detail::edge_traits<Edge>::src_port_t::spec_type, spec_t>
            > {};

        template<typename spec_t, std::size_t VID, std::size_t PORT, typename Edge>
        struct output_edge_match : output_edge_match_impl<spec_t, VID, PORT, Edge, detail::is_data_binding<Edge>::value> {};

        template<typename spec_t, std::size_t VID, std::size_t PORT, typename EdgeList>
        struct output_connection_count_impl;

        template<typename spec_t, std::size_t VID, std::size_t PORT>
        struct output_connection_count_impl<spec_t, VID, PORT, detail::type_list<>>
            : std::integral_constant<std::size_t, 0> {};

        template<typename spec_t, std::size_t VID, std::size_t PORT, typename E0, typename... Rest>
        struct output_connection_count_impl<spec_t, VID, PORT, detail::type_list<E0, Rest...>>
            : std::integral_constant<
            std::size_t,
            (output_edge_match<spec_t, VID, PORT, E0>::value ? 1u : 0u) +
            output_connection_count_impl<spec_t, VID, PORT, detail::type_list<Rest...>>::value
            > {};

        template<typename spec_t, std::size_t VID, std::size_t PORT, typename EdgeList>
        struct input_edge_key_impl;

        template<typename spec_t, std::size_t VID, std::size_t PORT>
        struct input_edge_key_impl<spec_t, VID, PORT, detail::type_list<>> {
            using type = void;
        };

        template<typename spec_t, std::size_t VID, std::size_t PORT, typename E0, typename... Rest>
        struct input_edge_key_impl<spec_t, VID, PORT, detail::type_list<E0, Rest...>> {
            using tr = detail::edge_traits<E0>;
            using type = std::conditional_t<
                (tr::dst_id == VID) &&
                (tr::dst_port_index == PORT) &&
                std::is_same_v<typename tr::dst_port_t::spec_type, spec_t>,
                typename detail::edge_key_type<E0>::type,
                typename input_edge_key_impl<spec_t, VID, PORT, detail::type_list<Rest...>>::type
            >;
        };

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

        template<typename spec_t, std::size_t NodeIndex, std::size_t PortIndex>
        static constexpr std::size_t input_index_for_spec() {
            using key_t = typename input_key_for_spec<spec_t, NodeIndex, PortIndex>::type;
            if constexpr (std::is_same_v<key_t, void>) {
                return invalid_index;
            }
            else {
                return input_index_for<key_t, NodeIndex, PortIndex>();
            }
        }

        template<typename spec_t, std::size_t NodeIndex, std::size_t PortIndex>
        static constexpr std::size_t output_index_for_spec() {
            using tag_key_t = typename io_key<spec_t>::type;
            constexpr std::size_t tagged_index = output_index_for<tag_key_t, NodeIndex, PortIndex>();

            if constexpr (io_traits<spec_t>::is_tagged) {
                using value_key_t = typename io_traits<spec_t>::type;
                constexpr std::size_t value_index = output_index_for<value_key_t, NodeIndex, PortIndex>();
                static_assert(
                    !(tagged_index != invalid_index && value_index != invalid_index),
                    "Tagged output port cannot be connected through both tag and value keys"
                    );
                return tagged_index != invalid_index ? tagged_index : value_index;
            }
            else {
                return tagged_index;
            }
        }

        template<typename spec_t, std::size_t NodeIndex, std::size_t PortIndex>
        static constexpr bool has_explicit_input_binding_for_spec() {
            constexpr std::size_t vid = topology_t::template id_at<NodeIndex>();
            return has_explicit_input_binding_impl<spec_t, vid, PortIndex, all_edge_types_list>::value;
        }

        template<typename spec_t, std::size_t NodeIndex, std::size_t PortIndex>
        static constexpr bool has_explicit_output_binding_for_spec() {
            constexpr std::size_t vid = topology_t::template id_at<NodeIndex>();
            return has_explicit_output_binding_impl<spec_t, vid, PortIndex, all_edge_types_list>::value;
        }

        template<typename spec_t, std::size_t NodeIndex, std::size_t PortIndex>
        static constexpr std::size_t input_connection_count_for_spec() {
            constexpr std::size_t vid = topology_t::template id_at<NodeIndex>();
            return input_connection_count_impl<spec_t, vid, PortIndex, all_edge_types_list>::value;
        }

        template<typename spec_t, std::size_t NodeIndex, std::size_t PortIndex>
        static constexpr std::size_t output_connection_count_for_spec() {
            constexpr std::size_t vid = topology_t::template id_at<NodeIndex>();
            return output_connection_count_impl<spec_t, vid, PortIndex, all_edge_types_list>::value;
        }

        template<std::size_t NodeIndex, typename spec_t, std::size_t... PortIndices>
        static constexpr bool spec_inputs_unique_impl(std::index_sequence<PortIndices...>) {
            return ((input_connection_count_for_spec<spec_t, NodeIndex, PortIndices>() <= 1) && ...);
        }

        template<std::size_t NodeIndex, std::size_t... SpecIndices>
        static constexpr bool node_inputs_unique_impl(std::index_sequence<SpecIndices...>) {
            using node_manifest = typename node_type_at<NodeIndex>::module_type::Manifest;
            return ([] {
                using spec_t = typename node_manifest::template spec_at<SpecIndices>;
                return spec_inputs_unique_impl<NodeIndex, spec_t>(std::make_index_sequence<spec_t::input_count>{});
                }() && ...);
        }

        template<std::size_t... NodeIndices>
        static constexpr bool has_unique_input_connections_impl(std::index_sequence<NodeIndices...>) {
            return (node_inputs_unique_impl<NodeIndices>(
                std::make_index_sequence<node_type_at<NodeIndices>::module_type::Manifest::spec_count>{}
            ) && ...);
        }

        static constexpr bool has_unique_input_connections() {
            return has_unique_input_connections_impl(std::make_index_sequence<topology_t::size()>{});
        }

        template<std::size_t NodeIndex, typename spec_t, std::size_t... PortIndices>
        static constexpr bool spec_outputs_exclusive_impl(std::index_sequence<PortIndices...>) {
            return (((output_connection_count_for_spec<spec_t, NodeIndex, PortIndices>() == 0) ||
                !has_explicit_output_binding_for_spec<spec_t, NodeIndex, PortIndices>()) && ...);
        }

        template<std::size_t NodeIndex, std::size_t... SpecIndices>
        static constexpr bool node_outputs_exclusive_impl(std::index_sequence<SpecIndices...>) {
            using node_manifest = typename node_type_at<NodeIndex>::module_type::Manifest;
            return ([] {
                using spec_t = typename node_manifest::template spec_at<SpecIndices>;
                return spec_outputs_exclusive_impl<NodeIndex, spec_t>(std::make_index_sequence<spec_t::output_count>{});
                }() && ...);
        }

        template<std::size_t... NodeIndices>
        static constexpr bool has_valid_output_connections_impl(std::index_sequence<NodeIndices...>) {
            return (node_outputs_exclusive_impl<NodeIndices>(
                std::make_index_sequence<node_type_at<NodeIndices>::module_type::Manifest::spec_count>{}
            ) && ...);
        }

        static constexpr bool has_valid_output_connections() {
            return has_valid_output_connections_impl(std::make_index_sequence<topology_t::size()>{});
        }

        template<typename spec_t, std::size_t NodeIndex, std::size_t PortIndex>
        struct input_key_for_spec {
            static constexpr std::size_t vid = topology_t::template id_at<NodeIndex>();
            using type = typename input_edge_key_impl<spec_t, vid, PortIndex, edge_types_list>::type;
        };

        template<typename spec_t, std::size_t NodeIndex, std::size_t PortIndex>
        struct output_key_for_spec {
            using tag_key_t = typename io_key<spec_t>::type;
            using type = std::conditional_t<
                output_index_for<tag_key_t, NodeIndex, PortIndex>() != invalid_index,
                tag_key_t,
                typename io_traits<spec_t>::type
            >;
        };


    };

} // namespace ugraph::detail
