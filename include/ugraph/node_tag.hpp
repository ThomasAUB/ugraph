#pragma once

#include <cstddef>
#include <utility>

namespace ugraph {

    template<std::size_t _id, typename _module_t>
    struct NodeTag {
        static constexpr std::size_t id() { return _id; }
        using module_type = _module_t;
        using node_type = NodeTag<_id, _module_t>;
    };

    template<typename src_port_t, typename dst_port_t>
    using Link = std::pair<src_port_t, dst_port_t>;

} // namespace ugraph
