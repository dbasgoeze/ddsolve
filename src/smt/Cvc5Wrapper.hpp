#ifndef CVC5WRAPPER_HPP
#define CVC5WRAPPER_HPP

#include <cvc5/cvc5.h>

#include <stdexcept>
#include <iostream>

#include "ArithmeticTypes.hpp"

namespace ddsolve::smt {

static constexpr cvc5::Kind cvc5TranslateToKind(
    const ddsolve::expression::ComparisonType type) {
  switch (type) {
    case ddsolve::expression::ComparisonType::kEquality:
      return cvc5::Kind::EQUAL;
    case ddsolve::expression::ComparisonType::kInequality:
      return cvc5::Kind::DISTINCT;
    case ddsolve::expression::ComparisonType::kLessThan:
      return cvc5::Kind::LT;
    case ddsolve::expression::ComparisonType::kLessThanOrEqual:
      return cvc5::Kind::LEQ;
    case ddsolve::expression::ComparisonType::kGreaterThan:
      return cvc5::Kind::GT;
    case ddsolve::expression::ComparisonType::kGreaterThanOrEqual:
      return cvc5::Kind::GEQ;

    [[unlikely]] default:
      return cvc5::Kind::UNDEFINED_KIND;
  }
}

template <bool real_valued>
class Cvc5Wrapper {
 public:
  using Expression = cvc5::Term;

  Cvc5Wrapper() {
    if constexpr (real_valued) {
      solver_.setLogic("QF_LRA");
    } else {
      solver_.setLogic("QF_LIA");
    }

    // solver_.setOption("output", "pre-asserts");
    solver_.setOption("produce-models", "true");
    solver_.setOption("tlimit-per", "5000");
  }

  [[nodiscard]] static constexpr bool isRealValued() {
    return real_valued;
  }

  [[nodiscard]] static constexpr bool isIntValued() {
    return !real_valued;
  }

  [[nodiscard]] Expression mkReal(const std::string &s) {
    return term_manager_.mkReal(s);
  }

  [[nodiscard]] Expression mkComparison(const Expression &lhs,
                                        const Expression &rhs,
                                        const expression::ComparisonType type) {
    return term_manager_.mkTerm(cvc5TranslateToKind(type), {lhs, rhs});
  }

  [[nodiscard]] Expression mkProduct(const std::vector<Expression> &factors) {
    if (factors.empty()) {
      return term_manager_.mkReal(1);
    }
    if (factors.size() == 1) {
      return factors[0];
    }
    return term_manager_.mkTerm(cvc5::Kind::MULT, factors);
  }

  [[nodiscard]] Expression mkSum(const std::vector<Expression> &summands) {
    if (summands.empty()) {
      return term_manager_.mkReal(0);
    }
    if (summands.size() == 1) {
      return summands[0];
    }

    return term_manager_.mkTerm(cvc5::Kind::ADD, summands);
  }

  [[nodiscard]] Expression mkNegation(const Expression &expr) {
    return term_manager_.mkTerm(cvc5::Kind::NOT, {expr});
  }

  [[nodiscard]] Expression mkConst(const std::string &symbol) {
    if constexpr (real_valued) {
      return term_manager_.mkConst(term_manager_.getRealSort(), symbol);
    }
    return term_manager_.mkConst(term_manager_.getIntegerSort(), symbol);
  }

  [[nodiscard]] Expression mkTrue() {
    return term_manager_.mkTrue();
  }

  [[nodiscard]] Expression mkFalse() {
    return term_manager_.mkFalse();
  }

  [[nodiscard]] std::vector<Expression> getAssertions() const {
    return solver_.getAssertions();
  }

  void addAssertion(const Expression &expr) {
    solver_.assertFormula(expr);
  }

  void push() {
    solver_.push();
  }

  void pop(const uint32_t n = 1) {
    solver_.pop(n);
  }

  [[nodiscard]] ddsolve::expression::SmtResult checkSat() {
    auto &&result{solver_.checkSat()};

    if (result.isSat()) {
      return ddsolve::expression::SmtResult::kSat;
    }
    if (result.isUnsat()) {
      return ddsolve::expression::SmtResult::kUnsat;
    }

    return ddsolve::expression::SmtResult::kUnknown;
  }

  [[nodiscard]]
  std::vector<Expression> getValues(
      const std::vector<Expression> &terms) const {
    return solver_.getValue(terms);
  }

 private:
  cvc5::TermManager term_manager_;
  cvc5::Solver solver_{term_manager_};
};

}  // namespace ddsolve::smt

#endif /* CVC5WRAPPER_HPP */
