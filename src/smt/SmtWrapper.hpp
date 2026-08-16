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
using RealValuedWrapper = Cvc5Wrapper<true>;
using IntValuedWrapper = Cvc5Wrapper<false>;
#endif

#if SMT_SOLVER == Z3
using RealValuedWrapper = Z3Wrapper<true>;
using IntValuedWrapper = Z3Wrapper<false>;
#endif

static_assert(SmtWrapper<RealValuedWrapper> &&
              RealValuedWrapper::isRealValued());
static_assert(SmtWrapper<IntValuedWrapper>);

}  // namespace ddsolve::smt
