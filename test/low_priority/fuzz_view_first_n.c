#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_COLS 32
#define MAX_ROWS 1024

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

/* Forward-declare these if they are in another .c file */
void print_header(const Table *t);
void print_row(const Table *t, const Row *r);

static void view_first_n(const Table *t, int n) {
    if (!t || t->col_count == 0) {
        printf("No table loaded.\n");
        return;
    }
    if (n <= 0 || n > t->row_count) n = t->row_count;
    printf("\n-- First %d row(s) --\n", n);
    print_header(t);
    for (int i = 0; i < n; i++) {
        print_row(t, &t->rows[i]);
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {

    /* Redirect stdout to /dev/null so fuzzing stays fast & clean */
    FILE *devnull = fopen("/dev/null", "w");
    if (!devnull) return 0;
    FILE *orig_stdout = stdout;
    stdout = devnull;

    Table t;

    /* Column & row counts from fuzz input */
    int col_count = (size > 0) ? (data[0] % (MAX_COLS + 1)) : 0;
    int row_count = (size > 1) ? (data[1] % (MAX_ROWS + 1)) : 0;

    t.col_count = col_count;
    t.row_count = row_count;

    /* --- Construct real, valid table data --- */

    /* allocate column names */
    for (int i = 0; i < col_count; i++) {
        size_t alloc_size = 4 + (size > 2 ? data[(2 + i) % size] % 32 : 8);
        t.col_names[i] = malloc(alloc_size);
        if (t.col_names[i]) {
            size_t copy_len = alloc_size - 1;
            memcpy(t.col_names[i],
                   &data[(2 + i) % size],
                   (copy_len <= size ? copy_len : size));
            t.col_names[i][alloc_size - 1] = '\0';
        }
    }
    for (int i = col_count; i < MAX_COLS; i++) {
        t.col_names[i] = NULL;
    }

    /* allocate rows */
    for (int r = 0; r < row_count; r++) {
        int cell_count = (size > 3) ? (data[(3 + r) % size] % (col_count + 1)) : 0;
        t.rows[r].cell_count = cell_count;

        for (int c = 0; c < cell_count; c++) {
            size_t alloc_size = 4 + ((r + c) % 32);
            t.rows[r].cells[c] = malloc(alloc_size);
            if (t.rows[r].cells[c]) {
                size_t copy_len = alloc_size - 1;
                memcpy(t.rows[r].cells[c],
                       &data[(4 + r + c) % size],
                       (copy_len <= size ? copy_len : size));
                t.rows[r].cells[c][alloc_size - 1] = '\0';
            }
        }
        for (int c = cell_count; c < MAX_COLS; c++) {
            t.rows[r].cells[c] = NULL;
        }
    }

    /* --- fuzz-controlled n --- */
    int n = (size > 2) ? (data[2] % (MAX_ROWS + 5)) : 1;

    /* --- execute function under test --- */
    view_first_n(&t, n);

    /* --- cleanup --- */
    for (int i = 0; i < col_count; i++)
        free(t.col_names[i]);

    for (int r = 0; r < row_count; r++)
        for (int c = 0; c < t.rows[r].cell_count; c++)
            free(t.rows[r].cells[c]);

    stdout = orig_stdout;
    fclose(devnull);

    return 0;
}
