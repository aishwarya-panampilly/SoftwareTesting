#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define MAX_COLS 32
#define MAX_ROWS 256  // adjust to your real value

typedef struct {
    char *cells[MAX_COLS];
    int cell_count;
} Row;

static void free_row(Row *r) {
    if (!r) return;
    for (int i = 0; i < r->cell_count; i++) {
        free(r->cells[i]);
        r->cells[i] = NULL;
    }
    r->cell_count = 0;
}

typedef struct {
    int col_count;
    int row_count;
    char *col_names[MAX_COLS];
    Row rows[MAX_ROWS];
} Table;

static void free_table(Table *t) {
    if (!t) return;
    for (int i = 0; i < t->col_count; i++) {
        free(t->col_names[i]);
        t->col_names[i] = NULL;
    }
    for (int i = 0; i < t->row_count; i++) {
        free_row(&t->rows[i]);
    }
    t->col_count = 0;
    t->row_count = 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {

    Table t;

    // Use fuzz input to decide how many cols/rows to allocate
    int col_count = (size > 0) ? (data[0] % (MAX_COLS + 1)) : 0;
    int row_count = (size > 1) ? (data[1] % (MAX_ROWS + 1)) : 0;

    t.col_count = col_count;
    t.row_count = row_count;

    // Allocate memory for column names
    for (int i = 0; i < col_count; i++) {
        size_t alloc_size = 8 + (data[(i % size)] % 32); // 8–40 bytes
        t.col_names[i] = malloc(alloc_size);
        if (t.col_names[i])
            memset(t.col_names[i], 'A' + (i % 26), alloc_size);
    }
    for (int i = col_count; i < MAX_COLS; i++) {
        t.col_names[i] = NULL;
    }

    // Allocate memory for row cells  
    for (int r = 0; r < row_count; r++) {
        int cell_count = (size > 2) ? (data[(r + 2) % size] % (MAX_COLS + 1)) : 0;
        t.rows[r].cell_count = cell_count;

        for (int c = 0; c < cell_count; c++) {
            size_t alloc_size = 8 + ((r + c) % 32); // deterministic pattern
            t.rows[r].cells[c] = malloc(alloc_size);
            if (t.rows[r].cells[c])
                memset(t.rows[r].cells[c], 'a' + ((r+c) % 26), alloc_size);
        }
        for (int c = cell_count; c < MAX_COLS; c++) {
            t.rows[r].cells[c] = NULL;
        }
    }

    for (int r = row_count; r < MAX_ROWS; r++) {
        t.rows[r].cell_count = 0;
        for (int c = 0; c < MAX_COLS; c++)
            t.rows[r].cells[c] = NULL;
    }

    // Call function under test
    free_table(&t);

    return 0;
}
