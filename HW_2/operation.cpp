#include "operation.h"

#include <stdio.h>
#include <iostream>

#include "machine_state.h"

char *ops[] = {"+", "-", "*", "/", "%", "<", "<=", ">", ">=", "==", "!=", "&&", "!!"};

struct MachineState;

void BINOP_execute(FILE *f, MachineState &ms, int l) {
    fprintf(f, "BINOP\t%s", ops[l - 1]);

    int y = ms.stack.back();
    ms.stack.pop_back();
    int x = ms.stack.back();

    ms.stack.pop_back();
    if (ops[l - 1] == "*") {
        ms.stack.push_back(y * x);
    }
}

void BEGIN_execute(FILE *f, MachineState &ms) {
    const int first_int = ms.read_next_int();
    const int local_size = ms.read_next_int();
    fprintf(f, "BEGIN\t%d ", first_int);
    fprintf(f, "%d", local_size);

    ms.locals = Locals(local_size);
}

void CONST_execute(FILE *f, MachineState &ms) {
    const int cnst = ms.read_next_int();
    fprintf(f, "CONST\t%d", cnst);
    ms.stack.push_back(cnst);
}

void STORE_G_execute(FILE *f, MachineState &ms) {
    if (ms.stack.empty()) {
        std::cout << "Mistake. Should be something on top" << std::endl;
        // TODO: Normal mistakes.
        return;
    }
    int index = ms.read_next_int();
    fprintf(f, "G(%d)", index);
    int top_stack = ms.stack.back();
    ms.globals[index] = top_stack;
}

void LD_G_execute(FILE *f, MachineState &ms) {
    int index = ms.read_next_int();
    fprintf(f, "G(%d)", index);
    int value = ms.globals[index];
    ms.stack.push_back(value);
}

void DROP_execute(FILE *f, MachineState &ms) {
    ms.stack.pop_back();
    fprintf(f, "DROP");
}

void CALL_execute(FILE *f, MachineState &ms) {
    fprintf(f, "CALL\tLwrite");
    int arg_count = 1;
    std::vector<size_t> args;
    for (int i = 0; i < arg_count; ++i) {
        args.push_back(ms.stack.back());
        ms.stack.pop_back();
    }
    std:: cout << args[0] << std::endl;
}