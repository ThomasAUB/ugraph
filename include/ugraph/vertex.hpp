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

    template<
        std::size_t id,
        typename meta_type,
        std::size_t _input_count,
        std::size_t _output_count,
        typename data_t
    >
    struct RoutingVertex : ugraph::Vertex<id, meta_type> {

        using base_type = ugraph::Vertex<id, meta_type>;
        using data_t_type = data_t;

        static constexpr std::size_t input_count() { return _input_count; };
        static constexpr std::size_t output_count() { return _output_count; }

        template<std::size_t _index>
        struct Port {
            constexpr Port(RoutingVertex& vertex) :
                mVertex(vertex) {}
            static constexpr auto index() { return _index; }
            using vertex_type = RoutingVertex;
            RoutingVertex& mVertex;
        };

        template<std::size_t index>
        struct OutputPort : Port<index> {
            constexpr OutputPort(RoutingVertex& vertex) :
                Port<index>(vertex) {}
            template<typename other_port>
            constexpr auto operator>>(const other_port& p) const {
                return Edge<OutputPort<index>, other_port>(*this, p);
            }
        };

        constexpr RoutingVertex(meta_type& impl) : mImpl(impl) {}

        template<std::size_t index = 0>
        constexpr auto in() {
            static_assert(index < _input_count);
            return Port<index>(*this);
        }

        template<std::size_t index = 0>
        constexpr auto out() {
            static_assert(index < _output_count);
            return OutputPort<index>(*this);
        }

        constexpr meta_type& get_user_type() {
            return mImpl;
        }

        constexpr const meta_type& get_user_type() const {
            return mImpl;
        }

        template<std::size_t index>
        constexpr void set_input_buffer(data_t& d) {
            static_assert(index < _input_count);
            mInputs[index] = &d;
        }

        template<std::size_t index>
        constexpr void set_output_buffer(data_t& d) {
            static_assert(index < _output_count);
            mOutputs[index] = &d;
        }

        template<std::size_t index>
        constexpr data_t& input() {
            static_assert(index < _input_count);
            return *mInputs[index];
        }

        template<std::size_t index>
        constexpr const data_t& input() const {
            static_assert(index < _input_count);
            return *mInputs[index];
        }

        template<std::size_t index>
        constexpr data_t& output() {
            static_assert(index < _output_count);
            return *mOutputs[index];
        }

        template<std::size_t index>
        constexpr const data_t& output() const {
            static_assert(index < _output_count);
            return *mOutputs[index];
        }

    private:
        meta_type& mImpl;
        data_t* mInputs[_input_count == 0 ? 1 : _input_count] {};
        data_t* mOutputs[_output_count == 0 ? 1 : _output_count] {};
    };

}
