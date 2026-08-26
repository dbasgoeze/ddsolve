/**
 * @file SylvanInterface.cpp
 * @brief Implementation of the Sylvan backend interface for decision diagram
 * operations.
 *
 * This file implements the free-standing interface functions (add, sub, mul,
 * div, scale, prune, compare, max, min, getTerm, etc.) that delegate to the
 * Sylvan library.  It provides template specialisations for LinearTerm and
 * SmtTerm, along with a MinMaxHelper that materialises term-pair comparisons
 * as decision-diagram variables.  A C-compatible API is exposed via extern "C"
 * for use by the Sylvan C code.
 *
 * @note The Doxygen-style comments in this file were generated with the help
 *       of AI (Qwen3.6-35B-A3B). They have been human-checked on a best-effort
 *       basis. No lines of logic in this file were written or modified by AI.
 */

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

/**
 * @brief Comparator for term pairs used in @p MinMaxHelper.
 *
 * Orders pairs lexicographically: first by the left term, then by the
 * right term.  Used as the key comparator in the term-pair set.
 *
 * @tparam Term  The term type (LinearTerm or SmtTerm).
 */
template <typename Term>
struct TermPairCompare {
  using TermPair = std::pair<typename Term::ConstTermPointer,
                             typename Term::ConstTermPointer>;

  bool operator()(const TermPair& lhs, const TermPair& rhs) const {
    return *lhs.first < *rhs.first ||
           (*lhs.first == *rhs.first && *lhs.second < *rhs.second);
  }
};

/**
 * @brief Helper for materialising min/max term-pair comparisons as DD
 * variables.
 *
 * Collects term pairs requested via @p addPair, then assigns each pair a
 * decision-diagram variable by building a comparison node through the
 * variable order.  Thread-safe via a shared mutex.
 *
 * @tparam Term  The term type (LinearTerm or SmtTerm).
 */
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

/**
 * @brief Recursively convert an MTBDD to a conjunction of comparisons.
 *
 * Traverses the MTBDD structure: when the high child is false the node
 * represents the comparison itself (with odd parity indicating negation);
 * when the low child is false the node represents the negated comparison.
 * The recursion accumulates results into @p conjunction.
 *
 * @tparam Term       The term type.
 * @param dd          The MTBDD to convert.
 * @param order       The variable order providing comparison objects.
 * @param conjunction  Output vector to accumulate comparison-with-parity pairs.
 */
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

/**
 * @brief Convert an MTBDD to a conjunction of comparisons.
 *
 * Convenience wrapper that allocates and returns the conjunction vector.
 *
 * @tparam Term  The term type.
 * @param dd     The MTBDD to convert.
 * @param order  The variable order.
 * @return A vector of comparison-with-parity pairs.
 */
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

/**
 * @brief Retrieve the unique ID of a LinearTerm scalar type.
 */
uint64_t LinearScalarType_get_id(LinearScalarTypeHandle handle) {
  return LinearTerm::ScalarType_get_id(*toPtr(handle));
}

/**
 * @brief Copy a LinearTerm handle.
 *
 * @param handle  The term handle to copy.
 * @return A new handle owning a copy of the term.
 */
LinearTermHandle LinearTerm_copy(LinearTermHandle handle) {
  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
  return toHandle(new LinearTerm::ConstTermPointer{*toPtr(handle)});
}

/**
 * @brief Destroy a LinearTerm handle.
 *
 * @param handle  The term handle to destroy.
 */
void LinearTerm_destroy(LinearTermHandle handle) {
  delete toPtr(handle);  // NOLINT(cppcoreguidelines-owning-memory)
}

/**
 * @brief Check equality of two LinearTerm handles.
 *
 * @param lhs  The left-hand term.
 * @param rhs  The right-hand term.
 * @return 1 if equal, 0 otherwise.
 */
int LinearTerm_equals(LinearTermHandle lhs, LinearTermHandle rhs) {
  return static_cast<int>(toRef(lhs) == toRef(rhs));
}

/**
 * @brief Add two LinearTerm handles.
 *
 * @param lhs  The left-hand term.
 * @param rhs  The right-hand term.
 * @return A new handle owning the sum.
 */
LinearTermHandle LinearTerm_add(LinearTermHandle lhs, LinearTermHandle rhs) {
  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
  return toHandle(new LinearTerm::ConstTermPointer{
      std::make_shared<LinearTerm>(toRef(lhs) + toRef(rhs))});
}

/**
 * @brief Subtract two LinearTerm handles.
 *
 * @param lhs  The left-hand term.
 * @param rhs  The right-hand term.
 * @return A new handle owning the difference.
 */
LinearTermHandle LinearTerm_sub(LinearTermHandle lhs, LinearTermHandle rhs) {
  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
  return toHandle(new LinearTerm::ConstTermPointer{
      std::make_shared<LinearTerm>(toRef(lhs) - toRef(rhs))});
}

/**
 * @brief Scale a LinearTerm by a scalar.
 *
 * @param term_handle     The term to scale.
 * @param scalar_handle   The scalar multiplier.
 * @return A new handle owning the scaled term.
 */
LinearTermHandle LinearTerm_scale(LinearTermHandle term_handle,
                                  LinearScalarTypeHandle scalar_handle) {
  return toHandle(
      // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
      new LinearTerm::ConstTermPointer{std::make_shared<LinearTerm>(
          toRef(scalar_handle) * toRef(term_handle))});
}

/**
 * @brief Substitute variables in a LinearTerm.
 *
 * @param term_handle          The term to substitute in.
 * @param substitution_handle  The substitution mapping variables to terms.
 * @return A new handle owning the substituted term.
 */
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

/**
 * @brief Compute the hash of a LinearTerm.
 *
 * @param handle  The term handle.
 * @return The term's hash value.
 */
uint64_t LinearTerm_hash(LinearTermHandle handle) {
  return toRef(handle).hash();
}

/**
 * @brief Convert a LinearTerm to a human-readable string.
 *
 * If @p buf is large enough the string is written there; otherwise a new
 * buffer is allocated (caller must free).
 *
 * @param handle     The term handle.
 * @param buf        Pre-allocated buffer (may be null).
 * @param bufsize    Size of @p buf.
 * @return A pointer to the string (either @p buf or a newly allocated buffer).
 */
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

/**
 * @brief Perform a trivial (fast-path) check on a LinearTerm comparison.
 *
 * @param lhs         The left-hand term.
 * @param rhs         The right-hand term.
 * @param comp_type   The comparison type.
 * @return The result of the trivial check.
 */
SmtResult LinearTerm_trivial_check(LinearTermHandle lhs, LinearTermHandle rhs,
                                   ComparisonType comp_type) {
  const auto rval{
      LinearTerm::Comparison{toRef(lhs), toRef(rhs),
                             static_cast<expression::ComparisonType>(comp_type)}
          .trivialCheck()};

  return static_cast<SmtResult>(rval);
}

/**
 * @brief Decide satisfiability of a LinearTerm comparison conjunction.
 *
 * Converts the history MTBDD to a conjunction of comparisons and attempts
 * to decide it using the appropriate solver (real or unsigned integer).
 *
 * @param handle  The variable order handle.
 * @param history The history MTBDD representing the conjunction.
 * @return SMTRESULT_SAT or SMTRESULT_UNSAT.
 */
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

/**
 * @brief Compare two LinearTerm handles under a history constraint.
 *
 * Converts the history MTBDD to a conjunction, adds the comparison between
 * @p lhs and @p rhs, and decides the result.
 *
 * @param handle      The variable order handle.
 * @param lhs         The left-hand term.
 * @param rhs         The right-hand term.
 * @param history     The history MTBDD.
 * @param comp_type   The comparison type.
 * @return 1 if lhs < rhs is unsat, 0 if sat, throws if unknown.
 */
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

/**
 * @brief Retrieve the unique ID of a LinearTerm variable order.
 */
uint64_t LinearVariableOrder_get_id(LinearVariableOrderHandle handle) {
  return toPtr(handle)->getId();
}

/**
 * @brief Shut down LinearTerm comparison infrastructure.
 */
void LinearVariableOrder_shutdown() {
  LinearTerm::Comparison::shutdown();
}

/**
 * @brief Retrieve the unique ID of a LinearTerm substitution.
 */
uint64_t LinearSubstitution_get_id(LinearSubstitutionHandle handle) {
  return toPtr(handle)->getId();
}

/**
 * @brief Retrieve the unique ID of a LinearTerm MinMaxHelper.
 */
uint64_t LinearMinMaxHelper_get_id(LinearMinMaxHelperHandle handle) {
  return toRef(handle).getId();
}

/**
 * @brief Add a term pair to the MinMaxHelper.
 *
 * @param handle  The helper handle.
 * @param lhs     The left-hand term.
 * @param rhs     The right-hand term.
 */
void LinearMinMaxHelper_add_pair(LinearMinMaxHelperHandle handle,
                                 LinearTermHandle lhs, LinearTermHandle rhs) {
  toRef(handle).addPair(*toPtr(lhs), *toPtr(rhs));
}

/**
 * @brief Finalise the MinMaxHelper (assign DD variables to all pairs).
 */
void LinearMinMaxHelper_finalize(LinearMinMaxHelperHandle handle) {
  toRef(handle).finalize();
}

/**
 * @brief Get the DD variable for a term pair in the MinMaxHelper.
 *
 * @param handle  The helper handle.
 * @param lhs     The left-hand term.
 * @param rhs     The right-hand term.
 * @return The DD variable ID.
 */
uint32_t LinearMinMaxHelper_get_var(LinearMinMaxHelperHandle handle,
                                    LinearTermHandle lhs,
                                    LinearTermHandle rhs) {
  return toRef(handle).getVar(*toPtr(lhs), *toPtr(rhs));
}

/**
 * @brief Get the variable order handle from the MinMaxHelper.
 */
LinearVariableOrderHandle LinearMinMaxHelper_get_order(
    LinearMinMaxHelperHandle handle) {
  return toRef(handle).getOrderHandle();
}

///// SMT Part

/**
 * @brief Retrieve the unique ID of an SmtTerm scalar type.
 */
uint64_t SmtScalarType_get_id(SmtScalarTypeHandle handle) {
  return SmtTerm::ScalarType_get_id(*toPtr(handle));
}

/**
 * @brief Copy an SmtTerm handle.
 *
 * @param handle  The term handle to copy.
 * @return A new handle owning a copy of the term.
 */
SmtTermHandle SmtTerm_copy(SmtTermHandle handle) {
  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
  return toHandle(new SmtTerm::ConstTermPointer{*toPtr(handle)});
}

/**
 * @brief Destroy an SmtTerm handle.
 *
 * @param handle  The term handle to destroy.
 */
void SmtTerm_destroy(SmtTermHandle handle) {
  delete toPtr(handle);  // NOLINT(cppcoreguidelines-owning-memory)
}

/**
 * @brief Check equality of two SmtTerm handles.
 *
 * @param lhs  The left-hand term.
 * @param rhs  The right-hand term.
 * @return 1 if equal, 0 otherwise.
 */
int SmtTerm_equals(SmtTermHandle lhs, SmtTermHandle rhs) {
  return static_cast<int>(toRef(lhs) == toRef(rhs));
}

/**
 * @brief Add two SmtTerm handles.
 *
 * @param lhs  The left-hand term.
 * @param rhs  The right-hand term.
 * @return A new handle owning the sum.
 */
SmtTermHandle SmtTerm_add(SmtTermHandle lhs, SmtTermHandle rhs) {
  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
  return toHandle(new SmtTerm::ConstTermPointer{
      std::make_shared<SmtTerm>(toRef(lhs) + toRef(rhs))});
}

/**
 * @brief Subtract two SmtTerm handles.
 *
 * @param lhs  The left-hand term.
 * @param rhs  The right-hand term.
 * @return A new handle owning the difference.
 */
SmtTermHandle SmtTerm_sub(SmtTermHandle lhs, SmtTermHandle rhs) {
  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
  return toHandle(new SmtTerm::ConstTermPointer{
      std::make_shared<SmtTerm>(toRef(lhs) - toRef(rhs))});
}

/**
 * @brief Multiply two SmtTerm handles.
 *
 * @param lhs  The left-hand term.
 * @param rhs  The right-hand term.
 * @return A new handle owning the product.
 */
SmtTermHandle SmtTerm_mul(SmtTermHandle lhs, SmtTermHandle rhs) {
  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
  return toHandle(new SmtTerm::ConstTermPointer{
      std::make_shared<SmtTerm>(toRef(lhs) * toRef(rhs))});
}

/**
 * @brief Divide two SmtTerm handles.
 *
 * @param lhs  The left-hand term.
 * @param rhs  The right-hand term.
 * @return A new handle owning the quotient.
 */
SmtTermHandle SmtTerm_div(SmtTermHandle lhs, SmtTermHandle rhs) {
  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
  return toHandle(new SmtTerm::ConstTermPointer{
      std::make_shared<SmtTerm>(toRef(lhs) / toRef(rhs))});
}

/**
 * @brief Scale an SmtTerm by a scalar.
 *
 * @param term_handle     The term to scale.
 * @param scalar_handle   The scalar multiplier.
 * @return A new handle owning the scaled term.
 */
SmtTermHandle SmtTerm_scale(SmtTermHandle term_handle,
                            SmtScalarTypeHandle scalar_handle) {
  return toHandle(
      // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
      new SmtTerm::ConstTermPointer{std::make_shared<SmtTerm>(
          toRef(scalar_handle) * toRef(term_handle))});
}

/**
 * @brief Substitute variables in an SmtTerm.
 *
 * @param term_handle          The term to substitute in.
 * @param substitution_handle  The substitution mapping variables to terms.
 * @return A new handle owning the substituted term.
 */
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

/**
 * @brief Compute the hash of an SmtTerm.
 *
 * @param handle  The term handle.
 * @return The term's hash value.
 */
uint64_t SmtTerm_hash(SmtTermHandle handle) {
  return toRef(handle).hash();
}

/**
 * @brief Convert an SmtTerm to a human-readable string.
 *
 * If @p buf is large enough the string is written there; otherwise a new
 * buffer is allocated (caller must free).
 *
 * @param handle     The term handle.
 * @param buf        Pre-allocated buffer (may be null).
 * @param bufsize    Size of @p buf.
 * @return A pointer to the string (either @p buf or a newly allocated buffer).
 */
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

/**
 * @brief Perform a trivial (fast-path) check on an SmtTerm comparison.
 *
 * @param lhs         The left-hand term.
 * @param rhs         The right-hand term.
 * @param comp_type   The comparison type.
 * @return The result of the trivial check.
 */
SmtResult SmtTerm_trivial_check(SmtTermHandle lhs, SmtTermHandle rhs,
                                ComparisonType comp_type) {
  const auto rval{
      SmtTerm::Comparison{toRef(lhs), toRef(rhs),
                          static_cast<expression::ComparisonType>(comp_type)}
          .trivialCheck()};

  return static_cast<SmtResult>(rval);
}

/**
 * @brief Decide satisfiability of an SmtTerm comparison conjunction.
 *
 * Converts the history MTBDD to a conjunction of comparisons and attempts
 * to decide it using the SMT solver.
 *
 * @param handle  The variable order handle.
 * @param history The history MTBDD representing the conjunction.
 * @return SMTRESULT_SAT or SMTRESULT_UNSAT.
 */
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

/**
 * @brief Compare two SmtTerm handles under a history constraint.
 *
 * Converts the history MTBDD to a conjunction, adds the comparison between
 * @p lhs and @p rhs, and decides the result.
 *
 * @param handle      The variable order handle.
 * @param lhs         The left-hand term.
 * @param rhs         The right-hand term.
 * @param history     The history MTBDD.
 * @param comp_type   The comparison type.
 * @return 1 if lhs < rhs is unsat, 0 if sat, throws if unknown.
 */
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

/**
 * @brief Retrieve the unique ID of an SmtTerm variable order.
 */
uint64_t SmtVariableOrder_get_id(SmtVariableOrderHandle handle) {
  return toPtr(handle)->getId();
}

/**
 * @brief Shut down SmtTerm comparison infrastructure.
 */
void SmtVariableOrder_shutdown() {
  SmtTerm::Comparison::shutdown();
}

/**
 * @brief Retrieve the unique ID of an SmtTerm substitution.
 */
uint64_t SmtSubstitution_get_id(SmtSubstitutionHandle handle) {
  return toPtr(handle)->getId();
}

/**
 * @brief Retrieve the unique ID of an SmtTerm MinMaxHelper.
 */
uint64_t SmtMinMaxHelper_get_id(SmtMinMaxHelperHandle handle) {
  return toRef(handle).getId();
}

/**
 * @brief Add a term pair to the SmtTerm MinMaxHelper.
 *
 * @param handle  The helper handle.
 * @param lhs     The left-hand term.
 * @param rhs     The right-hand term.
 */
void SmtMinMaxHelper_add_pair(SmtMinMaxHelperHandle handle, SmtTermHandle lhs,
                              SmtTermHandle rhs) {
  toRef(handle).addPair(*toPtr(lhs), *toPtr(rhs));
}

/**
 * @brief Finalise the SmtTerm MinMaxHelper (assign DD variables to all pairs).
 */
void SmtMinMaxHelper_finalize(SmtMinMaxHelperHandle handle) {
  toRef(handle).finalize();
}

/**
 * @brief Get the DD variable for a term pair in the SmtTerm MinMaxHelper.
 *
 * @param handle  The helper handle.
 * @param lhs     The left-hand term.
 * @param rhs     The right-hand term.
 * @return The DD variable ID.
 */
uint32_t SmtMinMaxHelper_get_var(SmtMinMaxHelperHandle handle,
                                 SmtTermHandle lhs, SmtTermHandle rhs) {
  return toRef(handle).getVar(*toPtr(lhs), *toPtr(rhs));
}

/**
 * @brief Get the variable order handle from the SmtTerm MinMaxHelper.
 */
SmtVariableOrderHandle SmtMinMaxHelper_get_order(SmtMinMaxHelperHandle handle) {
  return toRef(handle).getOrderHandle();
}
}

// C++ part

/**
 * @brief Enable or disable model printing during SMT solving.
 *
 * @param flag  true to print models, false to suppress.
 */
void printModels(const bool flag) {
  gPrintModelsFlag = flag;
}

//////////////////////////////////////////////////////////////////////////////

/**
 * @brief Fully prune a LinearTerm decision diagram.
 *
 * Delegates to Sylvan's LinearTerm-specific prune_full operation when
 * @c DO_PRUNE is enabled; otherwise returns the diagram unchanged.
 *
 * @param dd     The MTBDD to prune.
 * @param order  The variable order.
 * @return The pruned MTBDD.
 */
template <>
sylvan::Mtbdd pruneFull<expression::LinearTerm>(
    const sylvan::Mtbdd& dd, const VariableOrder<LinearTerm>& order) {
#ifdef DO_PRUNE
  return sylvan::sylvan_LinearTerm_prune_full(dd.GetMTBDD(), toHandle(&order));
#else
  return dd;
#endif
}

/**
 * @brief Get the prune mask for a LinearTerm decision diagram.
 *
 * Delegates to Sylvan's LinearTerm-specific get_prunes operation when
 * @c DO_PRUNE is enabled; otherwise returns the all-ones MTBDD.
 *
 * @param dd     The MTBDD.
 * @param order  The variable order.
 * @return The prune mask MTBDD.
 */
template <>
sylvan::Mtbdd getPrunes<expression::LinearTerm>(
    const sylvan::Mtbdd& dd, const VariableOrder<LinearTerm>& order) {
#ifdef DO_PRUNE
  return sylvan::sylvan_LinearTerm_get_prunes(dd.GetMTBDD(), toHandle(&order));
#else
  return sylvan::Mtbdd::mtbddOne();
#endif
}

/**
 * @brief Prune zero leaves in a LinearTerm decision diagram.
 *
 * @param dd  The MTBDD to prune.
 * @return The pruned MTBDD.
 */
template <>
sylvan::Mtbdd prune<expression::LinearTerm>(const sylvan::Mtbdd& dd) {
  using sylvan::sylvan_LinearTerm_op_prune_RUN;
  return sylvan_LinearTerm_prune(dd.GetMTBDD());
}

/**
 * @brief Add two LinearTerm decision diagrams.
 *
 * @param lhs  The left-hand MTBDD.
 * @param rhs  The right-hand MTBDD.
 * @return The sum MTBDD.
 */
template <>
sylvan::Mtbdd add<expression::LinearTerm>(const sylvan::Mtbdd& lhs,
                                          const sylvan::Mtbdd& rhs) {
  using sylvan::mtbdd_apply_RUN;
  using sylvan::sylvan_LinearTerm_op_plus_CALL;
  return sylvan_LinearTerm_plus(lhs.GetMTBDD(), rhs.GetMTBDD());
}

/**
 * @brief Subtract two LinearTerm decision diagrams.
 *
 * @param lhs  The left-hand MTBDD.
 * @param rhs  The right-hand MTBDD.
 * @return The difference MTBDD.
 */
template <>
sylvan::Mtbdd sub<expression::LinearTerm>(const sylvan::Mtbdd& lhs,
                                          const sylvan::Mtbdd& rhs) {
  using sylvan::mtbdd_apply_RUN;
  using sylvan::sylvan_LinearTerm_op_minus_CALL;
  return sylvan_LinearTerm_minus(lhs.GetMTBDD(), rhs.GetMTBDD());
}

/**
 * @brief Scale a LinearTerm decision diagram by a scalar.
 *
 * @param scalar  The scalar multiplier.
 * @param dd      The MTBDD to scale.
 * @return The scaled MTBDD.
 */
template <>
sylvan::Mtbdd scale<expression::LinearTerm>(
    const LinearTerm::ConstScalarPointer& scalar, const sylvan::Mtbdd& dd) {
  using sylvan::sylvan_LinearTerm_op_scale_RUN;
  return sylvan_LinearTerm_scale(dd.GetMTBDD(), toHandle(&scalar));
}

/**
 * @brief Convert a LinearTerm to a decision diagram leaf.
 *
 * @param term  The term to convert.
 * @return The MTBDD leaf representing the term.
 */
template <>
sylvan::Mtbdd getTerm<expression::LinearTerm>(const LinearTerm::Term& term) {
  const auto* tmp = new auto(std::make_shared<const LinearTerm::Term>(term));
  // sylvan_LinearTerm_term does take ownership!
  const auto rval = sylvan::sylvan_LinearTerm_term(toHandle(tmp));
  return rval;
}

/**
 * @brief Extract a LinearTerm from an MTBDD leaf.
 *
 * @param leaf  The MTBDD leaf.
 * @return The term if the node is a leaf, or an empty pointer otherwise.
 */
template <>
expression::LinearTerm::ConstTermPointer getTerm<expression::LinearTerm>(
    const sylvan::Mtbdd& leaf) {
  if (leaf.isLeaf()) {
    return *toPtr(sylvan::sylvan_LinearTerm_get_term(leaf.GetMTBDD()));
  }
  return {};
}

/**
 * @brief Replace a leaf in a LinearTerm decision diagram.
 *
 * @param mtbdd     The MTBDD.
 * @param old_leaf  The leaf to replace.
 * @param new_leaf  The replacement leaf.
 * @return The MTBDD with the leaf replaced.
 */
template <>
sylvan::Mtbdd replaceLeaf<expression::LinearTerm>(
    const sylvan::Mtbdd& mtbdd, const sylvan::Mtbdd& old_leaf,
    const sylvan::Mtbdd& new_leaf) {
  using sylvan::sylvan_LinearTerm_op_replace_leaf_RUN;
  assert(old_leaf.isLeaf() && new_leaf.isLeaf());

  return sylvan_LinearTerm_replace_leaf(mtbdd.GetMTBDD(), old_leaf.GetMTBDD(),
                                        new_leaf.GetMTBDD());
}

/**
 * @brief Substitute leafs in a LinearTerm decision diagram.
 *
 * @param mtbdd          The MTBDD.
 * @param substitution   The substitution mapping variables to terms.
 * @return The MTBDD with leafs substituted.
 */
template <>
sylvan::Mtbdd substituteLeafs<expression::LinearTerm>(
    const sylvan::Mtbdd& mtbdd,
    const ddsolve::Substitution<expression::LinearTerm>& substitution) {
  using sylvan::sylvan_LinearTerm_op_substitute_leafs_RUN;
  return sylvan_LinearTerm_substitute_leafs(mtbdd.GetMTBDD(),
                                            toHandle(&substitution));
}

/**
 * @brief Compare two LinearTerm decision diagrams.
 *
 * @param lhs     The left-hand MTBDD.
 * @param rhs     The right-hand MTBDD.
 * @param comp    The comparison type.
 * @param order   The variable order.
 * @return true if the comparison holds.
 */
template <>
bool compare<expression::LinearTerm>(const sylvan::Mtbdd& lhs,
                                     const sylvan::Mtbdd& rhs,
                                     const expression::ComparisonType& comp,
                                     const VariableOrder<LinearTerm>& order) {
  return sylvan::sylvan_LinearTerm_compare(lhs.GetMTBDD(), rhs.GetMTBDD(),
                                           static_cast<ComparisonType>(comp),
                                           toHandle(&order)) != 0;
}

/**
 * @brief Compute the element-wise maximum of two LinearTerm decision diagrams.
 *
 * @param lhs     The left-hand MTBDD.
 * @param rhs     The right-hand MTBDD.
 * @param order   The variable order.
 * @return The maximum MTBDD.
 */
template <>
sylvan::Mtbdd max<expression::LinearTerm>(
    const sylvan::Mtbdd& lhs, const sylvan::Mtbdd& rhs,
    VariableOrder<LinearTerm>::VariableOrderPointer order) {
  MinMaxHelper<LinearTerm> helper{std::move(order)};

  return sylvan::sylvan_LinearTerm_max(lhs.GetMTBDD(), rhs.GetMTBDD(),
                                       toHandle(&helper));
}

/**
 * @brief Compute the element-wise minimum of two LinearTerm decision diagrams.
 *
 * @param lhs     The left-hand MTBDD.
 * @param rhs     The right-hand MTBDD.
 * @param order   The variable order.
 * @return The minimum MTBDD.
 */
template <>
sylvan::Mtbdd min<expression::LinearTerm>(
    const sylvan::Mtbdd& lhs, const sylvan::Mtbdd& rhs,
    VariableOrder<LinearTerm>::VariableOrderPointer order) {
  MinMaxHelper<LinearTerm> helper{std::move(order)};

  return sylvan::sylvan_LinearTerm_min(lhs.GetMTBDD(), rhs.GetMTBDD(),
                                       toHandle(&helper));
}

//////////////////////////////////////////////////////////////////////////////

/**
 * @brief Fully prune an SmtTerm decision diagram.
 *
 * @param dd     The MTBDD to prune.
 * @param order  The variable order.
 * @return The pruned MTBDD.
 */
template <>
sylvan::Mtbdd pruneFull<expression::SmtTerm>(
    const sylvan::Mtbdd& dd, const VariableOrder<SmtTerm>& order) {
  return sylvan::sylvan_SmtTerm_prune_full(dd.GetMTBDD(), toHandle(&order));
}

/**
 * @brief Get the prune mask for an SmtTerm decision diagram.
 *
 * @param dd     The MTBDD.
 * @param order  The variable order.
 * @return The prune mask MTBDD.
 */
template <>
sylvan::Mtbdd getPrunes<expression::SmtTerm>(
    const sylvan::Mtbdd& dd, const VariableOrder<SmtTerm>& order) {
  return sylvan::sylvan_SmtTerm_get_prunes(dd.GetMTBDD(), toHandle(&order));
}

/**
 * @brief Prune zero leaves in an SmtTerm decision diagram.
 *
 * @param dd  The MTBDD to prune.
 * @return The pruned MTBDD.
 */
template <>
sylvan::Mtbdd prune<expression::SmtTerm>(const sylvan::Mtbdd& dd) {
  using sylvan::sylvan_SmtTerm_op_prune_RUN;
  return sylvan_SmtTerm_prune(dd.GetMTBDD());
}

/**
 * @brief Add two SmtTerm decision diagrams.
 *
 * @param lhs  The left-hand MTBDD.
 * @param rhs  The right-hand MTBDD.
 * @return The sum MTBDD.
 */
template <>
sylvan::Mtbdd add<expression::SmtTerm>(const sylvan::Mtbdd& lhs,
                                       const sylvan::Mtbdd& rhs) {
  using sylvan::mtbdd_apply_RUN;
  using sylvan::sylvan_SmtTerm_op_plus_CALL;
  return sylvan_SmtTerm_plus(lhs.GetMTBDD(), rhs.GetMTBDD());
}

/**
 * @brief Subtract two SmtTerm decision diagrams.
 *
 * @param lhs  The left-hand MTBDD.
 * @param rhs  The right-hand MTBDD.
 * @return The difference MTBDD.
 */
template <>
sylvan::Mtbdd sub<expression::SmtTerm>(const sylvan::Mtbdd& lhs,
                                       const sylvan::Mtbdd& rhs) {
  using sylvan::mtbdd_apply_RUN;
  using sylvan::sylvan_SmtTerm_op_minus_CALL;
  return sylvan_SmtTerm_minus(lhs.GetMTBDD(), rhs.GetMTBDD());
}

/**
 * @brief Multiply two SmtTerm decision diagrams.
 *
 * @param lhs  The left-hand MTBDD.
 * @param rhs  The right-hand MTBDD.
 * @return The product MTBDD.
 */
template <>
sylvan::Mtbdd mul<expression::SmtTerm>(const sylvan::Mtbdd& lhs,
                                       const sylvan::Mtbdd& rhs) {
  using sylvan::mtbdd_apply_RUN;
  using sylvan::sylvan_SmtTerm_op_mul_CALL;
  return sylvan_SmtTerm_mul(lhs.GetMTBDD(), rhs.GetMTBDD());
}

/**
 * @brief Divide two SmtTerm decision diagrams.
 *
 * @param lhs  The left-hand MTBDD.
 * @param rhs  The right-hand MTBDD.
 * @return The quotient MTBDD.
 */
template <>
sylvan::Mtbdd div<expression::SmtTerm>(const sylvan::Mtbdd& lhs,
                                       const sylvan::Mtbdd& rhs) {
  using sylvan::mtbdd_apply_RUN;
  using sylvan::sylvan_SmtTerm_op_div_CALL;
  return sylvan_SmtTerm_div(lhs.GetMTBDD(), rhs.GetMTBDD());
}

/**
 * @brief Scale an SmtTerm decision diagram by a scalar.
 *
 * @param scalar  The scalar multiplier.
 * @param dd      The MTBDD to scale.
 * @return The scaled MTBDD.
 */
template <>
sylvan::Mtbdd scale<expression::SmtTerm>(
    const SmtTerm::ConstScalarPointer& scalar, const sylvan::Mtbdd& dd) {
  using sylvan::sylvan_SmtTerm_op_scale_RUN;
  return sylvan_SmtTerm_scale(dd.GetMTBDD(), toHandle(&scalar));
}

/**
 * @brief Convert an SmtTerm to a decision diagram leaf.
 *
 * @param term  The term to convert.
 * @return The MTBDD leaf representing the term.
 */
template <>
sylvan::Mtbdd getTerm<expression::SmtTerm>(const SmtTerm::Term& term) {
  const auto* tmp = new auto(std::make_shared<const SmtTerm::Term>(term));
  // sylvan_SmtTerm_term does take ownership!
  const auto rval = sylvan::sylvan_SmtTerm_term(toHandle(tmp));
  return rval;
}

/**
 * @brief Extract an SmtTerm from an MTBDD leaf.
 *
 * @param leaf  The MTBDD leaf.
 * @return The term if the node is a leaf, or an empty pointer otherwise.
 */
template <>
expression::SmtTerm::ConstTermPointer getTerm<expression::SmtTerm>(
    const sylvan::Mtbdd& leaf) {
  if (leaf.isLeaf()) {
    return *toPtr(sylvan::sylvan_SmtTerm_get_term(leaf.GetMTBDD()));
  }
  return {};
}

/**
 * @brief Replace a leaf in an SmtTerm decision diagram.
 *
 * @param mtbdd     The MTBDD.
 * @param old_leaf  The leaf to replace.
 * @param new_leaf  The replacement leaf.
 * @return The MTBDD with the leaf replaced.
 */
template <>
sylvan::Mtbdd replaceLeaf<expression::SmtTerm>(const sylvan::Mtbdd& mtbdd,
                                               const sylvan::Mtbdd& old_leaf,
                                               const sylvan::Mtbdd& new_leaf) {
  using sylvan::sylvan_SmtTerm_op_replace_leaf_RUN;
  assert(old_leaf.isLeaf() && new_leaf.isLeaf());

  return sylvan_SmtTerm_replace_leaf(mtbdd.GetMTBDD(), old_leaf.GetMTBDD(),
                                     new_leaf.GetMTBDD());
}

/**
 * @brief Substitute leafs in an SmtTerm decision diagram.
 *
 * @param mtbdd          The MTBDD.
 * @param substitution   The substitution mapping variables to terms.
 * @return The MTBDD with leafs substituted.
 */
template <>
sylvan::Mtbdd substituteLeafs<expression::SmtTerm>(
    const sylvan::Mtbdd& mtbdd,
    const ddsolve::Substitution<expression::SmtTerm>& substitution) {
  using sylvan::sylvan_SmtTerm_op_substitute_leafs_RUN;
  return sylvan_SmtTerm_substitute_leafs(mtbdd.GetMTBDD(),
                                         toHandle(&substitution));
}

/**
 * @brief Compare two SmtTerm decision diagrams.
 *
 * @param lhs     The left-hand MTBDD.
 * @param rhs     The right-hand MTBDD.
 * @param comp    The comparison type.
 * @param order   The variable order.
 * @return true if the comparison holds.
 */
template <>
bool compare<expression::SmtTerm>(const sylvan::Mtbdd& lhs,
                                  const sylvan::Mtbdd& rhs,
                                  const expression::ComparisonType& comp,
                                  const VariableOrder<SmtTerm>& order) {
  return sylvan::sylvan_SmtTerm_compare(lhs.GetMTBDD(), rhs.GetMTBDD(),
                                        static_cast<ComparisonType>(comp),
                                        toHandle(&order)) != 0;
}

/**
 * @brief Compute the element-wise maximum of two SmtTerm decision diagrams.
 *
 * @param lhs     The left-hand MTBDD.
 * @param rhs     The right-hand MTBDD.
 * @param order   The variable order.
 * @return The maximum MTBDD.
 */
template <>
sylvan::Mtbdd max<expression::SmtTerm>(
    const sylvan::Mtbdd& lhs, const sylvan::Mtbdd& rhs,
    VariableOrder<SmtTerm>::VariableOrderPointer order) {
  MinMaxHelper<SmtTerm> helper{std::move(order)};

  return sylvan::sylvan_SmtTerm_max(lhs.GetMTBDD(), rhs.GetMTBDD(),
                                    toHandle(&helper));
}

/**
 * @brief Compute the element-wise minimum of two SmtTerm decision diagrams.
 *
 * @param lhs     The left-hand MTBDD.
 * @param rhs     The right-hand MTBDD.
 * @param order   The variable order.
 * @return The minimum MTBDD.
 */
template <>
sylvan::Mtbdd min<expression::SmtTerm>(
    const sylvan::Mtbdd& lhs, const sylvan::Mtbdd& rhs,
    VariableOrder<SmtTerm>::VariableOrderPointer order) {
  MinMaxHelper<SmtTerm> helper{std::move(order)};

  return sylvan::sylvan_SmtTerm_min(lhs.GetMTBDD(), rhs.GetMTBDD(),
                                    toHandle(&helper));
}

}  // namespace ddsolve::interface
