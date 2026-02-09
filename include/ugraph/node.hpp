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
#include "link.hpp"

namespace ugraph {

    template<
        std::size_t id,
        typename module_t,
        typename manifest_t = typename module_t::Manifest,
        std::size_t _priority = 0
    >
    constexpr auto make_node(module_t& module);

    template<
        std::size_t _id,
        typename _module_t,
        std::size_t _input_count,
        std::size_t _output_count,
        std::size_t _priority = 0
    >
    struct NodePortTag {
        static constexpr std::size_t id() { return _id; }
        static constexpr std::size_t priority() { return _priority; }
        using module_type = _module_t;

        static constexpr std::size_t input_count() { return _input_count; }
        static constexpr std::size_t output_count() { return _output_count; }

        template<std::size_t idx>
        struct Port {
            using node_type = NodePortTag<_id, _module_t, _input_count, _output_count, _priority>;
            static constexpr std::size_t index() { return idx; }
        };

        template<std::size_t index>
        using InputPort = Port<index>;

        template<std::size_t index>
        using OutputPort = Port<index>;
    };


    template<std::size_t _id, typename module_t, typename manifest_t, std::size_t _priority = 0>
    class Node {
    public:
        using module_type = module_t;
        static constexpr std::size_t id() { return _id; }
        static constexpr std::size_t priority() { return _priority; }

        constexpr Node(module_type& module) : mModule(module) {}

        constexpr module_type& module() { return mModule; }
        constexpr const module_type& module() const { return mModule; }

        template<typename T>
        using NodeType = NodePortTag<_id, module_type,
            manifest_t::template input_count<T>(),
            manifest_t::template output_count<T>(),
            _priority>;

        template<typename T, std::size_t _index>
        struct InputPort : NodeType<T>::template InputPort<_index> {
            using data_type = T;
            constexpr InputPort(Node& n) : mNode(n) {}
            Node& mNode;
        };

        template<typename T, std::size_t _index>
        struct OutputPort : NodeType<T>::template OutputPort<_index> {
            using data_type = T;
            constexpr OutputPort(Node& n) : mNode(n) {}
            Node& mNode;

            template<typename other_port_t>
            constexpr auto operator >> (const other_port_t& p) const {
                return Link<OutputPort<T, _index>, other_port_t>(*this, p);
            }
        };

        template<typename T, std::size_t I = 0>
        constexpr auto input() const {
            static_assert(manifest_t::template contains<T>, "Type not declared in Manifest");
            return InputPort<T, I>(const_cast<Node&>(*this));
        }

        template<typename T, std::size_t I = 0>
        constexpr auto output() const {
            static_assert(manifest_t::template contains<T>, "Type not declared in Manifest");
            return OutputPort<T, I>(const_cast<Node&>(*this));
        }

    private:
        module_type& mModule;
    };

    template<
        std::size_t id,
        typename module_t,
        typename manifest_t,
        std::size_t _priority
    >
    constexpr auto make_node(module_t& module) {
        return Node<id, module_t, manifest_t, _priority>(module);
    }

} // namespace ugraph