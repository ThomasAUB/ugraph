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

#include <cstddef>
#include <utility>

namespace ugraph {

    template<
        std::size_t _id,
        typename _module_t,
        std::size_t _priority = 0
    >
    struct NodeTag {
        static constexpr std::size_t id() { return _id; }
        static constexpr std::size_t priority() { return _priority; }
        using module_type = _module_t;
        using node_type = NodeTag<_id, _module_t>;
    };

    template<typename node_t, std::size_t _index>
    struct PortTag {
        using node_type = node_t;
        static constexpr std::size_t index() { return _index; }
    };

    template<
        std::size_t _id,
        typename _module_t,
        std::size_t _input_count,
        std::size_t _output_count,
        std::size_t _priority = 0
    >
    struct NodePortTag : NodeTag<_id, _module_t, _priority> {

        using base_type = NodeTag<_id, _module_t, _priority>;
        using module_type = _module_t;
        using node_type = NodePortTag<_id, _module_t, _input_count, _output_count, _priority>;

        static constexpr std::size_t input_count() { return _input_count; }
        static constexpr std::size_t output_count() { return _output_count; }

        template<std::size_t index>
        using InputPort = PortTag<NodePortTag, index>;

        template<std::size_t index>
        using OutputPort = PortTag<NodePortTag, index>;

    };

    template<typename src_port_t, typename dst_port_t>
    using Link = std::pair<src_port_t, dst_port_t>;

} // namespace ugraph
