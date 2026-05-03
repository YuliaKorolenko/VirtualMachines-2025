#include "stack.h"

#include <stdbool.h>

#include "runtime.h"

extern size_t __gc_stack_top, __gc_stack_bottom;

typedef struct {
    aint operand_stack[STACK_SIZE];
    aint ebp_index;
} OperandStack;

OperandStack g_stack = {.ebp_index = 0};

aint *SP_ptr(void) {
    return (aint *) ((char *) __gc_stack_top + sizeof(size_t));
}

aint get_ebp_index(void) {
    return g_stack.ebp_index;
}

void set_ebp_index(aint value) {
    g_stack.ebp_index = value;
}

size_t stack_top_index(void) {
    return (size_t) (SP_ptr() - &g_stack.operand_stack[0]);
}

void set_stack_top_index(size_t idx) {
    __gc_stack_top = (size_t) &(g_stack.operand_stack[idx]) - sizeof(size_t);
}

void gc_get_stack_top_checked(const size_t new_top) {
    if (new_top < (size_t) &(g_stack.operand_stack[0])) {
        failure("stack overflow\n");
    }
    if (new_top >= __gc_stack_bottom) {
        failure("stack underflow\n");
    }
}

void gc_stack_offset(int count) {
    __gc_stack_top += count * sizeof(aint);
}

void operand_push(aint value, const ValueType type) {
    gc_get_stack_top_checked(__gc_stack_top - sizeof(aint));
    if (type == VAL) {
        value = BOX(value);
    }
    gc_stack_offset(-1);
    *SP_ptr() = value;
}

aint operand_top(const ValueType type) {
    gc_get_stack_top_checked(__gc_stack_top);
    aint result = *SP_ptr();
    if (type == VAL) {
        result = UNBOX(result);
    }
    return result;
}

void operand_pop(void) {
    gc_get_stack_top_checked(__gc_stack_top);
    gc_stack_offset(1);
}

aint operand_get(const size_t k, const ValueType type) {
    if (k >= STACK_SIZE) {
        failure("operand stack underflow in get operation\n");
    }
    aint result = g_stack.operand_stack[k];
    const bool is_unboxes = UNBOXED(result);
    if (!is_unboxes && type == VAL) {
        failure("Expected VAL, but receives POINTER\n");
    }

    if (type == VAL) {
        result = UNBOX(result);
    }
    return result;
}

void operand_set(const size_t k, aint value, const ValueType type) {
    if (k >= STACK_SIZE) {
        failure("operand stack underflow in set operation\n");
    }
    if (type == VAL) {
        value = BOX(value);
    }
    g_stack.operand_stack[k] = value;
}

void stack_init(int bf_global_area_size) {
    // stack_top < stack_bottom
    __gc_stack_top= (size_t) &g_stack.operand_stack[0];
    __gc_stack_bottom = (size_t) &g_stack.operand_stack[STACK_SIZE];
    // Places reserved for global variables
    set_stack_top_index((size_t) (STACK_SIZE - bf_global_area_size));
    operand_push(-1, VAL);
    operand_push(-1, VAL);

    // closure for first begin
    operand_push(0, POINTER);
    // return address for first begin
    operand_push(0, POINTER);
}

