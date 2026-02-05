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
#include <type_traits>
#include "node_tag.hpp"

namespace ugraph {

    template<
        std::size_t _id,
        typename _module_t,
        std::size_t _input_count,
        std::size_t _output_count,
        std::size_t _priority = 0
    >
    struct Node :
        ugraph::NodePortTag<
        _id,
        _module_t,
        _input_count,
        _output_count,
        _priority
        > {

        using base_type = ugraph::NodePortTag<_id, _module_t, _input_count, _output_count, _priority>;
        using module_type = typename base_type::module_type;

        template<std::size_t _index>
        struct Port : ugraph::PortTag<Node, _index> {
            constexpr Port(Node& v) : mNode(v) {}
            Node& mNode;
        };

        template<std::size_t _index>
        struct OutputPort : Port<_index> {
            constexpr OutputPort(Node& v) : Port<_index>(v) {}
            template<typename other_port_t>
            constexpr auto operator >> (const other_port_t& p) const {
                return Link<OutputPort<_index>, other_port_t>(*this, p);
            }
        };

        template<std::size_t _index = 0>
        constexpr auto in() {
            static_assert(_index < _input_count, "Input port index out of range");
            return Port<_index>(*this);
        }

        template<std::size_t _index = 0>
        constexpr auto out() {
            static_assert(_index < _output_count, "Output port index out of range");
            return OutputPort<_index>(*this);
        }

        constexpr module_type& module() { return mModule; }
        constexpr const module_type& module() const { return mModule; }

        constexpr Node(module_type& module) : mModule(module) {}

    private:
        module_type& mModule;
    };

    template<std::size_t id, std::size_t in, std::size_t out, std::size_t prio = 0, typename m>
    Node(m&) -> Node<id, std::remove_cv_t<std::remove_reference_t<m>>, in, out, prio>;

} // namespace ugraph
