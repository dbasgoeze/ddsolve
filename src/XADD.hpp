#ifndef XADD_HPP
#define XADD_HPP

/**
 * @file XADD.hpp
 * @brief Extended Arithmetic Decision Diagram.
 *
 * XADD is an Extended Arithmetic Decision Diagram – a decision diagram where
 * each leaf holds an arithmetic value (e.g. a rational number) rather than a
 * simple boolean.  It inherits from XDD and specialises the interface for
 * arithmetic operations such as addition, subtraction, scaling, and
 * comparison.
 *
 * XADD is parameterised by a @p Term type that encodes the semantics of
 * variables and arithmetic operations.  The implementation is templated but
 * currently instantiated for @p expression::LinearTerm and @p
 * expression::SmtTerm.
 */

#include <stdexcept>
#include <sylvan_obj.hpp>

#include "ArithmeticTypes.hpp"
#include "LinearTerm.hpp"
#include "VariableOrder.hpp"
#include "XBDD.hpp"
#include "XDD.hpp"

namespace ddsolve {

template <typename T>
class XBDD;

/**
 * @brief Extended Arithmetic Decision Diagram (XADD).
 *
 * An XADD extends the base XDD class to support arithmetic leaf values.
 * It provides operators for addition, subtraction, multiplication, division,
 * min/max, scaling, and comparison, all implemented via the Sylvan backend.
 *
 * @tparam Term  The term type (e.g. LinearTerm or SmtTerm).
 */
template <typename Term>
class XADD final : public XDD<XADD<Term>, Term> {
 protected:
  /**
   * @brief Construct an XADD from a raw Sylvan MTBDD and a variable order.
   * @param add   The underlying Sylvan MTBDD.
   * @param order The variable order for this diagram.
   */
  XADD(const sylvan::Mtbdd& add,
       VariableOrder<Term>::VariableOrderPointer order)
      : XDD<XADD<Term>, Term>{add, std::move(order)} {}

 public:
  using TermType = Term;

  XADD() = default;
  XADD(const XADD&) = default;
  XADD(XADD&&) = default;
  XADD& operator=(const XADD&) = default;
  XADD& operator=(XADD&&) = default;
  ~XADD() = default;

  /**
   * @brief Construct an XADD from an existing XDD.
   * @param xdd  The base XDD to convert.
   */
  explicit XADD(XDD<XADD<Term>, Term>&& xdd)
      : XDD<XADD<Term>, Term>{std::move(xdd)} {}

  /**
   * @brief Construct an empty XADD with a given variable order.
   * @param order  The variable order for this diagram.
   */
  explicit XADD(VariableOrder<Term>::VariableOrderPointer order)
      : XDD<XADD<Term>, Term>{std::move(order)} {}

  /**
   * @brief Construct an XADD from an if-then-else expression.
   * @param _if   The condition (XBDD).
   * @param _then The then-branch (XADD).
   * @param _else The else-branch (XADD).
   *
   * All three operands must share the same variable order.
   */
  XADD(const XBDD<Term>& _if, const XADD& _then, const XADD& _else)
      : XADD{_if.getMtbdd().Ite(_then.getMtbdd(), _else.getMtbdd()),
             _if.getOrder()} {
    if (!(_if.getOrder() == _then.getOrder() &&
          _then.getOrder() == _else.getOrder())) {
      throw std::invalid_argument{"Operation on DDs with different Orderings!"};
    }
  }

  /**
   * @brief Create an undefined XADD (zero leaf with current variable order).
   * @return An XADD representing the undefined / zero value.
   */
  [[nodiscard]] XADD getUndefined() const {
    return XADD{sylvan::Mtbdd::mtbddZero(), this->getOrder()};
  }

  /**
   * @brief Strict less-than comparison between two XADDs.
   * @param other  The XADD to compare against.
   * @return true if this XADD is strictly less than @p other.
   */
  [[nodiscard]] bool operator<(const XADD& other) const;

  /**
   * @brief Less-than-or-equal comparison between two XADDs.
   * @param other  The XADD to compare against.
   * @return true if this XADD is less than or equal to @p other.
   */
  [[nodiscard]] bool operator<=(const XADD& other) const;

  /**
   * @brief Add two XADDs.
   * @param other  The XADD to add.
   * @return The sum of this XADD and @p other.
   */
  [[nodiscard]] XADD operator+(const XADD& other) const;

  /**
   * @brief Subtract two XADDs.
   * @param other  The XADD to subtract.
   * @return The difference of this XADD and @p other.
   */
  [[nodiscard]] XADD operator-(const XADD& other) const;

  /**
   * @brief Element-wise maximum of two XADDs.
   * @param other  The other XADD.
   * @return An XADD whose leaves are the pointwise maximum of the two inputs.
   */
  [[nodiscard]] XADD max(const XADD& other) const;

  /**
   * @brief Element-wise minimum of two XADDs.
   * @param other  The other XADD.
   * @return An XADD whose leaves are the pointwise minimum of the two inputs.
   */
  [[nodiscard]] XADD min(const XADD& other) const;

  /**
   * @brief Prune zero leaves that are not reachable through meaningful paths.
   * @return A pruned copy of this XADD.
   */
  [[nodiscard]] XADD prune() const;

  /**
   * @brief Fully prune the diagram (removing redundant structure).
   * @return A fully pruned copy of this XADD.
   */
  [[nodiscard]] XADD pruneFull() const;

  /**
   * @brief Scale all leaf values by a scalar.
   * @param scalar  The scalar multiplier.
   * @return A new XADD with scaled leaf values.
   */
  [[nodiscard]] XADD scale(const Term::ConstScalarPointer& scalar) const;

  /**
   * @brief Replace undefined leaves with zero.
   * @return A new XADD where undefined values have been replaced by zero.
   */
  [[nodiscard]] XADD undefinedToZero() const;

  /**
   * @brief Convert this XADD to a term, using a given reference term.
   * @param term  A reference term used to guide the conversion.
   * @return The corresponding term representation.
   */
  [[nodiscard]] XADD getTerm(const Term::ConstTermPointer& term) const;

  /**
   * @brief Convert this XADD to a term.
   * @return The corresponding term representation.
   */
  [[nodiscard]] Term::ConstTermPointer getTerm() const;

  /**
   * @brief Get the support of this XADD as a boolean diagram.
   * @return An XBDD representing the variables that affect the result.
   */
  [[nodiscard]] XBDD<Term> getSupport() const {
    return XBDD<Term>::getSupport(*this);
  }

  /**
   * @brief Get the prunes of this XADD as a boolean diagram.
   * @return An XBDD representing the prunable parts of the diagram.
   */
  [[nodiscard]] XBDD<Term> getPrunes() const {
    return XBDD<Term>::getPrunes(*this);
  }

  /**
   * @brief Divide two XADDs (requires division on the Term type).
   * @param other  The divisor XADD.
   * @return The quotient of this XADD divided by @p other.
   */
  [[nodiscard]] XADD operator/(const XADD& other) const
    requires requires(Term a, Term b) { a / b; };

  /**
   * @brief Multiply two XADDs (requires multiplication on the Term type).
   * @param other  The multiplier XADD.
   * @return The product of this XADD and @p other.
   */
  [[nodiscard]] XADD operator*(const XADD& other) const
    requires requires(Term a, Term b) { a * b; };

  template <typename T>
  friend XADD<T> operator*(const T::ConstScalarPointer& scalar,
                           const XADD<T>& add);

  template <typename T>
  friend XADD<T> operator*(const XBDD<T>& bdd, const XADD<T>& add);
};

/**
 * @brief Swap two XADDs.
 * @param lhs  The left-hand side XADD.
 * @param rhs  The right-hand side XADD.
 */
template <typename Term>
inline void swap(XADD<Term>& lhs, XADD<Term>& rhs) noexcept {
  lhs.swap(rhs);
}

}  // namespace ddsolve

#endif /* XADD_HPP */
