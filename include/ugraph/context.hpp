#pragma once

#include <array>
#include <tuple>
#include <type_traits>

#include "manifest.hpp"

namespace ugraph {

    template<typename T> struct DataSpan;

    template<typename manifest_t>
    struct Context {

        template<typename key_t>
        using spec_for_t = typename manifest_t::template spec_for<key_t>;

        template<typename key_t>
        using value_for_t = typename manifest_t::template data_type_for<key_t>;

        template<typename spec_t>
        using data_array_for_spec_t = std::array<
            typename detail::io_traits<spec_t>::type*,
            detail::io_traits<spec_t>::input_count + detail::io_traits<spec_t>::output_count
        >;

        template<typename spec_t>
        struct data_ptr_slot {
            data_array_for_spec_t<spec_t> ptrs {};
        };

        constexpr Context() = default;

        template<typename data_t>
        static constexpr bool contains() {
            return manifest_t::template contains<data_t>;
        }

        template<typename data_t>
        static constexpr std::size_t input_count() {
            return manifest_t::template input_count<data_t>();
        }

        template<typename data_t>
        static constexpr std::size_t output_count() {
            return manifest_t::template output_count<data_t>();
        }

        template<typename key_t>
        using data_array_t = data_array_for_spec_t<spec_for_t<key_t>>;

        template<typename key_t>
        constexpr inline const value_for_t<key_t>& input() const {
            static_assert(contains<key_t>(), "Type not declared in Manifest");
            static_assert(input_count<key_t>() == 1, "This overload is only valid for single-input types");
            return *slot_ptrs<key_t>()[0];
        }

        template<typename key_t>
        constexpr inline value_for_t<key_t>& output() {
            static_assert(contains<key_t>(), "Type not declared in Manifest");
            static_assert(output_count<key_t>() == 1, "This overload is only valid for single-output types");
            return *slot_ptrs<key_t>()[input_count<key_t>()];
        }

        template<typename key_t>
        constexpr inline const value_for_t<key_t>& input(std::size_t port) const {
            static_assert(contains<key_t>(), "Type not declared in Manifest");
            return *slot_ptrs<key_t>()[port];
        }

        template<typename key_t>
        constexpr inline value_for_t<key_t>& output(std::size_t port) {
            static_assert(contains<key_t>(), "Type not declared in Manifest");
            return *slot_ptrs<key_t>()[input_count<key_t>() + port];
        }

        template<typename key_t>
        constexpr inline auto inputs() const {
            static_assert(contains<key_t>(), "Type not declared in Manifest");
            static_assert(input_count<key_t>() > 0, "No input ports for this type");
            return DataSpan<const value_for_t<key_t>>(slot_ptrs<key_t>().data(), input_count<key_t>());
        }

        template<typename key_t>
        constexpr inline auto outputs() {
            static_assert(contains<key_t>(), "Type not declared in Manifest");
            static_assert(output_count<key_t>() > 0, "No output ports for this type");
            return DataSpan<value_for_t<key_t>>(slot_ptrs<key_t>().data() + input_count<key_t>(), output_count<key_t>());
        }

        template<typename key_t>
        constexpr inline auto inputs_ptr() const {
            static_assert(contains<key_t>(), "Type not declared in Manifest");
            static_assert(input_count<key_t>() > 0, "No input ports for this type");
            return slot_ptrs<key_t>().data();
        }

        template<typename key_t>
        constexpr inline auto outputs_ptr() {
            static_assert(contains<key_t>(), "Type not declared in Manifest");
            static_assert(output_count<key_t>() > 0, "No output ports for this type");
            return slot_ptrs<key_t>().data() + input_count<key_t>();
        }

        template<typename key_t, std::size_t I = 0>
        constexpr inline bool has_input() const {
            static_assert(contains<key_t>(), "Type not declared in Manifest");
            if constexpr (I >= input_count<key_t>()) {
                return false;
            }
            else {
                return slot_ptrs<key_t>()[I] != nullptr;
            }
        }

        template<typename key_t, std::size_t I = 0>
        constexpr inline bool has_output() const {
            static_assert(contains<key_t>(), "Type not declared in Manifest");
            if constexpr (I >= output_count<key_t>()) {
                return false;
            }
            else {
                return slot_ptrs<key_t>()[input_count<key_t>() + I] != nullptr;
            }
        }

        template<typename key_t>
        constexpr void set_ios(const data_array_t<key_t>& inData) {
            slot_ptrs<key_t>() = inData;
        }

        template<typename data_t, std::size_t N>
        constexpr void set_ios(const std::array<data_t*, N>& inData) {
            static_assert(manifest_t::template spec_count_for_value<data_t> == 1, "set_ios(data array) requires exactly one untagged spec for this type");
            static_assert(N == input_count<data_t>() + output_count<data_t>(), "Invalid IO array size for this type");
            slot_ptrs<data_t>() = inData;
        }

        template<std::size_t I, typename key_t>
        constexpr void set_input_ptr(value_for_t<key_t>* ptr) {
            static_assert(contains<key_t>(), "Type not declared in Manifest");
            static_assert(I < input_count<key_t>(), "Invalid input index");
            auto& data = slot_ptrs<key_t>();
            data[I] = ptr;
        }

        template<std::size_t I, typename key_t>
        constexpr void set_output_ptr(value_for_t<key_t>* ptr) {
            static_assert(contains<key_t>(), "Type not declared in Manifest");
            static_assert(I < output_count<key_t>(), "Invalid output index");
            auto& data = slot_ptrs<key_t>();
            data[input_count<key_t>() + I] = ptr;
        }

    private:

        template<typename key_t>
        constexpr auto& slot_ptrs() {
            constexpr std::size_t index = manifest_t::template index<key_t>();
            return std::get<index>(mDataPtrsTuple).ptrs;
        }

        template<typename key_t>
        constexpr const auto& slot_ptrs() const {
            constexpr std::size_t index = manifest_t::template index<key_t>();
            return std::get<index>(mDataPtrsTuple).ptrs;
        }

        template<typename Seq>
        struct data_ptrs_tuple_maker;

        template<std::size_t... Is>
        struct data_ptrs_tuple_maker<std::index_sequence<Is...>> {
            using type = std::tuple<data_ptr_slot<typename manifest_t::template spec_at<Is>>...>;
        };

        using tuple_type = typename data_ptrs_tuple_maker<std::make_index_sequence<manifest_t::spec_count>>::type;

        tuple_type mDataPtrsTuple;

    };


    template<typename T>
    struct DataSpan {

        class iterator {
            T* const* mPtr;
        public:
            constexpr iterator(T* const* p) : mPtr(p) {}
            constexpr T& operator*() const { return **mPtr; }
            constexpr iterator& operator++() { ++mPtr; return *this; }
            constexpr bool operator!=(const iterator& other) const { return mPtr != other.mPtr; }
        };

        class const_iterator {
            T* const* mPtr;
        public:
            constexpr const_iterator(T* const* p) : mPtr(p) {}
            constexpr const T& operator*() const { return **mPtr; }
            constexpr const_iterator& operator++() { ++mPtr; return *this; }
            constexpr bool operator!=(const const_iterator& other) const { return mPtr != other.mPtr; }
        };

        constexpr DataSpan() : mData(nullptr), mSize(0) {}
        constexpr DataSpan(T* const* d, std::size_t s) : mData(d), mSize(s) {}

        constexpr iterator begin() { return iterator { mData }; }
        constexpr iterator end() { return iterator { mData + mSize }; }

        constexpr const_iterator begin() const { return const_iterator { mData }; }
        constexpr const_iterator end() const { return const_iterator { mData + mSize }; }

        constexpr std::size_t size() const { return mSize; }

        constexpr inline T& operator [](std::size_t i) {
            return **(mData + i);
        }

        constexpr inline const T& operator [](std::size_t i) const {
            return **(mData + i);
        }

    private:
        T* const* mData;
        std::size_t mSize;
    };

}