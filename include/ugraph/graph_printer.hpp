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
#include "type_traits/edge_traits.hpp"
#include "type_traits/type_list.hpp"

namespace ugraph {

    template<typename... edges_t>
    class Topology;

    template<typename graph_t, typename stream_t>
    void print_graph(stream_t& stream, const std::string_view& inGraphName = "", bool inShowLinkTypes = true, bool inShowVertexIds = false);

    template<typename graph_t, typename stream_t>
    void print_pipeline(stream_t& stream, const std::string_view& inGraphName = "", bool inShowVertexIds = true);



    namespace {

        // Allow showing an alternative display type (strip wrappers like NodeTag or node types)
        template<typename T, typename = void>
        struct user_type { using type = T; };

        // If a type exposes `module_type`, prefer that as the display type (covers NodeTag and DataNode::NodeType)
        template<typename T>
        struct user_type<T, std::void_t<typename T::module_type>> { using type = typename T::module_type; };

        // Raw printer that returns the compiler-generated type name for T,
        // then trims common prefixes and namespaces to a bare name.
        template<typename T>
        constexpr std::string_view type_name() {
            std::string_view s;
#if defined(__clang__)
            {
                constexpr std::string_view p = __PRETTY_FUNCTION__;
                // Find the bracketed template-args section immediately after the function name
                constexpr std::string_view fn = "type_name()";
                const auto fnpos = p.find(fn);
                if (fnpos == p.npos) return p;
                const auto br_open = p.find('[', fnpos + fn.size());
                if (br_open == p.npos) return p;
                const auto br_close = p.find(']', br_open);
                const auto section = (br_close == p.npos) ? p.substr(br_open + 1) : p.substr(br_open + 1, br_close - br_open - 1);
                // look for "with T = " or "T = " inside that bracket only
                constexpr std::string_view keys[] = { "with T = ", "T = " };
                s = section;
                for (auto k : keys) {
                    const auto start = section.find(k);
                    if (start == section.npos) continue;
                    const auto spos = start + k.size();
                    s = (br_close == p.npos) ? section.substr(spos) : section.substr(spos);
                    // if there are additional characters after the type, trim up to any ',' or ';' (unlikely)
                    const auto endpos1 = s.find(';');
                    const auto endpos2 = s.find(',');
                    std::size_t endpos = s.npos;
                    if (endpos1 != s.npos) endpos = endpos1;
                    if (endpos2 != s.npos && (endpos2 < endpos || endpos == s.npos)) endpos = endpos2;
                    if (endpos != s.npos) s = s.substr(0, endpos);
                    break;
                }
            }
#elif defined(__GNUC__)
            {
                constexpr std::string_view fn = "type_name()";
                constexpr std::string_view p = __PRETTY_FUNCTION__;
                const auto fnpos = p.find(fn);
                if (fnpos == p.npos) return p;
                const auto br_open = p.find('[', fnpos + fn.size());
                if (br_open == p.npos) return p;
                const auto br_close = p.find(']', br_open);
                const auto section = (br_close == p.npos) ? p.substr(br_open + 1) : p.substr(br_open + 1, br_close - br_open - 1);
                constexpr std::string_view keys2[] = { "with T = ", "T = " };
                s = section;
                for (auto k : keys2) {
                    const auto start = section.find(k);
                    if (start == section.npos) continue;
                    const auto spos = start + k.size();
                    s = section.substr(spos);
                    const auto endpos1 = s.find(';');
                    const auto endpos2 = s.find(',');
                    std::size_t endpos = s.npos;
                    if (endpos1 != s.npos) endpos = endpos1;
                    if (endpos2 != s.npos && (endpos2 < endpos || endpos == s.npos)) endpos = endpos2;
                    if (endpos != s.npos) s = s.substr(0, endpos);
                    break;
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
                    const auto st = start + key.size();
                    // Find the matching closing '>' for the template args, handling nested '<...>'
                    std::size_t endpos = p.npos;
                    int nested = 0;
                    for (std::size_t i = st; i < p.size(); ++i) {
                        const char c = p[i];
                        if (c == '<') {
                            ++nested;
                        }
                        else if (c == '>') {
                            if (nested == 0) { endpos = i; break; }
                            --nested;
                        }
                    }
                    s = (endpos == p.npos) ? p.substr(st) : p.substr(st, endpos - st);
                }
            }
#else
            s = "unknown";
#endif
            // Strip leading cv-qualifiers like "const "/"volatile "
            constexpr std::string_view skip_prefixes[] = { "const ", "volatile " };
            constexpr std::string_view keys[] = { "struct ", "class ", "enum " };
            bool changed = true;
            while (changed) {
                changed = false;
                for (auto pfx : skip_prefixes) {
                    if (s.rfind(pfx, 0) == 0) { s = s.substr(pfx.size()); changed = true; break; }
                }
                if (changed) continue;
                for (auto k : keys) {
                    if (s.rfind(k, 0) == 0) { s = s.substr(k.size()); changed = true; break; }
                }
            }
            // Trim leading spaces
            while (!s.empty() && s.front() == ' ') s = s.substr(1);
            // Keep only the last qualifier after '::'
            const auto pos = s.rfind("::");
            if (pos != s.npos) return s.substr(pos + 2);
            return s;
        }

        // Main entry: print the (possibly unwrapped) display type for T
        template<typename T>
        constexpr std::string_view node_name() {
            using dt = typename user_type<T>::type;
            return type_name<dt>();
        }

        template<typename T, typename = void>
        struct printer_edge_types {
            using type = typename std::decay_t<T>::edge_types_list;
        };

        template<typename... edges_t>
        struct printer_edge_types<Topology<edges_t...>, void> {
            using type = detail::type_list<edges_t...>;
        };

        template<typename edge_t, typename = void>
        struct printer_edge_label {
            static constexpr bool available = false;
            static constexpr std::string_view value() {
                return {};
            }
        };

        template<typename edge_t>
        struct printer_edge_label<edge_t, std::void_t<typename detail::edge_traits<edge_t>::src_port_t::spec_type>> {
            using spec_t = typename detail::edge_traits<edge_t>::src_port_t::spec_type;
            using data_t = typename detail::io_traits<spec_t>::type;

            static constexpr bool available = true;
            static constexpr std::string_view value() {
                return type_name<data_t>();
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

        // If a type provides a topology_type, prefer it; otherwise assume the type itself is a topology-like type.
        template<typename T, typename = void>
        struct printer_topology { using type = T; };
        template<typename T>
        struct printer_topology<T, std::void_t<typename T::topology_type>> { using type = typename T::topology_type; };

        template<typename graph_t, typename stream_t>
        void print_node_names(stream_t& stream, bool inShowVertexIds) {
            using topo_t = typename printer_topology<std::decay_t<graph_t>>::type;
            topo_t::for_each(
                [&] (auto v) {
                    using vt = decltype(v);
                    stream << vt::id() << "(" << node_name<vt>();
                    if (inShowVertexIds) {
                        stream << " " << vt::id();
                    }
                    stream << ")\n";
                }
            );
        }

        template<typename graph_t, typename stream_t, std::size_t... I>
        void print_graph_edges_impl(stream_t& stream, bool inShowLinkTypes, std::index_sequence<I...>) {
            using topo_t = typename printer_topology<std::decay_t<graph_t>>::type;
            using edge_list_t = typename printer_edge_types<std::decay_t<graph_t>>::type;
            constexpr auto edges_ids = topo_t::edges();

            (([&] {
                using edge_t = typename detail::type_list_at<I, edge_list_t>::type;
                const auto& edge = edges_ids[I];

                stream << edge.first << " -->";
                if constexpr (printer_edge_label<edge_t>::available) {
                    if (inShowLinkTypes) {
                        stream << "|" << printer_edge_label<edge_t>::value() << "|";
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
    }

    template<typename graph_t, typename stream_t>
    void print_graph(stream_t& stream, const std::string_view& inGraphName, bool inShowLinkTypes, bool inShowVertexIds) {
        using topo_t = typename printer_topology<std::decay_t<graph_t>>::type;

        constexpr auto edges_ids = topo_t::edges();
        constexpr auto ids = topo_t::ids();
        print_header(stream, inGraphName);

        print_node_names<graph_t>(stream, inShowVertexIds);

        print_graph_edges<graph_t>(stream, inShowLinkTypes);

        // Print any isolated vertices (not appearing in edges)
        for (auto vid : ids) {
            bool found = false;
            for (const auto& e : edges_ids) {
                if (e.first == vid || e.second == vid) { found = true; break; }
            }
            if (!found) {
                stream << vid << "\n";
            }
        }

        print_footer(stream, inGraphName);
    }

    template<typename graph_t, typename stream_t>
    void print_pipeline(stream_t& stream, const std::string_view& inGraphName, bool inShowVertexIds) {
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