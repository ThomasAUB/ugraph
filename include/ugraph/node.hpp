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
#include "type_traits/port_traits.hpp"

namespace ugraph {

    template<typename data_t, typename input_t>
    struct InDataBind {
        data_t* mPtr;
        input_t mPort;
    };

    template<typename data_t, typename output_t>
    struct OutDataBind {
        data_t* mPtr;
        output_t mPort;
    };

    template<typename Bind>
    struct binding_traits;

    template<typename data_t, typename in_port_t>
    struct binding_traits<InDataBind<data_t, in_port_t>> {
        using data_type = data_t;
        using port_type = in_port_t;
    };

    template<typename data_t, typename out_port_t>
    struct binding_traits<OutDataBind<data_t, out_port_t>> {
        using data_type = data_t;
        using port_type = out_port_t;
    };

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
        std::size_t _priority = 0,
        typename _spec_t = void
    >
    struct NodePortTag {
        static constexpr std::size_t id() { return _id; }
        static constexpr std::size_t priority() { return _priority; }
        static constexpr std::size_t input_count() { return _input_count; }
        static constexpr std::size_t output_count() { return _output_count; }
        using spec_type = _spec_t;

        template<std::size_t idx>
        struct Port {
            using node_type = NodePortTag<_id, _module_t, _input_count, _output_count, _priority, _spec_t>;
            using spec_type = typename node_type::spec_type;
            static constexpr std::size_t index() { return idx; }
            constexpr Port(_module_t& module) : mModule(&module) {}
            constexpr _module_t& module() const { return *mModule; }
        protected:
            _module_t* mModule;
        };

        using module_type = _module_t;
    };


    template<std::size_t _id, typename module_t, typename manifest_t, std::size_t _priority = 0>
    struct Node {

        using module_type = module_t;
        static constexpr std::size_t id() { return _id; }
        static constexpr std::size_t priority() { return _priority; }

        constexpr Node(module_type& module) : mModule(module) {}
        constexpr module_type& module() { return mModule; }
        constexpr const module_type& module() const { return mModule; }

        template<typename T>
        using NodeType = NodePortTag<
            _id,
            module_type,
            manifest_t::template input_count<T>(),
            manifest_t::template output_count<T>(),
            _priority,
            typename manifest_t::template spec_for<T>
        >;

        template<typename T, std::size_t _index>
        struct InputPort : NodeType<T>::template Port<_index> {
            using spec_type = typename manifest_t::template spec_for<T>;

            constexpr InputPort(Node& node) :
                NodeType<T>::template Port<_index>(node.module()) {}
        };

        template<typename T, std::size_t _index>
        struct OutputPort : NodeType<T>::template Port<_index> {
            using data_type = typename manifest_t::template data_type_for<T>;
            using spec_type = typename manifest_t::template spec_for<T>;
            constexpr OutputPort(Node& node) :
                NodeType<T>::template Port<_index>(node.module()) {}
        };

        // single input
        template<typename T>
        constexpr auto input() { return make_port<T, 0, false, true>(); }

        // single output
        template<typename T>
        constexpr auto output() { return make_port<T, 0, true, true>(); }

        template<typename T, std::size_t I>
        constexpr auto input() { return make_port<T, I, false, false>(); }

        template<typename T, std::size_t I>
        constexpr auto output() { return make_port<T, I, true, false>(); }

    private:

        template<typename T, std::size_t I, bool is_output, bool enforce_single>
        constexpr auto make_port() {

            using input_port_t = InputPort<T, I>;
            using output_port_t = OutputPort<T, I>;

            static_assert(manifest_t::template contains<T>, "Type not declared in Manifest");

            if constexpr (is_output) {
                if constexpr (enforce_single) {
                    static_assert(manifest_t::template output_count<T>() == 1, "Undefined output index");
                }
                else {
                    static_assert(manifest_t::template output_count<T>() > I, "Invalid output index");
                }
                return output_port_t { *this };
            }
            else {
                if constexpr (enforce_single) {
                    static_assert(manifest_t::template input_count<T>() == 1, "Undefined input index");
                }
                else {
                    static_assert(manifest_t::template input_count<T>() > I, "Invalid input index");
                }
                return input_port_t { *this };
            }
        }

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

namespace ugraph::detail {
    template<typename T>
    struct is_data_binding : std::false_type {};

    template<typename data_t, typename in_port_t>
    struct is_data_binding<InDataBind<data_t, in_port_t>> : std::true_type {};

    template<typename data_t, typename out_port_t>
    struct is_data_binding<OutDataBind<data_t, out_port_t>> : std::true_type {};

} // namespace ugraph