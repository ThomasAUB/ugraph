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
#include <tuple>
#include <type_traits>
#include <utility>

#include "context.hpp"
#include "manifest.hpp"
#include "topology.hpp"
#include "graph_printer.hpp"
#include "type_traits/type_list.hpp"
#include "type_traits/edge_traits.hpp"
#include "type_traits/graph_traits.hpp"

namespace ugraph {

    template<typename data_t, typename tuple_t, std::size_t I = 0, bool InRange = (I < std::tuple_size_v<tuple_t>)>
    struct tuple_index_of_type_impl;

    template<typename... edges_t>
    class ExternalDataGraph {

        using traits = detail::data_graph_traits<edges_t...>;
        using topology_t = typename traits::topology_t;
        static_assert(!topology_t::is_cyclic(), "Cycle detected in graph definition");

        template<std::size_t I>
        using node_type_at = typename traits::template node_type_at<I>;

        using manifest_t = typename traits::manifest_t;
        using modules_tuple_impl_t = typename traits::modules_tuple_t;

        template<typename key_t, typename data_t, std::size_t Count>
        struct graph_data_slot {
            using spec_type = key_t;
            using data_type = data_t;
            std::array<data_type, Count> data {};
        };

        template<std::size_t... I>
        static constexpr auto make_contexts_tuple_t(std::index_sequence<I...>) ->
            std::tuple<Context<typename node_type_at<I>::module_type::Manifest>...>;

        using contexts_tuple_t = decltype(make_contexts_tuple_t(std::make_index_sequence<topology_t::size()>{}));

        template<typename E>
        static constexpr void process_binding_fn(const E& e, ExternalDataGraph* self) {
            if constexpr (detail::is_data_binding<E>::value) {
                using bind_t = E;
                using bt = ::ugraph::binding_traits<bind_t>;
                using port_t = typename bt::port_type;
                using spec_t = typename port_t::spec_type;
                constexpr std::size_t node_id = port_t::node_type::id();
                constexpr std::size_t port_index = port_t::index();
                if constexpr (detail::is_output_port<port_t>::value) {
                    self->template bind_output_key_at<node_id, port_index, spec_t>(*e.mPtr);
                }
                else if constexpr (detail::is_input_port<port_t>::value) {
                    self->template bind_input_key_at<node_id, port_index, spec_t>(*e.mPtr);
                }
            }
        }

        template<std::size_t... I>
        static constexpr auto make_graph_data_storage_t(std::index_sequence<I...>) ->
            std::tuple<
            graph_data_slot<
            typename traits::template key_type_at<I>,
            typename manifest_t::template data_type_for<typename traits::template key_type_at<I>>,
            traits::template coloring_t<typename traits::template key_type_at<I>>::data_count()
            >...
            >;

        modules_tuple_impl_t mModules;
        contexts_tuple_t mContexts;

    public:

        using topology_type = topology_t;
        using Manifest = manifest_t;
        using vertex_types_list_public = typename topology_t::vertex_types_list_public;
        using edge_types_list_public = typename traits::flattened_edges_t;

        class graph_data_t {
            using storage_t = decltype(make_graph_data_storage_t(std::make_index_sequence<traits::key_count>{}));

            template<typename data_t>
            using slot_t = std::tuple_element_t<tuple_index_of_type_impl<data_t, storage_t>::value, storage_t>;

            template<typename data_t>
            using array_t = decltype(std::declval<slot_t<data_t>&>().data);

        public:

            template<typename data_t>
            static constexpr std::size_t count() {
                static_assert(tuple_index_of_type_impl<data_t, storage_t>::value != static_cast<std::size_t>(-1), "Type not found in graph_data_t");
                return std::tuple_size_v<array_t<data_t>>;
            }

            template<typename data_t>
            constexpr auto& slots() {
                constexpr std::size_t index = tuple_index_of_type_impl<data_t, storage_t>::value;
                static_assert(index != static_cast<std::size_t>(-1), "Type not found in graph_data_t");
                return std::get<index>(mData).data;
            }

            template<typename data_t>
            constexpr const auto& slots() const {
                constexpr std::size_t index = tuple_index_of_type_impl<data_t, storage_t>::value;
                static_assert(index != static_cast<std::size_t>(-1), "Type not found in graph_data_t");
                return std::get<index>(mData).data;
            }

            template<typename data_t>
            constexpr auto& slot(std::size_t i) {
                return slots<data_t>()[i];
            }

            template<typename data_t>
            constexpr const auto& slot(std::size_t i) const {
                return slots<data_t>()[i];
            }

        private:

            storage_t mData {};
        };

    public:

        constexpr ExternalDataGraph(const edges_t&... es) :
            mModules(traits::build_modules(std::make_index_sequence<topology_t::size()>{}, es...)) {
            (process_binding_fn(es, this), ...);

            static_assert(
                is_fully_wired_impl(std::make_index_sequence<topology_t::size()>{}),
                "The graph is missing connections"
                );
        }

        static constexpr auto ids() { return topology_t::ids(); }
        static constexpr std::size_t size() { return topology_t::size(); }
        static constexpr auto edges() { return topology_t::edges(); }

        template<std::size_t node_id>
        constexpr auto module_ptr_by_id()
            -> typename topology_t::template find_type_by_id<node_id>::type::module_type* {
            static_assert(topology_t::template has_id<node_id>(), "Invalid node id");
            constexpr std::size_t node_index = [] () constexpr {
                constexpr auto ids = topology_t::ids();
                for (std::size_t i = 0; i < topology_t::size(); ++i) {
                    if (ids[i] == node_id) {
                        return i;
                    }
                }
                return static_cast<std::size_t>(-1);
                }();
            static_assert(node_index != static_cast<std::size_t>(-1), "Invalid node id");
            return std::get<node_index>(mModules);
        }

            template<typename F>
        constexpr void for_each(F&& f) {
            for_each_impl(std::forward<F>(f), std::make_index_sequence<topology_t::size()>{});
        }

        constexpr void init(graph_data_t& graphData) {
            init_graph_data(graphData);
        }

        template<typename stream_t>
        void print(stream_t& stream, const std::string_view& inGraphName = "") const {
            ugraph::print_graph<topology_t>(stream, inGraphName);
        }

    private:

        template<std::size_t node_id, std::size_t output_index, typename key_t, typename data_t>
        constexpr void bind_output_key_at(data_t& data) {

            constexpr std::size_t node_index = [] () constexpr {
                constexpr auto ids = topology_t::ids();
                for (std::size_t i = 0; i < topology_t::size(); ++i) if (ids[i] == node_id) return i;
                return static_cast<std::size_t>(-1);
                }();
            static_assert(node_index != static_cast<std::size_t>(-1), "Invalid node id");
            using node_type = node_type_at<node_index>;
            using node_manifest = typename node_type::module_type::Manifest;
            using spec_t = typename node_manifest::template spec_for<key_t>;
            using stream_key_t = typename manifest_t::template key_for<spec_t>;
            using storage_t = typename detail::io_traits<spec_t>::type;
            static_assert(node_manifest::template contains<key_t>, "Type not declared in node Manifest");
            static_assert(std::is_same_v<std::remove_cv_t<std::remove_reference_t<data_t>>, storage_t>, "Bound data type does not match node output type");
            constexpr std::size_t out_count = node_manifest::template output_count<key_t>();
            static_assert(output_index < out_count, "Invalid output index for this node/type");

            // Ensure the specific output port is not already connected in the graph
            constexpr bool is_connected = (
                traits::template output_index_for<stream_key_t, node_index, output_index>() !=
                traits::invalid_index
                );
            static_assert(
                !is_connected,
                "Requested output port is already connected; cannot bind_output_at"
                );

            auto& ctx = std::get<node_index>(mContexts);
            ctx.template set_output_ptr<output_index, spec_t>(&data);
        }

        template<std::size_t node_id, std::size_t input_index, typename key_t, typename data_t>
        constexpr void bind_input_key_at(data_t& data) {
            constexpr std::size_t node_index = [] () constexpr {
                constexpr auto ids = topology_t::ids();
                for (std::size_t i = 0; i < topology_t::size(); ++i) if (ids[i] == node_id) return i;
                return static_cast<std::size_t>(-1);
                }();
            static_assert(node_index != static_cast<std::size_t>(-1), "Invalid node id");
            using node_type = node_type_at<node_index>;
            using node_manifest = typename node_type::module_type::Manifest;
            using spec_t = typename node_manifest::template spec_for<key_t>;
            using stream_key_t = typename manifest_t::template key_for<spec_t>;
            using storage_t = typename detail::io_traits<spec_t>::type;
            static_assert(node_manifest::template contains<key_t>, "Type not declared in node Manifest");
            static_assert(std::is_same_v<std::remove_cv_t<std::remove_reference_t<data_t>>, storage_t>, "Bound data type does not match node input type");
            constexpr std::size_t in_count = node_manifest::template input_count<key_t>();
            static_assert(input_index < in_count, "Invalid input index for this node/type");

            // Ensure the specific input port is not already connected in the graph
            constexpr bool is_connected = (
                traits::template input_index_for<stream_key_t, node_index, input_index>() !=
                traits::invalid_index
                );
            static_assert(!is_connected, "Requested input port is already connected; cannot bind_input_at");

            auto& ctx = std::get<node_index>(mContexts);
            ctx.template set_input_ptr<input_index, spec_t>(&data);
        }

        template<typename F, std::size_t... I>
        constexpr void for_each_impl(F&& f, std::index_sequence<I...>) {
            auto&& fn = f;
            (fn(*std::get<I>(mModules), std::get<I>(mContexts)), ...);
        }

        constexpr void init_graph_data(graph_data_t& graphData) {
            init_graph_data_impl(graphData, std::make_index_sequence<topology_t::size()>{});
        }

        template<std::size_t node_index, std::size_t... tidx>
        constexpr void init_node_types(graph_data_t& graphData, std::index_sequence<tidx...>) {
            using node_type = node_type_at<node_index>;
            using node_manifest = typename node_type::module_type::Manifest;
            auto& ctx = std::get<node_index>(mContexts);
            (init_type<node_index, typename node_manifest::template spec_at<tidx>>(graphData, ctx), ...);
        }

        template<std::size_t node_index, typename spec_t, typename ctx_t>
        constexpr void init_type(graph_data_t& graphData, ctx_t& ctx) {
            using node_type = node_type_at<node_index>;
            using node_manifest = typename node_type::module_type::Manifest;;
            using data_t = typename detail::io_traits<spec_t>::type;
            using key_t = typename manifest_t::template key_for<spec_t>;
            auto& arr = graphData.template slots<key_t>();

            constexpr std::size_t in_count = node_manifest::template input_count<spec_t>();
            init_inputs_impl<node_index, spec_t>(ctx, arr, std::make_index_sequence<in_count>{});

            constexpr std::size_t out_count = node_manifest::template output_count<spec_t>();
            init_outputs_impl<node_index, spec_t>(ctx, arr, std::make_index_sequence<out_count>{});
        }

        template<std::size_t node_index, typename spec_t, typename ctx_t, std::size_t... ps>
        constexpr void init_inputs_impl(
            ctx_t& ctx,
            std::array<typename detail::io_traits<spec_t>::type, traits::template coloring_t<typename manifest_t::template key_for<spec_t>>::data_count()>& arr,
            std::index_sequence<ps...>
        ) {
            (([&] {
                constexpr std::size_t data_index = traits::template input_index_for<typename manifest_t::template key_for<spec_t>, node_index, ps>();
                if constexpr (data_index != traits::invalid_index) {
                    ctx.template set_input_ptr<ps, spec_t>(&arr[data_index]);
                }
                }()), ...);
        }

        template<std::size_t node_index, typename spec_t, typename ctx_t, std::size_t... ps>
        constexpr void init_outputs_impl(
            ctx_t& ctx,
            std::array<typename detail::io_traits<spec_t>::type, traits::template coloring_t<typename manifest_t::template key_for<spec_t>>::data_count()>& arr,
            std::index_sequence<ps...>
        ) {
            (([&] {
                constexpr std::size_t data_index = traits::template output_index_for<typename manifest_t::template key_for<spec_t>, node_index, ps>();
                if constexpr (data_index != traits::invalid_index) {
                    ctx.template set_output_ptr<ps, spec_t>(&arr[data_index]);
                }
                }()), ...);
        }

        template<std::size_t... Is>
        constexpr void init_graph_data_impl(graph_data_t& graphData, std::index_sequence<Is...>) {
            (
                init_node_types<Is>(
                    graphData,
                    std::make_index_sequence<node_type_at<Is>::module_type::Manifest::spec_count>{}
                ), ...
                );
        }

        template<typename T>
        struct is_input_binding : std::false_type {};

        template<typename data_t, typename in_port_t>
        struct is_input_binding<InDataBind<data_t, in_port_t>> : std::true_type {};

        template<typename T>
        struct is_output_binding : std::false_type {};

        template<typename data_t, typename out_port_t>
        struct is_output_binding<OutDataBind<data_t, out_port_t>> : std::true_type {};

        template<typename spec_t, std::size_t node_id, std::size_t input_index, typename link_t, bool IsInputBinding = is_input_binding<link_t>::value>
        struct input_binding_matches : std::false_type {};

        template<typename spec_t, std::size_t node_id, std::size_t input_index, typename link_t>
        struct input_binding_matches<spec_t, node_id, input_index, link_t, true>
            : std::bool_constant<
            std::is_same_v<spec_t, typename binding_traits<link_t>::port_type::spec_type> &&
            (binding_traits<link_t>::port_type::node_type::id() == node_id) &&
            (binding_traits<link_t>::port_type::index() == input_index)
            > {};

        template<typename spec_t, std::size_t node_id, std::size_t input_index>
        static constexpr bool has_input_binding() {
            return (false || ... || input_binding_matches<spec_t, node_id, input_index, edges_t>::value);
        }

        template<typename spec_t, std::size_t node_id, std::size_t output_index, typename link_t, bool IsOutputBinding = is_output_binding<link_t>::value>
        struct output_binding_matches : std::false_type {};

        template<typename spec_t, std::size_t node_id, std::size_t output_index, typename link_t>
        struct output_binding_matches<spec_t, node_id, output_index, link_t, true>
            : std::bool_constant<
            std::is_same_v<spec_t, typename binding_traits<link_t>::port_type::spec_type> &&
            (binding_traits<link_t>::port_type::node_type::id() == node_id) &&
            (binding_traits<link_t>::port_type::index() == output_index)
            > {};

        template<typename spec_t, std::size_t node_id, std::size_t output_index>
        static constexpr bool has_output_binding() {
            return (false || ... || output_binding_matches<spec_t, node_id, output_index, edges_t>::value);
        }

        template<std::size_t node_index>
        static constexpr bool is_entry_node() {
            constexpr std::size_t node_id = topology_t::template id_at<node_index>();
            constexpr auto graph_edges = topology_t::edges();
            for (const auto& edge : graph_edges) {
                if (edge.second == node_id) {
                    return false;
                }
            }
            return true;
        }

        template<std::size_t node_index>
        static constexpr bool is_exit_node() {
            constexpr std::size_t node_id = topology_t::template id_at<node_index>();
            constexpr auto graph_edges = topology_t::edges();
            for (const auto& edge : graph_edges) {
                if (edge.first == node_id) {
                    return false;
                }
            }
            return true;
        }

        template<std::size_t node_index, typename spec_t, std::size_t... input_indices>
        static constexpr bool spec_inputs_wired_impl(std::index_sequence<input_indices...>) {
            constexpr std::size_t node_id = topology_t::template id_at<node_index>();
            using key_t = typename manifest_t::template key_for<spec_t>;
            return (((traits::template input_index_for<key_t, node_index, input_indices>() != traits::invalid_index) ||
                has_input_binding<spec_t, node_id, input_indices>() ||
                is_entry_node<node_index>()) && ...);
        }

        template<std::size_t node_index, typename spec_t, std::size_t... output_indices>
        static constexpr bool spec_outputs_wired_impl(std::index_sequence<output_indices...>) {
            constexpr std::size_t node_id = topology_t::template id_at<node_index>();
            using key_t = typename manifest_t::template key_for<spec_t>;
            return (((traits::template output_index_for<key_t, node_index, output_indices>() != traits::invalid_index) ||
                has_output_binding<spec_t, node_id, output_indices>() ||
                is_exit_node<node_index>()) && ...);
        }

        template<std::size_t node_index, typename spec_t>
        static constexpr bool spec_dependencies_wired() {
            if constexpr (!detail::io_traits<spec_t>::strict_connection) {
                return true;
            }
            else {
                return spec_inputs_wired_impl<node_index, spec_t>(std::make_index_sequence<spec_t::input_count>{}) &&
                    spec_outputs_wired_impl<node_index, spec_t>(std::make_index_sequence<spec_t::output_count>{});
            }
        }

        template<std::size_t node_index, std::size_t... spec_indices>
        static constexpr bool node_dependencies_wired_impl(std::index_sequence<spec_indices...>) {
            using node_manifest = typename node_type_at<node_index>::module_type::Manifest;
            return (spec_dependencies_wired<node_index, typename node_manifest::template spec_at<spec_indices>>() && ...);
        }

        template<std::size_t... node_indices>
        static constexpr bool is_fully_wired_impl(std::index_sequence<node_indices...>) {
            return (node_dependencies_wired_impl<node_indices>(
                std::make_index_sequence<node_type_at<node_indices>::module_type::Manifest::spec_count>{}
            ) && ...);
        }

    };

    template<typename... edges_t>
    class Graph : public ExternalDataGraph<edges_t...> {
        using base_t = ExternalDataGraph<edges_t...>;

        constexpr void rebind_graph_data() {
            this->init(mGraphData);
        }

    public:

        using typename base_t::topology_type;
        using typename base_t::Manifest;
        using typename base_t::vertex_types_list_public;
        using typename base_t::edge_types_list_public;
        using typename base_t::graph_data_t;

        constexpr Graph(const edges_t&... es) :
            base_t(es...) {
            rebind_graph_data();
        }

        constexpr Graph(const Graph& other) :
            base_t(other),
            mGraphData(other.mGraphData) {
            rebind_graph_data();
        }

        constexpr Graph(Graph&& other) noexcept :
            base_t(std::move(other)),
            mGraphData(std::move(other.mGraphData)) {
            rebind_graph_data();
        }

        constexpr Graph& operator=(const Graph& other) {
            if (this != &other) {
                base_t::operator=(other);
                mGraphData = other.mGraphData;
                rebind_graph_data();
            }
            return *this;
        }

        constexpr Graph& operator=(Graph&& other) noexcept {
            if (this != &other) {
                base_t::operator=(std::move(other));
                mGraphData = std::move(other.mGraphData);
                rebind_graph_data();
            }
            return *this;
        }

        constexpr graph_data_t& graph_data() {
            return mGraphData;
        }

        constexpr const graph_data_t& graph_data() const {
            return mGraphData;
        }

    private:

        graph_data_t mGraphData {};
    };

    template<typename data_t, typename tuple_t, std::size_t I>
    struct tuple_index_of_type_impl<data_t, tuple_t, I, true> {
        using arr_t = std::tuple_element_t<I, tuple_t>;
        using elem_t = typename arr_t::data_type;
        using key_t = typename arr_t::spec_type;
        static constexpr std::size_t value = (std::is_same_v<data_t, elem_t> || std::is_same_v<data_t, key_t>)
            ? I
            : tuple_index_of_type_impl<data_t, tuple_t, I + 1>::value;
    };

    template<typename data_t, typename tuple_t, std::size_t I>
    struct tuple_index_of_type_impl<data_t, tuple_t, I, false> {
        static constexpr std::size_t value = static_cast<std::size_t>(-1);
    };

    template<typename E0, typename... ERest>
    ExternalDataGraph(E0 const&, ERest const&...) -> ExternalDataGraph<std::decay_t<E0>, std::decay_t<ERest>...>;

    template<typename E0, typename... ERest>
    Graph(E0 const&, ERest const&...) -> Graph<std::decay_t<E0>, std::decay_t<ERest>...>;

} // namespace ugraph
