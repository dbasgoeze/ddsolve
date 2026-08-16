#ifndef XADD_HPP
#define XADD_HPP

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

template <typename Term>
class XADD final : public XDD<XADD<Term>, Term> {
 protected:
  XADD(const sylvan::Mtbdd &add,
       VariableOrder<Term>::VariableOrderPointer order)
      : XDD<XADD<Term>, Term>{add, std::move(order)} {}

 public:
  using TermType = Term;

  XADD() = default;
  XADD(const XADD &) = default;
  XADD(XADD &&) = default;
  XADD &operator=(const XADD &) = default;
  XADD &operator=(XADD &&) = default;
  ~XADD() = default;

  explicit XADD(XDD<XADD<Term>, Term> &&xdd)
      : XDD<XADD<Term>, Term>{std::move(xdd)} {}
  explicit XADD(VariableOrder<Term>::VariableOrderPointer order)
      : XDD<XADD<Term>, Term>{std::move(order)} {}
  XADD(const XBDD<Term> &_if, const XADD &_then, const XADD &_else)
      : XADD{_if.getMtbdd().Ite(_then.getMtbdd(), _else.getMtbdd()),
             _if.getOrder()} {
    if (!(_if.getOrder() == _then.getOrder() &&
          _then.getOrder() == _else.getOrder())) {
      throw std::invalid_argument{"Operation on DDs with different Orderings!"};
    }
  }

  [[nodiscard]] XADD getUndefined() const {
    return XADD{sylvan::Mtbdd::mtbddZero(), this->getOrder()};
  }

  [[nodiscard]] bool operator<(const XADD &other) const;
  [[nodiscard]] bool operator<=(const XADD &other) const;

  [[nodiscard]] XADD operator+(const XADD &other) const;
  [[nodiscard]] XADD operator-(const XADD &other) const;
  [[nodiscard]] XADD max(const XADD &other) const;
  [[nodiscard]] XADD min(const XADD &other) const;
  [[nodiscard]] XADD prune() const;
  [[nodiscard]] XADD pruneFull() const;
  [[nodiscard]] XADD scale(const Term::ConstScalarPointer &scalar) const;
  [[nodiscard]] XADD undefinedToZero() const;
  [[nodiscard]] XADD getTerm(const Term::ConstTermPointer &term) const;
  [[nodiscard]] Term::ConstTermPointer getTerm() const;

  [[nodiscard]] XBDD<Term> getSupport() const {
    return XBDD<Term>::getSupport(*this);
  }

  [[nodiscard]] XBDD<Term> getPrunes() const {
    return XBDD<Term>::getPrunes(*this);
  }

  [[nodiscard]] XADD operator/(const XADD &other) const
    requires requires(Term a, Term b) { a / b; };

  [[nodiscard]] XADD operator*(const XADD &other) const
    requires requires(Term a, Term b) { a * b; };

  template <typename T>
  friend XADD<T> operator*(const T::ConstScalarPointer &scalar,
                           const XADD<T> &add);

  template <typename T>
  friend XADD<T> operator*(const XBDD<T> &bdd, const XADD<T> &add);
};

template <typename Term>
inline void swap(XADD<Term> &lhs, XADD<Term> &rhs) noexcept {
  lhs.swap(rhs);
}

}  // namespace ddsolve

#endif /* XADD_HPP */
