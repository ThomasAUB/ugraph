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

#include <string_view>
#include <type_traits>
#include "topology.hpp"
#include "graph_view.hpp"

namespace ugraph {



    template<typename graph_t, typename stream_t>
    void print_graph(stream_t& stream, const std::string_view& inGraphName = "");

    template<typename graph_t, typename stream_t>
    void print_pipeline(stream_t& stream, const std::string_view& inGraphName = "");



    namespace {

        // Allow showing an alternative display type (strip wrappers like NodeTag)
        template<typename T> struct user_type { using type = T; };

        template<std::size_t Id, typename N>
        struct user_type<NodeTag<Id, N>> { using type = N; };

        template<std::size_t Id, typename N, std::size_t In, std::size_t Out, std::size_t Prio>
        struct user_type<NodePortTag<Id, N, In, Out, Prio>> { using type = N; };

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

        // Extract underlying Topology from an edges parameter pack by folding back to Topology
        template<typename... edges_t> struct topology_from_edges { using type = Topology<edges_t...>; };

        // Map an Edge type (possibly using runtime Node/Port types) to a Link of NodeTag types
        // Helper: get the underlying node-tag for either a tag type or a Port type
        template<typename T, typename = void>
        struct node_tag { using type = T; };
        template<typename port_t>
        struct node_tag<port_t, std::void_t<typename port_t::node_type>> {
            using type = typename port_t::node_type::base_type;
        };

        template<typename edge_t> struct tag_edge_from_edge { using type = edge_t; };
        template<typename src_t, typename dst_t>
        struct tag_edge_from_edge<Link<src_t, dst_t>> {
            using type = Link<typename node_tag<src_t>::type, typename node_tag<dst_t>::type>;
        };

        // Extract underlying Topology from types we know (Topology or GraphView)
        template<typename graph_t> struct underlying_topology { using type = void; };
        template<typename... edges_t> struct underlying_topology<Topology<edges_t...>> { using type = typename topology_from_edges<edges_t...>::type; };
        template<typename... edges_t>
        struct underlying_topology<GraphView<edges_t...>> {
            using type = typename topology_from_edges<typename tag_edge_from_edge<edges_t>::type...>::type;
        };

        template<typename graph_t, typename stream_t>
        void print_node_names(stream_t& stream) {
            using topo_t = typename underlying_topology<std::decay_t<graph_t>>::type;
            static_assert(!std::is_same_v<topo_t, void>, "Incompatible graph type");
            topo_t::for_each(
                [&] (auto v) {
                    using vt = decltype(v);
                    stream << vt::id() << "(" << node_name<vt>() << " " << vt::id() << ")\n";
                }
            );
        }
    }

    template<typename graph_t, typename stream_t>
    void print_graph(stream_t& stream, const std::string_view& inGraphName) {

        constexpr auto edges_ids = graph_t::edges();
        constexpr auto ids = graph_t::ids();
        constexpr std::size_t vertex_count = graph_t::size();

        print_header(stream, inGraphName);

        print_node_names<graph_t>(stream);

        // Print each configured edge as a mermaid arrow
        for (const auto& e : edges_ids) {
            stream << e.first << " --> " << e.second << "\n";
        }

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
    void print_pipeline(stream_t& stream, const std::string_view& inGraphName) {

        constexpr auto edges_ids = graph_t::edges();
        constexpr auto ids = graph_t::ids();
        constexpr std::size_t vertex_count = graph_t::size();

        print_header(stream, inGraphName);

        print_node_names<graph_t>(stream);

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