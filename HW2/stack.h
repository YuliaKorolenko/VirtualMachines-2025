
#include "runtime_common.h"

#define STACK_SIZE 1000000

typedef enum {
    VAL, // unboxed integer value
    POINTER, // boxed pointer to heap object
    UNKNOWN
} ValueType;

aint *SP_ptr(void);

aint get_ebp_index(void);

void set_ebp_index(aint value);

size_t stack_top_index(void);

void set_stack_top_index(size_t idx);

void gc_stack_offset(int count);

void operand_push(aint value, const ValueType type);

aint operand_top(const ValueType type);

void operand_pop(void);

aint operand_get(const size_t k, const ValueType type);

void operand_set(const size_t k, aint value, const ValueType type);

void stack_init(int bf_global_area_size);
