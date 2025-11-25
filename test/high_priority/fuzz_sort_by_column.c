#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Import REAL project implementation */
#include "../../csv_sql.c"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {

    if (size < 4)
        return 0;

    /* ---- Build a valid table ---- */
    Table t;

    t.col_count = data[0] % (MAX_COLS + 1);
    if (t.col_count == 0) t.col_count = 1;

    t.row_count = data[1] % (MAX_ROWS + 1);
    if (t.row_count == 0) t.row_count = 1;

    /* allocate column names */
    for (int i = 0; i < t.col_count; i++) {
        t.col_names[i] = malloc(8);
        strcpy(t.col_names[i], "col");
    }

    /* allocate row cells (safe memcpy) */
    for (int r = 0; r < t.row_count; r++) {
        t.rows[r].cell_count = t.col_count;

        for (int c = 0; c < t.col_count; c++) {
            size_t alloc = 4 + ((r + c) % 32);
            t.rows[r].cells[c] = malloc(alloc);

            if (t.rows[r].cells[c]) {
                size_t to_copy = alloc - 1;
                size_t idx = (2 + r + c) % size;
                size_t avail = size - idx;
                size_t n = to_copy < avail ? to_copy : avail;

                memcpy(t.rows[r].cells[c], &data[idx], n);
                t.rows[r].cells[c][n] = '\0';
            }
        }
    }

    /* ---- Fuzzed parameters ---- */
    int col = data[2] % t.col_count;
    int asc = data[3] & 1;

    /* ---- Call REAL function from csv_sql.c ---- */
    sort_by_column(&t, col, asc);

    /* ---- Cleanup ---- */
    for (int r = 0; r < t.row_count; r++)
        for (int c = 0; c < t.col_count; c++)
            free(t.rows[r].cells[c]);

    for (int i = 0; i < t.col_count; i++)
        free(t.col_names[i]);

    return 0;
}
