#ifndef GSL_HPP
#define GSL_HPP

#include <type_traits>

namespace gsl {
template <typename T, typename = std::enable_if_t<std::is_pointer_v<T>>>
using owner = T;
}

#endif /* GSL_HPP */
