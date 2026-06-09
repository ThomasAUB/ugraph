#pragma once

#include <cstddef>

template<auto Factory>
struct GraphHelper;

template<typename Graph, typename... Args, Graph(*Factory)(Args...)>
struct GraphHelper<Factory> {
    using graph_t = Graph;
    using graph_data_t = typename graph_t::graph_data_t;

    template<typename data_t>
    static constexpr std::size_t data_count() {
        return graph_data_t::template count<data_t>();
    }
};