#ifndef SYLVANINTERFACE_HPP
#define SYLVANINTERFACE_HPP

#include <sylvan_obj.hpp>

#include "LinearTerm.hpp"
#include "VariableOrder.hpp"

namespace ddsolve::interface {

template <typename T>
sylvan::Mtbdd pruneFull(const sylvan::Mtbdd &dd, const VariableOrder<T> &order);

template <typename T>
sylvan::Mtbdd getPrunes(const sylvan::Mtbdd &dd, const VariableOrder<T> &order);

template <typename T>
sylvan::Mtbdd prune(const sylvan::Mtbdd &dd);

template <typename T>
sylvan::Mtbdd add(const sylvan::Mtbdd &lhs, const sylvan::Mtbdd &rhs);

template <typename T>
sylvan::Mtbdd sub(const sylvan::Mtbdd &lhs, const sylvan::Mtbdd &rhs);

template <typename T>
sylvan::Mtbdd scale(const typename T::ConstScalarPointer &scalar,
                    const sylvan::Mtbdd &dd);
template <typename T>
sylvan::Mtbdd mul(const sylvan::Mtbdd &lhs, const sylvan::Mtbdd &rhs);

template <typename T>
sylvan::Mtbdd div(const sylvan::Mtbdd &lhs, const sylvan::Mtbdd &rhs);

template <typename T>
sylvan::Mtbdd getTerm(const T &term);

template <typename T>
T::ConstTermPointer getTerm(const sylvan::Mtbdd &leaf);

template <typename T>
sylvan::Mtbdd replaceLeaf(const sylvan::Mtbdd &mtbdd,
                          const sylvan::Mtbdd &old_leaf,
                          const sylvan::Mtbdd &new_leaf);

template <typename T>
sylvan::Mtbdd substituteLeafs(const sylvan::Mtbdd &mtbdd,
                              const ddsolve::Substitution<T> &substitution);

void printModels(bool flag);

template <typename T>
bool compare(const sylvan::Mtbdd &lhs, const sylvan::Mtbdd &rhs,
             const expression::ComparisonType &comp,
             const VariableOrder<T> &order);

template <typename T>
sylvan::Mtbdd max(const sylvan::Mtbdd &lhs, const sylvan::Mtbdd &rhs,
                  typename VariableOrder<T>::VariableOrderPointer order);

template <typename T>
sylvan::Mtbdd min(const sylvan::Mtbdd &lhs, const sylvan::Mtbdd &rhs,
                  typename VariableOrder<T>::VariableOrderPointer order);

}  // namespace ddsolve::interface

#endif /* SYLVANINTERFACE_HPP */
