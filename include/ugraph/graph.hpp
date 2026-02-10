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
#include "type_traits/edge_traits.hpp"
#include "type_traits/type_list.hpp"
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

        modules_tuple_impl_t mModules;
        contexts_tuple_t mContexts;
        template<std::size_t... I>
        static constexpr auto make_data_storage_tuple_t(std::index_sequence<I...>) ->
            std::tuple<std::array<typename manifest_t::template type_at<I>,
            traits::template coloring_t<typename manifest_t::template type_at<I>>::data_count() >...>;

        using data_storage_tuple_t = decltype(make_data_storage_tuple_t(std::make_index_sequence<manifest_t::type_count>{}));
        data_storage_tuple_t mDataStorage {};

    public:

        using topology_type = topology_t;

        constexpr Graph(const edges_t&... es) :
            mModules(traits::build_modules(std::make_index_sequence<topology_t::size()>{}, es...)) {
            init_contexts(std::make_index_sequence<topology_t::size()>{});
        }

        template<typename F>
        constexpr void for_each(F&& f) {
            for_each_impl(std::forward<F>(f), std::make_index_sequence<topology_t::size()>{});
        }

        template<typename T>
        constexpr T* data() {
            static_assert(manifest_t::template contains<T>, "Type not declared in Manifest");
            if constexpr (data_count<T>() == 0) {
                return nullptr;
            }
            else {
                constexpr std::size_t idx = manifest_t::template index<T>();
                return std::get<idx>(mDataStorage).data();
            }
        }

        template<typename T>
        constexpr const T* data() const {
            static_assert(manifest_t::template contains<T>, "Type not declared in Manifest");
            if constexpr (data_count<T>() == 0) {
                return nullptr;
            }
            else {
                constexpr std::size_t idx = manifest_t::template index<T>();
                return std::get<idx>(mDataStorage).data();
            }
        }

        template<typename T>
        constexpr T& data_at(std::size_t i) {
            auto ptr = data<T>();
            return ptr[i];
        }

        template<typename T>
        constexpr const T& data_at(std::size_t i) const {
            auto ptr = data<T>();
            return ptr[i];
        }

        template<typename T>
        static constexpr std::size_t data_count() {
            return traits::template coloring_t<T>::data_count();
        }

        template<typename stream_t>
        void print(stream_t& stream, const std::string_view& inGraphName = "") const {
            ugraph::print_graph<topology_t>(stream, inGraphName);
        }

        template<typename stream_t>
        void print_pipeline(stream_t& stream, const std::string_view& inGraphName = "") const {
            ugraph::print_pipeline<topology_t>(stream, inGraphName);
        }

        constexpr bool all_ios_connected() const {
            return all_ios_connected_impl(std::make_index_sequence<topology_t::size()>{});
        }

        template<typename node_port_t, typename T>
        constexpr void bind(const node_port_t&, T& data) {
            constexpr std::size_t vid = node_port_t::node_type::id();
            constexpr std::size_t node_index = node_index_for_id<vid>();
            static_assert(node_index != traits::invalid_index, "Node not found in graph");

            using node_type = node_type_at<node_index>;
            using node_manifest = typename node_type::module_type::Manifest;
            static_assert(node_manifest::template contains<T>, "Type not declared in Node Manifest");

            constexpr std::size_t port_index = node_port_t::index();
            auto& ctx = std::get<node_index>(mContexts);

            if constexpr (is_output_port<node_port_t>::value) {
                static_assert(std::is_same_v<T, typename node_port_t::data_type>, "Output port type mismatch");
                static_assert(
                    !traits::template has_output_edge<T, vid, port_index>(),
                    "Output port already connected in graph"
                    );
                ctx.template set_output_ptr<T, port_index>(&data);
            }
            else {
                static_assert(
                    !traits::template has_input_edge<T, vid, port_index>(),
                    "Input port already connected in graph"
                    );
                ctx.template set_input_ptr<T, port_index>(&data);
            }
        }

    private:

        template<typename P, typename = void>
        struct is_output_port : std::false_type {};

        template<typename P>
        struct is_output_port<P, std::void_t<typename P::data_type>> : std::true_type {};

        template<std::size_t VID, std::size_t I = 0>
        static constexpr std::size_t node_index_for_id() {
            if constexpr (I >= topology_t::size()) {
                return traits::invalid_index;
            }
            else if constexpr (topology_t::template id_at<I>() == VID) {
                return I;
            }
            else {
                return node_index_for_id<VID, I + 1>();
            }
        }

        template<typename T, std::size_t InN, std::size_t OutN, std::size_t... I, std::size_t... O>
        constexpr std::array<T*, InN + OutN>
            concat_ptr_arrays(const std::array<T*, InN>& in,
                const std::array<T*, OutN>& out,
                std::index_sequence<I...>,
                std::index_sequence<O...>) {
            return { in[I]..., out[O]... };
        }

        template<typename NodeManifest, typename T, std::size_t NodeIndex, std::size_t... P>
        constexpr std::array<T*, NodeManifest::template input_count<T>()>
            build_input_ptrs_array_impl(std::index_sequence<P...>) {
            if constexpr (NodeManifest::template strict_connection<T>()) {
                constexpr std::size_t vid = topology_t::template id_at<NodeIndex>();
                static_assert((traits::template has_input_edge<T, vid, P>() && ...),
                    "Strict input connection missing in graph");
            }
            if constexpr (data_count<T>() == 0) {
                return { ((void) P, nullptr)... };
            }
            else {
                auto* base = data<T>();
                return { ((traits::template input_index_for<T, NodeIndex, P>() == traits::invalid_index)
                    ? nullptr
                    : &base[traits::template input_index_for<T, NodeIndex, P>()])... };
            }
        }

        template<typename NodeManifest, typename T, std::size_t NodeIndex>
        constexpr std::array<T*, NodeManifest::template input_count<T>()>
            build_input_ptrs_array() {
            return build_input_ptrs_array_impl<NodeManifest, T, NodeIndex>(
                std::make_index_sequence<NodeManifest::template input_count<T>()>{});
        }

        template<typename NodeManifest, typename T, std::size_t NodeIndex, std::size_t... P>
        constexpr std::array<T*, NodeManifest::template output_count<T>()>
            build_output_ptrs_array_impl(std::index_sequence<P...>) {
            if constexpr (NodeManifest::template strict_connection<T>()) {
                constexpr std::size_t vid = topology_t::template id_at<NodeIndex>();
                static_assert((traits::template has_output_edge<T, vid, P>() && ...),
                    "Strict output connection missing in graph");
            }
            if constexpr (data_count<T>() == 0) {
                return { ((void) P, nullptr)... };
            }
            else {
                auto* base = data<T>();
                return { ((traits::template output_index_for<T, NodeIndex, P>() == traits::invalid_index)
                    ? nullptr
                    : &base[traits::template output_index_for<T, NodeIndex, P>()])... };
            }
        }

        template<typename NodeManifest, typename T, std::size_t NodeIndex>
        constexpr std::array<T*, NodeManifest::template output_count<T>()>
            build_output_ptrs_array() {
            return build_output_ptrs_array_impl<NodeManifest, T, NodeIndex>(
                std::make_index_sequence<NodeManifest::template output_count<T>()>{});
        }

        template<typename NodeManifest, typename T, std::size_t NodeIndex>
        constexpr typename Context<NodeManifest>::template data_array_t<T>
            build_io_ptrs_array() {
            auto input_ptrs = build_input_ptrs_array<NodeManifest, T, NodeIndex>();
            auto output_ptrs = build_output_ptrs_array<NodeManifest, T, NodeIndex>();
            return concat_ptr_arrays<T,
                NodeManifest::template input_count<T>(),
                NodeManifest::template output_count<T>()>(
                    input_ptrs,
                    output_ptrs,
                    std::make_index_sequence<NodeManifest::template input_count<T>()>{},
                    std::make_index_sequence<NodeManifest::template output_count<T>()>{});
        }

        template<typename NodeManifest, std::size_t NodeIndex, std::size_t... Tidx>
        constexpr void init_context_io(Context<NodeManifest>& ctx, std::index_sequence<Tidx...>) {
            (ctx.template set_ios<typename NodeManifest::template type_at<Tidx>>(
                build_io_ptrs_array<NodeManifest, typename NodeManifest::template type_at<Tidx>, NodeIndex>()), ...);
        }

        template<std::size_t I, typename F>
        constexpr void for_each_at(F&& f) {
            f(*std::get<I>(mModules), std::get<I>(mContexts));
        }

        template<typename F, std::size_t... I>
        constexpr void for_each_impl(F&& f, std::index_sequence<I...>) {
            (for_each_at<I>(std::forward<F>(f)), ...);
        }

        template<std::size_t I>
        constexpr void init_context_at() {
            using node_type = node_type_at<I>;
            using node_manifest = typename node_type::module_type::Manifest;
            auto& ctx = std::get<I>(mContexts);
            init_context_io<node_manifest, I>(ctx, std::make_index_sequence<node_manifest::type_count>{});
        }

        template<std::size_t... I>
        constexpr void init_contexts(std::index_sequence<I...>) {
            (init_context_at<I>(), ...);
        }

        template<typename NodeManifest, typename T, std::size_t... P>
        constexpr bool all_inputs_connected(const Context<NodeManifest>& ctx,
            std::index_sequence<P...>) const {
            if constexpr (NodeManifest::template input_count<T>() == 0) {
                return true;
            }
            else {
                return (ctx.template has_input<T, P>() && ...);
            }
        }

        template<typename NodeManifest, typename T, std::size_t... P>
        constexpr bool all_outputs_connected(const Context<NodeManifest>& ctx,
            std::index_sequence<P...>) const {
            if constexpr (NodeManifest::template output_count<T>() == 0) {
                return true;
            }
            else {
                return (ctx.template has_output<T, P>() && ...);
            }
        }

        template<typename NodeManifest, typename T>
        constexpr bool all_ios_connected_for_type(const Context<NodeManifest>& ctx) const {
            const bool inputs_ok = all_inputs_connected<NodeManifest, T>(ctx,
                std::make_index_sequence<NodeManifest::template input_count<T>()>{});
            const bool outputs_ok = all_outputs_connected<NodeManifest, T>(ctx,
                std::make_index_sequence<NodeManifest::template output_count<T>()>{});
            return inputs_ok && outputs_ok;
        }

        template<typename NodeManifest, std::size_t... Tidx>
        constexpr bool all_ios_connected_for_manifest(const Context<NodeManifest>& ctx,
            std::index_sequence<Tidx...>) const {
            if constexpr (NodeManifest::type_count == 0) {
                return true;
            }
            else {
                return (all_ios_connected_for_type<NodeManifest, typename NodeManifest::template type_at<Tidx>>(ctx) && ...);
            }
        }

        template<std::size_t I>
        constexpr bool all_ios_connected_at() const {
            using node_type = node_type_at<I>;
            using node_manifest = typename node_type::module_type::Manifest;
            const auto& ctx = std::get<I>(mContexts);
            return all_ios_connected_for_manifest<node_manifest>(ctx,
                std::make_index_sequence<node_manifest::type_count>{});
        }

        template<std::size_t... I>
        constexpr bool all_ios_connected_impl(std::index_sequence<I...>) const {
            if constexpr (topology_t::size() == 0) {
                return true;
            }
            else {
                return (all_ios_connected_at<I>() && ...);
            }
        }

    };

    template<typename E0, typename... ERest>
    Graph(E0 const&, ERest const&...) -> Graph<std::decay_t<E0>, std::decay_t<ERest>...>;

} // namespace ugraph
