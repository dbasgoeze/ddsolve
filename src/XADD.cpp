#include "XADD.hpp"

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

template <typename T, typename Term>
[[nodiscard]] T XDD<T, Term>::substitute(
    const Term::Variable &var, const Term::ConstTermPointer &term) const {
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
  for (const auto &[from, to] : std::views::zip(old_vars, new_reprs)) {
    reorder.put(from, to);
  }

  return T{{interface::substituteLeafs(getMtbdd().Compose(reorder),
                                       Substitution<Term>{var, term}),
            getOrder()}};
}

template <typename Term>
[[nodiscard]] XADD<Term> XBDD<Term>::ite(const XADD<Term> &b,
                                         const XADD<Term> &c) const {
  if (this->getOrder() != b.getOrder() || this->getOrder() != c.getOrder()) {
    throw std::invalid_argument{"Operation on DDs with different Orderings!"};
  }
  return XADD<Term>{*this, b, c};
}

template <typename Term>
[[nodiscard]] bool XADD<Term>::operator<(const XADD<Term> &other) const {
  return interface::compare(this->getMtbdd(), other.getMtbdd(),
                            expression::ComparisonType::kLessThan,
                            *this->getOrder());
}
template <typename Term>
[[nodiscard]] bool XADD<Term>::operator<=(const XADD<Term> &other) const {
  return interface::compare(this->getMtbdd(), other.getMtbdd(),
                            expression::ComparisonType::kLessThanOrEqual,
                            *this->getOrder());
}

template <typename Term>
[[nodiscard]] XADD<Term> XADD<Term>::operator+(const XADD<Term> &other) const {
  if (this->getOrder() != other.getOrder()) {
    throw std::invalid_argument{"Operation on DDs with different Orderings!"};
  }
  return {interface::add<Term>(this->getMtbdd(), other.getMtbdd()),
          this->getOrder()};
}

template <typename Term>
[[nodiscard]] XADD<Term> XADD<Term>::operator-(const XADD<Term> &other) const {
  if (this->getOrder() != other.getOrder()) {
    throw std::invalid_argument{"Operation on DDs with different Orderings!"};
  }
  return {interface::sub<Term>(this->getMtbdd(), other.getMtbdd()),
          this->getOrder()};
}

template <typename Term>
[[nodiscard]] XADD<Term> XADD<Term>::max(const XADD<Term> &other) const {
  if (this->getOrder() != other.getOrder()) {
    throw std::invalid_argument{"Operation on DDs with different Orderings!"};
  }
  return {interface::max<Term>(this->getMtbdd(), other.getMtbdd(),
                               this->getOrder()),
          this->getOrder()};
}

template <typename Term>
[[nodiscard]] XADD<Term> XADD<Term>::min(const XADD<Term> &other) const {
  if (this->getOrder() != other.getOrder()) {
    throw std::invalid_argument{"Operation on DDs with different Orderings!"};
  }
  return {interface::min<Term>(this->getMtbdd(), other.getMtbdd(),
                               this->getOrder()),
          this->getOrder()};
}

template <typename Term>
[[nodiscard]] XADD<Term> XADD<Term>::prune() const {
  return {interface::prune<Term>(this->getMtbdd()), this->getOrder()};
}

template <typename Term>
[[nodiscard]] XADD<Term> XADD<Term>::pruneFull() const {
  return {interface::pruneFull<Term>(this->getMtbdd(), *this->getOrder()),
          this->getOrder()};
}

template <typename Term>
[[nodiscard]] XADD<Term> XADD<Term>::scale(
    const Term::ConstScalarPointer &scalar) const {
  return {interface::scale<Term>(scalar, this->getMtbdd()), this->getOrder()};
}

template <typename Term>
[[nodiscard]] XADD<Term> XADD<Term>::undefinedToZero() const {
  return {interface::replaceLeaf<Term>(
              this->getMtbdd(), sylvan::Mtbdd::mtbddZero(),
              getTerm(std::make_shared<Term>()).getMtbdd()),
          this->getOrder()};
}

template <typename Term>
[[nodiscard]] XADD<Term> XADD<Term>::getTerm(
    const Term::ConstTermPointer &term) const {
  return {interface::getTerm(*term), this->getOrder()};
}

template <typename Term>
[[nodiscard]] Term::ConstTermPointer XADD<Term>::getTerm() const {
  return interface::getTerm<Term>(this->getMtbdd());
}

template <typename Term>
[[nodiscard]] XBDD<Term> XBDD<Term>::getPrunes(const XADD<Term> &add) {
  return {interface::getPrunes(add.getMtbdd(), *add.getOrder()),
          add.getOrder()};
}

template <typename Term>
[[nodiscard]] XBDD<Term> XBDD<Term>::getSupport(const XADD<Term> &add) {
  return {add.getMtbdd().Support(), add.getOrder()};
}

template <typename Term>
XADD<Term> operator*(const XBDD<Term> &bdd, const XADD<Term> &add) {
  if (bdd.getOrder() != add.getOrder()) {
    throw std::invalid_argument{"Operation on DDs with different Orderings!"};
  }
  return {bdd.getMtbdd() * add.getMtbdd(), add.getOrder()};
}

template <typename Term>
XADD<Term> XADD<Term>::operator/(const XADD<Term> &other) const
  requires requires(Term a, Term b) { a / b; }
{
  if (this->getOrder() != other.getOrder()) {
    throw std::invalid_argument{"Operation on DDs with different Orderings!"};
  }
  return {interface::div<Term>(this->getMtbdd(), other.getMtbdd()),
          this->getOrder()};
}

template <typename Term>
XADD<Term> XADD<Term>::operator*(const XADD<Term> &other) const
  requires requires(Term a, Term b) { a * b; }
{
  if (this->getOrder() != other.getOrder()) {
    throw std::invalid_argument{"Operation on DDs with different Orderings!"};
  }
  return {interface::mul<Term>(this->getMtbdd(), other.getMtbdd()),
          this->getOrder()};
}

template <typename Term>
XADD<Term> operator*(const typename Term::ConstScalarPointer &scalar,
                     const XADD<Term> &add) {
  return add.scale(scalar);
}

// instantiate templates
template class XBDD<expression::LinearTerm>;
template class XADD<expression::LinearTerm>;

template class XDD<XBDD<expression::LinearTerm>, expression::LinearTerm>;
template class XDD<XADD<expression::LinearTerm>, expression::LinearTerm>;

template XADD<expression::LinearTerm> operator*(
    const XBDD<expression::LinearTerm> &bdd,
    const XADD<expression::LinearTerm> &add);
template XADD<expression::LinearTerm> operator*(
    const expression::LinearTerm::ConstScalarPointer &scalar,
    const XADD<expression::LinearTerm> &add);

template class XBDD<expression::SmtTerm>;
template class XADD<expression::SmtTerm>;

template class XDD<XBDD<expression::SmtTerm>, expression::SmtTerm>;
template class XDD<XADD<expression::SmtTerm>, expression::SmtTerm>;

template XADD<expression::SmtTerm> operator*(
    const XBDD<expression::SmtTerm> &bdd, const XADD<expression::SmtTerm> &add);
template XADD<expression::SmtTerm> operator*(
    const expression::SmtTerm::ConstScalarPointer &scalar,
    const XADD<expression::SmtTerm> &add);

};  // namespace ddsolve
