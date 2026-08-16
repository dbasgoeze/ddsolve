#include <sys/sdt.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <map>
#include <memory>
#include <ranges>
#include <set>
#include <syncstream>

#include "ArithmeticTypes.hpp"
#include "LinearTerm.hpp"
#include "SmtWrapper.hpp"
#include "smt/smt_config.hpp"
#include "MetricMeasure.hpp"

namespace {

#if SMT_SOLVER == CVC5
using RealValuedWrapper = ddsolve::smt::Cvc5Wrapper<true>;
using IntValuedWrapper = ddsolve::smt::Cvc5Wrapper<false>;
#endif

#if SMT_SOLVER == Z3
using RealValuedWrapper = ddsolve::smt::Z3Wrapper<true>;
using IntValuedWrapper = ddsolve::smt::Z3Wrapper<false>;
#endif

#ifdef MEASURE_SMT
ddsolve::MetricMeasure<MEASURE_SMT_MAX_THREADS> gMeasure{};
#endif

template <typename Wrapper>
class State {
 public:
  using SmtResult = ddsolve::expression::SmtResult;
  [[nodiscard]] SmtResult decide(
      std::vector<ddsolve::expression::LinearComparison::WithParity>
          conjunction,
      const bool print_model) {
    auto var_ids{gatherVariables(conjunction)};

#ifdef USE_INCREMENTAL_SMT

    const auto mismatch_result{mismatch(conjunction)};

#ifdef SMT_DEBUG
    static std::mutex m{};
    m.lock();
    std::cout << "--- ---\n";
    for (const auto &i : last_querry_) {
      std::cout << (i.parity ? ' ' : '!') << i.comparison->str() << "; ";
    }
    std::cout << '\n';
    for (const auto &i : conjunction) {
      std::cout << (i.parity ? ' ' : '!') << i.comparison->str() << "; ";
    }
    std::cout << '\n';
    for (const auto &i :
         std::ranges::subrange(last_querry_.begin(), mismatch_result.in1)) {
      std::cout << (i.parity ? ' ' : '!') << i.comparison->str() << "; ";
    }
    std::cout << '\n';
#endif

    popAssertions(
        std::ranges::subrange(mismatch_result.in1, last_querry_.end()),
        var_ids);
    pushVariables(var_ids);

#ifdef SMT_DEBUG
    for (const auto &i : wrapper_.getAssertions()) {
      std::cout << i << "; ";
    }
    std::cout << '\n';
#endif

    pushAssertions(
        std::ranges::subrange(mismatch_result.in2, conjunction.end()));

    last_querry_ = std::move(conjunction);

#ifdef SMT_DEBUG
    for (const auto &i : wrapper_.getAssertions()) {
      std::cout << i << "; ";
    }
    std::cout << "\n---\n" << std::flush;
#endif


#ifdef MEASURE_SMT
    gMeasure.start();
#endif
    DTRACE_PROBE(ddsolve, solve_start);
    const auto rval{wrapper_.checkSat()};
    DTRACE_PROBE(ddsolve, solve_stop);
#ifdef MEASURE_SMT
    gMeasure.stop();
#endif

#else
    for (const auto &var_id : var_ids) {
      const auto term = wrapper_.mkComparison(
          zero_, variables_[var_id],
          ddsolve::expression::ComparisonType::kLessThanOrEqual);
      wrapper_.addAssertion(term);
    }
    wrapper_.push();
    for (const auto &comp : conjunction) {
      wrapper_.addAssertion(asExpression(comp));
    }

    const auto rval{wrapper_.checkSat()};
    wrapper_.pop();
#endif

#ifdef SMT_DEBUG
    if (rval == SmtResult::kSat) {
#else
    if (print_model && rval == SmtResult::kSat) {
#endif
      std::vector<Expression> variables{std::views::values(variables_) |
                                        std::ranges::to<std::vector>()};
      std::osyncstream oss{std::cout};

      oss << "{\n";
      // is_first hack is ugly
      bool is_first = true;
      for (auto &&[x, y] : std::views::drop(
               std::views::zip(variables, wrapper_.getValues(variables)), 1)) {
        if (!is_first) {
          oss << ",\n";
        }
        oss << '"' << x << '"' << ": " << y;
        is_first = false;
      }
      oss << "\n}\n";
    }
#ifdef SMT_DEBUG
    std::cout << "SatResult: " << ddsolve::expression::smtResultToStr(rval) << std::flush;
    m.unlock();
#endif

    return rval;
  }

 private:
  using Expression = Wrapper::Expression;

  template <std::ranges::forward_range R>
  void pushAssertions(R &&tmp_range) {
    for (const auto &comp : std::forward<R>(tmp_range)) {
      wrapper_.push();
      ++assertion_level_;
      wrapper_.addAssertion(asExpression(comp));
    }
  }

  template <std::ranges::forward_range R>
  void popAssertions(R &&tmp_range,
                     std::set<ddsolve::expression::VariableId> &var_ids) {
    for (const auto &i : std::forward<R>(tmp_range)) {
      auto last_variable_assertions{variable_assertions_.empty()
                                        ? -1
                                        : variable_assertions_.back().first};
      while (assertion_level_ == last_variable_assertions) {
        var_ids.merge(variable_assertions_.back().second);
        variable_assertions_.pop_back();
        wrapper_.pop();
        --assertion_level_;

        last_variable_assertions = variable_assertions_.empty()
                                       ? -1
                                       : variable_assertions_.back().first;
      }

      wrapper_.pop();
      --assertion_level_;
    }
  }

  void pushVariables(std::set<ddsolve::expression::VariableId> &var_ids) {
    if (!var_ids.empty()) {
      wrapper_.push();
      ++assertion_level_;
      variable_assertions_.emplace_back(assertion_level_, var_ids);

      for (const auto &var_id : var_ids) {
        const auto term = wrapper_.mkComparison(
            zero_, variables_[var_id],
            ddsolve::expression::ComparisonType::kLessThanOrEqual);
        wrapper_.addAssertion(term);
      }
    }
  }

  std::set<ddsolve::expression::VariableId> gatherVariables(
      const std::vector<ddsolve::expression::LinearComparison::WithParity>
          &conjunction) {
    std::set<ddsolve::expression::VariableId> ids;

    for (const auto &comp : conjunction) {
      for (const auto &i : comp.comparison->getTerm()) {
        if (!variables_.contains(i.variable)) {
          ids.insert(i.variable);
          const auto variable =
              wrapper_.mkConst("x_" + std::to_string(i.variable));

          variables_[i.variable] = variable;
        }
      }
    }

    return ids;
  }

  template <typename R>
  void push_formulas(R term_range) {
    for (const auto &term : term_range) {
      wrapper_.push();
      wrapper_.addAssertion(term);
      ++assertion_level_;
    }
  }

  auto mismatch(
      const std::vector<ddsolve::expression::LinearComparison::WithParity>
          &current_querry) {
    const auto rval{std::ranges::mismatch(
        last_querry_, current_querry, [](const auto &lhs, const auto &rhs) {
          return lhs.parity == rhs.parity && *lhs.comparison == *rhs.comparison;
        })};
    return rval;
  }

  Expression asExpression(
      const ddsolve::expression::LinearComparison::WithParity &comp) {
    const bool is_negated = !comp.parity;
    // This (hopefully) deals with infinity too
    if (const auto check{comp.comparison->trivialCheck()};
        check != SmtResult::kUnknown) {
      const auto tmp{is_negated ? ddsolve::expression::flip(check) : check};

      const bool value{tmp != SmtResult::kUnsat};
      return value ? wrapper_.mkTrue() : wrapper_.mkFalse();
    }

    std::vector<Expression> sum{};
    sum.reserve(comp.comparison->getTerm().size());
    for (const auto &product : comp.comparison->getTerm()) {
      const auto &var{variables_.at(product.variable)};
      const auto coefficent{wrapper_.mkReal(product.coefficent.get_str())};
      sum.push_back(wrapper_.mkProduct({coefficent, var}));
    }

    Expression rval = wrapper_.mkComparison(wrapper_.mkSum(sum), zero_,
                                            comp.comparison->getType());

    if (is_negated) {
      rval = wrapper_.mkNegation(rval);
    }

    return rval;
  }

  Wrapper wrapper_{};
  Expression zero_{wrapper_.mkReal("0")};

  // The '0' variable is actully the constant one
  std::map<ddsolve::expression::VariableId, Expression> variables_{
      {0, wrapper_.mkReal("1")}};

  std::vector<std::pair<int, std::set<ddsolve::expression::VariableId>>>
      variable_assertions_;
  size_t assertion_level_{};

  std::vector<ddsolve::expression::LinearComparison::WithParity> last_querry_;
};

template <typename Wrapper>
auto &getState() {
  thread_local auto state{std::make_unique<State<Wrapper>>()};
  return state;
}

}  // namespace

namespace ddsolve::expression {

[[nodiscard]] SmtResult LinearComparison::decideReal(
    const std::vector<LinearComparison::WithParity> &conjunction,
    const bool print_model) {
  STAP_PROBE(ddsolve, use_smt_real);
  auto &state{*getState<RealValuedWrapper>()};

  return state.decide(conjunction, print_model);
}

[[nodiscard]] SmtResult LinearComparison::decideUInt(
    const std::vector<LinearComparison::WithParity> &conjunction,
    const bool print_model) {
  STAP_PROBE(ddsolve, use_smt_uint);
  auto &state{*getState<IntValuedWrapper>()};

  return state.decide(conjunction, print_model);
}

void LinearComparison::shutdown() {
  static std::atomic_bool printed{false};
  getState<RealValuedWrapper>().reset();
  getState<IntValuedWrapper>().reset();


#ifdef MEASURE_SMT
  if (!printed.exchange(true, std::memory_order_relaxed)) {
    gMeasure.write();
  }
#endif
}

}  // namespace ddsolve::expression
