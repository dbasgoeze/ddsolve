#include "XADD.hpp"

/**
 * @file XADD.cpp
 * @brief Implementation of Extended Arithmetic Decision Diagram operations.
 *
 * This file contains the out-of-line definitions for the templated member
 * functions of XDD and XADD, as well as free-standing operators.  It also
 * provides explicit template instantiations for the two supported term types:
 * @p expression::LinearTerm and @p expression::SmtTerm.
 *
 * @note The Doxygen-style comments in this file were generated with the help
 *       of AI (Qwen3.6-35B-A3B). They have been human-checked on a best-effort
 *       basis. No lines of logic in this file were written or modified by AI.
 */

#include <algorithm>
#include <memory>
#include <ranges>

#include "LinearTerm.hpp"
#include "SmtTerm.hpp"
#include "VariableOrder.hpp"
#include "XBDD.hpp"
#include "XDD.hpp"
#include "sylvan/SylvanInterface.hpp"
#include "sylvan/sylvan_ddsolve.h"

namespace ddsolve {

/**
 * @brief Substitute a variable with a term in the diagram.
 *
 * Collects the variables from the support of the diagram, maps each to its
 * new representation based on the substitution, and applies the reordering
 * via Sylvan's Compose operation.  Leaf values are updated through the
 * @p substituteLeafs interface.
 *
 * @tparam T      Derived diagram type.
 * @tparam Term   Term type.
 * @param var     The variable to substitute.
 * @param term    The term to substitute it with.
 * @return A new diagram with the substitution applied.
 */
template <typename T, typename Term>
[[nodiscard]] T XDD<T, Term>::substitute(
    const Term::Variable& var, const Term::ConstTermPointer& term) const {
  const auto one{sylvan::Mtbdd::mtbddOne()};
  const auto support{getMtbdd().Support()};

  std::vector<SylvanVariable> old_vars{};
  old_vars.reserve(support.NodeCount());
  for (auto i{support.GetMTBDD()}; i != sylvan::mtbdd_true;
       i = sylvan::mtbdd_gethigh(i)) {
    old_vars.push_back(sylvan::mtbdd_getvar(i));
  }

  const auto new_reprs{getOrder()->substitute(var, *term, old_vars)};

  sylvan::MtbddMap reorder{};
  for (const auto& [from, to] : std::views::zip(old_vars, new_reprs)) {
    reorder.put(from, to);
  }

  return T{{interface::substituteLeafs(getMtbdd().Compose(reorder),
                                       Substitution<Term>{var, term}),
            getOrder()}};
}

/**
 * @brief If-then-else combining a boolean condition with XADD branches.
 *
 * @tparam Term  Term type.
 * @param b  The then-branch (XADD).
 * @param c  The else-branch (XADD).
 * @return An XADD representing @p ite(condition, b, c).
 *
 * All operands must share the same variable order.
 */
template <typename Term>
[[nodiscard]] XADD<Term> XBDD<Term>::ite(const XADD<Term>& b,
                                         const XADD<Term>& c) const {
  if (this->getOrder() != b.getOrder() || this->getOrder() != c.getOrder()) {
    throw std::invalid_argument{"Operation on DDs with different Orderings!"};
  }
  return XADD<Term>{*this, b, c};
}

/**
 * @brief Strict less-than comparison between two XADDs.
 */
template <typename Term>
[[nodiscard]] bool XADD<Term>::operator<(const XADD<Term>& other) const {
  return interface::compare(this->getMtbdd(), other.getMtbdd(),
                            expression::ComparisonType::kLessThan,
                            *this->getOrder());
}

/**
 * @brief Less-than-or-equal comparison between two XADDs.
 */
template <typename Term>
[[nodiscard]] bool XADD<Term>::operator<=(const XADD<Term>& other) const {
  return interface::compare(this->getMtbdd(), other.getMtbdd(),
                            expression::ComparisonType::kLessThanOrEqual,
                            *this->getOrder());
}

/**
 * @brief Add two XADDs.
 *
 * Delegates to @p interface::add after verifying matching variable orders.
 */
template <typename Term>
[[nodiscard]] XADD<Term> XADD<Term>::operator+(const XADD<Term>& other) const {
  if (this->getOrder() != other.getOrder()) {
    throw std::invalid_argument{"Operation on DDs with different Orderings!"};
  }
  return {interface::add<Term>(this->getMtbdd(), other.getMtbdd()),
          this->getOrder()};
}

/**
 * @brief Subtract two XADDs.
 *
 * Delegates to @p interface::sub after verifying matching variable orders.
 */
template <typename Term>
[[nodiscard]] XADD<Term> XADD<Term>::operator-(const XADD<Term>& other) const {
  if (this->getOrder() != other.getOrder()) {
    throw std::invalid_argument{"Operation on DDs with different Orderings!"};
  }
  return {interface::sub<Term>(this->getMtbdd(), other.getMtbdd()),
          this->getOrder()};
}

/**
 * @brief Element-wise maximum of two XADDs.
 *
 * Delegates to @p interface::max after verifying matching variable orders.
 */
template <typename Term>
[[nodiscard]] XADD<Term> XADD<Term>::max(const XADD<Term>& other) const {
  if (this->getOrder() != other.getOrder()) {
    throw std::invalid_argument{"Operation on DDs with different Orderings!"};
  }
  return {interface::max<Term>(this->getMtbdd(), other.getMtbdd(),
                               this->getOrder()),
          this->getOrder()};
}

/**
 * @brief Element-wise minimum of two XADDs.
 *
 * Delegates to @p interface::min after verifying matching variable orders.
 */
template <typename Term>
[[nodiscard]] XADD<Term> XADD<Term>::min(const XADD<Term>& other) const {
  if (this->getOrder() != other.getOrder()) {
    throw std::invalid_argument{"Operation on DDs with different Orderings!"};
  }
  return {interface::min<Term>(this->getMtbdd(), other.getMtbdd(),
                               this->getOrder()),
          this->getOrder()};
}

/**
 * @brief Prune zero leaves that are not reachable through meaningful paths.
 */
template <typename Term>
[[nodiscard]] XADD<Term> XADD<Term>::prune() const {
  return {interface::prune<Term>(this->getMtbdd()), this->getOrder()};
}

/**
 * @brief Fully prune the diagram (removing redundant structure).
 */
template <typename Term>
[[nodiscard]] XADD<Term> XADD<Term>::pruneFull() const {
  return {interface::pruneFull<Term>(this->getMtbdd(), *this->getOrder()),
          this->getOrder()};
}

/**
 * @brief Scale all leaf values by a scalar.
 *
 * Delegates to @p interface::scale.
 */
template <typename Term>
[[nodiscard]] XADD<Term> XADD<Term>::scale(
    const Term::ConstScalarPointer& scalar) const {
  return {interface::scale<Term>(scalar, this->getMtbdd()), this->getOrder()};
}

/**
 * @brief Replace undefined leaves with zero.
 *
 * Replaces every leaf whose value equals the current "undefined" sentinel
 * with the zero constant.
 */
template <typename Term>
[[nodiscard]] XADD<Term> XADD<Term>::undefinedToZero() const {
  return {interface::replaceLeaf<Term>(
              this->getMtbdd(), sylvan::Mtbdd::mtbddZero(),
              getTerm(std::make_shared<Term>()).getMtbdd()),
          this->getOrder()};
}

/**
 * @brief Convert this XADD to a term, using a given reference term.
 */
template <typename Term>
[[nodiscard]] XADD<Term> XADD<Term>::getTerm(
    const Term::ConstTermPointer& term) const {
  return {interface::getTerm(*term), this->getOrder()};
}

/**
 * @brief Convert this XADD to a term.
 */
template <typename Term>
[[nodiscard]] Term::ConstTermPointer XADD<Term>::getTerm() const {
  return interface::getTerm<Term>(this->getMtbdd());
}

/**
 * @brief Get the prunes of an XADD as a boolean diagram.
 */
template <typename Term>
[[nodiscard]] XBDD<Term> XBDD<Term>::getPrunes(const XADD<Term>& add) {
  return {interface::getPrunes(add.getMtbdd(), *add.getOrder()),
          add.getOrder()};
}

/**
 * @brief Get the support of an XADD as a boolean diagram.
 */
template <typename Term>
[[nodiscard]] XBDD<Term> XBDD<Term>::getSupport(const XADD<Term>& add) {
  return {add.getMtbdd().Support(), add.getOrder()};
}

/**
 * @brief Multiply a boolean diagram by an arithmetic diagram.
 *
 * Uses the boolean diagram as a selector: when the BDD leaf is true the
 * XADD value is selected; when false the result is zero.
 */
template <typename Term>
XADD<Term> operator*(const XBDD<Term>& bdd, const XADD<Term>& add) {
  if (bdd.getOrder() != add.getOrder()) {
    throw std::invalid_argument{"Operation on DDs with different Orderings!"};
  }
  return {bdd.getMtbdd() * add.getMtbdd(), add.getOrder()};
}

/**
 * @brief Divide two XADDs (requires division on the Term type).
 *
 * Delegates to @p interface::div after verifying matching variable orders.
 */
template <typename Term>
XADD<Term> XADD<Term>::operator/(const XADD<Term>& other) const
  requires requires(Term a, Term b) { a / b; }
{
  if (this->getOrder() != other.getOrder()) {
    throw std::invalid_argument{"Operation on DDs with different Orderings!"};
  }
  return {interface::div<Term>(this->getMtbdd(), other.getMtbdd()),
          this->getOrder()};
}

/**
 * @brief Multiply two XADDs (requires multiplication on the Term type).
 *
 * Delegates to @p interface::mul after verifying matching variable orders.
 */
template <typename Term>
XADD<Term> XADD<Term>::operator*(const XADD<Term>& other) const
  requires requires(Term a, Term b) { a * b; }
{
  if (this->getOrder() != other.getOrder()) {
    throw std::invalid_argument{"Operation on DDs with different Orderings!"};
  }
  return {interface::mul<Term>(this->getMtbdd(), other.getMtbdd()),
          this->getOrder()};
}

/**
 * @brief Scalar multiplication (scalar on the left).
 */
template <typename Term>
XADD<Term> operator*(const typename Term::ConstScalarPointer& scalar,
                     const XADD<Term>& add) {
  return add.scale(scalar);
}

/* ==========================================================================
 * Explicit template instantiations
 * ========================================================================== */

// LinearTerm variants
template class XBDD<expression::LinearTerm>;
template class XADD<expression::LinearTerm>;

template class XDD<XBDD<expression::LinearTerm>, expression::LinearTerm>;
template class XDD<XADD<expression::LinearTerm>, expression::LinearTerm>;

template XADD<expression::LinearTerm> operator*(
    const XBDD<expression::LinearTerm>& bdd,
    const XADD<expression::LinearTerm>& add);
template XADD<expression::LinearTerm> operator*(
    const expression::LinearTerm::ConstScalarPointer& scalar,
    const XADD<expression::LinearTerm>& add);

// SmtTerm variants
template class XBDD<expression::SmtTerm>;
template class XADD<expression::SmtTerm>;

template class XDD<XBDD<expression::SmtTerm>, expression::SmtTerm>;
template class XDD<XADD<expression::SmtTerm>, expression::SmtTerm>;

template XADD<expression::SmtTerm> operator*(
    const XBDD<expression::SmtTerm>& bdd, const XADD<expression::SmtTerm>& add);
template XADD<expression::SmtTerm> operator*(
    const expression::SmtTerm::ConstScalarPointer& scalar,
    const XADD<expression::SmtTerm>& add);

};  // namespace ddsolve
