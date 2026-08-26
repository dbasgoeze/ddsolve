#ifndef SYLVANINTERFACE_HPP
#define SYLVANINTERFACE_HPP

/**
 * @file SylvanInterface.hpp
 * @brief Frontend interface for Sylvan decision-diagram operations.
 *
 * This header declares the free-standing functions that wrap Sylvan MTBDD
 * operations for the two supported term types (LinearTerm and SmtTerm).
 * Operations include arithmetic (add, sub, mul, div, scale), comparison
 * (compare, max, min), structural manipulation (prune, pruneFull, replaceLeaf,
 * substituteLeafs), and term conversion (getTerm).
 *
 * @note The Doxygen-style comments in this file were generated with the help
 *       of AI (Qwen3.6-35B-A3B). They have been human-checked on a best-effort
 *       basis. No lines of logic in this file were written or modified by AI.
 */

#include <sylvan_obj.hpp>

#include "LinearTerm.hpp"
#include "VariableOrder.hpp"

namespace ddsolve::interface {

/**
 * @brief Fully prune an MTBDD given a variable order.
 *
 * Removes redundant structure from the decision diagram based on the
 * provided variable order, producing a fully pruned representation.
 *
 * @tparam T   The term type (e.g. LinearTerm or SmtTerm).
 * @param dd   The MTBDD to prune.
 * @param order The variable order for this diagram.
 * @return A fully pruned MTBDD.
 */
template <typename T>
sylvan::Mtbdd pruneFull(const sylvan::Mtbdd& dd, const VariableOrder<T>& order);

/**
 * @brief Get prunable parts of an MTBDD as a boolean diagram.
 *
 * Returns an XBDD representing the parts of the diagram that can be
 * pruned without changing the function represented by the MTBDD.
 *
 * @tparam T   The term type (e.g. LinearTerm or SmtTerm).
 * @param dd   The MTBDD to analyse.
 * @param order The variable order for this diagram.
 * @return An XBDD of the prunable parts.
 */
template <typename T>
sylvan::Mtbdd getPrunes(const sylvan::Mtbdd& dd, const VariableOrder<T>& order);

/**
 * @brief Prune zero leaves from an MTBDD.
 *
 * Removes zero-valued leaves that are not reachable through meaningful
 * paths in the diagram.
 *
 * @tparam T   The term type (e.g. LinearTerm or SmtTerm).
 * @param dd   The MTBDD to prune.
 * @return A pruned MTBDD with zero leaves removed.
 */
template <typename T>
sylvan::Mtbdd prune(const sylvan::Mtbdd& dd);

/**
 * @brief Add two MTBDDs.
 *
 * Performs element-wise addition of the leaf values of two MTBDDs.
 *
 * @tparam T   The term type (e.g. LinearTerm or SmtTerm).
 * @param lhs  The left-hand side MTBDD.
 * @param rhs  The right-hand side MTBDD.
 * @return The sum of @p lhs and @p rhs.
 */
template <typename T>
sylvan::Mtbdd add(const sylvan::Mtbdd& lhs, const sylvan::Mtbdd& rhs);

/**
 * @brief Subtract two MTBDDs.
 *
 * Performs element-wise subtraction of the leaf values of two MTBDDs.
 *
 * @tparam T   The term type (e.g. LinearTerm or SmtTerm).
 * @param lhs  The left-hand side MTBDD (minuend).
 * @param rhs  The right-hand side MTBDD (subtrahend).
 * @return The difference of @p lhs and @p rhs.
 */
template <typename T>
sylvan::Mtbdd sub(const sylvan::Mtbdd& lhs, const sylvan::Mtbdd& rhs);

/**
 * @brief Scale leaf values of an MTBDD by a scalar.
 *
 * Multiplies all leaf values in the MTBDD by the given scalar.
 *
 * @tparam T     The term type (e.g. LinearTerm or SmtTerm).
 * @param scalar The scalar multiplier.
 * @param dd     The MTBDD to scale.
 * @return An MTBDD with all leaf values multiplied by @p scalar.
 */
template <typename T>
sylvan::Mtbdd scale(const typename T::ConstScalarPointer& scalar,
                    const sylvan::Mtbdd& dd);

/**
 * @brief Multiply two MTBDDs.
 *
 * Performs element-wise multiplication of the leaf values of two MTBDDs.
 *
 * @tparam T   The term type (e.g. LinearTerm or SmtTerm).
 * @param lhs  The left-hand side MTBDD.
 * @param rhs  The right-hand side MTBDD.
 * @return The product of @p lhs and @p rhs.
 */
template <typename T>
sylvan::Mtbdd mul(const sylvan::Mtbdd& lhs, const sylvan::Mtbdd& rhs);

/**
 * @brief Divide two MTBDDs.
 *
 * Performs element-wise division of the leaf values of two MTBDDs.
 *
 * @tparam T   The term type (e.g. LinearTerm or SmtTerm).
 * @param lhs  The left-hand side MTBDD (dividend).
 * @param rhs  The right-hand side MTBDD (divisor).
 * @return The quotient of @p lhs divided by @p rhs.
 */
template <typename T>
sylvan::Mtbdd div(const sylvan::Mtbdd& lhs, const sylvan::Mtbdd& rhs);

/**
 * @brief Convert a term to an MTBDD.
 *
 * @tparam T   The term type (e.g. LinearTerm or SmtTerm).
 * @param term The term to convert.
 * @return An MTBDD representing @p term.
 */
template <typename T>
sylvan::Mtbdd getTerm(const T& term);

/**
 * @brief Convert an MTBDD leaf value to a term.
 *
 * @tparam T   The term type (e.g. LinearTerm or SmtTerm).
 * @param leaf The MTBDD (expected to be a leaf node).
 * @return The corresponding term representation.
 */
template <typename T>
T::ConstTermPointer getTerm(const sylvan::Mtbdd& leaf);

/**
 * @brief Replace leaf values in an MTBDD.
 *
 * Replaces all occurrences of @p old_leaf with @p new_leaf in the
 * given MTBDD.
 *
 * @tparam T      The term type (e.g. LinearTerm or SmtTerm).
 * @param mtbdd   The MTBDD in which to perform the replacement.
 * @param old_leaf The leaf value to replace.
 * @param new_leaf The replacement leaf value.
 * @return A new MTBDD with the specified leaf replacements.
 */
template <typename T>
sylvan::Mtbdd replaceLeaf(const sylvan::Mtbdd& mtbdd,
                          const sylvan::Mtbdd& old_leaf,
                          const sylvan::Mtbdd& new_leaf);

/**
 * @brief Substitute leaves in an MTBDD using a substitution map.
 *
 * Applies the given substitution to replace leaves in the MTBDD.
 *
 * @tparam T         The term type (e.g. LinearTerm or SmtTerm).
 * @param mtbdd      The MTBDD in which to perform the substitution.
 * @param substitution The substitution map to apply.
 * @return A new MTBDD with the substitution applied.
 */
template <typename T>
sylvan::Mtbdd substituteLeafs(const sylvan::Mtbdd& mtbdd,
                              const ddsolve::Substitution<T>& substitution);

/**
 * @brief Enable or disable model printing.
 *
 * Controls whether satisfying models are printed during decision procedures.
 *
 * @param flag  Whether to enable (true) or disable (false) model printing.
 */
void printModels(bool flag);

/**
 * @brief Compare two MTBDDs under a given comparison type.
 *
 * Evaluates a comparison (e.g. equality, less-than) between two MTBDDs
 * using the provided variable order.
 *
 * @tparam T    The term type (e.g. LinearTerm or SmtTerm).
 * @param lhs   The left-hand side MTBDD.
 * @param rhs   The right-hand side MTBDD.
 * @param comp  The comparison type to apply.
 * @param order The variable order for this diagram.
 * @return true if the comparison holds.
 */
template <typename T>
bool compare(const sylvan::Mtbdd& lhs, const sylvan::Mtbdd& rhs,
             const expression::ComparisonType& comp,
             const VariableOrder<T>& order);

/**
 * @brief Element-wise maximum of two MTBDDs.
 *
 * @tparam T     The term type (e.g. LinearTerm or SmtTerm).
 * @param lhs    The left-hand side MTBDD.
 * @param rhs    The right-hand side MTBDD.
 * @param order  The variable order for this diagram.
 * @return An MTBDD whose leaves are the pointwise maximum of the two inputs.
 */
template <typename T>
sylvan::Mtbdd max(const sylvan::Mtbdd& lhs, const sylvan::Mtbdd& rhs,
                  typename VariableOrder<T>::VariableOrderPointer order);

/**
 * @brief Element-wise minimum of two MTBDDs.
 *
 * @tparam T     The term type (e.g. LinearTerm or SmtTerm).
 * @param lhs    The left-hand side MTBDD.
 * @param rhs    The right-hand side MTBDD.
 * @param order  The variable order for this diagram.
 * @return An MTBDD whose leaves are the pointwise minimum of the two inputs.
 */
template <typename T>
sylvan::Mtbdd min(const sylvan::Mtbdd& lhs, const sylvan::Mtbdd& rhs,
                  typename VariableOrder<T>::VariableOrderPointer order);

}  // namespace ddsolve::interface

#endif /* SYLVANINTERFACE_HPP */
