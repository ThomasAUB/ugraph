#pragma once

#include <tuple>
#include <array>
#include <utility>
#include <type_traits>

#include "topology.hpp"
#include "light_pipeline_vertex.hpp"

namespace ugraph {

    // A lightweight graph that ONLY performs compile-time topological ordering
    // of vertices and exposes traversal helpers. It does not allocate or wire
    // data buffers. Users can implement their own executor / router by
    // iterating vertices in topological order and managing any per-edge
    // storage themselves.
    template<typename... edges_t>
    class LightPipelineGraph {

        using topology_t = Topology<edges_t...>;
        static_assert(!topology_t::is_cyclic(), "Cycle detected in graph definition");

        // Edge traits (only structural; no buffer info)
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

        template<std::size_t... I>
        static auto make_vertices_tuple_t(std::index_sequence<I...>) ->
            std::tuple<typename topology_t::template find_type_by_id<topology_t::ids()[I]>::type*...>;
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

        // ===== buffer count calculation ===== //

        // --- Compile-time minimal data instance computation (edge-coloring) ---
        // We treat each unique (producer vertex id, output port index) as a resource whose
        // lifetime spans from the producer vertex execution position to the furthest
        // downstream consumer vertex position. The minimal number of simultaneously
        // required data instances equals the chromatic number of this interval graph,
        // which can be obtained greedily after sorting by start positions.

        // Producer tag representing a unique output port of a vertex.
        template<std::size_t _vid, std::size_t _port>
        struct producer_tag { static constexpr std::size_t vid = _vid; static constexpr std::size_t port = _port; };

        template<typename... Ts> struct type_list {};

        // Append unique helper (by vid+port)
        template<typename List, typename Tag> struct append_unique;
        template<typename Tag, typename... Ts>
        struct append_unique<type_list<Ts...>, Tag> {
            static constexpr bool exists = ((Tag::vid == Ts::vid && Tag::port == Ts::port) || ... || false);
            using type = std::conditional_t<exists, type_list<Ts...>, type_list<Ts..., Tag>>;
        };

        // Fold edges into producer list
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

        // Helper: position in topologically ordered ids array
        static constexpr std::size_t id_to_pos(std::size_t id) {
            auto ids = topology_t::ids();
            for (std::size_t i = 0; i < topology_t::size(); ++i) {
                if (ids[i] == id) return i;
            }
            return (std::size_t) -1;
        }

        template<std::size_t N, typename List> struct type_list_at;
        template<std::size_t N, typename T, typename... Ts>
        struct type_list_at<N, type_list<T, Ts...>> : type_list_at<N - 1, type_list<Ts...>> {};
        template<typename T, typename... Ts>
        struct type_list_at<0, type_list<T, Ts...>> { using type = T; };

        // Find producer index compile-time (linear recursion)
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
                // Extend end positions based on consumer vertices of each edge
                (
                    [&] () {
                        using ET = edge_traits<edges_t>;
                        constexpr std::size_t idx = find_prod_index_impl<ET::src_id, ET::src_port_index, 0>::value;
                        const std::size_t dpos = id_to_pos(ET::dst_id);
                        if (dpos > lt.end[idx]) lt.end[idx] = dpos;
                    }(), ...
                        );
            }
            return lt;
        }
        static constexpr auto lifetimes = build_lifetimes();

        // ===== Buffer assignment (which producer gets which buffer index) ===== //
        // We replicate the greedy coloring but record the buffer index chosen for
        // each producer interval. This allows a compile-time mapping from
        // (vertex id, output port index) -> buffer index. Inputs map to the
        // buffer index of their producing output.

        struct assignment_t {
            std::array<std::size_t, producer_count == 0 ? 1 : producer_count> buf {}; // per producer
            std::size_t count {}; // total buffers used
        };

        static constexpr assignment_t build_assignment() {
            assignment_t a {};
            if constexpr (producer_count > 0) {
                // Order producers by start time (same as compute_min_instances)
                std::array<std::size_t, producer_count> order {};
                for (std::size_t i = 0; i < producer_count; ++i) order[i] = i;
                for (std::size_t i = 0; i < producer_count; ++i) {
                    std::size_t best = i;
                    for (std::size_t j = i + 1; j < producer_count; ++j) {
                        if (lifetimes.start[order[j]] < lifetimes.start[order[best]]) best = j;
                    }
                    if (best != i) {
                        auto tmp = order[i]; order[i] = order[best]; order[best] = tmp;
                    }
                }
                std::array<std::size_t, producer_count> buffer_end {}; // end position for each allocated buffer
                std::size_t buffers = 0;
                for (std::size_t k = 0; k < producer_count; ++k) {
                    auto p = order[k];
                    auto s = lifetimes.start[p];
                    auto e = lifetimes.end[p];
                    std::size_t reuse = buffers; // default allocate new
                    for (std::size_t b = 0; b < buffers; ++b) {
                        if (buffer_end[b] < s) { reuse = b; break; }
                    }
                    if (reuse == buffers) { // allocate new buffer slot
                        buffer_end[buffers] = e;
                        a.buf[p] = buffers;
                        ++buffers;
                    }
                    else { // reuse existing slot
                        buffer_end[reuse] = e;
                        a.buf[p] = reuse;
                    }
                }
                a.count = buffers;
            }
            else {
                a.count = 0;
            }
            return a;
        }

        static constexpr assignment_t assignment = build_assignment();

        // ===== Helper to expose compile-time mapping ===== //

        // Find source (producer) for a given destination vertex id + input port index.
        template<std::size_t DVID, std::size_t DPORT, typename... Es>
        struct find_input_edge_impl; // fwd

        template<std::size_t DVID, std::size_t DPORT>
        struct find_input_edge_impl<DVID, DPORT> { // not found
            static constexpr std::size_t src_vid = (std::size_t) -1;
            static constexpr std::size_t src_port = (std::size_t) -1;
        };

        template<std::size_t DVID, std::size_t DPORT, typename E0, typename... Rest>
        struct find_input_edge_impl<DVID, DPORT, E0, Rest...> {
            using tr = edge_traits<E0>;
            static constexpr bool match = (tr::dst_id == DVID && tr::dst_port_index == DPORT);
            static constexpr std::size_t src_vid = match ? tr::src_id : find_input_edge_impl<DVID, DPORT, Rest...>::src_vid;
            static constexpr std::size_t src_port = match ? tr::src_port_index : find_input_edge_impl<DVID, DPORT, Rest...>::src_port;
        };

        template<std::size_t DVID, std::size_t DPORT>
        struct find_input_edge : find_input_edge_impl<DVID, DPORT, edges_t...> {};

        // Compile-time accessor: output (producer) port -> data instance index
        template<std::size_t VID, std::size_t PORT>
        static constexpr std::size_t data_index_for_output() {
            constexpr std::size_t pidx = find_prod_index_impl<VID, PORT, 0>::value;
            static_assert(pidx != (std::size_t) -1, "(vertex id, output port) not a producer in this graph");
            return assignment.buf[pidx];
        }

        // Compile-time accessor: input (consumer) port -> data instance index of its producer
        template<std::size_t VID, std::size_t PORT>
        static constexpr std::size_t data_index_for_input() {
            constexpr std::size_t src_vid = find_input_edge<VID, PORT>::src_vid;
            constexpr std::size_t src_port = find_input_edge<VID, PORT>::src_port;
            static_assert(src_vid != (std::size_t) -1, "No edge found feeding (vertex id, input port)");
            return data_index_for_output<src_vid, src_port>();
        }

        // ============================ //

    public:
        /**
         * @brief Construct the lightweight pipeline graph view.
         *
         * The constructor only stores pointers to the vertex objects embedded
         * in the provided edge objects and arranges them according to the
         * compile-time topological order. No dynamic allocation or runtime
         * topological sorting is performed.
         *
         * @param es Edge objects (typically temporaries) that contain the
         *           source/destination vertex wrapper instances. Their types
         *           define the graph structure at compile time.
         */
        LightPipelineGraph(const edges_t&... es)
            : mVertices(build(std::make_index_sequence<topology_t::size()>{}, es...)) {}

        /**
         * @brief Get the compile-time array of vertex IDs in topological order.
         * @return constexpr std::array<std::size_t, size()> listing vertex IDs.
         */
        static constexpr auto ids() { return topology_t::ids(); }

        /**
         * @brief Number of distinct vertices in the graph.
         * @return Count of vertices.
         */
        static constexpr std::size_t size() { return topology_t::size(); }

        /**
         * @brief Minimal number of simultaneous data instances (buffer slots)
         *        required so that every produced value can live until its
         *        last consumer (derived from greedy interval coloring).
         */
        static constexpr std::size_t data_instance_count() { return assignment.count; }

        /**
         * @brief Obtain the buffer index assigned to a specific producer
         *        (vertex id + output port).
         * @tparam VID Vertex ID (compile-time constant via Vertex::id()).
         * @tparam PORT Output port index.
         * @return Buffer slot index.
         * @note Triggers a compile-time error if (VID, PORT) never produces data.
         */
        template<std::size_t VID, std::size_t PORT>
        static constexpr std::size_t data_index_of_output() { return data_index_for_output<VID, PORT>(); }

        /**
         * @brief Obtain the buffer index that feeds a consumer input port.
         * @tparam VID Consumer vertex ID.
         * @tparam PORT Input port index.
         * @return Buffer slot index corresponding to the producing output.
         * @note Compile-time error if the input has no matching incoming edge.
         */
        template<std::size_t VID, std::size_t PORT>
        static constexpr std::size_t data_index_of_input() { return data_index_for_input<VID, PORT>(); }

        /**
         * @brief Invoke a callable with references to all vertices as separate parameters.
         * @tparam F Callable taking N vertex references (N = size()).
         * @param f Functor/lambda.
         */
        template<typename F>
        void apply(F&& f) const { std::apply([&] (auto*... vp) { f(*vp...); }, mVertices); }

        /**
         * @brief Iterate over vertices (topological order) invoking a unary callable.
         * @tparam F Callable taking (vertex&).
         * @param f Functor/lambda executed once per vertex.
         */
        template<typename F>
        void for_each(F&& f) const { std::apply([&] (auto*... vp) { (f(*vp), ...); }, mVertices); }
    };

    template<typename E0, typename... ERest>
    LightPipelineGraph(E0 const&, ERest const&...)
        -> LightPipelineGraph<std::decay_t<E0>, std::decay_t<ERest>...>;

} // namespace ugraph
