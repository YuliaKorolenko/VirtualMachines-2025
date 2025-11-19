/* Lama SM Bytecode interpreter */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "runtime/runtime.h"
#include "runtime/gc.h"


#define STACK_SIZE 20000

typedef struct {
    aint operand_stack[STACK_SIZE];
    aint stack_top_index; // points to next free slot
    aint ebp_index;
} OperandStack;

OperandStack g_stack = {.stack_top_index = STACK_SIZE, .ebp_index = 0};

typedef enum {
    VAL, // unboxed integer value
    POINTER, // boxed pointer to heap object
    UNKNOWN
} ValueType;

typedef enum OpGroup {
    OP_BINOP = 0,
    OP_MISC = 1,
    OP_LD = 2,
    OP_LDA = 3,
    OP_ST = 4,
    OP_CTRL = 5,
    OP_PATT = 6,
    OP_RT = 7,
    OP_END = 15
};

// (low nibble)
typedef enum LowOp {
    BINOP_INVALID = 0,
    BINOP_ADD = 1,
    BINOP_SUB = 2,
    BINOP_MUL = 3,
    BINOP_DIV = 4,
    BINOP_MOD = 5,
    BINOP_LT = 6,
    BINOP_LE = 7,
    BINOP_GT = 8,
    BINOP_GE = 9,
    BINOP_EQ = 10,
    BINOP_NE = 11,
    BINOP_AND = 12,
    BINOP_OR = 13,

    MI_CONST = 0,
    MI_STRING = 1,
    MI_SEXP = 2,
    MI_STI = 3,
    MI_STA = 4,
    MI_JMP = 5,
    MI_END = 6,
    MI_RET = 7,
    MI_DROP = 8,
    MI_DUP = 9,
    MI_SWAP = 10,
    MI_ELEM = 11,

    LDS_G = 0,
    LDS_L = 1,
    LDS_A = 2,
    LDS_C = 3,

    CTRL_CJMPZ = 0,
    CTRL_CJMPNZ = 1,
    CTRL_BEGIN = 2,
    CTRL_CBEGIN = 3,
    CTRL_CLOSURE = 4,
    CTRL_CALLC = 5,
    CTRL_CALL = 6,
    CTRL_TAG = 7,
    CTRL_ARRAY = 8,
    CTRL_FAIL = 9,
    CTRL_LINE = 10,

    PATT_STRING = 0,
    PATT_STRING_TAG = 1,
    PATT_ARRAY_TAG = 2,
    PATT_SEXP_TAG = 3,
    PATT_BOXED = 4,
    PATT_UNBOXED = 5,
    PATT_CLOSURE_TAG = 6,

    RT_READ = 0,
    RT_WRITE = 1,
    RT_LENGTH = 2,
    RT_STRING = 3,
    RT_BARRAY = 4
};

static void operand_push(aint value, const ValueType type) {
    if (g_stack.stack_top_index == 0) {
        failure("operand stack overflow\n");
    }

    if (type == VAL) {
        value = BOX(value);
    }
    g_stack.operand_stack[--g_stack.stack_top_index] = value;
    gc_set_stack_top((size_t) &(g_stack.operand_stack[g_stack.stack_top_index]));
}

static aint operand_top(const ValueType type) {
    if (g_stack.stack_top_index + 1 >= STACK_SIZE) {
        failure("operand stack underflow in top operation\n");
    }
    aint result = g_stack.operand_stack[g_stack.stack_top_index];
    if (type == VAL) {
        result = UNBOX(result);
    }
    return result;
}

static void operand_pop(void) {
    if (g_stack.stack_top_index + 1 >= STACK_SIZE) {
        failure("operand stack underflow in pop operation\n");
    }
    g_stack.stack_top_index++;
    gc_set_stack_top((size_t) &(g_stack.operand_stack[g_stack.stack_top_index]));
}


static aint operand_get(const size_t k, const ValueType type) {
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

static void operand_set(const size_t k, aint value, const ValueType type) {
    if (k >= STACK_SIZE) {
        failure("operand stack underflow in set operation\n");
    }
    if (type == VAL) {
        value = BOX(value);
    }
    g_stack.operand_stack[k] = value;
}

static void store_global(const size_t k) {
    if (k >= STACK_SIZE) {
        failure("global index out of bounds: %d (size=%d)\n", k, STACK_SIZE);
    }
    const aint v = operand_top(UNKNOWN);
    operand_set(STACK_SIZE - 1 - k, v, UNKNOWN);
}

static void load_global(const size_t k) {
    if (k >= STACK_SIZE) {
        failure("global index out of bounds: %d (size=%d)\n", k, STACK_SIZE);
    }
    const aint v = operand_get(STACK_SIZE - 1 - k, UNKNOWN);
    operand_push(v, UNKNOWN);
}

static aint get_closure_pointer() {
    aint closure_pointer = operand_get(g_stack.ebp_index + 2, POINTER);
    if (TAG(TO_DATA(closure_pointer)->data_header) != CLOSURE_TAG) {
        failure("Expected closure\n");
    }
    return closure_pointer;
}

static void load_closure(const size_t k) {
    const aint closure_pointer = get_closure_pointer();
    const data *closure_data = TO_DATA(closure_pointer);
    if (TAG(closure_data->data_header) != CLOSURE_TAG) {
        failure("Expected closure in store_closure\n");
    }
    const aint res = ((aint *) closure_data->contents)[k + 1];
    operand_push(res, UNKNOWN);
}


static void store_closure(const size_t k) {
    const aint closure_pointer = get_closure_pointer();
    const data *closure_data = TO_DATA(closure_pointer);
    if (TAG(closure_data->data_header) != CLOSURE_TAG) {
        failure("Expected closure in store_closure\n");
    }
    const aint len = LEN(closure_data->data_header);
    if (k >= len) {
        failure("closure index out of bounds: %zu (len=%zu)\n", k, len);
    }
    const aint v = operand_top(UNKNOWN);
    ((aint *) closure_data->contents)[k + 1] = v;
}

static size_t get_local_pos(const size_t k) {
    const aint local_count = operand_get(g_stack.ebp_index - 2, VAL);
    if (k >= local_count) {
        failure("local index out of bounds: %zu (count=%zu)\n", k, local_count);
    }
    const size_t local_position = g_stack.ebp_index - 3 - k;

    if (local_position >= STACK_SIZE) {
        failure("local position out of stack bounds: %zu\n", local_position);
    }
    return local_position;
}

static void load_local(const size_t k) {
    const size_t local_position = get_local_pos(k);
    const aint v = operand_get(local_position, UNKNOWN);
    operand_push(v, UNKNOWN);
}

static void store_local(const size_t k) {
    const size_t local_position = get_local_pos(k);
    const aint v = operand_top(UNKNOWN);
    operand_set(local_position, v, UNKNOWN);
}

static void load_arg(const size_t k) {
    const size_t arg_count = operand_get(g_stack.ebp_index + 1, VAL);
    if (k >= arg_count) {
        failure("argument index out of bounds: %zu (count=%zu)\n", k, arg_count);
    }
    const size_t arg_position = g_stack.ebp_index + 3 + k;
    const aint v = operand_get(arg_position, UNKNOWN);
    operand_push(v, UNKNOWN);
}

static void store_arg(const size_t k) {
    const aint arg_count = operand_get(g_stack.ebp_index + 1, VAL);
    if (k >= arg_count) {
        failure("argument index out of bounds: %zu (count=%zu)\n", k, arg_count);
    }
    const size_t arg_position = g_stack.ebp_index + 3 + k;
    const aint v = operand_top(UNKNOWN);
    operand_set(arg_position, v, UNKNOWN);
}

// Shema before:
// [top] = ret_ip
// [top - 1] = closure
// [top - 1] = args[1]
// [top - 2] = args[0]

void begin_function(const size_t num_args, const size_t local_size) {
    // Shema: the number of arguments, EBP, return address, local vars number, local vars
    //   [ebp + 2 ...] = arguments
    //   [ebp + 2] == closure/empty
    //   [ebp + 1] = number of arguments
    //   [ebp] = old ebp
    //   [ebp-1] = return address
    //   [ebp-2] = local_size
    //   [ebp-3 - i] = local i (0..local_size-1)

    const aint old_ebp = g_stack.ebp_index;

    const aint ret_ip = operand_top(POINTER);
    operand_pop();

    operand_push((aint) num_args, VAL);
    operand_push(old_ebp, VAL);
    g_stack.ebp_index = g_stack.stack_top_index;
    operand_push(ret_ip, POINTER);
    operand_push((aint) local_size, VAL);

    for (int i = 0; i < local_size; i++) {
        operand_push(-1, VAL);
    }
}

static aint end_function() {
    const aint stack_top = operand_top(UNKNOWN);
    const aint ebp = g_stack.ebp_index;
    const aint ret_ip = operand_get(g_stack.ebp_index - 1, POINTER); // return address saved by CALL
    const aint old_ebp = operand_get(ebp, VAL);
    const aint num_args = operand_get(ebp + 1, VAL);

    g_stack.stack_top_index = ebp + 3 + num_args;
    gc_set_stack_top((size_t) &(g_stack.operand_stack[g_stack.stack_top_index]));
    g_stack.ebp_index = old_ebp;
    operand_push(stack_top, UNKNOWN);

    return ret_ip;
}

static void reverse_last_el(const int el_count) {
    if (el_count <= 1) {
        return;
    }

    const size_t have = STACK_SIZE - g_stack.stack_top_index;
    if ((size_t) el_count > have) {
        failure("reverse_last_el: not enough operands: need=%d have=%zu\n", el_count, have);
    }
    aint *SP = &g_stack.operand_stack[g_stack.stack_top_index];
    for (int i = 0, j = el_count - 1; i < j; ++i, --j) {
        aint tmp = SP[i];
        SP[i] = SP[j];
        SP[j] = tmp;
    }
}

static void barray_function(const int n) {
    if (n < 0) {
        failure("Barray: invalid size %d\n", n);
    }

    reverse_last_el(n);
    aint *SP = &g_stack.operand_stack[g_stack.stack_top_index];

    const aint arr = (aint) Barray(SP, BOX(n));
    g_stack.stack_top_index += n;
    gc_set_stack_top((size_t) &(g_stack.operand_stack[g_stack.stack_top_index]));
    operand_push(arr, POINTER);
}

static void sexp_function(char *tag, const int elem_size) {
    const aint hash_tag = UNBOX(LtagHash(tag));
    operand_push(hash_tag, VAL);

    reverse_last_el(elem_size + 1);
    aint *SP = &g_stack.operand_stack[g_stack.stack_top_index];
    const aint result = (aint) Bsexp(SP, BOX(elem_size + 1));
    g_stack.stack_top_index += elem_size + 1;
    gc_set_stack_top((size_t) &(g_stack.operand_stack[g_stack.stack_top_index]));
    operand_push(result, POINTER);
}

static void closure_function(const int code_pointer, const int arg_number) {
    reverse_last_el(arg_number);
    operand_push(code_pointer, POINTER);
    aint *SP = &g_stack.operand_stack[g_stack.stack_top_index];
    aint *closure = Bclosure(SP, BOX(arg_number));

    g_stack.stack_top_index += arg_number + 1;
    gc_set_stack_top((size_t) &(g_stack.operand_stack[g_stack.stack_top_index]));
    operand_push((aint) closure, POINTER);
}


static aint callc_function(const int arg_number) {
    size_t closure_pos = g_stack.stack_top_index + arg_number;
    if (closure_pos >= STACK_SIZE)
        failure("CALLC: invalid stack layout\n");

    aint closure_val = operand_get(closure_pos, POINTER);
    data *closure_data = TO_DATA(closure_val);

    for (int i = arg_number - 1; i >= 0; --i) {
        operand_set(g_stack.stack_top_index + i + 1,
                    operand_get(g_stack.stack_top_index + i, UNKNOWN),
                    UNKNOWN);
    }
    operand_set(g_stack.stack_top_index, closure_val, POINTER);
    const aint code_pointer = ((aint *) closure_data->contents)[0];
    if (TAG(closure_data->data_header) != CLOSURE_TAG) {
        failure("Expected closure\n");
    }


    return code_pointer;
}

/* The unpacked representation of bytecode file */
typedef struct {
    char *entry_ptr;
    char *string_ptr; /* A pointer to the beginning of the string table */
    int *public_ptr; /* A pointer to the beginning of publics table    */
    char *code_ptr; /* A pointer to the bytecode itself               */
    char *code_end;
    int stringtab_size; /* The size (in bytes) of the string table        */
    int global_area_size; /* The size (in words) of global area             */
    int public_symbols_number; /* The number of public symbols                   */
    char buffer[0];
} bytefile;

/* Gets a string from a string table by an index */
char *get_string(const bytefile *f, int pos) {
    if (pos < 0 || pos > f->stringtab_size) {
        failure("Incorrect string index: %d (size=%d)\n", pos, f->stringtab_size);
    }
    return &f->string_ptr[pos];
}

static inline char get_byte(const bytefile *bf, char **ip) {
    if (*ip + 1 > bf->code_end) {
        failure("Instruction pointer %p out of bounds [%p, %p)",
                (void *) *ip + 1, (void *) bf->code_ptr, (void *) bf->code_end);
    }
    return *(*ip)++;
}


static inline int get_int(const bytefile *bf, char **ip) {
    if (*ip < bf->code_ptr || *ip + sizeof(int) > bf->code_end) {
        failure("Instruction pointer %p out of bounds [%p, %p)",
                (void *) *ip + sizeof(int), (void *) bf->code_ptr, (void *) bf->code_end);
    }
    *ip += sizeof(int);
    return *(int *) (*ip - sizeof(int));
}

/* Gets a name for a public symbol */
char *get_public_name(bytefile *f, int i) {
    return get_string(f, f->public_ptr[i * 2]);
}

/* Gets an offset for a publie symbol */
int get_public_offset(bytefile *f, int i) {
    return f->public_ptr[i * 2 + 1];
}

/* Reads a binary bytecode file by name and unpacks it */
bytefile *read_file(char *fname) {
    FILE *f = fopen(fname, "rb");
    bytefile *file;

    if (f == 0) {
        failure("%s\n", strerror(errno));
    }

    if (fseek(f, 0, SEEK_END) == -1) {
        failure("%s\n", strerror(errno));
    }

    const long size = ftell(f);

    const long MAX_SIZE = 2LL * 1024 * 1024 * 1024;
    if (size > MAX_SIZE)
        failure("Bytecode file too large: %ld", size);

    file = (bytefile *) malloc(sizeof(int) * 4 + size);

    if (file == 0) {
        failure("*** FAILURE: unable to allocate memory.\n");
    }

    rewind(f);

    if (size != fread(&file->stringtab_size, 1, size, f)) {
        failure("%s\n", strerror(errno));
    }

    fclose(f);

    if (file->public_symbols_number < 0) {
        failure("Incorrect bytecode file: negative public_symbols_number");
    }
    file->string_ptr = &file->buffer[file->public_symbols_number * 2 * sizeof(int)];
    file->public_ptr = (int *) file->buffer;
    if (file->stringtab_size < 0 || file->public_symbols_number * 2 * sizeof(int) + file->stringtab_size > size) {
        failure("Incorrect bytecode file: invalid string table size");
    }
    file->code_ptr = &file->string_ptr[file->stringtab_size];
    file->code_end = file->code_ptr + (size - (file->stringtab_size + file->public_symbols_number * 2 * sizeof(int)));

    return file;
}

/* Disassembles the bytecode pool */
void disassemble(FILE *f, bytefile *bf) {
    char *ip = bf->entry_ptr;
    static const char *const ops[] = {"+", "-", "*", "/", "%", "<", "<=", ">", ">=", "==", "!=", "&&", "!!"};
    static const char *const pats[] = {"=str", "#string", "#array", "#sexp", "#ref", "#val", "#fun"};
    static const char *const lds[] = {"LD", "LDA", "ST"};

#define INT get_int(bf, &ip)
#define BYTE get_byte(bf, &ip)
#define STRING get_string(bf, INT)
#define FAIL failure("ERROR: invalid opcode %d-%d\n", h, l)
    do {
        unsigned char x = BYTE,
                h = (x & 0xF0) >> 4,
                l = x & 0x0F;

        fprintf(f, "0x%.8x:\t", ip - bf->code_ptr - 1);

        switch (h) {
            case OP_END:
                goto stop;

            /* BINOP */
            case OP_BINOP:
                fprintf(f, "BINOP\t%s", (l >= BINOP_ADD && l <= BINOP_OR) ? ops[l - 1] : "<invalid>"); {
                    if (l < BINOP_ADD || l > BINOP_OR) { FAIL; }
                    aint right = operand_top(VAL);
                    operand_pop();
                    aint left = operand_top(VAL);
                    operand_pop();

                    aint result = 0;
                    switch (l) {
                        case BINOP_ADD:
                            result = left + right;
                            break;
                        case BINOP_SUB:
                            result = left - right;
                            break;
                        case BINOP_MUL:
                            result = left * right;
                            break;
                        case BINOP_DIV:
                            if (right == 0) {
                                failure("ERROR at 0x%.8x, division by zero (%d/%d)", ip - bf->code_ptr - 1, left,
                                        right);
                            }
                            result = left / right;
                            break;
                        case BINOP_MOD:
                            if (right == 0) {
                                failure("ERROR at 0x%.8x, division by zero (mod) (%d/%d)", ip - bf->code_ptr - 1, left,
                                        right);
                            }
                            result = left % right;
                            break;
                        case BINOP_LT:
                            result = left < right ? 1 : 0;
                            break;
                        case BINOP_LE:
                            result = left <= right ? 1 : 0;
                            break;
                        case BINOP_GT:
                            result = left > right ? 1 : 0;
                            break;
                        case BINOP_GE:
                            result = left >= right ? 1 : 0;
                            break;
                        case BINOP_EQ:
                            result = left == right ? 1 : 0;
                            break;
                        case BINOP_NE:
                            result = left != right ? 1 : 0;
                            break;
                        case BINOP_AND:
                            result = left && right ? 1 : 0;
                            break;
                        case BINOP_OR:
                            result = left || right ? 1 : 0;
                            break;
                        default:
                            FAIL;
                    }

                    operand_push(result, VAL);
                }
                break;

            case OP_MISC:
                switch (l) {
                    case MI_CONST: {
                        int cnst = INT;
                        fprintf(f, "CONST\t%d", cnst);
                        operand_push(cnst, VAL);
                        break;
                    }

                    case MI_STRING: {
                        const char *s = STRING;
                        fprintf(f, "STRING\t%s", s);
                        aint res = (aint) Bstring((aint *) &s);
                        operand_push(res, POINTER);
                        break;
                    }

                    case MI_SEXP: {
                        char *tag = STRING;
                        int elem_size = INT;
                        fprintf(f, "SEXP\t%s ", tag);
                        fprintf(f, "%d", elem_size);
                        sexp_function(tag, elem_size);
                        break;
                    }

                    case MI_STI:
                        fprintf(f, "STI");
                        break;

                    case MI_STA: {
                        fprintf(f, "STA");
                        const aint value = operand_top(UNKNOWN);
                        operand_pop();
                        const aint ind = operand_top(UNKNOWN);
                        operand_pop();
                        const aint arr = operand_top(POINTER);
                        operand_pop();
                        aint res = (aint) Bsta((void *) arr, ind, (void *) value);
                        operand_push(res, VAL);
                        break;
                    }

                    case MI_JMP: {
                        aint jump_address = INT;
                        fprintf(f, "JMP\t0x%.8x", jump_address);
                        if (bf->code_ptr + jump_address > bf->code_end) {
                            failure("JMP target out of range: 0x%x\n", (unsigned) jump_address);
                        }
                        ip = bf->code_ptr + jump_address;
                        break;
                    }

                    case MI_END: {
                        fprintf(f, "END\t");
                        aint return_address = end_function();
                        if (return_address == 0) {
                            goto stop;
                        }
                        if ((char *) return_address < bf->code_ptr || (char *) return_address > bf->code_end) {
                            failure("END return address out of range\n");
                        }
                        ip = (char *) return_address;
                        break;
                    }

                    case MI_RET:
                        fprintf(f, "RET");
                        break;

                    case MI_DROP:
                        fprintf(f, "DROP");
                        operand_pop();
                        break;

                    case MI_DUP: {
                        fprintf(f, "DUP");
                        operand_push(operand_top(UNKNOWN), UNKNOWN);
                        break;
                    }

                    case MI_SWAP: {
                        fprintf(f, "SWAP");
                        aint a = operand_top(UNKNOWN);
                        operand_pop();
                        aint b = operand_top(UNKNOWN);
                        operand_pop();
                        operand_push(a, UNKNOWN);
                        operand_push(b, UNKNOWN);
                        break;
                    }

                    case MI_ELEM: {
                        fprintf(f, "ELEM");
                        aint b = operand_top(VAL);
                        operand_pop();
                        aint a = operand_top(POINTER); // container
                        operand_pop();

                        void *res = Belem((void *) a, BOX(b));
                        operand_push((aint) res, UNKNOWN);
                        break;
                    }

                    default:
                        FAIL;
                }
                break;

            case OP_LD:
            case OP_LDA:
            case OP_ST:
                fprintf(f, "%s\t", lds[h - 2]);
                switch (l) {
                    case LDS_G: {
                        int pos = INT;
                        fprintf(f, "G(%d)", pos);
                        if (h == OP_ST) {
                            store_global(pos);
                        } else if (h == OP_LD) {
                            load_global(pos);
                        } else {
                            failure("G is not supported");
                        }
                    }
                    break;
                    case LDS_L: {
                        int l_number = INT;
                        if (h == OP_ST) {
                            store_local(l_number);
                        } else if (h == OP_LD) {
                            load_local(l_number);
                        } else {
                            failure("L is not supported");
                        }
                        fprintf(f, "L(%d)", l_number);
                        break;
                    }
                    case LDS_A: {
                        int arg_number = INT;
                        fprintf(f, "A(%d)", arg_number);
                        if (h == OP_ST) {
                            store_arg(arg_number);
                        } else if (h == OP_LD) {
                            load_arg(arg_number);
                        } else {
                            failure("A is not supported");
                        }
                        break;
                    }
                    case LDS_C: {
                        int num_args = INT;
                        fprintf(f, "C(%d)", num_args);
                        if (h == OP_ST) {
                            store_closure(num_args);
                        } else if (h == OP_LD) {
                            load_closure(num_args);
                        } else {
                            failure("C is not supported");
                        }
                        break;
                    }
                    default:
                        FAIL;
                }
                break;

            case OP_CTRL:
                switch (l) {
                    case CTRL_CJMPZ: {
                        int target = INT;
                        fprintf(f, "CJMPz\t0x%.8x", target);
                        aint cond = operand_top(VAL);
                        operand_pop();
                        if (cond == 0) {
                            if (target < 0 || bf->code_ptr + target > bf->code_end) {
                                failure("CJMPz target out of range: 0x%x\n", (unsigned) target);
                            }
                            ip = bf->code_ptr + target;
                        }
                        break;
                    }

                    case CTRL_CJMPNZ: {
                        int target = INT;
                        fprintf(f, "CJMPnz\t0x%.8x", target);
                        aint cond = operand_top(VAL);
                        operand_pop();
                        if (cond != 0) {
                            if (target < 0 || bf->code_ptr + target > bf->code_end) {
                                failure("CJMPnz target out of range: 0x%x\n", (unsigned) target);
                            }
                            ip = bf->code_ptr + target;
                        }
                        break;
                    }

                    case CTRL_CBEGIN:
                    case CTRL_BEGIN: {
                        int num_args = INT;
                        int local_size = INT;
                        fprintf(f, "BEGIN\t%d ", num_args); // + CBEGIN
                        fprintf(f, "%d", local_size);
                        begin_function(num_args, local_size);
                        break;
                    }

                    case CTRL_CLOSURE: {
                        int code_pntr = INT;
                        int n = INT;
                        fprintf(f, "CLOSURE\t0x%.8x ", code_pntr);
                        for (int i = 0; i < n; i++) {
                            switch (BYTE) {
                                case LDS_G: {
                                    int pos = INT;
                                    fprintf(f, "G(%d)", pos);
                                    load_global(pos);
                                    break;
                                }
                                case LDS_L: {
                                    int pos = INT;
                                    fprintf(f, "L(%d)", pos);
                                    load_local(pos);
                                    break;
                                }
                                case LDS_A: {
                                    int pos = INT;
                                    fprintf(f, "A(%d)", pos);
                                    load_arg(pos);
                                    break;
                                }
                                case LDS_C: {
                                    int pos = INT;
                                    fprintf(f, "C(%d)", pos);
                                    load_closure(pos);
                                    break;
                                }
                                default:
                                    FAIL;
                            }
                        }
                        closure_function(code_pntr, n);
                        break;
                    }

                    case CTRL_CALLC: {
                        int arg_number = INT;
                        fprintf(f, "CALLC\t%d", arg_number);
                        aint offset = callc_function(arg_number);
                        operand_push((aint) ip, POINTER);
                        ip = bf->code_ptr + offset;
                        break;
                    }

                    case CTRL_CALL: {
                        int call_pos = INT;
                        int number_of_args = INT;
                        fprintf(f, "CALL\t0x%.8x ", call_pos);
                        fprintf(f, "%d", number_of_args);
                        reverse_last_el(number_of_args);
                        // fprintf(f, "IP in call\t0x%.8x ", ip - bf->code_ptr);
                        operand_push(0, VAL);
                        operand_push((aint) ip, POINTER);
                        ip = bf->code_ptr + call_pos;
                        break;
                    }

                    case CTRL_TAG: {
                        char *tag = STRING;
                        int elem_size = INT;
                        fprintf(f, "TAG\t%s ", tag);
                        fprintf(f, "%d", elem_size);
                        aint sexp = operand_top(POINTER);
                        operand_pop();
                        aint res = Btag((void *) sexp, LtagHash(tag), BOX(elem_size));
                        res = UNBOX(res);
                        operand_push(res, VAL);
                        break;
                    }

                    case CTRL_ARRAY: {
                        int el_size = INT;
                        fprintf(f, "ARRAY\t%d", el_size);

                        aint arr = operand_top(POINTER);
                        operand_push(UNBOX(Barray_patt((void *) arr, BOX(el_size))), VAL);
                        break;
                    }

                    case CTRL_FAIL: {
                        fprintf(f, "FAIL\t%d", INT);
                        fprintf(f, "%d", INT);
                        failure("FAIL");
                        break;
                    }

                    case CTRL_LINE:
                        fprintf(f, "LINE\t%d", INT);
                        break;

                    default:
                        FAIL;
                }
                break;

            case OP_PATT: {
                fprintf(f, "PATT\t%s", pats[l]);
                switch (l) {
                    case PATT_STRING: {
                        aint str = operand_top(POINTER);
                        operand_pop();
                        aint y = operand_top(POINTER);
                        operand_pop();
                        operand_push(UNBOX(Bstring_patt((void *)str, (void *)y)), VAL);
                        break;
                    }
                    case PATT_STRING_TAG: {
                        aint el = operand_top(POINTER);
                        operand_pop();
                        operand_push(UNBOX(Bstring_tag_patt((void *)el)), VAL);
                        break;
                    }
                    case PATT_ARRAY_TAG: {
                        aint el = operand_top(POINTER);
                        operand_pop();
                        operand_push(UNBOX(Barray_tag_patt((void *)el)), VAL);
                        break;
                    }
                    case PATT_SEXP_TAG: {
                        aint el = operand_top(UNKNOWN);
                        operand_pop();
                        operand_push(UNBOX(Bsexp_tag_patt((void *)el)), VAL);
                        break;
                    }
                    case PATT_BOXED: {
                        aint el = operand_top(UNKNOWN);
                        operand_pop();
                        operand_push(UNBOX(Bboxed_patt((void *)el)), VAL);
                        break;
                    }
                    case PATT_UNBOXED: {
                        aint el = operand_top(UNKNOWN);
                        operand_pop();
                        aint res = UNBOX(Bunboxed_patt((void *)el));
                        operand_push(res, VAL);
                        break;
                    }
                    case PATT_CLOSURE_TAG: {
                        aint el = operand_top(POINTER);
                        operand_pop();
                        aint res = UNBOX(Bclosure_tag_patt((void *) el));
                        operand_push(res, VAL);
                        break;
                    }
                    default:
                        failure("No patt for: %d", l);
                }
                break;
            }

            case OP_RT: {
                switch (l) {
                    case RT_READ:
                        fprintf(f, "CALL\tLread");
                        aint in = Lread();
                        operand_push(UNBOX(in), VAL);
                        break;

                    case RT_WRITE: {
                        fprintf(f, "CALL\tLwrite");
                        aint out = operand_top(VAL);
                        Lwrite(BOX(out));
                        break;
                    }

                    case RT_LENGTH:
                        fprintf(f, "CALL\tLlength");
                        aint out = operand_top(POINTER);
                        operand_pop();
                        aint res = Llength((void *) out);
                        operand_push(res, UNKNOWN);
                        break;

                    case RT_STRING:
                        fprintf(f, "CALL\tLstring");
                        aint *SP = &g_stack.operand_stack[g_stack.stack_top_index];
                        operand_push((aint) Lstring(SP), POINTER);
                        break;

                    case RT_BARRAY: {
                        int size = INT;
                        fprintf(f, "CALL\tBarray\t%d", size);
                        barray_function(size);
                        break;
                    }

                    default:
                        FAIL;
                }
            }
            break;

            default:
                FAIL;
        }

        fprintf(f, "\n");
    } while (1);
stop:
    fprintf(f, "<end>\n");
}

/* Dumps the contents of the file */
void dump_file(FILE *f, bytefile *bf) {
    int i;

    fprintf(f, "String table size       : %d\n", bf->stringtab_size);
    fprintf(f, "Global area size        : %d\n", bf->global_area_size);
    // Places reserved for global variables
    g_stack.stack_top_index = STACK_SIZE - bf->global_area_size;
    gc_set_stack_top((size_t) &(g_stack.operand_stack[g_stack.stack_top_index]));
    operand_push(-1, VAL);
    operand_push(-1, VAL);
    fprintf(f, "Number of public symbols: %d\n", bf->public_symbols_number);
    fprintf(f, "Public symbols          :\n");

    for (i = 0; i < bf->public_symbols_number; i++) {
        const char* public_name = get_public_name(bf, i);
        const int offset = get_public_offset(bf, i);
        if (strcmp(public_name, "main") == 0) {
            bf->entry_ptr = bf->code_ptr + offset;
        }
        fprintf(f, "   0x%.8x: %s\n", offset, public_name);
    }

    fprintf(f, "Code:\n");

    // closure for first begin
    operand_push(0, POINTER);
    // return address for first begin
    operand_push(0, POINTER);
    disassemble(f, bf);
}


FILE *old_stderr;

void disable_stderr() {
    old_stderr = stderr;
    stderr = fopen("/dev/null", "w");
}

int main(int argc, char *argv[]) {
    // stack_top < stack_bottom
    disable_stderr();
    __gc_init();
    size_t stack_top = (size_t) &g_stack.operand_stack[0];
    size_t stack_bottom = (size_t) &g_stack.operand_stack[STACK_SIZE];
    set_stack(stack_top, stack_bottom);

    bytefile *f = read_file(argv[1]);
    dump_file(stderr, f);
    return 0;
}
