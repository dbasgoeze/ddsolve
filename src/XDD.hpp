#ifndef XDD_HPP
#define XDD_HPP
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <sylvan_obj.hpp>

#include "LinearTerm.hpp"
#include "VariableOrder.hpp"
#include "gsl.hpp"

namespace ddsolve {

template <typename Term>
class XBDD;

template <typename T, typename Term>
class XDD {
 protected:
  // private as I don't trust the user to uphold the invariant
  // that order is actually the order of the MTBDD
  XDD(const sylvan::Mtbdd &mtbdd,
      VariableOrder<Term>::VariableOrderPointer order)
      : order_{std::move(order)}, mtbdd_{order_, mtbdd} {}

  ~XDD() = default;

 public:
  void swap(XDD &other) noexcept {
    std::swap(mtbdd_, other.mtbdd_);
    std::swap(order_, other.order_);
  };

  explicit XDD(VariableOrder<Term>::VariableOrderPointer order)
      : XDD({}, order) {}

  XDD() = default;
  XDD(const XDD &other) = default;
  XDD(XDD &&other) = default;
  XDD &operator=(const XDD &other) = default;
  XDD &operator=(XDD &&other) noexcept = default;

  bool operator==(const XDD<T, Term> &other) const {
    return (order_ == other.order_) && getMtbdd() == other.getMtbdd();
  }

  [[nodiscard]] Term::ConstComparisonPointer getTopVar() const {
    return order_->getComparison(getMtbdd().TopVar());
  }

  [[nodiscard]] T getThen() const {
    return T{{getMtbdd().Then(), order_}};
  }

  [[nodiscard]] T getElse() const {
    return T{{getMtbdd().Else(), order_}};
  }

  [[nodiscard]] std::string toDot() const {
    gsl::owner<FILE *> file = std::tmpfile();
    if (file == nullptr) {
      throw std::runtime_error{"Can't create tmpfile!"};
    }

    sylvan::mtbdd_fprintdot(file, getMtbdd().GetMTBDD());

    std::fflush(file);
    if (std::fseek(file, 0, SEEK_END) != 0) {
      throw std::runtime_error{"Can't seek in tmpfile!"};
    }
    const auto filesize{std::ftell(file)};
    if (std::fseek(file, 0, SEEK_SET) != 0) {
      throw std::runtime_error{"Can't seek in tmpfile!"};
    }
    std::string rval{};
    rval.resize(filesize);
    if (std::fread(rval.data(), 1, filesize, file) != filesize) {
      throw std::runtime_error{"Read in tmpfile was short!"};
    }
    std::fclose(file);

    return rval;
  }

  [[nodiscard]] bool isLeaf() const {
    return getMtbdd().isLeaf();
  }

  [[nodiscard]] bool isTrue() const {
    return isLeaf() && !getMtbdd().isZero();
  }

  [[nodiscard]] bool isFalse() const {
    return getMtbdd().isZero();
  }

  [[nodiscard]] size_t getNodeCount() {
    return getMtbdd().NodeCount();
  }

  [[nodiscard]] T withOrder(
      const VariableOrder<Term>::VariableOrderPointer &order) const {
    order->addVariables(order_->getComparisons());

    sylvan::MtbddMap reorder{};
    for (const auto &from : order_->getVariables()) {
      const auto &to{order->getVariable(order_->getComparison(from))};

      assert(to);
      reorder.put(from, sylvan::Bdd::bddVar(*to));
    }

    return T{{getMtbdd().Compose(reorder), order}};
  }

  [[nodiscard]] T substitute(const Term::Variable &var,
                             const Term::ConstTermPointer &term) const;

  auto &getOrder() const {
    return order_;
  }

  auto &getMtbdd() const {
    return *mtbdd_;
  }

 private:
  VariableOrder<Term>::VariableOrderPointer order_{
      std::make_shared<VariableOrder<Term>>()};
  OrderedMtbddPointer<Term> mtbdd_{order_};
};
}  // namespace ddsolve

#endif /* XDD_HPP */
