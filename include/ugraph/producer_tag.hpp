#pragma once

#include <cstddef>

namespace ugraph {

template<std::size_t Vid, std::size_t Port>
struct producer_tag {
    static constexpr std::size_t vid = Vid;
    static constexpr std::size_t port = Port;
};

} // namespace ugraph
