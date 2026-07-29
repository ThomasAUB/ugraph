#pragma once

#include <tuple>
#include <type_traits>

#include "graph_coloring.hpp"
#include "../graph_io.hpp"
#include "edge_info.hpp"

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
            "Edge connects ports with incompatible value types — source and destination IO specs must use the same T in IO<T, ...>"
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



    // ---------------------------------------------------------------------------
    // Edge-pack queries.
    //
    // These used to be recursive class templates nested inside data_graph_traits, so a
    // full walk of the edge pack was instantiated afresh for every (spec, node, port)
    // triple -- O(nodes * edges) class instantiations, which dominated compile time.
    //
    // They are now namespace-scope templates whose body is a single fold expression over
    // per-edge facts memoized in edge_info<E>. Each query costs one instantiation, and the
    // per-edge work is shared across every query in the translation unit.
    // ---------------------------------------------------------------------------

    template<typename T, std::size_t VID, std::size_t PORT, typename EdgeList>
    struct has_input_edge_for;

    template<typename T, std::size_t VID, std::size_t PORT, typename... Es>
    struct has_input_edge_for<T, VID, PORT, type_list<Es...>> {
        static constexpr bool value = ((
            edge_info<Es>::dst_id == VID &&
            edge_info<Es>::dst_port == PORT &&
            std::is_same_v<T, typename edge_key_type<Es>::type>
            ) || ... || false);
    };

    template<typename T, std::size_t VID, std::size_t PORT, typename EdgeList>
    struct has_output_edge_for;

    template<typename T, std::size_t VID, std::size_t PORT, typename... Es>
    struct has_output_edge_for<T, VID, PORT, type_list<Es...>> {
        static constexpr bool value = ((
            edge_info<Es>::src_id == VID &&
            edge_info<Es>::src_port == PORT &&
            std::is_same_v<T, typename edge_key_type<Es>::type>
            ) || ... || false);
    };

    template<typename spec_t, std::size_t VID, std::size_t PORT, typename EdgeList>
    struct has_explicit_input_binding_for;

    template<typename spec_t, std::size_t VID, std::size_t PORT, typename... Es>
    struct has_explicit_input_binding_for<spec_t, VID, PORT, type_list<Es...>> {
        static constexpr bool value = ((
            edge_info<Es>::binds_input &&
            edge_info<Es>::bind_id == VID &&
            edge_info<Es>::bind_port == PORT &&
            std::is_same_v<typename edge_info<Es>::bind_spec, spec_t>
            ) || ... || false);
    };

    template<typename spec_t, std::size_t VID, std::size_t PORT, typename EdgeList>
    struct has_explicit_output_binding_for;

    template<typename spec_t, std::size_t VID, std::size_t PORT, typename... Es>
    struct has_explicit_output_binding_for<spec_t, VID, PORT, type_list<Es...>> {
        static constexpr bool value = ((
            edge_info<Es>::binds_output &&
            edge_info<Es>::bind_id == VID &&
            edge_info<Es>::bind_port == PORT &&
            std::is_same_v<typename edge_info<Es>::bind_spec, spec_t>
            ) || ... || false);
    };

    // Counts edges *and* explicit bindings landing on one input port.
    template<typename spec_t, std::size_t VID, std::size_t PORT, typename EdgeList>
    struct input_connection_count_for;

    template<typename spec_t, std::size_t VID, std::size_t PORT, typename... Es>
    struct input_connection_count_for<spec_t, VID, PORT, type_list<Es...>> {
        static constexpr std::size_t value = (0u + ... + (
            ((!edge_info<Es>::is_binding &&
              edge_info<Es>::dst_id == VID &&
              edge_info<Es>::dst_port == PORT &&
              std::is_same_v<typename edge_info<Es>::dst_spec, spec_t>) ||
             (edge_info<Es>::binds_input &&
              edge_info<Es>::bind_id == VID &&
              edge_info<Es>::bind_port == PORT &&
              std::is_same_v<typename edge_info<Es>::bind_spec, spec_t>)) ? 1u : 0u
            ));
    };

    template<typename spec_t, std::size_t VID, std::size_t PORT, typename EdgeList>
    struct output_connection_count_for;

    template<typename spec_t, std::size_t VID, std::size_t PORT, typename... Es>
    struct output_connection_count_for<spec_t, VID, PORT, type_list<Es...>> {
        static constexpr std::size_t value = (0u + ... + (
            (!edge_info<Es>::is_binding &&
             edge_info<Es>::src_id == VID &&
             edge_info<Es>::src_port == PORT &&
             std::is_same_v<typename edge_info<Es>::src_spec, spec_t>) ? 1u : 0u
            ));
    };

    template<typename T>
    struct type_identity_t { using type = T; };

    template<std::size_t I, typename EdgeList>
    struct edge_key_at {
        using type = typename edge_key_type<typename type_list_at<I, EdgeList>::type>::type;
    };

    // Key type of the first edge feeding (VID, PORT) for spec_t, or void if there is none.
    template<typename spec_t, std::size_t VID, std::size_t PORT, typename EdgeList>
    struct input_edge_key_for;

    template<typename spec_t, std::size_t VID, std::size_t PORT, typename... Es>
    struct input_edge_key_for<spec_t, VID, PORT, type_list<Es...>> {

        static constexpr std::size_t match_index() {
            std::size_t i = 0;
            std::size_t found = edge_npos;
            (((found == edge_npos &&
               edge_info<Es>::dst_id == VID &&
               edge_info<Es>::dst_port == PORT &&
               std::is_same_v<typename edge_info<Es>::dst_spec, spec_t>)
                ? (found = i, void())
                : void(), ++i), ...);
            return found;
        }

        static constexpr std::size_t index = match_index();

        // conditional_t only names the branches; ::type instantiates just the chosen one,
        // so edge_key_at is never instantiated with the sentinel index.
        using type = typename std::conditional_t<
            index == edge_npos,
            type_identity_t<void>,
            edge_key_at<index == edge_npos ? 0 : index, type_list<Es...>>
        >::type;
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

        // Which link supplies the module pointer for a given vertex.
        //
        // The previous shape folded every edge for every node, instantiating
        // try_edge_module<Id, Edge> once per (node, edge) pair -- O(nodes * edges) functions.
        // Here the edge endpoints are flattened into constexpr arrays once and the provider is
        // located by a constant-evaluated scan, so only O(nodes + edges) functions exist.
        // The scan order (first link in declaration order, source side before destination)
        // reproduces the original fold exactly.
        static constexpr std::size_t link_count = sizeof...(edges_t);
        static constexpr std::size_t link_arr_size = link_count == 0 ? 1 : link_count;

        static constexpr std::array<bool, link_arr_size> link_is_binding { detail::edge_info<edges_t>::is_binding... };
        static constexpr std::array<std::size_t, link_arr_size> link_src_id { detail::edge_info<edges_t>::src_id... };
        static constexpr std::array<std::size_t, link_arr_size> link_dst_id { detail::edge_info<edges_t>::dst_id... };

        struct module_source_t {
            std::size_t link_index;
            bool from_src;
        };

        static constexpr module_source_t module_source_for(std::size_t vid) {
            for (std::size_t i = 0; i < link_count; ++i) {
                if (link_is_binding[i]) {
                    continue;
                }
                if (link_src_id[i] == vid) {
                    return { i, true };
                }
                if (link_dst_id[i] == vid) {
                    return { i, false };
                }
            }
            return { invalid_index, false };
        }

        template<std::size_t I, typename links_tuple_t>
        static constexpr auto module_ptr_at(const links_tuple_t& links) {
            constexpr module_source_t source = module_source_for(topology_t::id_at(I));
            static_assert(source.link_index != invalid_index, "No edge provides a module for this vertex");
            const auto& e = std::get<source.link_index>(links);
            if constexpr (source.from_src) {
                return &e.first.module();
            }
            else {
                return &e.second.module();
            }
        }

        template<std::size_t... I>
        static constexpr modules_tuple_t build_modules(std::index_sequence<I...>, const edges_t&... es) {
            return modules_tuple_t { module_ptr_at<I>(std::forward_as_tuple(es...))... };
        }

        template<typename T>
        using edge_list_for_t = typename detail::filter_edges<T, edge_types_list>::type;

        template<typename T>
        using coloring_t = typename detail::coloring_or_empty<topology_t, edge_list_for_t<T>>::type;

        template<typename T, std::size_t VID, std::size_t PORT>
        static constexpr bool has_input_edge() {
            return detail::has_input_edge_for<T, VID, PORT, edge_types_list>::value;
        }

        template<typename T, std::size_t VID, std::size_t PORT>
        static constexpr bool has_output_edge() {
            return detail::has_output_edge_for<T, VID, PORT, edge_types_list>::value;
        }

        template<typename T, std::size_t NodeIndex, std::size_t PortIndex>
        static constexpr std::size_t input_index_for() {
            constexpr std::size_t vid = topology_t::id_at(NodeIndex);
            if constexpr (has_input_edge<T, vid, PortIndex>()) {
                return coloring_t<T>::input_data_index(vid, PortIndex);
            }
            else {
                return invalid_index;
            }
        }

        template<typename T, std::size_t NodeIndex, std::size_t PortIndex>
        static constexpr std::size_t output_index_for() {
            constexpr std::size_t vid = topology_t::id_at(NodeIndex);
            if constexpr (has_output_edge<T, vid, PortIndex>()) {
                return coloring_t<T>::output_data_index(vid, PortIndex);
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
            constexpr std::size_t vid = topology_t::id_at(NodeIndex);
            return detail::has_explicit_input_binding_for<spec_t, vid, PortIndex, all_edge_types_list>::value;
        }

        template<typename spec_t, std::size_t NodeIndex, std::size_t PortIndex>
        static constexpr bool has_explicit_output_binding_for_spec() {
            constexpr std::size_t vid = topology_t::id_at(NodeIndex);
            return detail::has_explicit_output_binding_for<spec_t, vid, PortIndex, all_edge_types_list>::value;
        }

        template<typename spec_t, std::size_t NodeIndex, std::size_t PortIndex>
        static constexpr std::size_t input_connection_count_for_spec() {
            constexpr std::size_t vid = topology_t::id_at(NodeIndex);
            return detail::input_connection_count_for<spec_t, vid, PortIndex, all_edge_types_list>::value;
        }

        template<typename spec_t, std::size_t NodeIndex, std::size_t PortIndex>
        static constexpr std::size_t output_connection_count_for_spec() {
            constexpr std::size_t vid = topology_t::id_at(NodeIndex);
            return detail::output_connection_count_for<spec_t, vid, PortIndex, all_edge_types_list>::value;
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
            static constexpr std::size_t vid = topology_t::id_at(NodeIndex);
            using type = typename detail::input_edge_key_for<spec_t, vid, PortIndex, edge_types_list>::type;
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
