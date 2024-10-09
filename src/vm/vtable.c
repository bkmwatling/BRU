#include "vtable.h"

typedef void (*fptr)();

// Below macro checks if the function pointer is NULL or not
// by casting the offset into the byte array to a _pointer_ to a function
// then dereferencing this to get the function address itself.
#define _bru_found_func(vt, func_offset)   \
    ((vt)->i < stc_vec_len((vt)->table) && \
     *((fptr *) ((vt)->table[(vt)->i] + func_offset)))

// VTable of byte arrays
typedef BruVTable_of(bru_byte_t) _bru_vtable;

void _bru_vt_lookup_from(void *vt, size_t offset, size_t __i)
{
    _bru_vtable *_vt = vt;
    // bru_byte_t  *instance;

    for (_vt->i = __i; !_bru_found_func(_vt, offset); _vt->i--);

    // _vt->i = __i;
    // while (_vt->i < stc_vec_len(_vt->table)) {
    //     instance = _vt->table[_vt->i];
    //     if (*((fptr *) (instance + offset)) != NULL) return;
    //     _vt->i--;
    // }
}
