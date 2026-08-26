#ifndef Z3WRAPPER_HPP
#define Z3WRAPPER_HPP

/**
 * @file Z3Wrapper.hpp
 * @brief Z3 SMT solver wrapper for linear arithmetic.
 *
 * This header provides the Z3Wrapper template class, which wraps the Z3
 * C++ API to expose a uniform interface for building linear arithmetic
 * expressions and querying satisfiability.  It is parameterised by a boolean
 * template argument to select between real and integer theories.  Expressions
 * are stored in std::optional to handle cases where construction fails.
 *
 * @note The Doxygen-style comments in this file were generated with the help
 *       of AI (Qwen3.6-35B-A3B). They have been human-checked on a best-effort
 *       basis. No lines of logic in this file were written or modified by AI.
 */

#include <z3++.h>

#include <ranges>

#include "LinearTerm.hpp"

namespace ddsolve::smt {

/**
 * @brief Optional Z3 expression wrapper.
 *
 * Wraps a z3::expr in std::optional to handle cases where expression
 * construction may fail (e.g. when operand expressions are empty).
 */
using Z3Expression = std::optional<z3::expr>;

/**
 * @brief Z3 SMT solver wrapper for linear arithmetic.
 *
 * Provides a uniform interface for building linear arithmetic expressions
 * and querying satisfiability using the Z3 SMT solver. It is parameterised
 * by a boolean template argument to select between real and integer theories.
 *
 * @tparam real_valued  true for real arithmetic, false for integer arithmetic.
 */
template <bool real_valued>
class Z3Wrapper {
 public:
  /**
   * @brief Alias for the expression type (Z3Expression).
   */
  using Expression = Z3Expression;

  /**
   * @brief Check whether this wrapper represents real-valued arithmetic.
   * @return true if real_valued is true.
   */
  [[nodiscard]] static constexpr bool isRealValued() {
    return real_valued;
  }

  /**
   * @brief Check whether this wrapper represents integer-valued arithmetic.
   * @return true if real_valued is false.
   */
  [[nodiscard]] static constexpr bool isIntValued() {
    return !real_valued;
  }

  /**
   * @brief Create a real value from a string.
   *
   * @param s  The string representation of the real value.
   * @return A Z3Expression containing the real value, or empty on failure.
   */
  [[nodiscard]] Expression mkReal(const std::string& s) {
    return context_.real_val(s.c_str());
  }

  /**
   * @brief Create a comparison expression between two operands.
   *
   * @param lhs  The left-hand side expression.
   * @param rhs  The right-hand side expression.
   * @param type The comparison type (equality, inequality, less-than, etc.).
   * @return A Z3Expression containing the comparison, or empty if either
   *         operand is empty.
   */
  [[nodiscard]] Expression mkComparison(const Expression& lhs,
                                        const Expression& rhs,
                                        const expression::ComparisonType type) {
    if (lhs && rhs) {
      switch (type) {
        case expression::ComparisonType::kEquality:
          return *lhs == *rhs;
        case expression::ComparisonType::kInequality:
          return *lhs != *rhs;
        case expression::ComparisonType::kLessThan:
          return *lhs < *rhs;
        case expression::ComparisonType::kLessThanOrEqual:
          return *lhs <= *rhs;
        case expression::ComparisonType::kGreaterThan:
          return *lhs > *rhs;
        case expression::ComparisonType::kGreaterThanOrEqual:
          return *lhs >= *rhs;
      }
    }

    [[unlikely]] return {};
  }

  /**
   * @brief Create a product of expressions.
   *
   * Multiplies all expressions in @p factors. Returns 1 for an empty vector,
   * the single element for a one-element vector, and the product for two or
   * more elements. Returns empty if any operand is empty (for size >= 2).
   *
   * @param factors  The vector of expressions to multiply.
   * @return A Z3Expression containing the product, or empty on failure.
   */
  [[nodiscard]] Expression mkProduct(const std::vector<Expression>& factors) {
    if (factors.empty()) {
      return context_.real_val(1);
    }
    if (factors.size() == 1) {
      return factors[0];
    }
    if (!factors[0] || !factors[1]) {
      [[unlikely]] return {};
    }

    Expression rval{(*factors[0]) * (*factors[1])};
    for (const Expression& e : std::views::drop(factors, 2)) {
      if (!e) {
        [[unlikely]] return {};
      }
      rval = (*rval) * (*e);
    }
    return rval;
  }

  /**
   * @brief Create a sum of expressions.
   *
   * Adds all expressions in @p summands. Returns 0 for an empty vector,
   * the single element for a one-element vector, and the sum for two or
   * more elements. Returns empty if any operand is empty (for size >= 2).
   *
   * @param summands  The vector of expressions to add.
   * @return A Z3Expression containing the sum, or empty on failure.
   */
  [[nodiscard]] Expression mkSum(const std::vector<Expression>& summands) {
    if (summands.empty()) {
      return context_.real_val(0);
    }
    if (summands.size() == 1) {
      return summands[0];
    }

    if (!summands[0] || !summands[1]) {
      [[unlikely]] return {};
    }

    Expression rval{*summands[0] + *summands[1]};
    for (const Expression& e : std::views::drop(summands, 2)) {
      if (!e) {
        [[unlikely]] return {};
      }
      rval = (*rval) + (*e);
    }
    return rval;
  }

  /**
   * @brief Create a negation of an expression.
   *
   * @param expr  The expression to negate.
   * @return A Z3Expression containing the negated expression.
   */
  [[nodiscard]] Expression mkNegation(const Expression& expr) {
    return expr.transform([](const auto& e) { return !e; });
  }

  /**
   * @brief Create a constant variable expression.
   *
   * Creates a real constant if real_valued is true, otherwise an integer
   * constant.
   *
   * @param symbol  The symbol name for the constant.
   * @return A Z3Expression containing the constant, or empty on failure.
   */
  [[nodiscard]] Expression mkConst(const std::string& symbol) {
    if constexpr (real_valued) {
      return context_.real_const(symbol.c_str());
    }
    return context_.int_const(symbol.c_str());
  }

  /**
   * @brief Create a constant true expression.
   * @return A Z3Expression containing the constant true value.
   */
  [[nodiscard]] Expression mkTrue() {
    return context_.bool_val(true);
  }

  /**
   * @brief Create a constant false expression.
   * @return A Z3Expression containing the constant false value.
   */
  [[nodiscard]] Expression mkFalse() {
    return context_.bool_val(false);
  }

  /**
   * @brief Get the current assertions from the solver.
   * @return A vector of Z3Expression containing all current assertions.
   */
  [[nodiscard]] std::vector<Expression> getAssertions() const {
    auto&& assertions{solver_.assertions()};
    std::vector<Expression> rval{};
    rval.reserve(assertions.size());

    for (auto&& i : assertions) {
      rval.emplace_back(i);
    }

    return rval;
  }

  /**
   * @brief Add an assertion to the solver.
   *
   * @param expr  The expression to assert. If empty, no assertion is added.
   */
  void addAssertion(const Expression& expr) {
    if (expr) {
      solver_.add(*expr);
    }
  }

  /**
   * @brief Push the current solver state onto the assertion stack.
   */
  void push() {
    solver_.push();
  }

  /**
   * @brief Pop @p n levels from the assertion stack.
   *
   * @param n  The number of levels to pop (default: 1).
   */
  void pop(const uint32_t n = 1) {
    solver_.pop(n);
  }

  /**
   * @brief Check the satisfiability of the current assertions.
   * @return SmtResult indicating Sat, Unsat, or Unknown.
   */
  [[nodiscard]] ddsolve::expression::SmtResult checkSat() {
    const auto result{solver_.check()};

    if (result == z3::check_result::sat) {
      return ddsolve::expression::SmtResult::kSat;
    }
    if (result == z3::check_result::unsat) {
      return ddsolve::expression::SmtResult::kUnsat;
    }

    return ddsolve::expression::SmtResult::kUnknown;
  }

  /**
   * @brief Get the values of terms from the current model.
   *
   * Requires that checkSat() returned Sat. Evaluates each term in
   * the model produced by the solver.
   *
   * @param terms  The expressions to evaluate.
   * @return A vector of Z3Expression containing the evaluated values.
   */
  [[nodiscard]]
  std::vector<Expression> getValues(
      const std::vector<Expression>& terms) const {
    const auto model{solver_.get_model()};

    return std::views::transform(terms,
                                 [&model](const auto& i) {
                                   return i.transform([&model](const auto& v) {
                                     return model.eval(v);
                                   });
                                 }) |
           std::ranges::to<std::vector>();
  }

 private:
  z3::context context_;
  z3::solver solver_{context_};
};

}  // namespace ddsolve::smt

// TODO(DB) This is ugly
/**
 * @brief Stream output operator for Z3Expression.
 *
 * Writes the underlying expression to the output stream if present,
 * or writes "Error!" if the expression is empty.
 *
 * @param out  The output stream.
 * @param expr  The Z3Expression to write.
 * @return A reference to the output stream.
 */
inline std::ostream& operator<<(std::ostream& out,
                                const ddsolve::smt::Z3Expression& expr) {
  if (expr) {
    return out << *expr;
  }

  return out << "Error!";
}

#endif /* Z3WRAPPER_HPP */
