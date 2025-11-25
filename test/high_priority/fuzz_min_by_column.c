#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../../csv_sql.c"   // include real implementation

#define MAX_ROWS 1024

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {

    /* prevent %0 and other small-input crashes */
    if (size < 3)
        return 0;

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

    /* Allocate column names */
    for (int i = 0; i < t.col_count; i++) {
        t.col_names[i] = malloc(8);
        if (t.col_names[i])
            strcpy(t.col_names[i], "col");
    }

    /* Allocate cell values (safe memcpy) */
    for (int r = 0; r < t.row_count; r++) {
        t.rows[r].cell_count = t.col_count;

        for (int c = 0; c < t.col_count; c++) {

            size_t alloc = 4 + ((r + c) % 32);
            t.rows[r].cells[c] = malloc(alloc);
            if (!t.rows[r].cells[c]) continue;

            size_t copy_len = alloc - 1;
            size_t idx = (2 + r + c) % size;

            size_t avail = size - idx;
            size_t to_copy = (copy_len < avail ? copy_len : avail);

            if (to_copy > 0)
                memcpy(t.rows[r].cells[c], &data[idx], to_copy);

            t.rows[r].cells[c][to_copy] = '\0';
        }
    }

    /* Fake stdin */
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

    /* Run real function (from csv_sql.c) */
    min_by_column(&t);

    /* Restore IO */
    stdin = orig_stdin;
    stdout = orig_stdout;

    fclose(fake_stdin);
    fclose(devnull);
    free(input);

    /* Free memory */
    for (int i = 0; i < t.col_count; i++)
        free(t.col_names[i]);

    for (int r = 0; r < t.row_count; r++)
        for (int c = 0; c < t.col_count; c++)
            free(t.rows[r].cells[c]);

    return 0;
}
