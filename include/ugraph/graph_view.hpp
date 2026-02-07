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

#include <tuple>
#include <array>
#include <utility>
#include <type_traits>

#include "topology.hpp"
#include "interval_coloring.hpp"
#include "node.hpp"
#include "edge_traits.hpp"

namespace ugraph {

    template<typename... edges_t>
    class GraphView {
        using topology_t = Topology<edges_t...>;
        using coloring_t = IntervalColoring<edges_t...>;
        static_assert(!topology_t::is_cyclic(), "Cycle detected in graph definition");

        template<typename E>
        using edge_traits = ugraph::edge_traits<E>;

        template<std::size_t... I>
        static constexpr auto make_vertices_tuple_t(std::index_sequence<I...>) ->
            std::tuple<typename topology_t::template find_type_by_id<topology_t::template id_at<I>()>::type*...>;
        using vertices_tuple_impl_t = decltype(make_vertices_tuple_t(std::make_index_sequence<topology_t::size()>{}));

        template<std::size_t Id, typename Edge>
        static constexpr auto try_edge(const Edge& e) {
            using S = typename edge_traits<Edge>::src_vertex_t;
            if constexpr (S::id() == Id) {
                return &e.first.mNode;
            }
            else {
                using D = typename edge_traits<Edge>::dst_vertex_t;
                if constexpr (D::id() == Id) {
                    return &e.second.mNode;
                }
                else {
                    return (typename topology_t::template find_type_by_id<Id>::type*)nullptr;
                }
            }
        }

        template<std::size_t Id>
        static constexpr auto get_vertex_ptr(const edges_t&... es) {
            typename topology_t::template find_type_by_id<Id>::type* r = nullptr;
            ((r = r ? r : try_edge<Id>(es)), ...);
            return r;
        }

        template<std::size_t... I>
        static constexpr vertices_tuple_impl_t build(std::index_sequence<I...>, const edges_t&... es) {
            return { get_vertex_ptr<topology_t::template id_at<I>()>(es...)... };
        }

        vertices_tuple_impl_t mVertices;

    public:

        // Expose the vertex pointer tuple type for compile-time transforms.
        using vertices_tuple_t = vertices_tuple_impl_t;

        constexpr GraphView(const edges_t&... es) :
            mVertices(build(std::make_index_sequence<topology_t::size()>{}, es...)) {}

        static constexpr auto ids() { return topology_t::ids(); }

        static constexpr std::size_t size() { return topology_t::size(); }

        static constexpr auto edges() { return topology_t::edges(); }

        static constexpr std::size_t data_instance_count() { return coloring_t::data_instance_count(); }

        static constexpr std::size_t input_count() {
            return coloring_t::input_count();
        }

        static constexpr std::size_t output_count() {
            return coloring_t::output_count();
        }

        template<std::size_t VID, std::size_t PORT>
        static constexpr std::size_t output_data_index() { return coloring_t::template output_data_index<VID, PORT>(); }

        template<std::size_t VID, std::size_t PORT>
        static constexpr std::size_t input_data_index() { return coloring_t::template input_data_index<VID, PORT>(); }

        template<typename F>
        constexpr auto apply(F&& f) const { return std::apply([&] (auto*... vp) { return f(*vp...); }, mVertices); }

        template<typename F>
        constexpr void for_each(F&& f) const { std::apply([&] (auto*... vp) { (f(*vp), ...); }, mVertices); }

    };

    template<typename E0, typename... ERest>
    GraphView(E0 const&, ERest const&...) -> GraphView<std::decay_t<E0>, std::decay_t<ERest>...>;

} // namespace ugraph
