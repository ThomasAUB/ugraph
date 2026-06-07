#pragma once
#include <type_traits>

namespace ugraph::detail {

    // Trait to check if a type is an output port (has data_type and node_type)
    template<typename T, typename = void>
    struct is_output_port : std::false_type {};

    template<typename T>
    struct is_output_port<T, std::void_t<typename T::data_type, typename T::node_type>> : std::true_type {};

    // Trait to check if a type is an input port (has node_type, but not necessarily data_type)
    template<typename T, typename = void>
    struct is_input_port : std::false_type {};

    template<typename T>
    struct is_input_port<T, std::void_t<typename T::node_type>> : std::true_type {};


    // Trait to check if a type is a port (input or output)
    template<typename T, typename = void>
    struct is_a_port : std::false_type {};

    template<typename T>
    struct is_a_port<T, std::enable_if_t<is_input_port<T>::value || is_output_port<T>::value>> : std::true_type {};


}
