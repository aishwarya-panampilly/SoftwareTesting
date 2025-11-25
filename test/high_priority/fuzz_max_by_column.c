#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../../csv_sql.c"   // include project implementation ONCE

//#define MAX_COLS 32
#define MAX_ROWS 1024

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {

    /* ---- Safety check to avoid division-by-zero and very small inputs ---- */
    if (size < 3) return 0;

    /* ---- Redirect stdout ---- */
    FILE *devnull = fopen("/dev/null", "w");
    if (!devnull) return 0;
    FILE *orig_stdout = stdout;
    stdout = devnull;

    /* ---- Build synthetic Table ---- */
    Table t;
    t.col_count = data[0] % (MAX_COLS + 1);
    if (t.col_count == 0) t.col_count = 1;

    t.row_count = data[1] % (MAX_ROWS + 1);

    /* Allocate column names */
    for (int i = 0; i < t.col_count; i++) {
        t.col_names[i] = malloc(8);
        if (t.col_names[i]) strcpy(t.col_names[i], "col");
    }

    /* Allocate rows + cells (safe copy) */
    for (int r = 0; r < t.row_count; r++) {
        t.rows[r].cell_count = t.col_count;

        for (int c = 0; c < t.col_count; c++) {
            size_t alloc = 4 + ((r + c) % 32);   // allocated bytes (including terminator)
            t.rows[r].cells[c] = malloc(alloc);
            if (!t.rows[r].cells[c]) continue;

            size_t copy_len = alloc - 1;         // bytes we want to copy at most
            size_t idx = (2 + (size_t)r + (size_t)c) % size; // valid because size >= 3

            /* compute how many bytes are actually available from data[idx] */
            size_t avail = size - idx;
            size_t to_copy = (copy_len < avail) ? copy_len : avail;

            /* perform the safe copy and null-terminate at the correct place */
            if (to_copy > 0) {
                memcpy(t.rows[r].cells[c], &data[idx], to_copy);
            }
            t.rows[r].cells[c][to_copy] = '\0';
        }
    }

    /* ---- Fake stdin from fuzz data ---- */
    char *input = malloc(size + 1);
    if (!input) {
        /* cleanup */
        for (int i = 0; i < t.col_count; i++) free(t.col_names[i]);
        for (int r = 0; r < t.row_count; r++)
            for (int c = 0; c < t.col_count; c++)
                free(t.rows[r].cells[c]);
        fclose(devnull);
        return 0;
    }
    memcpy(input, data, size);
    input[size] = '\0';

    FILE *fake_stdin = fmemopen(input, size, "r");
    if (!fake_stdin) {
        free(input);
        for (int i = 0; i < t.col_count; i++) free(t.col_names[i]);
        for (int r = 0; r < t.row_count; r++)
            for (int c = 0; c < t.col_count; c++)
                free(t.rows[r].cells[c]);
        fclose(devnull);
        return 0;
    }

    FILE *orig_stdin = stdin;
    stdin = fake_stdin;

    /* ---- Call the real function from csv_sql.c ---- */
    max_by_column(&t);

    /* ---- Restore stdout + stdin ---- */
    stdin = orig_stdin;
    stdout = orig_stdout;

    fclose(fake_stdin);
    fclose(devnull);
    free(input);

    /* ---- Cleanup ---- */
    for (int i = 0; i < t.col_count; i++)
        free(t.col_names[i]);

    for (int r = 0; r < t.row_count; r++)
        for (int c = 0; c < t.col_count; c++)
            free(t.rows[r].cells[c]);

    return 0;
}
