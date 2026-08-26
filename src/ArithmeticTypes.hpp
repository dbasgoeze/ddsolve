#ifndef ARITHMETICTYPES_HPP
#define ARITHMETICTYPES_HPP

/**
 * @file ArithmeticTypes.hpp
 * @brief Common arithmetic and SMT-related type definitions.
 *
 * This header defines shared enumerations and utility functions used
 * throughout the solver for comparison operations and SMT solver results.
 *
 * @note The Doxygen-style comments in this file were generated with the help
 *       of AI (Qwen3.6-35B-A3B). They have been human-checked on a best-effort
 *       basis. No lines of logic in this file were written or modified by AI.
 */

#include <cstdint>
#include <string>
namespace ddsolve::expression {

/**
 * @brief Types of comparison operations supported by the solver.
 *
 * These values are used to specify the kind of comparison when evaluating
 * arithmetic expressions or generating SMT constraints.
 */
enum class ComparisonType : uint8_t {
  kEquality = 0,
  kInequality,
  kLessThan,
  kLessThanOrEqual,
  kGreaterThan,
  kGreaterThanOrEqual,
};

/**
 * @brief Result of an SMT solver query.
 *
 * Mirrors the standard SMT solver return values: the problem is unsatisfiable,
 * satisfiable, or the solver could not determine a definitive answer.
 */
enum class SmtResult : uint8_t {
  kUnsat = 0,
  kSat,
  kUnknown,
};

/**
 * @brief Convert a boolean to an SMT result.
 *
 * @param b  The boolean value to convert.
 * @return @p SmtResult::kSat if @p b is true, @p SmtResult::kUnsat otherwise.
 */
constexpr SmtResult boolToResult(const bool b) {
  return b ? SmtResult::kSat : SmtResult::kUnsat;
}

/**
 * @brief Negate an SMT result.
 *
 * Inverts a satisfiability result: @p kSat becomes @p kUnsat and vice versa.
 * @p kUnknown is preserved.
 *
 * @param result  The SMT result to negate.
 * @return The negated SMT result.
 */
constexpr SmtResult flip(const SmtResult result) {
  switch (result) {
    case SmtResult::kUnsat:
      return SmtResult::kSat;
    case SmtResult::kSat:
      return SmtResult::kUnsat;
    default:
      return SmtResult::kUnknown;
  }
}

/**
 * @brief Convert an SMT result to its string representation.
 *
 * @param result  The SMT result to convert.
 * @return A human-readable string: "Sat", "Unsat", or "Unknown".
 */
constexpr std::string smtResultToStr(const SmtResult result) {
  switch (result) {
    case SmtResult::kUnsat:
      return "Unsat";
    case SmtResult::kSat:
      return "Sat";
    default:
      return "Unknown";
  }
}

}  // namespace ddsolve::expression

#endif /* ARITHMETICTYPES_HPP */
