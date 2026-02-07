#pragma once

#include <type_traits>
#include <cstddef>

namespace ugraph {

    template<typename E>
    struct edge_traits {
        using edge_t = std::decay_t<E>;
        using src_port_t = typename edge_t::first_type;
        using dst_port_t = typename edge_t::second_type;
        using src_vertex_t = typename src_port_t::node_type;
        using dst_vertex_t = typename dst_port_t::node_type;
        static constexpr std::size_t src_id = src_vertex_t::id();
        static constexpr std::size_t dst_id = dst_vertex_t::id();
        static constexpr std::size_t src_port_index = src_port_t::index();
        static constexpr std::size_t dst_port_index = dst_port_t::index();
    };

} // namespace ugraph
