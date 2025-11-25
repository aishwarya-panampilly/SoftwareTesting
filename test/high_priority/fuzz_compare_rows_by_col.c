#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_COLS 32
#define MAX_ROWS 64
#define MAX_FIELD_LEN 256

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

/* external real functions */
int parse_double(const char *s, double *out);

static int compare_rows_by_col(const Table *t, const Row *a, const Row *b, int col, int asc) {
    const char *ca = (col < a->cell_count && a->cells[col]) ? a->cells[col] : "";
    const char *cb = (col < b->cell_count && b->cells[col]) ? b->cells[col] : "";

    double va, vb;
    int na = parse_double(ca, &va);
    int nb = parse_double(cb, &vb);
    int cmp;
    if (na && nb) {
        if (va < vb) cmp = -1;
        else if (va > vb) cmp = 1;
        else cmp = 0;
    } else {
        cmp = strcmp(ca, cb);
    }
    return asc ? cmp : -cmp;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {

    if (size < 4) return 0;

    /* Build a minimal Table */
    Table t;

    t.col_count = (data[0] % (MAX_COLS + 1));
    if (t.col_count == 0) t.col_count = 1;

    t.row_count = 2; /* only 2 rows are needed */

    /* allocate column names */
    for (int i = 0; i < t.col_count; i++) {
        t.col_names[i] = malloc(8);
        strcpy(t.col_names[i], "col");
    }

    /* Build row A and row B */
    for (int r = 0; r < 2; r++) {
        t.rows[r].cell_count = t.col_count;

        for (int c = 0; c < t.col_count; c++) {

            /* allocate variable-sized string */
            size_t alloc = 4 + ((r + c) % 32);
            t.rows[r].cells[c] = malloc(alloc);

            if (t.rows[r].cells[c]) {
                size_t copy_len = alloc - 1;
                memcpy(t.rows[r].cells[c],
                       &data[(2 + r + c) % size],
                       (copy_len <= size ? copy_len : size));
                t.rows[r].cells[c][alloc - 1] = '\0';
            }
        }
    }

    int col = data[1] % t.col_count;
    int asc = data[2] & 1;

    /* Call function under test */
    compare_rows_by_col(&t, &t.rows[0], &t.rows[1], col, asc);

    /* Free memory */
    for (int r = 0; r < 2; r++)
        for (int c = 0; c < t.col_count; c++)
            free(t.rows[r].cells[c]);

    for (int i = 0; i < t.col_count; i++)
        free(t.col_names[i]);

    return 0;
}
