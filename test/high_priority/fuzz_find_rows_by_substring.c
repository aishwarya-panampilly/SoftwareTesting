#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Use the REAL implementation */
#include "../../csv_sql.c"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {

    if (size < 3)
        return 0;

    /* Build a valid Table */
    Table t;

    t.col_count = data[0] % (MAX_COLS + 1);
    if (t.col_count == 0) t.col_count = 1;

    t.row_count = data[1] % (MAX_ROWS + 1);
    if (t.row_count == 0) t.row_count = 1;

    /* Column names */
    for (int i = 0; i < t.col_count; i++) {
        t.col_names[i] = malloc(8);
        strcpy(t.col_names[i], "col");
    }

    /* Row strings */
    for (int r = 0; r < t.row_count; r++) {
        t.rows[r].cell_count = t.col_count;

        for (int c = 0; c < t.col_count; c++) {

            size_t alloc = 4 + ((r + c) % 32);
            t.rows[r].cells[c] = malloc(alloc);

            if (t.rows[r].cells[c]) {
                size_t usable = alloc - 1;
                size_t idx = (2 + r + c) % size;
                size_t avail = size - idx;
                size_t n = usable < avail ? usable : avail;

                memcpy(t.rows[r].cells[c], &data[idx], n);
                t.rows[r].cells[c][n] = '\0';
            }
        }
    }

    /* pattern */
    char *pattern = malloc(size + 1);
    memcpy(pattern, data, size);
    pattern[size] = '\0';

    int indices[MAX_ROWS];

    /* Call REAL project function */
    find_rows_by_substring(&t,
                           data[2] % t.col_count,
                           pattern,
                           indices,
                           MAX_ROWS);

    /* Cleanup */
    free(pattern);

    for (int i = 0; i < t.col_count; i++)
        free(t.col_names[i]);

    for (int r = 0; r < t.row_count; r++)
        for (int c = 0; c < t.col_count; c++)
            free(t.rows[r].cells[c]);

    return 0;
}
