#ifndef LINEARTERM_HPP
#define LINEARTERM_HPP

#include <gmpxx.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <ostream>
#include <ranges>
#include <shared_mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "ArithmeticTypes.hpp"
#include "utils.hpp"

namespace ddsolve::expression {
using VariableId = int;

class LinearComparison;

struct Product {
  VariableId variable{};
  mpq_class coefficent;
};

enum class FinityType : uint8_t {
  kFinite = 0,
  kNegativeInfinite,
  kPositiveInfinite,
};

class LinearTerm {
 public:
  using ScalarType = mpq_class;
  using CoefficientType = mpq_class;
  using CoefficientPointer = std::shared_ptr<CoefficientType>;
  using ConstCoefficientPointer = std::shared_ptr<const CoefficientType>;
  using ScalarPointer = std::shared_ptr<ScalarType>;
  using ConstScalarPointer = std::shared_ptr<const ScalarType>;

  using Variable = VariableId;
  using Term = LinearTerm;
  using TermPointer = std::shared_ptr<LinearTerm>;
  using ConstTermPointer = std::shared_ptr<const LinearTerm>;

  using Comparison = LinearComparison;
  using ComparisonPointer = std::shared_ptr<LinearComparison>;
  using ConstComparisonPointer = std::shared_ptr<const LinearComparison>;

  static uint64_t ScalarType_get_id(
      const std::shared_ptr<const ScalarType> &s) {
    static std::shared_mutex map_mutex{};
    static std::map<std::shared_ptr<const ScalarType>, uint64_t,
                    indirection_less>
        map{};

    {
      std::shared_lock lock{map_mutex};
      const auto it{map.find(s)};
      if (it != map.end()) {
        return it->second;
      }
    }
    std::scoped_lock lock{map_mutex};
    const auto it{map.find(s)};
    if (!map.contains(s)) {
      const auto id = map.size();
      map[s] = id;
      return id;
    }
    return it->second;
  }

  static ScalarType scalarFromString(const std::string &str) {
    ScalarType scalar{str};
    scalar.canonicalize();
    return scalar;
  }

  static CoefficientType coefficientFromString(const std::string &str) {
    CoefficientType coefficent{str};
    coefficent.canonicalize();
    return coefficent;
  }

  LinearTerm() : sum_{Product{}}, hash_cache_{calc_hash()} {}

  explicit LinearTerm(std::vector<Product> sum) : sum_{std::move(sum)} {
    if (sum_.empty()) {
      sum_.emplace_back();
    }

    canonicalize();
    if (std::adjacent_find(sum_.begin(), sum_.end(),
                           [](const auto &a, const auto &b) {
                             return a.variable == b.variable;
                           }) != sum_.end()) {
      throw std::invalid_argument(
          "Tried to construct LinearTerm with multiple occurences"
          "of the same variable");
    }

    hash_cache_ = calc_hash();
  }

  explicit LinearTerm(FinityType finity_type)
      : finity_{finity_type}, hash_cache_{calc_hash()} {}

  [[nodiscard]] LinearTerm scale(const ScalarType scalar) const {
    if (!isFinite()) {
      return *this;
    }

    decltype(sum_) tmp{std::views::transform(
                           sum_,
                           [&scalar](auto const &i) {
                             return Product{i.variable, scalar * i.coefficent};
                           }) |
                       std::ranges::to<std::vector>()};

    return LinearTerm{std::move(tmp)};
  }

  [[nodiscard]] LinearTerm fusedScaleAdd(const ScalarType &scalar,
                                         const LinearTerm &other) const {
    if (scalar == 0) {
      return LinearTerm{};
    }

    const auto new_finity{scaleFinity(scalar)};
    if (new_finity == FinityType::kNegativeInfinite) {
      if (other.finity_ == FinityType::kPositiveInfinite) {
        return LinearTerm{};
      }

      return LinearTerm{new_finity};
    }
    if (new_finity == FinityType::kPositiveInfinite) {
      if (other.finity_ == FinityType::kNegativeInfinite) {
        return LinearTerm{};
      }
      return LinearTerm{new_finity};
    }

    if (!other.isFinite()) {
      return other;
    }

    auto it1 = sum_.begin();
    auto it2 = other.sum_.begin();

    decltype(sum_) new_sum{};
    new_sum.reserve(size() + other.size());

    while (it1 != sum_.end() || it2 != other.sum_.end()) {
      if (it1 != sum_.end() && it2 != other.sum_.end()) {
        if (it1->variable < it2->variable) {
          new_sum.push_back({it1->variable, scalar * it1->coefficent});
          ++it1;
        } else if (it1->variable > it2->variable) {
          new_sum.push_back({it2->variable, it2->coefficent});
          ++it2;
        } else {
          new_sum.push_back(Product{
              .variable = it1->variable,
              .coefficent = scalar * it1->coefficent + it2->coefficent});
          ++it1;
          ++it2;
        }
      } else if (it1 != sum_.end()) {
        new_sum.push_back({Product{.variable = it1->variable,
                                   .coefficent = scalar * it1->coefficent}});
        ++it1;
      } else {
        new_sum.push_back(*it2);
        ++it2;
      }
    }

    return LinearTerm{std::move(new_sum)};
  }

  [[nodiscard]] LinearTerm operator+(const LinearTerm &other) const {
    return fusedScaleAdd(ScalarType{1}, other);
  }

  LinearTerm operator-(const LinearTerm &other) const {
    return other.fusedScaleAdd(ScalarType{-1}, *this);
  }

  [[nodiscard]] bool operator==(const LinearTerm &other) const {
    if (hash() != other.hash() || size() != other.size() ||
        finity_ != other.finity_) {
      return false;
    }

    if (!isFinite()) {
      return true;
    }

    return std::ranges::all_of(std::views::zip(sum_, other.sum_),
                               [](const auto &p) {
                                 const auto &[lhs, rhs] = p;
                                 return lhs.variable == rhs.variable &&
                                        lhs.coefficent == rhs.coefficent;
                               });
  }

  [[nodiscard]] bool operator<(const LinearTerm &other) const {
    if (size() != other.size()) {
      return size() < other.size();
    }

    if (finity_ != other.finity_) {
      return finity_ < other.finity_;
    }

    for (const auto &[lhs, rhs] : std::views::zip(sum_, other.sum_)) {
      if (lhs.variable != rhs.variable || lhs.coefficent != rhs.coefficent) {
        return lhs.variable < rhs.variable || (lhs.variable == rhs.variable &&
                                               lhs.coefficent < rhs.coefficent);
      }
    }
    return false;
  }

  [[nodiscard]] LinearTerm substitute(const VariableId variable,
                                      const LinearTerm &other) const {
    if (!isFinite()) {
      return *this;
    }

    auto it{getProductFromVariable(variable)};
    if (it != sum_.end()) {
      const auto coefficent{it->coefficent};
      auto it1 = sum_.begin();
      auto it2 = other.sum_.begin();

      decltype(sum_) new_sum{};
      new_sum.reserve(size() + other.size());

      while (it1 != sum_.end() || it2 != other.sum_.end()) {
        if (it1 != sum_.end() && it1->variable == variable) {
          ++it1;
          continue;
        }

        if (it1 != sum_.end() && it2 != other.sum_.end()) {
          if (it1->variable < it2->variable) {
            new_sum.push_back(*it1);
            ++it1;
          } else if (it1->variable > it2->variable) {
            new_sum.push_back({it2->variable, coefficent * it2->coefficent});
            ++it2;
          } else {
            new_sum.push_back(Product{
                .variable = it1->variable,
                .coefficent = it1->coefficent + coefficent * it2->coefficent});
            ++it1;
            ++it2;
          }
        } else if (it1 != sum_.end()) {
          new_sum.push_back(*it1);
          ++it1;
        } else {
          new_sum.push_back(
              Product(it2->variable, coefficent * it2->coefficent));
          ++it2;
        }
      }

      // TODO(DB): find out if std::move was needed
      return LinearTerm{std::move(new_sum)};
    }

    return *this;
  }

  [[nodiscard]] size_t size() const {
    return sum_.size();
  }

  [[nodiscard]] auto begin() const {
    return sum_.begin();
  }

  [[nodiscard]] auto end() const {
    return sum_.end();
  }

  [[nodiscard]] bool isFinite() const {
    return finity_ == FinityType::kFinite;
  }

  [[nodiscard]] FinityType getFinity() const {
    return finity_;
  }

  [[nodiscard]] std::string str() const {
    if (finity_ == FinityType::kNegativeInfinite) {
      return "-∞";
    }
    if (finity_ == FinityType::kPositiveInfinite) {
      return "∞";
    }
    if (sum_.empty()) {
      return "0";
    }

    std::stringstream ss{};
    for (const auto &i : sum_) {
      ss << i.coefficent;
      if (i.variable != 0) {
        ss << "*x" << i.variable;
      }

      ss << " + ";
    }

    auto view = ss.view();
    view.remove_suffix(3);
    return std::string{view};
  }

  [[nodiscard]] uint64_t hash() const {
    return hash_cache_;
  }

 private:
  std::vector<Product> sum_;
  FinityType finity_{FinityType::kFinite};
  uint64_t hash_cache_;

  [[nodiscard]] FinityType scaleFinity(const ScalarType &scalar) const {
    auto new_finity{finity_};
    if (scalar < 0) {
      if (new_finity == FinityType::kPositiveInfinite) {
        new_finity = FinityType::kNegativeInfinite;
      } else if (new_finity == FinityType::kNegativeInfinite) {
        new_finity = FinityType::kPositiveInfinite;
      }
    } else if (scalar == 0) {
      new_finity = FinityType::kFinite;
    }

    return new_finity;
  }

  [[nodiscard]] uint64_t calc_hash() const {
    if (finity_ == FinityType::kNegativeInfinite) {
      // Generated through a fair dice
      constexpr uint64_t very_random{2576791930780013023};
      return very_random;
    }

    if (finity_ == FinityType::kPositiveInfinite) {
      // Generated through a fair dice
      constexpr uint64_t very_random{4331719511055861042};
      return very_random;
    }

    const auto tmp{
        std::views::transform(
            sum_,
            [](const Product &prod) {
              constexpr int kNrHashes = 4;
              std::array<size_t, kNrHashes> hashes{};

              const auto *den{prod.coefficent.get_den_mpz_t()};
              hashes[0] =
                  den->_mp_size > 0
                      ? SpanHash{}(std::span(den->_mp_d, abs(den->_mp_size)))
                      : 0;
              const auto *num{prod.coefficent.get_num_mpz_t()};
              hashes[1] =
                  num->_mp_size > 0
                      ? SpanHash{}(std::span(num->_mp_d, abs(num->_mp_size)))
                      : 1;
              hashes[2] = prod.variable;

              hashes[3] = mpq_sgn(prod.coefficent.get_mpq_t()) + 1;
              if (prod.variable < 0) {
                // for all x: 4 > sign(x) + 1
                hashes[3] |= 4;
              }

              return SpanHash{}(std::span(hashes.begin(), hashes.end()));
            }) |
        std::ranges::to<std::vector>()};

    const auto rval{SpanHash{}(std::span{tmp.begin(), tmp.end()})};
    return rval;
  }

  void canonicalize() {
    assert(0 < sum_.size());
    if (sum_.size() > 1) {
      std::ranges::sort(sum_, {}, &Product::variable);

      const auto e{std::ranges::remove(sum_.begin(), sum_.end(), 0,
                                       &Product::coefficent)};
      sum_.erase(e.begin(), e.end());
    }
    if (sum_.size() == 0) {
      sum_.emplace_back();
    }
  }

  [[nodiscard]] decltype(sum_)::iterator getProductFromVariable(
      const VariableId variable) {
    auto it{std::ranges::lower_bound(sum_, variable, {}, &Product::variable)};

    if (it != sum_.end() && it->variable == variable) {
      return it;
    }

    return sum_.end();
  }

  [[nodiscard]] decltype(sum_)::const_iterator getProductFromVariable(
      const VariableId variable) const {
    auto it{std::ranges::lower_bound(sum_, variable, {}, &Product::variable)};

    if (it != sum_.end() && it->variable == variable) {
      return it;
    }

    return sum_.end();
  }
};

inline LinearTerm operator*(const LinearTerm::ScalarType &s,
                            const LinearTerm &term) {
  return term.scale(s);
}

class LinearComparison {
 public:
  LinearComparison() = default;
  LinearComparison(LinearTerm term, const ComparisonType &type)
      : term_{std::move(term)}, type_{type} {}
  LinearComparison(const LinearTerm &lhs, const LinearTerm &rhs,
                   const ComparisonType &type)
      : term_{lhs - rhs}, type_{type} {}

  [[nodiscard]] ComparisonType getType() const {
    return type_;
  }

  [[nodiscard]] const LinearTerm &getTerm() const {
    return term_;
  }

  [[nodiscard]] std::string str() const {
    std::string comp;

    switch (type_) {
      case ComparisonType::kEquality:
        comp = "=";
        break;
      case ComparisonType::kInequality:
        comp = "!=";
        break;
      case ComparisonType::kLessThan:
        comp = "<";
        break;
      case ComparisonType::kLessThanOrEqual:
        comp = "<=";
        break;
      case ComparisonType::kGreaterThan:
        comp = ">";
        break;
      case ComparisonType::kGreaterThanOrEqual:
        comp = ">=";
        break;
      default:
        comp = "ERROR";
        break;
    }

    return term_.str() + ' ' + comp + ' ' + "0";
  }

  [[nodiscard]] LinearComparison substitute(const VariableId variable,
                                            const LinearTerm &other) const {
    return LinearComparison{term_.substitute(variable, other), type_};
  }

  [[nodiscard]] bool operator==(const LinearComparison &other) const {
    return type_ == other.type_ && term_ == other.term_;
  }

  [[nodiscard]] bool operator<(const LinearComparison &other) const {
    return std::make_tuple(term_, type_) <
           std::make_tuple(other.term_, other.type_);
  }

  [[nodiscard]] uint64_t hash() const {
    constexpr int kNrHashes = 2;
    std::array<size_t, kNrHashes> hashes{};

    hashes[0] = term_.hash();
    hashes[1] = static_cast<size_t>(type_);

    return SpanHash{}(std::span(hashes.begin(), hashes.end()));
  }

  struct WithParity {
    bool parity;
    std::shared_ptr<const LinearComparison> comparison;
  };

  [[nodiscard]] SmtResult trivialCheck() const {
    if (const auto check{trivialFinityCheck()}; check != SmtResult::kUnknown) {
      return check;
    }

    if (const auto check{trivialConstantCheck()};
        check != SmtResult::kUnknown) {
      return check;
    }

    if (const auto check{trivialLessThanCheck()};
        check != SmtResult::kUnknown) {
      return check;
    }

    return SmtResult::kUnknown;
  }

  [[nodiscard]] static SmtResult decideReal(
      const std::vector<WithParity> &conjunction, bool printModel);
  [[nodiscard]] static SmtResult decideUInt(
      const std::vector<WithParity> &conjunction, bool printModel);
  static void shutdown();

  [[nodiscard]] SmtResult trivialFinityCheck() const {
    const auto finity{term_.getFinity()};
    if (finity == FinityType::kNegativeInfinite) {
      return boolToResult(type_ == ComparisonType::kInequality ||
                          type_ == ComparisonType::kLessThan ||
                          type_ == ComparisonType::kLessThanOrEqual);
    }
    if (finity == FinityType::kPositiveInfinite) {
      return boolToResult(type_ == ComparisonType::kInequality ||
                          type_ == ComparisonType::kGreaterThan ||
                          type_ == ComparisonType::kGreaterThanOrEqual);
    }

    return SmtResult::kUnknown;
  }

  [[nodiscard]] SmtResult trivialConstantCheck() const {
    if (term_.size() == 1 && term_.begin()->variable == 0) {
      const auto &coefficent{term_.begin()->coefficent};
      switch (type_) {
        case ComparisonType::kEquality:
          return boolToResult(coefficent == 0);
        case ComparisonType::kInequality:
          return boolToResult(coefficent != 0);
        case ComparisonType::kLessThan:
          return boolToResult(coefficent < 0);
        case ComparisonType::kLessThanOrEqual:
          return boolToResult(coefficent <= 0);
        case ComparisonType::kGreaterThan:
          return boolToResult(coefficent > 0);
        case ComparisonType::kGreaterThanOrEqual:
          return boolToResult(coefficent >= 0);
      }
    }

    if (term_.size() == 0) {
      return boolToResult(type_ == ComparisonType::kEquality ||
                          type_ == ComparisonType::kLessThanOrEqual ||
                          type_ == ComparisonType::kGreaterThanOrEqual);
    }

    return SmtResult::kUnknown;
  }

  [[nodiscard]] SmtResult trivialLessThanCheck() const {
    if (type_ == ComparisonType::kLessThan) {
      bool geq{true};

      // all coefficients leq 0 does not suffice
      // needs a constant < 0 as otherwise term could be 0 which is not < 0
      bool leq{term_.begin()->variable == 0 && term_.begin()->coefficent < 0};

      for (const auto &i : term_) {
        if (i.coefficent >= 0) {
          leq &= false;
          geq &= true;
        } else if (i.coefficent <= 0) {
          geq &= false;
          leq &= true;
        }

        if (!leq && !geq) {
          break;
        }
      }

      assert(!leq || !geq);

      if (geq) {
        return SmtResult::kUnsat;
      }
      if (leq) {
        return SmtResult::kSat;
      }
    }

    return SmtResult::kUnknown;
  }

 private:
  LinearTerm term_;
  ComparisonType type_{};

};

}  // namespace ddsolve::expression

#endif /* LINEARTERM_HPP */
