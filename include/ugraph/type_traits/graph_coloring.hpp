#pragma once

#include <array>
#include <type_traits>

#include "type_list.hpp"
#include "edge_traits.hpp"

namespace ugraph::detail {

    template<std::size_t Vid, std::size_t Port>
    struct producer_tag {
        static constexpr std::size_t vid = Vid;
        static constexpr std::size_t port = Port;
    };

    template<typename Topology, typename... edges_t>
    class data_coloring {
        using topology_t = Topology;

        template<std::size_t _vid, std::size_t _port>
        using producer_tag = producer_tag<_vid, _port>;
        template<typename List, typename Tag> struct append_unique;
        template<typename Tag, typename... Ts>
        struct append_unique<detail::type_list<Ts...>, Tag> {
            static constexpr bool exists = ((Tag::vid == Ts::vid && Tag::port == Ts::port) || ... || false);
            using type = std::conditional_t<exists, detail::type_list<Ts...>, detail::type_list<Ts..., Tag>>;
        };
        template<typename List, typename Edge> struct add_edge_prod {
            using tag = producer_tag<edge_traits<Edge>::src_id, edge_traits<Edge>::src_port_index>;
            using type = typename append_unique<List, tag>::type;
        };
        template<typename List, typename... Es> struct fold_prod;
        template<typename List> struct fold_prod<List> { using type = List; };
        template<typename List, typename E, typename... R>
        struct fold_prod<List, E, R...> { using type = typename fold_prod<typename add_edge_prod<List, E>::type, R...>::type; };
        using producer_list = typename fold_prod<detail::type_list<>, edges_t...>::type;

        static constexpr std::size_t producer_count = detail::type_list_size<producer_list>::value;
        static constexpr std::size_t npos = static_cast<std::size_t>(-1);
        static constexpr std::size_t arr_size = producer_count == 0 ? 1 : producer_count;

        static constexpr std::size_t id_to_pos(std::size_t id) {
            auto ids = topology_t::ids();
            for (std::size_t i = 0; i < topology_t::size(); ++i) if (ids[i] == id) return i;
            return npos;
        }

        // Producer (vid, port) keys flattened into arrays once, so lookups become a
        // constexpr scan instead of a recursive class template per (VID, PORT) query.
        template<typename List>
        struct producer_keys;

        template<typename... Ps>
        struct producer_keys<detail::type_list<Ps...>> {
            static constexpr std::array<std::size_t, arr_size> vids() { return { Ps::vid... }; }
            static constexpr std::array<std::size_t, arr_size> ports() { return { Ps::port... }; }
        };

        static constexpr auto prod_vids = producer_keys<producer_list>::vids();
        static constexpr auto prod_ports = producer_keys<producer_list>::ports();

        static constexpr std::size_t find_prod_index(std::size_t vid, std::size_t port) {
            for (std::size_t i = 0; i < producer_count; ++i) {
                if (prod_vids[i] == vid && prod_ports[i] == port) {
                    return i;
                }
            }
            return npos;
        }

        struct lifetimes_t {
            std::array<std::size_t, producer_count == 0 ? 1 : producer_count> start {}, end {};
        };

        template<std::size_t... I>
        static constexpr void init_lifetimes_indices(lifetimes_t& l, std::index_sequence<I...>) {
            ((l.start[I] = id_to_pos(detail::type_list_at<I, producer_list>::type::vid), l.end[I] = l.start[I]), ...);
        }

        static constexpr lifetimes_t build_lifetimes() {
            lifetimes_t lt {};
            if constexpr (producer_count > 0) {
                init_lifetimes_indices(lt, std::make_index_sequence<producer_count>{});

                ([&] () {
                    using ET = edge_traits<edges_t>;
                    const std::size_t idx = find_prod_index(ET::src_id, ET::src_port_index);
                    const std::size_t dpos = id_to_pos(ET::dst_id);
                    if (dpos > lt.end[idx]) {
                        lt.end[idx] = dpos;
                    }
                    }(), ...);
            }
            return lt;
        }

        static constexpr lifetimes_t lifetimes = build_lifetimes();

        struct assignment_t {
            std::array<std::size_t, producer_count == 0 ? 1 : producer_count> buf {}; std::size_t count {};
        };

        static constexpr assignment_t build_assignment() {
            assignment_t a {};
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
                std::size_t buffers = 0;
                for (std::size_t k = 0; k < producer_count; ++k) {
                    auto p = order[k];
                    auto s = lifetimes.start[p];
                    auto e = lifetimes.end[p];
                    std::size_t reuse = buffers;
                    for (std::size_t b = 0; b < buffers; ++b) {
                        if (buffer_end[b] < s) {
                            reuse = b;
                            break;
                        }
                    }
                    if (reuse == buffers) {
                        buffer_end[buffers] = e;
                        a.buf[p] = buffers;
                        ++buffers;
                    }
                    else {
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

        // Edge endpoints flattened once; the per-(VID, PORT) search becomes a constexpr scan
        // rather than a recursive class template chain instantiated per query.
        static constexpr std::size_t edge_count = sizeof...(edges_t);
        static constexpr std::size_t edge_arr_size = edge_count == 0 ? 1 : edge_count;

        static constexpr std::array<std::size_t, edge_arr_size> edge_src_ids { edge_traits<edges_t>::src_id... };
        static constexpr std::array<std::size_t, edge_arr_size> edge_src_ports { edge_traits<edges_t>::src_port_index... };
        static constexpr std::array<std::size_t, edge_arr_size> edge_dst_ids { edge_traits<edges_t>::dst_id... };
        static constexpr std::array<std::size_t, edge_arr_size> edge_dst_ports { edge_traits<edges_t>::dst_port_index... };

        struct src_ref { std::size_t vid, port; };

        static constexpr src_ref find_input_source(std::size_t dvid, std::size_t dport) {
            for (std::size_t i = 0; i < edge_count; ++i) {
                if (edge_dst_ids[i] == dvid && edge_dst_ports[i] == dport) {
                    return { edge_src_ids[i], edge_src_ports[i] };
                }
            }
            return { npos, npos };
        }

    public:
        static constexpr std::size_t data_count() { return assignment.count; }

        // Non-template accessors: one instantiation for the whole coloring instead of one per
        // queried (vertex, port) pair. The template forms below remain for API compatibility.
        static constexpr std::size_t output_data_index(std::size_t vid, std::size_t port) {
            const std::size_t pidx = find_prod_index(vid, port);
            return pidx == npos ? npos : assignment.buf[pidx];
        }

        static constexpr std::size_t input_data_index(std::size_t vid, std::size_t port) {
            const src_ref s = find_input_source(vid, port);
            return s.vid == npos ? npos : output_data_index(s.vid, s.port);
        }

        template<std::size_t VID, std::size_t PORT>
        static constexpr std::size_t output_data_index() {
            constexpr std::size_t r = output_data_index(VID, PORT);
            static_assert(r != npos, "output_data_index<VID, PORT>(): no producer found — vertex VID has no outgoing edge from output port PORT in this graph");
            return r;
        }

        template<std::size_t VID, std::size_t PORT>
        static constexpr std::size_t input_data_index() {
            constexpr std::size_t r = input_data_index(VID, PORT);
            static_assert(r != npos, "input_data_index<VID, PORT>(): no edge found driving input port PORT on vertex VID — add an edge connecting to this port");
            return r;
        }
    };

    template<typename Topology, typename List>
    struct coloring_from_list;

    template<typename Topology, typename... Es>
    struct coloring_from_list<Topology, detail::type_list<Es...>> {
        using type = data_coloring<Topology, Es...>;
    };

    struct empty_coloring {
        static constexpr std::size_t data_count() { return 0; }
        static constexpr std::size_t input_data_index(std::size_t, std::size_t) { return static_cast<std::size_t>(-1); }
        static constexpr std::size_t output_data_index(std::size_t, std::size_t) { return static_cast<std::size_t>(-1); }
        template<std::size_t, std::size_t>
        static constexpr std::size_t input_data_index() { return static_cast<std::size_t>(-1); }
        template<std::size_t, std::size_t>
        static constexpr std::size_t output_data_index() { return static_cast<std::size_t>(-1); }
    };

    template<typename Topology, typename List>
    struct coloring_or_empty { using type = typename coloring_from_list<Topology, List>::type; };

    template<typename Topology>
    struct coloring_or_empty<Topology, detail::type_list<>> { using type = empty_coloring; };

} // namespace ugraph::detail
