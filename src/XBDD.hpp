#ifndef XBDD_HPP
#define XBDD_HPP

/**
 * @file XBDD.hpp
 * @brief Extended Binary Decision Diagram.
 *
 * XBDD is an Extended Binary Decision Diagram – a decision diagram where
 * each leaf holds a boolean value (true or false).  It inherits from the
 * base XDD class and adds boolean-specific operations such as negation,
 * conjunction, disjunction, and if-then-else.
 *
 * XBDD can also be combined with XADD (Extended Arithmetic Decision Diagram)
 * through the multiplication operator, allowing boolean diagrams to act as
 * selectors for arithmetic diagrams.
 *
 * @tparam Term  The term type (e.g. LinearTerm or SmtTerm).
 */

#include <iostream>
#include <memory>
#include <sylvan_obj.hpp>

#include "LinearTerm.hpp"
#include "VariableOrder.hpp"
#include "XDD.hpp"

namespace ddsolve {

template <typename T>
class XADD;

/**
 * @brief Extended Binary Decision Diagram (XBDD).
 *
 * An XBDD is a decision diagram with boolean leaf values.  It supports
 * standard boolean operations (negation, AND, OR, ITE) and can be used
 * to select among arithmetic values when multiplied with an XADD.
 */
template <typename Term>
class XBDD final : public XDD<XBDD<Term>, Term> {
 protected:
  /**
   * @brief Construct an XBDD from a raw Sylvan MTBDD and a variable order.
   * @param mtbdd  The underlying Sylvan MTBDD.
   * @param order  The variable order for this diagram.
   */
  XBDD(const sylvan::Mtbdd& mtbdd,
       VariableOrder<Term>::VariableOrderPointer order)
      : XDD<XBDD<Term>, Term>{mtbdd, std::move(order)} {}

 public:
  using TermType = Term;

  XBDD() = default;
  XBDD(const XBDD&) = default;
  XBDD(XBDD&&) = default;
  XBDD& operator=(const XBDD&) = default;
  XBDD& operator=(XBDD&&) = default;
  ~XBDD() = default;

  /**
   * @brief Construct an XBDD from an existing XDD.
   * @param xdd  The base XDD to convert.
   */
  explicit XBDD(XDD<XBDD<Term>, Term>&& xdd)
      : XDD<XBDD<Term>, Term>{std::move(xdd)} {}

  /**
   * @brief Construct an empty XBDD with a given variable order.
   * @param order  The variable order for this diagram.
   */
  explicit XBDD(VariableOrder<Term>::VariableOrderPointer order)
      : XDD<XBDD<Term>, Term>{std::move(order)} {}

  /**
   * @brief Construct an XBDD from an if-then-else expression.
   * @param if_    The condition.
   * @param then_  The then-branch.
   * @param else_  The else-branch.
   *
   * All three operands must share the same variable order.
   */
  XBDD(const XBDD& if_, const XBDD& then_, const XBDD& else_)
      : XBDD{if_.ite(then_, else_)} {
    if (!(if_.getOrder() == then_.getOrder() &&
          then_.getOrder() == else_.getOrder())) {
      throw std::invalid_argument{"Operation on DDs with different Orderings!"};
    }
  }

  /**
   * @brief Negate this XBDD.
   * @return A new XBDD with all truth values flipped.
   */
  [[nodiscard]] XBDD operator~() const {
    return XBDD{~this->getMtbdd(), this->getOrder()};
  }

  /**
   * @brief Logical AND with another XBDD.
   * @param other  The other XBDD.
   * @return The conjunction of this XBDD and @p other.
   *
   * Both operands must share the same variable order.
   */
  [[nodiscard]] XBDD operator&(const XBDD& other) const {
    if (this->getOrder() != other.getOrder()) {
      throw std::invalid_argument{"Operation on DDs with different Orderings!"};
    }
    return XBDD{this->getMtbdd() * other.getMtbdd(), this->getOrder()};
  }

  /**
   * @brief Logical OR with another XBDD.
   * @param other  The other XBDD.
   * @return The disjunction of this XBDD and @p other.
   *
   * Both operands must share the same variable order.
   */
  [[nodiscard]] XBDD operator|(const XBDD& other) const {
    if (this->getOrder() != other.getOrder()) {
      throw std::invalid_argument{"Operation on DDs with different Orderings!"};
    }
    return XBDD{this->getMtbdd() + other.getMtbdd(), this->getOrder()};
  }

  /**
   * @brief Create a constant true XBDD with the current variable order.
   * @return An XBDD representing the constant true value.
   */
  [[nodiscard]] XBDD getTrue() const {
    return XBDD{sylvan::Mtbdd::mtbddOne(), this->getOrder()};
  }

  /**
   * @brief Create a constant false XBDD with the current variable order.
   * @return An XBDD representing the constant false value.
   */
  [[nodiscard]] XBDD getFalse() const {
    return XBDD{sylvan::Mtbdd::mtbddZero(), this->getOrder()};
  }

  /**
   * @brief Obtain the XBDD representation of a comparison.
   * @param comp  The comparison to represent.
   * @return An XBDD encoding the truth of @p comp under the current variable
   * order.
   */
  [[nodiscard]] XBDD obtainRepresentation(
      const Term::ConstComparisonPointer& comp) const {
    return XBDD{this->getOrder()->obtainRepresentation(comp), this->getOrder()};
  }

  /**
   * @brief If-then-else on boolean operands.
   * @param b  The then-branch.
   * @param c  The else-branch.
   * @return The ITE of this XBDD (condition), @p b (then), and @p c (else).
   *
   * All three operands must share the same variable order.
   */
  [[nodiscard]] XBDD ite(const XBDD& b, const XBDD& c) const {
    if (this->getOrder() != b.getOrder() || this->getOrder() != c.getOrder()) {
      throw std::invalid_argument{"Operation on DDs with different Orderings!"};
    }
    return {this->getMtbdd().Ite(b.getMtbdd(), c.getMtbdd()), this->getOrder()};
  }

  /**
   * @brief If-then-else with XADD operands.
   * @param b  The then-branch (XADD).
   * @param c  The else-branch (XADD).
   * @return The ITE of this XBDD (condition), @p b (then), and @p c (else).
   *
   * All three operands must share the same variable order.
   */
  [[nodiscard]] XADD<Term> ite(const XADD<Term>& b, const XADD<Term>& c) const;

  /**
   * @brief Get the support of this XBDD.
   * @return A new XBDD containing only the support variables.
   */
  [[nodiscard]] XBDD getSupport() const {
    return {this->getMtbdd().Support(), this->getOrder()};
  }

  /**
   * @brief Get the prunes of an XADD as a boolean diagram.
   * @param add  The XADD to analyse.
   * @return An XBDD representing the prunable parts of @p add.
   */
  [[nodiscard]] static XBDD getPrunes(const XADD<Term>& add);

  /**
   * @brief Get the support of an XADD as a boolean diagram.
   * @param add  The XADD to analyse.
   * @return An XBDD representing the variables that affect @p add.
   */
  [[nodiscard]] static XBDD getSupport(const XADD<Term>& add);

  template <typename T>
  friend XADD<T> operator*(const XBDD<T>& bdd, const XADD<T>& add);
};

/**
 * @brief Swap two XBDDs.
 * @param lhs  The left-hand side XBDD.
 * @param rhs  The right-hand side XBDD.
 */
template <typename Term>
inline void swap(XBDD<Term>& lhs, XBDD<Term>& rhs) noexcept {
  lhs.swap(rhs);
}

/**
 * @brief Multiply a boolean diagram by an arithmetic diagram.
 *
 * This operator uses the boolean diagram as a selector: when the BDD leaf
 * is true, the XADD value is selected; when false, the result is zero.
 *
 * @param bdd  The boolean selector.
 * @param add  The arithmetic diagram.
 * @return The pointwise product of @p bdd and @p add.
 */
template <typename Term>
XADD<Term> operator*(const XBDD<Term>& bdd, const XADD<Term>& add);

}  // namespace ddsolve

#endif /* XBDD_HPP */
