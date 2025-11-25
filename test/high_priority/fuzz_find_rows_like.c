#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Import REAL implementation */
#include "../../csv_sql.c"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 4) return 0;

    /* Redirect stdout */
    FILE *devnull = fopen("/dev/null", "w");
    if (!devnull) return 0;

    FILE *orig_stdout = stdout;
    stdout = devnull;

    /* Build table */
    Table t;

    t.col_count = data[0] % (MAX_COLS + 1);
    if (t.col_count == 0) t.col_count = 1;

    t.row_count = data[1] % (MAX_ROWS + 1);
    if (t.row_count == 0) t.row_count = 1;

    /* Allocate column names */
    for (int i = 0; i < t.col_count; i++) {
        t.col_names[i] = malloc(8);
        strcpy(t.col_names[i], "col");
    }

    /* Allocate each cell */
    for (int r = 0; r < t.row_count; r++) {
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

    /* Fake stdin — supplies column number + substring pattern */
    char *input = malloc(size + 1);
    memcpy(input, data, size);
    input[size] = '\0';

    FILE *fake_stdin = fmemopen(input, size, "r");
    if (!fake_stdin) {
        free(input);
        return 0;
    }

    FILE *orig_stdin = stdin;
    stdin = fake_stdin;

    /* Run the REAL function */
    find_rows_like(&t);

    /* Restore stdio */
    stdin = orig_stdin;
    stdout = orig_stdout;

    fclose(fake_stdin);
    fclose(devnull);
    free(input);

    /* Cleanup */
    for (int i = 0; i < t.col_count; i++)
        free(t.col_names[i]);

    for (int r = 0; r < t.row_count; r++)
        for (int c = 0; c < t.col_count; c++)
            free(t.rows[r].cells[c]);

    return 0;
}
