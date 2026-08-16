#include "SylvanInterface.h"

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <ranges>
#include <shared_mutex>
#include <stdexcept>
#include <utility>

#include "ArithmeticTypes.hpp"
#include "LinearTerm.hpp"
#include "SmtTerm.hpp"
#include "VariableOrder.hpp"
#include "sylvan/SylvanInterface.hpp"
#include "sylvan/sylvan_ddsolve.h"
#include "sylvan/sylvan_ddsolve_config.h"
#include "sylvan_obj.hpp"

// dirty hack until sylvan refactor
extern "C" {
int gPrintModelsFlag = 0;
}

namespace {

using ddsolve::expression::LinearTerm;
using ddsolve::expression::SmtTerm;

const LinearTerm::ConstScalarPointer* toPtr(
    ddsolve::interface::LinearScalarTypeHandle handle) {
  return reinterpret_cast  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
      <const LinearTerm::ConstScalarPointer*>(handle);
}

const LinearTerm::ConstTermPointer* toPtr(
    ddsolve::interface::LinearTermHandle handle) {
  return reinterpret_cast  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
      <const LinearTerm::ConstTermPointer*>(handle);
}

const ddsolve::Substitution<LinearTerm>* toPtr(
    ddsolve::interface::LinearSubstitutionHandle handle) {
  return reinterpret_cast  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
      <const ddsolve::Substitution<LinearTerm>*>(handle);
}

const ddsolve::VariableOrder<LinearTerm>* toPtr(
    ddsolve::interface::LinearVariableOrderHandle handle) {
  return reinterpret_cast  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
      <const ddsolve::VariableOrder<LinearTerm>*>(handle);
}

const LinearTerm::Term& toRef(
    const ddsolve::interface::LinearTermHandle handle) {
  return **toPtr(handle);
}

const LinearTerm::ScalarType& toRef(
    const ddsolve::interface::LinearScalarTypeHandle handle) {
  return **toPtr(handle);
}

ddsolve::interface::LinearTermHandle toHandle(
    const LinearTerm::ConstTermPointer* ptr) {
  return reinterpret_cast  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
      <const ddsolve::interface::LinearTermHandle>(ptr);
}

ddsolve::interface::LinearScalarTypeHandle toHandle(
    const LinearTerm::ConstScalarPointer* ptr) {
  return reinterpret_cast  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
      <const ddsolve::interface::LinearScalarTypeHandle>(ptr);
}

ddsolve::interface::LinearSubstitutionHandle toHandle(
    const ddsolve::Substitution<LinearTerm>* ptr) {
  return reinterpret_cast  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
      <const ddsolve::interface::LinearSubstitutionHandle>(ptr);
}

ddsolve::interface::LinearVariableOrderHandle toHandle(
    const ddsolve::VariableOrder<LinearTerm>* ptr) {
  return reinterpret_cast  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
      <const ddsolve::interface::LinearVariableOrderHandle>(ptr);
}

const SmtTerm::ConstTermPointer* toPtr(
    ddsolve::interface::SmtTermHandle handle) {
  return reinterpret_cast  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
      <const SmtTerm::ConstTermPointer*>(handle);
}

const ddsolve::Substitution<SmtTerm>* toPtr(
    ddsolve::interface::SmtSubstitutionHandle handle) {
  return reinterpret_cast  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
      <const ddsolve::Substitution<SmtTerm>*>(handle);
}

const ddsolve::VariableOrder<SmtTerm>* toPtr(
    ddsolve::interface::SmtVariableOrderHandle handle) {
  return reinterpret_cast  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
      <const ddsolve::VariableOrder<SmtTerm>*>(handle);
}

const SmtTerm::Term& toRef(const ddsolve::interface::SmtTermHandle handle) {
  return **toPtr(handle);
}

ddsolve::interface::SmtTermHandle toHandle(
    const SmtTerm::ConstTermPointer* ptr) {
  return reinterpret_cast  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
      <const ddsolve::interface::SmtTermHandle>(ptr);
}

ddsolve::interface::SmtSubstitutionHandle toHandle(
    const ddsolve::Substitution<SmtTerm>* ptr) {
  return reinterpret_cast  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
      <const ddsolve::interface::SmtSubstitutionHandle>(ptr);
}

ddsolve::interface::SmtVariableOrderHandle toHandle(
    const ddsolve::VariableOrder<SmtTerm>* ptr) {
  return reinterpret_cast  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
      <const ddsolve::interface::SmtVariableOrderHandle>(ptr);
}

template <typename Term>
struct TermPairCompare {
  using TermPair = std::pair<typename Term::ConstTermPointer,
                             typename Term::ConstTermPointer>;

  bool operator()(const TermPair& lhs, const TermPair& rhs) const {
    return *lhs.first < *rhs.first ||
           (*lhs.first == *rhs.first && *lhs.second < *rhs.second);
  }
};

template <typename Term>
class MinMaxHelper {
 public:
  using TermPairSet = std::map<typename TermPairCompare<Term>::TermPair,
                               ddsolve::SylvanVariable, TermPairCompare<Term>>;

  explicit MinMaxHelper(
      ddsolve::VariableOrder<Term>::VariableOrderPointer order)
      : order_{std::move(order)} {}

  [[nodiscard]] uint64_t getId() const {
    // We need 1 bit for the cache
    // to indicate min/max
    return id_ << 1;
  }

  void addPair(const Term::ConstTermPointer& lhs,
               const Term::ConstTermPointer& rhs) {
    const auto p = std::make_pair(lhs, rhs);
    {
      std::shared_lock lock{m_};
      if (term_pairs_.contains(p)) {
        return;
      }
    }
    {
      std::scoped_lock lock(m_);
      term_pairs_[p] = 0;
    }
  }

  void finalize() {
    std::scoped_lock lock(m_);
    auto comps{
        std::views::transform(
            term_pairs_,
            [](const auto& map_pair) {
              const auto& term_pair = map_pair.first;
              const auto& [lhs, rhs] = term_pair;

              const auto comp{std::make_shared<const typename Term::Comparison>(
                  *lhs, *rhs, ddsolve::expression::ComparisonType::kLessThan)};

              return std::make_tuple(lhs, rhs, comp);
            }) |
        std::ranges::to<std::vector>()};

    for (const auto& [lhs, rhs, comp] : comps) {
      term_pairs_[std::make_pair(lhs, rhs)] = order_->addVariable(comp);
    }
  }

  ddsolve::SylvanVariable getVar(const Term::ConstTermPointer& lhs,
                                 const Term::ConstTermPointer& rhs) {
    const auto p = std::make_pair(lhs, rhs);
    std::shared_lock lock{m_};
    const auto it{term_pairs_.find(p)};
    if (it != term_pairs_.end()) {
      return it->second;
    }

    // unreachable hopefully
    assert(false);
    return 0;
  }

  [[nodiscard]] auto getOrderHandle() const {
    return toHandle(order_.get());
  }

 private:
  inline static std::atomic_uint64_t next_id{};

  uint64_t id_{next_id.fetch_add(1, std::memory_order_relaxed)};
  std::shared_mutex m_;
  ddsolve::VariableOrder<Term>::VariableOrderPointer order_;
  TermPairSet term_pairs_;
};

MinMaxHelper<LinearTerm>* toPtr(
    ddsolve::interface::LinearMinMaxHelperHandle handle) {
  return reinterpret_cast  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
      <MinMaxHelper<LinearTerm>*>(handle);
}

MinMaxHelper<LinearTerm>& toRef(
    ddsolve::interface::LinearMinMaxHelperHandle handle) {
  return *toPtr(handle);
}

ddsolve::interface::LinearMinMaxHelperHandle toHandle(
    MinMaxHelper<LinearTerm>* ptr) {
  return reinterpret_cast  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
      <ddsolve::interface::LinearMinMaxHelperHandle>(ptr);
}

MinMaxHelper<SmtTerm>* toPtr(ddsolve::interface::SmtMinMaxHelperHandle handle) {
  return reinterpret_cast  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
      <MinMaxHelper<SmtTerm>*>(handle);
}

MinMaxHelper<SmtTerm>& toRef(ddsolve::interface::SmtMinMaxHelperHandle handle) {
  return *toPtr(handle);
}

ddsolve::interface::SmtMinMaxHelperHandle toHandle(MinMaxHelper<SmtTerm>* ptr) {
  return reinterpret_cast  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
      <ddsolve::interface::SmtMinMaxHelperHandle>(ptr);
}

template <typename Term>
void ddToConjunction(
    const sylvan::MTBDD& dd, const ddsolve::VariableOrder<Term>& order,
    std::vector<typename Term::Comparison::WithParity>& conjunction) {
  if (sylvan::mtbdd_isleaf(dd) != 0) {
    return;
  }

  if (sylvan::mtbdd_gethigh(dd) == sylvan::mtbdd_false) {
    const auto comp{order.getComparison(sylvan::mtbdd_getvar(dd))};
    if (comp) {
      conjunction.emplace_back(false, comp);
    }

    ddToConjunction(sylvan::mtbdd_getlow(dd), order, conjunction);
  } else {
    assert(sylvan::mtbdd_getlow(dd) == sylvan::mtbdd_false);
    const auto comp{order.getComparison(sylvan::mtbdd_getvar(dd))};
    if (comp) {
      conjunction.emplace_back(true, comp);
    }

    ddToConjunction(sylvan::mtbdd_gethigh(dd), order, conjunction);
  }
}

template <typename Term>
std::vector<typename Term::Comparison::WithParity> ddToConjunction(
    const sylvan::MTBDD& dd, const ddsolve::VariableOrder<Term>& order) {
  std::vector<typename Term::Comparison::WithParity> conjunction{};
  conjunction.reserve(sylvan::mtbdd_nodecount(dd));
  ddToConjunction(dd, order, conjunction);
  return conjunction;
}

};  // namespace

namespace ddsolve::interface {

// C part
extern "C" {

uint64_t LinearScalarType_get_id(LinearScalarTypeHandle handle) {
  return LinearTerm::ScalarType_get_id(*toPtr(handle));
}

LinearTermHandle LinearTerm_copy(LinearTermHandle handle) {
  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
  return toHandle(new LinearTerm::ConstTermPointer{*toPtr(handle)});
}

void LinearTerm_destroy(LinearTermHandle handle) {
  delete toPtr(handle);  // NOLINT(cppcoreguidelines-owning-memory)
}

int LinearTerm_equals(LinearTermHandle lhs, LinearTermHandle rhs) {
  return static_cast<int>(toRef(lhs) == toRef(rhs));
}

LinearTermHandle LinearTerm_add(LinearTermHandle lhs, LinearTermHandle rhs) {
  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
  return toHandle(new LinearTerm::ConstTermPointer{
      std::make_shared<LinearTerm>(toRef(lhs) + toRef(rhs))});
}

LinearTermHandle LinearTerm_sub(LinearTermHandle lhs, LinearTermHandle rhs) {
  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
  return toHandle(new LinearTerm::ConstTermPointer{
      std::make_shared<LinearTerm>(toRef(lhs) - toRef(rhs))});
}

LinearTermHandle LinearTerm_scale(LinearTermHandle term_handle,
                                  LinearScalarTypeHandle scalar_handle) {
  return toHandle(
      // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
      new LinearTerm::ConstTermPointer{std::make_shared<LinearTerm>(
          toRef(scalar_handle) * toRef(term_handle))});
}

LinearTermHandle LinearTerm_substitute(
    LinearTermHandle term_handle,
    LinearSubstitutionHandle substitution_handle) {
  const auto& substitution{*toPtr(substitution_handle)};

  return toHandle(
      // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
      new LinearTerm::ConstTermPointer{std::make_shared<LinearTerm>(
          (*toPtr(term_handle))
              ->substitute(substitution.getVariable(),
                           *substitution.getTerm()))});
}

uint64_t LinearTerm_hash(LinearTermHandle handle) {
  return toRef(handle).hash();
}

char* LinearTerm_to_str(LinearTermHandle handle, char* buf, size_t bufsize) {
  const auto str{toRef(handle).str()};
  if (bufsize > str.length() + 1) {
    strncpy(buf, str.c_str(), bufsize);
    return buf;
  }

  // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
  auto* newbuf{static_cast<char*>(malloc(str.length() + 2))};
  strncpy(newbuf, str.c_str(), str.length() + 1);
  return newbuf;
}

SmtResult LinearTerm_trivial_check(LinearTermHandle lhs, LinearTermHandle rhs,
                                   ComparisonType comp_type) {
  const auto rval{
      LinearTerm::Comparison{toRef(lhs), toRef(rhs),
                             static_cast<expression::ComparisonType>(comp_type)}
          .trivialCheck()};

  return static_cast<SmtResult>(rval);
}

SmtResult LinearVariableOrder_decide(LinearVariableOrderHandle handle,
                                     MTBDD history) {
  if (history == sylvan::mtbdd_true) {
    return SMTRESULT_SAT;
  }
  sylvan::Mtbdd dd{history};
  const auto& order{*toPtr(handle)};
  auto conjunction{ddToConjunction(dd.GetMTBDD(), order)};
  assert(!conjunction.empty());

  if (const auto trivial_check{conjunction.back().comparison->trivialCheck()};
      trivial_check != expression::SmtResult::kUnknown) {
    if ((trivial_check == expression::SmtResult::kSat) !=
        conjunction.back().parity) {
      return SMTRESULT_UNSAT;
    }
  }

#ifdef REAL_PRUNEING
  return static_cast<SmtResult>(
      LinearTerm::Comparison::decideReal(conjunction, gPrintModelsFlag));
#else
  return static_cast<SmtResult>(
      LinearTerm::Comparison::decideUInt(conjunction, gPrintModelsFlag));
#endif
}

int LinearVariableOrder_compare(LinearVariableOrderHandle handle,
                                LinearTermHandle lhs, LinearTermHandle rhs,
                                MTBDD history, ComparisonType comp_type) {
  sylvan::Mtbdd dd{history};
  const auto& order{*toPtr(handle)};
  auto conjunction{ddToConjunction(dd.GetMTBDD(), order)};

  conjunction.emplace_back(
      false, std::make_shared<const LinearTerm::Comparison>(
                 toRef(lhs), toRef(rhs),
                 static_cast<expression::ComparisonType>(comp_type)));

  switch (LinearTerm::Comparison::decideUInt(conjunction, gPrintModelsFlag)) {
    case expression::SmtResult::kUnsat:
      return 1;
    case expression::SmtResult::kSat:
      return 0;
    case expression::SmtResult::kUnknown:
      throw std::runtime_error{
          "Could not prove a comparison: Smt solver returned unknown!"};
  }
  std::unreachable();
}

/*
SmtResult LinearVariableOrder_compare_real(LinearVariableOrderHandle handle,
                                           LinearTermHandle lhs,
                                           LinearTermHandle rhs, MTBDD history,
                                           ComparisonType comp_type) {
  sylvan::Mtbdd dd{history};
  const auto& order{*toPtr(handle)};
  auto conjunction{ddToConjunction(dd.GetMTBDD(), order)};

  conjunction.emplace_back(
      false, std::make_shared<const LinearTerm::Comparison>(
                 toRef(lhs), toRef(rhs),
                 static_cast<expression::ComparisonType>(comp_type)));

  return static_cast<SmtResult>(expression::flip(
      LinearTerm::Comparison::decideReal(conjunction, gPrintModelsFlag)));
}
*/

uint64_t LinearVariableOrder_get_id(LinearVariableOrderHandle handle) {
  return toPtr(handle)->getId();
}

void LinearVariableOrder_shutdown() {
  LinearTerm::Comparison::shutdown();
}

uint64_t LinearSubstitution_get_id(LinearSubstitutionHandle handle) {
  return toPtr(handle)->getId();
}

uint64_t LinearMinMaxHelper_get_id(LinearMinMaxHelperHandle handle) {
  return toRef(handle).getId();
}

void LinearMinMaxHelper_add_pair(LinearMinMaxHelperHandle handle,
                                 LinearTermHandle lhs, LinearTermHandle rhs) {
  toRef(handle).addPair(*toPtr(lhs), *toPtr(rhs));
}

void LinearMinMaxHelper_finalize(LinearMinMaxHelperHandle handle) {
  toRef(handle).finalize();
}

uint32_t LinearMinMaxHelper_get_var(LinearMinMaxHelperHandle handle,
                                    LinearTermHandle lhs,
                                    LinearTermHandle rhs) {
  return toRef(handle).getVar(*toPtr(lhs), *toPtr(rhs));
}

LinearVariableOrderHandle LinearMinMaxHelper_get_order(
    LinearMinMaxHelperHandle handle) {
  return toRef(handle).getOrderHandle();
}

///// SMT Part

uint64_t SmtScalarType_get_id(SmtScalarTypeHandle handle) {
  return SmtTerm::ScalarType_get_id(*toPtr(handle));
}

SmtTermHandle SmtTerm_copy(SmtTermHandle handle) {
  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
  return toHandle(new SmtTerm::ConstTermPointer{*toPtr(handle)});
}

void SmtTerm_destroy(SmtTermHandle handle) {
  delete toPtr(handle);  // NOLINT(cppcoreguidelines-owning-memory)
}

int SmtTerm_equals(SmtTermHandle lhs, SmtTermHandle rhs) {
  return static_cast<int>(toRef(lhs) == toRef(rhs));
}

SmtTermHandle SmtTerm_add(SmtTermHandle lhs, SmtTermHandle rhs) {
  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
  return toHandle(new SmtTerm::ConstTermPointer{
      std::make_shared<SmtTerm>(toRef(lhs) + toRef(rhs))});
}

SmtTermHandle SmtTerm_sub(SmtTermHandle lhs, SmtTermHandle rhs) {
  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
  return toHandle(new SmtTerm::ConstTermPointer{
      std::make_shared<SmtTerm>(toRef(lhs) - toRef(rhs))});
}

SmtTermHandle SmtTerm_mul(SmtTermHandle lhs, SmtTermHandle rhs) {
  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
  return toHandle(new SmtTerm::ConstTermPointer{
      std::make_shared<SmtTerm>(toRef(lhs) * toRef(rhs))});
}

SmtTermHandle SmtTerm_div(SmtTermHandle lhs, SmtTermHandle rhs) {
  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
  return toHandle(new SmtTerm::ConstTermPointer{
      std::make_shared<SmtTerm>(toRef(lhs) / toRef(rhs))});
}

SmtTermHandle SmtTerm_scale(SmtTermHandle term_handle,
                            SmtScalarTypeHandle scalar_handle) {
  return toHandle(
      // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
      new SmtTerm::ConstTermPointer{std::make_shared<SmtTerm>(
          toRef(scalar_handle) * toRef(term_handle))});
}

SmtTermHandle SmtTerm_substitute(SmtTermHandle term_handle,
                                 SmtSubstitutionHandle substitution_handle) {
  const auto& substitution{*toPtr(substitution_handle)};

  return toHandle(
      // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
      new SmtTerm::ConstTermPointer{std::make_shared<SmtTerm>(
          (*toPtr(term_handle))
              ->substitute(substitution.getVariable(),
                           *substitution.getTerm()))});
}

uint64_t SmtTerm_hash(SmtTermHandle handle) {
  return toRef(handle).hash();
}

char* SmtTerm_to_str(SmtTermHandle handle, char* buf, size_t bufsize) {
  const auto str{toRef(handle).str()};
  if (bufsize > str.length() + 1) {
    strncpy(buf, str.c_str(), bufsize);
    return buf;
  }

  // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
  auto* newbuf{static_cast<char*>(malloc(str.length() + 2))};
  strncpy(newbuf, str.c_str(), str.length() + 1);
  return newbuf;
}

SmtResult SmtTerm_trivial_check(SmtTermHandle lhs, SmtTermHandle rhs,
                                ComparisonType comp_type) {
  const auto rval{
      SmtTerm::Comparison{toRef(lhs), toRef(rhs),
                          static_cast<expression::ComparisonType>(comp_type)}
          .trivialCheck()};

  return static_cast<SmtResult>(rval);
}

SmtResult SmtVariableOrder_decide(SmtVariableOrderHandle handle,
                                  MTBDD history) {
  if (history == sylvan::mtbdd_true) {
    return SMTRESULT_SAT;
  }
  sylvan::Mtbdd dd{history};
  const auto& order{*toPtr(handle)};
  auto conjunction{ddToConjunction(dd.GetMTBDD(), order)};
  assert(!conjunction.empty());

  if (const auto trivial_check{conjunction.back().comparison->trivialCheck()};
      trivial_check != expression::SmtResult::kUnknown) {
    if ((trivial_check == expression::SmtResult::kSat) !=
        conjunction.back().parity) {
      return SMTRESULT_UNSAT;
    }
  }

  return static_cast<SmtResult>(
      SmtTerm::Comparison::decide(conjunction, gPrintModelsFlag));
}

int SmtVariableOrder_compare(SmtVariableOrderHandle handle, SmtTermHandle lhs,
                             SmtTermHandle rhs, MTBDD history,
                             ComparisonType comp_type) {
  sylvan::Mtbdd dd{history};
  const auto& order{*toPtr(handle)};
  auto conjunction{ddToConjunction(dd.GetMTBDD(), order)};

  conjunction.emplace_back(
      false, std::make_shared<const SmtTerm::Comparison>(
                 toRef(lhs), toRef(rhs),
                 static_cast<expression::ComparisonType>(comp_type)));

  switch (SmtTerm::Comparison::decide(conjunction, gPrintModelsFlag)) {
    case expression::SmtResult::kUnsat:
      return 1;
    case expression::SmtResult::kSat:
      return 0;
    case expression::SmtResult::kUnknown:
      // This could become a problem
      // Maybe throw my own exception to catch?
      throw std::runtime_error{
          "Could not prove a comparison: Smt solver returned unknown!"};
  }

  std::unreachable();
}

uint64_t SmtVariableOrder_get_id(SmtVariableOrderHandle handle) {
  return toPtr(handle)->getId();
}

void SmtVariableOrder_shutdown() {
  SmtTerm::Comparison::shutdown();
}

uint64_t SmtSubstitution_get_id(SmtSubstitutionHandle handle) {
  return toPtr(handle)->getId();
}

uint64_t SmtMinMaxHelper_get_id(SmtMinMaxHelperHandle handle) {
  return toRef(handle).getId();
}

void SmtMinMaxHelper_add_pair(SmtMinMaxHelperHandle handle, SmtTermHandle lhs,
                              SmtTermHandle rhs) {
  toRef(handle).addPair(*toPtr(lhs), *toPtr(rhs));
}

void SmtMinMaxHelper_finalize(SmtMinMaxHelperHandle handle) {
  toRef(handle).finalize();
}

uint32_t SmtMinMaxHelper_get_var(SmtMinMaxHelperHandle handle,
                                 SmtTermHandle lhs, SmtTermHandle rhs) {
  return toRef(handle).getVar(*toPtr(lhs), *toPtr(rhs));
}

SmtVariableOrderHandle SmtMinMaxHelper_get_order(SmtMinMaxHelperHandle handle) {
  return toRef(handle).getOrderHandle();
}
}

// C++ part

void printModels(const bool flag) {
  gPrintModelsFlag = flag;
}

///////////////////////////////////////////////////////////////////////////////

template <>
sylvan::Mtbdd pruneFull<expression::LinearTerm>(
    const sylvan::Mtbdd& dd, const VariableOrder<LinearTerm>& order) {
#ifdef DO_PRUNE
  return sylvan::sylvan_LinearTerm_prune_full(dd.GetMTBDD(), toHandle(&order));
#else
  return dd;
#endif
}

template <>
sylvan::Mtbdd getPrunes<expression::LinearTerm>(
    const sylvan::Mtbdd& dd, const VariableOrder<LinearTerm>& order) {
#ifdef DO_PRUNE
  return sylvan::sylvan_LinearTerm_get_prunes(dd.GetMTBDD(), toHandle(&order));
#else
  return sylvan::Mtbdd::mtbddOne();
#endif
}

template <>
sylvan::Mtbdd prune<expression::LinearTerm>(const sylvan::Mtbdd& dd) {
  using sylvan::sylvan_LinearTerm_op_prune_RUN;
  return sylvan_LinearTerm_prune(dd.GetMTBDD());
}

template <>
sylvan::Mtbdd add<expression::LinearTerm>(const sylvan::Mtbdd& lhs,
                                          const sylvan::Mtbdd& rhs) {
  using sylvan::mtbdd_apply_RUN;
  using sylvan::sylvan_LinearTerm_op_plus_CALL;
  return sylvan_LinearTerm_plus(lhs.GetMTBDD(), rhs.GetMTBDD());
}

template <>
sylvan::Mtbdd sub<expression::LinearTerm>(const sylvan::Mtbdd& lhs,
                                          const sylvan::Mtbdd& rhs) {
  using sylvan::mtbdd_apply_RUN;
  using sylvan::sylvan_LinearTerm_op_minus_CALL;
  return sylvan_LinearTerm_minus(lhs.GetMTBDD(), rhs.GetMTBDD());
}

template <>
sylvan::Mtbdd scale<expression::LinearTerm>(
    const LinearTerm::ConstScalarPointer& scalar, const sylvan::Mtbdd& dd) {
  using sylvan::sylvan_LinearTerm_op_scale_RUN;
  return sylvan_LinearTerm_scale(dd.GetMTBDD(), toHandle(&scalar));
}

template <>
sylvan::Mtbdd getTerm<expression::LinearTerm>(const LinearTerm::Term& term) {
  const auto* tmp = new auto(std::make_shared<const LinearTerm::Term>(term));
  // sylvan_LinearTerm_term does take ownership!
  const auto rval = sylvan::sylvan_LinearTerm_term(toHandle(tmp));
  return rval;
}

template <>
expression::LinearTerm::ConstTermPointer getTerm<expression::LinearTerm>(
    const sylvan::Mtbdd& leaf) {
  if (leaf.isLeaf()) {
    return *toPtr(sylvan::sylvan_LinearTerm_get_term(leaf.GetMTBDD()));
  }
  return {};
}

template <>
sylvan::Mtbdd replaceLeaf<expression::LinearTerm>(
    const sylvan::Mtbdd& mtbdd, const sylvan::Mtbdd& old_leaf,
    const sylvan::Mtbdd& new_leaf) {
  using sylvan::sylvan_LinearTerm_op_replace_leaf_RUN;
  assert(old_leaf.isLeaf() && new_leaf.isLeaf());

  return sylvan_LinearTerm_replace_leaf(mtbdd.GetMTBDD(), old_leaf.GetMTBDD(),
                                        new_leaf.GetMTBDD());
}

template <>
sylvan::Mtbdd substituteLeafs<expression::LinearTerm>(
    const sylvan::Mtbdd& mtbdd,
    const ddsolve::Substitution<expression::LinearTerm>& substitution) {
  using sylvan::sylvan_LinearTerm_op_substitute_leafs_RUN;
  return sylvan_LinearTerm_substitute_leafs(mtbdd.GetMTBDD(),
                                            toHandle(&substitution));
}

template <>
bool compare<expression::LinearTerm>(const sylvan::Mtbdd& lhs,
                                     const sylvan::Mtbdd& rhs,
                                     const expression::ComparisonType& comp,
                                     const VariableOrder<LinearTerm>& order) {
  return sylvan::sylvan_LinearTerm_compare(lhs.GetMTBDD(), rhs.GetMTBDD(),
                                           static_cast<ComparisonType>(comp),
                                           toHandle(&order)) != 0;
}

template <>
sylvan::Mtbdd max<expression::LinearTerm>(
    const sylvan::Mtbdd& lhs, const sylvan::Mtbdd& rhs,
    VariableOrder<LinearTerm>::VariableOrderPointer order) {
  MinMaxHelper<LinearTerm> helper{std::move(order)};

  return sylvan::sylvan_LinearTerm_max(lhs.GetMTBDD(), rhs.GetMTBDD(),
                                       toHandle(&helper));
}

template <>
sylvan::Mtbdd min<expression::LinearTerm>(
    const sylvan::Mtbdd& lhs, const sylvan::Mtbdd& rhs,
    VariableOrder<LinearTerm>::VariableOrderPointer order) {
  MinMaxHelper<LinearTerm> helper{std::move(order)};

  return sylvan::sylvan_LinearTerm_min(lhs.GetMTBDD(), rhs.GetMTBDD(),
                                       toHandle(&helper));
}

//////////////////////////////////////////////////////////////////////////////

template <>
sylvan::Mtbdd pruneFull<expression::SmtTerm>(
    const sylvan::Mtbdd& dd, const VariableOrder<SmtTerm>& order) {
  return sylvan::sylvan_SmtTerm_prune_full(dd.GetMTBDD(), toHandle(&order));
}

template <>
sylvan::Mtbdd getPrunes<expression::SmtTerm>(
    const sylvan::Mtbdd& dd, const VariableOrder<SmtTerm>& order) {
  return sylvan::sylvan_SmtTerm_get_prunes(dd.GetMTBDD(), toHandle(&order));
}

template <>
sylvan::Mtbdd prune<expression::SmtTerm>(const sylvan::Mtbdd& dd) {
  using sylvan::sylvan_SmtTerm_op_prune_RUN;
  return sylvan_SmtTerm_prune(dd.GetMTBDD());
}

template <>
sylvan::Mtbdd add<expression::SmtTerm>(const sylvan::Mtbdd& lhs,
                                       const sylvan::Mtbdd& rhs) {
  using sylvan::mtbdd_apply_RUN;
  using sylvan::sylvan_SmtTerm_op_plus_CALL;
  return sylvan_SmtTerm_plus(lhs.GetMTBDD(), rhs.GetMTBDD());
}

template <>
sylvan::Mtbdd sub<expression::SmtTerm>(const sylvan::Mtbdd& lhs,
                                       const sylvan::Mtbdd& rhs) {
  using sylvan::mtbdd_apply_RUN;
  using sylvan::sylvan_SmtTerm_op_minus_CALL;
  return sylvan_SmtTerm_minus(lhs.GetMTBDD(), rhs.GetMTBDD());
}

template <>
sylvan::Mtbdd mul<expression::SmtTerm>(const sylvan::Mtbdd& lhs,
                                       const sylvan::Mtbdd& rhs) {
  using sylvan::mtbdd_apply_RUN;
  using sylvan::sylvan_SmtTerm_op_mul_CALL;
  return sylvan_SmtTerm_mul(lhs.GetMTBDD(), rhs.GetMTBDD());
}

template <>
sylvan::Mtbdd div<expression::SmtTerm>(const sylvan::Mtbdd& lhs,
                                       const sylvan::Mtbdd& rhs) {
  using sylvan::mtbdd_apply_RUN;
  using sylvan::sylvan_SmtTerm_op_div_CALL;
  return sylvan_SmtTerm_div(lhs.GetMTBDD(), rhs.GetMTBDD());
}


template <>
sylvan::Mtbdd scale<expression::SmtTerm>(
    const SmtTerm::ConstScalarPointer& scalar, const sylvan::Mtbdd& dd) {
  using sylvan::sylvan_SmtTerm_op_scale_RUN;
  return sylvan_SmtTerm_scale(dd.GetMTBDD(), toHandle(&scalar));
}

template <>
sylvan::Mtbdd getTerm<expression::SmtTerm>(const SmtTerm::Term& term) {
  const auto* tmp = new auto(std::make_shared<const SmtTerm::Term>(term));
  // sylvan_SmtTerm_term does take ownership!
  const auto rval = sylvan::sylvan_SmtTerm_term(toHandle(tmp));
  return rval;
}

template <>
expression::SmtTerm::ConstTermPointer getTerm<expression::SmtTerm>(
    const sylvan::Mtbdd& leaf) {
  if (leaf.isLeaf()) {
    return *toPtr(sylvan::sylvan_SmtTerm_get_term(leaf.GetMTBDD()));
  }
  return {};
}

template <>
sylvan::Mtbdd replaceLeaf<expression::SmtTerm>(const sylvan::Mtbdd& mtbdd,
                                               const sylvan::Mtbdd& old_leaf,
                                               const sylvan::Mtbdd& new_leaf) {
  using sylvan::sylvan_SmtTerm_op_replace_leaf_RUN;
  assert(old_leaf.isLeaf() && new_leaf.isLeaf());

  return sylvan_SmtTerm_replace_leaf(mtbdd.GetMTBDD(), old_leaf.GetMTBDD(),
                                     new_leaf.GetMTBDD());
}

template <>
sylvan::Mtbdd substituteLeafs<expression::SmtTerm>(
    const sylvan::Mtbdd& mtbdd,
    const ddsolve::Substitution<expression::SmtTerm>& substitution) {
  using sylvan::sylvan_SmtTerm_op_substitute_leafs_RUN;
  return sylvan_SmtTerm_substitute_leafs(mtbdd.GetMTBDD(),
                                         toHandle(&substitution));
}

template <>
bool compare<expression::SmtTerm>(const sylvan::Mtbdd& lhs,
                                  const sylvan::Mtbdd& rhs,
                                  const expression::ComparisonType& comp,
                                  const VariableOrder<SmtTerm>& order) {
  return sylvan::sylvan_SmtTerm_compare(lhs.GetMTBDD(), rhs.GetMTBDD(),
                                        static_cast<ComparisonType>(comp),
                                        toHandle(&order)) != 0;
}

template <>
sylvan::Mtbdd max<expression::SmtTerm>(
    const sylvan::Mtbdd& lhs, const sylvan::Mtbdd& rhs,
    VariableOrder<SmtTerm>::VariableOrderPointer order) {
  MinMaxHelper<SmtTerm> helper{std::move(order)};

  return sylvan::sylvan_SmtTerm_max(lhs.GetMTBDD(), rhs.GetMTBDD(),
                                    toHandle(&helper));
}

template <>
sylvan::Mtbdd min<expression::SmtTerm>(
    const sylvan::Mtbdd& lhs, const sylvan::Mtbdd& rhs,
    VariableOrder<SmtTerm>::VariableOrderPointer order) {
  MinMaxHelper<SmtTerm> helper{std::move(order)};

  return sylvan::sylvan_SmtTerm_min(lhs.GetMTBDD(), rhs.GetMTBDD(),
                                    toHandle(&helper));
}

}  // namespace ddsolve::interface
