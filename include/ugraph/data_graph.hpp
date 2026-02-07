#pragma once

#include <array>
#include <tuple>
#include <type_traits>
#include <utility>
#include <functional>
#include <memory>
#include <algorithm>

#include "manifest.hpp"
#include "node_tag.hpp"
#include "topology.hpp"
#include "edge_traits.hpp"
#include "type_list.hpp"

namespace ugraph {

    template<typename ManifestT>
    struct NodeContext {

        using index_fn_t = std::size_t(*)(std::size_t, std::size_t);

        constexpr NodeContext() = default;

        constexpr NodeContext(void** data_ptrs, index_fn_t* input_index, index_fn_t* output_index,
            const std::size_t* type_map, std::size_t node_index)
            : mNodeIndex(node_index),
            mDataPtrs(data_ptrs),
            mInputIndex(input_index),
            mOutputIndex(output_index),
            mTypeMap(type_map),
            mInputPtrsTuple(),
            mOutputPtrsTuple() {
            refresh_all_pointers();
        }

        template<typename T>
        constexpr const T& input(std::size_t port = 0) const {
            static_assert(ManifestT::template contains<T>, "Type not declared in Manifest");
            const T* ptr = input_ptrs<T>()[port];
            return *ptr;
        }

        template<typename T>
        constexpr T& output(std::size_t port = 0) {
            static_assert(ManifestT::template contains<T>, "Type not declared in Manifest");
            T* ptr = output_ptrs<T>()[port];
            return *ptr;
        }

        template<typename T, std::size_t I>
        constexpr bool has_input() const {
            return input_ptrs<T>()[I] != nullptr;
        }

        template<typename T, std::size_t I>
        constexpr bool has_output() const {
            return output_ptrs<T>()[I] != nullptr;
        }

        template<typename T, std::size_t... I>
        constexpr auto inputs_impl(std::index_sequence<I...>) {
            static_assert(ManifestT::template contains<T>, "Type not declared in Manifest");
            auto& ptrs = input_ptrs<T>();
            return std::array<std::reference_wrapper<const T>, sizeof...(I)>{ std::cref(static_cast<const T&>(*ptrs[I]))... };
        }

        template<typename T, std::size_t... I>
        constexpr auto inputs_impl(std::index_sequence<I...>) const {
            static_assert(ManifestT::template contains<T>, "Type not declared in Manifest");
            auto& ptrs = input_ptrs<T>();
            return std::array<std::reference_wrapper<const T>, sizeof...(I)>{ std::cref(static_cast<const T&>(*ptrs[I]))... };
        }

        template<typename T>
        constexpr auto inputs() {
            constexpr std::size_t N = ManifestT::template input_count<T>();
            static_assert(N > 0, "No input ports for this type");
            return inputs_impl<T>(std::make_index_sequence<N>{});
        }

        template<typename T>
        constexpr auto inputs() const {
            constexpr std::size_t N = ManifestT::template input_count<T>();
            static_assert(N > 0, "No input ports for this type");
            return inputs_impl<T>(std::make_index_sequence<N>{});
        }

        template<typename T, std::size_t... I>
        constexpr auto outputs_impl(std::index_sequence<I...>) {
            static_assert(ManifestT::template contains<T>, "Type not declared in Manifest");
            auto& ptrs = output_ptrs<T>();
            return std::array<std::reference_wrapper<T>, sizeof...(I)>{ std::ref(*ptrs[I])... };
        }

        template<typename T, std::size_t... I>
        constexpr auto outputs_impl(std::index_sequence<I...>) const {
            static_assert(ManifestT::template contains<T>, "Type not declared in Manifest");
            auto& ptrs = output_ptrs<T>();
            return std::array<std::reference_wrapper<const T>, sizeof...(I)>{ std::cref(static_cast<const T&>(*ptrs[I]))... };
        }

        template<typename T>
        constexpr auto outputs() {
            constexpr std::size_t N = ManifestT::template output_count<T>();
            static_assert(N > 0, "No output ports for this type");
            return outputs_impl<T>(std::make_index_sequence<N>{});
        }

        template<typename T>
        constexpr auto& input_ptrs() {
            static_assert(ManifestT::template contains<T>, "Type not declared in Manifest");
            return std::get<ManifestT::template index<T>()>(mInputPtrsTuple);
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

        // Public refresh wrapper so external code can request all pointers be refreshed
        constexpr void refresh() {
            refresh_data_ptrs_tuple();
            refresh_all_pointers();
        }



    private:

        static constexpr std::size_t invalid_index = static_cast<std::size_t>(-1);

        template<typename T>
        struct PortCache {
            static constexpr std::size_t in_count = ManifestT::template input_count<T>();
            static constexpr std::size_t out_count = ManifestT::template output_count<T>();
            std::array<T*, in_count> inputs {};
            std::array<T*, out_count> outputs {};
        };

        template<std::size_t... I>
        static constexpr auto make_cache_tuple_t(std::index_sequence<I...>) ->
            std::tuple<PortCache<typename ManifestT::template type_at<I>>...>;

        using cache_tuple_t = decltype(make_cache_tuple_t(std::make_index_sequence<ManifestT::type_count>{}));

        template<std::size_t... I>
        static constexpr auto make_input_ptrs_tuple_t(std::index_sequence<I...>) ->
            std::tuple<std::array<typename ManifestT::template type_at<I>*, ManifestT::template input_count<typename ManifestT::template type_at<I>>() >...>;

        template<std::size_t... I>
        static constexpr auto make_output_ptrs_tuple_t(std::index_sequence<I...>) ->
            std::tuple<std::array<typename ManifestT::template type_at<I>*, ManifestT::template output_count<typename ManifestT::template type_at<I>>() >...>;

        using input_ptrs_tuple_t = decltype(make_input_ptrs_tuple_t(std::make_index_sequence<ManifestT::type_count>{}));
        using output_ptrs_tuple_t = decltype(make_output_ptrs_tuple_t(std::make_index_sequence<ManifestT::type_count>{}));

        template<typename T>
        constexpr std::size_t graph_type_index() const {
            const std::size_t local_idx = ManifestT::template index<T>();
            static_assert(local_idx < ManifestT::type_count, "Type index out of range");
            return mTypeMap[local_idx];
        }

        template<typename T>
        constexpr PortCache<T>& cache_for() {
            return std::get<ManifestT::template index<T>()>(mPortCache);
        }

        template<typename T>
        constexpr const PortCache<T>& cache_for() const {
            return std::get<ManifestT::template index<T>()>(mPortCache);
        }

        template<typename T>
        constexpr std::size_t input_index(std::size_t port) const {
            const std::size_t graph_idx = graph_type_index<T>();
            auto fn = mInputIndex[graph_idx];
            return fn ? fn(mNodeIndex, port) : invalid_index;
        }

        template<typename T>
        constexpr std::size_t output_index(std::size_t port) const {
            const std::size_t graph_idx = graph_type_index<T>();
            auto fn = mOutputIndex[graph_idx];
            return fn ? fn(mNodeIndex, port) : invalid_index;
        }

        template<typename T>
        constexpr void refresh_pointers_for_type() {
            constexpr std::size_t in_count = ManifestT::template input_count<T>();
            constexpr std::size_t out_count = ManifestT::template output_count<T>();
            refresh_data_ptrs_tuple();
            auto& input_arr = std::get<ManifestT::template index<T>()>(mInputPtrsTuple);
            auto& output_arr = std::get<ManifestT::template index<T>()>(mOutputPtrsTuple);
            if (!mDataPtrs || !mInputIndex || !mOutputIndex || !mTypeMap) {
                for (std::size_t i = 0; i < in_count; ++i) input_arr[i] = nullptr;
                for (std::size_t i = 0; i < out_count; ++i) output_arr[i] = nullptr;
                return;
            }
            constexpr std::size_t local_idx = ManifestT::template index<T>();
            auto* base = std::get<local_idx>(mDataPtrsTuple);
            if (!base) {
                for (std::size_t i = 0; i < in_count; ++i) input_arr[i] = nullptr;
                for (std::size_t i = 0; i < out_count; ++i) output_arr[i] = nullptr;
                return;
            }
            for (std::size_t i = 0; i < in_count; ++i) {
                const std::size_t idx = input_index<T>(i);
                input_arr[i] = (idx == invalid_index) ? nullptr : &base[idx];
            }
            for (std::size_t i = 0; i < out_count; ++i) {
                const std::size_t idx = output_index<T>(i);
                output_arr[i] = (idx == invalid_index) ? nullptr : &base[idx];
            }
        }

        template<std::size_t... I>
        static constexpr auto make_data_ptrs_tuple_t(std::index_sequence<I...>) ->
            std::tuple<typename ManifestT::template type_at<I>*...>;

        using data_ptrs_tuple_t = decltype(make_data_ptrs_tuple_t(std::make_index_sequence<ManifestT::type_count>{}));

        template<std::size_t... I>
        constexpr void refresh_data_ptrs_tuple_impl(std::index_sequence<I...>) {
            if (!mDataPtrs || !mTypeMap) {
                ((std::get<I>(mDataPtrsTuple) = nullptr), ...);
                return;
            }
            ((std::get<I>(mDataPtrsTuple) = static_cast<typename ManifestT::template type_at<I>*>(mDataPtrs[mTypeMap[I]])), ...);
        }

        constexpr void refresh_data_ptrs_tuple() {
            refresh_data_ptrs_tuple_impl(std::make_index_sequence<ManifestT::type_count>{});
        }

        template<std::size_t... I>
        constexpr void refresh_all_pointers_impl(std::index_sequence<I...>) {
            (refresh_pointers_for_type<typename ManifestT::template type_at<I>>(), ...);
        }

        constexpr void refresh_all_pointers() {
            refresh_all_pointers_impl(std::make_index_sequence<ManifestT::type_count>{});
        }

        std::size_t mNodeIndex = 0;
        void** mDataPtrs = nullptr;
        index_fn_t* mInputIndex = nullptr;
        index_fn_t* mOutputIndex = nullptr;
        const std::size_t* mTypeMap = nullptr;
        data_ptrs_tuple_t mDataPtrsTuple {};
        cache_tuple_t mPortCache {};
        input_ptrs_tuple_t mInputPtrsTuple {};
        output_ptrs_tuple_t mOutputPtrsTuple {};

    };


    template<std::size_t Id, typename module_t, typename manifest_t, std::size_t _priority = 0>
    class DataNode {
    public:
        using module_type = module_t;
        static constexpr std::size_t id() { return Id; }
        static constexpr std::size_t priority() { return _priority; }

        constexpr DataNode(module_type& module) : mModule(module) {}

        constexpr module_type& module() { return mModule; }
        constexpr const module_type& module() const { return mModule; }

        template<typename T>
        struct NodeType : NodePortTag<Id, module_type,
            manifest_t::template input_count<T>(),
            manifest_t::template output_count<T>(),
            _priority> {};

        template<typename T, std::size_t Index>
        struct InputPort : PortTag<NodeType<T>, Index> {
            using data_type = T;
            constexpr InputPort(DataNode& n) : mNode(n) {}
            DataNode& mNode;
        };

        template<typename T, std::size_t Index>
        struct OutputPort : PortTag<NodeType<T>, Index> {
            using data_type = T;
            constexpr OutputPort(DataNode& n) : mNode(n) {}
            DataNode& mNode;

            template<typename other_port_t>
            constexpr auto operator >> (const other_port_t& p) const {
                return Link<OutputPort<T, Index>, other_port_t>(*this, p);
            }
        };

        template<typename T, std::size_t I>
        constexpr auto input() {
            static_assert(manifest_t::template contains<T>, "Type not declared in Manifest");
            return InputPort<T, I>(*this);
        }

        template<typename T, std::size_t I>
        constexpr auto output() {
            static_assert(manifest_t::template contains<T>, "Type not declared in Manifest");
            return OutputPort<T, I>(*this);
        }

        template<typename T, std::size_t I>
        constexpr auto input() const {
            static_assert(manifest_t::template contains<T>, "Type not declared in Manifest");
            return InputPort<T, I>(const_cast<DataNode&>(*this));
        }

        template<typename T, std::size_t I>
        constexpr auto output() const {
            static_assert(manifest_t::template contains<T>, "Type not declared in Manifest");
            return OutputPort<T, I>(const_cast<DataNode&>(*this));
        }

    private:
        module_type& mModule;
    };

    template<std::size_t Id, typename M, std::size_t _priority = 0>
    constexpr auto make_data_node(M& module) {
        using module_t = std::remove_cv_t<std::remove_reference_t<M>>;
        using manifest_t = typename module_t::Manifest;
        return DataNode<Id, module_t, manifest_t, _priority>(module);
    }

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

        template<typename Topology, typename... edges_t>
        class data_coloring {
            using topology_t = Topology;

            template<typename E>
            using edge_traits = ugraph::edge_traits<E>;

            template<std::size_t _vid, std::size_t _port>
            using producer_tag = ugraph::producer_tag<_vid, _port>;
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
            static constexpr std::size_t data_instance_count() { return assignment.count; }

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
            static constexpr std::size_t data_instance_count() { return 0; }
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

        template<typename T, std::size_t LinearIndex, std::size_t PortCount>
        struct linear_port_traits {
            static constexpr std::size_t node_index = LinearIndex / PortCount;
            static constexpr std::size_t port_index = LinearIndex % PortCount;
        };

        template<typename... edges_t>
        struct data_graph_traits {
            using topology_t = Topology<edges_t...>;

            template<std::size_t I>
            using node_type_at = typename topology_t::template find_type_by_id<topology_t::template id_at<I>()>::type;

            using first_edge_t = typename std::tuple_element<0, std::tuple<edges_t...>>::type;
            using module_type = typename ugraph::edge_traits<first_edge_t>::src_vertex_t::module_type;
            using graph_types_list = typename collect_specs_from_typelist<typename topology_t::vertex_types_list_public>::type;
            using manifest_t = typename manifest_from_list<graph_types_list>::type;
            using index_fn_t = std::size_t(*)(std::size_t, std::size_t);

            static constexpr std::size_t invalid_index = static_cast<std::size_t>(-1);

            template<typename T, std::size_t... I>
            static constexpr std::size_t graph_input_port_count_impl(std::index_sequence<I...>) {
                const std::size_t vals[] = { (std::is_same_v<typename io_traits<typename detail::type_list_at<I, graph_types_list>::type>::type, T> ?
                    io_traits<typename detail::type_list_at<I, graph_types_list>::type>::input_count : 0)... };
                std::size_t m = 0;
                for (std::size_t i = 0; i < sizeof...(I); ++i) if (vals[i] > m) m = vals[i];
                return m;
            }

            template<typename T>
            static constexpr std::size_t graph_input_port_count() {
                constexpr std::size_t N = detail::type_list_size<graph_types_list>::value;
                if constexpr (N == 0) return 0;
                else return graph_input_port_count_impl<T>(std::make_index_sequence<N>{});
            }

            template<typename T, std::size_t... I>
            static constexpr std::size_t graph_output_port_count_impl(std::index_sequence<I...>) {
                const std::size_t vals[] = { (std::is_same_v<typename io_traits<typename detail::type_list_at<I, graph_types_list>::type>::type, T> ?
                    io_traits<typename detail::type_list_at<I, graph_types_list>::type>::output_count : 0)... };
                std::size_t m = 0;
                for (std::size_t i = 0; i < sizeof...(I); ++i) if (vals[i] > m) m = vals[i];
                return m;
            }

            template<typename T>
            static constexpr std::size_t graph_output_port_count() {
                constexpr std::size_t N = detail::type_list_size<graph_types_list>::value;
                if constexpr (N == 0) return 0;
                else return graph_output_port_count_impl<T>(std::make_index_sequence<N>{});
            }

            template<std::size_t... I>
            static constexpr auto make_modules_tuple_t(std::index_sequence<I...>) ->
                std::tuple<typename topology_t::template find_type_by_id<topology_t::template id_at<I>()>::type::module_type*...>;

            using modules_tuple_t = decltype(make_modules_tuple_t(std::make_index_sequence<topology_t::size()>{}));

            template<std::size_t Id, typename Edge>
            static constexpr auto try_edge_module(const Edge& e) {
                using S = typename ugraph::edge_traits<Edge>::src_vertex_t;
                if constexpr (S::id() == Id) {
                    return &e.first.mNode.module();
                }
                else {
                    using D = typename ugraph::edge_traits<Edge>::dst_vertex_t;
                    if constexpr (D::id() == Id) {
                        return &e.second.mNode.module();
                    }
                    else {
                        return (typename topology_t::template find_type_by_id<Id>::type::module_type*)nullptr;
                    }
                }
            }

            template<std::size_t Id>
            static constexpr auto get_module_ptr(const edges_t&... es) {
                typename topology_t::template find_type_by_id<Id>::type::module_type* r = nullptr;
                ((r = r ? r : try_edge_module<Id>(es)), ...);
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

            template<typename Manifest, typename T, std::size_t NodeIndex, std::size_t... P>
            static constexpr bool all_inputs_connected_impl(std::index_sequence<P...>) {
                constexpr std::size_t vid = topology_t::template id_at<NodeIndex>();
                return (has_input_edge<T, vid, P>() && ... && true);
            }

            template<typename Manifest, typename T, std::size_t NodeIndex>
            static constexpr bool all_inputs_connected() {
                if constexpr (Manifest::template input_count<T>() == 0) return true;
                else return all_inputs_connected_impl<Manifest, T, NodeIndex>(std::make_index_sequence<Manifest::template input_count<T>()>{});
            }

            template<typename Manifest, typename T, std::size_t NodeIndex, std::size_t... P>
            static constexpr bool all_outputs_connected_impl(std::index_sequence<P...>) {
                constexpr std::size_t vid = topology_t::template id_at<NodeIndex>();
                return (has_output_edge<T, vid, P>() && ... && true);
            }

            template<typename Manifest, typename T, std::size_t NodeIndex>
            static constexpr bool all_outputs_connected() {
                if constexpr (Manifest::template output_count<T>() == 0) return true;
                else return all_outputs_connected_impl<Manifest, T, NodeIndex>(std::make_index_sequence<Manifest::template output_count<T>()>{});
            }

            template<typename Manifest, typename T, std::size_t NodeIndex>
            static constexpr bool all_ports_connected_for_type() {
                return all_inputs_connected<Manifest, T, NodeIndex>() && all_outputs_connected<Manifest, T, NodeIndex>();
            }

            template<typename Manifest, std::size_t NodeIndex, std::size_t... TIdx>
            static constexpr bool all_ports_connected_for_manifest_types(std::index_sequence<TIdx...>) {
                return (all_ports_connected_for_type<Manifest, typename Manifest::template type_at<TIdx>, NodeIndex>() && ... && true);
            }

            template<std::size_t NodeIndex>
            static constexpr bool all_ports_connected_for_node() {
                using node_type = typename topology_t::template find_type_by_id<topology_t::template id_at<NodeIndex>()>::type;
                using node_manifest = typename node_type::module_type::Manifest;
                return all_ports_connected_for_manifest_types<node_manifest, NodeIndex>(std::make_index_sequence<node_manifest::type_count>{});
            }

            template<std::size_t... NodeIdx>
            static constexpr bool all_ports_connected_all_nodes(std::index_sequence<NodeIdx...>) {
                return (all_ports_connected_for_node<NodeIdx>() && ... && true);
            }

            static constexpr bool all_ports_connected =
                all_ports_connected_all_nodes(std::make_index_sequence<topology_t::size()>{});

            template<typename T, std::size_t NodeIndex, std::size_t... P>
            static constexpr bool has_any_input_for_node_impl(std::index_sequence<P...>) {
                constexpr std::size_t vid = topology_t::template id_at<NodeIndex>();
                return (has_input_edge<T, vid, P>() || ... || false);
            }

            template<typename T, std::size_t NodeIndex>
            static constexpr bool has_any_input_for_node() {
                if constexpr (graph_input_port_count<T>() == 0) return false;
                else return has_any_input_for_node_impl<T, NodeIndex>(std::make_index_sequence<graph_input_port_count<T>()>{});
            }

            template<typename T, std::size_t NodeIndex, std::size_t... P>
            static constexpr bool has_any_output_for_node_impl(std::index_sequence<P...>) {
                constexpr std::size_t vid = topology_t::template id_at<NodeIndex>();
                return (has_output_edge<T, vid, P>() || ... || false);
            }

            template<typename T, std::size_t NodeIndex>
            static constexpr bool has_any_output_for_node() {
                if constexpr (graph_output_port_count<T>() == 0) return false;
                else return has_any_output_for_node_impl<T, NodeIndex>(std::make_index_sequence<graph_output_port_count<T>()>{});
            }

            template<typename T, std::size_t NodeIndex>
            static constexpr bool participates_in_type() {
                return has_any_input_for_node<T, NodeIndex>() || has_any_output_for_node<T, NodeIndex>();
            }

            template<typename T, std::size_t NodeIndex, std::size_t... P>
            static constexpr std::size_t missing_inputs_for_node_impl(std::index_sequence<P...>) {
                constexpr std::size_t vid = topology_t::template id_at<NodeIndex>();
                return ((has_input_edge<T, vid, P>() ? 0 : 1) + ... + 0);
            }

            template<typename T, std::size_t NodeIndex>
            static constexpr std::size_t missing_inputs_for_node() {
                if constexpr (graph_input_port_count<T>() == 0) return 0;
                else if constexpr (!participates_in_type<T, NodeIndex>()) return 0;
                else return missing_inputs_for_node_impl<T, NodeIndex>(std::make_index_sequence<graph_input_port_count<T>()>{});
            }

            template<typename T, std::size_t... N>
            static constexpr std::size_t missing_inputs_all(std::index_sequence<N...>) {
                return (missing_inputs_for_node<T, N>() + ... + 0);
            }

            template<typename T>
            static constexpr std::size_t external_input_count() {
                return missing_inputs_all<T>(std::make_index_sequence<topology_t::size()>{});
            }

            template<typename T, std::size_t NodeIndex, std::size_t... P>
            static constexpr std::size_t missing_outputs_for_node_impl(std::index_sequence<P...>) {
                constexpr std::size_t vid = topology_t::template id_at<NodeIndex>();
                return ((has_output_edge<T, vid, P>() ? 0 : 1) + ... + 0);
            }

            template<typename T, std::size_t NodeIndex>
            static constexpr std::size_t missing_outputs_for_node() {
                if constexpr (graph_output_port_count<T>() == 0) return 0;
                else if constexpr (!participates_in_type<T, NodeIndex>()) return 0;
                else return missing_outputs_for_node_impl<T, NodeIndex>(std::make_index_sequence<graph_output_port_count<T>()>{});
            }

            template<typename T, std::size_t... N>
            static constexpr std::size_t missing_outputs_all(std::index_sequence<N...>) {
                return (missing_outputs_for_node<T, N>() + ... + 0);
            }

            template<typename T>
            static constexpr std::size_t external_output_count() {
                return missing_outputs_all<T>(std::make_index_sequence<topology_t::size()>{});
            }

            template<typename T, std::size_t LinearIndex>
            struct missing_input_before {
                static constexpr std::size_t in_count = graph_input_port_count<T>();
                static constexpr std::size_t node_index = detail::linear_port_traits<T, LinearIndex, in_count>::node_index;
                static constexpr std::size_t port_index = detail::linear_port_traits<T, LinearIndex, in_count>::port_index;
                static constexpr std::size_t vid = topology_t::template id_at<node_index>();
                static constexpr std::size_t prev = missing_input_before<T, LinearIndex - 1>::value;
                static constexpr bool missing = participates_in_type<T, node_index>() && !has_input_edge<T, vid, port_index>();
                static constexpr std::size_t value = prev + (missing ? 1 : 0);
            };

            template<typename T>
            struct missing_input_before<T, 0> {
                static constexpr std::size_t value = 0;
            };

            template<typename T, std::size_t LinearIndex>
            struct missing_output_before {
                static constexpr std::size_t out_count = manifest_t::template output_count<T>();
                static constexpr std::size_t node_index = detail::linear_port_traits<T, LinearIndex, out_count>::node_index;
                static constexpr std::size_t port_index = detail::linear_port_traits<T, LinearIndex, out_count>::port_index;
                static constexpr std::size_t vid = topology_t::template id_at<node_index>();
                static constexpr std::size_t prev = missing_output_before<T, LinearIndex - 1>::value;
                static constexpr bool missing = participates_in_type<T, node_index>() && !has_output_edge<T, vid, port_index>();
                static constexpr std::size_t value = prev + (missing ? 1 : 0);
            };

            template<typename T>
            struct missing_output_before<T, 0> {
                static constexpr std::size_t value = 0;
            };

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

            template<typename T, std::size_t NodeIndex, std::size_t... P>
            static constexpr std::array<std::size_t, graph_input_port_count<T>()>
                make_input_row(std::index_sequence<P...>) {
                return { input_index_for<T, NodeIndex, P>()... };
            }

            template<typename T, std::size_t NodeIndex, std::size_t... P>
            static constexpr std::array<std::size_t, graph_output_port_count<T>()>
                make_output_row(std::index_sequence<P...>) {
                return { output_index_for<T, NodeIndex, P>()... };
            }

            template<typename T, std::size_t... N>
            static constexpr std::array<std::array<std::size_t, graph_input_port_count<T>()>, topology_t::size()>
                make_input_map(std::index_sequence<N...>) {
                return { make_input_row<T, N>(std::make_index_sequence<graph_input_port_count<T>()>{})... };
            }

            template<typename T, std::size_t... N>
            static constexpr std::array<std::array<std::size_t, graph_output_port_count<T>()>, topology_t::size()>
                make_output_map(std::index_sequence<N...>) {
                return { make_output_row<T, N>(std::make_index_sequence<graph_output_port_count<T>()>{})... };
            }

            template<typename T>
            static constexpr auto input_map() {
                return make_input_map<T>(std::make_index_sequence<topology_t::size()>{});
            }

            template<typename T>
            static constexpr auto output_map() {
                return make_output_map<T>(std::make_index_sequence<topology_t::size()>{});
            }

            template<typename T>
            static std::size_t input_index_fn(std::size_t node_index, std::size_t port) {
                static constexpr auto map = input_map<T>();
                constexpr std::size_t port_count = graph_input_port_count<T>();
                if (node_index >= map.size() || port >= port_count) {
                    return invalid_index;
                }
                return map[node_index][port];
            }

            template<typename T>
            static std::size_t output_index_fn(std::size_t node_index, std::size_t port) {
                static constexpr auto map = output_map<T>();
                constexpr std::size_t port_count = graph_output_port_count<T>();
                if (node_index >= map.size() || port >= port_count) {
                    return invalid_index;
                }
                return map[node_index][port];
            }

            template<std::size_t... I>
            static constexpr void init_index_fns(std::array<index_fn_t, manifest_t::type_count>& input,
                std::array<index_fn_t, manifest_t::type_count>& output,
                std::index_sequence<I...>) {
                ((input[I] = &input_index_fn<typename manifest_t::template type_at<I>>,
                    output[I] = &output_index_fn<typename manifest_t::template type_at<I>>), ...);
            }

            static constexpr void init_index_fns(std::array<index_fn_t, manifest_t::type_count>& input,
                std::array<index_fn_t, manifest_t::type_count>& output) {
                init_index_fns(input, output, std::make_index_sequence<manifest_t::type_count>{});
            }

            template<typename NodeManifest, std::size_t... I>
            static constexpr std::array<std::size_t, NodeManifest::type_count>
                make_type_map(std::index_sequence<I...>) {
                return { manifest_t::template index<typename NodeManifest::template type_at<I>>()... };
            }

            template<typename NodeManifest>
            static const std::array<std::size_t, NodeManifest::type_count>& type_map_for() {
                static constexpr auto map = make_type_map<NodeManifest>(std::make_index_sequence<NodeManifest::type_count>{});
                return map;
            }
        };
    } // namespace detail

    template<typename... edges_t>
    class DataGraph {
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
        std::array<void*, manifest_t::type_count> mDataPtrs {};
        std::array<typename traits::index_fn_t, manifest_t::type_count> mInputIndex {};
        std::array<typename traits::index_fn_t, manifest_t::type_count> mOutputIndex {};
        template<std::size_t... I>
        static constexpr auto make_data_storage_tuple_t(std::index_sequence<I...>) ->
            std::tuple<std::unique_ptr<typename manifest_t::template type_at<I>[]>...>;

        using data_storage_tuple_t = decltype(make_data_storage_tuple_t(std::make_index_sequence<manifest_t::type_count>{}));
        data_storage_tuple_t mDataStorage {};

    public:
        using topology_type = topology_t;

        constexpr DataGraph(const edges_t&... es) :
            mModules(traits::build_modules(std::make_index_sequence<topology_t::size()>{}, es...)) {
            traits::init_index_fns(mInputIndex, mOutputIndex);
            allocate_data_storage_impl(std::make_index_sequence<manifest_t::type_count>{});
            init_contexts(std::make_index_sequence<topology_t::size()>{});
        }

        template<typename T>
        static constexpr std::size_t data_instance_count() {
            return traits::template coloring_t<T>::data_instance_count();
        }

        template<typename F>
        constexpr void for_each(F&& f) {
            for_each_impl(std::forward<F>(f), std::make_index_sequence<topology_t::size()>{});
        }

    private:
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
            auto& map = traits::template type_map_for<node_manifest>();
            auto& ctx = std::get<I>(mContexts);
            ctx = NodeContext<node_manifest>(mDataPtrs.data(), mInputIndex.data(), mOutputIndex.data(), map.data(), I);
        }

        template<std::size_t... I>
        constexpr void init_contexts(std::index_sequence<I...>) {
            (init_context_at<I>(), ...);
        }

        template<std::size_t I>
        constexpr void allocate_for_index() {
            using T = typename manifest_t::template type_at<I>;
            constexpr std::size_t count = data_instance_count<T>();
            if constexpr (count > 0) {
                std::get<I>(mDataStorage) = std::unique_ptr<T[]>(new T[count]);
                mDataPtrs[I] = std::get<I>(mDataStorage).get();
            }
            else {
                mDataPtrs[I] = nullptr;
            }
        }

        template<std::size_t... I>
        constexpr void allocate_data_storage_impl(std::index_sequence<I...>) {
            (allocate_for_index<I>(), ...);
        }

        template<typename T, std::size_t I>
        constexpr void refresh_context_for_type_at() {
            using node_type = node_type_at<I>;
            using node_manifest = typename node_type::module_type::Manifest;
            if constexpr (node_manifest::template contains<T>) {
                std::get<I>(mContexts).template refresh_pointers_for_type<T>();
            }
        }
    };

    template<typename E0, typename... ERest>
    DataGraph(E0 const&, ERest const&...) -> DataGraph<std::decay_t<E0>, std::decay_t<ERest>...>;

} // namespace ugraph
