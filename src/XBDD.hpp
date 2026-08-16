#ifndef XBDD_HPP
#define XBDD_HPP

#include <iostream>
#include <memory>
#include <sylvan_obj.hpp>

#include "LinearTerm.hpp"
#include "VariableOrder.hpp"
#include "XDD.hpp"

namespace ddsolve {

template <typename T>
class XADD;

template <typename Term>
class XBDD final : public XDD<XBDD<Term>, Term> {
 protected:
  XBDD(const sylvan::Mtbdd &mtbdd,
       VariableOrder<Term>::VariableOrderPointer order)
      : XDD<XBDD<Term>, Term>{mtbdd, std::move(order)} {}

 public:
  using TermType = Term;

  XBDD() = default;
  XBDD(const XBDD &) = default;
  XBDD(XBDD &&) = default;
  XBDD &operator=(const XBDD &) = default;
  XBDD &operator=(XBDD &&) = default;
  ~XBDD() = default;

  explicit XBDD(XDD<XBDD<Term>, Term> &&xdd) : XDD<XBDD<Term>, Term>{std::move(xdd)} {}
  explicit XBDD(VariableOrder<Term>::VariableOrderPointer order)
      : XDD<XBDD<Term>, Term>{std::move(order)} {}
  XBDD(const XBDD &if_, const XBDD &then_, const XBDD &else_)
      : XBDD{if_.ite(then_, else_)} {
    if (!(if_.getOrder() == then_.getOrder() &&
          then_.getOrder() == else_.getOrder())) {
      throw std::invalid_argument{"Operation on DDs with different Orderings!"};
    }
  }

  [[nodiscard]] XBDD operator~() const {
    return XBDD{~this->getMtbdd(), this->getOrder()};
  }

  [[nodiscard]] XBDD operator&(const XBDD &other) const {
    if (this->getOrder() != other.getOrder()) {
      throw std::invalid_argument{"Operation on DDs with different Orderings!"};
    }
    return XBDD{this->getMtbdd() * other.getMtbdd(), this->getOrder()};
  }

  [[nodiscard]] XBDD operator|(const XBDD &other) const {
    if (this->getOrder() != other.getOrder()) {
      throw std::invalid_argument{"Operation on DDs with different Orderings!"};
    }
    return XBDD{this->getMtbdd() + other.getMtbdd(), this->getOrder()};
  }

  [[nodiscard]] XBDD getTrue() const {
    return XBDD{sylvan::Mtbdd::mtbddOne(), this->getOrder()};
  }

  [[nodiscard]] XBDD getFalse() const {
    return XBDD{sylvan::Mtbdd::mtbddZero(), this->getOrder()};
  }

  [[nodiscard]] XBDD obtainRepresentation(
      const Term::ConstComparisonPointer &comp) const {
    return XBDD{this->getOrder()->obtainRepresentation(comp), this->getOrder()};
  }

  [[nodiscard]] XBDD ite(const XBDD &b, const XBDD &c) const {
    if (this->getOrder() != b.getOrder() || this->getOrder() != c.getOrder()) {
      throw std::invalid_argument{"Operation on DDs with different Orderings!"};
    }
    return {this->getMtbdd().Ite(b.getMtbdd(), c.getMtbdd()), this->getOrder()};
  }

  [[nodiscard]] XADD<Term> ite(const XADD<Term> &b, const XADD<Term> &c) const;

  [[nodiscard]] XBDD getSupport() const {
    return {this->getMtbdd().Support(), this->getOrder()};
  }

  [[nodiscard]] static XBDD getPrunes(const XADD<Term> &add);
  [[nodiscard]] static XBDD getSupport(const XADD<Term> &add);

  template <typename T>
  friend XADD<T> operator*(const XBDD<T> &bdd, const XADD<T> &add);
};

template <typename Term>
inline void swap(XBDD<Term> &lhs, XBDD<Term> &rhs) noexcept {
  lhs.swap(rhs);
}

template <typename Term>
XADD<Term> operator*(const XBDD<Term> &bdd, const XADD<Term> &add);

}  // namespace ddsolve

#endif /* XBDD_HPP */
