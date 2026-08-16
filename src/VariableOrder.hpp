#ifndef VARIABLEORDER_HPP
#define VARIABLEORDER_HPP

#include <algorithm>
#include <atomic>
#include <concepts>
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <sylvan_obj.hpp>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ArithmeticTypes.hpp"
#include "LinearTerm.hpp"
#include "utils.hpp"

namespace ddsolve {

using SubstitutionId = int;
using SylvanVariable = uint32_t;

template <typename Term>
class Substitution {
 public:
  Substitution(const Term::Variable &var, Term::ConstTermPointer term)
      : var_{var}, term_{std::move(term)} {}

  [[nodiscard]] uint64_t getId() const {
    return id_;
  }

  [[nodiscard]] Term::Variable getVariable() const {
    return var_;
  }

  [[nodiscard]] Term::ConstTermPointer getTerm() const {
    return term_;
  }

 private:
  inline static std::atomic_uint64_t next_id{};

  uint64_t id_{next_id.fetch_add(1, std::memory_order_relaxed)};
  Term::Variable var_;
  Term::ConstTermPointer term_;
};

template <typename Term>
class VariableOrder {
 public:
  using VariableOrderPointer = std::shared_ptr<VariableOrder<Term>>;
  using ConstVariableOrderPointer = std::shared_ptr<const VariableOrder<Term>>;

  void swap(VariableOrder &other) noexcept {
    std::swap(id_, other.id_);
    std::swap(references_, other.references_);
    std::swap(to_variable_, other.to_variable_);
    std::swap(to_comparison_, other.to_comparison_);
  }

  VariableOrder(const VariableOrder &other) = delete;
  VariableOrder() = default;
  VariableOrder(VariableOrder &&other) = default;
  ~VariableOrder() = default;

  VariableOrder &operator=(const VariableOrder &other) = delete;

  VariableOrder &operator=(VariableOrder &&other) noexcept = default;

  sylvan::Mtbdd obtainRepresentation(const Term::ConstComparisonPointer &comp) {
    if (const auto check{comp->trivialCheck()};
        check != expression::SmtResult::kUnknown) {
      return check == expression::SmtResult::kSat ? sylvan::Mtbdd::mtbddOne()
                                                  : sylvan::Mtbdd::mtbddZero();
    }

    // Can be expanded for other types in the future
    if constexpr (!std::is_same_v<Term, expression::LinearTerm>) {
      return sylvan::Mtbdd::mtbddVar(addVariable(comp));
    } else {
      const auto &term{comp->getTerm()};

      switch (comp->getType()) {
        [[likely]] case expression::ComparisonType::kLessThan:
          // recursion base case
          return sylvan::Mtbdd::mtbddVar(addVariable(comp));
        case expression::ComparisonType::kEquality:
          return !obtainRepresentation(
                     std::make_shared<const typename Term::Comparison>(
                         term.scale(-1),
                         expression::ComparisonType::kLessThan)) *
                 !obtainRepresentation(
                     std::make_shared<const typename Term::Comparison>(
                         term, expression::ComparisonType::kLessThan));
        case expression::ComparisonType::kInequality:
          return obtainRepresentation(
                     std::make_shared<const typename Term::Comparison>(
                         term.scale(-1),
                         expression::ComparisonType::kLessThan)) +
                 obtainRepresentation(
                     std::make_shared<const typename Term::Comparison>(
                         term, expression::ComparisonType::kLessThan));
        case expression::ComparisonType::kLessThanOrEqual:
          return !obtainRepresentation(
              std::make_shared<const typename Term::Comparison>(
                  term.scale(-1), expression::ComparisonType::kLessThan));
        case expression::ComparisonType::kGreaterThan:
          return obtainRepresentation(
              std::make_shared<const typename Term::Comparison>(
                  term.scale(-1), expression::ComparisonType::kLessThan));
        case expression::ComparisonType::kGreaterThanOrEqual:
          return !obtainRepresentation(
              std::make_shared<const typename Term::Comparison>(
                  term, expression::ComparisonType::kLessThan));
        [[unlikely]] default:
          return sylvan::Mtbdd::mtbddZero();
      }
    }
  }

  SylvanVariable addVariable(
      const typename Term::ConstComparisonPointer &comp) {
    if constexpr (std::is_same_v<Term, expression::LinearTerm>) {
      if (comp->getType() != expression::ComparisonType::kLessThan) {
        throw std::invalid_argument{
            "Tried to add non canonical comparison as sylvan variable."};
      }
    }
    const auto it{to_variable_.find(comp)};
    if (it != to_variable_.end()) {
      return it->second;
    }

    const auto new_var = to_variable_.size();
    to_variable_[comp] = new_var;
    to_comparison_[new_var] = comp;

    assert(to_variable_.size() == to_comparison_.size());
    return new_var;
  }

  template <typename R>
    requires std::ranges::forward_range<R> && std::ranges::sized_range<R> &&
             std::convertible_to<std::ranges::range_value_t<R>,
                                 const typename Term::ConstComparisonPointer>
  void addVariables(const R &comps) {
    for (const auto &comp : comps) {
      addVariable(comp);
    }

    assert(to_variable_.size() == to_comparison_.size());
  }

  [[nodiscard]] Term::ConstComparisonPointer getComparison(
      SylvanVariable var) const {
    const auto it{to_comparison_.find(var)};

    if (it != to_comparison_.end()) {
      return it->second;
    }

    return {};
  }

  [[nodiscard]] std::optional<SylvanVariable> getVariable(
      const Term::ConstComparisonPointer &comp) const {
    const auto it{to_variable_.find(comp)};

    if (it != to_variable_.end()) {
      return it->second;
    }

    return {};
  }

  [[nodiscard]] std::vector<SylvanVariable> getVariables() const {
    return std::views::elements<1>(to_variable_) |
           std::ranges::to<std::vector>();
  }

  [[nodiscard]] std::vector<typename Term::ConstComparisonPointer>
  getComparisons() const {
    return std::views::elements<0>(to_variable_) |
           std::ranges::to<std::vector>();
  }

  template <typename R>
    requires std::ranges::forward_range<R> && std::ranges::sized_range<R> &&
             std::convertible_to<std::ranges::range_value_t<R>,
                                 const SylvanVariable>
  std::vector<sylvan::Mtbdd> substitute(const Term::Variable &var,
                                        const Term::Term &term,
                                        const R &old_vars) {
    return std::views::transform(
               old_vars,
               [this, &var, &term](const auto &i) {
                 return obtainRepresentation(
                     std::make_shared<const typename Term::Comparison>(
                         getComparison(i)->substitute(var, term)));
               }) |
           std::ranges::to<std::vector>();
  }

  void registerMtbdd(const std::weak_ptr<sylvan::Mtbdd> &mtbdd) {
    if (references_.size() == references_.capacity()) {
      // realloc would take place...
      collectGarbidge();
    }
    references_.push_back(mtbdd);
  }

  void collectGarbidge() {
    std::erase_if(references_, [](auto&& ref){return ref.expired();});

    std::ranges::sort(references_, std::owner_less{});
    const auto unique_deleter{
        std::ranges::unique(references_, [](const auto &lhs, const auto &rhs) {
          return !lhs.owner_before(rhs) && !rhs.owner_before(lhs);
        })};
    references_.erase(unique_deleter.begin(), unique_deleter.end());
  }

  [[nodiscard]] uint64_t getId() const {
    return id_;
  }

 private:
  inline static std::atomic_uint64_t next_id{};

  uint64_t id_{next_id.fetch_add(1, std::memory_order_relaxed)};

  std::vector<std::weak_ptr<sylvan::Mtbdd>> references_;

  std::unordered_map<typename Term::ConstComparisonPointer, SylvanVariable,
                     IndirectHash<typename Term::Comparison>,
                     indirection_equal_to>
      to_variable_;
  std::unordered_map<SylvanVariable, typename Term::ConstComparisonPointer>
      to_comparison_;
};

template <typename Term>
class OrderedMtbddPointer {
 public:
  OrderedMtbddPointer(const std::shared_ptr<VariableOrder<Term>> &order,
                      const sylvan::Mtbdd &mtbdd)
      : ptr_{std::make_shared<sylvan::Mtbdd>(mtbdd)} {
    order->registerMtbdd(ptr_);
  }

  explicit OrderedMtbddPointer(
      const std::shared_ptr<VariableOrder<Term>> &order)
      : OrderedMtbddPointer{order, {}} {}

  sylvan::Mtbdd &operator*() const noexcept {
    return *ptr_;
  }

  sylvan::Mtbdd *operator->() const noexcept {
    return ptr_.get();
  }

  constexpr explicit operator bool() const noexcept {
    return true;
  }

 private:
  std::shared_ptr<sylvan::Mtbdd> ptr_;
};

}  // namespace ddsolve

#endif /* VARIABLEORDER_HPP */
