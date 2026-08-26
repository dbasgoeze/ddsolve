#ifndef SMTTERM_HPP
#define SMTTERM_HPP

/**
 * @file SmtTerm.hpp
 * @brief SMT-based arithmetic terms and comparisons.
 *
 * This header defines the SmtTerm and SmtComparison classes, which wrap
 * SMT-LIB-formatted expressions as opaque string terms.  SmtTerm provides
 * factory methods for creating integer/real variables, constants, functions,
 * and arrays, as well as arithmetic operators and array operations.
 * SmtComparison wraps boolean combinations of SMT terms and supports
 * assumption management and decision procedures via the SMT solver.
 *
 * @note The Doxygen-style comments in this file were generated with the help
 *       of AI (Qwen3.6-35B-A3B). They have been human-checked on a best-effort
 *       basis. No lines of logic in this file were written or modified by AI.
 */

#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <vector>

#include "ArithmeticTypes.hpp"
#include "utils.hpp"

namespace ddsolve::expression {

class SmtComparison;

/**
 * @brief SMT-based arithmetic terms represented as opaque SMT-LIB strings.
 *
 * SmtTerm wraps SMT-LIB-formatted expressions as opaque string terms.
 * It provides factory methods for creating integer/real variables, constants,
 * functions, and arrays, as well as arithmetic operators and array operations.
 */
class SmtTerm {
 public:
  /**
   * @brief Alias for this type (SmtTerm itself).
   */
  using Scalar = SmtTerm;
  /**
   * @brief Alias for the variable type (SmtTerm itself).
   */
  using Variable = SmtTerm;

  /**
   * @brief Alias for the scalar type used throughout (SmtTerm itself).
   */
  using ScalarType = SmtTerm;
  /**
   * @brief Alias for the coefficient type (same as ScalarType).
   */
  using CoefficientType = SmtTerm;
  /**
   * @brief Shared pointer to a mutable coefficient.
   */
  using CoefficientPointer = std::shared_ptr<CoefficientType>;
  /**
   * @brief Shared pointer to a const coefficient.
   */
  using ConstCoefficientPointer = std::shared_ptr<const CoefficientType>;
  /**
   * @brief Shared pointer to a mutable scalar.
   */
  using ScalarPointer = std::shared_ptr<ScalarType>;
  /**
   * @brief Shared pointer to a const scalar.
   */
  using ConstScalarPointer = std::shared_ptr<const ScalarType>;

  /**
   * @brief Alias for the term type (SmtTerm itself).
   */
  using Term = SmtTerm;
  /**
   * @brief Shared pointer to a mutable term.
   */
  using TermPointer = std::shared_ptr<Term>;
  /**
   * @brief Shared pointer to a const term.
   */
  using ConstTermPointer = std::shared_ptr<const Term>;

  /**
   * @brief Alias for the comparison type (SmtComparison).
   */
  using Comparison = SmtComparison;
  /**
   * @brief Shared pointer to a mutable SmtComparison.
   */
  using ComparisonPointer = std::shared_ptr<SmtComparison>;
  /**
   * @brief Shared pointer to a const SmtComparison.
   */
  using ConstComparisonPointer = std::shared_ptr<const SmtComparison>;

  // TODO(DB) this does not look right
  // The map is broken
  /**
   * @brief Assign a unique numeric ID to a shared scalar pointer.
   *
   * Maintains a static map of scalar string representations to IDs for
   * efficient comparison. Returns an existing ID if the string has been
   * seen before, or assigns a new one otherwise.
   *
   * @param s  The shared scalar pointer to identify.
   * @return A unique non-negative ID for the given scalar pointer.
   */
  static uint64_t ScalarType_get_id(
      const std::shared_ptr<const ScalarType>& s) {
    static std::shared_mutex map_mutex{};
    static std::map<std::string, uint64_t> map{};

    const std::string str{s->str()};

    {
      std::shared_lock lock{map_mutex};
      const auto it{map.find(str)};
      if (it != map.end()) {
        return it->second;
      }
    }
    std::scoped_lock lock{map_mutex};
    const auto it{map.find(str)};
    if (!map.contains(str)) {
      const auto id = map.size();
      map[str] = id;
      return id;
    }
    return it->second;
  }

  /**
   * @brief Parse a scalar (SmtTerm) from a string.
   *
   * @param str  The string representation of the scalar.
   * @return The parsed SmtTerm.
   */
  static ScalarType scalarFromString(const std::string& str) {
    return mkReal(str);
  }

  /**
   * @brief Parse a coefficient (SmtTerm) from a string.
   *
   * @param str  The string representation of the coefficient.
   * @return The parsed SmtTerm.
   */
  static CoefficientType coefficientFromString(const std::string& str) {
    return mkReal(str);
  }

  /**
   * @brief Default constructor – creates an empty term.
   */
  SmtTerm() = default;
  /**
   * @brief Construct an SmtTerm from a string representation.
   *
   * @param term         The SMT-LIB-formatted string representation.
   * @param isSimplified Whether the term is already simplified.
   */
  explicit SmtTerm(std::string term, bool isSimplified = true)
      : term_{std::move(term)},
        hash_cache_{calc_hash()},
        isSimplified_{isSimplified} {}
  SmtTerm(const SmtTerm&) = default;
  SmtTerm(SmtTerm&&) = default;
  SmtTerm& operator=(const SmtTerm&) = default;
  SmtTerm& operator=(SmtTerm&&) = default;
  ~SmtTerm() = default;

  /**
   * @brief Create an integer variable term.
   * @param name  The variable name.
   * @return An SmtTerm representing an integer variable.
   */
  [[nodiscard]] static SmtTerm mkIntVar(const std::string& name);
  /**
   * @brief Create a real variable term.
   * @param name  The variable name.
   * @return An SmtTerm representing a real variable.
   */
  [[nodiscard]] static SmtTerm mkRealVar(const std::string& name);
  /**
   * @brief Create a real constant term.
   * @param value  The string representation of the real value.
   * @return An SmtTerm representing a real constant.
   */
  [[nodiscard]] static SmtTerm mkReal(const std::string& value);
  /**
   * @brief Create an integer constant term.
   * @param value  The string representation of the integer value.
   * @return An SmtTerm representing an integer constant.
   */
  [[nodiscard]] static SmtTerm mkInt(const std::string& value);

  /**
   * @brief Create a real function declaration term.
   * @param name  The function name.
   * @param arity The number of arguments.
   * @return An SmtTerm representing a real function declaration.
   */
  [[nodiscard]] static SmtTerm mkRealFunction(const std::string& name,
                                              unsigned int arity);
  /**
   * @brief Create an integer function declaration term.
   * @param name  The function name.
   * @param arity The number of arguments.
   * @return An SmtTerm representing an integer function declaration.
   */
  [[nodiscard]] static SmtTerm mkIntFunction(const std::string& name,
                                             unsigned int arity);
  /**
   * @brief Apply a function to a list of arguments.
   * @param arguments  The function arguments.
   * @return An SmtTerm representing the function application.
   */
  [[nodiscard]] SmtTerm applyFunction(
      const std::vector<SmtTerm>& arguments) const;

  /**
   * @brief Create an integer array term.
   * @param name  The array name.
   * @return An SmtTerm representing an integer array.
   */
  [[nodiscard]] static SmtTerm mkIntArray(const std::string& name);
  /**
   * @brief Create a real array term.
   * @param name  The array name.
   * @return An SmtTerm representing a real array.
   */
  [[nodiscard]] static SmtTerm mkRealArray(const std::string& name);
  /**
   * @brief Array store operation (update).
   *
   * Creates a new array identical to this one except at the given index,
   * where it has the given value.
   *
   * @param index  The index to store at.
   * @param value  The value to store.
   * @return An SmtTerm representing the updated array.
   */
  [[nodiscard]] SmtTerm arrayStore(const SmtTerm& index,
                                   const SmtTerm& value) const;
  /**
   * @brief Array select operation (read).
   * @param index  The index to read from.
   * @return An SmtTerm representing the value at the given index.
   */
  [[nodiscard]] SmtTerm arraySelect(const SmtTerm& index) const;

  /**
   * @brief Cast an integer term to real.
   * @return An SmtTerm representing the real cast of this term.
   */
  [[nodiscard]] SmtTerm toReal() const;
  /**
   * @brief Cast a real term to integer.
   * @return An SmtTerm representing the integer cast of this term.
   */
  [[nodiscard]] SmtTerm toInt() const;

  /**
   * @brief Division operator.
   * @param other  The divisor.
   * @return The quotient of this term divided by @p other.
   */
  [[nodiscard]] SmtTerm operator/(const SmtTerm& other) const;
  /**
   * @brief Multiplication operator.
   * @param other  The multiplier.
   * @return The product of this term and @p other.
   */
  [[nodiscard]] SmtTerm operator*(const SmtTerm& other) const;
  /**
   * @brief Addition operator.
   * @param other  The term to add.
   * @return The sum of this term and @p other.
   */
  [[nodiscard]] SmtTerm operator+(const SmtTerm& other) const;
  /**
   * @brief Subtraction operator.
   * @param other  The term to subtract.
   * @return The difference of this term and @p other.
   */
  [[nodiscard]] SmtTerm operator-(const SmtTerm& other) const;

  /**
   * @brief Scale this term by a scalar.
   * @param scalar  The scalar multiplier.
   * @return A new SmtTerm with this term multiplied by @p scalar.
   */
  [[nodiscard]] SmtTerm scale(const Scalar& scalar) const {
    return scalar * (*this);
  }

  /**
   * @brief Fused scale and add: scalar * this + other.
   * @param scalar  The scalar multiplier.
   * @param other   The term to add.
   * @return The result of scalar * this + other.
   */
  [[nodiscard]] SmtTerm fusedScaleAdd(const Scalar& scalar,
                                      const SmtTerm& other) const {
    return scalar * (*this) + other;
  }

  /**
   * @brief Substitute a variable with another term.
   * @param variable  The variable to substitute.
   * @param other     The term to substitute with.
   * @return A new SmtTerm with the substitution applied.
   */
  [[nodiscard]] SmtTerm substitute(const Variable& variable,
                                   const SmtTerm& other) const;

  /**
   * @brief Simplify this term.
   * @return A simplified copy of this term.
   */
  [[nodiscard]] SmtTerm simplify() const;

  /**
   * @brief Equality comparison between two SmtTerms.
   * @param other  The term to compare with.
   * @return true if the underlying string representations are equal.
   */
  [[nodiscard]] bool operator==(const SmtTerm& other) const {
    return term_ == other.term_;
  }

  /**
   * @brief Lexicographic less-than comparison between two SmtTerms.
   * @param other  The term to compare with.
   * @return true if this term's string representation is lexicographically
   *         less than @p other's.
   */
  [[nodiscard]] bool operator<(const SmtTerm& other) const {
    return term_ < other.term_;
  }
  /**
   * @brief Convert this term to a string representation.
   * @return The SMT-LIB-formatted string of this term.
   */
  [[nodiscard]] std::string str() const {
    return term_;
  }

  /**
   * @brief Get the precomputed hash of this term.
   * @return The cached hash value.
   */
  [[nodiscard]] uint64_t hash() const {
    return hash_cache_;
  }

  /**
   * @brief Check whether this term has been simplified.
   * @return true if the term is marked as simplified.
   */
  [[nodiscard]] bool isSimplified() const {
    return isSimplified_;
  }

 private:
  [[nodiscard]] uint64_t calc_hash() const {
    return SpanHash{}(std::span{term_.data(), term_.size()});
  }

  std::string term_;
  uint64_t hash_cache_{};
  bool isSimplified_{};

  friend class SmtComparison;
};

/**
 * @brief A comparison between SMT terms with boolean combination support.
 *
 * SmtComparison wraps a boolean combination of SMT terms and supports
 * assumption management and decision procedures via the SMT solver.
 */
class SmtComparison {
 public:
  /**
   * @brief Default constructor – creates an empty comparison.
   */
  SmtComparison();
  /**
   * @brief Construct a comparison from a string term.
   * @param term  The string representation of the comparison.
   */
  explicit SmtComparison(std::string term) : term_{std::move(term)} {}
  /**
   * @brief Construct a comparison from an SmtTerm.
   * @param term  The SmtTerm to compare.
   */
  explicit SmtComparison(SmtTerm term) : term_{std::move(term)} {}

  /**
   * @brief Construct a comparison from a term and comparison type.
   * @param term  The term to compare.
   * @param type  The comparison type (equality, less-than, etc.).
   */
  SmtComparison(const SmtTerm& term, const ComparisonType& type) {}

  /**
   * @brief Construct a comparison from two terms and a comparison type.
   * @param lhs  The left-hand side term.
   * @param rhs  The right-hand side term.
   * @param type The comparison type.
   */
  SmtComparison(const SmtTerm& lhs, const SmtTerm& rhs,
                const ComparisonType& type);

  SmtComparison(const SmtComparison&) = default;
  SmtComparison(SmtComparison&&) = default;
  SmtComparison& operator=(const SmtComparison&) = default;
  SmtComparison& operator=(SmtComparison&&) = default;
  ~SmtComparison() = default;

  /**
   * @brief Equality comparison between two comparisons.
   * @param other  The comparison to compare with.
   * @return true if the underlying string representations are equal.
   */
  [[nodiscard]] bool operator==(const SmtComparison& other) const {
    return term_ == other.term_;
  }

  /**
   * @brief Lexicographic less-than comparison between two comparisons.
   * @param other  The comparison to compare with.
   * @return true if this comparison's string representation is
   *         lexicographically less than @p other's.
   */
  [[nodiscard]] bool operator<(const SmtComparison& other) const {
    return term_ < other.term_;
  }

  /**
   * @brief Substitute a variable with a term in this comparison.
   * @param var   The variable to substitute.
   * @param other The term to substitute with.
   * @return A new SmtComparison with the substitution applied.
   */
  [[nodiscard]] SmtComparison substitute(const SmtTerm::Variable& var,
                                         const SmtTerm& other) const;

  /**
   * @brief A comparison paired with a parity bit.
   *
   * Used when encoding parity constraints alongside arithmetic comparisons.
   */
  struct WithParity {
    bool parity;
    std::shared_ptr<const SmtComparison> comparison;
  };

  /**
   * @brief Perform trivial satisfiability checks.
   * @return SmtResult indicating satisfiability (always kUnknown in the
   *         base implementation).
   */
  [[nodiscard]] SmtResult trivialCheck() const {
    return SmtResult::kUnknown;
  }

  /**
   * @brief Push an assumption onto the assumption stack.
   * @param assumption  The assumption (comparison with parity) to push.
   */
  static void pushAssumption(const WithParity& assumption);
  /**
   * @brief Pop the most recent assumption from the assumption stack.
   */
  static void popAssumption();

  /**
   * @brief Decide satisfiability for a conjunction of comparisons.
   * @param conjunction  The set of comparisons (with parity) to decide.
   * @param printModel   Whether to print a satisfying model.
   * @return SmtResult indicating satisfiability.
   */
  [[nodiscard]] static SmtResult decide(
      const std::vector<WithParity>& conjunction, bool printModel);

  /**
   * @brief Shut down static resources used by the decision procedures.
   */
  static void shutdown();

  /**
   * @brief Get the hash of this comparison.
   * @return The hash of the underlying term.
   */
  [[nodiscard]] uint64_t hash() const {
    return term_.hash();
  }

  /**
   * @brief Convert this comparison to a string representation.
   * @return The SMT-LIB-formatted string of this comparison.
   */
  [[nodiscard]] std::string str() const {
    return term_.str();
  }

  /**
   * @brief Get the underlying term of this comparison.
   * @return A const reference to the stored SmtTerm.
   */
  [[nodiscard]] const SmtTerm& getTerm() const {
    return term_;
  }

  /**
   * @brief Simplify this comparison.
   * @return A simplified copy of this comparison.
   */
  [[nodiscard]] SmtComparison simplify() const {
    return SmtComparison{term_.simplify()};
  }

  /**
   * @brief Logical AND with another comparison.
   * @param other  The other comparison.
   * @return The conjunction of this comparison and @p other.
   */
  [[nodiscard]] SmtComparison operator&(const SmtComparison& other) const;
  /**
   * @brief Logical OR with another comparison.
   * @param other  The other comparison.
   * @return The disjunction of this comparison and @p other.
   */
  [[nodiscard]] SmtComparison operator|(const SmtComparison& other) const;
  /**
   * @brief Negation of this comparison.
   * @return The negation of this comparison.
   */
  [[nodiscard]] SmtComparison operator~() const;

  /**
   * @brief Create a universally quantified comparison over integer variables.
   * @param var   The integer variable to quantify over.
   * @param expr  The comparison expression.
   * @return An SmtComparison representing ∀ var . expr.
   */
  [[nodiscard]] static SmtComparison mkForallInt(const SmtTerm& var,
                                                 const SmtComparison& expr);
  /**
   * @brief Create a universally quantified comparison over real variables.
   * @param var   The real variable to quantify over.
   * @param expr  The comparison expression.
   * @return An SmtComparison representing ∀ var . expr.
   */
  [[nodiscard]] static SmtComparison mkForallReal(const SmtTerm& var,
                                                  const SmtComparison& expr);

 private:
  SmtTerm term_;
};

}  // namespace ddsolve::expression

#endif /* SMTTERM_HPP */
