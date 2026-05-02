#include <stdio.h>
/* Reads a binary bytecode file by name and unpacks it */
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


bytefile *read_file(char *fname);

const char *get_string(const bytefile *f, int pos);

char *get_public_name(bytefile *f, int i);

int get_public_offset(bytefile *f, int i);

void dump_file(FILE *f, bytefile *bf);
