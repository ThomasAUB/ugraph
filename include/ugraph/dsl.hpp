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

#include <type_traits>
#include <utility>

#include "node.hpp"

namespace ugraph {

    template<
        typename out_port_t,
        typename in_port_t,
        typename = std::void_t<typename out_port_t::data_type, typename out_port_t::node_type, typename in_port_t::node_type>
    >
    constexpr std::pair<out_port_t, in_port_t> operator>>(const out_port_t& out, const in_port_t& in) {
        return std::pair<out_port_t, in_port_t>{ out, in };
    }

    template<
        typename data_t,
        typename in_port_t,
        typename = std::void_t<typename in_port_t::node_type>,
        typename = std::enable_if_t<!detail::is_a_port<data_t>::value, int>
    >
    constexpr InDataBind<data_t, in_port_t> operator|(data_t& data, const in_port_t& in) {
        return InDataBind<data_t, in_port_t>{ &data, in };
    }

    template<
        typename out_port_t,
        typename data_t,
        typename = std::void_t<typename out_port_t::data_type, typename out_port_t::node_type>,
        typename = std::enable_if_t<!detail::is_a_port<data_t>::value, int>
    >
    constexpr OutDataBind<data_t, out_port_t> operator|(const out_port_t& out, data_t& data) {
        return OutDataBind<data_t, out_port_t>{ &data, out };
    }

} // namespace ugraph
