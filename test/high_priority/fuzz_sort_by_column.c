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
int compare_rows_by_col(const Table *t, const Row *a, const Row *b, int col, int asc);

static void sort_by_column(Table *t, int col, int asc) {
    if (!t || t->col_count == 0) {
        printf("No table loaded.\n");
        return;
    }
    if (col < 0 || col >= t->col_count) {
        printf("Invalid column index.\n");
        return;
    }
    if (t->row_count <= 1) return;

    for (int i = 0; i < t->row_count - 1; i++) {
        for (int j = 0; j < t->row_count - 1 - i; j++) {
            if (compare_rows_by_col(t, &t->rows[j], &t->rows[j + 1], col, asc) > 0) {
                Row tmp = t->rows[j];
                t->rows[j] = t->rows[j + 1];
                t->rows[j + 1] = tmp;
            }
        }
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {

    if (size < 3) return 0;

    /* Build a valid Table */
    Table t;

    /* fuzz-driven column count */
    t.col_count = (data[0] % (MAX_COLS + 1));
    if (t.col_count == 0) t.col_count = 1;

    /* fuzz-driven row count */
    t.row_count = (data[1] % (MAX_ROWS + 1));
    if (t.row_count == 0) t.row_count = 1;

    /* allocate column names */
    for (int i = 0; i < t.col_count; i++) {
        t.col_names[i] = malloc(8);
        strcpy(t.col_names[i], "col");
    }

    /* allocate row cells */
    for (int r = 0; r < t.row_count; r++) {
        t.rows[r].cell_count = t.col_count;

        for (int c = 0; c < t.col_count; c++) {
            size_t alloc = 4 + ((r + c) % 32);
            t.rows[r].cells[c] = malloc(alloc);
            if (t.rows[r].cells[c]) {
                size_t len = alloc - 1;
                memcpy(t.rows[r].cells[c],
                       &data[(2 + r + c) % size],
                       (len <= size ? len : size));
                t.rows[r].cells[c][alloc - 1] = '\0';
            }
        }
    }

    /* fuzz control of column and direction */
    int col = data[2] % t.col_count;
    int asc = data[3] & 1;

    /* Call function under test */
    sort_by_column(&t, col, asc);

    /* Clean up */
    for (int r = 0; r < t.row_count; r++)
        for (int c = 0; c < t.col_count; c++)
            free(t.rows[r].cells[c]);

    for (int i = 0; i < t.col_count; i++)
        free(t.col_names[i]);

    return 0;
}
