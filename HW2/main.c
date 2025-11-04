/* Lama SM Bytecode interpreter */

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include "runtime/runtime.h"
#include "runtime/gc.h"

void *__start_custom_data;
void *__stop_custom_data;
#define STACK_SIZE 30

typedef struct {
    aint operand_stack[STACK_SIZE];
    size_t stack_top_index; // points to next free slot; stack grows downwards (from end to start)
    size_t ebp_index;
} OperandStack;

OperandStack g_stack = {.stack_top_index = STACK_SIZE, .ebp_index = 0};

static void operand_push(aint value) {
    if (g_stack.stack_top_index == 0) {
        failure("operand stack overflow\n");
    }
    g_stack.operand_stack[--g_stack.stack_top_index] = value;
}

static aint operand_top(void) {
    if (g_stack.stack_top_index + 1 >= STACK_SIZE) {
        failure("operand stack underflow in top operation\n");
    }
    return g_stack.operand_stack[g_stack.stack_top_index];
}

static void operand_pop(void) {
    if (g_stack.stack_top_index + 1 >= STACK_SIZE) {
        failure("operand stack underflow in pop operation\n");
    }
    g_stack.stack_top_index++;
}

static void store_operation(FILE *f, size_t k) {
    fprintf(f, "G(%d)", k);
    if (k < 0 || k >= STACK_SIZE) {
        failure("global index out of bounds: %d (size=%d)\n", k, STACK_SIZE);
    }
    aint v = operand_top();
    g_stack.operand_stack[STACK_SIZE - 1 - k] = v;
}

static size_t get_local_pos(size_t k) {
    size_t local_size = g_stack.operand_stack[g_stack.ebp_index - 2];
    if (k >= local_size) {
        failure("local index out of bounds: %zu (size=%zu)\n", k, local_size);
    }
    size_t local_position = g_stack.ebp_index - 3 - k;
    if (local_position >= STACK_SIZE) {
        failure("local position out of stack bounds: %zu\n", local_position);
    }
    return local_position;
}

static void load_local(size_t k) {
    size_t local_position = get_local_pos(k);
    aint v = g_stack.operand_stack[local_position];
    operand_push(v);
}

static void store_local(size_t k) {
    size_t local_position = get_local_pos(k);
    const size_t v = operand_top();
    g_stack.operand_stack[local_position] = v;
}

void begin_function(FILE *f, int num_args, int local_size) {
    fprintf(f, "BEGIN\t%d ", num_args);
    fprintf(f, "%d", local_size);

    // Shema: the number of arguments, EBP, return address, local vars number, local vars
    //   [ebp + 2 ...] = arguments
    //   [ebp + 1] = number of arguments
    //   [ebp] = old ebp
    //   [ebp-1] = return address
    //   [ebp-2] = local_size
    //   [ebp-3 - i] = local i (0..local_size-1)

    size_t old_ebp = g_stack.ebp_index;
    aint ret_ip = operand_top();
    operand_pop();

    operand_push(num_args);
    operand_push(old_ebp);
    g_stack.ebp_index = g_stack.stack_top_index;
    operand_push(ret_ip);
    operand_push(local_size);

    for (int i = 0; i < local_size; i++) {
        operand_push(-1);
    }
}


/* The unpacked representation of bytecode file */
typedef struct {
    char *string_ptr; /* A pointer to the beginning of the string table */
    int *public_ptr; /* A pointer to the beginning of publics table    */
    char *code_ptr; /* A pointer to the bytecode itself               */
    int *global_ptr; /* A pointer to the global area                   */
    int stringtab_size; /* The size (in bytes) of the string table        */
    int global_area_size; /* The size (in words) of global area             */
    int public_symbols_number; /* The number of public symbols                   */
    char buffer[0];
} bytefile;

/* Gets a string from a string table by an index */
char *get_string(bytefile *f, int pos) {
    return &f->string_ptr[pos];
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
    long size;
    bytefile *file;

    if (f == 0) {
        failure("%s\n", strerror(errno));
    }

    if (fseek(f, 0, SEEK_END) == -1) {
        failure("%s\n", strerror(errno));
    }

    file = (bytefile *) malloc(sizeof(int) * 4 + (size = ftell(f)));

    if (file == 0) {
        failure("*** FAILURE: unable to allocate memory.\n");
    }

    rewind(f);

    if (size != fread(&file->stringtab_size, 1, size, f)) {
        failure("%s\n", strerror(errno));
    }

    fclose(f);

    file->string_ptr = &file->buffer[file->public_symbols_number * 2 * sizeof(int)];
    file->public_ptr = (int *) file->buffer;
    file->code_ptr = &file->string_ptr[file->stringtab_size];
    file->global_ptr = (int *) malloc(file->global_area_size * sizeof(int));

    return file;
}

/* Disassembles the bytecode pool */
void disassemble(FILE *f, bytefile *bf) {
#define INT (ip += sizeof(int), *(int *)(ip - sizeof(int)))
#define BYTE *ip++
#define STRING get_string(bf, INT)
#define FAIL failure("ERROR: invalid opcode %d-%d\n", h, l)

    char *ip = bf->code_ptr;
    char *ops[] = {"+", "-", "*", "/", "%", "<", "<=", ">", ">=", "==", "!=", "&&", "!!"};
    char *pats[] = {"=str", "#string", "#array", "#sexp", "#ref", "#val", "#fun"};
    char *lds[] = {"LD", "LDA", "ST"};
    do {
        char x = BYTE,
                h = (x & 0xF0) >> 4,
                l = x & 0x0F;

        fprintf(f, "0x%.8x:\t", ip - bf->code_ptr - 1);

        switch (h) {
            case 15:
                goto stop;

            /* BINOP */
            case 0:
                fprintf(f, "BINOP\t%s", ops[l - 1]);
                break;

            case 1:
                switch (l) {
                    case 0: {
                        int cnst = INT;
                        fprintf(f, "CONST\t%d", cnst);
                        operand_push(BOX(cnst));
                        break;
                    }

                    case 1:
                        fprintf(f, "STRING\t%s", STRING);
                        break;

                    case 2:
                        fprintf(f, "SEXP\t%s ", STRING);
                        fprintf(f, "%d", INT);
                        break;

                    case 3:
                        fprintf(f, "STI");
                        break;

                    case 4:
                        fprintf(f, "STA");
                        break;

                    case 5:
                        fprintf(f, "JMP\t0x%.8x", INT);
                        break;

                    case 6:
                        fprintf(f, "END");
                        break;

                    case 7:
                        fprintf(f, "RET");
                        break;

                    case 8:
                        fprintf(f, "DROP");
                        operand_pop();
                        break;

                    case 9:
                        fprintf(f, "DUP");
                        break;

                    case 10:
                        fprintf(f, "SWAP");
                        break;

                    case 11:
                        fprintf(f, "ELEM");
                        break;

                    default:
                        FAIL;
                }
                break;

            case 2:
            case 3:
            case 4:
                fprintf(f, "%s\t", lds[h - 2]);
                switch (l) {
                    case 0: {
                        if (h == 4) {
                            store_operation(f, INT);
                        } else {
                            fprintf(f, "G(%d)", INT);
                        }
                    }
                    break;
                    case 1: {
                        aint l_number = INT;
                        if (h == 4) {
                            store_local(l_number);
                        }
                        if (h == 2) {
                            load_local(l_number);
                        }
                        fprintf(f, "L(%d)", l_number);
                        break;
                    }
                    case 2:
                        fprintf(f, "A(%d)", INT);
                        break;
                    case 3:
                        fprintf(f, "C(%d)", INT);
                        break;
                    default:
                        FAIL;
                }
                break;

            case 5:
                switch (l) {
                    case 0:
                        fprintf(f, "CJMPz\t0x%.8x", INT);
                        break;

                    case 1:
                        fprintf(f, "CJMPnz\t0x%.8x", INT);
                        break;

                    case 2:
                        begin_function(f, INT, INT);
                        break;

                    case 3:
                        fprintf(f, "CBEGIN\t%d ", INT);
                        fprintf(f, "%d", INT);
                        break;

                    case 4:
                        fprintf(f, "CLOSURE\t0x%.8x", INT); {
                            int n = INT;
                            for (int i = 0; i < n; i++) {
                                switch (BYTE) {
                                    case 0:
                                        fprintf(f, "G(%d)", INT);
                                        break;
                                    case 1:
                                        fprintf(f, "L(%d)", INT);
                                        break;
                                    case 2:
                                        fprintf(f, "A(%d)", INT);
                                        break;
                                    case 3:
                                        fprintf(f, "C(%d)", INT);
                                        break;
                                    default:
                                        FAIL;
                                }
                            }
                        };
                        break;

                    case 5: {
                        fprintf(f, "CALLC\t%d", INT);
                        break;
                    }

                    case 6: {
                        int call_pos = INT;
                        int number_of_args = INT;
                        fprintf(f, "CALL\t0x%.8x ", call_pos);
                        fprintf(f, "%d", number_of_args);

                        operand_push((aint) ip);
                        ip = bf->code_ptr + call_pos;
                        break;
                    }

                    case 7:
                        fprintf(f, "TAG\t%s ", STRING);
                        fprintf(f, "%d", INT);
                        break;

                    case 8:
                        fprintf(f, "ARRAY\t%d", INT);
                        break;

                    case 9:
                        fprintf(f, "FAIL\t%d", INT);
                        fprintf(f, "%d", INT);
                        break;

                    case 10:
                        fprintf(f, "LINE\t%d", INT);
                        break;

                    default:
                        FAIL;
                }
                break;

            case 6:
                fprintf(f, "PATT\t%s", pats[l]);
                break;

            case 7: {
                switch (l) {
                    case 0:
                        fprintf(f, "CALL\tLread");
                        aint in = Lread();
                        operand_push(in);
                        break;

                    case 1: {
                        fprintf(f, "CALL\tLwrite");
                        aint out = operand_top();
                        Lwrite(out);
                        break;
                    }

                    case 2:
                        fprintf(f, "CALL\tLlength");
                        break;

                    case 3:
                        fprintf(f, "CALL\tLstring");
                        break;

                    case 4:
                        fprintf(f, "CALL\tBarray\t%d", INT);
                        break;

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
    fprintf(f, "Number of public symbols: %d\n", bf->public_symbols_number);
    fprintf(f, "Public symbols          :\n");

    for (i = 0; i < bf->public_symbols_number; i++)
        fprintf(f, "   0x%.8x: %s\n", get_public_offset(bf, i), get_public_name(bf, i));

    fprintf(f, "Code:\n");

    // return address for first begin
    operand_push(0);
    disassemble(f, bf);
}

int main(int argc, char *argv[]) {
    // stack_top < stack_bottom
    size_t stack_top = (size_t) &g_stack.operand_stack[0];
    size_t stack_bottom = (size_t) &g_stack.operand_stack[STACK_SIZE];
    set_stack(stack_top, stack_bottom);

    bytefile *f = read_file(argv[1]);
    dump_file(stdout, f);
    return 0;
}
