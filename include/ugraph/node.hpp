#pragma once

#include <cstddef>
#include <utility>
#include "node_tag.hpp"

namespace ugraph {

    template<
        std::size_t _id,
        typename _module_t,
        std::size_t _input_count,
        std::size_t _output_count
    >
    struct Node : ugraph::NodeTag<_id, _module_t> {

        using base_type = ugraph::NodeTag<_id, _module_t>;
        using module_type = _module_t;

        template<std::size_t _index>
        struct Port {
            constexpr Port(Node& v) : mNode(v) {}
            static constexpr auto index() { return _index; }
            using node_type = Node;
            Node& mNode;
        };

        template<std::size_t _index>
        struct OutputPort : Port<_index> {
            constexpr OutputPort(Node& v) : Port<_index>(v) {}
            template<typename other_port_t>
            constexpr auto operator>>(const other_port_t& p) const {
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

        static constexpr std::size_t input_count() { return _input_count; }
        static constexpr std::size_t output_count() { return _output_count; }

        constexpr module_type& module() { return mModule; }
        constexpr const module_type& module() const { return mModule; }

        constexpr Node(module_type& module) : mModule(module) {}

    private:
        module_type& mModule;
    };

} // namespace ugraph
