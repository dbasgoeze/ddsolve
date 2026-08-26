/**
 * @file SmtWrapper.hpp
 * @brief SMT wrapper concept and type aliases for the selected solver.
 *
 * This header defines the SmtWrapper C++20 concept that constrains the
 * interface expected of SMT solver backends (CVC5 or Z3).  It also provides
 * type aliases (RealValuedWrapper, IntValuedWrapper) that resolve to the
 * concrete wrapper type based on the SMT_SOLVER compile-time configuration.
 *
 * @note The Doxygen-style comments in this file were generated with the help
 *       of AI (Qwen3.6-35B-A3B). They have been human-checked on a best-effort
 *       basis. No lines of logic in this file were written or modified by AI.
 */

#include <concepts>
#include <type_traits>

#include "ArithmeticTypes.hpp"
#include "smt/smt_config.hpp"

#if SMT_SOLVER == CVC5
#include "smt/Cvc5Wrapper.hpp"
#endif

#if SMT_SOLVER == Z3
#include "smt/Z3Wrapper.hpp"
#endif

namespace ddsolve::smt {

/**
 * @brief Concept constraining SMT solver backend wrappers.
 *
 * Types satisfying this concept must provide expression types, factory
 * methods for arithmetic operations (sums, products, negation), comparison
 * construction, boolean constants, solver interaction (assertions, push/pop,
 * check-sat), and value retrieval.
 */
template <typename Wrapper>
concept SmtWrapper =
    requires { typename Wrapper::Expression; } &&
    requires(Wrapper wrapper, Wrapper::Expression expr1,
             Wrapper::Expression expr2) {
      { Wrapper::isRealValued() } -> std::convertible_to<bool>;
      { Wrapper::isIntValued() } -> std::convertible_to<bool>;
      {
        wrapper.mkReal("1/23")
      } -> std::convertible_to<typename Wrapper::Expression>;
      {
        wrapper.mkComparison(expr1, expr2,
                             ddsolve::expression::ComparisonType{})
      } -> std::convertible_to<typename Wrapper::Expression>;
      {
        wrapper.mkProduct({})
      } -> std::convertible_to<typename Wrapper::Expression>;
      {
        wrapper.mkProduct({expr1, expr2})
      } -> std::convertible_to<typename Wrapper::Expression>;
      {
        wrapper.mkSum({})
      } -> std::convertible_to<typename Wrapper::Expression>;
      {
        wrapper.mkSum({expr1, expr2})
      } -> std::convertible_to<typename Wrapper::Expression>;
      {
        wrapper.mkNegation(expr1)
      } -> std::convertible_to<typename Wrapper::Expression>;
      {
        wrapper.mkConst("variable_name")
      } -> std::convertible_to<typename Wrapper::Expression>;
      { wrapper.mkTrue() } -> std::convertible_to<typename Wrapper::Expression>;
      {
        wrapper.mkFalse()
      } -> std::convertible_to<typename Wrapper::Expression>;
      {
        wrapper.getAssertions()
      } -> std::convertible_to<std::vector<typename Wrapper::Expression>>;
      { wrapper.addAssertion(expr1) };
      { wrapper.push() };
      { wrapper.pop(1) };
      { wrapper.checkSat() } -> std::convertible_to<expression::SmtResult>;
      {
        wrapper.getValues({expr1, expr2})
      } -> std::convertible_to<std::vector<typename Wrapper::Expression>>;
    };

#if SMT_SOLVER == CVC5
/**
 * @brief Alias for the real-valued SMT solver wrapper (CVC5).
 *
 * Resolves to `Cvc5Wrapper<true>`, the CVC5 backend configured for
 * real-number arithmetic.
 */
using RealValuedWrapper = Cvc5Wrapper<true>;
/**
 * @brief Alias for the integer-valued SMT solver wrapper (CVC5).
 *
 * Resolves to `Cvc5Wrapper<false>`, the CVC5 backend configured for
 * integer arithmetic.
 */
using IntValuedWrapper = Cvc5Wrapper<false>;
#endif

#if SMT_SOLVER == Z3
/**
 * @brief Alias for the real-valued SMT solver wrapper (Z3).
 *
 * Resolves to `Z3Wrapper<true>`, the Z3 backend configured for
 * real-number arithmetic.
 */
using RealValuedWrapper = Z3Wrapper<true>;
/**
 * @brief Alias for the integer-valued SMT solver wrapper (Z3).
 *
 * Resolves to `Z3Wrapper<false>`, the Z3 backend configured for
 * integer arithmetic.
 */
using IntValuedWrapper = Z3Wrapper<false>;
#endif

static_assert(SmtWrapper<RealValuedWrapper> &&
              RealValuedWrapper::isRealValued());
static_assert(SmtWrapper<IntValuedWrapper>);

}  // namespace ddsolve::smt
