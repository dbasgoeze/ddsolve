#ifndef SYLVANINTERFACE_H
#define SYLVANINTERFACE_H

#include <stddef.h>
#include <stdint.h>
#include <sylvan.h>
#include <sylvan_mtbdd.h>

#ifdef __cplusplus
namespace ddsolve::interface {
using sylvan::MTBDD;
extern "C" {
#endif

extern int gPrintModelsFlag;

typedef uint32_t SylvanVariableId;

typedef struct LinearTermBase LinearTermBase;
typedef struct LinearScalarTypeBase LinearScalarTypeBase;
typedef struct LinearVariableOrderBase LinearVariableOrderBase;
typedef struct LinearSubstitutionBase LinearSubstitutionBase;
typedef struct LinearMinMaxHelperBase LinearMinMaxHelperBase;

typedef const LinearTermBase* LinearTermHandle;
typedef const LinearScalarTypeBase* LinearScalarTypeHandle;
typedef const LinearVariableOrderBase* LinearVariableOrderHandle;
typedef const LinearSubstitutionBase* LinearSubstitutionHandle;
typedef LinearMinMaxHelperBase* LinearMinMaxHelperHandle;

enum ComparisonType {
  COMPARISONTYPE_EQUALITY = 0,
  COMPARISONTYPE_INEQUALITY,
  COMPARISONTYPE_LESS_THAN,
  COMPARISONTYPE_LESS_THAN_OR_EQUAL,
  COMPARISONTYPE_GREATER_THAN,
  COMPARISONTYPE_GREATER_THAN_OR_EQUAL,
};
typedef enum ComparisonType ComparisonType;

enum SmtResult {
  SMTRESULT_UNSAT = 0,
  SMTRESULT_SAT,
  SMTRESULT_UNKNOWN,
};
typedef enum SmtResult SmtResult;

uint64_t LinearScalarType_get_id(LinearScalarTypeHandle handle);

LinearTermHandle LinearTerm_copy(LinearTermHandle handle);
void LinearTerm_destroy(LinearTermHandle handle);
int LinearTerm_equals(LinearTermHandle lhs, LinearTermHandle rhs);
int LinearTerm_less(LinearTermHandle lhs, LinearTermHandle rhs, MTBDD history);
int LinearTerm_less_eq(LinearTermHandle lhs, LinearTermHandle rhs,
                       MTBDD history);
LinearTermHandle LinearTerm_add(LinearTermHandle lhs, LinearTermHandle rhs);
LinearTermHandle LinearTerm_sub(LinearTermHandle lhs, LinearTermHandle rhs);
LinearTermHandle LinearTerm_scale(LinearTermHandle term_handle,
                                  LinearScalarTypeHandle scalar_handle);
LinearTermHandle LinearTerm_substitute(
    LinearTermHandle term_handle, LinearSubstitutionHandle substitution_handle);
uint64_t LinearTerm_hash(LinearTermHandle handle);
char* LinearTerm_to_str(LinearTermHandle handle, char* buf, size_t bufsize);

SmtResult LinearTerm_trivial_check(LinearTermHandle lhs, LinearTermHandle rhs,
                                   ComparisonType comp);

int LinearVariableOrder_compare(LinearVariableOrderHandle handle,
                                LinearTermHandle lhs, LinearTermHandle rhs,
                                MTBDD history, ComparisonType comp);
SmtResult LinearVariableOrder_compare_real(LinearVariableOrderHandle handle,
                                           LinearTermHandle lhs,
                                           LinearTermHandle rhs, MTBDD history,
                                           ComparisonType comp);
SmtResult LinearVariableOrder_decide(LinearVariableOrderHandle handle,
                                     MTBDD history);
uint64_t LinearVariableOrder_get_id(LinearVariableOrderHandle handlem);
void LinearVariableOrder_shutdown();

uint64_t LinearSubstitution_get_id(LinearSubstitutionHandle handle);

uint64_t LinearMinMaxHelper_get_id(LinearMinMaxHelperHandle handle);

void LinearMinMaxHelper_add_pair(LinearMinMaxHelperHandle handle,
                                 LinearTermHandle lhs, LinearTermHandle rhs);

void LinearMinMaxHelper_finalize(LinearMinMaxHelperHandle handle);

uint32_t LinearMinMaxHelper_get_var(LinearMinMaxHelperHandle handle,
                                    LinearTermHandle lhs, LinearTermHandle rhs);

LinearVariableOrderHandle LinearMinMaxHelper_get_order(
    LinearMinMaxHelperHandle handle);

////////////////////

typedef struct SmtTermBase SmtTermBase;
typedef SmtTermBase SmtScalarTypeBase;
typedef struct SmtVariableOrderBase SmtVariableOrderBase;
typedef struct SmtSubstitutionBase SmtSubstitutionBase;
typedef struct SmtMinMaxHelperBase SmtMinMaxHelperBase;

typedef const SmtTermBase* SmtTermHandle;
typedef SmtTermHandle SmtScalarTypeHandle;
typedef const SmtVariableOrderBase* SmtVariableOrderHandle;
typedef const SmtSubstitutionBase* SmtSubstitutionHandle;
typedef SmtMinMaxHelperBase* SmtMinMaxHelperHandle;

uint64_t SmtScalarType_get_id(SmtScalarTypeHandle handle);

SmtTermHandle SmtTerm_copy(SmtTermHandle handle);
void SmtTerm_destroy(SmtTermHandle handle);
int SmtTerm_equals(SmtTermHandle lhs, SmtTermHandle rhs);
int SmtTerm_less(SmtTermHandle lhs, SmtTermHandle rhs, MTBDD history);
int SmtTerm_less_eq(SmtTermHandle lhs, SmtTermHandle rhs, MTBDD history);
SmtTermHandle SmtTerm_add(SmtTermHandle lhs, SmtTermHandle rhs);
SmtTermHandle SmtTerm_sub(SmtTermHandle lhs, SmtTermHandle rhs);
SmtTermHandle SmtTerm_mul(SmtTermHandle lhs, SmtTermHandle rhs);
SmtTermHandle SmtTerm_div(SmtTermHandle lhs, SmtTermHandle rhs);
SmtTermHandle SmtTerm_scale(SmtTermHandle term_handle,
                            SmtScalarTypeHandle scalar_handle);
SmtTermHandle SmtTerm_substitute(SmtTermHandle term_handle,
                                 SmtSubstitutionHandle substitution_handle);
uint64_t SmtTerm_hash(SmtTermHandle handle);
char* SmtTerm_to_str(SmtTermHandle handle, char* buf, size_t bufsize);

SmtResult SmtTerm_trivial_check(SmtTermHandle lhs, SmtTermHandle rhs,
                                ComparisonType comp);

int SmtVariableOrder_compare(SmtVariableOrderHandle handle, SmtTermHandle lhs,
                             SmtTermHandle rhs, MTBDD history,
                             ComparisonType comp);
SmtResult SmtVariableOrder_decide(SmtVariableOrderHandle handle, MTBDD history);
uint64_t SmtVariableOrder_get_id(SmtVariableOrderHandle handlem);
void SmtVariableOrder_shutdown();

uint64_t SmtSubstitution_get_id(SmtSubstitutionHandle handle);

uint64_t SmtMinMaxHelper_get_id(SmtMinMaxHelperHandle handle);

void SmtMinMaxHelper_add_pair(SmtMinMaxHelperHandle handle, SmtTermHandle lhs,
                              SmtTermHandle rhs);

void SmtMinMaxHelper_finalize(SmtMinMaxHelperHandle handle);

uint32_t SmtMinMaxHelper_get_var(SmtMinMaxHelperHandle handle,
                                 SmtTermHandle lhs, SmtTermHandle rhs);

SmtVariableOrderHandle SmtMinMaxHelper_get_order(SmtMinMaxHelperHandle handle);

#ifdef __cplusplus
}
}  // namespace ddsolve::interface
#endif

#endif /* SYLVANINTERFACE_H */
