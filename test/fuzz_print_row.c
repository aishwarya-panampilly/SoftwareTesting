#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_COLS 32
#define MAX_ROWS 256

typedef struct {
    char *cells[MAX_COLS];
    int cell_count;
} Row;

typedef struct {
    int col_count;
    int row_count;
    char *col_names[MAX_COLS];
    Row rows[MAX_ROWS];
} Table;

static void print_row(const Table *t, const Row *r) {
    if (!t || !r) return;
    for (int i = 0; i < t->col_count; i++) {
        const char *val = (i < r->cell_count && r->cells[i]) ? r->cells[i] : "NULL";
        printf("%s", val);
        if (i + 1 < t->col_count) printf(" | ");
    }
    printf("\n");
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {

    // Redirect stdout to avoid spamming terminal
    FILE *devnull = fopen("/dev/null", "w");
    if (!devnull) return 0;
    FILE *orig_stdout = stdout;
    stdout = devnull;

    Table t;
    Row r;

    // Set table column count based on fuzz input
    int col_count = (size > 0) ? (data[0] % (MAX_COLS + 1)) : 0;
    t.col_count = col_count;

    // Choose row cell count from fuzz input
    int cell_count = (size > 1) ? (data[1] % (MAX_COLS + 1)) : 0;
    r.cell_count = cell_count;

    // Allocate valid strings for row cells
    for (int i = 0; i < cell_count; i++) {
        // Each cell gets a small, real string
        size_t alloc_size = 4 + (data[(2 + i) % size] % 32);  // 4–36 bytes
        r.cells[i] = malloc(alloc_size);
        if (r.cells[i]) {
            // Fill with fuzzed data, ensure null-terminated
            size_t copy_len = alloc_size - 1;
            memcpy(r.cells[i],
                   &data[(2 + i) % size],
                   (copy_len <= size ? copy_len : size));
            r.cells[i][alloc_size - 1] = '\0';
        }
    }

    for (int i = cell_count; i < MAX_COLS; i++) {
        r.cells[i] = NULL;
    }

    // Call function under test
    print_row(&t, &r);

    // Cleanup
    for (int i = 0; i < cell_count; i++) {
        free(r.cells[i]);
    }

    stdout = orig_stdout;
    fclose(devnull);

    return 0;
}
