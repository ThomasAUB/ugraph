#pragma once

#include "topology.hpp"
#include "pipeline_vertex.hpp"
#include <array>
#include <tuple>
#include <initializer_list>
#include <type_traits>

namespace ugraph {

    // PipelineGraph is parameterized directly on the edge pack so it can drive both Topology and routing.
    template<typename data_t, typename... edges_t>
    class PipelineGraph {

        using topology_t = Topology<edges_t...>;
        static_assert(!topology_t::is_cyclic(), "Cycle detected in graph definition");

        // Edge traits including port indices
        template<typename E>
        struct edge_traits {
            using edge_t = std::decay_t<E>;
            using src_port_t = typename edge_t::first_type;  // OutputPort<idx>
            using dst_port_t = typename edge_t::second_type; // Port<idx>
            using src_vertex_t = typename src_port_t::vertex_type;
            using dst_vertex_t = typename dst_port_t::vertex_type;
            static constexpr std::size_t src_id = src_vertex_t::id();
            static constexpr std::size_t dst_id = dst_vertex_t::id();
            static constexpr std::size_t src_port_index = src_port_t::index();
            static constexpr std::size_t dst_port_index = dst_port_t::index();
        };

        // Producer list (unique (vertex id, output port))
        template<std::size_t _vid, std::size_t _port>
        struct producer_tag {
            static constexpr std::size_t vid = _vid;
            static constexpr std::size_t port = _port;
        };
        template<typename... Ts> struct type_list {};
        template<typename List, typename Tag> struct append_unique;
        template<typename Tag, typename... Ts>
        struct append_unique<type_list<Ts...>, Tag> {
            static constexpr bool exists = ((Tag::vid == Ts::vid && Tag::port == Ts::port) || ... || false);
            using type = std::conditional_t<exists, type_list<Ts...>, type_list<Ts..., Tag>>;
        };
        template<typename List, typename Edge> struct add_edge_prod {
            using tag = producer_tag<edge_traits<Edge>::src_id, edge_traits<Edge>::src_port_index>;
            using type = typename append_unique<List, tag>::type;
        };
        template<typename List, typename... Es> struct fold_prod;
        template<typename List> struct fold_prod<List> { using type = List; };
        template<typename List, typename E, typename... R>
        struct fold_prod<List, E, R...> { using type = typename fold_prod<typename add_edge_prod<List, E>::type, R...>::type; };
        using producer_list = typename fold_prod<type_list<>, edges_t...>::type;
        template<typename List> struct list_size;
        template<typename... Ts> struct list_size<type_list<Ts...>> : std::integral_constant<std::size_t, sizeof...(Ts)> {};
        static constexpr std::size_t producer_count = list_size<producer_list>::value;

        // Helpers
        static constexpr std::size_t id_to_pos(std::size_t id) {
            auto ids = topology_t::ids();
            for (std::size_t i = 0; i < topology_t::size(); ++i) {
                if (ids[i] == id) {
                    return i;
                }
            }
            return (std::size_t) -1;
        }
        template<std::size_t N, typename List> struct type_list_at;
        template<std::size_t N, typename T, typename... Ts>
        struct type_list_at<N, type_list<T, Ts...>> : type_list_at<N - 1, type_list<Ts...>> {};
        template<typename T, typename... Ts>
        struct type_list_at<0, type_list<T, Ts...>> { using type = T; };
        template<std::size_t VID, std::size_t PORT, std::size_t I>
        struct find_prod_index_impl {
            using PT = typename type_list_at<I, producer_list>::type;
            static constexpr std::size_t value = (PT::vid == VID && PT::port == PORT) ? I : find_prod_index_impl<VID, PORT, I + 1>::value;
        };
        template<std::size_t VID, std::size_t PORT>
        struct find_prod_index_impl<VID, PORT, producer_count> { static constexpr std::size_t value = (std::size_t) -1; };

        struct lifetimes_t {
            std::array<std::size_t, producer_count == 0 ? 1 : producer_count> start {};
            std::array<std::size_t, producer_count == 0 ? 1 : producer_count> end {};
        };
        static constexpr lifetimes_t build_lifetimes() {
            lifetimes_t lt {};
            if constexpr (producer_count > 0) {
                [] <std::size_t... I>(lifetimes_t & l, std::index_sequence<I...>) {
                    ((l.start[I] = id_to_pos(type_list_at<I, producer_list>::type::vid), l.end[I] = l.start[I]), ...);
                }(lt, std::make_index_sequence<producer_count>{});
                (
                    [&] () {
                        using ET = edge_traits<edges_t>;
                        constexpr std::size_t idx = find_prod_index_impl<ET::src_id, ET::src_port_index, 0>::value;
                        const std::size_t dpos = id_to_pos(ET::dst_id);
                        if (dpos > lt.end[idx]) {
                            lt.end[idx] = dpos;
                        }
                    }(), ...
                        );
            }
            return lt;
        }
        static constexpr auto lifetimes = build_lifetimes();

        static constexpr auto assign_buffers() {
            struct result {
                std::array<std::size_t, producer_count == 0 ? 1 : producer_count> assign {};
                std::size_t buffers = 0;
            };
            result r {};
            if constexpr (producer_count > 0) {
                std::array<std::size_t, producer_count> order {};
                for (std::size_t i = 0; i < producer_count; ++i) {
                    order[i] = i;
                }
                for (std::size_t i = 0; i < producer_count; ++i) {
                    std::size_t best = i;
                    for (std::size_t j = i + 1; j < producer_count; ++j) {
                        if (lifetimes.start[order[j]] < lifetimes.start[order[best]]) {
                            best = j;
                            if (best != i) {
                                auto tmp = order[i];
                                order[i] = order[best];
                                order[best] = tmp;
                            }
                        }
                    }
                }
                std::array<std::size_t, producer_count> buffer_end {};
                for (std::size_t k = 0; k < producer_count; ++k) {
                    auto p = order[k];
                    auto s = lifetimes.start[p];
                    auto e = lifetimes.end[p];
                    std::size_t reuse = r.buffers;
                    for (std::size_t b = 0; b < r.buffers; ++b) {
                        if (buffer_end[b] < s) {
                            reuse = b;
                            break;
                        }
                    }
                    if (reuse == r.buffers) {
                        buffer_end[r.buffers] = e;
                        r.assign[p] = r.buffers;
                        ++r.buffers;
                    }
                    else {
                        buffer_end[reuse] = e;
                        r.assign[p] = reuse;
                    }
                }
            }
            return r;
        }
        static constexpr auto assignment = assign_buffers();
        static constexpr std::size_t _buffer_count = assignment.buffers;

        template<std::size_t I>
        static constexpr std::size_t buffer_for_impl(std::size_t vid, std::size_t port) {
            if constexpr (I >= producer_count) {
                return (std::size_t) -1;
            }
            else {
                using PT = typename type_list_at<I, producer_list>::type;
                return (PT::vid == vid && PT::port == port) ? assignment.assign[I] : buffer_for_impl<I + 1>(vid, port);
            }
        }
        static constexpr std::size_t buffer_for(std::size_t vid, std::size_t port) {
            if constexpr (producer_count == 0) {
                return (std::size_t) -1;
            }
            else {
                return buffer_for_impl<0>(vid, port);
            }
        }

        template<std::size_t... I>
        static auto make_vertices_tuple_t(std::index_sequence<I...>) -> std::tuple<typename topology_t::template find_type_by_id<topology_t::ids()[I]>::type*...>;
        using vertices_tuple_t = decltype(make_vertices_tuple_t(std::make_index_sequence<topology_t::size()>{}));

        template<std::size_t Id, typename Edge>
        static auto try_edge(const Edge& e) {
            using S = typename edge_traits<Edge>::src_vertex_t;
            if constexpr (S::id() == Id) {
                return &e.first.mVertex;
            }
            else {
                using D = typename edge_traits<Edge>::dst_vertex_t;
                if constexpr (D::id() == Id) {
                    return &e.second.mVertex;
                }
                else {
                    return (typename topology_t::template find_type_by_id<Id>::type*)nullptr;
                }
            }
        }
        template<std::size_t Id>
        static auto get_vertex_ptr(const edges_t&... es) {
            typename topology_t::template find_type_by_id<Id>::type* r = nullptr;
            ((r = r ? r : try_edge<Id>(es)), ...);
            return r;
        }
        template<std::size_t... I>
        static vertices_tuple_t build(std::index_sequence<I...>, const edges_t&... es) {
            constexpr auto ordered = topology_t::ids();
            return { get_vertex_ptr<ordered[I]>(es...)... };
        }

        vertices_tuple_t mVertices;
        std::array<data_t, _buffer_count == 0 ? 1 : _buffer_count> mBuffers {};

        template<typename ETraits, typename GetPtr>
        void wire_edge(GetPtr&& get_ptr) {
            constexpr std::size_t sid = ETraits::src_id;
            constexpr std::size_t s_port = ETraits::src_port_index;
            constexpr std::size_t did = ETraits::dst_id;
            constexpr std::size_t d_port = ETraits::dst_port_index;
            const std::size_t buf_idx = buffer_for(sid, s_port);
            if (buf_idx == (std::size_t) -1) return;
            auto* svp = static_cast<typename ETraits::src_vertex_t*>(get_ptr(sid));
            auto* dvp = static_cast<typename ETraits::dst_vertex_t*>(get_ptr(did));
            svp->template set_output_buffer<s_port>(mBuffers[buf_idx]);
            dvp->template set_input_buffer<d_port>(mBuffers[buf_idx]);
        }

        template<typename Vtx>
        static void run_vertex(Vtx& vertex) {
            using V = std::decay_t<Vtx>;
            auto& u = vertex.get_user_type();
            constexpr std::size_t IN = V::input_count();
            constexpr std::size_t OUT = V::output_count();
            auto invoke = [&] <std::size_t... I, std::size_t... O>(
                std::index_sequence<I...>, std::index_sequence<O...>) {
                if constexpr (IN == 0 && OUT == 0) {
                }
                else if constexpr (IN == 0) {
                    u.process(vertex.template output<O>()...);
                }
                else if constexpr (OUT == 0) {
                    u.process(vertex.template input<I>()...);
                }
                else {
                    u.process(vertex.template input<I>()..., vertex.template output<O>()...);
                }
            };
            invoke(std::make_index_sequence<IN>{}, std::make_index_sequence<OUT>{});
        }

        template<std::size_t... I>
        void execute_impl(std::index_sequence<I...>) {
            (run_vertex(*std::get<I>(mVertices)), ...);
        }

    public:

        PipelineGraph(const edges_t&... es) :
            mVertices(build(std::make_index_sequence<topology_t::size()>{}, es...)) {
            static_assert(
                ((
                    std::is_same_v<typename edge_traits<edges_t>::src_vertex_t::data_t_type, data_t> &&
                    std::is_same_v<typename edge_traits<edges_t>::dst_vertex_t::data_t_type, data_t>) && ...),
                "Mixed data_t types in graph"
                );
            auto ids_arr = topology_t::ids();
            auto get_ptr = [this, ids_arr] (std::size_t id) -> void* {
                void* result = nullptr;
                std::apply(
                    [&] (auto*... vp) {
                        std::size_t idx = 0;
                        (void) std::initializer_list<int>{
                            ((ids_arr[idx] == id ? result = vp : result), ++idx, 0)...
                        };
                    },
                    mVertices
                );
                return result;
                };
            (void) std::initializer_list<int>{
                (wire_edge<edge_traits<edges_t>>(get_ptr), 0)...
            };
        }

        template<typename F>
        void apply(F&& f) const { std::apply([&] (auto*... vp) { f(*vp...); }, mVertices); }

        template<typename F>
        void for_each(F&& f) { std::apply([&] (auto*... vp) { (f(*vp), ...); }, mVertices); }

        template<typename F>
        void for_each(F&& f) const { std::apply([&] (auto const*... vp) { (f(*vp), ...); }, mVertices); }

        static constexpr auto ids() { return topology_t::ids(); }
        static constexpr std::size_t size() { return topology_t::size(); }
        static constexpr std::size_t buffer_count() { return _buffer_count; }

        template<std::size_t VID, std::size_t PORT>
        static constexpr std::size_t buffer_index_for() { return buffer_for(VID, PORT); }
        static constexpr std::size_t buffer_index(std::size_t vid, std::size_t port) { return buffer_for(vid, port); }

        void execute() { execute_impl(std::make_index_sequence<topology_t::size()>{}); }

        template<typename Instrument>
        void execute(Instrument& inst) {
            if constexpr (!Instrument::enabled) {
                execute_impl(std::make_index_sequence<topology_t::size()>{});
            }
            else {
                inst.on_pipeline_start();
                std::size_t idx = 0;
                std::apply(
                    [&] (auto*... vp) {
                        ((inst.on_vertex_start(idx, *vp),
                            run_vertex(*vp),
                            inst.on_vertex_end(idx, *vp),
                            ++idx), ...);
                    },
                    mVertices
                );
                inst.on_pipeline_end();
            }
        }
    };

    // Deduction guide: allows "PipelineGraph(e1, e2, ...)" without specifying data_t explicitly.
    template<typename E0, typename... ERest>
    PipelineGraph(E0 const&, ERest const&...)
        -> PipelineGraph<
        typename std::decay_t<E0>::first_type::vertex_type::data_t_type,
        std::decay_t<E0>, std::decay_t<ERest>...>;

}
