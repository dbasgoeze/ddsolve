#ifndef Z3WRAPPER_HPP
#define Z3WRAPPER_HPP

#include <z3++.h>

#include <ranges>

#include "LinearTerm.hpp"

namespace ddsolve::smt {

using Z3Expression = std::optional<z3::expr>;

template <bool real_valued>
class Z3Wrapper {
 public:
  using Expression = Z3Expression;

  [[nodiscard]] static constexpr bool isRealValued() {
    return real_valued;
  }

  [[nodiscard]] static constexpr bool isIntValued() {
    return !real_valued;
  }

  [[nodiscard]] Expression mkReal(const std::string &s) {
    return context_.real_val(s.c_str());
  }

  [[nodiscard]] Expression mkComparison(const Expression &lhs,
                                        const Expression &rhs,
                                        const expression::ComparisonType type) {
    if (lhs && rhs) {
      switch (type) {
        case expression::ComparisonType::kEquality:
          return *lhs == *rhs;
        case expression::ComparisonType::kInequality:
          return *lhs != *rhs;
        case expression::ComparisonType::kLessThan:
          return *lhs < *rhs;
        case expression::ComparisonType::kLessThanOrEqual:
          return *lhs <= *rhs;
        case expression::ComparisonType::kGreaterThan:
          return *lhs > *rhs;
        case expression::ComparisonType::kGreaterThanOrEqual:
          return *lhs >= *rhs;
      }
    }

    [[unlikely]] return {};
  }

  [[nodiscard]] Expression mkProduct(const std::vector<Expression> &factors) {
    if (factors.empty()) {
      return context_.real_val(1);
    }
    if (factors.size() == 1) {
      return factors[0];
    }
    if (!factors[0] || !factors[1]) {
      [[unlikely]] return {};
    }

    Expression rval{(*factors[0]) * (*factors[1])};
    for (const Expression &e : std::views::drop(factors, 2)) {
      if (!e) {
        [[unlikely]] return {};
      }
      rval = (*rval) * (*e);
    }
    return rval;
  }

  [[nodiscard]] Expression mkSum(const std::vector<Expression> &summands) {
    if (summands.empty()) {
      return context_.real_val(0);
    }
    if (summands.size() == 1) {
      return summands[0];
    }

    if (!summands[0] || !summands[1]) {
      [[unlikely]] return {};
    }

    Expression rval{*summands[0] + *summands[1]};
    for (const Expression &e : std::views::drop(summands, 2)) {
      if (!e) {
        [[unlikely]] return {};
      }
      rval = (*rval) + (*e);
    }
    return rval;
  }

  [[nodiscard]] Expression mkNegation(const Expression &expr) {
    return expr.transform([](const auto &e) { return !e; });
  }

  [[nodiscard]] Expression mkConst(const std::string &symbol) {
    if constexpr (real_valued) {
      return context_.real_const(symbol.c_str());
    }
    return context_.int_const(symbol.c_str());
  }

  [[nodiscard]] Expression mkTrue() {
    return context_.bool_val(true);
  }

  [[nodiscard]] Expression mkFalse() {
    return context_.bool_val(false);
  }

  [[nodiscard]] std::vector<Expression> getAssertions() const {
    auto &&assertions{solver_.assertions()};
    std::vector<Expression> rval{};
    rval.reserve(assertions.size());

    for (auto &&i : assertions) {
      rval.emplace_back(i);
    }

    return rval;
  }

  void addAssertion(const Expression &expr) {
    if (expr) {
      solver_.add(*expr);
    }
  }

  void push() {
    solver_.push();
  }

  void pop(const uint32_t n = 1) {
    solver_.pop(n);
  }

  [[nodiscard]] ddsolve::expression::SmtResult checkSat() {
    const auto result {solver_.check()};

    if (result == z3::check_result::sat) {
      return ddsolve::expression::SmtResult::kSat;
    }
    if (result == z3::check_result::unsat) {
      return ddsolve::expression::SmtResult::kUnsat;
    }

    return ddsolve::expression::SmtResult::kUnknown;
  }

  [[nodiscard]]
  std::vector<Expression> getValues(
      const std::vector<Expression> &terms) const {
    const auto model{solver_.get_model()};

    return std::views::transform(terms,
                                 [&model](const auto &i) {
                                   return i.transform([&model](const auto &v) {
                                     return model.eval(v);
                                   });
                                 }) |
           std::ranges::to<std::vector>();
  }

 private:
  z3::context context_;
  z3::solver solver_{context_};
};

}  // namespace ddsolve::smt

// TODO(DB) This is ugly
inline std::ostream &operator<<(std::ostream &out,
                                const ddsolve::smt::Z3Expression &expr) {
  if (expr) {
    return out << *expr;
  }

  return out << "Error!";
}

#endif /* Z3WRAPPER_HPP */
