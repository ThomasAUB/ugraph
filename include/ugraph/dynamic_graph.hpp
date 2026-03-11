#pragma once

#include <array>
#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

#include "context.hpp"
#include "node.hpp"
#include "type_traits/edge_traits.hpp"
#include "type_traits/type_list.hpp"

namespace ugraph {

    namespace detail {

        template<typename manifest_t, std::size_t... I>
        constexpr std::size_t manifest_input_total_impl(std::index_sequence<I...>) {
            return (std::size_t(0) + ... + manifest_t::template input_count<typename manifest_t::template type_at<I>>());
        }

        template<typename manifest_t, std::size_t... I>
        constexpr std::size_t manifest_output_total_impl(std::index_sequence<I...>) {
            return (std::size_t(0) + ... + manifest_t::template output_count<typename manifest_t::template type_at<I>>());
        }

        template<typename node_t>
        struct node_manifest_io_totals {
            using manifest_t = typename node_t::module_type::Manifest;
            static constexpr std::size_t input_total = manifest_input_total_impl<manifest_t>(
                std::make_index_sequence<manifest_t::type_count>{}
            );
            static constexpr std::size_t output_total = manifest_output_total_impl<manifest_t>(
                std::make_index_sequence<manifest_t::type_count>{}
            );
        };

    } // namespace detail

    template<typename... node_ts>
    class DynamicGraph {

        static_assert(sizeof...(node_ts) > 0, "DynamicGraph requires at least one node");

        using node_list_t = detail::type_list<node_ts...>;
        static constexpr std::size_t node_count = sizeof...(node_ts);
        static constexpr std::size_t invalid_index = static_cast<std::size_t>(-1);

        template<std::size_t I>
        using node_type_at = typename detail::type_list_at<I, node_list_t>::type;

        template<std::size_t... I>
        static constexpr auto make_node_ids(std::index_sequence<I...>) {
            return std::array<std::size_t, sizeof...(I)>{ node_type_at<I>::id()... };
        }

        template<std::size_t... I>
        static constexpr auto make_node_priorities(std::index_sequence<I...>) {
            return std::array<std::size_t, sizeof...(I)>{ node_type_at<I>::priority()... };
        }

        template<std::size_t... I>
        static constexpr auto make_node_input_counts(std::index_sequence<I...>) {
            return std::array<std::size_t, sizeof...(I)>{ detail::node_manifest_io_totals<node_type_at<I>>::input_total... };
        }

        template<std::size_t... I>
        static constexpr auto make_node_output_counts(std::index_sequence<I...>) {
            return std::array<std::size_t, sizeof...(I)>{ detail::node_manifest_io_totals<node_type_at<I>>::output_total... };
        }

        static constexpr auto node_ids = make_node_ids(std::make_index_sequence<node_count>{});
        static constexpr auto node_priorities = make_node_priorities(std::make_index_sequence<node_count>{});
        static constexpr auto node_input_counts = make_node_input_counts(std::make_index_sequence<node_count>{});
        static constexpr auto node_output_counts = make_node_output_counts(std::make_index_sequence<node_count>{});

        static constexpr bool has_unique_node_ids() {
            for (std::size_t i = 0; i < node_count; ++i) {
                for (std::size_t j = i + 1; j < node_count; ++j) {
                    if (node_ids[i] == node_ids[j]) {
                        return false;
                    }
                }
            }
            return true;
        }

        static_assert(has_unique_node_ids(), "Duplicate node ids detected in DynamicGraph");

        static constexpr std::size_t route_capacity = (detail::node_manifest_io_totals<node_ts>::input_total + ... + 0);
        static constexpr std::size_t output_capacity = (detail::node_manifest_io_totals<node_ts>::output_total + ... + 0);

        template<std::size_t... I>
        static constexpr auto make_contexts_tuple_t(std::index_sequence<I...>) ->
            std::tuple<Context<typename node_type_at<I>::module_type::Manifest>...>;

        using contexts_tuple_t = decltype(make_contexts_tuple_t(std::make_index_sequence<node_count>{}));
        using modules_tuple_t = std::tuple<typename node_ts::module_type*...>;

        struct route_entry {
            std::size_t src_index = invalid_index;
            std::size_t dst_index = invalid_index;
            bool active = false;
        };

        modules_tuple_t mModules;
        contexts_tuple_t mContexts;
        std::array<route_entry, route_capacity> mRoutes {};
        std::array<const void*, output_capacity> mOutputData {};
        std::array<void*, node_count> mModulePtrs {};
        std::array<void*, node_count> mContextPtrs {};
        std::array<void (*) (void*, void*), node_count> mInvokers {};
        std::array<void*, node_count> mOrderedModules {};
        std::array<void*, node_count> mOrderedContexts {};
        std::array<void (*) (void*, void*), node_count> mOrderedInvokers {};
        bool mDirty = true;
        bool mCompiled = false;

        template<std::size_t node_id>
        static constexpr std::size_t node_index_by_id() {
            for (std::size_t i = 0; i < node_count; ++i) {
                if (node_ids[i] == node_id) {
                    return i;
                }
            }
            return invalid_index;
        }

        template<typename manifest_t, typename data_t, std::size_t... I>
        static constexpr std::size_t manifest_input_offset_impl(std::index_sequence<I...>) {
            constexpr std::size_t target_index = manifest_t::template index<data_t>();
            return (std::size_t(0) + ... + (I < target_index ? manifest_t::template input_count<typename manifest_t::template type_at<I>>() : 0));
        }

        template<typename manifest_t, typename data_t, std::size_t... I>
        static constexpr std::size_t manifest_output_offset_impl(std::index_sequence<I...>) {
            constexpr std::size_t target_index = manifest_t::template index<data_t>();
            return (std::size_t(0) + ... + (I < target_index ? manifest_t::template output_count<typename manifest_t::template type_at<I>>() : 0));
        }

        template<std::size_t NodeIndex, std::size_t... I>
        static constexpr std::size_t node_input_base_impl(std::index_sequence<I...>) {
            return (std::size_t(0) + ... + (I < NodeIndex ? detail::node_manifest_io_totals<node_type_at<I>>::input_total : 0));
        }

        template<std::size_t NodeIndex, std::size_t... I>
        static constexpr std::size_t node_output_base_impl(std::index_sequence<I...>) {
            return (std::size_t(0) + ... + (I < NodeIndex ? detail::node_manifest_io_totals<node_type_at<I>>::output_total : 0));
        }

        template<std::size_t node_id, typename data_t, std::size_t input_index>
        static constexpr std::size_t input_slot_for() {
            constexpr std::size_t node_index = node_index_by_id<node_id>();
            static_assert(node_index != invalid_index, "Invalid node id");
            using node_type = node_type_at<node_index>;
            using manifest_t = typename node_type::module_type::Manifest;
            static_assert(manifest_t::template contains<data_t>, "Type not declared in node Manifest");
            static_assert(input_index < manifest_t::template input_count<data_t>(), "Invalid input index for this node/type");

            return node_input_base_impl<node_index>(std::make_index_sequence<node_count>{}) +
                manifest_input_offset_impl<manifest_t, data_t>(std::make_index_sequence<manifest_t::type_count>{}) +
                input_index;
        }

        template<std::size_t node_id, typename data_t, std::size_t output_index>
        static constexpr std::size_t output_slot_for() {
            constexpr std::size_t node_index = node_index_by_id<node_id>();
            static_assert(node_index != invalid_index, "Invalid node id");
            using node_type = node_type_at<node_index>;
            using manifest_t = typename node_type::module_type::Manifest;
            static_assert(manifest_t::template contains<data_t>, "Type not declared in node Manifest");
            static_assert(output_index < manifest_t::template output_count<data_t>(), "Invalid output index for this node/type");

            return node_output_base_impl<node_index>(std::make_index_sequence<node_count>{}) +
                manifest_output_offset_impl<manifest_t, data_t>(std::make_index_sequence<manifest_t::type_count>{}) +
                output_index;
        }

        template<std::size_t I>
        static void invoke_process(void* module_ptr, void* context_ptr) {
            using node_type = node_type_at<I>;
            using module_t = typename node_type::module_type;
            using context_t = Context<typename module_t::Manifest>;

            auto& module = *static_cast<module_t*>(module_ptr);
            auto& context = *static_cast<context_t*>(context_ptr);
            module.process(context);
        }

        template<std::size_t... I>
        constexpr void init_runtime_views(std::index_sequence<I...>) {
            ((
                mModulePtrs[I] = std::get<I>(mModules),
                mContextPtrs[I] = &std::get<I>(mContexts),
                mInvokers[I] = &invoke_process<I>
            ), ...);
        }

        template<
            std::size_t src_id,
            std::size_t src_port_index,
            std::size_t dst_id,
            std::size_t dst_port_index,
            typename data_t
        >
        constexpr bool route_impl(data_t& data) {
            constexpr std::size_t src_node_index = node_index_by_id<src_id>();
            constexpr std::size_t dst_node_index = node_index_by_id<dst_id>();
            static_assert(src_node_index != invalid_index, "Invalid source node id");
            static_assert(dst_node_index != invalid_index, "Invalid destination node id");

            constexpr std::size_t input_slot = input_slot_for<dst_id, data_t, dst_port_index>();
            if (mRoutes[input_slot].active) {
                return false;
            }

            constexpr std::size_t output_slot = output_slot_for<src_id, data_t, src_port_index>();
            const void* data_ptr = &data;
            if (mOutputData[output_slot] != nullptr && mOutputData[output_slot] != data_ptr) {
                return false;
            }

            auto& src_ctx = std::get<src_node_index>(mContexts);
            auto& dst_ctx = std::get<dst_node_index>(mContexts);
            src_ctx.template set_output_ptr<src_port_index, data_t>(&data);
            dst_ctx.template set_input_ptr<dst_port_index, data_t>(&data);

            mOutputData[output_slot] = data_ptr;
            mRoutes[input_slot] = route_entry { src_node_index, dst_node_index, true };
            mDirty = true;
            return true;
        }

    public:

        constexpr DynamicGraph(node_ts&... nodes) :
            mModules((&nodes.module())...) {
            init_runtime_views(std::make_index_sequence<node_count>{});
        }

        static constexpr auto ids() { return node_ids; }
        static constexpr std::size_t size() { return node_count; }

        template<std::size_t node_id>
        static constexpr bool contains_node_id() {
            return node_index_by_id<node_id>() != invalid_index;
        }

        template<std::size_t node_id>
        constexpr auto module_ptr_by_id()
            -> typename node_type_at<node_index_by_id<node_id>()>::module_type* {
            constexpr std::size_t node_index = node_index_by_id<node_id>();
            static_assert(node_index != invalid_index, "Invalid node id");
            return std::get<node_index>(mModules);
        }

        template<typename edge_t, typename data_t>
        constexpr bool route(const edge_t&, data_t& data) {
            using edge_type = std::decay_t<edge_t>;
            using edge_traits = detail::edge_traits<edge_type>;
            using edge_data_t = typename edge_type::first_type::data_type;
            static_assert(
                std::is_same_v<std::remove_cv_t<std::remove_reference_t<data_t>>, std::remove_cv_t<edge_data_t>>,
                "Route data type must match the edge data type"
            );

            return route_impl<
                edge_traits::src_id,
                edge_traits::src_port_index,
                edge_traits::dst_id,
                edge_traits::dst_port_index,
                edge_data_t
            >(data);
        }

        template<typename out_port_t, typename in_port_t, typename data_t>
        constexpr bool route(const out_port_t& out, const in_port_t& in, data_t& data) {
            return route(out >> in, data);
        }

        template<std::size_t node_id, typename data_t>
        constexpr bool bind_input(data_t& data) {
            constexpr std::size_t node_index = node_index_by_id<node_id>();
            static_assert(node_index != invalid_index, "Invalid node id");
            using node_type = node_type_at<node_index>;
            using manifest_t = typename node_type::module_type::Manifest;
            static_assert(manifest_t::template contains<data_t>, "Type not declared in node Manifest");
            constexpr std::size_t in_count = manifest_t::template input_count<data_t>();
            static_assert(in_count > 0, "No input ports for this type");
            static_assert(in_count == 1, "Use bind_input_at for multi-input types");
            return bind_input_at<node_id, 0>(data);
        }

        template<std::size_t node_id, std::size_t input_index, typename data_t>
        constexpr bool bind_input_at(data_t& data) {
            constexpr std::size_t node_index = node_index_by_id<node_id>();
            static_assert(node_index != invalid_index, "Invalid node id");
            using node_type = node_type_at<node_index>;
            using manifest_t = typename node_type::module_type::Manifest;
            static_assert(manifest_t::template contains<data_t>, "Type not declared in node Manifest");
            constexpr std::size_t in_count = manifest_t::template input_count<data_t>();
            static_assert(input_index < in_count, "Invalid input index for this node/type");

            constexpr std::size_t slot = input_slot_for<node_id, data_t, input_index>();
            if (mRoutes[slot].active) {
                return false;
            }

            auto& ctx = std::get<node_index>(mContexts);
            ctx.template set_input_ptr<input_index, data_t>(&data);
            return true;
        }

        template<std::size_t node_id, typename data_t>
        constexpr bool bind_output(data_t& data) {
            constexpr std::size_t node_index = node_index_by_id<node_id>();
            static_assert(node_index != invalid_index, "Invalid node id");
            using node_type = node_type_at<node_index>;
            using manifest_t = typename node_type::module_type::Manifest;
            static_assert(manifest_t::template contains<data_t>, "Type not declared in node Manifest");
            constexpr std::size_t out_count = manifest_t::template output_count<data_t>();
            static_assert(out_count > 0, "No output ports for this type");
            static_assert(out_count == 1, "Use bind_output_at for multi-output types");
            return bind_output_at<node_id, 0>(data);
        }

        template<std::size_t node_id, std::size_t output_index, typename data_t>
        constexpr bool bind_output_at(data_t& data) {
            constexpr std::size_t node_index = node_index_by_id<node_id>();
            static_assert(node_index != invalid_index, "Invalid node id");
            using node_type = node_type_at<node_index>;
            using manifest_t = typename node_type::module_type::Manifest;
            static_assert(manifest_t::template contains<data_t>, "Type not declared in node Manifest");
            constexpr std::size_t out_count = manifest_t::template output_count<data_t>();
            static_assert(output_index < out_count, "Invalid output index for this node/type");

            constexpr std::size_t slot = output_slot_for<node_id, data_t, output_index>();
            const void* data_ptr = &data;
            if (mOutputData[slot] != nullptr && mOutputData[slot] != data_ptr) {
                return false;
            }

            mOutputData[slot] = data_ptr;
            auto& ctx = std::get<node_index>(mContexts);
            ctx.template set_output_ptr<output_index, data_t>(&data);
            return true;
        }

        constexpr bool compile() {
            std::array<std::size_t, node_count> indegrees {};
            std::array<bool, node_count> used {};
            std::array<std::size_t, node_count> order {};

            for (const auto& route : mRoutes) {
                if (route.active) {
                    ++indegrees[route.dst_index];
                }
            }

            std::size_t placed = 0;
            while (placed < node_count) {
                std::size_t pick = invalid_index;
                std::size_t best_priority = 0;
                bool found = false;

                for (std::size_t i = 0; i < node_count; ++i) {
                    if (!used[i] && indegrees[i] == 0) {
                        const auto priority = node_priorities[i];
                        if (!found || priority > best_priority) {
                            best_priority = priority;
                            pick = i;
                            found = true;
                        }
                    }
                }

                if (!found) {
                    mCompiled = false;
                    return false;
                }

                order[placed] = pick;
                used[pick] = true;

                for (const auto& route : mRoutes) {
                    if (route.active && route.src_index == pick && indegrees[route.dst_index] > 0) {
                        --indegrees[route.dst_index];
                    }
                }

                ++placed;
            }

            for (std::size_t i = 0; i < node_count; ++i) {
                const auto node_index = order[i];
                mOrderedModules[i] = mModulePtrs[node_index];
                mOrderedContexts[i] = mContextPtrs[node_index];
                mOrderedInvokers[i] = mInvokers[node_index];
            }

            mCompiled = true;
            mDirty = false;
            return true;
        }

        constexpr bool process() {
            if ((mDirty || !mCompiled) && !compile()) {
                return false;
            }

            for (std::size_t i = 0; i < node_count; ++i) {
                mOrderedInvokers[i](mOrderedModules[i], mOrderedContexts[i]);
            }
            return true;
        }

        constexpr bool all_ios_connected() const {
            return std::apply([] (auto const&... ctxs) { return (ctxs.all_ios_connected() && ...); }, mContexts);
        }

    };

    template<typename... node_ts>
    DynamicGraph(node_ts&...) -> DynamicGraph<std::decay_t<node_ts>...>;

} // namespace ugraph