/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
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

#include <array>
#include <tuple>
#include <type_traits>
#include <utility>

#include "manifest.hpp"
#include "node_tag.hpp"
#include "link.hpp"
#include "topology.hpp"
#include "edge_traits.hpp"
#include "type_list.hpp"
#include "graph_printer.hpp"

namespace ugraph {

    template<typename... edges_t>
    class Graph;

    template<typename ManifestT>
    struct NodeContext {

        template<std::size_t... I>
        static constexpr auto make_input_ptrs_tuple_t(std::index_sequence<I...>) ->
            std::tuple<std::array<typename ManifestT::template type_at<I>*, ManifestT::template input_count<typename ManifestT::template type_at<I>>() >...>;

        template<std::size_t... I>
        static constexpr auto make_output_ptrs_tuple_t(std::index_sequence<I...>) ->
            std::tuple<std::array<typename ManifestT::template type_at<I>*, ManifestT::template output_count<typename ManifestT::template type_at<I>>() >...>;

        using input_ptrs_tuple_t = decltype(make_input_ptrs_tuple_t(std::make_index_sequence<ManifestT::type_count>{}));
        using output_ptrs_tuple_t = decltype(make_output_ptrs_tuple_t(std::make_index_sequence<ManifestT::type_count>{}));

        constexpr NodeContext() = default;

        constexpr NodeContext(
            const input_ptrs_tuple_t& input_ptrs,
            const output_ptrs_tuple_t& output_ptrs
        ) : mInputPtrsTuple(input_ptrs),
            mOutputPtrsTuple(output_ptrs) {}

        template<typename T>
        constexpr const T& input(std::size_t port = 0) const {
            static_assert(ManifestT::template contains<T>, "Type not declared in Manifest");
            return *input_ptrs<T>()[port];
        }

        template<typename T>
        constexpr T& output(std::size_t port = 0) {
            static_assert(ManifestT::template contains<T>, "Type not declared in Manifest");
            return *output_ptrs<T>()[port];
        }

        template<typename T>
        constexpr auto inputs() const {
            static_assert(ManifestT::template contains<T>, "Type not declared in Manifest");
            constexpr std::size_t N = ManifestT::template input_count<T>();
            static_assert(N > 0, "No input ports for this type");
            using view_t = const_ptr_ref_view<T, N>;
            return view_t(std::addressof(std::get<ManifestT::template index<T>()>(mInputPtrsTuple)));
        }

        template<typename T>
        constexpr auto outputs() {
            static_assert(ManifestT::template contains<T>, "Type not declared in Manifest");
            constexpr std::size_t N = ManifestT::template output_count<T>();
            static_assert(N > 0, "No output ports for this type");
            using view_t = ptr_ref_view<T, N>;
            return view_t(std::addressof(std::get<ManifestT::template index<T>()>(mOutputPtrsTuple)));
        }

        template<typename T, std::size_t I = 0>
        constexpr bool has_input() const {
            static_assert(ManifestT::template contains<T>, "Type not declared in Manifest");
            return input_ptrs<T>()[I] != nullptr;
        }

        template<typename T, std::size_t I = 0>
        constexpr bool has_output() const {
            static_assert(ManifestT::template contains<T>, "Type not declared in Manifest");
            return output_ptrs<T>()[I] != nullptr;
        }

        template<typename T>
        constexpr const auto& input_ptrs() const {
            static_assert(ManifestT::template contains<T>, "Type not declared in Manifest");
            return std::get<ManifestT::template index<T>()>(mInputPtrsTuple);
        }

        template<typename T>
        constexpr auto& output_ptrs() {
            static_assert(ManifestT::template contains<T>, "Type not declared in Manifest");
            return std::get<ManifestT::template index<T>()>(mOutputPtrsTuple);
        }

        template<typename T>
        constexpr const auto& output_ptrs() const {
            static_assert(ManifestT::template contains<T>, "Type not declared in Manifest");
            return std::get<ManifestT::template index<T>()>(mOutputPtrsTuple);
        }

    private:
        template<typename T, std::size_t N>
        struct ptr_ref_view {
            using array_t = std::array<T*, N>;
            array_t* arr = nullptr;
            struct iterator {
                array_t* arr = nullptr;
                std::size_t i = 0;
                constexpr T& operator*() const { return *((*arr)[i]); }
                constexpr iterator& operator++() { ++i; return *this; }
                constexpr bool operator!=(const iterator& o) const { return i != o.i; }
            };
            constexpr ptr_ref_view() = default;
            constexpr ptr_ref_view(array_t* a) : arr(a) {}
            constexpr iterator begin() const { return { arr, 0 }; }
            constexpr iterator end() const { return { arr, N }; }
            constexpr T& operator[](std::size_t i) const { return *((*arr)[i]); }
        };

        template<typename T, std::size_t N>
        struct const_ptr_ref_view {
            using array_t = std::array<T*, N>;
            const array_t* arr = nullptr;
            struct iterator {
                const array_t* arr = nullptr;
                std::size_t i = 0;
                constexpr const T& operator*() const { return *((*arr)[i]); }
                constexpr iterator& operator++() { ++i; return *this; }
                constexpr bool operator!=(const iterator& o) const { return i != o.i; }
            };
            constexpr const_ptr_ref_view() = default;
            constexpr const_ptr_ref_view(const array_t* a) : arr(a) {}
            constexpr iterator begin() const { return { arr, 0 }; }
            constexpr iterator end() const { return { arr, N }; }
            constexpr const T& operator[](std::size_t i) const { return *((*arr)[i]); }
        };

        input_ptrs_tuple_t mInputPtrsTuple {};
        output_ptrs_tuple_t mOutputPtrsTuple {};
    };


    namespace detail {
        template<typename Edge>
        struct edge_data_type {
            using type = typename Edge::first_type::data_type;
        };

        template<typename T, typename Edge>
        struct edge_is_type : std::false_type {};

        template<typename T, typename S, typename D>
        struct edge_is_type<T, Link<S, D>> : std::bool_constant<std::is_same_v<typename S::data_type, T>> {};

        template<typename List, typename T>
        struct type_list_append_unique;

        template<typename T, typename... Ts>
        struct type_list_append_unique<detail::type_list<Ts...>, T> {
            static constexpr bool exists = (std::is_same_v<T, Ts> || ... || false);
            using type = std::conditional_t<exists, detail::type_list<Ts...>, detail::type_list<Ts..., T>>;
        };

        template<typename List, typename... Ts>
        struct fold_append_unique;

        template<typename List>
        struct fold_append_unique<List> { using type = List; };

        template<typename List, typename T, typename... Rest>
        struct fold_append_unique<List, T, Rest...> {
            using next = typename type_list_append_unique<List, T>::type;
            using type = typename fold_append_unique<next, Rest...>::type;
        };



        template<typename List, typename TL>
        struct append_type_list_unique;
        template<typename List, typename... Ts>
        struct append_type_list_unique<List, detail::type_list<Ts...>> { using type = typename fold_append_unique<List, Ts...>::type; };

        template<typename TL>
        struct collect_specs_from_typelist;
        template<>
        struct collect_specs_from_typelist<detail::type_list<>> { using type = detail::type_list<>; };



        template<typename V, typename... Vs>
        struct collect_specs_from_typelist<detail::type_list<V, Vs...>> {
            using head_specs = typename V::module_type::Manifest::specs_list;
            using tail = typename collect_specs_from_typelist<detail::type_list<Vs...>>::type;
            using type = typename append_type_list_unique<head_specs, tail>::type;
        };

        template<typename List>
        struct manifest_from_list;

        template<typename... Ts>
        struct manifest_from_list<detail::type_list<Ts...>> { using type = Manifest<Ts...>; };

        template<typename T, typename List>
        struct filter_edges;

        template<typename T>
        struct filter_edges<T, detail::type_list<>> {
            using type = detail::type_list<>;
        };

        template<typename T, typename E, typename... Rest>
        struct filter_edges<T, detail::type_list<E, Rest...>> {
            using tail = typename filter_edges<T, detail::type_list<Rest...>>::type;
            using type = std::conditional_t<
                edge_is_type<T, E>::value,
                typename detail::type_list_prepend<E, tail>::type,
                tail
            >;
        };

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
            static constexpr std::size_t input_count() { return 0; }
            static constexpr std::size_t output_count() { return 0; }
            template<std::size_t, std::size_t>
            static constexpr std::size_t input_data_index() { return static_cast<std::size_t>(-1); }
            template<std::size_t, std::size_t>
            static constexpr std::size_t output_data_index() { return static_cast<std::size_t>(-1); }
        };

        template<typename Topology, typename List>
        struct coloring_or_empty { using type = typename coloring_from_list<Topology, List>::type; };

        template<typename Topology>
        struct coloring_or_empty<Topology, detail::type_list<>> { using type = empty_coloring; };

        template<typename... edges_t>
        struct data_graph_traits {
            using topology_t = Topology<edges_t...>;

            template<std::size_t I>
            using node_type_at = typename topology_t::template find_type_by_id<topology_t::template id_at<I>()>::type;

            using graph_types_list = typename collect_specs_from_typelist<typename topology_t::vertex_types_list_public>::type;
            using manifest_t = typename manifest_from_list<graph_types_list>::type;
            static constexpr std::size_t invalid_index = static_cast<std::size_t>(-1);

            template<std::size_t... I>
            static constexpr auto make_modules_tuple_t(std::index_sequence<I...>) ->
                std::tuple<typename topology_t::template find_type_by_id<topology_t::template id_at<I>()>::type::module_type*...>;

            using modules_tuple_t = decltype(make_modules_tuple_t(std::make_index_sequence<topology_t::size()>{}));

            template<std::size_t id, typename Edge>
            static constexpr auto try_edge_module(const Edge& e) {
                using S = typename ugraph::edge_traits<Edge>::src_vertex_t;
                if constexpr (S::id() == id) {
                    return &e.first.mNode.module();
                }
                else {
                    using D = typename ugraph::edge_traits<Edge>::dst_vertex_t;
                    if constexpr (D::id() == id) {
                        return &e.second.mNode.module();
                    }
                    else {
                        return (typename topology_t::template find_type_by_id<id>::type::module_type*)nullptr;
                    }
                }
            }

            template<std::size_t id>
            static constexpr auto get_module_ptr(const edges_t&... es) {
                typename topology_t::template find_type_by_id<id>::type::module_type* r = nullptr;
                ((r = r ? r : try_edge_module<id>(es)), ...);
                return r;
            }

            template<std::size_t... I>
            static constexpr modules_tuple_t build_modules(std::index_sequence<I...>, const edges_t&... es) {
                return { get_module_ptr<topology_t::template id_at<I>()>(es...)... };
            }

            template<typename T>
            using edge_list_for_t = typename detail::filter_edges<T, detail::type_list<edges_t...>>::type;

            template<typename T>
            using coloring_t = typename detail::coloring_or_empty<topology_t, edge_list_for_t<T>>::type;

            template<typename T, std::size_t VID, std::size_t PORT, typename... Es>
            struct has_input_edge_impl;

            template<typename T, std::size_t VID, std::size_t PORT>
            struct has_input_edge_impl<T, VID, PORT> : std::false_type {};

            template<typename T, std::size_t VID, std::size_t PORT, typename E0, typename... Rest>
            struct has_input_edge_impl<T, VID, PORT, E0, Rest...> {
                using tr = ugraph::edge_traits<E0>;
                static constexpr bool match = std::is_same_v<T, typename detail::edge_data_type<E0>::type> &&
                    (tr::dst_id == VID) && (tr::dst_port_index == PORT);
                static constexpr bool value = match ? true : has_input_edge_impl<T, VID, PORT, Rest...>::value;
            };

            template<typename T, std::size_t VID, std::size_t PORT>
            static constexpr bool has_input_edge() {
                return has_input_edge_impl<T, VID, PORT, edges_t...>::value;
            }

            template<typename T, std::size_t VID, std::size_t PORT, typename... Es>
            struct has_output_edge_impl;

            template<typename T, std::size_t VID, std::size_t PORT>
            struct has_output_edge_impl<T, VID, PORT> : std::false_type {};

            template<typename T, std::size_t VID, std::size_t PORT, typename E0, typename... Rest>
            struct has_output_edge_impl<T, VID, PORT, E0, Rest...> {
                using tr = ugraph::edge_traits<E0>;
                static constexpr bool match = std::is_same_v<T, typename detail::edge_data_type<E0>::type> &&
                    (tr::src_id == VID) && (tr::src_port_index == PORT);
                static constexpr bool value = match ? true : has_output_edge_impl<T, VID, PORT, Rest...>::value;
            };

            template<typename T, std::size_t VID, std::size_t PORT>
            static constexpr bool has_output_edge() {
                return has_output_edge_impl<T, VID, PORT, edges_t...>::value;
            }

            template<typename T, std::size_t NodeIndex, std::size_t PortIndex>
            static constexpr std::size_t input_index_for() {
                constexpr std::size_t vid = topology_t::template id_at<NodeIndex>();
                if constexpr (has_input_edge<T, vid, PortIndex>()) {
                    return coloring_t<T>::template input_data_index<vid, PortIndex>();
                }
                else {
                    return invalid_index;
                }
            }

            template<typename T, std::size_t NodeIndex, std::size_t PortIndex>
            static constexpr std::size_t output_index_for() {
                constexpr std::size_t vid = topology_t::template id_at<NodeIndex>();
                if constexpr (has_output_edge<T, vid, PortIndex>()) {
                    return coloring_t<T>::template output_data_index<vid, PortIndex>();
                }
                else {
                    return invalid_index;
                }
            }


        };
    } // namespace detail

    template<typename... edges_t>
    class Graph {
        using traits = detail::data_graph_traits<edges_t...>;
        using topology_t = typename traits::topology_t;
        static_assert(!topology_t::is_cyclic(), "Cycle detected in graph definition");

        template<std::size_t I>
        using node_type_at = typename traits::template node_type_at<I>;

        using manifest_t = typename traits::manifest_t;
        using modules_tuple_impl_t = typename traits::modules_tuple_t;
        template<std::size_t... I>
        static constexpr auto make_contexts_tuple_t(std::index_sequence<I...>) ->
            std::tuple<NodeContext<typename node_type_at<I>::module_type::Manifest>...>;
        using contexts_tuple_t = decltype(make_contexts_tuple_t(std::make_index_sequence<topology_t::size()>{}));

        modules_tuple_impl_t mModules;
        contexts_tuple_t mContexts;
        template<std::size_t... I>
        static constexpr auto make_data_storage_tuple_t(std::index_sequence<I...>) ->
            std::tuple<std::array<typename manifest_t::template type_at<I>,
            traits::template coloring_t<typename manifest_t::template type_at<I>>::data_count() >...>;

        using data_storage_tuple_t = decltype(make_data_storage_tuple_t(std::make_index_sequence<manifest_t::type_count>{}));
        data_storage_tuple_t mDataStorage {};

    public:
        using topology_type = topology_t;

        constexpr Graph(const edges_t&... es) :
            mModules(traits::build_modules(std::make_index_sequence<topology_t::size()>{}, es...)) {
            init_contexts(std::make_index_sequence<topology_t::size()>{});
        }

        template<typename F>
        constexpr void for_each(F&& f) {
            for_each_impl(std::forward<F>(f), std::make_index_sequence<topology_t::size()>{});
        }

        template<typename T>
        constexpr T* data() {
            static_assert(manifest_t::template contains<T>, "Type not declared in Manifest");
            if constexpr (data_count<T>() == 0) {
                return nullptr;
            }
            else {
                constexpr std::size_t idx = manifest_t::template index<T>();
                return std::get<idx>(mDataStorage).data();
            }
        }

        template<typename T>
        constexpr const T* data() const {
            static_assert(manifest_t::template contains<T>, "Type not declared in Manifest");
            if constexpr (data_count<T>() == 0) {
                return nullptr;
            }
            else {
                constexpr std::size_t idx = manifest_t::template index<T>();
                return std::get<idx>(mDataStorage).data();
            }
        }

        template<typename T>
        constexpr T& data_at(std::size_t i) {
            auto ptr = data<T>();
            return ptr[i];
        }

        template<typename T>
        constexpr const T& data_at(std::size_t i) const {
            auto ptr = data<T>();
            return ptr[i];
        }

        template<typename T>
        static constexpr std::size_t data_count() {
            return traits::template coloring_t<T>::data_count();
        }

        template<typename stream_t>
        void print(stream_t& stream, const std::string_view& inGraphName = "") const {
            ugraph::print_graph<topology_t>(stream, inGraphName);
        }

        template<typename stream_t>
        void print_pipeline(stream_t& stream, const std::string_view& inGraphName = "") const {
            ugraph::print_pipeline<topology_t>(stream, inGraphName);
        }

    private:
        template<typename NodeManifest, typename T, std::size_t NodeIndex, std::size_t... P>
        constexpr std::array<T*, NodeManifest::template input_count<T>()>
            build_input_ptrs_array_impl(std::index_sequence<P...>) {
            if constexpr (NodeManifest::template strict_connection<T>()) {
                constexpr std::size_t vid = topology_t::template id_at<NodeIndex>();
                static_assert((traits::template has_input_edge<T, vid, P>() && ...),
                    "Strict input connection missing in graph");
            }
            if constexpr (data_count<T>() == 0) {
                return { ((void) P, nullptr)... };
            }
            else {
                auto* base = data<T>();
                return { ((traits::template input_index_for<T, NodeIndex, P>() == traits::invalid_index)
                    ? nullptr
                    : &base[traits::template input_index_for<T, NodeIndex, P>()])... };
            }
        }

        template<typename NodeManifest, typename T, std::size_t NodeIndex>
        constexpr std::array<T*, NodeManifest::template input_count<T>()>
            build_input_ptrs_array() {
            return build_input_ptrs_array_impl<NodeManifest, T, NodeIndex>(
                std::make_index_sequence<NodeManifest::template input_count<T>()>{});
        }

        template<typename NodeManifest, typename T, std::size_t NodeIndex, std::size_t... P>
        constexpr std::array<T*, NodeManifest::template output_count<T>()>
            build_output_ptrs_array_impl(std::index_sequence<P...>) {
            if constexpr (NodeManifest::template strict_connection<T>()) {
                constexpr std::size_t vid = topology_t::template id_at<NodeIndex>();
                static_assert((traits::template has_output_edge<T, vid, P>() && ...),
                    "Strict output connection missing in graph");
            }
            if constexpr (data_count<T>() == 0) {
                return { ((void) P, nullptr)... };
            }
            else {
                auto* base = data<T>();
                return { ((traits::template output_index_for<T, NodeIndex, P>() == traits::invalid_index)
                    ? nullptr
                    : &base[traits::template output_index_for<T, NodeIndex, P>()])... };
            }
        }

        template<typename NodeManifest, typename T, std::size_t NodeIndex>
        constexpr std::array<T*, NodeManifest::template output_count<T>()>
            build_output_ptrs_array() {
            return build_output_ptrs_array_impl<NodeManifest, T, NodeIndex>(
                std::make_index_sequence<NodeManifest::template output_count<T>()>{});
        }

        template<typename NodeManifest, std::size_t NodeIndex, std::size_t... Tidx>
        constexpr typename NodeContext<NodeManifest>::input_ptrs_tuple_t
            build_input_ptrs_tuple(std::index_sequence<Tidx...>) {
            return { build_input_ptrs_array<NodeManifest, typename NodeManifest::template type_at<Tidx>, NodeIndex>()... };
        }

        template<typename NodeManifest, std::size_t NodeIndex, std::size_t... Tidx>
        constexpr typename NodeContext<NodeManifest>::output_ptrs_tuple_t
            build_output_ptrs_tuple(std::index_sequence<Tidx...>) {
            return { build_output_ptrs_array<NodeManifest, typename NodeManifest::template type_at<Tidx>, NodeIndex>()... };
        }

        template<std::size_t I, typename F>
        constexpr void for_each_at(F&& f) {
            f(*std::get<I>(mModules), std::get<I>(mContexts));
        }

        template<typename F, std::size_t... I>
        constexpr void for_each_impl(F&& f, std::index_sequence<I...>) {
            (for_each_at<I>(std::forward<F>(f)), ...);
        }

        template<std::size_t I>
        constexpr void init_context_at() {
            using node_type = node_type_at<I>;
            using node_manifest = typename node_type::module_type::Manifest;
            auto& ctx = std::get<I>(mContexts);
            auto input_ptrs = build_input_ptrs_tuple<node_manifest, I>(std::make_index_sequence<node_manifest::type_count>{});
            auto output_ptrs = build_output_ptrs_tuple<node_manifest, I>(std::make_index_sequence<node_manifest::type_count>{});
            ctx = NodeContext<node_manifest>(input_ptrs, output_ptrs);
        }

        template<std::size_t... I>
        constexpr void init_contexts(std::index_sequence<I...>) {
            (init_context_at<I>(), ...);
        }


    };

    template<typename E0, typename... ERest>
    Graph(E0 const&, ERest const&...) -> Graph<std::decay_t<E0>, std::decay_t<ERest>...>;

} // namespace ugraph
