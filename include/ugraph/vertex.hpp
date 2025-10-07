#pragma once

#include <cstddef>
#include <utility>

namespace ugraph {

    template<std::size_t _id, typename meta_type>
    struct Vertex {
        static constexpr std::size_t id() { return _id; }
        using type = meta_type;
        using vertex_type = Vertex<_id, meta_type>;
    };

    template<typename src_vertex_t, typename dst_vertex_t>
    using Edge = std::pair<src_vertex_t, dst_vertex_t>;

} // namespace ugraph
