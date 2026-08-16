#include "sylvan/sylvan_ddsolve.h"

#include <sylvan_int.h>

#include "sylvan/SylvanInterface.h"
#include "sylvan/sylvan_ddsolve_config.h"

static uint32_t kLinearTerm_type;
static uint64_t kcache_LinearTerm_prune;
static uint64_t kcache_LinearTerm_scale;
static uint64_t kcache_LinearTerm_decisions;
static uint64_t kcache_LinearTerm_substitute_leafs;
static uint64_t kcache_LinearTerm_replace_leaf;
static uint64_t kcache_LinearTerm_compare;
static uint64_t kcache_LinearTerm_minmax_gather;
static uint64_t kcache_LinearTerm_minmax_transform;

static_assert(sizeof(void*) <= sizeof(uint64_t),
              "Sylvan only works if pointers are at most 64 bit");

static uint64_t sylvan_LinearTerm_hash(const uint64_t a, const uint64_t seed) {
  return LinearTerm_hash((LinearTermHandle)a) ^ seed;
}

static int sylvan_LinearTerm_equals(uint64_t a, uint64_t b) {
  if (a == b) {
    return 1;
  }

  LinearTermHandle x = (LinearTermHandle)a;
  LinearTermHandle y = (LinearTermHandle)b;

  return LinearTerm_equals(x, y);
}

static void sylvan_LinearTerm_create(uint64_t* a) {
  LinearTermHandle* x = (LinearTermHandle*)a;
  *x = LinearTerm_copy(*x);
}

static void sylvan_LinearTerm_destroy(uint64_t a) {
  LinearTermHandle x = (LinearTermHandle)a;
  LinearTerm_destroy(x);
}

static char* sylvan_LinearTerm_to_str(int comp, uint64_t a, char* buf,
                                      size_t buflen) {
  (void)comp;
  LinearTermHandle x = (LinearTermHandle)a;
  return LinearTerm_to_str(x, buf, buflen);
}

VOID_TASK_0(sylvan_LinearTerm_finalize_thread_local) {
  LinearVariableOrder_shutdown();
}

//////////////////////////////////////////////////////////////////////////////
static uint32_t kSmtTerm_type;
static uint64_t kcache_SmtTerm_prune;
static uint64_t kcache_SmtTerm_scale;
static uint64_t kcache_SmtTerm_decisions;
static uint64_t kcache_SmtTerm_substitute_leafs;
static uint64_t kcache_SmtTerm_replace_leaf;
static uint64_t kcache_SmtTerm_compare;
static uint64_t kcache_SmtTerm_minmax_gather;
static uint64_t kcache_SmtTerm_minmax_transform;

static_assert(sizeof(void*) <= sizeof(uint64_t),
              "Sylvan only works if pointers are at most 64 bit");

static uint64_t sylvan_SmtTerm_hash(const uint64_t a, const uint64_t seed) {
  return SmtTerm_hash((SmtTermHandle)a) ^ seed;
}

static int sylvan_SmtTerm_equals(uint64_t a, uint64_t b) {
  if (a == b) {
    return 1;
  }

  SmtTermHandle x = (SmtTermHandle)a;
  SmtTermHandle y = (SmtTermHandle)b;

  return SmtTerm_equals(x, y);
}

static void sylvan_SmtTerm_create(uint64_t* a) {
  SmtTermHandle* x = (SmtTermHandle*)a;
  *x = SmtTerm_copy(*x);
}

static void sylvan_SmtTerm_destroy(uint64_t a) {
  SmtTermHandle x = (SmtTermHandle)a;
  SmtTerm_destroy(x);
}

static char* sylvan_SmtTerm_to_str(int comp, uint64_t a, char* buf,
                                   size_t buflen) {
  (void)comp;
  SmtTermHandle x = (SmtTermHandle)a;
  return SmtTerm_to_str(x, buf, buflen);
}

VOID_TASK_0(sylvan_SmtTerm_finalize_thread_local) {
  SmtVariableOrder_shutdown();
}

MTBDD sylvan_SmtTerm_term(SmtTermHandle handle) {
  MTBDD rval = mtbdd_makeleaf(kSmtTerm_type, (uint64_t)handle);
  SmtTerm_destroy(handle);
  return rval;
}

//////////////////////////////////////////////////////////////////////////////

void sylvan_quit_ddsolve() {
  TOGETHER(sylvan_LinearTerm_finalize_thread_local);
  TOGETHER(sylvan_SmtTerm_finalize_thread_local);
}

void sylvan_init_ddsolve() {
  kLinearTerm_type = sylvan_mt_create_type();
  kcache_LinearTerm_prune = cache_next_opid();
  kcache_LinearTerm_decisions = cache_next_opid();
  kcache_LinearTerm_scale = cache_next_opid();
  kcache_LinearTerm_substitute_leafs = cache_next_opid();
  kcache_LinearTerm_replace_leaf = cache_next_opid();
  kcache_LinearTerm_compare = cache_next_opid();
  kcache_LinearTerm_minmax_gather = cache_next_opid();
  kcache_LinearTerm_minmax_transform = cache_next_opid();

  sylvan_mt_set_hash(kLinearTerm_type, sylvan_LinearTerm_hash);
  sylvan_mt_set_equals(kLinearTerm_type, sylvan_LinearTerm_equals);
  sylvan_mt_set_create(kLinearTerm_type, sylvan_LinearTerm_create);
  sylvan_mt_set_destroy(kLinearTerm_type, sylvan_LinearTerm_destroy);
  sylvan_mt_set_to_str(kLinearTerm_type, sylvan_LinearTerm_to_str);

  kSmtTerm_type = sylvan_mt_create_type();
  kcache_SmtTerm_prune = cache_next_opid();
  kcache_SmtTerm_decisions = cache_next_opid();
  kcache_SmtTerm_scale = cache_next_opid();
  kcache_SmtTerm_substitute_leafs = cache_next_opid();
  kcache_SmtTerm_replace_leaf = cache_next_opid();
  kcache_SmtTerm_compare = cache_next_opid();
  kcache_SmtTerm_minmax_gather = cache_next_opid();
  kcache_SmtTerm_minmax_transform = cache_next_opid();

  sylvan_mt_set_hash(kSmtTerm_type, sylvan_SmtTerm_hash);
  sylvan_mt_set_equals(kSmtTerm_type, sylvan_SmtTerm_equals);
  sylvan_mt_set_create(kSmtTerm_type, sylvan_SmtTerm_create);
  sylvan_mt_set_destroy(kSmtTerm_type, sylvan_SmtTerm_destroy);
  sylvan_mt_set_to_str(kSmtTerm_type, sylvan_SmtTerm_to_str);
}

MTBDD sylvan_LinearTerm_term(LinearTermHandle handle) {
  MTBDD rval = mtbdd_makeleaf(kLinearTerm_type, (uint64_t)handle);
  LinearTerm_destroy(handle);
  return rval;
}

LinearTermHandle sylvan_LinearTerm_get_term(MTBDD a) {
  if (mtbdd_gettype(a) == kLinearTerm_type) {
    return (LinearTermHandle)mtbdd_getvalue(a);
  }

  return NULL;
}

TASK_IMPL_2(MTBDD, sylvan_LinearTerm_op_plus, MTBDD*, pa, MTBDD*, pb) {
  MTBDD a = *pa;
  MTBDD b = *pb;

  if (a == mtbdd_false) {
    return b;
  }
  if (b == mtbdd_false) {
    return a;
  }

  if (mtbdd_isleaf(a) && mtbdd_isleaf(b)) {
    assert(mtbdd_gettype(a) == kLinearTerm_type &&
           mtbdd_gettype(b) == kLinearTerm_type);

    LinearTermHandle ah = (LinearTermHandle)mtbdd_getvalue(a);
    LinearTermHandle bh = (LinearTermHandle)mtbdd_getvalue(b);

    LinearTermHandle abh = LinearTerm_add(ah, bh);
    return sylvan_LinearTerm_term(abh);
  }

  // Commutative, so order canonically for cacheing
  if (a < b) {
    *pa = b;
    *pb = a;
  }

  return mtbdd_invalid;
}

TASK_IMPL_2(MTBDD, sylvan_LinearTerm_op_minus, MTBDD*, pa, MTBDD*, pb) {
  MTBDD a = *pa;
  MTBDD b = *pb;

  if (a == mtbdd_false) {
    return b;
  }
  if (b == mtbdd_false) {
    return a;
  }

  if (mtbdd_isleaf(a) && mtbdd_isleaf(b)) {
    assert(mtbdd_gettype(a) == kLinearTerm_type &&
           mtbdd_gettype(b) == kLinearTerm_type);

    LinearTermHandle ah = (LinearTermHandle)mtbdd_getvalue(a);
    LinearTermHandle bh = (LinearTermHandle)mtbdd_getvalue(b);

    LinearTermHandle abh = LinearTerm_sub(ah, bh);
    return sylvan_LinearTerm_term(abh);
  }

  return mtbdd_invalid;
}

TASK_IMPL_1(MTBDD, sylvan_LinearTerm_op_prune, MTBDD, a) {
  if (mtbdd_isleaf(a)) {
    return a;
  }

  MTBDD result = mtbdd_invalid;
  if (cache_get3(kcache_LinearTerm_prune, a, 0, 0, &result)) {
    return result;
  }

  /* Maybe perform garbage collection */
  sylvan_gc_test();

  mtbddnode_t na = MTBDD_GETNODE(a);
  uint32_t va = mtbddnode_getvariable(na);

  /* Get cofactors */
  MTBDD alow = node_getlow(a, na);
  MTBDD ahigh = node_gethigh(a, na);

  mtbdd_refs_spawn(SPAWN(sylvan_LinearTerm_op_prune, ahigh));
  MTBDD low = mtbdd_refs_push(CALL(sylvan_LinearTerm_op_prune, alow));
  MTBDD high = mtbdd_refs_sync(SYNC(sylvan_LinearTerm_op_prune));
  mtbdd_refs_pop(1);

  if (alow == mtbdd_false) {
    result = high;
  } else if (ahigh == mtbdd_false) {
    result = low;
  } else {
    result = mtbdd_makenode(va, low, high);
  }

  cache_put3(kcache_LinearTerm_prune, a, 0, 0, result);

  return result;
}

TASK_IMPL_2(MTBDD, sylvan_LinearTerm_op_scale, MTBDD, a, LinearScalarTypeHandle,
            s) {
  if (a == mtbdd_false) {
    return a;
  }

  const uint64_t scalar_id = LinearScalarType_get_id(s);
  MTBDD result = mtbdd_invalid;

#ifndef NO_SCALE_CACHE
  if (cache_get3(kcache_LinearTerm_scale, a, scalar_id, 0, &result)) {
    return result;
  }
#endif

  /* Maybe perform garbage collection */
  sylvan_gc_test();

  if (mtbdd_isleaf(a)) {
    assert(mtbdd_gettype(a) == kLinearTerm_type);
    LinearTermHandle ah = (LinearTermHandle)mtbdd_getvalue(a);

    result = sylvan_LinearTerm_term(LinearTerm_scale(ah, s));
  } else {
    mtbddnode_t na = MTBDD_GETNODE(a);
    uint32_t va = mtbddnode_getvariable(na);

    /* Get cofactors */
    MTBDD alow = node_getlow(a, na);
    MTBDD ahigh = node_gethigh(a, na);

    mtbdd_refs_spawn(SPAWN(sylvan_LinearTerm_op_scale, ahigh, s));
    MTBDD low = mtbdd_refs_push(CALL(sylvan_LinearTerm_op_scale, alow, s));
    MTBDD high = mtbdd_refs_sync(SYNC(sylvan_LinearTerm_op_scale));
    mtbdd_refs_pop(1);

    result = mtbdd_makenode(va, low, high);
  }

#ifndef NO_SCALE_CACHE
  cache_put3(kcache_LinearTerm_scale, a, scalar_id, 0, result);
#endif

  return result;
}

TASK_IMPL_2(MTBDD, sylvan_LinearTerm_op_substitute_leafs, MTBDD, a,
            LinearSubstitutionHandle, handle) {
  if (mtbdd_isleaf(a) && mtbdd_gettype(a) != kLinearTerm_type) {
    return a;
  }

  MTBDD result = mtbdd_invalid;
  if (cache_get3(kcache_LinearTerm_substitute_leafs, a,
                 LinearSubstitution_get_id(handle), 0, &result)) {
    return result;
  }

  /* Maybe perform garbage collection */
  sylvan_gc_test();

  if (mtbdd_isleaf(a)) {
    LinearTermHandle ah = (LinearTermHandle)mtbdd_getvalue(a);

    return sylvan_LinearTerm_term(LinearTerm_substitute(ah, handle));
  }

  mtbddnode_t na = MTBDD_GETNODE(a);
  uint32_t va = mtbddnode_getvariable(na);

  /* Get cofactors */
  MTBDD alow = node_getlow(a, na);
  MTBDD ahigh = node_gethigh(a, na);

  mtbdd_refs_spawn(SPAWN(sylvan_LinearTerm_op_substitute_leafs, ahigh, handle));
  MTBDD low = mtbdd_refs_push(
      CALL(sylvan_LinearTerm_op_substitute_leafs, alow, handle));
  MTBDD high = mtbdd_refs_sync(SYNC(sylvan_LinearTerm_op_substitute_leafs));
  mtbdd_refs_pop(1);

  result = mtbdd_makenode(va, low, high);

  cache_put3(kcache_LinearTerm_substitute_leafs, a,
             LinearSubstitution_get_id(handle), 0, result);

  return result;
}

TASK_IMPL_3(MTBDD, sylvan_LinearTerm_op_replace_leaf, MTBDD, a, MTBDD, old_leaf,
            MTBDD, new_leaf) {
  if (mtbdd_isleaf(a)) {
    if (a == old_leaf) {
      return new_leaf;
    }
    return a;
  }

  MTBDD result = mtbdd_invalid;
  if (cache_get3(kcache_LinearTerm_replace_leaf, a, old_leaf, new_leaf,
                 &result)) {
    return result;
  }

  /* Maybe perform garbage collection */
  sylvan_gc_test();

  mtbddnode_t na = MTBDD_GETNODE(a);

  /* Get cofactors */
  MTBDD alow = node_getlow(a, na);
  MTBDD ahigh = node_gethigh(a, na);

  mtbdd_refs_spawn(
      SPAWN(sylvan_LinearTerm_op_replace_leaf, ahigh, old_leaf, new_leaf));
  MTBDD low = mtbdd_refs_push(
      CALL(sylvan_LinearTerm_op_replace_leaf, alow, old_leaf, new_leaf));
  MTBDD high =
      mtbdd_refs_push(mtbdd_refs_sync(SYNC(sylvan_LinearTerm_op_replace_leaf)));
  MTBDD va_dd = mtbdd_refs_push(mtbdd_ithvar(mtbddnode_getvariable(na)));
  result = CALL(mtbdd_ite, va_dd, high, low);
  mtbdd_refs_pop(3);

  cache_put3(kcache_LinearTerm_replace_leaf, a, old_leaf, new_leaf, result);

  return result;
}

TASK_2(SmtResult, sylvan_LinearTerm_decide, MTBDD, history,
       LinearVariableOrderHandle, order) {
  return LinearVariableOrder_decide(order, history);
}

TASK_3(MTBDD, sylvan_LinearTerm_op_get_prunes, MTBDD, a, MTBDD, history,
       LinearVariableOrderHandle, order) {
  MTBDD rval = mtbdd_true;
  if (mtbdd_isleaf(a)) {
    return rval;
  }

  mtbdd_refs_pushptr(&rval);
  mtbddnode_t na = MTBDD_GETNODE(a);
  uint32_t va = mtbddnode_getvariable(na);
  MTBDD high_history = mtbdd_refs_push(mtbdd_times(history, mtbdd_ithvar(va)));
  MTBDD low_history =
      mtbdd_refs_push(mtbdd_times(history, mtbdd_not(mtbdd_ithvar(va))));
  uint64_t order_id = LinearVariableOrder_get_id(order);

  uint64_t high_result;
  int spawned_high = 0;
  if (!cache_get3(kcache_LinearTerm_decisions, high_history, order_id, 0,
                  &high_result)) {
    // fprintf(stderr, "Cache miss in high at var %d\n", va);
    SPAWN(sylvan_LinearTerm_decide, high_history, order);
    spawned_high = 1;
  }

  uint64_t low_result;
  if (!cache_get3(kcache_LinearTerm_decisions, low_history, order_id, 0,
                  &low_result)) {
    // fprintf(stderr, "Cache miss in low at var %d\n", va);
    low_result = CALL(sylvan_LinearTerm_decide, low_history, order);
    cache_put3(kcache_LinearTerm_decisions, low_history, order_id, 0,
               low_result);
  }

  if (spawned_high) {
    high_result = SYNC(sylvan_LinearTerm_decide);
    cache_put3(kcache_LinearTerm_decisions, high_history, order_id, 0,
               high_result);
  }

  if (low_result == SMTRESULT_UNSAT) {
    rval = mtbdd_times(rval, mtbdd_not(low_history));
  } else {
    MTBDD alow = node_getlow(a, na);
    mtbdd_refs_spawn(
        SPAWN(sylvan_LinearTerm_op_get_prunes, alow, low_history, order));
  }

  if (high_result == SMTRESULT_UNSAT) {
    rval = mtbdd_times(rval, mtbdd_not(high_history));
  } else {
    MTBDD ahigh = node_gethigh(a, na);
    MTBDD tmp = mtbdd_refs_push(
        CALL(sylvan_LinearTerm_op_get_prunes, ahigh, high_history, order));
    rval = mtbdd_times(rval, tmp);
    mtbdd_refs_pop(1);
  }

  if (low_result != SMTRESULT_UNSAT) {
    rval = mtbdd_times(rval,
                       mtbdd_refs_sync(SYNC(sylvan_LinearTerm_op_get_prunes)));
  }

  mtbdd_refs_popptr(1);
  mtbdd_refs_pop(2);

  // fprintf(stderr, "%lu %lu\n", low_result, high_result);

  return rval;
}

MTBDD sylvan_LinearTerm_get_prunes(MTBDD a, LinearVariableOrderHandle order) {
  return RUN(sylvan_LinearTerm_op_get_prunes, a, mtbdd_true, order);
}

TASK_3(MTBDD, sylvan_LinearTerm_op_prune_full, MTBDD, a, MTBDD, history,
       LinearVariableOrderHandle, order) {
  if (mtbdd_isleaf(a)) {
    return a;
  }

  mtbddnode_t na = MTBDD_GETNODE(a);
  uint32_t va = mtbddnode_getvariable(na);
  MTBDD high_history = mtbdd_refs_push(mtbdd_times(history, mtbdd_ithvar(va)));
  MTBDD low_history =
      mtbdd_refs_push(mtbdd_times(history, mtbdd_not(mtbdd_ithvar(va))));
  uint64_t order_id = LinearVariableOrder_get_id(order);

  uint64_t high_result;
  int spawned_high = 0;
  if (!cache_get3(kcache_LinearTerm_decisions, high_history, order_id, 0,
                  &high_result)) {
    SPAWN(sylvan_LinearTerm_decide, high_history, order);
    spawned_high = 1;
  }

  uint64_t low_result;
  if (!cache_get3(kcache_LinearTerm_decisions, low_history, order_id, 0,
                  &low_result)) {
    low_result = CALL(sylvan_LinearTerm_decide, low_history, order);
    cache_put3(kcache_LinearTerm_decisions, low_history, order_id, 0,
               low_result);
  }

  if (spawned_high) {
    high_result = SYNC(sylvan_LinearTerm_decide);
    cache_put3(kcache_LinearTerm_decisions, high_history, order_id, 0,
               high_result);
  }

  MTBDD rval = mtbdd_false;
  mtbdd_refs_pushptr(&rval);
  MTBDD alow = node_getlow(a, na);
  MTBDD ahigh = node_gethigh(a, na);
  MTBDD low, high;

  // We only care about if it is proveable UNSAT
  low_result = (low_result != SMTRESULT_UNSAT);
  high_result = (high_result != SMTRESULT_UNSAT);

  if (low_result || high_result) {
    if (low_result) {
      mtbdd_refs_spawn(
          SPAWN(sylvan_LinearTerm_op_prune_full, alow, low_history, order));
    }

    if (high_result) {
      high = mtbdd_refs_push(
          CALL(sylvan_LinearTerm_op_prune_full, ahigh, high_history, order));
    }

    if (low_result) {
      low = mtbdd_refs_push(
          mtbdd_refs_sync(SYNC(sylvan_LinearTerm_op_prune_full)));
    }

    // fprintf(stderr, "What %lu %lu\n", low_result, high_result);

    if (!low_result) {
      rval = high;
      mtbdd_refs_pop(1);
    } else if (!high_result) {
      rval = low;
      mtbdd_refs_pop(1);
    } else {
      rval = mtbdd_makenode(va, low, high);
      mtbdd_refs_pop(2);
    }
  }

  // histories
  mtbdd_refs_pop(2);

  // rval
  mtbdd_refs_popptr(1);

  return rval;
}

MTBDD sylvan_LinearTerm_prune_full(MTBDD a, LinearVariableOrderHandle order) {
  return RUN(sylvan_LinearTerm_op_prune_full, a, mtbdd_true, order);
}

TASK_5(int, sylvan_LinearTerm_op_compare, MTBDD, a, MTBDD, b, MTBDD, history,
       ComparisonType, comp, LinearVariableOrderHandle, order) {
  uint64_t order_id = LinearVariableOrder_get_id(order);

  uint64_t result;
  if (cache_get6(kcache_LinearTerm_compare, a, b, history, order_id, comp,
                 &result, NULL)) {
    return result;
  }

  int la = mtbdd_isleaf(a);
  int lb = mtbdd_isleaf(b);

  if (la && lb) {
    if (mtbdd_gettype(a) == kLinearTerm_type &&
        mtbdd_gettype(b) == kLinearTerm_type) {
      LinearTermHandle ah = (LinearTermHandle)mtbdd_getvalue(a);
      LinearTermHandle bh = (LinearTermHandle)mtbdd_getvalue(b);

#ifdef PERFECT_SMT
      SmtResult trivial = LinearTerm_trivial_check(ah, bh, comp);

      switch (trivial) {
        case SMTRESULT_UNSAT:
          if (!cache_get3(kcache_LinearTerm_decisions, history, order_id, 0,
                          &result)) {
            result = CALL(sylvan_LinearTerm_decide, history, order);
            cache_put3(kcache_LinearTerm_decisions, history, order_id, 0,
                       result);
          }
          result = (result == SMTRESULT_UNSAT);
          break;
        case SMTRESULT_SAT:
          result = 1;
          break;
        default:
          result = LinearVariableOrder_compare(order, ah, bh, history, comp);
          break;
      }
#else
      result = LinearVariableOrder_compare(order, ah, bh, history, comp);
#endif
    } else {
      result = 1;
    }
  } else {
    mtbddnode_t na, nb;
    uint32_t va, vb;
    if (!la) {
      na = MTBDD_GETNODE(a);
      va = mtbddnode_getvariable(na);
    } else {
      na = 0;
      va = 0xffffffff;
    }
    if (!lb) {
      nb = MTBDD_GETNODE(b);
      vb = mtbddnode_getvariable(nb);
    } else {
      nb = 0;
      vb = 0xffffffff;
    }
    uint32_t v = va < vb ? va : vb;

    MTBDD high_history = mtbdd_refs_push(mtbdd_times(history, mtbdd_ithvar(v)));
    MTBDD low_history =
        mtbdd_refs_push(mtbdd_times(history, mtbdd_not(mtbdd_ithvar(v))));

    /* Get cofactors */
    MTBDD alow, ahigh, blow, bhigh;
    if (!la && va == v) {
      alow = node_getlow(a, na);
      ahigh = node_gethigh(a, na);
    } else {
      alow = a;
      ahigh = a;
    }
    if (!lb && vb == v) {
      blow = node_getlow(b, nb);
      bhigh = node_gethigh(b, nb);
    } else {
      blow = b;
      bhigh = b;
    }

    SPAWN(sylvan_LinearTerm_op_compare, ahigh, bhigh, high_history, comp,
          order);
    int low = CALL(sylvan_LinearTerm_op_compare, alow, blow, low_history, comp,
                   order);
    int high = 0;
    high = SYNC(sylvan_LinearTerm_op_compare);
    mtbdd_refs_pop(2);

    result = low && high;
  }

  /* Store in cache */
  cache_put6(kcache_LinearTerm_compare, a, b, history,
             LinearVariableOrder_get_id(order), comp, result, 0);

  return result;
}

int sylvan_LinearTerm_compare(MTBDD a, MTBDD b, ComparisonType comp,
                              LinearVariableOrderHandle order) {
  // normalize for caching
  if (comp == COMPARISONTYPE_GREATER_THAN) {
    MTBDD tmp = a;
    a = b;
    b = tmp;
  }
  int negate = 0;
  switch (comp) {
    case COMPARISONTYPE_INEQUALITY:
      negate = 1;
      comp = COMPARISONTYPE_EQUALITY;
      break;
    case COMPARISONTYPE_GREATER_THAN:
      negate = 1;
      comp = COMPARISONTYPE_LESS_THAN_OR_EQUAL;
      break;
    case COMPARISONTYPE_GREATER_THAN_OR_EQUAL:
      negate = 1;
      comp = COMPARISONTYPE_LESS_THAN;
      break;
    default:
      break;
  }

  const int result =
      RUN(sylvan_LinearTerm_op_compare, a, b, mtbdd_true, comp, order);

  return negate ? !result : result;
}

VOID_TASK_4(sylvan_LinearTerm_op_minmax_gather, MTBDD, a, MTBDD, b, MTBDD,
            history, LinearMinMaxHelperHandle, handle) {
  // is commutative so swap if needed
  if (a < b) {
    MTBDD tmp = a;
    a = b;
    b = tmp;
  }

  int la = mtbdd_isleaf(a);
  int lb = mtbdd_isleaf(b);

  uint64_t minmax_helper_id = LinearMinMaxHelper_get_id(handle);

#ifndef NO_MINMAX_GATHER_CACHE
  uint64_t result = 0;
  if (cache_get6(kcache_LinearTerm_minmax_gather, a, b, history,
                 minmax_helper_id, 0, &result, NULL)) {
    // fprintf(stderr, "Found!\n");
    // mtbdd_fprintdot(stderr, history);
    return;
  }
#endif

  if (la && lb) {
    if (mtbdd_gettype(a) == kLinearTerm_type &&
        mtbdd_gettype(b) == kLinearTerm_type) {
      if (CALL(sylvan_LinearTerm_op_compare, a, b, history,
               COMPARISONTYPE_LESS_THAN,
               LinearMinMaxHelper_get_order(handle)) &&
          CALL(sylvan_LinearTerm_op_compare, b, a, history,
               COMPARISONTYPE_LESS_THAN,
               LinearMinMaxHelper_get_order(handle))) {
        LinearTermHandle ah = (LinearTermHandle)mtbdd_getvalue(a);
        LinearTermHandle bh = (LinearTermHandle)mtbdd_getvalue(b);

        LinearMinMaxHelper_add_pair(handle, ah, bh);
      }
    }
  } else {
    mtbddnode_t na, nb;
    uint32_t va, vb;
    if (!la) {
      na = MTBDD_GETNODE(a);
      va = mtbddnode_getvariable(na);
    } else {
      na = 0;
      va = 0xffffffff;
    }
    if (!lb) {
      nb = MTBDD_GETNODE(b);
      vb = mtbddnode_getvariable(nb);
    } else {
      nb = 0;
      vb = 0xffffffff;
    }
    uint32_t v = va < vb ? va : vb;

    /* Get cofactors */
    MTBDD alow, ahigh, blow, bhigh;
    if (!la && va == v) {
      alow = node_getlow(a, na);
      ahigh = node_gethigh(a, na);
    } else {
      alow = a;
      ahigh = a;
    }
    if (!lb && vb == v) {
      blow = node_getlow(b, nb);
      bhigh = node_gethigh(b, nb);
    } else {
      blow = b;
      bhigh = b;
    }

    MTBDD high_history = mtbdd_refs_push(mtbdd_times(history, mtbdd_ithvar(v)));
    MTBDD low_history =
        mtbdd_refs_push(mtbdd_times(history, mtbdd_not(mtbdd_ithvar(v))));

    SPAWN(sylvan_LinearTerm_op_minmax_gather, ahigh, bhigh, high_history,
          handle);
    CALL(sylvan_LinearTerm_op_minmax_gather, alow, blow, low_history, handle);
    SYNC(sylvan_LinearTerm_op_minmax_gather);

    mtbdd_refs_pop(2);
  }

#ifndef NO_MINMAX_GATHER_CACHE
  result = 1;
  cache_put6(kcache_LinearTerm_minmax_gather, a, b, history, minmax_helper_id,
             0, result, 0);
#endif
}

TASK_5(MTBDD, sylvan_LinearTerm_op_minmax_transform, MTBDD, a, MTBDD, b, MTBDD,
       history, LinearMinMaxHelperHandle, handle, int, min) {
  // is commutative so swap if needed
  if (a < b) {
    MTBDD tmp = a;
    a = b;
    b = tmp;
  }

  MTBDD result;

#ifndef NO_MINMAX_TRANSFORM_CACHE
  // TODO(DB) investigate if min is needed here for the cache
  if (cache_get3(kcache_LinearTerm_minmax_transform, a, b,
                 LinearMinMaxHelper_get_id(handle) | !!min, &result)) {
    return result;
  }
#endif

  int la = mtbdd_isleaf(a);
  int lb = mtbdd_isleaf(b);

  if (la && lb) {
    if (mtbdd_gettype(a) == kLinearTerm_type &&
        mtbdd_gettype(b) == kLinearTerm_type) {
      int a_less =
          CALL(sylvan_LinearTerm_op_compare, a, b, history,
               COMPARISONTYPE_LESS_THAN, LinearMinMaxHelper_get_order(handle));
      int a_greater =
          CALL(sylvan_LinearTerm_op_compare, b, a, history,
               COMPARISONTYPE_LESS_THAN, LinearMinMaxHelper_get_order(handle));

      if (a_less && !a_greater) {
        result = min ? a : b;
      } else if (a_greater && !a_less) {
        result = min ? b : a;
      } else if (!a_greater && !a_less) {
        // a == b? Or not reduced?
        result = a;
      } else {
        LinearTermHandle ah = (LinearTermHandle)mtbdd_getvalue(a);
        LinearTermHandle bh = (LinearTermHandle)mtbdd_getvalue(b);
        uint32_t var = LinearMinMaxHelper_get_var(handle, ah, bh);

        result = min ? mtbdd_makenode(var, b, a) : mtbdd_makenode(var, a, b);
      }
    } else {
      result = mtbdd_gettype(a) == kLinearTerm_type ? a : b;
    }
  } else {
    mtbddnode_t na, nb;
    uint32_t va, vb;
    if (!la) {
      na = MTBDD_GETNODE(a);
      va = mtbddnode_getvariable(na);
    } else {
      na = 0;
      va = 0xffffffff;
    }
    if (!lb) {
      nb = MTBDD_GETNODE(b);
      vb = mtbddnode_getvariable(nb);
    } else {
      nb = 0;
      vb = 0xffffffff;
    }
    uint32_t v = va < vb ? va : vb;

    /* Get cofactors */
    MTBDD alow, ahigh, blow, bhigh;
    if (!la && va == v) {
      alow = node_getlow(a, na);
      ahigh = node_gethigh(a, na);
    } else {
      alow = a;
      ahigh = a;
    }
    if (!lb && vb == v) {
      blow = node_getlow(b, nb);
      bhigh = node_gethigh(b, nb);
    } else {
      blow = b;
      bhigh = b;
    }

    MTBDD high_history = mtbdd_refs_push(mtbdd_times(history, mtbdd_ithvar(v)));
    MTBDD low_history =
        mtbdd_refs_push(mtbdd_times(history, mtbdd_not(mtbdd_ithvar(v))));

    mtbdd_refs_spawn(SPAWN(sylvan_LinearTerm_op_minmax_transform, ahigh, bhigh,
                           high_history, handle, min));
    MTBDD low = mtbdd_refs_push(CALL(sylvan_LinearTerm_op_minmax_transform,
                                     alow, blow, low_history, handle, min));
    MTBDD high = mtbdd_refs_push(
        mtbdd_refs_sync(SYNC(sylvan_LinearTerm_op_minmax_transform)));
    MTBDD v_dd = mtbdd_refs_push(mtbdd_ithvar(v));
    result = CALL(mtbdd_ite, v_dd, high, low);
    mtbdd_refs_pop(5);
  }

#ifndef NO_MINMAX_TRANSFORM_CACHE
  cache_put3(kcache_LinearTerm_minmax_transform, a, b,
             LinearMinMaxHelper_get_id(handle) & !!min, result);
#endif

  return result;
}

MTBDD sylvan_LinearTerm_max(MTBDD lhs, MTBDD rhs,
                            LinearMinMaxHelperHandle handle) {
  RUN(sylvan_LinearTerm_op_minmax_gather, lhs, rhs, mtbdd_true, handle);
  LinearMinMaxHelper_finalize(handle);
  return RUN(sylvan_LinearTerm_op_minmax_transform, lhs, rhs, mtbdd_true,
             handle, 0);
}

MTBDD sylvan_LinearTerm_min(MTBDD lhs, MTBDD rhs,
                            LinearMinMaxHelperHandle handle) {
  RUN(sylvan_LinearTerm_op_minmax_gather, lhs, rhs, mtbdd_true, handle);
  LinearMinMaxHelper_finalize(handle);
  return RUN(sylvan_LinearTerm_op_minmax_transform, lhs, rhs, mtbdd_true,
             handle, 1);
}

///////////////////////////////////////////////////////////////////////////////

SmtTermHandle sylvan_SmtTerm_get_term(MTBDD a) {
  if (mtbdd_gettype(a) == kSmtTerm_type) {
    return (SmtTermHandle)mtbdd_getvalue(a);
  }

  return NULL;
}

TASK_IMPL_2(MTBDD, sylvan_SmtTerm_op_plus, MTBDD*, pa, MTBDD*, pb) {
  MTBDD a = *pa;
  MTBDD b = *pb;

  if (a == mtbdd_false) {
    return b;
  }
  if (b == mtbdd_false) {
    return a;
  }

  if (mtbdd_isleaf(a) && mtbdd_isleaf(b)) {
    assert(mtbdd_gettype(a) == kSmtTerm_type &&
           mtbdd_gettype(b) == kSmtTerm_type);

    SmtTermHandle ah = (SmtTermHandle)mtbdd_getvalue(a);
    SmtTermHandle bh = (SmtTermHandle)mtbdd_getvalue(b);

    SmtTermHandle abh = SmtTerm_add(ah, bh);
    return sylvan_SmtTerm_term(abh);
  }

  // Commutative, so order canonically for cacheing
  if (a < b) {
    *pa = b;
    *pb = a;
  }

  return mtbdd_invalid;
}

TASK_IMPL_2(MTBDD, sylvan_SmtTerm_op_minus, MTBDD*, pa, MTBDD*, pb) {
  MTBDD a = *pa;
  MTBDD b = *pb;

  if (a == mtbdd_false) {
    return b;
  }
  if (b == mtbdd_false) {
    return a;
  }

  if (mtbdd_isleaf(a) && mtbdd_isleaf(b)) {
    assert(mtbdd_gettype(a) == kSmtTerm_type &&
           mtbdd_gettype(b) == kSmtTerm_type);

    SmtTermHandle ah = (SmtTermHandle)mtbdd_getvalue(a);
    SmtTermHandle bh = (SmtTermHandle)mtbdd_getvalue(b);

    SmtTermHandle abh = SmtTerm_sub(ah, bh);
    return sylvan_SmtTerm_term(abh);
  }

  return mtbdd_invalid;
}

TASK_IMPL_2(MTBDD, sylvan_SmtTerm_op_mul, MTBDD*, pa, MTBDD*, pb) {
  MTBDD a = *pa;
  MTBDD b = *pb;

  if (a == mtbdd_false) {
    return b;
  }
  if (b == mtbdd_false) {
    return a;
  }

  if (mtbdd_isleaf(a) && mtbdd_isleaf(b)) {
    assert(mtbdd_gettype(a) == kSmtTerm_type &&
           mtbdd_gettype(b) == kSmtTerm_type);

    SmtTermHandle ah = (SmtTermHandle)mtbdd_getvalue(a);
    SmtTermHandle bh = (SmtTermHandle)mtbdd_getvalue(b);

    SmtTermHandle abh = SmtTerm_mul(ah, bh);
    return sylvan_SmtTerm_term(abh);
  }

  // Commutative, so order canonically for cacheing
  if (a < b) {
    *pa = b;
    *pb = a;
  }

  return mtbdd_invalid;
}

TASK_IMPL_2(MTBDD, sylvan_SmtTerm_op_div, MTBDD*, pa, MTBDD*, pb) {
  MTBDD a = *pa;
  MTBDD b = *pb;

  if (a == mtbdd_false) {
    return b;
  }
  if (b == mtbdd_false) {
    return a;
  }

  if (mtbdd_isleaf(a) && mtbdd_isleaf(b)) {
    assert(mtbdd_gettype(a) == kSmtTerm_type &&
           mtbdd_gettype(b) == kSmtTerm_type);

    SmtTermHandle ah = (SmtTermHandle)mtbdd_getvalue(a);
    SmtTermHandle bh = (SmtTermHandle)mtbdd_getvalue(b);

    SmtTermHandle abh = SmtTerm_div(ah, bh);
    return sylvan_SmtTerm_term(abh);
  }

  // Commutative, so order canonically for cacheing
  if (a < b) {
    *pa = b;
    *pb = a;
  }

  return mtbdd_invalid;
}

TASK_IMPL_1(MTBDD, sylvan_SmtTerm_op_prune, MTBDD, a) {
  if (mtbdd_isleaf(a)) {
    return a;
  }

  MTBDD result = mtbdd_invalid;
  if (cache_get3(kcache_SmtTerm_prune, a, 0, 0, &result)) {
    return result;
  }

  /* Maybe perform garbage collection */
  sylvan_gc_test();

  mtbddnode_t na = MTBDD_GETNODE(a);
  uint32_t va = mtbddnode_getvariable(na);

  /* Get cofactors */
  MTBDD alow = node_getlow(a, na);
  MTBDD ahigh = node_gethigh(a, na);

  mtbdd_refs_spawn(SPAWN(sylvan_SmtTerm_op_prune, ahigh));
  MTBDD low = mtbdd_refs_push(CALL(sylvan_SmtTerm_op_prune, alow));
  MTBDD high = mtbdd_refs_sync(SYNC(sylvan_SmtTerm_op_prune));
  mtbdd_refs_pop(1);

  if (alow == mtbdd_false) {
    result = high;
  } else if (ahigh == mtbdd_false) {
    result = low;
  } else {
    result = mtbdd_makenode(va, low, high);
  }

  cache_put3(kcache_SmtTerm_prune, a, 0, 0, result);

  return result;
}

TASK_IMPL_2(MTBDD, sylvan_SmtTerm_op_scale, MTBDD, a, SmtScalarTypeHandle, s) {
  if (a == mtbdd_false) {
    return a;
  }

  const uint64_t scalar_id = SmtScalarType_get_id(s);
  MTBDD result = mtbdd_invalid;

#ifndef NO_SCALE_CACHE
  if (cache_get3(kcache_SmtTerm_scale, a, scalar_id, 0, &result)) {
    return result;
  }
#endif

  /* Maybe perform garbage collection */
  sylvan_gc_test();

  if (mtbdd_isleaf(a)) {
    assert(mtbdd_gettype(a) == kSmtTerm_type);
    SmtTermHandle ah = (SmtTermHandle)mtbdd_getvalue(a);

    result = sylvan_SmtTerm_term(SmtTerm_scale(ah, s));
  } else {
    mtbddnode_t na = MTBDD_GETNODE(a);
    uint32_t va = mtbddnode_getvariable(na);

    /* Get cofactors */
    MTBDD alow = node_getlow(a, na);
    MTBDD ahigh = node_gethigh(a, na);

    mtbdd_refs_spawn(SPAWN(sylvan_SmtTerm_op_scale, ahigh, s));
    MTBDD low = mtbdd_refs_push(CALL(sylvan_SmtTerm_op_scale, alow, s));
    MTBDD high = mtbdd_refs_sync(SYNC(sylvan_SmtTerm_op_scale));
    mtbdd_refs_pop(1);

    result = mtbdd_makenode(va, low, high);
  }

#ifndef NO_SCALE_CACHE
  cache_put3(kcache_SmtTerm_scale, a, scalar_id, 0, result);
#endif

  return result;
}

TASK_IMPL_2(MTBDD, sylvan_SmtTerm_op_substitute_leafs, MTBDD, a,
            SmtSubstitutionHandle, handle) {
  if (mtbdd_isleaf(a) && mtbdd_gettype(a) != kSmtTerm_type) {
    return a;
  }

  MTBDD result = mtbdd_invalid;
  if (cache_get3(kcache_SmtTerm_substitute_leafs, a,
                 SmtSubstitution_get_id(handle), 0, &result)) {
    return result;
  }

  /* Maybe perform garbage collection */
  sylvan_gc_test();

  if (mtbdd_isleaf(a)) {
    SmtTermHandle ah = (SmtTermHandle)mtbdd_getvalue(a);

    return sylvan_SmtTerm_term(SmtTerm_substitute(ah, handle));
  }

  mtbddnode_t na = MTBDD_GETNODE(a);
  uint32_t va = mtbddnode_getvariable(na);

  /* Get cofactors */
  MTBDD alow = node_getlow(a, na);
  MTBDD ahigh = node_gethigh(a, na);

  mtbdd_refs_spawn(SPAWN(sylvan_SmtTerm_op_substitute_leafs, ahigh, handle));
  MTBDD low =
      mtbdd_refs_push(CALL(sylvan_SmtTerm_op_substitute_leafs, alow, handle));
  MTBDD high = mtbdd_refs_sync(SYNC(sylvan_SmtTerm_op_substitute_leafs));
  mtbdd_refs_pop(1);

  result = mtbdd_makenode(va, low, high);

  cache_put3(kcache_SmtTerm_substitute_leafs, a, SmtSubstitution_get_id(handle),
             0, result);

  return result;
}

TASK_IMPL_3(MTBDD, sylvan_SmtTerm_op_replace_leaf, MTBDD, a, MTBDD, old_leaf,
            MTBDD, new_leaf) {
  if (mtbdd_isleaf(a)) {
    if (a == old_leaf) {
      return new_leaf;
    }
    return a;
  }

  MTBDD result = mtbdd_invalid;
  if (cache_get3(kcache_SmtTerm_replace_leaf, a, old_leaf, new_leaf, &result)) {
    return result;
  }

  /* Maybe perform garbage collection */
  sylvan_gc_test();

  mtbddnode_t na = MTBDD_GETNODE(a);

  /* Get cofactors */
  MTBDD alow = node_getlow(a, na);
  MTBDD ahigh = node_gethigh(a, na);

  mtbdd_refs_spawn(
      SPAWN(sylvan_SmtTerm_op_replace_leaf, ahigh, old_leaf, new_leaf));
  MTBDD low = mtbdd_refs_push(
      CALL(sylvan_SmtTerm_op_replace_leaf, alow, old_leaf, new_leaf));
  MTBDD high =
      mtbdd_refs_push(mtbdd_refs_sync(SYNC(sylvan_SmtTerm_op_replace_leaf)));
  MTBDD va_dd = mtbdd_refs_push(mtbdd_ithvar(mtbddnode_getvariable(na)));
  result = CALL(mtbdd_ite, va_dd, high, low);
  mtbdd_refs_pop(3);

  cache_put3(kcache_SmtTerm_replace_leaf, a, old_leaf, new_leaf, result);

  return result;
}

TASK_2(SmtResult, sylvan_SmtTerm_decide, MTBDD, history, SmtVariableOrderHandle,
       order) {
  return SmtVariableOrder_decide(order, history);
}

TASK_3(MTBDD, sylvan_SmtTerm_op_get_prunes, MTBDD, a, MTBDD, history,
       SmtVariableOrderHandle, order) {
  MTBDD rval = mtbdd_true;
  if (mtbdd_isleaf(a)) {
    return rval;
  }

  mtbdd_refs_pushptr(&rval);
  mtbddnode_t na = MTBDD_GETNODE(a);
  uint32_t va = mtbddnode_getvariable(na);
  MTBDD high_history = mtbdd_refs_push(mtbdd_times(history, mtbdd_ithvar(va)));
  MTBDD low_history =
      mtbdd_refs_push(mtbdd_times(history, mtbdd_not(mtbdd_ithvar(va))));
  uint64_t order_id = SmtVariableOrder_get_id(order);

  uint64_t high_result;
  int spawned_high = 0;
  if (!cache_get3(kcache_SmtTerm_decisions, high_history, order_id, 0,
                  &high_result)) {
    // fprintf(stderr, "Cache miss in high at var %d\n", va);
    SPAWN(sylvan_SmtTerm_decide, high_history, order);
    spawned_high = 1;
  }

  uint64_t low_result;
  if (!cache_get3(kcache_SmtTerm_decisions, low_history, order_id, 0,
                  &low_result)) {
    // fprintf(stderr, "Cache miss in low at var %d\n", va);
    low_result = CALL(sylvan_SmtTerm_decide, low_history, order);
    cache_put3(kcache_SmtTerm_decisions, low_history, order_id, 0, low_result);
  }

  if (spawned_high) {
    high_result = SYNC(sylvan_SmtTerm_decide);
    cache_put3(kcache_SmtTerm_decisions, high_history, order_id, 0,
               high_result);
  }

  if (low_result == SMTRESULT_UNSAT) {
    rval = mtbdd_times(rval, mtbdd_not(low_history));
  } else {
    MTBDD alow = node_getlow(a, na);
    mtbdd_refs_spawn(
        SPAWN(sylvan_SmtTerm_op_get_prunes, alow, low_history, order));
  }

  if (high_result == SMTRESULT_UNSAT) {
    rval = mtbdd_times(rval, mtbdd_not(high_history));
  } else {
    MTBDD ahigh = node_gethigh(a, na);
    MTBDD tmp = mtbdd_refs_push(
        CALL(sylvan_SmtTerm_op_get_prunes, ahigh, high_history, order));
    rval = mtbdd_times(rval, tmp);
    mtbdd_refs_pop(1);
  }

  if (low_result != SMTRESULT_UNSAT) {
    rval =
        mtbdd_times(rval, mtbdd_refs_sync(SYNC(sylvan_SmtTerm_op_get_prunes)));
  }

  mtbdd_refs_popptr(1);
  mtbdd_refs_pop(2);

  // fprintf(stderr, "%lu %lu\n", low_result, high_result);

  return rval;
}

MTBDD sylvan_SmtTerm_get_prunes(MTBDD a, SmtVariableOrderHandle order) {
  return RUN(sylvan_SmtTerm_op_get_prunes, a, mtbdd_true, order);
}

TASK_3(MTBDD, sylvan_SmtTerm_op_prune_full, MTBDD, a, MTBDD, history,
       SmtVariableOrderHandle, order) {
  if (mtbdd_isleaf(a)) {
    return a;
  }

  mtbddnode_t na = MTBDD_GETNODE(a);
  uint32_t va = mtbddnode_getvariable(na);
  MTBDD high_history = mtbdd_refs_push(mtbdd_times(history, mtbdd_ithvar(va)));
  MTBDD low_history =
      mtbdd_refs_push(mtbdd_times(history, mtbdd_not(mtbdd_ithvar(va))));
  uint64_t order_id = SmtVariableOrder_get_id(order);

  uint64_t high_result;
  int spawned_high = 0;
  if (!cache_get3(kcache_SmtTerm_decisions, high_history, order_id, 0,
                  &high_result)) {
    SPAWN(sylvan_SmtTerm_decide, high_history, order);
    spawned_high = 1;
  }

  uint64_t low_result;
  if (!cache_get3(kcache_SmtTerm_decisions, low_history, order_id, 0,
                  &low_result)) {
    low_result = CALL(sylvan_SmtTerm_decide, low_history, order);
    cache_put3(kcache_SmtTerm_decisions, low_history, order_id, 0, low_result);
  }

  if (spawned_high) {
    high_result = SYNC(sylvan_SmtTerm_decide);
    cache_put3(kcache_SmtTerm_decisions, high_history, order_id, 0,
               high_result);
  }

  MTBDD rval = mtbdd_false;
  mtbdd_refs_pushptr(&rval);
  MTBDD alow = node_getlow(a, na);
  MTBDD ahigh = node_gethigh(a, na);
  MTBDD low, high;

  // We only care about if it is proveable UNSAT
  low_result = (low_result != SMTRESULT_UNSAT);
  high_result = (high_result != SMTRESULT_UNSAT);

  if (low_result || high_result) {
    if (low_result) {
      mtbdd_refs_spawn(
          SPAWN(sylvan_SmtTerm_op_prune_full, alow, low_history, order));
    }

    if (high_result) {
      high = mtbdd_refs_push(
          CALL(sylvan_SmtTerm_op_prune_full, ahigh, high_history, order));
    }

    if (low_result) {
      low =
          mtbdd_refs_push(mtbdd_refs_sync(SYNC(sylvan_SmtTerm_op_prune_full)));
    }

    // fprintf(stderr, "What %lu %lu\n", low_result, high_result);

    if (!low_result) {
      rval = high;
      mtbdd_refs_pop(1);
    } else if (!high_result) {
      rval = low;
      mtbdd_refs_pop(1);
    } else {
      rval = mtbdd_makenode(va, low, high);
      mtbdd_refs_pop(2);
    }
  }

  // histories
  mtbdd_refs_pop(2);

  // rval
  mtbdd_refs_popptr(1);

  return rval;
}

MTBDD sylvan_SmtTerm_prune_full(MTBDD a, SmtVariableOrderHandle order) {
  return RUN(sylvan_SmtTerm_op_prune_full, a, mtbdd_true, order);
}

TASK_5(int, sylvan_SmtTerm_op_compare, MTBDD, a, MTBDD, b, MTBDD, history,
       ComparisonType, comp, SmtVariableOrderHandle, order) {
  uint64_t order_id = SmtVariableOrder_get_id(order);

  uint64_t result;
  if (cache_get6(kcache_SmtTerm_compare, a, b, history, order_id, comp, &result,
                 NULL)) {
    return result;
  }

  int la = mtbdd_isleaf(a);
  int lb = mtbdd_isleaf(b);

  if (la && lb) {
    if (mtbdd_gettype(a) == kSmtTerm_type &&
        mtbdd_gettype(b) == kSmtTerm_type) {
      SmtTermHandle ah = (SmtTermHandle)mtbdd_getvalue(a);
      SmtTermHandle bh = (SmtTermHandle)mtbdd_getvalue(b);

      result = SmtVariableOrder_compare(order, ah, bh, history, comp);
    } else {
      result = 1;
    }
  } else {
    mtbddnode_t na, nb;
    uint32_t va, vb;
    if (!la) {
      na = MTBDD_GETNODE(a);
      va = mtbddnode_getvariable(na);
    } else {
      na = 0;
      va = 0xffffffff;
    }
    if (!lb) {
      nb = MTBDD_GETNODE(b);
      vb = mtbddnode_getvariable(nb);
    } else {
      nb = 0;
      vb = 0xffffffff;
    }
    uint32_t v = va < vb ? va : vb;

    MTBDD high_history = mtbdd_refs_push(mtbdd_times(history, mtbdd_ithvar(v)));
    MTBDD low_history =
        mtbdd_refs_push(mtbdd_times(history, mtbdd_not(mtbdd_ithvar(v))));

    /* Get cofactors */
    MTBDD alow, ahigh, blow, bhigh;
    if (!la && va == v) {
      alow = node_getlow(a, na);
      ahigh = node_gethigh(a, na);
    } else {
      alow = a;
      ahigh = a;
    }
    if (!lb && vb == v) {
      blow = node_getlow(b, nb);
      bhigh = node_gethigh(b, nb);
    } else {
      blow = b;
      bhigh = b;
    }

    SPAWN(sylvan_SmtTerm_op_compare, ahigh, bhigh, high_history, comp, order);
    int low =
        CALL(sylvan_SmtTerm_op_compare, alow, blow, low_history, comp, order);
    int high = 0;
    high = SYNC(sylvan_SmtTerm_op_compare);
    mtbdd_refs_pop(2);

    result = low && high;
  }

  /* Store in cache */
  cache_put6(kcache_SmtTerm_compare, a, b, history,
             SmtVariableOrder_get_id(order), comp, result, 0);

  return result;
}

int sylvan_SmtTerm_compare(MTBDD a, MTBDD b, ComparisonType comp,
                           SmtVariableOrderHandle order) {
  // normalize for caching
  if (comp == COMPARISONTYPE_GREATER_THAN) {
    MTBDD tmp = a;
    a = b;
    b = tmp;
  }
  int negate = 0;
  switch (comp) {
    case COMPARISONTYPE_INEQUALITY:
      negate = 1;
      comp = COMPARISONTYPE_EQUALITY;
      break;
    case COMPARISONTYPE_GREATER_THAN:
      negate = 1;
      comp = COMPARISONTYPE_LESS_THAN_OR_EQUAL;
      break;
    case COMPARISONTYPE_GREATER_THAN_OR_EQUAL:
      negate = 1;
      comp = COMPARISONTYPE_LESS_THAN;
      break;
    default:
      break;
  }

  const int result =
      RUN(sylvan_SmtTerm_op_compare, a, b, mtbdd_true, comp, order);

  return negate ? !result : result;
}

VOID_TASK_4(sylvan_SmtTerm_op_minmax_gather, MTBDD, a, MTBDD, b, MTBDD, history,
            SmtMinMaxHelperHandle, handle) {
  // is commutative so swap if needed
  if (a < b) {
    MTBDD tmp = a;
    a = b;
    b = tmp;
  }

  int la = mtbdd_isleaf(a);
  int lb = mtbdd_isleaf(b);

  uint64_t minmax_helper_id = SmtMinMaxHelper_get_id(handle);

#ifndef NO_MINMAX_GATHER_CACHE
  uint64_t result = 0;
  if (cache_get6(kcache_SmtTerm_minmax_gather, a, b, history, minmax_helper_id,
                 0, &result, NULL)) {
    // fprintf(stderr, "Found!\n");
    // mtbdd_fprintdot(stderr, history);
    return;
  }
#endif

  if (la && lb) {
    if (mtbdd_gettype(a) == kSmtTerm_type &&
        mtbdd_gettype(b) == kSmtTerm_type) {
      if (!!(CALL(sylvan_SmtTerm_op_compare, a, b, history,
                  COMPARISONTYPE_LESS_THAN,
                  SmtMinMaxHelper_get_order(handle))) ==
          !!(CALL(sylvan_SmtTerm_op_compare, b, a, history,
                  COMPARISONTYPE_LESS_THAN,
                  SmtMinMaxHelper_get_order(handle)))) {
        SmtTermHandle ah = (SmtTermHandle)mtbdd_getvalue(a);
        SmtTermHandle bh = (SmtTermHandle)mtbdd_getvalue(b);

        SmtMinMaxHelper_add_pair(handle, ah, bh);
      }
    }
  } else {
    mtbddnode_t na, nb;
    uint32_t va, vb;
    if (!la) {
      na = MTBDD_GETNODE(a);
      va = mtbddnode_getvariable(na);
    } else {
      na = 0;
      va = 0xffffffff;
    }
    if (!lb) {
      nb = MTBDD_GETNODE(b);
      vb = mtbddnode_getvariable(nb);
    } else {
      nb = 0;
      vb = 0xffffffff;
    }
    uint32_t v = va < vb ? va : vb;

    /* Get cofactors */
    MTBDD alow, ahigh, blow, bhigh;
    if (!la && va == v) {
      alow = node_getlow(a, na);
      ahigh = node_gethigh(a, na);
    } else {
      alow = a;
      ahigh = a;
    }
    if (!lb && vb == v) {
      blow = node_getlow(b, nb);
      bhigh = node_gethigh(b, nb);
    } else {
      blow = b;
      bhigh = b;
    }

    MTBDD high_history = mtbdd_refs_push(mtbdd_times(history, mtbdd_ithvar(v)));
    MTBDD low_history =
        mtbdd_refs_push(mtbdd_times(history, mtbdd_not(mtbdd_ithvar(v))));

    SPAWN(sylvan_SmtTerm_op_minmax_gather, ahigh, bhigh, high_history, handle);
    CALL(sylvan_SmtTerm_op_minmax_gather, alow, blow, low_history, handle);
    SYNC(sylvan_SmtTerm_op_minmax_gather);

    mtbdd_refs_pop(2);
  }

#ifndef NO_MINMAX_GATHER_CACHE
  result = 1;
  cache_put6(kcache_SmtTerm_minmax_gather, a, b, history, minmax_helper_id, 0,
             result, 0);
#endif
}

TASK_5(MTBDD, sylvan_SmtTerm_op_minmax_transform, MTBDD, a, MTBDD, b, MTBDD,
       history, SmtMinMaxHelperHandle, handle, int, min) {
  // is commutative so swap if needed
  if (a < b) {
    MTBDD tmp = a;
    a = b;
    b = tmp;
  }

  MTBDD result;

#ifndef NO_MINMAX_TRANSFORM_CACHE
  // TODO(DB) investigate if min is needed here for the cache
  if (cache_get3(kcache_SmtTerm_minmax_transform, a, b,
                 SmtMinMaxHelper_get_id(handle) | !!min, &result)) {
    return result;
  }
#endif

  int la = mtbdd_isleaf(a);
  int lb = mtbdd_isleaf(b);

  if (la && lb) {
    if (mtbdd_gettype(a) == kSmtTerm_type &&
        mtbdd_gettype(b) == kSmtTerm_type) {
      int a_less =
          CALL(sylvan_SmtTerm_op_compare, a, b, history,
               COMPARISONTYPE_LESS_THAN, SmtMinMaxHelper_get_order(handle));
      int a_greater =
          CALL(sylvan_SmtTerm_op_compare, b, a, history,
               COMPARISONTYPE_LESS_THAN, SmtMinMaxHelper_get_order(handle));

      if (a_less && !a_greater) {
        result = min ? a : b;
      } else if (a_greater && !a_less) {
        result = min ? b : a;
      } else {
        SmtTermHandle ah = (SmtTermHandle)mtbdd_getvalue(a);
        SmtTermHandle bh = (SmtTermHandle)mtbdd_getvalue(b);
        uint32_t var = SmtMinMaxHelper_get_var(handle, ah, bh);

        result = min ? mtbdd_makenode(var, b, a) : mtbdd_makenode(var, a, b);
      }
    } else {
      result = mtbdd_gettype(a) == kSmtTerm_type ? a : b;
    }
  } else {
    mtbddnode_t na, nb;
    uint32_t va, vb;
    if (!la) {
      na = MTBDD_GETNODE(a);
      va = mtbddnode_getvariable(na);
    } else {
      na = 0;
      va = 0xffffffff;
    }
    if (!lb) {
      nb = MTBDD_GETNODE(b);
      vb = mtbddnode_getvariable(nb);
    } else {
      nb = 0;
      vb = 0xffffffff;
    }
    uint32_t v = va < vb ? va : vb;

    /* Get cofactors */
    MTBDD alow, ahigh, blow, bhigh;
    if (!la && va == v) {
      alow = node_getlow(a, na);
      ahigh = node_gethigh(a, na);
    } else {
      alow = a;
      ahigh = a;
    }
    if (!lb && vb == v) {
      blow = node_getlow(b, nb);
      bhigh = node_gethigh(b, nb);
    } else {
      blow = b;
      bhigh = b;
    }

    MTBDD high_history = mtbdd_refs_push(mtbdd_times(history, mtbdd_ithvar(v)));
    MTBDD low_history =
        mtbdd_refs_push(mtbdd_times(history, mtbdd_not(mtbdd_ithvar(v))));

    mtbdd_refs_spawn(SPAWN(sylvan_SmtTerm_op_minmax_transform, ahigh, bhigh,
                           high_history, handle, min));
    MTBDD low = mtbdd_refs_push(CALL(sylvan_SmtTerm_op_minmax_transform, alow,
                                     blow, low_history, handle, min));
    MTBDD high = mtbdd_refs_push(
        mtbdd_refs_sync(SYNC(sylvan_SmtTerm_op_minmax_transform)));
    MTBDD v_dd = mtbdd_refs_push(mtbdd_ithvar(v));
    result = CALL(mtbdd_ite, v_dd, high, low);
    mtbdd_refs_pop(5);
  }

#ifndef NO_MINMAX_TRANSFORM_CACHE
  cache_put3(kcache_SmtTerm_minmax_transform, a, b,
             SmtMinMaxHelper_get_id(handle) & !!min, result);
#endif

  return result;
}

MTBDD sylvan_SmtTerm_max(MTBDD lhs, MTBDD rhs, SmtMinMaxHelperHandle handle) {
  RUN(sylvan_SmtTerm_op_minmax_gather, lhs, rhs, mtbdd_true, handle);
  SmtMinMaxHelper_finalize(handle);
  return RUN(sylvan_SmtTerm_op_minmax_transform, lhs, rhs, mtbdd_true, handle,
             0);
}

MTBDD sylvan_SmtTerm_min(MTBDD lhs, MTBDD rhs, SmtMinMaxHelperHandle handle) {
  RUN(sylvan_SmtTerm_op_minmax_gather, lhs, rhs, mtbdd_true, handle);
  SmtMinMaxHelper_finalize(handle);
  return RUN(sylvan_SmtTerm_op_minmax_transform, lhs, rhs, mtbdd_true, handle,
             1);
}
