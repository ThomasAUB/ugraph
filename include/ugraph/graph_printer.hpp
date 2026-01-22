#pragma once

#include <string_view>

namespace ugraph {

    namespace {

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

    }

    template<typename graph_t, typename stream_t>
    void print_graph(stream_t& stream, const std::string_view& inGraphName = "") {

        constexpr auto edges_ids = graph_t::edges();
        constexpr auto ids = graph_t::ids();
        constexpr std::size_t vertex_count = graph_t::size();

        print_header(stream, inGraphName);

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