#include "bytefile.h"
#include <sys/stat.h>

#include "debug.h"
#include "runtime.h"

bytefile *read_file(char *fname) {
    FILE *f = fopen(fname, "rb");
    bytefile *file;

    if (f == 0) {
        failure("%s\n", strerror(errno));
    }

    if (fseek(f, 0, SEEK_END) == -1) {
        failure("%s\n", strerror(errno));
    }

    struct stat st;
    stat(fname, &st);
    if (st.st_size > LONG_MAX)
        failure("Bytecode file too large: %lld", st.st_size);

    const long size = ftell(f);
    file = malloc(offsetof(bytefile, stringtab_size) + (size_t)size);

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

/* Gets a string from a string table by an index */
const char *get_string(const bytefile *f, int pos) {
    if (pos < 0 || pos >= f->stringtab_size) {
        failure("Incorrect string index: %d (size=%d)\n", pos, f->stringtab_size);
    }
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

void bytefile_set_entry(bytefile *bf) {
    int i;
    bf->entry_ptr = 0;
    for (i = 0; i < bf->public_symbols_number; i++) {
        const char *public_name = get_public_name(bf, i);
        const int offset = get_public_offset(bf, i);
        if (strcmp(public_name, "main") == 0) {
            bf->entry_ptr = bf->code_ptr + offset;
        }
        DEBUG_LOG(f, "   0x%.8x: %s\n", offset, public_name);
    }
    if (bf->entry_ptr == 0) {
        failure("Main function not found");
    }
}

/* Dumps the contents of the file */
void dump_file(FILE *f, bytefile *bf) {
    DEBUG_LOG(f, "String table size       : %d\n", bf->stringtab_size);
    DEBUG_LOG(f, "Global area size        : %d\n", bf->global_area_size);
    DEBUG_LOG(f, "Number of public symbols: %d\n", bf->public_symbols_number);
    DEBUG_LOG(f, "Public symbols          :\n");

    DEBUG_LOG(f, "Code:\n");
}
