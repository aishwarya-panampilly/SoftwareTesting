#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Import the REAL implementation */
#include "../../csv_sql.c"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 4) return 0;

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

    /* Rows and cells */
    for (int r = 0; r < t.row_count; r++) {
        t.rows[r].cell_count = t.col_count;

        for (int c = 0; c < t.col_count; c++) {

            size_t alloc = 4 + ((r + c) % 32);
            t.rows[r].cells[c] = malloc(alloc);

            if (t.rows[r].cells[c]) {
                size_t usable = alloc - 1;
                size_t idx = (2 + r + c) % size;
                size_t available = size - idx;
                size_t n = usable < available ? usable : available;

                memcpy(t.rows[r].cells[c], &data[idx], n);
                t.rows[r].cells[c][n] = '\0';
            }
        }
    }

    int col = data[2] % t.col_count;

    /* fuzz min and max values */
    double min_val = 0, max_val = 0;
    char min_buf[32], max_buf[32];

    size_t half = size / 2;
    size_t ml = (half < 31 ? half : 31);
    memcpy(min_buf, data, ml);
    min_buf[ml] = '\0';

    size_t xl = (size - half < 31 ? size - half : 31);
    memcpy(max_buf, data + half, xl);
    max_buf[xl] = '\0';

    parse_double(min_buf, &min_val);
    parse_double(max_buf, &max_val);

    int indices[MAX_ROWS];

    /* CALL REAL FUNCTION */
    find_rows_in_range(&t, col, min_val, max_val, indices, MAX_ROWS);

    /* cleanup */
    for (int i = 0; i < t.col_count; i++)
        free(t.col_names[i]);

    for (int r = 0; r < t.row_count; r++)
        for (int c = 0; c < t.col_count; c++)
            free(t.rows[r].cells[c]);

    return 0;
}

