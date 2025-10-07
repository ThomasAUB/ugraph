#pragma once

#include <cstddef>
#include <utility>
#include "vertex.hpp"

namespace ugraph {

    // A lightweight pipeline vertex that only encodes compile-time metadata
    // (id, port counts, user meta object reference) and port tagging for edge
    // construction. It intentionally does NOT store or manage any buffer
    // pointers. This allows users to build their own execution / wiring layer
    // with custom storage or scheduling policies while still reusing the
    // Topology / routing compile-time machinery.
    template<
        std::size_t id,
        typename meta_type,
        std::size_t _input_count,
        std::size_t _output_count
    >
    struct LightPipelineVertex : ugraph::Vertex<id, meta_type> {

        using base_type = ugraph::Vertex<id, meta_type>;
        using meta_t = meta_type;

        template<std::size_t _index>
        struct Port {
            constexpr Port(LightPipelineVertex& v) : mVertex(v) {}
            static constexpr auto index() { return _index; }
            using vertex_type = LightPipelineVertex;
            LightPipelineVertex& mVertex;
        };

        template<std::size_t _index>
        struct OutputPort : Port<_index> {
            constexpr OutputPort(LightPipelineVertex& v) : Port<_index>(v) {}
            template<typename OtherPort>
            constexpr auto operator>>(const OtherPort& p) const {
                return Edge<OutputPort<_index>, OtherPort>(*this, p);
            }
        };

        template<std::size_t index = 0>
        constexpr auto in() {
            static_assert(index < _input_count, "Input port index out of range");
            return Port<index>(*this);
        }

        template<std::size_t index = 0>
        constexpr auto out() {
            static_assert(index < _output_count, "Output port index out of range");
            return OutputPort<index>(*this);
        }

        static constexpr std::size_t input_count() { return _input_count; }
        static constexpr std::size_t output_count() { return _output_count; }

        constexpr meta_type& get_user_type() { return mImpl; }
        constexpr const meta_type& get_user_type() const { return mImpl; }

        constexpr LightPipelineVertex(meta_type& impl) : mImpl(impl) {}

    private:
        meta_type& mImpl;
    };

} // namespace ugraph
