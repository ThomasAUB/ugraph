#pragma once

#include <string_view>
#include <type_traits>
#include "topology.hpp"
#include "graph_view.hpp"

namespace ugraph {

    namespace {

        // Allow showing an alternative display type (strip wrappers like NodeTag)
        template<typename T> struct user_type { using type = T; };

        template<std::size_t Id, typename N>
        struct user_type<NodeTag<Id, N>> { using type = N; };

        // Raw printer that returns the compiler-generated type name for T,
        // then trims common prefixes and namespaces to a bare name.
        template<typename T>
        constexpr std::string_view type_name() {
#if defined(__clang__)
            constexpr std::string_view p = __PRETTY_FUNCTION__;
            constexpr std::string_view key = "type_name() [T = ";
            const auto start = p.find(key);
            if (start == p.npos) return p;
            const auto spos = start + key.size();
            const auto e = p.find(']', spos);
            std::string_view s = (e == p.npos) ? p.substr(spos) : p.substr(spos, e - spos);
#elif defined(__GNUC__)
            constexpr std::string_view p = __PRETTY_FUNCTION__;
            constexpr std::string_view key = "with T = ";
            const auto start = p.find(key);
            if (start == p.npos) return p;
            const auto spos = start + key.size();
            const auto e = p.find(']', spos);
            std::string_view s = (e == p.npos) ? p.substr(spos) : p.substr(spos, e - spos);
#elif defined(_MSC_VER)
            constexpr std::string_view p = __FUNCSIG__;
            constexpr std::string_view key = "type_name<";
            const auto start = p.find(key);
            std::string_view s;
            if (start == p.npos) {
                s = p;
            }
            else {
                const auto st = start + key.size();
                const auto e = p.find(">(void)", st);
                if (e == p.npos) {
                    const auto e2 = p.find('>', st);
                    s = (e2 == p.npos) ? p.substr(st) : p.substr(st, e2 - st);
                }
                else {
                    s = p.substr(st, e - st);
                }
            }
#else
            std::string_view s = "unknown";
#endif
            // Trim common compiler prefixes (e.g. "struct ", "class ", "enum ")
            constexpr std::string_view keys[] = { "struct ", "class ", "enum " };
            for (auto k : keys) {
                if (s.rfind(k, 0) == 0) { s = s.substr(k.size()); break; }
            }
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
                    stream << vt::id() << "(" << node_name<vt>() << ")\n";
                }
            );
        }
    }

    template<typename graph_t, typename stream_t>
    void print_graph(stream_t& stream, const std::string_view& inGraphName = "") {

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
    void print_pipeline(stream_t& stream, const std::string_view& inGraphName = "") {

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