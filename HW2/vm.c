#include "debug.h"
#include "runtime.h"
#include "stack.h"
#include "vm.h"

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
    aint closure_pointer = operand_get(get_ebp_index() + 2, POINTER);
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
    const aint len = LEN(closure_data->data_header);
    if (k >= len) {
        failure("closure index out of bounds: %zu (len=%zu)\n", k, len);
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
    const aint local_count = operand_get(get_ebp_index() - 2, VAL);
    if (k >= local_count) {
        failure("local index out of bounds: %zu (count=%zu)\n", k, local_count);
    }
    const size_t local_position = get_ebp_index() - 3 - k;

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
    const size_t arg_count = operand_get(get_ebp_index() + 1, VAL);
    if (k >= arg_count) {
        failure("argument index out of bounds: %zu (count=%zu)\n", k, arg_count);
    }
    const size_t arg_position = get_ebp_index() + 3 + k;
    const aint v = operand_get(arg_position, UNKNOWN);
    operand_push(v, UNKNOWN);
}

static void store_arg(const size_t k) {
    const aint arg_count = operand_get(get_ebp_index() + 1, VAL);
    if (k >= arg_count) {
        failure("argument index out of bounds: %zu (count=%zu)\n", k, arg_count);
    }
    const size_t arg_position = get_ebp_index() + 3 + k;
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

    const aint old_ebp = get_ebp_index();

    const aint ret_ip = operand_top(POINTER);
    operand_pop();

    operand_push((aint) num_args, VAL);
    operand_push(old_ebp, VAL);
    set_ebp_index((aint) stack_top_index());
    operand_push(ret_ip, POINTER);
    operand_push((aint) local_size, VAL);

    for (int i = 0; i < local_size; i++) {
        operand_push(-1, VAL);
    }
}

static aint end_function() {
    const aint stack_top = operand_top(UNKNOWN);
    const aint ebp = get_ebp_index();
    const aint ret_ip = operand_get(get_ebp_index() - 1, POINTER); // return address saved by CALL
    const aint old_ebp = operand_get(ebp, VAL);
    const aint num_args = operand_get(ebp + 1, VAL);

    set_stack_top_index(ebp + 3 + num_args);
    set_ebp_index(old_ebp);
    operand_push(stack_top, UNKNOWN);

    return ret_ip;
}

static void reverse_last_el(const int el_count) {
    if (el_count <= 1) {
        return;
    }

    const size_t have = STACK_SIZE - stack_top_index();
    if ((size_t) el_count > have) {
        failure("reverse_last_el: not enough operands: need=%d have=%zu\n", el_count, have);
    }
    aint *SP = SP_ptr();
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

    aint *SP = SP_ptr();

    const aint arr = (aint) Barray(SP, BOX(n));
    gc_stack_offset(n);
    operand_push(arr, POINTER);
}

static void sexp_function(char *tag, const int elem_size) {
    const aint hash_tag = UNBOX(LtagHash(tag));
    operand_push(hash_tag, VAL);

    aint *SP = SP_ptr();
    const aint result = (aint) Bsexp(SP, BOX(elem_size + 1));
    gc_stack_offset(elem_size + 1);
    operand_push(result, POINTER);
}

static void closure_function(const int code_pointer, const int arg_number) {
    operand_push(code_pointer, POINTER);
    aint *SP = SP_ptr();
    aint *closure = Bclosure(SP, BOX(arg_number));

    gc_stack_offset(arg_number + 1);
    operand_push((aint) closure, POINTER);
}

static aint callc_function(const int arg_number) {
    size_t closure_pos = stack_top_index() + (size_t) arg_number;
    if (closure_pos >= STACK_SIZE)
        failure("CALLC: invalid stack layout\n");

    aint closure_val = operand_get(closure_pos, POINTER);
    data *closure_data = TO_DATA(closure_val);

    if (TAG(closure_data->data_header) != CLOSURE_TAG) {
        failure("Expected closure\n");
    }

    for (int i = arg_number - 1; i >= 0; --i) {
        size_t base = stack_top_index();
        operand_set(base + (size_t) i + 1,
                    operand_get(base + (size_t) i, UNKNOWN),
                    UNKNOWN);
    }
    operand_set(stack_top_index(), closure_val, POINTER);
    const aint code_pointer = ((aint *) closure_data->contents)[0];

    return code_pointer;
}

static inline char get_byte(const bytefile *bf, char **ip) {
    if (*ip < bf->code_ptr || *ip + 1 > bf->code_end) {
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

void interpret(FILE *f, bytefile *bf) {
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

        DEBUG_LOG(f, "0x%.8x:\t", ip - bf->code_ptr - 1);

        switch (h) {
            case OP_END:
                goto stop;

            /* BINOP */
            case OP_BINOP:
                DEBUG_LOG(f, "BINOP\t%s", (l >= BINOP_ADD && l <= BINOP_OR) ? ops[l - 1] : "<invalid>"); {
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
                        DEBUG_LOG(f, "CONST\t%d", cnst);
                        operand_push(cnst, VAL);
                        break;
                    }

                    case MI_STRING: {
                        const char *s = STRING;
                        DEBUG_LOG(f, "STRING\t%s", s);
                        aint res = (aint) Bstring((aint *) &s);
                        operand_push(res, POINTER);
                        break;
                    }

                    case MI_SEXP: {
                        char *tag = STRING;
                        int elem_size = INT;
                        DEBUG_LOG(f, "SEXP\t%s ", tag);
                        DEBUG_LOG(f, "%d", elem_size);
                        sexp_function(tag, elem_size);
                        break;
                    }

                    case MI_STI:
                        DEBUG_LOG(f, "STI");
                        break;

                    case MI_STA: {
                        DEBUG_LOG(f, "STA");
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
                        DEBUG_LOG(f, "JMP\t0x%.8x", jump_address);
                        if (jump_address < 0 || bf->code_ptr + jump_address >= bf->code_end) {
                            failure("JMP target out of range: 0x%x\n", (unsigned) jump_address);
                        }
                        ip = bf->code_ptr + jump_address;
                        break;
                    }

                    case MI_END: {
                        DEBUG_LOG(f, "END\t");
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
                        DEBUG_LOG(f, "RET");
                        break;

                    case MI_DROP:
                        DEBUG_LOG(f, "DROP");
                        operand_pop();
                        break;

                    case MI_DUP: {
                        DEBUG_LOG(f, "DUP");
                        operand_push(operand_top(UNKNOWN), UNKNOWN);
                        break;
                    }

                    case MI_SWAP: {
                        DEBUG_LOG(f, "SWAP");
                        aint a = operand_top(UNKNOWN);
                        operand_pop();
                        aint b = operand_top(UNKNOWN);
                        operand_pop();
                        operand_push(a, UNKNOWN);
                        operand_push(b, UNKNOWN);
                        break;
                    }

                    case MI_ELEM: {
                        DEBUG_LOG(f, "ELEM");
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
                DEBUG_LOG(f, "%s\t", lds[h - 2]);
                switch (l) {
                    case LDS_G: {
                        int pos = INT;
                        DEBUG_LOG(f, "G(%d)", pos);
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
                        DEBUG_LOG(f, "L(%d)", l_number);
                        break;
                    }
                    case LDS_A: {
                        int arg_number = INT;
                        DEBUG_LOG(f, "A(%d)", arg_number);
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
                        DEBUG_LOG(f, "C(%d)", num_args);
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
                        DEBUG_LOG(f, "CJMPz\t0x%.8x", target);
                        aint cond = operand_top(VAL);
                        operand_pop();
                        if (cond == 0) {
                            if (target < 0 || bf->code_ptr + target >= bf->code_end) {
                                failure("CJMPz target out of range: 0x%x\n", (unsigned) target);
                            }
                            ip = bf->code_ptr + target;
                        }
                        break;
                    }

                    case CTRL_CJMPNZ: {
                        int target = INT;
                        DEBUG_LOG(f, "CJMPnz\t0x%.8x", target);
                        aint cond = operand_top(VAL);
                        operand_pop();
                        if (cond != 0) {
                            if (target < 0 || bf->code_ptr + target >= bf->code_end) {
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
                        DEBUG_LOG(f, "BEGIN\t%d ", num_args); // + CBEGIN
                        DEBUG_LOG(f, "%d", local_size);
                        begin_function(num_args, local_size);
                        break;
                    }

                    case CTRL_CLOSURE: {
                        int code_pntr = INT;
                        int n = INT;
                        DEBUG_LOG(f, "CLOSURE\t0x%.8x ", code_pntr);
                        for (int i = 0; i < n; i++) {
                            switch (BYTE) {
                                case LDS_G: {
                                    int pos = INT;
                                    DEBUG_LOG(f, "G(%d)", pos);
                                    load_global(pos);
                                    break;
                                }
                                case LDS_L: {
                                    int pos = INT;
                                    DEBUG_LOG(f, "L(%d)", pos);
                                    load_local(pos);
                                    break;
                                }
                                case LDS_A: {
                                    int pos = INT;
                                    DEBUG_LOG(f, "A(%d)", pos);
                                    load_arg(pos);
                                    break;
                                }
                                case LDS_C: {
                                    int pos = INT;
                                    DEBUG_LOG(f, "C(%d)", pos);
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
                        DEBUG_LOG(f, "CALLC\t%d", arg_number);
                        aint offset = callc_function(arg_number);
                        if (offset < 0 || bf->code_ptr + offset >= bf->code_end) {
                            failure("CALLC target out of range: 0x%x\n", (unsigned) offset);
                        }
                        operand_push((aint) ip, POINTER);
                        ip = bf->code_ptr + offset;
                        break;
                    }

                    case CTRL_CALL: {
                        int call_pos = INT;
                        int number_of_args = INT;
                        DEBUG_LOG(f, "CALL\t0x%.8x ", call_pos);
                        DEBUG_LOG(f, "%d", number_of_args);
                        reverse_last_el(number_of_args);
                        operand_push(0, VAL);
                        operand_push((aint) ip, POINTER);
                        ip = bf->code_ptr + call_pos;
                        break;
                    }

                    case CTRL_TAG: {
                        char *tag = STRING;
                        int elem_size = INT;
                        DEBUG_LOG(f, "TAG\t%s ", tag);
                        DEBUG_LOG(f, "%d", elem_size);
                        aint sexp = operand_top(POINTER);
                        operand_pop();
                        aint res = Btag((void *) sexp, LtagHash(tag), BOX(elem_size));
                        res = UNBOX(res);
                        operand_push(res, VAL);
                        break;
                    }

                    case CTRL_ARRAY: {
                        int el_size = INT;
                        DEBUG_LOG(f, "ARRAY\t%d", el_size);

                        aint arr = operand_top(POINTER);
                        operand_push(UNBOX(Barray_patt((void *) arr, BOX(el_size))), VAL);
                        break;
                    }

                    case CTRL_FAIL: {
                        aint x = INT;
                        aint y = INT;
                        DEBUG_LOG(f, "FAIL\t%d", x);
                        DEBUG_LOG(f, "%d", y);
                        failure("FAIL");
                        break;
                    }

                    case CTRL_LINE: {
                        aint x = INT;
                        DEBUG_LOG(f, "LINE\t%d", x);
                        break;
                    }
                    default:
                        FAIL;
                }
                break;

            case OP_PATT: {
                DEBUG_LOG(f, "PATT\t%s", pats[l]);
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
                        DEBUG_LOG(f, "CALL\tLread");
                        aint in = Lread();
                        operand_push(UNBOX(in), VAL);
                        break;

                    case RT_WRITE: {
                        DEBUG_LOG(f, "CALL\tLwrite");
                        aint out = operand_top(VAL);
                        Lwrite(BOX(out));
                        break;
                    }

                    case RT_LENGTH:
                        DEBUG_LOG(f, "CALL\tLlength");
                        aint out = operand_top(POINTER);
                        operand_pop();
                        aint res = Llength((void *) out);
                        operand_push(res, UNKNOWN);
                        break;

                    case RT_STRING:
                        DEBUG_LOG(f, "CALL\tLstring");
                        operand_push((aint) Lstring(SP_ptr()), POINTER);
                        break;

                    case RT_BARRAY: {
                        int size = INT;
                        DEBUG_LOG(f, "CALL\tBarray\t%d", size);
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

        DEBUG_LOG(f, "\n");
    } while (1);
stop:
    DEBUG_LOG(f, "<end>\n");
}
