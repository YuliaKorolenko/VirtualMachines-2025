/* Lama SM Bytecode interpreter */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "runtime/gc.h"
#include "stack.h"
#include "vm.h"

int main(int argc, char *argv[]) {
    __gc_init();
    if (argc != 2) {
        fprintf(stderr, "Expected file argument: %s <file.bc>\n", argv[0]);
        return 1;
    }
    bytefile *bf = read_file(argv[1]);
    dump_file(stderr, bf);
    bytefile_set_entry(bf);
    stack_init(bf->global_area_size);
    interpret(stderr, bf);
    free(bf);
    return 0;
}
