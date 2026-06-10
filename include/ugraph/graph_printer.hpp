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

#include <array>
#include <cstddef>
#include <string_view>
#include <type_traits>

#include "manifest.hpp"
#include "node.hpp"
#include "type_traits/edge_traits.hpp"
#include "type_traits/type_list.hpp"

namespace ugraph {

    template<typename... edges_t>
    class Topology;

    template<typename T>
    constexpr std::string_view type_name() {
        std::string_view s;
#if defined(__clang__) || defined(__GNUC__)
        {
            constexpr std::string_view p = __PRETTY_FUNCTION__;
            constexpr std::string_view key = "T = ";
            const auto start = p.find(key);
            if (start == p.npos) {
                s = p;
            }
            else {
                s = p.substr(start + key.size());
                const auto end = s.find_first_of("];,");
                if (end != s.npos) {
                    s = s.substr(0, end);
                }
            }
        }
#elif defined(_MSC_VER)
        {
            constexpr std::string_view p = __FUNCSIG__;
            constexpr std::string_view key = "type_name<";
            const auto start = p.rfind(key);
            if (start == p.npos) {
                s = p;
            }
            else {
                const auto valueStart = start + key.size();
                const auto valueEnd = p.find(">(void)", valueStart);
                s = (valueEnd == p.npos) ? p.substr(valueStart) : p.substr(valueStart, valueEnd - valueStart);
            }
        }
#else
        s = "unknown";
#endif

        constexpr std::string_view prefixes[] = { "const ", "volatile ", "struct ", "class ", "enum " };
        bool changed = true;
        while (changed) {
            changed = false;
            for (auto prefix : prefixes) {
                if (s.rfind(prefix, 0) == 0) {
                    s = s.substr(prefix.size());
                    changed = true;
                    break;
                }
            }
        }

        while (!s.empty() && s.front() == ' ') {
            s = s.substr(1);
        }

        const auto pos = s.rfind("::");
        return (pos == s.npos) ? s : s.substr(pos + 2);
    }

    namespace {

        template<typename T, typename = void>
        struct user_type { using type = T; };

        template<typename T>
        struct user_type<T, std::void_t<typename T::module_type>> { using type = typename T::module_type; };

        template<typename T>
        constexpr std::string_view node_name() {
            using display_t = typename user_type<T>::type;
            return type_name<display_t>();
        }

        template<typename vertex_t, typename stream_t>
        void print_node_label(stream_t& stream, bool inShowVertexIds) {
            stream << vertex_t::id() << "(\"" << node_name<vertex_t>();
            if (inShowVertexIds) {
                stream << " " << vertex_t::id();
            }
            stream << "\")\n";
        }

        template<typename T, typename = void>
        struct printer_edge_types {
            using type = typename std::decay_t<T>::edge_types_list;
        };

        template<typename... edges_t>
        struct printer_edge_types<Topology<edges_t...>, void> {
            using type = detail::type_list<edges_t...>;
        };

        template<typename T, typename = void>
        struct printer_all_edge_types {
            using type = typename printer_edge_types<std::decay_t<T>>::type;
        };

        template<typename T>
        struct printer_all_edge_types<T, std::void_t<typename std::decay_t<T>::all_edge_types_list>> {
            using type = typename std::decay_t<T>::all_edge_types_list;
        };

        template<typename T, typename = void>
        struct printer_topology {
            using type = T;
        };

        template<typename T>
        struct printer_topology<T, std::void_t<typename T::topology_type>> {
            using type = typename T::topology_type;
        };

        template<typename edge_t, typename = void>
        struct printer_edge_label {
            static constexpr bool available = false;
            template<typename stream_t>
            static void print(stream_t&) {}
        };

        template<typename edge_t>
        struct printer_edge_label<edge_t, std::void_t<
            typename detail::edge_traits<edge_t>::src_port_t::spec_type,
            typename detail::edge_traits<edge_t>::dst_port_t::spec_type
            >> {
            using src_spec_t = typename detail::edge_traits<edge_t>::src_port_t::spec_type;
            using dst_spec_t = typename detail::edge_traits<edge_t>::dst_port_t::spec_type;
            using src_traits_t = detail::io_traits<src_spec_t>;
            using dst_traits_t = detail::io_traits<dst_spec_t>;

            static constexpr bool available = true;

            template<typename stream_t>
            static void print(stream_t& stream) {
                if constexpr (src_traits_t::is_tagged && dst_traits_t::is_tagged) {
                    stream << type_name<typename src_traits_t::tag>();
                    if constexpr (!std::is_same_v<typename src_traits_t::tag, typename dst_traits_t::tag>) {
                        stream << " to " << type_name<typename dst_traits_t::tag>();
                    }
                    stream << ": " << type_name<typename src_traits_t::type>();
                }
                else if constexpr (src_traits_t::is_tagged) {
                    stream << type_name<typename src_traits_t::tag>() << ": " << type_name<typename src_traits_t::type>();
                }
                else if constexpr (dst_traits_t::is_tagged) {
                    stream << type_name<typename dst_traits_t::tag>() << ": " << type_name<typename src_traits_t::type>();
                }
                else {
                    stream << type_name<typename src_traits_t::type>();
                }
            }
        };

        template<typename edge_t, typename = void>
        struct printer_binding_label {
            template<typename stream_t>
            static void print(stream_t&) {}
        };

        template<typename edge_t>
        struct printer_binding_label<edge_t, std::void_t<typename binding_traits<edge_t>::data_type>> {
            using data_t = typename binding_traits<edge_t>::data_type;

            template<typename port_t, typename = void>
            struct label_type { using type = data_t; };

            template<typename port_t>
            struct label_type<port_t, std::void_t<typename port_t::spec_type>> {
                using spec_t = typename port_t::spec_type;
                using type = typename detail::io_traits<spec_t>::type;
                using tag = typename detail::io_traits<spec_t>::tag;
                static constexpr bool is_tagged = detail::io_traits<spec_t>::is_tagged;
            };

            using label_t = typename label_type<typename binding_traits<edge_t>::port_type>::type;
            using port_label_t = label_type<typename binding_traits<edge_t>::port_type>;

            template<typename stream_t>
            static void print(stream_t& stream) {
                if constexpr (port_label_t::is_tagged) {
                    stream << type_name<typename port_label_t::tag>() << ": ";
                }
                stream << type_name<label_t>();
            }
        };

        template<typename stream_t>
        void print_header(stream_t& stream, const std::string_view& inGraphName) {
            stream << "```mermaid\n";
            stream << "flowchart LR\n";
            if (!inGraphName.empty()) {
                stream << "subgraph " << inGraphName << "\n";
            }
        }

        template<typename stream_t>
        void print_footer(stream_t& stream, const std::string_view& inGraphName) {
            if (!inGraphName.empty()) {
                stream << "end\n";
            }
            stream << "```\n";
        }

        template<typename graph_t, typename stream_t>
        void print_node_names(stream_t& stream, bool inShowVertexIds) {
            using topo_t = typename printer_topology<std::decay_t<graph_t>>::type;
            topo_t::for_each(
                [&] (auto vertex) {
                    using vertex_t = decltype(vertex);
                    print_node_label<vertex_t>(stream, inShowVertexIds);
                }
            );
        }

        template<typename graph_t, typename stream_t, std::size_t... I>
        void print_graph_edges_impl(stream_t& stream, bool inShowLinkTypes, std::index_sequence<I...>) {
            using topo_t = typename printer_topology<std::decay_t<graph_t>>::type;
            using edge_list_t = typename printer_edge_types<std::decay_t<graph_t>>::type;
            constexpr auto edges = topo_t::edges();

            (([&] {
                using edge_t = typename detail::type_list_at<I, edge_list_t>::type;
                const auto& edge = edges[I];
                stream << edge.first << " -->";
                if constexpr (printer_edge_label<edge_t>::available) {
                    if (inShowLinkTypes) {
                        stream << "|";
                        printer_edge_label<edge_t>::print(stream);
                        stream << "|";
                    }
                }
                stream << " " << edge.second << "\n";
                }()), ...);
        }

        template<typename graph_t, typename stream_t>
        void print_graph_edges(stream_t& stream, bool inShowLinkTypes) {
            using edge_list_t = typename printer_edge_types<std::decay_t<graph_t>>::type;
            constexpr std::size_t edge_count = detail::type_list_size<edge_list_t>::value;
            print_graph_edges_impl<graph_t>(stream, inShowLinkTypes, std::make_index_sequence<edge_count>{});
        }

        template<typename graph_t, std::size_t... I>
        auto make_binding_ptrs_impl(const graph_t& graph, std::index_sequence<I...>) {
            using edge_list_t = typename printer_all_edge_types<std::decay_t<graph_t>>::type;
            return std::array<const void*, sizeof...(I)> {
                ([] (const graph_t& currentGraph) -> const void* {
                    using edge_t = typename detail::type_list_at<I, edge_list_t>::type;
                    if constexpr (detail::is_data_binding<edge_t>::value) {
                        return currentGraph.template binding_ptr<edge_t>();
                    }
                    return nullptr;
                    }(graph))...
            };
        }

        template<typename graph_t>
        auto make_binding_ptrs(const graph_t& graph) {
            using edge_list_t = typename printer_all_edge_types<std::decay_t<graph_t>>::type;
            constexpr std::size_t edge_count = detail::type_list_size<edge_list_t>::value;
            return make_binding_ptrs_impl(graph, std::make_index_sequence<edge_count>{});
        }

        template<typename binding_ptrs_t>
        std::size_t binding_node_index(const binding_ptrs_t& bindingPtrs, std::size_t bindingEdgeIndex) {
            const void* target = bindingPtrs[bindingEdgeIndex];
            std::size_t uniqueIndex = 0;

            for (std::size_t i = 0; i < bindingEdgeIndex; ++i) {
                const void* current = bindingPtrs[i];
                if (current == nullptr) {
                    continue;
                }

                bool seenEarlier = false;
                for (std::size_t j = 0; j < i; ++j) {
                    if (bindingPtrs[j] == current) {
                        seenEarlier = true;
                        break;
                    }
                }

                if (!seenEarlier) {
                    if (current == target) {
                        return uniqueIndex;
                    }
                    ++uniqueIndex;
                }
            }

            return uniqueIndex;
        }

        template<typename binding_ptrs_t>
        bool is_first_binding_occurrence(const binding_ptrs_t& bindingPtrs, std::size_t bindingEdgeIndex) {
            const void* target = bindingPtrs[bindingEdgeIndex];
            if (target == nullptr) {
                return false;
            }

            for (std::size_t i = 0; i < bindingEdgeIndex; ++i) {
                if (bindingPtrs[i] == target) {
                    return false;
                }
            }

            return true;
        }

        template<typename stream_t, typename binding_ptrs_t>
        void print_binding_node_id(stream_t& stream, const binding_ptrs_t& bindingPtrs, std::size_t bindingEdgeIndex) {
            stream << "data_" << binding_node_index(bindingPtrs, bindingEdgeIndex);
        }

        template<typename graph_t, typename stream_t, typename binding_ptrs_t, std::size_t... I>
        void print_binding_nodes_impl(stream_t& stream, const binding_ptrs_t& bindingPtrs, std::index_sequence<I...>) {
            using edge_list_t = typename printer_all_edge_types<std::decay_t<graph_t>>::type;

            (([&] {
                using edge_t = typename detail::type_list_at<I, edge_list_t>::type;
                if constexpr (detail::is_data_binding<edge_t>::value) {
                    if (is_first_binding_occurrence(bindingPtrs, I)) {
                        print_binding_node_id(stream, bindingPtrs, I);
                        stream << "(( ))\n";
                    }
                }
                }()), ...);
        }

        template<typename graph_t, typename stream_t, typename binding_ptrs_t>
        void print_binding_nodes(stream_t& stream, const binding_ptrs_t& bindingPtrs) {
            using edge_list_t = typename printer_all_edge_types<std::decay_t<graph_t>>::type;
            constexpr std::size_t edge_count = detail::type_list_size<edge_list_t>::value;
            print_binding_nodes_impl<graph_t>(stream, bindingPtrs, std::make_index_sequence<edge_count>{});
        }

        template<typename edge_t, typename stream_t, typename binding_ptrs_t>
        void print_binding_edge(stream_t& stream, const binding_ptrs_t& bindingPtrs, std::size_t bindingEdgeIndex, bool inShowLinkTypes) {
            using port_t = typename binding_traits<edge_t>::port_type;
            constexpr bool is_output_binding = detail::is_output_port<port_t>::value;
            constexpr std::size_t node_id = port_t::node_type::id();

            if constexpr (is_output_binding) {
                stream << node_id << " -->";
                if (inShowLinkTypes) {
                    stream << "|";
                    printer_binding_label<edge_t>::print(stream);
                    stream << "|";
                }
                stream << " ";
                print_binding_node_id(stream, bindingPtrs, bindingEdgeIndex);
                stream << "\n";
            }
            else {
                print_binding_node_id(stream, bindingPtrs, bindingEdgeIndex);
                stream << " -->";
                if (inShowLinkTypes) {
                    stream << "|";
                    printer_binding_label<edge_t>::print(stream);
                    stream << "|";
                }
                stream << " " << node_id << "\n";
            }
        }

        template<typename graph_t, typename stream_t, typename binding_ptrs_t, std::size_t... I>
        void print_binding_edges_impl(stream_t& stream, const binding_ptrs_t& bindingPtrs, bool inShowLinkTypes, std::index_sequence<I...>) {
            using edge_list_t = typename printer_all_edge_types<std::decay_t<graph_t>>::type;

            (([&] {
                using edge_t = typename detail::type_list_at<I, edge_list_t>::type;
                if constexpr (detail::is_data_binding<edge_t>::value) {
                    print_binding_edge<edge_t>(stream, bindingPtrs, I, inShowLinkTypes);
                }
                }()), ...);
        }

        template<typename graph_t, typename stream_t, typename binding_ptrs_t>
        void print_binding_edges(stream_t& stream, const binding_ptrs_t& bindingPtrs, bool inShowLinkTypes) {
            using edge_list_t = typename printer_all_edge_types<std::decay_t<graph_t>>::type;
            constexpr std::size_t edge_count = detail::type_list_size<edge_list_t>::value;
            print_binding_edges_impl<graph_t>(stream, bindingPtrs, inShowLinkTypes, std::make_index_sequence<edge_count>{});
        }

        template<typename topo_t, typename stream_t>
        void print_isolated_vertices(stream_t& stream) {
            constexpr auto edges = topo_t::edges();
            constexpr auto ids = topo_t::ids();

            for (auto vertexId : ids) {
                bool found = false;
                for (const auto& edge : edges) {
                    if (edge.first == vertexId || edge.second == vertexId) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    stream << vertexId << "\n";
                }
            }
        }
    }

    template<typename graph_t, typename stream_t>
    void print_graph(stream_t& stream, const std::string_view& inGraphName = "", bool inShowLinkTypes = true, bool inShowVertexIds = false) {
        using topo_t = typename printer_topology<std::decay_t<graph_t>>::type;

        print_header(stream, inGraphName);
        print_node_names<graph_t>(stream, inShowVertexIds);
        print_graph_edges<graph_t>(stream, inShowLinkTypes);
        print_isolated_vertices<topo_t>(stream);
        print_footer(stream, inGraphName);
    }

    template<typename graph_t, typename stream_t>
    void print_graph(const graph_t& graph, stream_t& stream, const std::string_view& inGraphName = "", bool inShowLinkTypes = true, bool inShowVertexIds = false) {
        using topo_t = typename printer_topology<std::decay_t<graph_t>>::type;
        const auto bindingPtrs = make_binding_ptrs(graph);

        print_header(stream, inGraphName);
        print_node_names<graph_t>(stream, inShowVertexIds);
        print_binding_nodes<graph_t>(stream, bindingPtrs);
        print_graph_edges<graph_t>(stream, inShowLinkTypes);
        print_binding_edges<graph_t>(stream, bindingPtrs, inShowLinkTypes);
        print_isolated_vertices<topo_t>(stream);
        print_footer(stream, inGraphName);
    }

    template<typename graph_t, typename stream_t>
    void print_pipeline(stream_t& stream, const std::string_view& inGraphName = "", bool inShowVertexIds = true) {
        using topo_t = typename printer_topology<std::decay_t<graph_t>>::type;
        constexpr auto ids = topo_t::ids();

        print_header(stream, inGraphName);
        print_node_names<graph_t>(stream, inShowVertexIds);

        for (std::size_t i = 0; i < ids.size(); ++i) {
            stream << ids[i];
            if (i + 1 < ids.size()) {
                stream << " --> ";
            }
        }
        stream << "\n";

        print_footer(stream, inGraphName);
    }

} // namespace ugraph