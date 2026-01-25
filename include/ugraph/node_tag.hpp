#pragma once

#include <cstddef>
#include <utility>

namespace ugraph {

    template<std::size_t _id, typename _module_t, std::size_t _priority = 0>
    struct NodeTag {
        static constexpr std::size_t id() { return _id; }
        static constexpr std::size_t priority() { return _priority; }
        using module_type = _module_t;
        using node_type = NodeTag<_id, _module_t, _priority>;

        // Convenience nested port type: use `MyTag::In<0>` or `MyTag::Out<1>` when declaring
        // links. The presence of a constexpr `index()` is detected by Topology and used
        // as the port index. Ports are optional — if a port type doesn't provide
        // `index()` Topology falls back to zero.
        template<std::size_t PortIndex>
        struct Port {
            static constexpr std::size_t index() { return PortIndex; }
            using node_type = NodeTag<_id, _module_t, _priority>;
        };

        template<std::size_t PortIndex>
        using In = Port<PortIndex>;

        template<std::size_t PortIndex>
        using Out = Port<PortIndex>;
    };

    template<typename src_port_t, typename dst_port_t>
    using Link = std::pair<src_port_t, dst_port_t>;

} // namespace ugraph
