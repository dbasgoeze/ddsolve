#ifndef GSL_HPP
#define GSL_HPP

/**
 * @file gsl.hpp
 * @brief Guideline Support Library – minimal subset.
 *
 * This header provides a lightweight subset of the Microsoft GSL library
 * used throughout the project. Currently it only includes the @p owner
 * smart-pointer alias for ownership semantics.
 *
 * @note The Doxygen-style comments in this file were generated with the help
 *       of AI (Qwen3.6-35B-A3B). They have been human-checked on a best-effort
 *       basis. No lines of logic in this file were written or modified by AI.
 */

#include <type_traits>

namespace gsl {

/**
 * @brief Ownership pointer alias.
 *
 * A type alias that marks a raw pointer as owning the pointed-to object.
 * It serves as a documentation aid and enables static analysis tools to
 * enforce ownership transfer rules.
 *
 * @tparam T  The pointed-to type. Must be a pointer type.
 */
template <typename T, typename = std::enable_if_t<std::is_pointer_v<T>>>
using owner = T;
}

#endif /* GSL_HPP */
