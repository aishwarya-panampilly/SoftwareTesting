#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define MAX_COLS 32
#define MAX_ROWS 256   // (adjust to match your actual constants)

typedef struct {
    char *cells[MAX_COLS];
    int cell_count;
} Row;

static void init_row(Row *r) {
    if (!r) return;
    r->cell_count = 0;
    for (int i = 0; i < MAX_COLS; i++) {
        r->cells[i] = NULL;
    }
}

typedef struct {
    int col_count;
    int row_count;
    char *col_names[MAX_COLS];
    Row rows[MAX_ROWS];
} Table;

static void init_table(Table *t) {
    if (!t) return;
    t->col_count = 0;
    t->row_count = 0;
    for (int i = 0; i < MAX_COLS; i++) {
        t->col_names[i] = NULL;
    }
    for (int i = 0; i < MAX_ROWS; i++) {
        init_row(&t->rows[i]);
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {

    Table t;

    // Fill table with fuzz bytes to simulate corrupted/uninitialized state
    size_t copy_len = size < sizeof(Table) ? size : sizeof(Table);
    memset(&t, 0xAA, sizeof(Table));   // known-garbage pattern
    memcpy(&t, data, copy_len);        // partially overwrite with fuzz

    // Function under test
    init_table(&t);

    // No dynamic memory in Table; nothing to free
    return 0;
}
