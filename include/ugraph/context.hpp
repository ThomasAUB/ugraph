#pragma once

#include <array>
#include <tuple>
#include <type_traits>

namespace ugraph {

    template<typename T> struct DataSpan;

    template<typename manifest_t>
    struct Context {

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

        template<typename data_t>
        using data_array_t = std::array<data_t*, input_count<data_t>() + output_count<data_t>()>;

        template<typename data_t>
        constexpr inline const data_t& input() const {
            static_assert(contains<data_t>(), "Type not declared in Manifest");
            static_assert(input_count<data_t>() == 1, "This overload is only valid for single-input types");
            return *std::get<data_array_t<data_t>>(mDataPtrsTuple)[0];
        }

        template<typename data_t>
        constexpr inline data_t& output() {
            static_assert(contains<data_t>(), "Type not declared in Manifest");
            static_assert(output_count<data_t>() == 1, "This overload is only valid for single-output types");
            return *std::get<data_array_t<data_t>>(mDataPtrsTuple)[input_count<data_t>()];
        }

        template<typename data_t>
        constexpr inline const data_t& input(std::size_t port) const {
            static_assert(contains<data_t>(), "Type not declared in Manifest");
            return *std::get<data_array_t<data_t>>(mDataPtrsTuple)[port];
        }

        template<typename data_t>
        constexpr inline data_t& output(std::size_t port) {
            static_assert(contains<data_t>(), "Type not declared in Manifest");
            return *std::get<data_array_t<data_t>>(mDataPtrsTuple)[input_count<data_t>() + port];
        }

        template<typename data_t>
        constexpr inline auto inputs() const {
            static_assert(contains<data_t>(), "Type not declared in Manifest");
            static_assert(input_count<data_t>() > 0, "No input ports for this type");
            return DataSpan<const data_t>(std::get<data_array_t<data_t>>(mDataPtrsTuple).data(), input_count<data_t>());
        }

        template<typename data_t>
        constexpr inline auto outputs() {
            static_assert(contains<data_t>(), "Type not declared in Manifest");
            static_assert(output_count<data_t>() > 0, "No output ports for this type");
            return DataSpan<data_t>(std::get<data_array_t<data_t>>(mDataPtrsTuple).data() + input_count<data_t>(), output_count<data_t>());
        }

        template<typename data_t, std::size_t I = 0>
        constexpr inline bool has_input() const {
            static_assert(contains<data_t>(), "Type not declared in Manifest");
            if constexpr (I >= input_count<data_t>()) {
                return false;
            }
            else {
                return std::get<data_array_t<data_t>>(mDataPtrsTuple)[I] != nullptr;
            }
        }

        template<typename data_t, std::size_t I = 0>
        constexpr inline bool has_output() const {
            static_assert(contains<data_t>(), "Type not declared in Manifest");
            if constexpr (I >= output_count<data_t>()) {
                return false;
            }
            else {
                return std::get<data_array_t<data_t>>(mDataPtrsTuple)[input_count<data_t>() + I] != nullptr;
            }
        }

        template<typename data_t>
        constexpr void set_ios(const data_array_t<data_t>& inData) {
            std::get<std::decay_t<decltype(inData)>>(mDataPtrsTuple) = inData;
        }

        template<std::size_t I, typename data_t>
        constexpr void set_input_ptr(data_t* ptr) {
            static_assert(contains<data_t>(), "Type not declared in Manifest");
            static_assert(I < input_count<data_t>(), "Invalid input index");
            auto& data = std::get<data_array_t<data_t>>(mDataPtrsTuple);
            data[I] = ptr;
        }

        template<std::size_t I, typename data_t >
        constexpr void set_output_ptr(data_t* ptr) {
            static_assert(contains<data_t>(), "Type not declared in Manifest");
            static_assert(I < output_count<data_t>(), "Invalid output index");
            auto& data = std::get<data_array_t<data_t>>(mDataPtrsTuple);
            data[input_count<data_t>() + I] = ptr;
        }

        constexpr bool all_ios_connected() const {
            return std::apply([] (auto const&... arrays) {
                return (true && ... && ([&] (auto const& arr) {
                    for (auto ptr : arr) {
                        if (ptr == nullptr) return false;
                    }
                    return true;
                    })(arrays));
                }, mDataPtrsTuple
            );
        }

    private:

        template<typename Seq>
        struct data_ptrs_tuple_maker;

        template<std::size_t... Is>
        struct data_ptrs_tuple_maker<std::index_sequence<Is...>> {
            using type = std::tuple<data_array_t<typename manifest_t::template type_at<Is>>...>;
        };

        using tuple_type = typename data_ptrs_tuple_maker<std::make_index_sequence<manifest_t::type_count>>::type;

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