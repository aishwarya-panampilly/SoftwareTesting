#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

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

/* Target function */
int find_rows_by_substring(const Table *t,
                           int col,
                           const char *pattern,
                           int out_indices[],
                           int max_out);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 3) return 0;

    /* Build a valid table */
    Table t;

    t.col_count = (data[0] % MAX_COLS);
    if (t.col_count == 0) t.col_count = 1;

    t.row_count = (data[1] % MAX_ROWS);
    if (t.row_count == 0) t.row_count = 1;

    /* allocate dummy column names */
    for (int i = 0; i < t.col_count; i++) {
        t.col_names[i] = malloc(8);
        strcpy(t.col_names[i], "col");
    }

    /* allocate cells */
    for (int r = 0; r < t.row_count; r++) {
        t.rows[r].cell_count = t.col_count;

        for (int c = 0; c < t.col_count; c++) {
            size_t alloc_len = 4 + (data[(2 + r + c) % size] % 32);
            t.rows[r].cells[c] = malloc(alloc_len);
            if (t.rows[r].cells[c]) {
                size_t copy_len = alloc_len - 1;
                memcpy(t.rows[r].cells[c], data, (copy_len < size) ? copy_len : size);
                t.rows[r].cells[c][alloc_len - 1] = '\0';
            }
        }
    }

    /* pattern */
    char *pattern = malloc(size + 1);
    memcpy(pattern, data, size);
    pattern[size] = '\0';

    int *indices = malloc(MAX_ROWS * sizeof(int));

    /* fuzz target */
    find_rows_by_substring(&t,
                           data[2] % t.col_count,
                           pattern,
                           indices,
                           MAX_ROWS);

    /* cleanup */
    free(pattern);
    for (int i = 0; i < t.col_count; i++) free(t.col_names[i]);
    for (int r = 0; r < t.row_count; r++)
        for (int c = 0; c < t.col_count; c++)
            free(t.rows[r].cells[c]);
    free(indices);

    return 0;
}
