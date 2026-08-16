#ifndef SYLVAN_DDSOLVE_H
#define SYLVAN_DDSOLVE_H

#include <sylvan.h>
#include <sylvan_mtbdd.h>

#include "sylvan/SylvanInterface.h"

#ifdef __cplusplus
namespace sylvan {
using ddsolve::interface::ComparisonType;

using ddsolve::interface::LinearMinMaxHelperHandle;
using ddsolve::interface::LinearScalarTypeHandle;
using ddsolve::interface::LinearSubstitutionHandle;
using ddsolve::interface::LinearTermHandle;
using ddsolve::interface::LinearVariableOrderHandle;

using ddsolve::interface::SmtMinMaxHelperHandle;
using ddsolve::interface::SmtScalarTypeHandle;
using ddsolve::interface::SmtSubstitutionHandle;
using ddsolve::interface::SmtTermHandle;
using ddsolve::interface::SmtVariableOrderHandle;
extern "C" {
#endif /* __cplusplus */

void sylvan_init_ddsolve();

void sylvan_quit_ddsolve();

MTBDD sylvan_LinearTerm_term(LinearTermHandle handle);

LinearTermHandle sylvan_LinearTerm_get_term(MTBDD a);

TASK_DECL_2(MTBDD, sylvan_LinearTerm_op_plus, MTBDD*, MTBDD*);  // NOLINT
#define sylvan_LinearTerm_plus(a, b) mtbdd_apply(a, b, TASK(sylvan_LinearTerm_op_plus))

TASK_DECL_2(MTBDD, sylvan_LinearTerm_op_minus, MTBDD*, MTBDD*);  // NOLINT
#define sylvan_LinearTerm_minus(a, b) mtbdd_apply(a, b, TASK(sylvan_LinearTerm_op_minus))

TASK_DECL_1(MTBDD, sylvan_LinearTerm_op_prune, MTBDD);  // NOLINT
#define sylvan_LinearTerm_prune(a) RUN(sylvan_LinearTerm_op_prune, a)

TASK_DECL_2(MTBDD, sylvan_LinearTerm_op_scale, MTBDD, LinearScalarTypeHandle);  // NOLINT
#define sylvan_LinearTerm_scale(a, s) RUN(sylvan_LinearTerm_op_scale, a, s)

TASK_DECL_2(MTBDD, sylvan_LinearTerm_op_substitute_leafs, MTBDD,
            LinearSubstitutionHandle);  // NOLINT
#define sylvan_LinearTerm_substitute_leafs(a, substitution) \
  RUN(sylvan_LinearTerm_op_substitute_leafs, a, substitution)

TASK_DECL_3(MTBDD, sylvan_LinearTerm_op_replace_leaf, MTBDD, MTBDD, MTBDD);  // NOLINT
#define sylvan_LinearTerm_replace_leaf(a, b, c) RUN(sylvan_LinearTerm_op_replace_leaf, a, b, c)

MTBDD sylvan_LinearTerm_get_prunes(MTBDD a, LinearVariableOrderHandle order);

MTBDD sylvan_LinearTerm_prune_full(MTBDD a, LinearVariableOrderHandle order);

int sylvan_LinearTerm_compare(MTBDD a, MTBDD b, ComparisonType comp,
                    LinearVariableOrderHandle order);

MTBDD sylvan_LinearTerm_max(MTBDD lhs, MTBDD rhs, LinearMinMaxHelperHandle handle);

MTBDD sylvan_LinearTerm_min(MTBDD lhs, MTBDD rhs, LinearMinMaxHelperHandle handle);

//////////////////////////////////////////////////////////////////////////////

MTBDD sylvan_SmtTerm_term(SmtTermHandle handle);

SmtTermHandle sylvan_SmtTerm_get_term(MTBDD a);

TASK_DECL_2(MTBDD, sylvan_SmtTerm_op_plus, MTBDD*, MTBDD*);  // NOLINT
#define sylvan_SmtTerm_plus(a, b) mtbdd_apply(a, b, TASK(sylvan_SmtTerm_op_plus))

TASK_DECL_2(MTBDD, sylvan_SmtTerm_op_minus, MTBDD*, MTBDD*);  // NOLINT
#define sylvan_SmtTerm_minus(a, b) mtbdd_apply(a, b, TASK(sylvan_SmtTerm_op_minus))

TASK_DECL_2(MTBDD, sylvan_SmtTerm_op_mul, MTBDD*, MTBDD*);  // NOLINT
#define sylvan_SmtTerm_mul(a, b) mtbdd_apply(a, b, TASK(sylvan_SmtTerm_op_mul))

TASK_DECL_2(MTBDD, sylvan_SmtTerm_op_div, MTBDD*, MTBDD*);  // NOLINT
#define sylvan_SmtTerm_div(a, b) mtbdd_apply(a, b, TASK(sylvan_SmtTerm_op_div))

TASK_DECL_1(MTBDD, sylvan_SmtTerm_op_prune, MTBDD);  // NOLINT
#define sylvan_SmtTerm_prune(a) RUN(sylvan_SmtTerm_op_prune, a)

TASK_DECL_2(MTBDD, sylvan_SmtTerm_op_scale, MTBDD, SmtScalarTypeHandle);  // NOLINT
#define sylvan_SmtTerm_scale(a, s) RUN(sylvan_SmtTerm_op_scale, a, s)

TASK_DECL_2(MTBDD, sylvan_SmtTerm_op_substitute_leafs, MTBDD,
            SmtSubstitutionHandle);  // NOLINT
#define sylvan_SmtTerm_substitute_leafs(a, substitution) \
  RUN(sylvan_SmtTerm_op_substitute_leafs, a, substitution)

TASK_DECL_3(MTBDD, sylvan_SmtTerm_op_replace_leaf, MTBDD, MTBDD, MTBDD);  // NOLINT
#define sylvan_SmtTerm_replace_leaf(a, b, c) RUN(sylvan_SmtTerm_op_replace_leaf, a, b, c)

MTBDD sylvan_SmtTerm_get_prunes(MTBDD a, SmtVariableOrderHandle order);

MTBDD sylvan_SmtTerm_prune_full(MTBDD a, SmtVariableOrderHandle order);

int sylvan_SmtTerm_compare(MTBDD a, MTBDD b, ComparisonType comp,
                    SmtVariableOrderHandle order);

MTBDD sylvan_SmtTerm_max(MTBDD lhs, MTBDD rhs, SmtMinMaxHelperHandle handle);

MTBDD sylvan_SmtTerm_min(MTBDD lhs, MTBDD rhs, SmtMinMaxHelperHandle handle);

#ifdef __cplusplus
}
}  // namespace sylvan
#endif /* __cplusplus */

#endif /* SYLVAN_DDSOLVE_H */
