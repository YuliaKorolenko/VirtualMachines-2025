/* Lama SM Bytecode interpreter */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "runtime/gc.h"
#include "stack.h"
#include "vm.h"

int main(int argc, char *argv[]) {
    __gc_init();
    bytefile *bf = read_file(argv[1]);
    dump_file(stderr, bf);
    stack_init(bf->global_area_size);
    interpret(stderr, bf);
    return 0;
}
