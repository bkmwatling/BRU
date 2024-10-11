#ifndef BRU_VM_VTABLE_H
#define BRU_VM_VTABLE_H

/**
 * VTable used for implementing simple linear inheritance of interfaces. Mainly
 * used by thread_manager to allow extending functionality.
 *
 * The VTable is implemented as an array of tables, where each table is a
 * user defined struct of functions (an interface to a class) and the macro
 * VTABLE_FIELDS.
 *
 * The type of the VTable is user-defined, and can be done using the provided
 * BruVTable macro, e.g.
 *
 *      typedef BruVTable_of(BruSomeObjectInterface) BruSomeObject;
 *
 * Then each function defined in BruSomeObjectInterface must take as first
 * argument something of type BruSomeObject *. This is the same type as the
 * VTable, but can be seen as similar to passing in 'self' in Python, or the
 * implicit 'this' in Java.
 *
 * If you want access to the implementing struct, use `bru_vt_impl(vtable)`,
 * which will provide the implementation of the current interface.
 *
 * Invariants:
 *   [Interface Implementation] A lookup always finds a function to call.
 *   [Current class] The current instance is found at vtable->table[vtable->i].
 *
 * To satisfy the first invariant, the base instance in the vtable
 * must have some implementation for every function (no NULL function
 * pointers). This is on the user to guarantee.
 *
 * The second invariant is satisfied by using the provided macros.
 */

#include "../stc/common.h"
#include "../stc/fatp/vec.h"
#include "../types.h"

// place this within your interface struct (used by BruVTable_of)
#define VTABLE_FIELDS \
    void  *__vt_impl; \
    size_t __vt_idx
#define BruVTable_of(interface_type)                                 \
    struct {                                                         \
        size_t           i;          /**< current interface index */ \
        size_t          *call_stack; /**< previous values of 'i'  */ \
        interface_type **table;      /**< list of interfaces      */ \
    }

#define bru_vt_save_curr_idx(vt)    (stc_vec_push_back((vt)->call_stack, (vt)->i))
#define bru_vt_restore_curr_idx(vt) ((vt)->i = stc_vec_pop((vt)->call_stack))
#define bru_vt_curr(vt)             ((vt)->table[(vt)->i])
#define bru_vt_curr_idx(vt)         (bru_vt_curr(vt)->__vt_idx)
#define bru_vt_curr_impl(vt)        (bru_vt_curr(vt)->__vt_impl)
#define bru_vt_super_idx(instance)  ((instance)->__vt_idx - 1)
#define bru_vt_super(vt, instance)  ((vt)->table[bru_vt_super_idx(instance)])
#define bru_vt_leaf_idx(vt)         (stc_vec_len((vt)->table) - 1)

#define bru_vt_extend(vt, instance)                      \
    do {                                                 \
        (instance)->__vt_idx = stc_vec_len((vt)->table); \
        stc_vec_push_back((vt)->table, instance);        \
        (vt)->i = bru_vt_leaf_idx(vt);                   \
    } while (0)
#define bru_vt_shrink(vt)              \
    do {                               \
        stc_vec_pop((vt)->table);      \
        (vt)->i = bru_vt_leaf_idx(vt); \
    } while (0)

#define bru_vt_init(vt, base)                   \
    do {                                        \
        (vt)->i = 0;                            \
        stc_vec_default_init((vt)->call_stack); \
        stc_vec_default_init((vt)->table);      \
        bru_vt_extend(vt, base);                \
    } while (0)
#define bru_vt_release(vt)              \
    do {                                \
        stc_vec_free((vt)->call_stack); \
        stc_vec_free((vt)->table);      \
        free(vt);                       \
    } while (0)

// offset computation is performed at runtime now, since the type is not
// provided. Enums could maybe be used instead?
#define bru_vt_lookup_from(vt, func, __i)                                    \
    _bru_vt_lookup_from(((void *) (vt)),                                     \
                        (size_t) (((bru_byte_t *) &((vt)->table[0]->func)) - \
                                  ((bru_byte_t *) (vt)->table[0])),          \
                        (__i))
#define bru_vt_lookup(vt, func) \
    bru_vt_lookup_from(vt, func, bru_vt_leaf_idx(vt))

#define bru_vt_call_unsafe(vt, func, ...) \
    ((vt)->table[(vt)->i]->func(vt, ##__VA_ARGS__))
#define bru_vt_call_procedure_from(vt, proc, __i, ...) \
    do {                                               \
        bru_vt_save_curr_idx(vt);                      \
        bru_vt_lookup_from(vt, proc, __i);             \
        bru_vt_call_unsafe(vt, proc, ##__VA_ARGS__);   \
        bru_vt_restore_curr_idx(vt);                   \
    } while (0)
#define bru_vt_call_function_from(vt, retvar, func, __i, ...)     \
    (bru_vt_save_curr_idx(vt), bru_vt_lookup_from(vt, func, __i), \
     (retvar) = bru_vt_call_unsafe(vt, func, ##__VA_ARGS__),      \
     bru_vt_restore_curr_idx(vt), (retvar))
#define bru_vt_call_procedure(vt, proc, ...) \
    bru_vt_call_procedure_from(vt, proc, bru_vt_leaf_idx(vt), ##__VA_ARGS__)
#define bru_vt_call_function(vt, retvar, func, ...)                  \
    bru_vt_call_function_from(vt, retvar, func, bru_vt_leaf_idx(vt), \
                              ##__VA_ARGS__)
#define bru_vt_call_super_procedure(vt, instance, proc, ...)         \
    bru_vt_call_procedure_from(vt, proc, bru_vt_super_idx(instance), \
                               ##__VA_ARGS__)
#define bru_vt_call_super_function(vt, instance, retvar, func, ...)         \
    bru_vt_call_function_from(vt, retvar, func, bru_vt_super_idx(instance), \
                              ##__VA_ARGS__)

/**
 * Look for a non-null function pointer at ((vt)->table[__i])+offset.
 *
 * This function should never need to be called explicitly -- rather use the
 * bru_vt_lookup* macros defined above.
 *
 * @param[in] vt     the VTable
 * @param[in] offset the byte-offset into the interface struct of the
 *                   function to look up
 * @param[in] __i    the instance to start looking from
 */
void _bru_vt_lookup_from(void *vt, size_t offset, size_t __i);

#endif /* BRU_VM_VTABLE_H */
