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

#include "manifest.hpp"
#include "node.hpp"

namespace ugraph {

    /// Module type representing an external input to a graph.
    /// Data flows from outside the graph through this node into inner nodes.
    /// The module has output ports only (it produces data for the inner graph).
    template<typename spec_t>
    struct GraphInput {
        using Manifest = ugraph::Manifest<ugraph::IO<spec_t, 0, 1>>;
        template<typename T> void process(T&) {}
    };

    /// Module type representing an external output from a graph.
    /// Data flows from inner nodes through this node to outside the graph.
    /// The module has input ports only (it consumes data from the inner graph).
    template<typename spec_t>
    struct GraphOutput {
        using Manifest = ugraph::Manifest<ugraph::IO<spec_t, 1, 0>>;
        template<typename T> void process(T&) {}
    };

    /// Tagged variant of GraphInput — uses TaggedIO so the tag propagates
    /// into the graph's IO manifest.
    template<typename tag_t, typename data_t>
    struct GraphInputTag {
        using Manifest = ugraph::Manifest<ugraph::TaggedIO<tag_t, data_t, 0, 1>>;
        template<typename T> void process(T&) {}
    };

    /// Tagged variant of GraphOutput — uses TaggedIO so the tag propagates
    /// into the graph's IO manifest.
    template<typename tag_t, typename data_t>
    struct GraphOutputTag {
        using Manifest = ugraph::Manifest<ugraph::TaggedIO<tag_t, data_t, 1, 0>>;
        template<typename T> void process(T&) {}
    };

    namespace detail {

        /// Helper: extracts the Manifest spec type from a GraphIO module.
        /// For plain spec: to_manifest_spec<GraphInput<float>> = IO<float,0,1>
        /// For tagged:     to_manifest_spec<GraphInputTag<MyTag,float>> = TaggedIO<MyTag,float,0,1>
        template<typename T>
        struct to_manifest_spec;

        template<typename spec_t>
        struct to_manifest_spec<GraphInput<spec_t>> { using type = ugraph::IO<spec_t, 0, 1>; };

        template<typename spec_t>
        struct to_manifest_spec<GraphOutput<spec_t>> { using type = ugraph::IO<spec_t, 1, 0>; };

        template<typename tag_t, typename data_t>
        struct to_manifest_spec<GraphInputTag<tag_t, data_t>> { using type = ugraph::TaggedIO<tag_t, data_t, 0, 1>; };

        template<typename tag_t, typename data_t>
        struct to_manifest_spec<GraphOutputTag<tag_t, data_t>> { using type = ugraph::TaggedIO<tag_t, data_t, 1, 0>; };


        template<typename T>
        struct is_graph_input : std::false_type {};

        template<typename spec_t>
        struct is_graph_input<GraphInput<spec_t>> : std::true_type {};

        template<typename tag_t, typename data_t>
        struct is_graph_input<GraphInputTag<tag_t, data_t>> : std::true_type {};

        template<typename T>
        struct is_graph_output : std::false_type {};

        template<typename spec_t>
        struct is_graph_output<GraphOutput<spec_t>> : std::true_type {};

        template<typename tag_t, typename data_t>
        struct is_graph_output<GraphOutputTag<tag_t, data_t>> : std::true_type {};

        template<typename T>
        using to_manifest_spec_t = typename to_manifest_spec<T>::type;

    } // namespace detail

    /// Create a graph input node with the given ID and spec type.
    /// Usage: auto myInput = graph_input<id, MySpec>();
    template<std::size_t Id, typename spec_t>
    constexpr auto graph_input() {
        GraphInput<spec_t> module;
        return make_node<Id>(module);
    }

    /// Create a graph output node with the given ID and spec type.
    /// Usage: auto myOutput = graph_output<id, MySpec>();
    template<std::size_t Id, typename spec_t>
    constexpr auto graph_output() {
        GraphOutput<spec_t> module;
        return make_node<Id>(module);
    }

    namespace graph_io {
        /// Builder for a graph input node with deferred spec type.
        /// The spec type is declared when calling .output<SpecType>().
        /// Usage: auto in = ugraph::graph_io::make_input<__COUNTER__>();
        ///        in.output<MySpec>() >> someNode.input<MySpec>();
        template<std::size_t Id>
        struct GraphInputBuilder {
            template<typename spec_t>
            constexpr auto output() {
                return graph_input<Id, spec_t>().template output<spec_t>();
            }
        };

        /// Builder for a graph output node with deferred spec type.
        /// The spec type is declared when calling .input<SpecType>().
        /// Usage: auto out = ugraph::graph_io::make_output<__COUNTER__>();
        ///        someNode.output<MySpec>() >> out.input<MySpec>();
        template<std::size_t Id>
        struct GraphOutputBuilder {
            template<typename spec_t>
            constexpr auto input() {
                return graph_output<Id, spec_t>().template input<spec_t>();
            }
        };

        /// Create a graph input builder with the given ID.
        /// The spec type is declared later when calling .output<SpecType>().
        template<std::size_t Id>
        constexpr auto make_input() { return GraphInputBuilder<Id>{}; }

        /// Create a graph output builder with the given ID.
        /// The spec type is declared later when calling .input<SpecType>().
        template<std::size_t Id>
        constexpr auto make_output() { return GraphOutputBuilder<Id>{}; }

    } // namespace graph_io

    namespace detail {
        /// Helper: maps a node type to a type_list of its IO spec if it's a graph input, else empty list.
        template<typename node_t, bool = is_graph_input<typename node_t::module_type>::value>
        struct input_spec_list { using type = type_list<>; };

        template<typename node_t>
        struct input_spec_list<node_t, true> { using type = type_list<to_manifest_spec_t<typename node_t::module_type>>; };

        /// Helper: maps a node type to a type_list of its IO spec if it's a graph output, else empty list.
        template<typename node_t, bool = is_graph_output<typename node_t::module_type>::value>
        struct output_spec_list { using type = type_list<>; };

        template<typename node_t>
        struct output_spec_list<node_t, true> { using type = type_list<to_manifest_spec_t<typename node_t::module_type>>; };
    } // namespace detail

    /// Extract the keys (IO spec types) for graph IO nodes, separated into inputs and outputs.
    /// Accepts a type_list of node types (from topology's vertex_types_list).
    /// For GraphInput modules: data flows from outside into the graph → external input.
    /// For GraphOutput modules: data flows from inside the graph to outside → external output.
    template<typename node_types_t>
    struct graph_io_keys;

    template<typename... node_types_t>
    struct graph_io_keys<detail::type_list<node_types_t...>> {
        using external_inputs = typename detail::type_list_cat<
            typename detail::input_spec_list<node_types_t>::type...
        >::type;
        using external_outputs = typename detail::type_list_cat<
            typename detail::output_spec_list<node_types_t>::type...
        >::type;
    };

    template<typename node_types_t>
    using external_inputs_t = typename graph_io_keys<node_types_t>::external_inputs;

    template<typename node_types_t>
    using external_outputs_t = typename graph_io_keys<node_types_t>::external_outputs;

} // namespace ugraph
