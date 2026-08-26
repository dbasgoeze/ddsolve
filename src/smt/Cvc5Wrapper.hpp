#ifndef CVC5WRAPPER_HPP
#define CVC5WRAPPER_HPP

/**
 * @file Cvc5Wrapper.hpp
 * @brief CVC5 SMT solver wrapper for linear arithmetic.
 *
 * This header provides the Cvc5Wrapper template class, which wraps the
 * CVC5 API to expose a uniform interface for building linear arithmetic
 * expressions and querying satisfiability.  It is parameterised by a
 * boolean template argument to select between linear real (QF_LRA) and
 * linear integer (QF_LIA) theories.
 *
 * @note The Doxygen-style comments in this file were generated with the help
 *       of AI (Qwen3.6-35B-A3B). They have been human-checked on a best-effort
 *       basis. No lines of logic in this file were written or modified by AI.
 */

#include <cvc5/cvc5.h>

#include <iostream>
#include <stdexcept>

#include "ArithmeticTypes.hpp"

namespace ddsolve::smt {

/**
 * @brief Translate a ComparisonType to the corresponding cvc5::Kind.
 *
 * Maps the domain's comparison type enumeration (equality, inequality,
 * less-than, etc.) to the matching cvc5::Kind used for building
 * comparison terms in the CVC5 SMT solver.
 *
 * @param type  The comparison type from the ddsolve expression namespace.
 * @return The corresponding cvc5::Kind.
 */
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

/**
 * @brief Wrapper around the CVC5 SMT solver for linear arithmetic.
 *
 * This template class provides a uniform interface for building linear
 * arithmetic expressions (sums, products, comparisons, negations) and
 * querying the CVC5 SMT solver for satisfiability. It is parameterised
 * by a boolean template argument to select between the linear real
 * (QF_LRA) and linear integer (QF_LIA) theories.
 *
 * @tparam real_valued  true for QF_LRA (reals), false for QF_LIA (integers).
 */
template <bool real_valued>
class Cvc5Wrapper {
 public:
  /**
   * @brief Alias for the CVC5 expression (term) type.
   */
  using Expression = cvc5::Term;

  /**
   * @brief Construct a Cvc5Wrapper, initialising the solver with
   *        the appropriate QF_LRA or QF_LIA logic.
   */
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

  /**
   * @brief Check whether this wrapper is configured for real-valued arithmetic.
   * @return true if the solver uses QF_LRA (real numbers).
   */
  [[nodiscard]] static constexpr bool isRealValued() {
    return real_valued;
  }

  /**
   * @brief Check whether this wrapper is configured for integer-valued
   * arithmetic.
   * @return true if the solver uses QF_LIA (integers).
   */
  [[nodiscard]] static constexpr bool isIntValued() {
    return !real_valued;
  }

  /**
   * @brief Create a real constant from a string representation.
   * @param s  The string representation of the real number.
   * @return A CVC5 term representing the real constant.
   */
  [[nodiscard]] Expression mkReal(const std::string& s) {
    return term_manager_.mkReal(s);
  }

  /**
   * @brief Create a comparison term between two expressions.
   * @param lhs  The left-hand side expression.
   * @param rhs  The right-hand side expression.
   * @param type The comparison type (equality, less-than, etc.).
   * @return A CVC5 term representing the comparison.
   */
  [[nodiscard]] Expression mkComparison(const Expression& lhs,
                                        const Expression& rhs,
                                        const expression::ComparisonType type) {
    return term_manager_.mkTerm(cvc5TranslateToKind(type), {lhs, rhs});
  }

  /**
   * @brief Create a product term from a list of factors.
   * @param factors  The vector of factors to multiply.
   * @return A CVC5 term representing the product. An empty vector yields 1,
   *         and a single element yields that element unchanged.
   */
  [[nodiscard]] Expression mkProduct(const std::vector<Expression>& factors) {
    if (factors.empty()) {
      return term_manager_.mkReal(1);
    }
    if (factors.size() == 1) {
      return factors[0];
    }
    return term_manager_.mkTerm(cvc5::Kind::MULT, factors);
  }

  /**
   * @brief Create a sum term from a list of summands.
   * @param summands  The vector of summands to add.
   * @return A CVC5 term representing the sum. An empty vector yields 0,
   *         and a single element yields that element unchanged.
   */
  [[nodiscard]] Expression mkSum(const std::vector<Expression>& summands) {
    if (summands.empty()) {
      return term_manager_.mkReal(0);
    }
    if (summands.size() == 1) {
      return summands[0];
    }

    return term_manager_.mkTerm(cvc5::Kind::ADD, summands);
  }

  /**
   * @brief Create a negation term.
   * @note This uses the boolean NOT operator (cvc5::Kind::NOT),
   *       appropriate for negating boolean expressions.
   * @param expr  The expression to negate.
   * @return A CVC5 term representing the negation of @p expr.
   */
  [[nodiscard]] Expression mkNegation(const Expression& expr) {
    return term_manager_.mkTerm(cvc5::Kind::NOT, {expr});
  }

  /**
   * @brief Create a constant variable with the given symbol name.
   * @param symbol  The symbol name for the variable.
   * @return A CVC5 term representing the constant variable.
   *         If real_valued is true, the variable is real-typed;
   *         otherwise it is integer-typed.
   */
  [[nodiscard]] Expression mkConst(const std::string& symbol) {
    if constexpr (real_valued) {
      return term_manager_.mkConst(term_manager_.getRealSort(), symbol);
    }
    return term_manager_.mkConst(term_manager_.getIntegerSort(), symbol);
  }

  /**
   * @brief Create a constant true expression.
   * @return A CVC5 term representing the boolean constant true.
   */
  [[nodiscard]] Expression mkTrue() {
    return term_manager_.mkTrue();
  }

  /**
   * @brief Create a constant false expression.
   * @return A CVC5 term representing the boolean constant false.
   */
  [[nodiscard]] Expression mkFalse() {
    return term_manager_.mkFalse();
  }

  /**
   * @brief Retrieve all asserted formulas.
   * @return A vector of all currently asserted expressions.
   */
  [[nodiscard]] std::vector<Expression> getAssertions() const {
    return solver_.getAssertions();
  }

  /**
   * @brief Assert a formula into the solver.
   * @param expr  The expression to assert.
   */
  void addAssertion(const Expression& expr) {
    solver_.assertFormula(expr);
  }

  /**
   * @brief Push a new scope onto the solver.
   *
   * Subsequent assertions will be contained within this scope and
   * can be removed by calling pop().
   */
  void push() {
    solver_.push();
  }

  /**
   * @brief Pop scopes from the solver, removing associated assertions.
   * @param n  The number of scopes to pop (default 1).
   */
  void pop(const uint32_t n = 1) {
    solver_.pop(n);
  }

  /**
   * @brief Check the satisfiability of the current assertions.
   * @return SmtResult indicating whether the assertions are satisfiable,
   *         unsatisfiable, or unknown.
   */
  [[nodiscard]] ddsolve::expression::SmtResult checkSat() {
    auto&& result{solver_.checkSat()};

    if (result.isSat()) {
      return ddsolve::expression::SmtResult::kSat;
    }
    if (result.isUnsat()) {
      return ddsolve::expression::SmtResult::kUnsat;
    }

    return ddsolve::expression::SmtResult::kUnknown;
  }

  /**
   * @brief Get the values of the given terms under the current model.
   * @param terms  The terms to evaluate.
   * @return A vector of evaluated term values.
   */
  [[nodiscard]]
  std::vector<Expression> getValues(
      const std::vector<Expression>& terms) const {
    return solver_.getValue(terms);
  }

 private:
  cvc5::TermManager term_manager_;
  cvc5::Solver solver_{term_manager_};
};

}  // namespace ddsolve::smt

#endif /* CVC5WRAPPER_HPP */
