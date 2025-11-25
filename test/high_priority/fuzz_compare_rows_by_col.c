#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Import REAL implementation */
#include "../../csv_sql.c"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {

    if (size < 4) return 0;

    /* Build a Table with 2 rows */
    Table t;

    t.col_count = data[0] % (MAX_COLS + 1);
    if (t.col_count == 0)
        t.col_count = 1;

    t.row_count = 2;

    /* Allocate column names */
    for (int i = 0; i < t.col_count; i++) {
        t.col_names[i] = malloc(8);
        strcpy(t.col_names[i], "col");
    }

    /* Build cell contents */
    for (int r = 0; r < 2; r++) {
        t.rows[r].cell_count = t.col_count;

        for (int c = 0; c < t.col_count; c++) {

            size_t alloc = 4 + ((r + c) % 32);
            t.rows[r].cells[c] = malloc(alloc);

            if (t.rows[r].cells[c]) {

                size_t usable = alloc - 1;
                size_t idx = (2 + r + c) % size;
                size_t avail = size - idx;
                size_t n = (usable < avail ? usable : avail);

                memcpy(t.rows[r].cells[c], &data[idx], n);
                t.rows[r].cells[c][n] = '\0';
            }
        }
    }

    int col = data[1] % t.col_count;
    int asc = data[2] & 1;

    /* Call REAL project function */
    compare_rows_by_col(&t, &t.rows[0], &t.rows[1], col, asc);

    /* Cleanup */
    for (int r = 0; r < 2; r++)
        for (int c = 0; c < t.col_count; c++)
            free(t.rows[r].cells[c]);

    for (int i = 0; i < t.col_count; i++)
        free(t.col_names[i]);

    return 0;
}
