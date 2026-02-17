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

    template<typename... edges_t>
    class Graph {

        using traits = detail::data_graph_traits<edges_t...>;
        using topology_t = typename traits::topology_t;
        static_assert(!topology_t::is_cyclic(), "Cycle detected in graph definition");

        template<std::size_t I>
        using node_type_at = typename traits::template node_type_at<I>;

        using manifest_t = typename traits::manifest_t;
        using modules_tuple_impl_t = typename traits::modules_tuple_t;

        template<std::size_t... I>
        static constexpr auto make_contexts_tuple_t(std::index_sequence<I...>) ->
            std::tuple<Context<typename node_type_at<I>::module_type::Manifest>...>;

        using contexts_tuple_t = decltype(make_contexts_tuple_t(std::make_index_sequence<topology_t::size()>{}));

        template<std::size_t... I>
        static constexpr auto make_graph_data_t(std::index_sequence<I...>) ->
            std::tuple<std::array<typename manifest_t::template type_at<I>, traits::template coloring_t<typename manifest_t::template type_at<I>>::data_count()>...>;

        modules_tuple_impl_t mModules;
        contexts_tuple_t mContexts;

    public:

        using topology_type = topology_t;

        constexpr Graph(const edges_t&... es) :
            mModules(traits::build_modules(std::make_index_sequence<topology_t::size()>{}, es...)) {}

        template<typename F>
        constexpr void for_each(F&& f) {
            for_each_impl(std::forward<F>(f), std::make_index_sequence<topology_t::size()>{});
        }

        template<typename T>
        static constexpr std::size_t data_count() {
            return traits::template coloring_t<T>::data_count();
        }

        using graph_data_t = decltype(make_graph_data_t(std::make_index_sequence<manifest_t::type_count>{}));

        constexpr void init_graph_data(graph_data_t& graph_data) {
            init_graph_data_impl(graph_data, std::make_index_sequence<topology_t::size()>{});
        }

        template<std::size_t node_id, typename T>
        constexpr void bind_output(T& t) {
            constexpr std::size_t node_index = [] () constexpr {
                constexpr auto ids = topology_t::ids();
                for (std::size_t i = 0; i < topology_t::size(); ++i) if (ids[i] == node_id) return i;
                return static_cast<std::size_t>(-1);
                }();
            static_assert(node_index != static_cast<std::size_t>(-1), "Invalid node id");
            using node_type = node_type_at<node_index>;
            using node_manifest = typename node_type::module_type::Manifest;
            static_assert(node_manifest::template contains<T>, "Type not declared in node Manifest");
            constexpr std::size_t out_count = node_manifest::template output_count<T>();
            static_assert(out_count > 0, "No output ports for this type");
            static_assert(out_count == 1, "bind_output is only valid for single-output types; use bind_output_at for multi-output types");
            bind_output_at<node_id, 0, T>(t);
        }

        template<std::size_t node_id, std::size_t output_index, typename T>
        constexpr void bind_output_at(T& t) {

            constexpr std::size_t node_index = [] () constexpr {
                constexpr auto ids = topology_t::ids();
                for (std::size_t i = 0; i < topology_t::size(); ++i) if (ids[i] == node_id) return i;
                return static_cast<std::size_t>(-1);
                }();
            static_assert(node_index != static_cast<std::size_t>(-1), "Invalid node id");
            using node_type = node_type_at<node_index>;
            using node_manifest = typename node_type::module_type::Manifest;
            static_assert(node_manifest::template contains<T>, "Type not declared in node Manifest");
            constexpr std::size_t out_count = node_manifest::template output_count<T>();
            static_assert(output_index < out_count, "Invalid output index for this node/type");

            // Ensure the specific output port is not already connected in the graph
            constexpr bool is_connected = (traits::template output_index_for<T, node_index, output_index>() != traits::invalid_index);
            static_assert(!is_connected, "Requested output port is already connected; cannot bind_output_at");

            auto& ctx = std::get<node_index>(mContexts);
            ctx.template set_output_ptr<T, output_index>(&t);
        }

        template<std::size_t node_id, typename T>
        constexpr void bind_input(T& t) {
            constexpr std::size_t node_index = [] () constexpr {
                constexpr auto ids = topology_t::ids();
                for (std::size_t i = 0; i < topology_t::size(); ++i) if (ids[i] == node_id) return i;
                return static_cast<std::size_t>(-1);
                }();
            static_assert(node_index != static_cast<std::size_t>(-1), "Invalid node id");
            using node_type = node_type_at<node_index>;
            using node_manifest = typename node_type::module_type::Manifest;
            static_assert(node_manifest::template contains<T>, "Type not declared in node Manifest");
            constexpr std::size_t in_count = node_manifest::template input_count<T>();
            static_assert(in_count > 0, "No input ports for this type");
            static_assert(in_count == 1, "bind_input is only valid for single-input types; use bind_input_at for multi-input types");
            bind_input_at<node_id, 0, T>(t);
        }

        template<std::size_t node_id, std::size_t input_index, typename T>
        constexpr void bind_input_at(T& t) {

            constexpr std::size_t node_index = [] () constexpr {
                constexpr auto ids = topology_t::ids();
                for (std::size_t i = 0; i < topology_t::size(); ++i) if (ids[i] == node_id) return i;
                return static_cast<std::size_t>(-1);
                }();
            static_assert(node_index != static_cast<std::size_t>(-1), "Invalid node id");
            using node_type = node_type_at<node_index>;
            using node_manifest = typename node_type::module_type::Manifest;
            static_assert(node_manifest::template contains<T>, "Type not declared in node Manifest");
            constexpr std::size_t in_count = node_manifest::template input_count<T>();
            static_assert(input_index < in_count, "Invalid input index for this node/type");

            // Ensure the specific input port is not already connected in the graph
            constexpr bool is_connected = (traits::template input_index_for<T, node_index, input_index>() != traits::invalid_index);
            static_assert(!is_connected, "Requested input port is already connected; cannot bind_input_at");

            auto& ctx = std::get<node_index>(mContexts);
            ctx.template set_input_ptr<T, input_index>(&t);
        }

        constexpr bool all_ios_connected() const {
            return std::apply([] (auto& ... ctxs) { return (ctxs.all_ios_connected() && ...); }, mContexts);
        }

        template<typename stream_t>
        void print(stream_t& stream, const std::string_view& inGraphName = "") const {
            ugraph::print_graph<topology_t>(stream, inGraphName);
        }

        template<typename stream_t>
        void print_pipeline(stream_t& stream, const std::string_view& inGraphName = "") const {
            ugraph::print_pipeline<topology_t>(stream, inGraphName);
        }

    private:

        template<std::size_t I, typename F>
        constexpr void for_each_at(F&& f) {
            f(*std::get<I>(mModules), std::get<I>(mContexts));
        }

        template<typename F, std::size_t... I>
        constexpr void for_each_impl(F&& f, std::index_sequence<I...>) {
            (for_each_at<I>(std::forward<F>(f)), ...);
        }

        template<std::size_t NodeIndex, std::size_t... Tidx>
        constexpr void init_node_types(graph_data_t& graph_data, std::index_sequence<Tidx...>) {
            using node_type = node_type_at<NodeIndex>;
            using node_manifest = typename node_type::module_type::Manifest;
            auto& ctx = std::get<NodeIndex>(mContexts);
            (init_type<NodeIndex, typename node_manifest::template type_at<Tidx>>(graph_data, ctx), ...);
        }

        template<std::size_t NodeIndex, typename DataT, typename CtxT>
        constexpr void init_type(graph_data_t& graph_data, CtxT& ctx) {
            using node_type = node_type_at<NodeIndex>;
            using node_manifest = typename node_type::module_type::Manifest;
            using data_t = DataT;
            constexpr std::size_t manifest_index = manifest_t::template index<data_t>();

            auto& arr = std::get<manifest_index>(graph_data);

            constexpr std::size_t in_count = node_manifest::template input_count<data_t>();
            init_inputs_impl<NodeIndex, data_t>(ctx, arr, std::make_index_sequence<in_count>{});

            constexpr std::size_t out_count = node_manifest::template output_count<data_t>();
            init_outputs_impl<NodeIndex, data_t>(ctx, arr, std::make_index_sequence<out_count>{});
        }

        template<std::size_t NodeIndex, typename DataT, typename CtxT, std::size_t... Ps>
        constexpr void init_inputs_impl(CtxT& ctx,
            std::array<DataT, traits::template coloring_t<DataT>::data_count()>& arr,
            std::index_sequence<Ps...>) {
            (ctx.template set_input_ptr<DataT, Ps>(
                (traits::template input_index_for<DataT, NodeIndex, Ps>() != traits::invalid_index)
                ? &arr[traits::template input_index_for<DataT, NodeIndex, Ps>()]
                : nullptr
            ), ...);
        }

        template<std::size_t NodeIndex, typename DataT, typename CtxT, std::size_t... Ps>
        constexpr void init_outputs_impl(CtxT& ctx,
            std::array<DataT, traits::template coloring_t<DataT>::data_count()>& arr,
            std::index_sequence<Ps...>) {
            (ctx.template set_output_ptr<DataT, Ps>(
                (traits::template output_index_for<DataT, NodeIndex, Ps>() != traits::invalid_index)
                ? &arr[traits::template output_index_for<DataT, NodeIndex, Ps>()]
                : nullptr
            ), ...);
        }

        template<std::size_t... Is>
        constexpr void init_graph_data_impl(graph_data_t& graph_data, std::index_sequence<Is...>) {
            (init_node_types<Is>(graph_data, std::make_index_sequence<node_type_at<Is>::module_type::Manifest::type_count>{}), ...);
        }

    };

    template<typename T, typename Tuple, std::size_t I = 0>
    struct tuple_index_of_type_impl {
        static_assert(I < std::tuple_size_v<Tuple> +1, "index out of bounds");
        using arr_t = std::tuple_element_t<I, Tuple>;
        using elem_t = typename arr_t::value_type;
        static constexpr std::size_t value = std::is_same_v<T, elem_t>
            ? I
            : tuple_index_of_type_impl<T, Tuple, I + 1>::value;
    };

    template<typename T, typename Tuple>
    struct tuple_index_of_type_impl<T, Tuple, std::tuple_size_v<Tuple>> {
        static constexpr std::size_t value = static_cast<std::size_t>(-1);
    };

    template<typename T, typename graph_data_t>
    static constexpr T& data_at(graph_data_t& graph_data, std::size_t i) {
        constexpr std::size_t index = tuple_index_of_type_impl<T, graph_data_t>::value;
        static_assert(index != static_cast<std::size_t>(-1), "Type not found in graph_data_t");
        auto& arr = std::get<index>(graph_data);
        return arr[i];
    }

    template<typename E0, typename... ERest>
    Graph(E0 const&, ERest const&...) -> Graph<std::decay_t<E0>, std::decay_t<ERest>...>;

} // namespace ugraph
