#ifndef LINEARTERM_HPP
#define LINEARTERM_HPP

/**
 * @file LinearTerm.hpp
 * @brief Linear arithmetic terms and comparisons over rational coefficients.
 *
 * This header defines the LinearTerm class, which represents a linear
 * combination of variables with rational (GMP mpq_class) coefficients,
 * as well as the Product struct and FinityType enumeration used to
 * represent individual monomials and infinite values.  It also defines
 * LinearComparison, which encodes a comparison between a linear term
 * and zero with support for trivial satisfiability checking.
 *
 * @note The Doxygen-style comments in this file were generated with the help
 *       of AI (Qwen3.6-35B-A3B). They have been human-checked on a best-effort
 *       basis. No lines of logic in this file were written or modified by AI.
 */

#include <gmpxx.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <ostream>
#include <ranges>
#include <shared_mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "ArithmeticTypes.hpp"
#include "utils.hpp"

namespace ddsolve::expression {
using VariableId = int;

class LinearComparison;

/**
 * @brief A product of a variable and a rational coefficient.
 *
 * Represents a single monomial in a linear term, consisting of a variable ID
 * and its corresponding rational (GMP mpq_class) coefficient.
 */
struct Product {
  VariableId variable{};
  mpq_class coefficent;
};

/**
 * @brief Enumeration for term finity type.
 *
 * Specifies whether a linear term represents a finite rational value,
 * positive infinity, or negative infinity.
 */
enum class FinityType : uint8_t {
  kFinite = 0,
  kNegativeInfinite,
  kPositiveInfinite,
};

/**
 * @brief A linear term with rational (GMP mpq_class) coefficients.
 *
 * Represents a linear combination of variables with rational coefficients,
 * as well as special infinite values (positive and negative infinity).
 * The term is internally stored as a sorted vector of Product structs,
 * with zero-coefficient products removed during canonicalization.
 *
 * Supports arithmetic operations (addition, subtraction, scaling),
 * substitution of variables with other terms, and hashing for use in
 * unordered containers.
 */
class LinearTerm {
 public:
  /**
   * @brief The scalar/coefficient type used throughout (rational numbers).
   */
  using ScalarType = mpq_class;
  /**
   * @brief Alias for the coefficient type (same as ScalarType).
   */
  using CoefficientType = mpq_class;
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
   * @brief Alias for the variable identifier type.
   */
  using Variable = VariableId;
  /**
   * @brief Alias for this type (LinearTerm itself).
   */
  using Term = LinearTerm;
  /**
   * @brief Shared pointer to a LinearTerm.
   */
  using TermPointer = std::shared_ptr<LinearTerm>;
  /**
   * @brief Shared pointer to a const LinearTerm.
   */
  using ConstTermPointer = std::shared_ptr<const LinearTerm>;

  /**
   * @brief Alias for the comparison type (LinearComparison).
   */
  using Comparison = LinearComparison;
  /**
   * @brief Shared pointer to a LinearComparison.
   */
  using ComparisonPointer = std::shared_ptr<LinearComparison>;
  /**
   * @brief Shared pointer to a const LinearComparison.
   */
  using ConstComparisonPointer = std::shared_ptr<const LinearComparison>;

  /**
   * @brief Assign a unique numeric ID to a shared scalar pointer.
   *
   * Maintains a static map of scalar pointers to IDs for efficient
   * comparison. Returns an existing ID if the pointer has been seen
   * before, or assigns a new one otherwise.
   *
   * @param s  The shared scalar pointer to identify.
   * @return A unique non-negative ID for the given scalar pointer.
   */
  static uint64_t ScalarType_get_id(
      const std::shared_ptr<const ScalarType>& s) {
    static std::shared_mutex map_mutex{};
    static std::map<std::shared_ptr<const ScalarType>, uint64_t,
                    indirection_less>
        map{};

    {
      std::shared_lock lock{map_mutex};
      const auto it{map.find(s)};
      if (it != map.end()) {
        return it->second;
      }
    }
    std::scoped_lock lock{map_mutex};
    const auto it{map.find(s)};
    if (!map.contains(s)) {
      const auto id = map.size();
      map[s] = id;
      return id;
    }
    return it->second;
  }

  /**
   * @brief Parse a scalar (rational number) from a string.
   *
   * @param str  The string representation of the scalar (e.g. "3/4").
   * @return The parsed ScalarType in canonical form.
   */
  static ScalarType scalarFromString(const std::string& str) {
    ScalarType scalar{str};
    scalar.canonicalize();
    return scalar;
  }

  /**
   * @brief Parse a coefficient (rational number) from a string.
   *
   * @param str  The string representation of the coefficient (e.g. "3/4").
   * @return The parsed CoefficientType in canonical form.
   */
  static CoefficientType coefficientFromString(const std::string& str) {
    CoefficientType coefficent{str};
    coefficent.canonicalize();
    return coefficent;
  }

  /**
   * @brief Default constructor – creates a zero term.
   *
   * Constructs a LinearTerm representing the value zero, stored as
   * a single empty Product.
   */
  LinearTerm() : sum_{Product{}}, hash_cache_{calc_hash()} {}

  /**
   * @brief Construct a LinearTerm from a vector of products.
   *
   * Canonicalizes the term by sorting products by variable ID and
   * removing zero-coefficient entries. Throws if the same variable
   * appears more than once.
   *
   * @param sum  The vector of products representing the term.
   */
  explicit LinearTerm(std::vector<Product> sum) : sum_{std::move(sum)} {
    if (sum_.empty()) {
      sum_.emplace_back();
    }

    canonicalize();
    if (std::adjacent_find(sum_.begin(), sum_.end(),
                           [](const auto& a, const auto& b) {
                             return a.variable == b.variable;
                           }) != sum_.end()) {
      throw std::invalid_argument(
          "Tried to construct LinearTerm with multiple occurences"
          "of the same variable");
    }

    hash_cache_ = calc_hash();
  }

  /**
   * @brief Construct an infinite LinearTerm.
   *
   * @param finity_type  The finity type (positive or negative infinity).
   */
  explicit LinearTerm(FinityType finity_type)
      : finity_{finity_type}, hash_cache_{calc_hash()} {}

  /**
   * @brief Scale all coefficients by a scalar.
   *
   * @param scalar  The scalar multiplier.
   * @return A new LinearTerm with all coefficients multiplied by @p scalar.
   *         If this term is infinite, returns a copy unchanged.
   */
  [[nodiscard]] LinearTerm scale(const ScalarType scalar) const {
    if (!isFinite()) {
      return *this;
    }

    decltype(sum_) tmp{std::views::transform(
                           sum_,
                           [&scalar](auto const& i) {
                             return Product{i.variable, scalar * i.coefficent};
                           }) |
                       std::ranges::to<std::vector>()};

    return LinearTerm{std::move(tmp)};
  }

  /**
   * @brief Compute scalar * this + other (fused scale and add).
   *
   * Merges two sorted product lists in linear time. Handles infinite
   * finity types correctly, including canceling positive and negative
   * infinity.
   *
   * @param scalar  The scalar multiplier for this term.
   * @param other   The term to add.
   * @return The result of scalar * this + other.
   */
  [[nodiscard]] LinearTerm fusedScaleAdd(const ScalarType& scalar,
                                         const LinearTerm& other) const {
    if (scalar == 0) {
      return LinearTerm{};
    }

    const auto new_finity{scaleFinity(scalar)};
    if (new_finity == FinityType::kNegativeInfinite) {
      if (other.finity_ == FinityType::kPositiveInfinite) {
        return LinearTerm{};
      }

      return LinearTerm{new_finity};
    }
    if (new_finity == FinityType::kPositiveInfinite) {
      if (other.finity_ == FinityType::kNegativeInfinite) {
        return LinearTerm{};
      }
      return LinearTerm{new_finity};
    }

    if (!other.isFinite()) {
      return other;
    }

    auto it1 = sum_.begin();
    auto it2 = other.sum_.begin();

    decltype(sum_) new_sum{};
    new_sum.reserve(size() + other.size());

    while (it1 != sum_.end() || it2 != other.sum_.end()) {
      if (it1 != sum_.end() && it2 != other.sum_.end()) {
        if (it1->variable < it2->variable) {
          new_sum.push_back({it1->variable, scalar * it1->coefficent});
          ++it1;
        } else if (it1->variable > it2->variable) {
          new_sum.push_back({it2->variable, it2->coefficent});
          ++it2;
        } else {
          new_sum.push_back(Product{
              .variable = it1->variable,
              .coefficent = scalar * it1->coefficent + it2->coefficent});
          ++it1;
          ++it2;
        }
      } else if (it1 != sum_.end()) {
        new_sum.push_back({Product{.variable = it1->variable,
                                   .coefficent = scalar * it1->coefficent}});
        ++it1;
      } else {
        new_sum.push_back(*it2);
        ++it2;
      }
    }

    return LinearTerm{std::move(new_sum)};
  }

  /**
   * @brief Add two linear terms.
   *
   * @param other  The term to add.
   * @return The sum of this term and @p other.
   */
  [[nodiscard]] LinearTerm operator+(const LinearTerm& other) const {
    return fusedScaleAdd(ScalarType{1}, other);
  }

  /**
   * @brief Subtract another linear term.
   *
   * @param other  The term to subtract.
   * @return The difference of this term and @p other.
   */
  LinearTerm operator-(const LinearTerm& other) const {
    return other.fusedScaleAdd(ScalarType{-1}, *this);
  }

  /**
   * @brief Equality comparison between two linear terms.
   *
   * @param other  The term to compare with.
   * @return true if both terms have the same hash, size, finity, and
   *         identical products.
   */
  [[nodiscard]] bool operator==(const LinearTerm& other) const {
    if (hash() != other.hash() || size() != other.size() ||
        finity_ != other.finity_) {
      return false;
    }

    if (!isFinite()) {
      return true;
    }

    return std::ranges::all_of(std::views::zip(sum_, other.sum_),
                               [](const auto& p) {
                                 const auto& [lhs, rhs] = p;
                                 return lhs.variable == rhs.variable &&
                                        lhs.coefficent == rhs.coefficent;
                               });
  }

  /**
   * @brief Lexicographic less-than comparison between two linear terms.
   *
   * Compares first by size, then by finity, then element-wise by
   * variable ID and coefficient.
   *
   * @param other  The term to compare with.
   * @return true if this term is strictly less than @p other.
   */
  [[nodiscard]] bool operator<(const LinearTerm& other) const {
    if (size() != other.size()) {
      return size() < other.size();
    }

    if (finity_ != other.finity_) {
      return finity_ < other.finity_;
    }

    for (const auto& [lhs, rhs] : std::views::zip(sum_, other.sum_)) {
      if (lhs.variable != rhs.variable || lhs.coefficent != rhs.coefficent) {
        return lhs.variable < rhs.variable || (lhs.variable == rhs.variable &&
                                               lhs.coefficent < rhs.coefficent);
      }
    }
    return false;
  }

  /**
   * @brief Substitute a variable with another term.
   *
   * Replaces all occurrences of @p variable with the linear term @p other,
   * distributing coefficients as needed. If this term is infinite, returns
   * a copy unchanged.
   *
   * @param variable  The variable ID to substitute.
   * @param other     The term to substitute with.
   * @return A new LinearTerm with the substitution applied.
   */
  [[nodiscard]] LinearTerm substitute(const VariableId variable,
                                      const LinearTerm& other) const {
    if (!isFinite()) {
      return *this;
    }

    auto it{getProductFromVariable(variable)};
    if (it != sum_.end()) {
      const auto coefficent{it->coefficent};
      auto it1 = sum_.begin();
      auto it2 = other.sum_.begin();

      decltype(sum_) new_sum{};
      new_sum.reserve(size() + other.size());

      while (it1 != sum_.end() || it2 != other.sum_.end()) {
        if (it1 != sum_.end() && it1->variable == variable) {
          ++it1;
          continue;
        }

        if (it1 != sum_.end() && it2 != other.sum_.end()) {
          if (it1->variable < it2->variable) {
            new_sum.push_back(*it1);
            ++it1;
          } else if (it1->variable > it2->variable) {
            new_sum.push_back({it2->variable, coefficent * it2->coefficent});
            ++it2;
          } else {
            new_sum.push_back(Product{
                .variable = it1->variable,
                .coefficent = it1->coefficent + coefficent * it2->coefficent});
            ++it1;
            ++it2;
          }
        } else if (it1 != sum_.end()) {
          new_sum.push_back(*it1);
          ++it1;
        } else {
          new_sum.push_back(
              Product(it2->variable, coefficent * it2->coefficent));
          ++it2;
        }
      }

      // TODO(DB): find out if std::move was needed
      return LinearTerm{std::move(new_sum)};
    }

    return *this;
  }

  /**
   * @brief Get the number of products in this term.
   * @return The number of monomials (products) in the term.
   */
  [[nodiscard]] size_t size() const {
    return sum_.size();
  }

  /**
   * @brief Get an iterator to the beginning of the products.
   * @return A const iterator to the first product.
   */
  [[nodiscard]] auto begin() const {
    return sum_.begin();
  }

  /**
   * @brief Get an iterator past the end of the products.
   * @return A const iterator past the last product.
   */
  [[nodiscard]] auto end() const {
    return sum_.end();
  }

  /**
   * @brief Check whether this term is finite.
   * @return true if the finity type is kFinite.
   */
  [[nodiscard]] bool isFinite() const {
    return finity_ == FinityType::kFinite;
  }

  /**
   * @brief Get the finity type of this term.
   * @return The FinityType (finite, positive infinite, or negative infinite).
   */
  [[nodiscard]] FinityType getFinity() const {
    return finity_;
  }

  /**
   * @brief Convert this term to a human-readable string.
   *
   * For infinite terms returns "∞" or "-∞". For finite terms, outputs
   * the sum of coefficient-variable pairs (e.g. "3/2*x1 + 1*x2").
   *
   * @return A string representation of this term.
   */
  [[nodiscard]] std::string str() const {
    if (finity_ == FinityType::kNegativeInfinite) {
      return "-∞";
    }
    if (finity_ == FinityType::kPositiveInfinite) {
      return "∞";
    }
    if (sum_.empty()) {
      return "0";
    }

    std::stringstream ss{};
    for (const auto& i : sum_) {
      ss << i.coefficent;
      if (i.variable != 0) {
        ss << "*x" << i.variable;
      }

      ss << " + ";
    }

    auto view = ss.view();
    view.remove_suffix(3);
    return std::string{view};
  }

  /**
   * @brief Get the precomputed hash of this term.
   * @return The cached hash value.
   */
  [[nodiscard]] uint64_t hash() const {
    return hash_cache_;
  }

 private:
  std::vector<Product> sum_;
  FinityType finity_{FinityType::kFinite};
  uint64_t hash_cache_;

  [[nodiscard]] FinityType scaleFinity(const ScalarType& scalar) const {
    auto new_finity{finity_};
    if (scalar < 0) {
      if (new_finity == FinityType::kPositiveInfinite) {
        new_finity = FinityType::kNegativeInfinite;
      } else if (new_finity == FinityType::kNegativeInfinite) {
        new_finity = FinityType::kPositiveInfinite;
      }
    } else if (scalar == 0) {
      new_finity = FinityType::kFinite;
    }

    return new_finity;
  }

  [[nodiscard]] uint64_t calc_hash() const {
    if (finity_ == FinityType::kNegativeInfinite) {
      // Generated through a fair dice
      constexpr uint64_t very_random{2576791930780013023};
      return very_random;
    }

    if (finity_ == FinityType::kPositiveInfinite) {
      // Generated through a fair dice
      constexpr uint64_t very_random{4331719511055861042};
      return very_random;
    }

    const auto tmp{
        std::views::transform(
            sum_,
            [](const Product& prod) {
              constexpr int kNrHashes = 4;
              std::array<size_t, kNrHashes> hashes{};

              const auto* den{prod.coefficent.get_den_mpz_t()};
              hashes[0] =
                  den->_mp_size > 0
                      ? SpanHash{}(std::span(den->_mp_d, abs(den->_mp_size)))
                      : 0;
              const auto* num{prod.coefficent.get_num_mpz_t()};
              hashes[1] =
                  num->_mp_size > 0
                      ? SpanHash{}(std::span(num->_mp_d, abs(num->_mp_size)))
                      : 1;
              hashes[2] = prod.variable;

              hashes[3] = mpq_sgn(prod.coefficent.get_mpq_t()) + 1;
              if (prod.variable < 0) {
                // for all x: 4 > sign(x) + 1
                hashes[3] |= 4;
              }

              return SpanHash{}(std::span(hashes.begin(), hashes.end()));
            }) |
        std::ranges::to<std::vector>()};

    const auto rval{SpanHash{}(std::span{tmp.begin(), tmp.end()})};
    return rval;
  }

  void canonicalize() {
    assert(0 < sum_.size());
    if (sum_.size() > 1) {
      std::ranges::sort(sum_, {}, &Product::variable);

      const auto e{std::ranges::remove(sum_.begin(), sum_.end(), 0,
                                       &Product::coefficent)};
      sum_.erase(e.begin(), e.end());
    }
    if (sum_.size() == 0) {
      sum_.emplace_back();
    }
  }

  [[nodiscard]] decltype(sum_)::iterator getProductFromVariable(
      const VariableId variable) {
    auto it{std::ranges::lower_bound(sum_, variable, {}, &Product::variable)};

    if (it != sum_.end() && it->variable == variable) {
      return it;
    }

    return sum_.end();
  }

  [[nodiscard]] decltype(sum_)::const_iterator getProductFromVariable(
      const VariableId variable) const {
    auto it{std::ranges::lower_bound(sum_, variable, {}, &Product::variable)};

    if (it != sum_.end() && it->variable == variable) {
      return it;
    }

    return sum_.end();
  }
};

/**
 * @brief Scale a linear term by a scalar on the left.
 *
 * @param s  The scalar multiplier.
 * @param term  The linear term to scale.
 * @return A new LinearTerm with all coefficients multiplied by @p s.
 */
inline LinearTerm operator*(const LinearTerm::ScalarType& s,
                            const LinearTerm& term) {
  return term.scale(s);
}

/**
 * @brief A comparison between a linear term and zero.
 *
 * Encodes a comparison (equality, inequality, less-than, etc.) between
 * a LinearTerm and zero, with support for trivial satisfiability checking
 * based on finity, constant terms, and coefficient signs.
 */
class LinearComparison {
 public:
  /**
   * @brief Default constructor – creates an empty comparison.
   */
  LinearComparison() = default;
  /**
   * @brief Construct a comparison from a term and comparison type.
   *
   * @param term   The linear term to compare.
   * @param type   The comparison type (equality, less-than, etc.).
   */
  LinearComparison(LinearTerm term, const ComparisonType& type)
      : term_{std::move(term)}, type_{type} {}
  /**
   * @brief Construct a comparison from two terms and a comparison type.
   *
   * Represents lhs <type> rhs by internally storing (lhs - rhs) <type> 0.
   *
   * @param lhs  The left-hand side term.
   * @param rhs  The right-hand side term.
   * @param type The comparison type.
   */
  LinearComparison(const LinearTerm& lhs, const LinearTerm& rhs,
                   const ComparisonType& type)
      : term_{lhs - rhs}, type_{type} {}

  /**
   * @brief Get the comparison type.
   * @return The ComparisonType (e.g. equality, less-than).
   */
  [[nodiscard]] ComparisonType getType() const {
    return type_;
  }

  /**
   * @brief Get the linear term being compared.
   * @return A const reference to the stored LinearTerm.
   */
  [[nodiscard]] const LinearTerm& getTerm() const {
    return term_;
  }

  /**
   * @brief Convert this comparison to a human-readable string.
   *
   * Outputs the term string, the comparison operator, and "0"
   * (e.g. "3/2*x1 + 1*x2 <= 0").
   *
   * @return A string representation of this comparison.
   */
  [[nodiscard]] std::string str() const {
    std::string comp;

    switch (type_) {
      case ComparisonType::kEquality:
        comp = "=";
        break;
      case ComparisonType::kInequality:
        comp = "!=";
        break;
      case ComparisonType::kLessThan:
        comp = "<";
        break;
      case ComparisonType::kLessThanOrEqual:
        comp = "<=";
        break;
      case ComparisonType::kGreaterThan:
        comp = ">";
        break;
      case ComparisonType::kGreaterThanOrEqual:
        comp = ">=";
        break;
      default:
        comp = "ERROR";
        break;
    }

    return term_.str() + ' ' + comp + ' ' + "0";
  }

  /**
   * @brief Substitute a variable with a term in this comparison.
   *
   * @param variable  The variable ID to substitute.
   * @param other     The term to substitute with.
   * @return A new LinearComparison with the substitution applied.
   */
  [[nodiscard]] LinearComparison substitute(const VariableId variable,
                                            const LinearTerm& other) const {
    return LinearComparison{term_.substitute(variable, other), type_};
  }

  /**
   * @brief Equality comparison between two comparisons.
   *
   * @param other  The comparison to compare with.
   * @return true if both the comparison type and term are equal.
   */
  [[nodiscard]] bool operator==(const LinearComparison& other) const {
    return type_ == other.type_ && term_ == other.term_;
  }

  /**
   * @brief Lexicographic less-than comparison between two comparisons.
   *
   * @param other  The comparison to compare with.
   * @return true if this comparison is strictly less than @p other.
   */
  [[nodiscard]] bool operator<(const LinearComparison& other) const {
    return std::make_tuple(term_, type_) <
           std::make_tuple(other.term_, other.type_);
  }

  /**
   * @brief Get the hash of this comparison.
   *
   * Combines the term hash and the comparison type into a single value.
   *
   * @return A hash value for this comparison.
   */
  [[nodiscard]] uint64_t hash() const {
    constexpr int kNrHashes = 2;
    std::array<size_t, kNrHashes> hashes{};

    hashes[0] = term_.hash();
    hashes[1] = static_cast<size_t>(type_);

    return SpanHash{}(std::span(hashes.begin(), hashes.end()));
  }

  /**
   * @brief A comparison paired with a parity bit.
   *
   * Used when encoding parity constraints alongside arithmetic comparisons.
   */
  struct WithParity {
    bool parity;
    std::shared_ptr<const LinearComparison> comparison;
  };

  /**
   * @brief Perform trivial satisfiability checks.
   *
   * Runs finity, constant, and less-than trivial checks in sequence.
   * Returns Sat/Unsat if any check is conclusive, otherwise Unknown.
   *
   * @return SmtResult indicating satisfiability.
   */
  [[nodiscard]] SmtResult trivialCheck() const {
    if (const auto check{trivialFinityCheck()}; check != SmtResult::kUnknown) {
      return check;
    }

    if (const auto check{trivialConstantCheck()};
        check != SmtResult::kUnknown) {
      return check;
    }

    if (const auto check{trivialLessThanCheck()};
        check != SmtResult::kUnknown) {
      return check;
    }

    return SmtResult::kUnknown;
  }

  /**
   * @brief Decide satisfiability for real-valued variables.
   *
   * @param conjunction  The set of comparisons (with parity) to decide.
   * @param printModel   Whether to print a satisfying model.
   * @return SmtResult indicating satisfiability.
   */
  [[nodiscard]] static SmtResult decideReal(
      const std::vector<WithParity>& conjunction, bool printModel);
  /**
   * @brief Decide satisfiability for unsigned integer variables.
   *
   * @param conjunction  The set of comparisons (with parity) to decide.
   * @param printModel   Whether to print a satisfying model.
   * @return SmtResult indicating satisfiability.
   */
  [[nodiscard]] static SmtResult decideUInt(
      const std::vector<WithParity>& conjunction, bool printModel);
  /**
   * @brief Shut down static resources used by the decision procedures.
   */
  static void shutdown();

  /**
   * @brief Check trivial satisfiability based on finity.
   *
   * If the term is positive or negative infinity, determines whether
   * the comparison is trivially satisfied or unsatisfied.
   *
   * @return SmtResult (Sat, Unsat, or Unknown).
   */
  [[nodiscard]] SmtResult trivialFinityCheck() const {
    const auto finity{term_.getFinity()};
    if (finity == FinityType::kNegativeInfinite) {
      return boolToResult(type_ == ComparisonType::kInequality ||
                          type_ == ComparisonType::kLessThan ||
                          type_ == ComparisonType::kLessThanOrEqual);
    }
    if (finity == FinityType::kPositiveInfinite) {
      return boolToResult(type_ == ComparisonType::kInequality ||
                          type_ == ComparisonType::kGreaterThan ||
                          type_ == ComparisonType::kGreaterThanOrEqual);
    }

    return SmtResult::kUnknown;
  }

  /**
   * @brief Check trivial satisfiability for constant terms.
   *
   * If the term has no variables (single product with variable ID 0),
   * evaluates the comparison directly against the constant coefficient.
   * Also handles the zero-product (completely empty) case.
   *
   * @return SmtResult (Sat, Unsat, or Unknown).
   */
  [[nodiscard]] SmtResult trivialConstantCheck() const {
    if (term_.size() == 1 && term_.begin()->variable == 0) {
      const auto& coefficent{term_.begin()->coefficent};
      switch (type_) {
        case ComparisonType::kEquality:
          return boolToResult(coefficent == 0);
        case ComparisonType::kInequality:
          return boolToResult(coefficent != 0);
        case ComparisonType::kLessThan:
          return boolToResult(coefficent < 0);
        case ComparisonType::kLessThanOrEqual:
          return boolToResult(coefficent <= 0);
        case ComparisonType::kGreaterThan:
          return boolToResult(coefficent > 0);
        case ComparisonType::kGreaterThanOrEqual:
          return boolToResult(coefficent >= 0);
      }
    }

    if (term_.size() == 0) {
      return boolToResult(type_ == ComparisonType::kEquality ||
                          type_ == ComparisonType::kLessThanOrEqual ||
                          type_ == ComparisonType::kGreaterThanOrEqual);
    }

    return SmtResult::kUnknown;
  }

  /**
   * @brief Check trivial satisfiability for strict less-than.
   *
   * Only applies when the comparison type is kLessThan. Analyzes
   * coefficient signs to determine whether the term is guaranteed
   * to be non-negative (unsatisfied) or guaranteed negative (satisfied).
   *
   * @return SmtResult (Sat, Unsat, or Unknown).
   */
  [[nodiscard]] SmtResult trivialLessThanCheck() const {
    if (type_ == ComparisonType::kLessThan) {
      bool geq{true};

      // all coefficients leq 0 does not suffice
      // needs a constant < 0 as otherwise term could be 0 which is not < 0
      bool leq{term_.begin()->variable == 0 && term_.begin()->coefficent < 0};

      for (const auto& i : term_) {
        if (i.coefficent >= 0) {
          leq &= false;
          geq &= true;
        } else if (i.coefficent <= 0) {
          geq &= false;
          leq &= true;
        }

        if (!leq && !geq) {
          break;
        }
      }

      assert(!leq || !geq);

      if (geq) {
        return SmtResult::kUnsat;
      }
      if (leq) {
        return SmtResult::kSat;
      }
    }

    return SmtResult::kUnknown;
  }

 private:
  LinearTerm term_;
  ComparisonType type_{};
};

}  // namespace ddsolve::expression

#endif /* LINEARTERM_HPP */
