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

        static constexpr std::size_t id_to_pos(std::size_t id) {
            auto ids = topology_t::ids();
            for (std::size_t i = 0; i < topology_t::size(); ++i) if (ids[i] == id) return i;
            return static_cast<std::size_t>(-1);
        }

        template<std::size_t VID, std::size_t PORT, std::size_t I>
        struct find_prod_index_impl {
            using PT = typename detail::type_list_at<I, producer_list>::type;
            static constexpr std::size_t value = (PT::vid == VID && PT::port == PORT) ? I : find_prod_index_impl<VID, PORT, I + 1>::value;
        };
        template<std::size_t VID, std::size_t PORT>
        struct find_prod_index_impl<VID, PORT, producer_count> { static constexpr std::size_t value = static_cast<std::size_t>(-1); };

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
                    constexpr std::size_t idx = find_prod_index_impl<ET::src_id, ET::src_port_index, 0>::value;
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

        template<std::size_t DVID, std::size_t DPORT, typename... Es>
        struct find_input_edge_impl;

        template<std::size_t DVID, std::size_t DPORT>
        struct find_input_edge_impl<DVID, DPORT> {
            static constexpr std::size_t src_vid = static_cast<std::size_t>(-1);
            static constexpr std::size_t src_port = static_cast<std::size_t>(-1);
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

        template<std::size_t VID, std::size_t PORT>
        static constexpr std::size_t data_index_for_output() {
            constexpr std::size_t pidx = find_prod_index_impl<VID, PORT, 0>::value;
            static_assert(pidx != static_cast<std::size_t>(-1), "(vertex id, output port) not a producer in this graph");
            return assignment.buf[pidx];
        }

        template<std::size_t VID, std::size_t PORT>
        static constexpr std::size_t data_index_for_input() {
            constexpr std::size_t src_vid = find_input_edge<VID, PORT>::src_vid;
            constexpr std::size_t src_port = find_input_edge<VID, PORT>::src_port;
            static_assert(src_vid != static_cast<std::size_t>(-1), "No edge found feeding (vertex id, input port)");
            return data_index_for_output<src_vid, src_port>();
        }

    public:
        static constexpr std::size_t data_count() { return assignment.count; }

        template<std::size_t VID, std::size_t PORT>
        static constexpr std::size_t output_data_index() { return data_index_for_output<VID, PORT>(); }

        template<std::size_t VID, std::size_t PORT>
        static constexpr std::size_t input_data_index() { return data_index_for_input<VID, PORT>(); }
    };

    template<typename Topology, typename List>
    struct coloring_from_list;

    template<typename Topology, typename... Es>
    struct coloring_from_list<Topology, detail::type_list<Es...>> {
        using type = data_coloring<Topology, Es...>;
    };

    struct empty_coloring {
        static constexpr std::size_t data_count() { return 0; }
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
